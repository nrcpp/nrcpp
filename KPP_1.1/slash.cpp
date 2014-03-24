// после обработки триграфов, обрабатываем слеши - slash.cpp


#include <cstdio>
#include <cstdlib>

#include "kpp.h"


// функция получает файл со слешами, 
// возвращает на выход файл без слешей
static inline void SlashWork( FILE *in, FILE *out )
{
	register int c, pc;
	int line = 0;

	while((c = fgetc(in)) != EOF)
	{
		if(c == '\\')
		{
			if((pc = fgetc(in)) == '\n')
			{
				line++;
				continue;
			}

			else if(pc == EOF)
				Fatal("неожиданный конец файла: после '\\'");
			ungetc(pc, in);
		}
			

		else if(c == '\n')
		{
			if(line)
				while(line)
					fputc('\n', out), line--;
		}
	
		fputc(c, out);
	}
}


// основная функция, скеливает строки со слешами
void ConcatSlashStrings( const char *fnamein, const char *fnameout )
{
	FILE *in, *out;
	
	in = xfopen(fnamein, "r");
	out = xfopen(fnameout, "w");

	SlashWork(in, out);

	fclose(in);
	fclose(out);

}
