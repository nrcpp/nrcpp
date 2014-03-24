// лексический анализатор для компилятора C++ - cpplex.cpp

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <string>

using namespace std;
#include "error.h"
#include "cpplex.h"


// структура которая обеспечивает поиск кода лексемы
// по ее имени 
struct keywords
{
	// имя ключевого слова
	const char *name;


	// код ключевого слова
	int code;

}	kpp_words[] = {
	{ "define", KPP_DEFINE },
	{ "error",  KPP_ERROR  },
	{ "undef",  KPP_UNDEF  },
	{ "elif",   KPP_ELIF   },
	{ "if",		KPP_IF     },
	{ "include", KPP_INCLUDE },
	{ "else",   KPP_ELSE   },
	{ "ifdef",  KPP_IFDEF  },
	{ "line",   KPP_LINE   },
	{ "endif",  KPP_ENDIF  },
	{ "ifndef", KPP_IFNDEF },
	{ "pragma", KPP_PRAGMA }
	},

	cpp_words[] = {
	{ "asm", KWASM }, { "auto", KWAUTO }, 
	{ "bool", KWBOOL }, { "break", KWBREAK },
	{ "case", KWCASE }, { "catch", KWCATCH },
	{ "char",  KWCHAR }, { "class", KWCLASS },
	{ "const", KWCONST }, { "const_cast", KWCONST_CAST },
	{ "continue", KWCONTINUE }, { "default", KWDEFAULT },
	{ "delete", KWDELETE }, { "do", KWDO },
	{ "double", KWDOUBLE }, { "dynamic_cast", KWDYNAMIC_CAST },
	{ "else", KWELSE }, { "enum", KWENUM },	
	{ "explicit", KWEXPLICIT }, { "export", KWEXPORT },
	{ "extern", KWEXTERN },  { "false", KWFALSE }, 
	{ "float", KWFLOAT }, { "for", KWFOR },
	{ "friend", KWFRIEND }, { "goto", KWGOTO },
	{ "if", KWIF }, { "inline", KWINLINE }, 
	{ "int", KWINT }, { "long", KWLONG },
	{ "mutable", KWMUTABLE }, { "namespace", KWNAMESPACE },
	{ "new", KWNEW }, { "operator", KWOPERATOR }, 
	{ "private", KWPRIVATE }, { "protected", KWPROTECTED },
	{ "public", KWPUBLIC }, { "register", KWREGISTER },
	{ "reinterpret_cast", KWREINTERPRET_CAST },
	{ "return", KWRETURN }, { "short", KWSHORT },
	{ "signed", KWSIGNED }, { "sizeof", KWSIZEOF },  
	{ "static", KWSTATIC }, { "static_cast", KWSTATIC_CAST }, 
	{ "struct", KWSTRUCT }, { "switch", KWSWITCH }, 
	{ "template", KWTEMPLATE }, { "this", KWTHIS },
	{ "throw", KWTHROW }, { "true", KWTRUE }, 
	{ "try", KWTRY }, { "typedef", KWTYPEDEF },
	{ "typeid", KWTYPEID }, { "typename", KWTYPENAME },
	{ "union", KWUNION },  { "unsigned", KWUNSIGNED },
	{ "using", KWUSING },  { "virtual", KWVIRTUAL },
	{ "void", KWVOID },  { "volatile", KWVOLATILE },
	{ "wchar_t", KWWCHAR_T }, { "while", KWWHILE } 
};


// счетчик строк
int linecount;


// буфер с содержимым лексемы
string lexbuf;



// функция возвращает код ключевого слова или -1
// в случае если такого ключевого слова нет
int LookupKeywordCode( const char *keyname, keywords *kmas, int szmas )
{
	for( int i = 0; i< szmas / sizeof(keywords); i++ )
		if( !strcmp( kmas[i].name, keyname ) )
			return kmas[i].code;
	return -1;
}


// ищет ключевые слова kpp
int LookupKppKeywords( const char *keyname )
{
	return LookupKeywordCode( keyname, kpp_words, sizeof( kpp_words ) );
}


// игнорировать только пробелы,
// если putspaces = true - выводить пробелы в lexbuf
int IgnoreSpaces( BaseRead &ob, bool putspaces )
{
	register int c;

	while( (ob >> c) != EOF )
		if( (c == ' ' || c == '\t') )
		{ 
			if(putspaces) 
				lexbuf += c;
		}
		else
			break;

	ob << c; 
	return c;
}


