// модуль выполняет основную работу препроцессора - preproc.cpp


// после обработки триграфов и слешей,
// выполняется основная часть работы препроцессора - 
// * исполняются директивы препроцессора
// * подставляются значения макросов
// * игнорируются комментарии

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <list>
#include <algorithm>
#include <string>
#include <stack>

using namespace std;
#include "cpplex.h"
#include "kpp.h"
#include "macro.h"
#include "limits.h"


// имя входного файла
extern string inname;


// стек результатов для #if/#elif
extern stack<int> IfResults;


// новая пара - указатель на поток, имя файла
// используется в include
extern string IncName;


// структура описывает атрибуты файла
struct FileAttributes
{
	// строка на которой остановились в файле
	int line;


	// имя файла
	string fname;


	// состояние стека #if/#elif, при входе и выходе,
	// должно быть одинаковое значение
	int state;


	// указатель на буфер из которого происходт чтение
	FileRead *buf;

	FileAttributes( FileRead *b ) { 
		line = linecount + 1; 
		fname = inname;
		state = IfResults.size();
		buf = b;
	}

	~FileAttributes() { }
};


// функтор для поиска имени файла в списке подключенных файлов
class FuncIncludeStack
{
	string &str;
public:
	FuncIncludeStack( string &s ) : str(s)  { }
	bool operator()( FileAttributes &attr ) { return attr.fname == str; }
}; 


// список подключаемых файлов
list<FileAttributes> IncFiles;


// реализует 3 фазы препроцессорной обработки над файлом
static string inline Do3Phases( const char *fnamein );


// вывести в файл директиву #line
void PutLine( FILE *out )
{
	string s = inname;
	fprintf(out, "#line %d %s\n", linecount, MakeStringLiteral(s).c_str() );
}


// поместить атрибуты файла в список
static void inline PushFileAttr( FileRead * &file, FILE *out )
{
	if( IncFiles.size() == MAX_INCLUDE_DEEP )
		Fatal( "стек переполнен: слишком глубокое подключение файлов" );

	// такой файл уже есть в стеке или это текущий файл
	if( (find_if( IncFiles.begin(), IncFiles.end(), 
			FuncIncludeStack( IncName )) != IncFiles.end()) ||
		IncName == inname )
		Fatal( "'%s': рекурсивное подключение файла", IncName.c_str() );
	

	// сохраняем в списке номер строки, имя файла, 
	// состояние стека if
	IncFiles.push_back( FileAttributes( file ) );
	
	// проходим по новому файлу три раза: триграфы, слеши, комментарии
	string newin = Do3Phases( IncName.c_str() );	

	// задаем новый буфер
	file = new FileRead( xfopen(newin.c_str(), "r") );
	inname = IncName;
	linecount = 1;	


	// выводим #line
	PutLine( out );
}


// восстановить атрибуты файла
static inline bool PopFileAttr( FileRead * &file, FILE *out )
{
	if( IncFiles.empty() ) 	
		return false;

	FileAttributes &attr = IncFiles.back();

	if( attr.state != IfResults.size() )
		Fatal( "пропущен '#endif'" ); 

	linecount = attr.line;
	inname = attr.fname;

	delete file;		// при удалении буфера, файл закрывается
	file   = attr.buf;

	PutLine( out );
	IncFiles.pop_back();

	
	return true;
}


// функция считывает строку из файла,
// возвращает fasle, если достигнут конец файла
bool ReadString( BaseRead &ob, string &fstr )
{
	register int c;

	if( (c = IgnoreSpaces(ob, false)) == EOF )
		return false;

	for(;;)
	{
		ob >> c;
		
		if( c == '\n' )
			break;

		else if( c == EOF )
		{
			ob << c;
			break;
		}

		else 
			fstr += (char)c;
	}
	
	SplitSpaces(fstr);	// удаляем пробелы сзади
	return true;
}


// выполняет всю основную работу препроцессора
static inline void KppWork(FILE *in, FILE *out)
{
	string s;
	FileRead *file = new FileRead(in);
	int Directive( string, FILE * );

	do
	{
		while( ReadString( *file, s ) )
		{
			if( s[0] == '#' )
			{
				int r;
				if( (r = 
					Directive(s, out)) == KPP_INCLUDE )		// была директива #include, подключаем файл
				{
					PushFileAttr( file, out );
					s = "";
					continue;
				}

				else if( r == KPP_LINE  || r == KPP_PRAGMA )
					fprintf(out, "%s", s.c_str());	// выводим информацию для компилятора				
			}

			else
				if( PutOut )
					fprintf(out, "%s", Substitution(s).c_str() );
		
			fputc('\n', out);
			s = "";
			linecount++;
		}

	} while(  PopFileAttr(file, out) );

	if( !IfResults.empty() )
		Fatal( "пропущен '#endif'" );
}


// основная фаза препроцессирования после обработки триграфов и
// склеивания строк
void Preprocess( const char *fnamein, const char *fnameout )
{
	FILE *in, *out;
	
	in = xfopen(fnamein, "r");
	out = xfopen(fnameout, "w");

	KppWork(in, out);

	fclose(in);
	fclose(out);
}


// реализует 3 фазы препроцессорной обработки над файлом
static string inline Do3Phases( const char *fnamein )
{
	string fcur = fnamein;
	string fout;
	int p;
	

	// формируем временные файлы только в текущей директории
	// удаляем '\\' 
	if( (p = fcur.find_last_of("\\")) != -1 )
	{
		if( fcur.size() == p+1)
			fcur.erase( fcur.end() - 1 );
		else
			fcur.erase(0, p+1);
	}


	// преобразуем триграф
	fout = fcur + ".trig";
	TrigraphPhase( fnamein, fout.c_str() );

	// склеиваем строки со слешами	
	fout = fcur + ".slash";
	ConcatSlashStrings( (fcur + ".trig").c_str(), fout.c_str() );

	// игнорируем комментарии
	fout = fcur + ".comment";
	IgnoreComment( (fcur + ".slash").c_str(), fout.c_str() );

	return fout;
}


// полное препроцессирование файла:
// 1. преобразует триграфы
// 2. склеивает строки со слешами
// 3. игнорирует комментарии
// 4. выплняет команды и подставляет макросы
void FullPreprocessing( const char *fnamein, const char *fnameout )
{
	string fout = Do3Phases( fnamein );

	// основное препроцессирование
	Preprocess( fout.c_str(), fnameout );
} 
