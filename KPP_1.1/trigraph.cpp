// обработка триграфов (первая фаза препроцессорной обработки) - trigraph.cpp


// триграфные последовательности и их символьные эквиваленты:
//  ??=  #        ??(  [
//  ??/  \        ??)  [
//  ??'  ^        ??!  |
//  ??<  {		  ??>  } 
//  ??-  ~ 

#include <cstdio>
#include <cstdlib>

#include "kpp.h"


// функция преобразует файл с триграфной 
// последовательностью в обычный CPP-файл
static inline void TrigraphToCPP( FILE *in, FILE *out )
{
	register int c;

	while( (c = fgetc(in)) != EOF )
	{
		if(c != '?')
		{
			fputc(c, out);
			continue;
		}

		if( (c = fgetc(in)) != '?' )
		{
			fputc('?', out); 
			ungetc(c, in);
			continue;
		}

		c = fgetc(in);

		// здесь начинается самое интересное
		switch((char)c)
		{
		case '=':  fputc('#', out);  break;
		case '/':  fputc('\\', out); break;
		case '\'': fputc('^', out);  break;
		case '<':  fputc('{', out); break;
		case '>':  fputc('}', out); break;
		case '-': fputc('~', out); break;
		case '(': fputc('[', out); break;
		case ')': fputc(']', out); break;
		case '!': fputc('|', out); break;
		default:  ungetc(c, in); fputc('?', out); fputc('?', out);
		}
	}
}


// функция обратная TrigraphToCPP
static inline void CPPToTrigraph( FILE *in, FILE *out )
{
	register int c;

	while( (c = fgetc(in)) != EOF )
	{
		// здесь начинается самое интересное
		switch((char)c)
		{
		case '#':  fprintf(out, "??"); fputc('=', out); break;
		case '\\':  fprintf(out, "??"); fputc('/', out);break;
		case '^': fprintf(out, "??");  fputc('\'', out); break;
		case '{':  fprintf(out, "??"); fputc('<', out); break;
		case '}':  fprintf(out, "??"); fputc('>', out); break;
		case '~': fprintf(out, "??");  fputc('-', out); break;
		case '[': fprintf(out, "??");  fputc('(', out); break;
		case ']': fprintf(out, "??");  fputc(')', out); break;
		case '|': fprintf(out, "??");  fputc('!', out); break;
		default:  fputc(c, out); break;
		}
	}

}


// безопасно открыть файл
FILE *xfopen(const char *name, const char *fmt)
{
	FILE *r;
	if((r = fopen(name, fmt)) == NULL)
		Fatal("не получаеться открыть файл '%s'", name);
	
	return r;
}


// функция преобразовывает файл с триграфами (in) в обычный файл (out)
void TrigraphPhase( const char *fnamein, const char *fnameout )
{
	FILE *in, *out;

	in = xfopen(fnamein, "r");
	out = xfopen(fnameout, "w");

	TrigraphToCPP(in, out);

	fclose(in);
	fclose(out);
}