// игнорирует пробелы и новые строки
int IgnoreNewlinesAndSpaces( BaseRead &ob )
{
	register int c;

	while( (ob >> c) != EOF )
		if( c == ' ' || c == '\t' )
			continue;

		else if( c == '\n' )
			linecount++;

		else
			break;

	ob << c; // возвращаем один символ в поток
	return c;
}


// выделить лексему 'идентификатор'
inline int LexemName( BaseRead &ob )
{
	register int c;

	while( (ob >> c) != EOF )
		if( !IS_NAME(c) )
			break;
		else
			lexbuf += (char)c;

	ob << c;
	return NAME;
}


// выделить лексему 'оператор'
inline int LexemOperator( BaseRead &ob )
{
	register int c;
	
	ob >> c;
	if( c == '-' )
	{
		ob >> c;
		
		if(c == '-') { lexbuf = "--"; return DECREMENT; }
		else if(c == '=') { lexbuf = "-="; return MINUS_ASSIGN; }
		else if(c == '>') 
		{
			ob >> c;
			if(c == '*') { lexbuf = "->*"; return ARROW_POINT; }
			else { ob << c; lexbuf = "->"; return ARROW; }
		}
		else { ob << c; lexbuf = '-'; return '-'; }
	}

	else if( c == '+' )
	{
		ob >> c;
		
		if(c == '+') { lexbuf = "++"; return INCREMENT; }
		else if(c == '=') { lexbuf = "+="; return PLUS_ASSIGN; }
		else { ob << c; lexbuf = '+'; return '+'; }
	}

	else if( c == '*' )
	{
		ob >> c;
		
		if(c == '=') { lexbuf = "*="; return MUL_ASSIGN; }
		else { ob << c; lexbuf = '*'; return '*'; }
	}

	else if( c == '/' )
	{
		ob >> c;
		
		if(c == '=') { lexbuf = "/="; return DIV_ASSIGN; }
		else { ob << c; lexbuf = '/'; return '/'; }
	}

	else if( c == '%' )
	{
		ob >> c;
		
		if(c == '=') { lexbuf = "%="; return PERCENT_ASSIGN; }
		else { ob << c; lexbuf = '%'; return '%'; }
	}

	else if( c == '<' )
	{
		ob >> c;
		
		if(c == '=') { lexbuf = "<="; return LESS_EQU; }
		else if(c == '<') 
		{
			ob >> c;
			if(c == '=') { lexbuf = "<<="; return LEFT_SHIFT_ASSIGN; }
			else { ob << c; lexbuf = "<<"; return LEFT_SHIFT; }
		}
		else { ob << c; lexbuf = '<'; return '<'; }
	}
	
	else if( c == '>' )
	{
		ob >> c;
		
		if(c == '=') { lexbuf = ">="; return GREATER_EQU; }
		else if(c == '>') 
		{
			ob >> c;
			if(c == '=') { lexbuf = ">>="; return RIGHT_SHIFT_ASSIGN; }
			else { ob << c; lexbuf = ">>"; return RIGHT_SHIFT; }
		}
		else { ob << c; lexbuf = '>'; return '>'; }
	}

	else if( c == '=' )
	{
		ob >> c;
		if( c == '=' ) { lexbuf = "=="; return EQUAL; }
		else { ob << c; lexbuf = '='; return '='; }
	}

	else if( c == '!' )
	{
		ob >> c;
		if( c == '=' ) { lexbuf = "!="; return NOT_EQUAL; }
		else { ob << c; lexbuf = '!'; return '!'; }
	}

	else if( c == '^' )
	{
		ob >> c;
		if( c == '=' ) { lexbuf = "^="; return XOR_ASSIGN; }
		else { ob << c; lexbuf = '^'; return '^'; }
	}

	else if( c == '&' )
	{
		ob >> c;
		if( c == '=' ) { lexbuf = "&="; return AND_ASSIGN; }
		else if( c == '&' ) { lexbuf = "&&"; return LOGIC_AND; }
		else { ob << c; lexbuf = '&'; return '&'; }
	}

	else if( c == '|' )
	{
		ob >> c;
		if( c == '=' ) { lexbuf = "|="; return OR_ASSIGN; }
		else if( c == '|' ) { lexbuf = "||"; return LOGIC_OR; }
		else { ob << c; lexbuf = '|'; return '|'; }
	}
    
	else if( c == ':' )
	{
		ob >> c;
		if( c == ':' ) { lexbuf = "::"; return COLON_COLON; }
		else { ob << c; lexbuf = ':'; return ':'; }
	}

	else if( c == '.' )
	{
		ob >> c;
		if( c == '*' ) { lexbuf = ".*"; return DOT_POINT; }
		else if( c == '.' ) 
		{
			ob >> c;
			if(c == '.') { lexbuf = "..."; return ELLIPSES; }
			else 
			{
				ob << c;
				Error( "пропущена '.' в операторе '...'");
				lexbuf = "...";
				return ELLIPSES;
			}
		}

		else { ob << c; lexbuf = '.'; return '.'; }
	}

	else
	{
		lexbuf = c;
		return c;
	}
}


