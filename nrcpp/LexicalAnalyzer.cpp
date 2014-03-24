// лексический анализатор для компилятора C++ - LexicalAnalyzer.cpp

#pragma warning(disable: 4786)
#include <nrc.h>
using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "LexicalAnalyzer.h"



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

	c_words[] = {
	{ "auto", KWAUTO }, { "break", KWBREAK },
	{ "case", KWCASE }, { "char",  KWCHAR },
	{ "const", KWCONST }, { "continue", KWCONTINUE }, 
	{ "default", KWDEFAULT }, { "do", KWDO },
	{ "double", KWDOUBLE }, { "else", KWELSE }, 
	{ "enum", KWENUM },	{ "extern", KWEXTERN },  
	{ "float", KWFLOAT }, { "for", KWFOR },
	{ "goto", KWGOTO }, { "if", KWIF }, 
	{ "int", KWINT }, { "long", KWLONG },
	{ "register", KWREGISTER }, { "return", KWRETURN }, 
	{ "short", KWSHORT }, { "signed", KWSIGNED }, 
	{ "sizeof", KWSIZEOF },  { "static", KWSTATIC }, 
	{ "struct", KWSTRUCT }, { "switch", KWSWITCH }, 
	{ "typedef", KWTYPEDEF }, { "union", KWUNION },  
	{ "unsigned", KWUNSIGNED }, { "void", KWVOID },  
	{ "volatile", KWVOLATILE }, { "while", KWWHILE } 
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


// буфер с содержимым лексемы
static string lexbuf;


// функция возвращает код ключевого слова или -1
// в случае если такого ключевого слова нет
inline int LookupKeywordCode( const char *keyname, keywords *kmas, int szmas )
{
	int r;
	for( int i = 0; i< szmas / sizeof(keywords); i++ )
	{
		r = strcmp( kmas[i].name, keyname );
		if( !r )
			return kmas[i].code;
		else if( r > 0 )
			break;
	}

	return -1;
}


// ищет ключевые слова kpp
static int LookupKppKeywords( const char *keyname )
{
	return LookupKeywordCode( keyname, kpp_words, sizeof( kpp_words ) );
}


// ищет ключевые слова языка С
static int LookupCKeywords( const char *keyname )
{
	return LookupKeywordCode( keyname, c_words, sizeof( c_words ) );
}


// ищет ключевые слова языка С++
static int LookupCPPKeywords( const char *keyname )
{
	return LookupKeywordCode( keyname, cpp_words, sizeof( cpp_words ) );
}


// возвращает имя ключевого слова по коду
const char *GetKeywordName( int code )
{
	return cpp_words[code - KWASM].name;
}


// игнорирует пробелы и новые строки
static int IgnoreNewlinesAndSpaces( BaseRead &ob )
{
	register int c;

	while( (ob >> c) != EOF )

		// внимание: табуляция считается как 1 символ (а не 4 пробела)
		if( c == ' ' || c == '\t' )		
			continue;

		else if( c == '\n' )		
			((CppFileRead &)(ob)).NewLine();		

		else
			break;

	ob << c; // возвращаем один символ в поток
	return c;
}


// проверяет, если 'nam' - альтернативное имя такое
// как and, or, ... то вернуть ненуловое значение - код
// настоящей лексемы, иначе 0
inline static int IsAlternativeName( const char *n ) 
{
	struct TempAgr
	{
		const char *name;
		int tok;
	} alt[] = {
		"and",	  LOGIC_AND,
		"and_eq", AND_ASSIGN, 
		"bitand", '&',
		"bitor",  '|',
		"compl",  '~',
		"not",	  '!',
		"not_eq", NOT_EQUAL,
		"or",	  LOGIC_OR,
		"or_eq",  OR_ASSIGN,
		"xor",	  '^',
		"xor_eq", XOR_ASSIGN
	};

	for( int i = 0; i<11; i++ )
		if( !strcmp( alt[i].name, n ) )
			return alt[i].tok;
	return 0;
}


// выделить лексему 'идентификатор'
inline static int LexemName( BaseRead &ob )
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
inline static int LexemOperator( BaseRead &ob )
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
		else if(c == '>') { lexbuf = "%>"; return '}'; }	
		
		// альтеранатива '%>'  - '{' , '%:' - #, '%:%:' - ##
		else if(c == ':') 
		{ 
			ob >> c;
			if( c == '%' )
			{
				ob >> c;
				if( c != ':' )
					theApp.Error("пропущен символ ':' в лексеме '%%:%%:'"),
					ob << c;
				lexbuf = "%:%:";
				return DOUBLE_SHARP;
			}

			else
			{
				ob << c;
				lexbuf = "%:";
				return '#';
			}
		}

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

		// альтеранатива '<%'  - '{', '<:' - '['
		else if(c == '%') { lexbuf = "<%"; return '{'; }
		else if(c == ':') { lexbuf = "<:"; return '['; }
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
		else if(c == '>') { lexbuf = ":>"; return ']'; }	// ':>' - ']'
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
				theApp.Error( "пропущена '.' в операторе '...'");
				lexbuf = "...";
				return ELLIPSES;
			}
		}

		else { ob << c; lexbuf = '.'; return '.'; }
	}

	else if( c == '#' )
	{
		ob >> c;
		if( c == '#' )
		{
			lexbuf = "##";
			return DOUBLE_SHARP;
		}

		ob << c;
		lexbuf = "#";
		return '#';
	}

	else if( c == EOF )
	{
		lexbuf = "<конец файла>";
		return c;
	}

	else
	{
		lexbuf = c;
		return c;
	}
}


