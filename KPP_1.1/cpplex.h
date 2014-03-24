// заголовчный файл для лексического анализатора C++ - cpplex.h



#define IS_NAME_START(c)  (isalpha( (c) ) || (c) == '_')
#define IS_NAME( c )	  ( IS_NAME_START( c ) || isdigit(c) )
#define IS_LITERAL( c )   ((c) == INTEGER10 || (c) == UINTEGER10 ||	\
						   (c) == INTEGER16 || (c) == UINTEGER16 ||	\
						   (c) == INTEGER8  || (c) == UINTEGER8  ||	\
						   (c) == CHARACTER || (c) == WCHARACTER )

// коды лексем С++ 
enum CPP_TOKENS {

	// любое имя, кроме ключевых слов
	NAME = 257,

	// константы
	STRING, WSTRING, CHARACTER, WCHARACTER, INTEGER10, INTEGER16, INTEGER8,
	UINTEGER10, UINTEGER16, UINTEGER8, LFLOAT, LDOUBLE,

	// операторы
	ARROW, INCREMENT, DECREMENT, DOT_POINT, ARROW_POINT,
	LEFT_SHIFT, RIGHT_SHIFT, LESS_EQU, GREATER_EQU, EQUAL, 
	NOT_EQUAL, LOGIC_AND, LOGIC_OR, MUL_ASSIGN, DIV_ASSIGN,
	PERCENT_ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, 
	LEFT_SHIFT_ASSIGN, RIGHT_SHIFT_ASSIGN, AND_ASSIGN, XOR_ASSIGN,
	OR_ASSIGN, COLON_COLON, ELLIPSES,

	// ключевые слова
	KWASM,	KWAUTO,	KWBOOL,	KWBREAK, 
	KWCASE,	KWCATCH, KWCHAR, KWCLASS,
	KWCONST, KWCONST_CAST, KWCONTINUE, KWDEFAULT,
	KWDELETE, KWDO,	KWDOUBLE, KWDYNAMIC_CAST,
	KWELSE,	KWENUM,	KWEXPLICIT, KWEXPORT,
	KWEXTERN, KWFALSE, KWFLOAT, KWFOR,
	KWFRIEND, KWGOTO, KWIF,	KWINLINE, 
	KWINT,	KWLONG,	KWMUTABLE, KWNAMESPACE,
	KWNEW, KWOPERATOR, KWPRIVATE, KWPROTECTED,
	KWPUBLIC, KWREGISTER, KWREINTERPRET_CAST, KWRETURN,
	KWSHORT, KWSIGNED, KWSIZEOF, KWSTATIC,
	KWSTATIC_CAST, KWSTRUCT, KWSWITCH, KWTEMPLATE,
	KWTHIS, KWTHROW, KWTRUE, KWTRY,
	KWTYPEDEF, KWTYPEID, KWTYPENAME, KWUNION,
	KWUNSIGNED, KWUSING, KWVIRTUAL, KWVOID,
	KWVOLATILE, KWWCHAR_T, KWWHILE
};


// коды лексем препроцессора
enum KPP_TOKENS {
	KPP_DEFINE = 257, KPP_ERROR,  KPP_UNDEF,  
	KPP_ELIF,   KPP_IF,     KPP_INCLUDE,
	KPP_ELSE,   KPP_IFDEF,  KPP_LINE,   
	KPP_ENDIF,  KPP_IFNDEF, KPP_PRAGMA
};


// класс считывания из файла
class BaseRead
{
public:
	BaseRead() { }
	virtual ~BaseRead() { }
 
	// считывание из потока в символ
	virtual int operator>>( register int &c ) = 0;

	// возврат символа в поток
	virtual void operator<<( register int &c ) = 0;
};


// класс считывания из буфера
class BufferRead : public BaseRead
{
	string buf;

	// текущий указатель на место в строке
	int i;	
public:
	BufferRead( string b ) : buf(b) { i = 0; }

	// считывание из буфера в символ
	int operator>>( register int &c ) {
		if( i == buf.length() ) 
			return (c = EOF);

		c = (unsigned char)buf[i++];
		return c;
	}

	// возврат символа в поток
	void operator<<( register int &c ) { if(c != EOF) i--; }
};


// класс считывания из файла
class FileRead : public BaseRead
{
	FILE *in;

public:
	FileRead( FILE *i ) : in(i) { }
	~FileRead( ) { fclose(in); }

	// считывание из буфера в символ
	int operator>>( register int &c ) {
		c = fgetc(in);
		return c;
	}

	// возврат символа в поток
	void operator<<( register int &c ) { ungetc(c, in); }
};


// счетчик строк
extern int linecount;


// буфер с содержимым лексемы
extern string lexbuf;


// процедура удаляет пробелы сзади
void SplitSpaces( string &s );


// функция преобразует строку s в строковый литерал
string &MakeStringLiteral( string &s );


// функция считывает строку из файла,
// возвращает fasle, если достигнут конец файла
bool ReadString( BaseRead &ob, string &fstr );


// считывает число из входного потока, пока
// функция isfunc возвращает true 
void ReadDigit( BaseRead &ob, int (*isfunc)(int) );


// игнорирует пробелы и новые строки
int IgnoreNewlinesAndSpaces( BaseRead &ob );


// игнорировать только пробелы
int IgnoreSpaces( BaseRead &ob, bool putspaces = true );


// функция возвращает код ключевого слова или -1
// в случае если такого ключевого слова нет
int LookupKeywordCode( const char *keyname, struct keywords *kmas, int szmas );


// ищет ключевые слова kpp
int LookupKppKeywords( const char *keyname );


// выделить лексему 'идентификатор'
inline int LexemName( BaseRead &ob );


// функция выделяет следующую лексему из
// потока in
int Lex( BaseRead &ob );
