// заголовчный файл для лексического анализатора C++ - LexicalAnalyzer.h


#ifndef _LEXICAL_ANALYZER_H_INCLUDE
#define _LEXICAL_ANALYZER_H_INCLUDE


// макросы проверки символов, используются при синтаксическом и лексическом анализе
#define IS_NAME_START(c)  (isalpha( (c) ) || (c) == '_')
#define IS_NAME( c )	  ( IS_NAME_START( c ) || isdigit(c) )
#define IS_INT_LITERAL( c )   ((c) == INTEGER10 || (c) == UINTEGER10 ||	\
			  			       (c) == INTEGER16 || (c) == UINTEGER16 ||	\
						       (c) == INTEGER8  || (c) == UINTEGER8  ||	\
						       (c) == CHARACTER || (c) == WCHARACTER )

#define IS_LITERAL( c )		  (IS_INT_LITERAL(c) ||						\
							   (c) == STRING || (c) == WSTRING ||		\
							   (c) == LFLOAT || (c) == LDOUBLE ||		\
							   (c) == KWTRUE || (c) == KWFALSE )	


// если лексема является простым спецификатором типа, который можно использовать
// в качестве типа для явного вызова конструктора или деструктора
#define IS_SIMPLE_TYPE_SPEC( c )	( (c) == KWINT     || (c) == KWCHAR || (c) == KWBOOL || \
									  (c) == KWWCHAR_T || (c) == KWSHORT|| (c) == KWLONG || \
									  (c) == KWSIGNED  || (c) == KWUNSIGNED ||				\
									  (c) == KWFLOAT   || (c) == KWDOUBLE || (c) == KWVOID )


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
	OR_ASSIGN, COLON_COLON, DOUBLE_SHARP, ELLIPSES,

	// ключевые слова
	KWASM,	KWAUTO,	KWBOOL,	KWBREAK, 
	KWCASE,	KWCATCH , KWCHAR, KWCLASS,				// KWCATCH = 300
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


// базовый класс считывания 
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


// класс считывания из файла для С++ анализатора
class CppFileRead : public BaseRead 
{
	// указатель на входной поток
	FILE *in;


	// ссылка на объект лексического анализатора - позиция
	Position &pos;

public:
	CppFileRead( FILE *i, Position &p ) : in(i), pos(p) { }
	~CppFileRead( ) {   }

	// считывание из буфера в символ
	int operator>>( register int &c ) {
		c = fgetc(in);
		pos.col++;
		return c;
	}

	// возврат символа в поток
	void operator<<( register int &c ) { pos.col--; ungetc(c, in); }


	// метод вызывается при появлении новой строки в файле
	void NewLine() { pos.line++, pos.col = 1; }

	// получить позицию
	Position GetPosition() const { return pos; }

};


// структура описывает лексему, каждая считанная из файла лексема
// преобразуется к такому виду
class Lexem
{
	// буфер
	CharString	buf;

	// код лексемы
	int code;

	// позиция в файле
	Position pos;

public:

	// конструктор по умолчанию
	Lexem() {
		code = 0;
	}


	// конструктор с заданием параметров
	Lexem( const CharString &b, int c, const Position &p ) : buf(b), code(c), pos(p) {
	}

	// получить буфер
	const CharString &GetBuf() const {
		return buf;
	}

	// получить код
	int GetCode() const {
		return code;
	}

	// получить позицию
	const Position &GetPos() const {
		return pos;
	}

	// получить код, с помощью приведения к типу int
	operator int() const {
		return code;
	}

	// доступ к закрытым полям класса может иметь только класс LexicalAnalyzer
	// это сделано для того, чтобы только этот класс мог задавать значения лексемы
	friend class LexicalAnalyzer;
};


// контейнер лексем, может сохранять в себе неограниченное 
// количество лексем. Необходим для перечитывания некоторого блока программы.
// Активно взаимодействует с классом LexicalAnalyzer
typedef list<Lexem>	LexemContainer;


// оператор вывода контейнера на стандартный вывод, используется при отладке
ostream &operator<<( ostream &out, const LexemContainer &lc );


// главный класс модуля - лексический анализатор
class LexicalAnalyzer
{
	// последняя и предыдущая считанная лексема
	Lexem lastLxm, prevLxm;

	// буферная лексема, может возвращатся назад в поток
	Lexem backLxm;
		
	// указатель на входной поток 
	CppFileRead *inStream;

	// позиция в файле
	const Position &curPos;

	// указатель на контейнер, если не равен NULL, значит считывание
	// происходит из него, иначе из потока. Обнуляется если все лексемы
	// считаны
	LexemContainer *lexemContainer;

public:

	// объект лексического анализатора можно создать только 
	// указанием имени файла из которого следует производить чтение 
	// лексем
	LexicalAnalyzer( FILE *in, Position &pos ) : curPos(pos), lexemContainer(NULL) {
		inStream = new CppFileRead(in, pos);
	}


	// деструктор уничтожает входной поток
	~LexicalAnalyzer() {
		delete inStream;
	}


	// получить следующую лексему
	const Lexem &NextLexem();

	// получить предыдущую лексему
	const Lexem &PrevLexem() const {
		return prevLxm; 
	}

	// получить последнюю считанную лексему
	const Lexem &LastLexem() const {
		return lastLxm;
	}

	// возвращает последнюю считанную лексему в поток,
	// при следующем вызове NextLexem, будет получена именно
	// она. Если режим установлен в LAM_FILE_TO_CONTAINER, 
	// возвращаенная лексема второй раз не записывается	
	void BackLexem( ) {	
		// возвращать можно только одну лексему в поток, для большего
		// количества лексем, существет контейнер
		INTERNAL_IF( backLxm.GetCode() != 0 );		
		backLxm = lastLxm;		
	}

	// загрузить контейнер для считывания. При этом указатель на контейнер
	// должен равняться 0
	void LoadContainer( LexemContainer *lc ) {
		INTERNAL_IF( lexemContainer != NULL );
		lexemContainer = lc;
	}

	// задать последнюю считанную лексему
	void SetLastLexem( const Lexem &lxm ) {
		lastLxm = lxm;
	}
};


// ищет ключевые слова языка С++
int LookupCPPKeywords( const char *keyname );


// возвращает имя ключевого слова по коду
const char *GetKeywordName( int code );


#endif // end  _LEXICAL_ANALYZER_H_INCLUDE
