// главный модуль программы - Application.cpp

#pragma warning(disable: 4786)
#include <nrc.h>
#include <windows.h>

using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "LexicalAnalyzer.h"
#include "Object.h"
#include "Scope.h"
#include "Class.h"
#include "Parser.h"
#include "Manager.h"


// объект приложение доступен для всех
Application theApp;


// в параметре конструктора задается имя модуля
TranslationUnit::TranslationUnit( PCSTR fnam ) 
{
	fileName = fnam;
	shortFileName = fnam;
	
	if( shortFileName.find("\\") != -1 || 
		shortFileName.find("/") != -1)
		shortFileName = shortFileName.DeleteRightWhileNot("\\/");

	inStream = fopen(fileName, "r");
	currentPos.line = 1;

	if( !inStream )
		theApp.Fatal("'%s' - невозможно открыть файл", fileName.c_str() );

	lexicalAnalyzer = new LexicalAnalyzer( inStream, currentPos );
	parser = new Parser(*lexicalAnalyzer);	

	// создаем систему управления областями видимости, при этом создаем
	// глобальную таблицу символов
	scope = new Scope( new GeneralSymbolTable(DEFAULT_GLOBAL_HASHTAB_SIZE, NULL) );
	isCompile = false;

	// выполнить внутенние декларации. В частности операторов выделения и 
	// освобождения памяти
	MakeImplicitDefinations();
}


// деструктор освобождает память
TranslationUnit::~TranslationUnit()
{
	fclose(inStream);
	delete lexicalAnalyzer;
	delete parser;
}


// выполнить встроенные декларации. Операторы new, new[], delete, delete[]
void TranslationUnit::MakeImplicitDefinations()
{
	SymbolTable *global = const_cast<SymbolTable *>(scope->GetFirstSymbolTable());
	DerivedTypeList empty, ptr;
	FunctionParametrList fpl;
	DerivedTypeList paramDtl;
		
	// создаем производный тип для delete
	paramDtl.AddDerivedType( new Pointer(false,false));
	fpl.AddFunctionParametr( new Parametr( 
		(BaseType*)&ImplicitTypeManager(KWVOID).GetImplicitType(), false, false, paramDtl,
		"", global, NULL, false) );
	empty.AddDerivedType( new FunctionPrototype(false, false, fpl,
		FunctionThrowTypeList(), false, false) );	

	// создаем производный тип для new
	fpl.ClearFunctionParametrList();
	fpl.AddFunctionParametr( new Parametr( 
		(BaseType*)&ImplicitTypeManager(KWINT, KWUNSIGNED).GetImplicitType(), false, false, 
		DerivedTypeList(), "", global, NULL, false) );	
	ptr.AddDerivedType( new FunctionPrototype(false, false, fpl, 
		FunctionThrowTypeList(), true, false) );
	ptr.AddDerivedType(new Pointer(false,false));

	struct
	{
		// имя идентификатора
		PCSTR name;

		// код оператора
		int opCode;

		// имя оператора
		PCSTR opName;

		// ссылка на список производных типов
		const DerivedTypeList *pdtl;
	} opmas[4] = {
		 "operator new", KWNEW, "new", &ptr ,
		 "operator new[]", OC_NEW_ARRAY, "new[]", &ptr ,
		 "operator delete", KWDELETE, "delete", &empty,
		 "operator delete[]", OC_DELETE_ARRAY, "delete[]", &empty
	};

	BaseType *btvoid = &const_cast<BaseType&>(ImplicitTypeManager(KWVOID).GetImplicitType());
	for( int i = 0; i<4; i++ )
		global->InsertSymbol(
			new OverloadOperator( opmas[i].name, global, 
				btvoid, false, false, *opmas[i].pdtl, false, Function::SS_NONE, 
				Function::CC_NON, opmas[i].opCode, opmas[i].opName) );
}


// запускаем процесс компиляции
void TranslationUnit::Compile() 
{
	isCompile = true;
	parser->Run();
}


// задать файл для генератора. Файл не должен существовать
void ApplicationGenerator::OpenFile( PCSTR fnam )
{
	INTERNAL_IF( fout != NULL );

#if !_DEBUG
	// проверяем, если файл существует, вывести ошибку
	if( fopen(fnam, "r") != NULL )
		theApp.Fatal("'%s' - файл уже существует; создание временного файла невозможно", fnam);
#endif

	fout = fopen(fnam, "w");
	if( !fout )
		theApp.Fatal("'%s' - невозможно создать временный файл для записи", fnam);
}


// сбросить текущий буфер в файл и очистить его
void ApplicationGenerator::FlushCurrentBuffer( ) 
{
	if( fputs(currentBuffer.c_str(), fout) == EOF )
		theApp.Fatal( "невозможно произвести запись в выходной файл" );
	currentBuffer = "";
}


// сбросить буфер отката в файл и очистить его	
void ApplicationGenerator::FlushUndoBuffer( ) 
{
	if( fputs(undoBuffer.c_str(), fout) == EOF )
		theApp.Fatal( "невозможно произвести запись в выходной файл" );
	undoBuffer = "";
}


