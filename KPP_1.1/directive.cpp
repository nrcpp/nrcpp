// выполняются директивы препроцессора - directive.cpp

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <stack>
#include <list>
#include <algorithm>
#include <string>

using namespace std;
#include "kpp.h"
#include "cpplex.h"
#include "macro.h"
#include "limits.h"


// флаг указывает на необходимость вывода считанного из файла
// требуется при выполнении директив уусловной компиляции (#if, #ifdef, ...)
bool PutOut = true;


// стек в котором хранятся значения конст. выражений из 
// директив условной компиляции (1 - положительный результат,
// 0 - отрицательный (игнорируем вывод), -1 - была условная директива,
// но результат не засчитали, так как текущий режим PutOut=false
stack<int> IfResults;


// возвращает результат константного выражения в строке s
int CnstExpr( string &s );


// здесь хранятся пути к директориям с заголовочными файлами
list<string> IncludeDirs;


// новая пара - указатель на поток, имя файла
// используется в include
string IncName;


// вывести #line
void PutLine( FILE *out );


// вычисляет выражение в директивах #if/#elif
static bool inline EvalExpression( BaseRead &buf )
{
	string s;
	int r;

	ReadString( buf, s );

	try
	{
		r = CnstExpr( Substitution(s, true) );
	}

	catch(KPP_EXCEPTION exc)
	{
		switch(exc)
		{
		case EXP_EMPTY:
			Fatal("#if/#elif: отсутствует константное выражение");

		case REST_SYMBOLS:
			Fatal("#if/#elif: синтаксическая ошибка");
		}
	}

	catch( const char *msg )
	{
		Fatal("#if/#elif: %s", msg);					
	}
	
	return r != 0;
}


// проеверяет наличие файла для подключения
// sys - true, если подключаем системные дирректории
static bool inline TryInclude( string &finc, bool sys ) 
{
	string temp;
	FILE *input;

	if( sys )
	{
		for( list<string>::iterator p = IncludeDirs.begin();
			 p != IncludeDirs.end(); p++ )
		{
			temp = (*p) + finc;
			input = fopen( temp.c_str(), "r" );
			if( input )
			{
				fclose(input);
				IncName = temp;
				finc = temp;
				return true;
			}
		}
	}

	if( (input = fopen( finc.c_str(), "r" )) != NULL )
	{
		fclose(input);
		IncName = finc;
		return true;
	}

	return false;
}


// просматривает строку include, делает подстановку макросов, если
// необходимо, возвращает готовую строку
static string inline ViewIncludeString( BaseRead &buf )
{
	string s;

	ReadString(buf, s);
	if( s[0] != '<' )
		s = Substitution(s, false);
	return s;
}


// возвращает true если все параметры макро-функции с уникальными именами
static bool inline CheckParams( list<Param> &p, const char *fname )
{
	bool rval = true;

	for( list<Param>::iterator i = p.begin(); i != p.end(); i++)
	{
		list<Param>::iterator j = i;
		j++;
		while( j != p.end() )
		{
			if( (*i).name == (*j).name )
			{
				Error( "'%s': '%s' - несколько параметров с одним именем", 
					fname, (*i).name.c_str() );
				rval = false;
			}

			j++;
		}
	}

	return rval;
}	


// возвращает true если у mac и ob одинаковые параметры
static inline bool EqualParams( Macro &mac, Macro &ob )
{
	list<Param>	&pm = mac.params, &pob = ob.params;
	list<Param>::iterator im, iob; 

	im = pm.begin();
	iob = pob.begin();

	for( ;; )
	{
		if( (im == pm.end() && iob != pob.end()) ||
			(im != pm.end() && iob == pob.end()) )
			return false;

		if( (im == pm.end()) && (iob == pob.end()) )
			return true;

		if( (*im).name == (*iob).name )
			im++, iob++;

		else
			return false;
	}

	return true;
}


// вставить макрос в таблицу с проверкой
static void inline InsertWithCheck( Macro &ob )
{
	Macro *mac = mtab.Find( (char *)ob.name.c_str() );
	if( mac )
	{
		// если макрос предопределен
		if( mac->pred == true )
			Error( "'%s': макрос предопределен", mac->name.c_str() );
		else
		{
			// если функции сначала проверяем соотв. параметров
			if( (mac->type == Macro::FUNCTION) && (ob.type == Macro::FUNCTION) )
				if( !EqualParams( *mac, ob ) )
				{
					Warning( "'%s': макрос переопределен", mac->name.c_str());
					mtab.Remove( (char *)mac->name.c_str() );
					mtab.Insert( ob );
					return;
				}


			// потом сравниваем значение и тип
			if( (mac->type != ob.type) || (mac->val  != ob.val ) )
			{
				Warning( "'%s': макрос переопределен", mac->name.c_str() );
				mtab.Remove( (char *)mac->name.c_str() );
				mtab.Insert( ob );
			}

			// иначе макрос не заменяется			
		}
	}

	else
	{
		// defined также нельзя переопределять
		if( ob.name == "defined" )
			Error( "'defined': оператор нельзя использовать в '#define'");

		else
			mtab.Insert(ob);
	}
}


