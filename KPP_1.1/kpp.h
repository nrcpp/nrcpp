// объ€влени€ дл€ модулей препроцессора - kpp.h


#include "error.h"


// функци€ преобразовывает файл с триграфами (in) в обычный файл (out)
void TrigraphPhase( const char *fnamein, const char *fnameout );


// скеливает строки со слешами
void ConcatSlashStrings( const char *fnamein, const char *fnameout );


// основна€ фаза препроцессировани€ после обработки триграфов и
// склеивани€ строк
void Preprocess( const char *fnamein, const char *fnameout );


// игнорирует комментарии в всем файле, если установлен флаг all
// иначе игнорирует только в командах препроцессора
// если установлен флаг line - игнорировать и строчные комментарии
void IgnoreComment( const char *fnamein, const char *fnameout );


// игнорировать целую строку считыва€ из файла в файл
void IgnoreStringLiteral( FILE *in, FILE *out );


// полное препроцессирование файла:
// 1. преобразует триграфы
// 2. склеивает строки со слешами
// 3. игнорирует комментарии
// 4. выплн€ет команды и подставл€ет макросы
void FullPreprocessing( const char *fnamein, const char *fnameout );


// безопасно открыть файл
FILE *xfopen(const char *name, const char *fmt);


// возвращает значение константы: hex, oct, char, wchar_t, int
int CnstValue( char *s, int code );


// флаг указывает на необходимость вывода считанного из файла
// требуетс€ при выполнении директив уусловной компил€ции (#if, #ifdef, ...)
extern bool PutOut;