// выделить лексему 'строковый литерал'
inline static int LexemString( BaseRead &ob )
{
	register int c;
	bool wstr = lexbuf.at(0) == 'L';

	// цикл выполняется пока есть возможность соединять строки
	for( ;; )
	{	
		for(;;)
		{
			ob >> c;
			if( c == '\"' )			
				break;		// строка считана

			else if( c == '\\' )
			{
				int pc;
						
				ob >> pc;
				if(pc == '\"' || pc == '\\')
				{
					lexbuf += '\\'; lexbuf += (char)pc;
					continue;
				}

				else
					ob << pc;
			}

			else if( c == '\n' || c == EOF )
			{
				ob << c;

				theApp.Error( "не хватает `\"' в конце строки" );
				lexbuf += '\"';
				return STRING;
			}		

			lexbuf += c;
		}

		// переходим к следующей лексеме, возможно это будет опять строка,
		// тогда возможно будет конкатенация
		c = IgnoreNewlinesAndSpaces( ob );
		if( c == '\"' )		// продолжаем итерацию цикла
		{
			if( wstr )
				theApp.Error("конкатенация строк разных типов");
			wstr = false;
			ob >> c;
		}

		// возможно строка типа wchar_t
		else if( c == 'L' )		
		{
			ob >> c, ob >> c;

			if( c == '\"' )
			{
				if( !wstr )
					theApp.Error("конкатенация строк разных типов");
				wstr = true;
			}

			else
			{
				ob << c, ob << (c = 'L');
				lexbuf += '\"';
				return STRING;
			}
		}

		else
		{
			lexbuf += '\"';
			return STRING;
		}
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
static void ReadDigit( BaseRead &ob, int (*isfunc)(int) )
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
				theApp.Warning("суффикс 'L' у числа уже задан");
			else
				sl = true, lexbuf += c;
		}

		// или 'U' или 'F'
		else if( toupper(c) == toupper(suf) )
		{
			if( ss )
				theApp.Warning("суффикс '%c' у числа уже задан", suf);
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
inline static int LexemDigit( BaseRead &ob )
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
				theApp.Error("отсутствует 16-ричная последовательность после '%c'",c);
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
			theApp.Error( "пропущено значение экспоненты" );
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
inline static int LexemCharacter( BaseRead &ob )
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
		theApp.Error( "пустой символ" );
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

			theApp.Error( "не хватает `\'' в конце строки" );
			lexbuf += '\'';
			return CHARACTER;
		}		

		lexbuf += c;
	}

	return CHARACTER;	// kill warning
}


// функция выделяет следующую лексему из
// потока in
static int Lex( BaseRead &ob, Position &lxmPos )
{
	register int c;

	lexbuf = "";
	c = IgnoreNewlinesAndSpaces(ob);

	lxmPos = ((CppFileRead&)ob).GetPosition();		// сохраняем позицию лексемы	

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

		// возможно альтернативное имя, такое как and, or...
		if( int a = IsAlternativeName( lexbuf.c_str() ) )
			return a;

		// иначе просто имя,
		// ключевые слова определяются потом
		return NAME;	
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

// оператор вывода контейнера на стандартный вывод, используется при отладке
ostream &operator<<( ostream &out, const LexemContainer &lc )
{
	out << "CALL   \"operator<<( ostream &out, const LexemContainer &lc )\"\n ";
	for( list<Lexem>::const_iterator p = lc.begin();
		 p !=  lc.end(); p++ )
		out << (*p).GetBuf() << ' ';
	out << endl << endl;
	return out;
}


// получить следующую лексему
const Lexem &LexicalAnalyzer::NextLexem()
{
	// сохраняем текущую лексему, как предыдущую и вычисляем след.
	prevLxm = lastLxm;

	// если буферная лексема задана, вернем ее
	if( backLxm.GetCode() != 0 )
	{
		lastLxm = backLxm;
		backLxm = Lexem();		// очищаем буферную лексему
		return lastLxm ;
	}

	// если задан контейнер, вернем из него
	if( lexemContainer != NULL )
	{
		lastLxm = lexemContainer->front();
		lexemContainer->pop_front();
		if( lexemContainer->empty() )
			lexemContainer = NULL;

		return lastLxm;
	}

	// иначе режим считывания из файла	
	lastLxm.code = Lex(*inStream, lastLxm.pos);
	lastLxm.buf = lexbuf.c_str();
	
	// если это имя, то проверим его семантическое значение
	if( lastLxm.code == NAME )
	{
		int nc = LookupCPPKeywords(lastLxm.buf.c_str());
		if( nc != -1 )
			lastLxm.code = nc;
	}

	return lastLxm;
}