// вывести сообщение на стандартный поток вывода
void Application::PutMessage( PCSTR head, PCSTR fname, const Position &pos, 
							 PCSTR fmt, va_list lst )
{
	char errbuf[512];	// буфер для формирования сообщения об ошибке

	_vsnprintf( errbuf, 512, fmt, lst );
	
	if( head )
		fprintf( stderr, "%s: ", head );

	if( fname )
      	fprintf( stderr, "%s", fname );

	if( pos.col > 0 || pos.line > 0 )
		fprintf( stderr, "(%d, %d): ", pos.line, pos.col );
	else
		fprintf( stderr, ": " );
	
	fprintf( stderr, "%s\n", errbuf );
}


// загрузить опции из командной строки
void Application::LoadOptions( int argc, char *argv[] )
{
}


// вывести ошибку
void Application::Error( const Position &pos, PCSTR fmt, ... )
{
	va_list vlst;	

	errcount++;
	va_start( vlst, fmt );
	PutMessage( "ошибка", translationUnit == NULL ? "<файл не открыт>" : 
			translationUnit->GetFileName().c_str(), pos, fmt, vlst );
	va_end( vlst );	

	if( errcount == MAX_ERROR_COUNT )
		Fatal( "количество ошибок превысило допустимый предел" );
}


// вывести предупреждение
void Application::Warning( const Position &pos, PCSTR fmt, ... )
{
	va_list vlst;	

	warncount++;
	va_start( vlst, fmt );
	PutMessage( "предупреждение", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), pos, fmt, vlst );
	va_end( vlst );	

	if( warncount == MAX_WARNING_COUNT )
		Fatal( "количество предупреждений превысило допустимый предел" );
}


// вывести фатальную ошибку
void Application::Fatal( const Position &pos, PCSTR fmt, ... )
{
	va_list vlst;	

	errcount++;
	va_start( vlst, fmt );
	PutMessage( "фатальная ошибка", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), pos, fmt, vlst );
	va_end( vlst );	
	exit( ERROR_EXIT_CODE );
}


// вывести внутренную ошибку компилятора
void Application::Internal( const Position &pos, PCSTR msg, PCSTR fname, int line )
{
	errcount++;	

	string fullMsg = msg;
	fullMsg = fullMsg + " --> (" + CharString(fname).DeleteRightWhileNot("\\/").c_str() + 
		", " + CharString(line).c_str() + ")";
	PutMessage( "внутренняя ошибка компилятора", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), pos, fullMsg.c_str(), 0 );	
	exit( ERROR_EXIT_CODE );
}


// вывести ошибку, которая обнаружена в опр. позиции
void Application::Error( PCSTR fmt, ... )
{
	va_list vlst;	

	errcount++;
	va_start( vlst, fmt );
	PutMessage( "ошибка", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(),	
		translationUnit == NULL ? Position() : translationUnit->GetPosition(), fmt, vlst );
	va_end( vlst );

	// проверяем количество выведенных ошибок и если достигнут предел,
	// выходим
	if( errcount == MAX_ERROR_COUNT )
		Fatal( "количество ошибок превысило допустимый предел" );
}

// вывести предупреждение, которая обнаружена в опр. позиции
void Application::Warning( PCSTR fmt, ... )
{
	va_list vlst;	

	warncount++;
	va_start( vlst, fmt );
	PutMessage( "предупреждение", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), 
		translationUnit == NULL ? Position() : translationUnit->GetPosition(), fmt, vlst );
	va_end( vlst );	

	if( warncount == MAX_WARNING_COUNT )
		Fatal( "количество предупреждений превысило допустимый предел" );
}

// вывести фатальную ошибку, которая обнаружена в опр. позиции
void Application::Fatal( PCSTR fmt, ... )
{
	va_list vlst;	

	errcount++;
	va_start( vlst, fmt );
	PutMessage( "фатальная ошибка", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), 
		translationUnit == NULL ? Position() : translationUnit->GetPosition(), fmt, vlst );
	va_end( vlst );	
	exit( ERROR_EXIT_CODE );
}

// вывести внутренную ошибку компилятора, которая обнаружена в опр. позиции
void Application::Internal( PCSTR msg, PCSTR fname, int line  )
{
	errcount++;	
	string fullMsg = msg;
	fullMsg = fullMsg + " --> (" + CharString(fname).DeleteRightWhileNot("\\/").c_str() +
		", " + CharString(line).c_str() + ")";

	PutMessage( "внутренняя ошибка компилятора", translationUnit == NULL ? "<файл не открыт>" : 
		translationUnit->GetFileName().c_str(), 
		translationUnit == NULL ? Position() : translationUnit->GetPosition(), 
		fullMsg.c_str(), 0 );	
	exit( ERROR_EXIT_CODE );
}


// компилировать файлы
int Application::Make()
{	
	// задаем выходной файл для генератора
	generator.OpenFile("out.txt");
	translationUnit = new TranslationUnit ("in.txt");		

	translationUnit->Compile();
	delete translationUnit;

	return SUCCESS_EXIT_CODE;
}


// стартовая точка
int main( int argc, char *argv[] )
{
	SetConsoleCP(1251); 
	SetConsoleOutputCP(1251);

	try { 
		theApp.LoadOptions(argc, argv);
		return theApp.Make();

	} catch( PCSTR msg ) {	// перехват с сообщением
		INTERNAL( msg );

	} catch( ... )	{		// перехват на самый крайний случай
		INTERNAL( "непредвиденная остановка компиляции" );
	}

	return 0;
}
