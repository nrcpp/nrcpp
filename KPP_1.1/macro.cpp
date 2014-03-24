// обработка значений макросов - macro.cpp

#include <algorithm>
#include <string>
#include <list>
#include <stack>
#include <iostream>
#include <cstdlib>

using namespace std;
#include "kpp.h"
#include "cpplex.h"
#include "macro.h"
#include "limits.h"


MacroTable mtab;


// возвращает указатель на объект в табице, или NULL
Macro *MacroTable::Find( char *name )
{
	list<Macro>::iterator i;
	return _Find( name, i ) == false ? NULL : &(*i);
}


// вставляет элемент в таблицу
void MacroTable::Insert( Macro ob )
{
	list<Macro> &p = HashFunc( (char *)ob.name.c_str() );
	p.push_back( ob );
}


// удаляет элемент из таблицы
void MacroTable::Remove( char *name )
{
	list<Macro> &p = HashFunc( name );
	list<Macro>::iterator i;

	if( _Find( name, i ) )
	{
		if( (*i).pred )
			Error( "'%s': макрос предопределен", (*i).name.c_str() );
		else
			p.remove( *i );
	}
}


static void InitPrms( Param &ob ) 
{
	ob.val = " "; 
}


// считывает значение одного параметра
static inline bool ReadOneParam( string &pval, BaseRead &buf )
{
	register int c; 
	int crmps = 1;

	c =  IgnoreSpaces( buf, true );
	for( ;; )
	{
		c = Lex(buf);
		if( c == ')' )
		{
			crmps = crmps ? crmps - 1 : 0;
			if( !crmps )
				return false;
		}

		else if( c == ',' )
		{
			if( crmps == 1 )
				return true;
		}

		else if( c == '(' )
			crmps += 1;

		else if( c == EOF )
		{
			Error( "не хватает ')' в конце строки" );
			return false;
		}

		pval += lexbuf;
		lexbuf = "";
		IgnoreSpaces( buf, true );
		pval += lexbuf;	// считываем пробелы
	}

	return false;
}


// считывает значения параметров
static inline bool ReadMacroParams( Macro &r, BaseRead &buf )
{
	register int c = IgnoreSpaces( buf, false );
	
	if( c != '(' )
		return false;
	// еще раз считываем (
	else
		buf >> c;

	int crmps = 1;
	string pval = "";
	list<Param>::iterator i = r.params.begin();
	bool err = false;
	bool notdone = true;

	for_each( r.params.begin(), r.params.end(), InitPrms );
	
	if( IgnoreSpaces( buf, false ) == ')' )	// пустые параметры
		buf >> c;

	else
	for( ; notdone ; )
	{
		notdone = ReadOneParam( pval, buf );

		SplitSpaces( pval );	// удаляем пробелы сзади
		if( pval == "" )
		{	
			Error( "'%s': пустой параметр", r.name.c_str() );
			pval = " ";
		}
		
		if( i == r.params.end() && !err )
		{
			Error( "'%s': параметров больше чем ожидается", r.name.c_str() );
			err = true;
			continue;
		}
		
		(*i).val = pval;
		i++;
		pval = "";
	}
		
	if( i != r.params.end() )
		Error( "'%s': параметров меньше чем ожидается", r.name.c_str() );

	return true;
}