void do_define( BaseRead &buf )
{
	register int c = Lex( buf );

	if( c != NAME )
	{
		Error( "ожидается имя макроса после '#define'" );
		return;
	}

	string name = lexbuf;
	list<Param> params;
	int type = Macro::MACROS;

	buf >> c;
	// макро-функция, считываем параметры
	if( c == '(' )
	{
		c = Lex(buf);
		
		if( c != ')' )
		for(;;)
		{
			Param prm;
			
			if( c != NAME )
			{
				Error( "'%s': ожидается параметр макро-функции", name.c_str() );
				return;
			}
		
			prm.name = lexbuf;
			params.push_back( prm );
			c = Lex(buf);

			if( c == ')' )
				break;

			else if( c == ',' )
				c = Lex(buf);

			else
			{
				Error( "'%s': ожидается `,' или `)'", name.c_str() );
				return;
			}
		}

		type = Macro::FUNCTION;

		// проверка уникальности имени каждого параметра
		if( CheckParams( params, name.c_str() ) == false )
			return;
	}

	else
		buf << c;

	string val;
	ReadString( buf, val );		// считываем значение макроса

	if( type == Macro::MACROS )
		InsertWithCheck( Macro( name, val ) );
		
	else
		InsertWithCheck( Macro( name, val, params ) );
}


void do_error( BaseRead &buf )
{
	string s;

	ReadString( buf, s );
	Error( s.c_str() );
}


void do_undef( BaseRead &buf )
{
	if( Lex( buf ) != NAME )
	{
		Error( "ожидается имя макроса после '#undef'" );
		return;
	}
	
	// remove также проверяет предопределенные макросы
	mtab.Remove( (char *)lexbuf.c_str() );		
	if( Lex(buf) != EOF )
		Warning( "'#undef': лишние символы в строке" );
}


void do_elif( BaseRead &buf )
{
	if( IfResults.empty() )
		Fatal( "'#elif' без '#if'" );

	int prev = IfResults.top();
	
	if( prev == -1 )
		return;	// ничего не делаем, вывод заблокирован

	if( prev == 0 )	// предыдущий if/elif был false
	{		
		bool r = EvalExpression(buf);
	
		IfResults.pop();
		IfResults.push( (unsigned)r );
		PutOut = (r ? true : false);
	}


	// #elif после #else
	else if( prev == 2 )
	{
		Fatal( "некорректная конструкция #if/#elif/#else: "
			   "'#elif' идет после '#else'" );
	}

	else
	{
		IfResults.pop();
		IfResults.push( 0 ); 
		PutOut = false;
	}
}


void do_if( BaseRead &buf ) 
{
	if( IfResults.empty() )
	{
		bool r = EvalExpression(buf);
		IfResults.push( (unsigned)r );
		PutOut = r;
	}

	else
	{
		int prev = IfResults.top();

		if( prev == 0 || prev == -1 )
			IfResults.push( -1 );

		else
		{
			bool r = EvalExpression(buf);
			IfResults.push( (unsigned)r );
			PutOut = r;
		}
	}
}


void do_include( BaseRead &buf )
{
	int c;
	string finc, s;
	bool sys;
	BufferRead nbuf( ViewIncludeString(buf) );
	
	c = Lex(nbuf);
	
	// ищем во всех системных директориях
	if( c == '<' )
	{
		ReadString( nbuf, finc );
		if( finc == "" || (*(finc.end() - 1) != '>') )
			Fatal("'#include': пропущен '>' в конце строки");

		finc.erase( finc.end() - 1 );
		sys = true;		
	}

	else if( c == STRING )
	{
		finc = lexbuf;
		if( Lex(nbuf) != EOF )
			Warning( "'#include': лишние символы в строке" );

		// удаляем кавычки
		finc.erase( finc.begin() );	
		finc.erase( finc.end()-1 );

		sys = false;
	}

	else
		Fatal( "'#include': пропущено имя файла" );

	if( finc == "" )
		Fatal("'#include': пустое имя файла" );


	// пробуем подключить файл на основании имеющихся директорий 
	if( !TryInclude( finc, sys ) )
		Fatal("'#include': '%s' - файл не найден", finc.c_str() );
}


void do_else( BaseRead &buf )
{
	if( IfResults.empty() )
		Fatal( "#else без предварительного #if" );

	if( Lex(buf) != EOF )
		Warning( "'#else': лишние символы в строке" );


	int prev = IfResults.top();
	if( prev == -1 )
		;	// ничего не делаем, вывод заблокирован

	else if( prev == 0 )	// предыдущий if/elif был false
	{
		IfResults.pop();

		// помещаем в стек 2, как знак что это был else,
		// чтобы не было дальше else'ов
		IfResults.push( 2 );	
		PutOut = true; 
	}

	// #else уже был
	else if( prev == 2 )
		Fatal( "некорректная конструкция #if/#elif/#else: '#else' идет после '#else'" );

	else
	{
		IfResults.pop();
		IfResults.push( 0 ); 
		PutOut = false;
	}
}


