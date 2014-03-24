// основной файл препроцессора - kpp.cpp

#include <cstdio>
#include <cstdlib>
#include <string>
#include <list>
#include <algorithm>
#include <ctime>

using namespace std;

#include "kpp.h"
#include "cpplex.h"
#include "macro.h"


// им€ входного файла
string inname;


// им€ выходного файла
string outname;


// подключаемые директории
extern list<string> IncludeDirs;


// запрещает вывод предупреждений
extern bool no_warnings;


// кодировка, в которой вывод€тьс€ сообщени€ (1251, 866)
extern int code_page;



// считывает и определ€ет макрос, используетс€ в опции '/D'
inline void DoOption_D( char *s )
{
	string temp;

	if( !IS_NAME_START(*s) )
		Fatal("kpp: не задан макрос в опции '/D'");

	while( IS_NAME(*s) )
		temp += *s, s++;

	if( *s == '\0' )
		mtab.Insert( Macro( temp,  "" ) );

	else if( *s == '=' )
	{
		string val;

		if( *(s+1) == '\"' )
		{
			val = s+2;
			if( *(val.end()-1) == '\"' )
				val.erase( val.end() - 1 );
		}

		else
			val = s+1;
		mtab.Insert( Macro( temp, val ) );
	}

	else
		Fatal("kpp: некорректный символ после макроса в опции '/D'");
}


// помещает в таблицу предопределенные макросы
// names - имена макросов, которые не нужно предопредел€ть
inline void InstallPredefined( list<string> &names )
{
#define NOT_INSTALL(n) (find(names.begin(), names.end(), n) != names.end())

	// предопределенные макросы:
	// __LINE__ - номер текущей строки
	// __FILE__ - им€ текущего файла
	// __TIME__ - врем€ компи€лции в формате чч:мм:сс
	// __DATE__ - дата компил€ции в формате гг:мм:дд
	// __cplusplus - объ€влен если компи€лци€ идет компил€тором C++ (задаетс€ компил€тором)
	// __STDC__ - объ€влен если компи€лци€ идет компил€тором C (задаетс€ компил€тором)
	string n[] = { "__LINE__", "__FILE__", "__TIME__", "__DATE__" };

	char buf[80];
	string v[] = { "", ('\"' + inname + '\"'), _strtime(buf), _strdate(buf) };

	for( int i = 0; i<4; i++ )
	{
		if( NOT_INSTALL( n[i] ) )
			continue;

		if( mtab.Find( (char *)n[i].c_str() ) != NULL )
			Warning("'%s': предопределенный макрос задан из командной строки", 
				n[i].c_str() );
		else
			mtab.Insert( Macro( n[i], v[i], true) );
	}
}


// разбирает опции из командной строки, и выполн€ет соотв.
// действи€
inline void ParseOptions( int argc, char *argv[] )
{
	// /UName - не предопредел€ть макрос NAME
	// /u     - не предопредел€ть все макросы
	// /IDir  - добавить директорию дл€ поиска файлов
	// /DName[=val] - объ€вить макрос
	// /W	  - запретить вывод предупреждений
	// /L=code_page - задать кодировку дл€ сообщений (по умолчанию dos866)
	// /?	  - вывести опции

	list<string> undef;	
	string temp;
	
	if( argc < 3 )
		Fatal( "kpp: не задан выходной файл" );

	for( int i = 1; i < argc; i++ )
	{
		if( *(argv[i]) == '/' )
		{
			int c = *(argv[i]+1);
			if( c == 'U' )
			{
				temp = (argv[i]+2);
				undef.push_back(temp);
			}

			else if( c == 'u' )
			{
				undef.push_back("__LINE__");
				undef.push_back("__FILE__");
				undef.push_back("__TIME__");
				undef.push_back("__DATE__");
			}

			else if( c == 'I' )
			{
				temp = (argv[i]+2);
				if(temp == "")
					Fatal("kpp: не задана директори€ в опции '/I'" );
				IncludeDirs.push_back(temp);
			}

			else if( c == 'D' )
				DoOption_D( argv[i]+2 );
			
			else if( c == 'W' )
				no_warnings = true;

			else if( c == 'L' )
			{
				temp = (argv[i]+2);
				int r = atoi(temp.c_str());

				if( r == 1251 || r == 866 )
					code_page = r;
				else
					Fatal("kpp: неизвестна€ кодировка в опции '/L'");				
			}

			else
				Fatal("kpp: '/%c' - неизвестна€ опци€, '/?' дл€ помощи", c );
		}

		else
		{
			temp = argv[i];
			if(inname == "")
				inname = temp;

			else if(outname == "")
				outname = temp;

			else
				Fatal("kpp: входной и выходной файл уже заданы");
		}
	}

	InstallPredefined(undef);
}


int main( int argc, char *argv[] )
{
	code_page = 866;
	linecount = -1;

	ParseOptions( argc, argv );
	linecount = 1;

	FullPreprocessing(inname.c_str(), outname.c_str());
	
	return errcount ? ERROR_EXIT_CODE : warncount;
}
