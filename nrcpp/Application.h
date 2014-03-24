// содержит объявления используемые во многих модулях - Application.h


#ifndef _APPLICATION_H_INCLUDE
#define _APPLICATION_H_INCLUDE

// подключаем модуль времени для режима проверки времени
#include <ctime>


// вывод внутренней ошибки компилятора
#define INTERNAL( msg )	(theApp.Internal( msg, __FILE__, __LINE__ ) )

// вывод внутренней ошибки компилятора, если условие ложно
#define INTERNAL_IF( exp )  ( (exp) ? INTERNAL( "\"" #exp "\"" ) : (void)0 )


// выход с ошибками
#define ERROR_EXIT_CODE		-1


// нормальное завершение приожения
#define SUCCESS_EXIT_CODE	0


// синоним типа - указатель на константную строку
typedef const char *PCSTR;


// структура описывает позицию в файле, активно используется в 
// лексическом анализаторе
struct Position
{
	// строка, колонка
	unsigned line, col;

	// конструткор
	Position( unsigned l = 0, unsigned c = 0 ) : line(l), col(c) {
	}
};

// класс объявлен в LexicalAnalyzer.h
class LexicalAnalyzer;

// класс объявлен в Parser.h
class Parser;

// класс объявлен в Scope.h
class Scope;

// класс объявлен в Class.h
class ClassType;

// объявлен в Object.h
class Identifier;


// модуль компиляции
class TranslationUnit
{
	// полное имя модуля
	CharString fileName;

	// короткое имя, без полного пути
	CharString shortFileName;

	// текущая позиция в файле
	Position currentPos;


	// указатель на поток файла
	FILE *inStream;

	// синтаксический анализатор модуля
	LexicalAnalyzer *lexicalAnalyzer;

	// синтаксический анализатор модуля
	Parser *parser;

	// система управления областями видимости
	Scope *scope;

	// true, если компиляция запущена
	bool isCompile;

	// выполнить встроенные декларации. Операторы new, new[], delete, delete[]
	void MakeImplicitDefinations();

public:

	// в параметре конструктора задается имя модуля
	TranslationUnit( PCSTR fnam ) ;

	// деструктор освобождает память
	~TranslationUnit();

	// запустить процесс компиляции
	void Compile();

	// получить текущую позицию
	Position GetPosition() const { return currentPos; }


	// получить имя файла
	const CharString &GetFileName() const { return fileName; }

	// получить короткое имя файла
	const CharString &GetShortFileName() const { return shortFileName; }

	// получить систему управления областями видимости
	const Scope &GetScopeSystem() const {
		return *scope;
	}

	// получить парсер
	const Parser &GetParser() const {
		return *parser;
	}
};


// генератор приложения, связан напрямую с файлом
class ApplicationGenerator
{
	// текущий буфер
	string currentBuffer;

	// буфер отката
	string undoBuffer;

	// указатель на выходной поток
	FILE *fout;

public:
	// задать файл. Файл не должен существовать
	ApplicationGenerator( ) 
		: fout(NULL) {
	}

	// закрыть файл
	~ApplicationGenerator() {
		if( fout )
			fclose(fout);
	}

	// открыть файл
	void OpenFile( PCSTR fnam );

	// генерировать в текущий буфер
	void GenerateToCurrentBuffer( const string &buf ) {
		currentBuffer += buf;
	}

	// генерировать в буфер отката
	void GenerateToUndoBuffer( const string &buf ) {
		currentBuffer += buf;
	}

	// сбросить текущий буфер в файл и очистить его
	void FlushCurrentBuffer( );

	// сбросить буфер отката в файл и очистить его	
	void FlushUndoBuffer( );
};


// приложение
class Application
{
	// список cpp-файлов для компиляции
	TranslationUnit *translationUnit;

	// генератор приложения. Используется транслятором для вывода сгенерированной
	// информации в выходной файл
	ApplicationGenerator generator;
	
	// счетчик ошибок и предупреждений
	int errcount, warncount;
	
	// время начала работы программы
	clock_t startTime;	

	// вывести сообщение на стандартный поток вывода
	void PutMessage( PCSTR head, PCSTR fname, const Position &pos, PCSTR fmt, va_list lst );

public:
	// конструктор
	Application()
		: translationUnit(NULL), errcount(0), warncount(0), startTime( clock() ){		
	} 

	// деструктор выводит время работы программы
	~Application() {				
//		printf( "Время компиляции: %lf секунд\n", 
//				(double)(clock() - startTime) / CLOCKS_PER_SEC );		
	}


	// получтиь данные текущего компилируемого файла
	const TranslationUnit &GetTranslationUnit() const {
		return *translationUnit;
	}

	// вернуть генератор
	ApplicationGenerator &GetGenerator() {
		return generator;
	}

	// если компилирование находится в режиме диагностики вернуть true
	bool IsDiagnostic() const {
		return errcount > 0;
	}

	// загрузить опции из командной строки
	void LoadOptions( int argc, char *argv[] );

	// компилировать файлы
	int Make();

	// вывести ошибку, которая обнаружена в опр. позиции
	void Error( const Position &pos, PCSTR fmt, ... );

	// вывести предупреждение, которая обнаружена в опр. позиции
	void Warning( const Position &pos, PCSTR fmt, ... );

	// вывести фатальную ошибку, которая обнаружена в опр. позиции
	void Fatal( const Position &pos, PCSTR fmt, ... );

	// вывести внутренную ошибку компилятора, которая обнаружена в опр. позиции
	void Internal( const Position &pos, PCSTR msg, PCSTR fname, int line  );

	// вывести ошибку
	void Error( PCSTR fmt, ... );

	// вывести предупреждение
	void Warning( PCSTR fmt, ... );

	// вывести фатальную ошибку
	void Fatal( PCSTR fmt, ... );

	// вывести внутренную ошибку компилятора
	void Internal( PCSTR msg, PCSTR fname, int line );

};


// объект приложение доступен для всех
extern Application theApp;


#endif		// end _APPLICATION_H_INCLUDE
