// модуль вывода ошибок - error.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace std;

#include <windows.h>

#include "cpplex.h"
#include "kpp.h"

#define ERRBUFSIZE	512


// счетчик ошибок
int errcount;


// счетчик предупреждений
int warncount;


// кодировка, в которой выводяться сообщения (1251, 866)
int code_page;


// запрещает вывод предупреждений
bool no_warnings = false;


// имя входного файла
extern string inname;


// вывод ошибки
static inline void ErrorMessage( const char *pred, const char *fmt, va_list lst )
{
	char errbuf[ERRBUFSIZE];	// буфер для формирования сообщения об ошибке

	_vsnprintf( errbuf, ERRBUFSIZE, fmt, lst );
	
	if(code_page == 866)
	{	
		char temp[ERRBUFSIZE], temp2[ERRBUFSIZE];		
		
		if( pred )
		{			
			CharToOem(pred, temp2);
			fprintf(stderr, "%s: ", temp2);
		}

		if(linecount == -1)	// фатальная ошибка может быть на стадии проверки опций
			_snprintf(temp, ERRBUFSIZE, "%s\n", errbuf);
		else
			_snprintf(temp, ERRBUFSIZE, "%s: %d: %s\n", inname.c_str(), 
				linecount, errbuf);

		CharToOem(temp, errbuf);
		fprintf(stderr, "%s", errbuf);
	}

	else
	{
		if( pred )
			fprintf( stderr, "%s: ", pred );

		if( linecount == -1 )
			fprintf( stderr, "Фатальная ошибка: %s\n", errbuf );

		else
			fprintf( stderr, "Фатальная ошибка: %s: %d: %s\n", inname, 
				linecount, errbuf );
	}	
}



// фатальная ошибка, выводит ошибку и выходит
void Fatal( const char *fmt, ... )
{
	va_list vlst;	

	va_start( vlst, fmt );
	ErrorMessage( "Фатальная ошибка", fmt, vlst );
	va_end( vlst );
	exit(ERROR_EXIT_CODE);	
}


// ошибка компиляции
void Error( const char *fmt, ... )
{
	va_list vlst;	

	errcount++;
	va_start( vlst, fmt );
	ErrorMessage( NULL, fmt, vlst );
	va_end( vlst );	
}


// предупреждение
void Warning( const char *fmt, ... )
{
	va_list vlst;	

	warncount++;
	if( no_warnings )
		return;

	va_start( vlst, fmt );
	ErrorMessage( "Предупреждение", fmt, vlst );
	va_end( vlst );	
}