// выделить лексему 'строковый литерал'
inline int LexemString( BaseRead &ob )
{
	register int c;

	for(;;)
	{
		ob >> c;
		if( c == '\"' )
		{
			lexbuf += c;
			return STRING;
		}

		else if( c == '\\' )
		{
			int pc;
				
			ob >> pc;
			if(pc == '\"')
			{
				lexbuf += '\\'; lexbuf += '\"';
				continue;
			}

			else
				ob << pc;
		}

		else if( c == '\n' || c == EOF )
		{
			ob << c;

			Error( "не хватает `\"' в конце строки" );
			lexbuf += '\"';
			return STRING;
		}		

		lexbuf += c;
	}

	return STRING;	// kill warning
}


// функция возвращает ненулевое значение если символ восьмеричный
int isdigit8( int c )
{
	return c >= '0' && c <= '7';
}


// считывает число из входного потока, пока
// функция isfunc возвращает true 
void ReadDigit( BaseRead &ob, int (*isfunc)(int) )
{
	register int c;

	while( (ob >> c) != EOF )
		if( !isfunc(c) )
			break;
		else
			lexbuf += c;

	ob << c;
}


// считать суффикс у числа, вернуть true если суффикс suf
// будет задан
static inline bool ReadDigitSuffix( BaseRead &ob, char suf )
{
	bool sl, ss;

	sl = ss = false;	// два суффикса могут быть установлены

	// первый суффикс 'l', второй suf
	for( register int c;; )
	{
		ob >> c;
		if( toupper(c) == 'L' )
		{
			if( sl ) 
				Warning("суффикс 'L' у числа уже задан");
			else
				sl = true, lexbuf += c;
		}

		// или 'U' или 'F'
		else if( toupper(c) == toupper(suf) )
		{
			if( ss )
				Warning("суффикс '%c' у числа уже задан", suf);
			else 			
				ss = true, lexbuf += c;				
		}

		else
		{
			ob << c;
			break;
		}
	}

	return ss;
}