void do_ifdef( BaseRead &buf )
{
	register int c = Lex( buf );

	if( c != NAME )
	{
		Fatal( "'#ifdef': пропущено имя макроса"  );
		return;
	}

	bool r = mtab.Find( (char *)lexbuf.c_str()) != NULL;

	
	if( Lex(buf) != EOF )
		Warning( "'#ifdef': лишние символы в строке" );

	if( IfResults.empty() )
	{
		IfResults.push( r );
		PutOut = r;
	}

	else
	{
		int prev = IfResults.top();

		if( prev == 0 || prev == -1 )
			IfResults.push( -1 );

		else
		{
			IfResults.push( r );
			PutOut = (r ? true : false);
		}
	}
}


void do_line( BaseRead &buf )
{
	extern string inname; // имя входного файла
	register int c;
	int temp;

	c = Lex( buf );
	if( IS_LITERAL(c) )
	{
		int r;
		if((r = CnstValue( (char *)lexbuf.c_str(), c )) != -1)
			temp = r;
	}
		
	else
	{
		Error("ожидается целое число после '#line'");
		return;
	}

	c = Lex( buf );
	if( c == STRING )
	{
		inname = lexbuf;

		// удаляем первый и последний символ (кавычки)
		inname.erase( inname.begin() );	
		inname.erase( inname.end()-1 );
	}

	else if( c == EOF )
	{
		linecount = temp;
		return;
	}

	else
	{
		Error("ожидается строковый литерал");
		return;
	}

	if( Lex(buf) != EOF )
		Warning( "'#line': лишние символы в строке" );
	
	linecount = temp;
}


void do_endif( BaseRead &buf )
{
	if( IfResults.empty() )
		Fatal( "'#endif' без '#if'" );

	IfResults.pop();
	if( IfResults.empty() )
		PutOut = true;

	else
	{
		int r = IfResults.top();
		PutOut = (r > 0 ? true : false);
	}
}

void do_ifndef( BaseRead &buf )
{
	register int c = Lex( buf );

	if( c != NAME )
	{
		Fatal( "'#ifndef': пропущено имя макроса"  );
		return;
	}

	int r = mtab.Find( (char *)lexbuf.c_str() ) == NULL;

	
	if( Lex(buf) != EOF )
		Warning( "'#ifndef': лишние символы в строке" );

	if( IfResults.empty() )
	{
		IfResults.push( r );
		PutOut = (r ? true : false);
	}

	else
	{
		int prev = IfResults.top();

		if( prev == 0 || prev == -1 )
			IfResults.push( -1 );

		else
		{
			IfResults.push( r );
			PutOut = (r ? true : false);
		}
	}
}


void do_pragma( BaseRead &buf )
{
	Warning( "'#pragma' игнорируется" );
}


// функция удаляет пробелы сзади
void SplitSpaces( string &s )
{
	if( s.empty() )
		return;

	register char *q, *p = (char *)s.c_str();
	
	q = p;
	p += s.length() - 1;

	while( *p == ' ' || *p == '\t' )
		p--;
	*(p+1) = 0;
	
	string t = q;
	s = t;
}


// функция преобразует строку s в строковый литерал
string &MakeStringLiteral( string &s )
{
	s = '\"' + s;

	int i = 1;
	while( i < s.length() )
	{
		if( s[i] == '\"' || s[i] == '\\' )
			s.insert( i, "\\" ), i++;
		i++;
	}

	s += '\"';
	return s;
}


// определяет какую директиву выполнить
// возвращает код директивы
int Directive( string s, FILE *out )
{
	// таблица функций, которые выполняют
	// директивы препроцессора
	static void (*sfuncs[])( BaseRead & ) = {
		do_define, do_error, do_undef,
		do_elif, do_if, do_include,
		do_else, do_ifdef, do_line,
		do_endif, do_ifndef, do_pragma
	};

	BufferRead buf( s.erase(0, 1) );
	int c = Lex ( buf );
	

	// пустая директива
	if( c == EOF )
		;

	// проверяем имя директивы
	else if( c == NAME )
	{
		int r = LookupKppKeywords( lexbuf.c_str() );
		if( r == -1 )
			Error( "'%s' - неизвестная директива препроцессора", lexbuf.c_str() );
		else
		{	 
			// если вывод заблокирован, то можно выполнять 
			// только условные директивы
			if( !PutOut &&
				(r != KPP_ELIF)   && (r != KPP_IF) &&
				(r != KPP_ELSE)   && (r != KPP_IFDEF) &&
				(r != KPP_ENDIF)  && (r != KPP_ENDIF) &&
			    (r != KPP_IFNDEF) )
				return -1;

			(sfuncs[r - KPP_DEFINE])( buf );
			return r;				
		}
	}

	// иначе ошибка
	else
		Error("отсутствует имя директивы после '#'");
		
	return -1;

}
