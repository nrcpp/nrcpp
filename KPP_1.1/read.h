// объ€вл€етс€ класс буферизированного считывани€ - read.h


enum READ_MODE { MODE_BUFFER, MODE_FILE, MODE_UNCKNOW };
typedef READ_MODE WRITE_MODE;


class ReadFileWriteFile 
{
};


class Read
{
	// входной буфер
	string inbuf;


	// выходной буфер 
	string outbuf;


	// входной файл 
	FILE *in;


	// выходной файл
	FILE *out;


	// текущий режим входного считывани€
	READ_MODE inmode;


	// текущий режим записи
	WRITE_MODE outmode;


public:
	Read() { in = out = NULL; inmode = outmode = MODE_UNCKNOW; }
	Read( FILE *i, FILE *o ) { in = i, o = out; inmode = outmode = MODE_FILE; }
	
	
};