// выделить лексему 'число'
inline int LexemDigit( BaseRead &ob )
{
	register int c;
	int state = 0;

	ob >> c;

	for(;;)
	switch(state)
	{
	case 0:
		if(c == '0') state = 1;
		else if(c == '.') 
		{
			int p;
			
			ob >> p; ob << p;

			if( !isdigit(p) )	// просто оператор точка
			{ ob << c;	return -1; }

			else
				state = 2;
		}

		// десятичное число 1-9
		else 
		{
			lexbuf += c;
			ReadDigit( ob, isdigit );
			
			ob >> c;
			if( c == '.' )
				state = 2;
			else if( c == 'e' || c == 'E' )
				state = 3;
			else
			{
				ob << c;
				return ReadDigitSuffix(ob, 'U') ? UINTEGER10 : INTEGER10;
			}
		}
		
		break;

	case 1:
		lexbuf += c;
		ob >> c;

		if( c == '.' ) state = 2;
		else if( c == 'e' || c == 'E' ) state = 3;
		else if( c == 'x' || c == 'X' ) 
		{
			lexbuf += c;
			ReadDigit( ob, isxdigit );

			if( toupper( *(lexbuf.end() - 1) ) == 'X' )
				Error("отсутствует 16-ричная последовательность после '%c'",c);
			return ReadDigitSuffix(ob, 'U') ? UINTEGER16 : INTEGER16;
		}

		else if( isdigit8(c) ) 
		{
			lexbuf += c;
			ReadDigit( ob, isdigit8 );
			return ReadDigitSuffix(ob, 'U') ? UINTEGER8 : INTEGER8;			
		}

		else 
		{
			ob << c;
			return ReadDigitSuffix(ob, 'U') ? UINTEGER10 : INTEGER10;
		}

		break;

	case 2:
		// сюда переход только после точки
		lexbuf += c;
		ob >> c;
	
		if( c == 'e' || c == 'E' ) 
			state = 3;

		else if( isdigit(c) )
		{
			lexbuf += c;
			ReadDigit(ob, isdigit);

			ob >> c;
			if( c == 'e' || c == 'E' ) 
				state = 3;
			else 
			{
			read_suffix:
				ob << c;
				return ReadDigitSuffix(ob, 'F') ? LFLOAT : LDOUBLE;
			}
		}

		// иначе было считано число и осталась просто точка
		else
			goto read_suffix;

		break;

	case 3:
		// сюда переход после E
		lexbuf += c;
		ob >> c;

		if( c == '+' || c == '-' )
			lexbuf += c, (ob >> c);

		if( !isdigit(c) )
		{
			ob << c;
			Error( "пропущено значение экспоненты" );
			return LDOUBLE;
		}

		else
		{	
			lexbuf += c;
			ReadDigit(ob, isdigit);	
			return ReadDigitSuffix(ob, 'F') ? LFLOAT : LDOUBLE;
		}
	}
}



// выделить лексему символьная константа
inline int LexemCharacter( BaseRead &ob )
{
	register int c;
	
	// символ ' уже считан, считываем до другого '
	// либо до новой строки, количество символов не имеет
	// значения, корректность значения символа проверяется после
	
	ob >> c;
	if( c == '\'' )	// пустой символ
	{
		lexbuf += '\\',
		lexbuf += '0', lexbuf += '\'';	// автоматически добавляем \0
		Error( "пустой символ" );
		return CHARACTER;
	}

	ob << c; 
	for( ;; )
	{
		ob >> c;
		if( c == '\'' )
		{
			lexbuf += c;
			return CHARACTER;
		}

		else if( c == '\\' )
		{
			int pc;
			
			ob >> pc;
			if(pc == '\'')
			{
				lexbuf += '\\'; lexbuf += '\'';
				continue;
			}

			else if(pc == '\\')
			{
				lexbuf += "\\\\";
				continue;
			}

			else
				ob << pc;
		}

		else if( c == '\n' || c == EOF )
		{
			ob << c;

			Error( "не хватает `\'' в конце строки" );
			lexbuf += '\'';
			return CHARACTER;
		}		

		lexbuf += c;
	}

	return CHARACTER;	// kill warning
}


// функция выделяет следующую лексему из
// потока in
int Lex( BaseRead &ob )
{
	register int c;

	lexbuf = "";
	c = IgnoreNewlinesAndSpaces(ob);

	if( IS_NAME_START(c) ) 
	{
		ob >> c;  // считываем этот символ еще раз
		lexbuf += c;
		
		// возможно признак обозначения wide-string
		if( c == 'L' )	
		{
			int p; 

			ob >> p;
			if( p == '\'')
			{
				lexbuf += p;
				LexemCharacter(ob);
				return WCHARACTER;
			}

			else if( p == '\"' )
			{
				lexbuf += p;
				LexemString(ob);
				return WSTRING;
			}

			else
				ob << p;
		}

		LexemName(ob);
		return NAME;	// ключевые слова определяются потом
	}

	else if( isdigit(c) || c == '.' )
	{
		int r;
		if( (r = LexemDigit(ob)) == -1 )
			return LexemOperator(ob);	// иначе считываем точку (.*)
		else
			return r;
	}

	else if( c == '\"' )
	{
		lexbuf += c;
		ob >> c;
		return LexemString(ob);
	}

	else if( c == '\'' )
	{
		lexbuf += c;
		ob >> c;
		return LexemCharacter(ob);
	}

	else
		return LexemOperator(ob);
}