// делает первый проход по значению макроса и 
// заменяет параметры их значениями
inline static string &ProcessParams( Macro &r )
{
	static string s;
	register int c;
	BufferRead buf(r.val);
	bool sharpop = false, strop = false;

	s = "";
	while( (c = Lex(buf)) != EOF )
	{
		// обрабатываем операторы # и ##
		if( c == '#' )
		{
			buf >> c;
			if( c == '#' )	// ##
			{
				if( s == "" )
					Error( "'%s': оператор '##' в начале строки", r.name.c_str() );
					
				SplitSpaces(s);		// удаляем пробелы для конкатенации строкы
				sharpop = true;
				continue;
			}

			else
			{
				buf << c;
				c = Lex(buf);
				if( c == NAME )
					strop = true;
			}
		}

		if( c == NAME )
		{
			list<Param>::iterator p = 
				find_if(r.params.begin(), r.params.end(), FuncParam(lexbuf));

			if( p != r.params.end() )
				s += strop ? MakeStringLiteral((*p).val) : (*p).val;
			else
			{
				if(strop)
					Error( "'%s': оператор '#' применяется не к параметру", r.name.c_str() );
				s += lexbuf;
			}
		}
		
		else
			s += lexbuf;

		strop = false;
		sharpop = false;
		lexbuf = "";
		IgnoreSpaces( buf, true );
		s += lexbuf;		// считываем пробелы
	}

	
	if(sharpop)
		Error( "'%s': оператор '##' в конце строки", r.name.c_str() );

	return s;
}


// предварительный просмотр значения макроса
static inline string &PredView( Macro &r )
{
	// если функция- то требутеся предварительный проход для 
	// подстановки параметров и выполнения операторов # и ##
	if( r.type == Macro::FUNCTION )
		return ProcessParams(r); 
	return r.val;
}


// обрабатывает оператор 'defined'
static char inline DefinedOperator( BufferRead &buf )
{
	int c = Lex(buf);

	if( c == '(' )
	{
		if( Lex(buf) == NAME )
		{
			char r = mtab.Find( (char *)lexbuf.c_str() ) ? '1' : '0';
			c = Lex(buf);
			if( c != ')' )
				throw ("не хватает ')' в операторе 'defined'");
			return r;
		}

		else
			throw ("пропущено имя макроса после 'defined'");
	}

	else if( c == NAME )
		return mtab.Find( (char *)lexbuf.c_str() ) != NULL;

	else
		throw ("пропущено имя макроса после 'defined'");
	return '1';	// kill warning
}


// проходит по строке s и выполняет макроподстановку
string Substitution( string b, bool dodef )
{
	string rval;
	register int c;
	BufferRead buf(b);
	static int deep = 0;
	static list<string> stck;

	if( deep++ == MAX_MACRO_DEEP )
		Fatal( "стек переполнен: макроподстановка слишком глубока" );

	while( ( c = Lex(buf) ) != EOF )
	{
		if( c == NAME )
		{
			Macro *r;

			// ищем макрос в таблице
			if( (r = mtab.Find( (char *)lexbuf.c_str() ) ) == NULL )
			{
				// возможно требуется обработать оператор defined
				if(dodef && lexbuf == "defined")	
					rval += DefinedOperator( buf );

				else
					rval += lexbuf;
			}

			
			// если рекурсивная подстановка (в стеке уже есть такой макрос),
			// то не выполнять макроподстановку, а подставить просто имя
			// если в параметре был этот же макрос M( M(10) ), то макро-параметр
			// также не подставляется, ФИКСМИ
			else if( find(stck.begin(), stck.end(), r->name) != stck.end() )
				rval += lexbuf; 
				
			else
			{
				// если макро-функция
				if( r->type == Macro::FUNCTION )
				{
					string temp = lexbuf;

					// загрузить значения параметров
					if( !ReadMacroParams( *r, buf ) )
					{
						rval += temp + " ";
						continue;
					}
				}

				// если макрос предопределен
				else if( r->pred )
				{
					if( r->name == "__LINE__" )
					{ 
						char buf[255];
						_snprintf(buf, 255, "%d ", linecount);
						rval += buf;
					}

					else
						rval += r->val + " ";
					continue;
				}

				
				stck.push_back( r->name );
				rval += Substitution( PredView(*r) );
				stck.pop_back();
			}
		}

		else
			rval += lexbuf;

	
		lexbuf = "";
		IgnoreSpaces( buf, true );
		rval += lexbuf;	// считываем пробелы
	}

	deep--;
	return rval;
}
