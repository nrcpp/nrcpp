// объявляются константы обозначающие пределы компилятора - limits.h


// максимальная глубина значения макроса
#define MAX_MACRO_DEEP  255


// макисмальная глубина директив '#if/#ifdef ...' 
#define MAX_IF_DEEP		80


// максимальное значение для типа CHAR
#define MAX_CHAR_VALUE	255


// максимальное значение для типа WCHAR_T
#define MAX_WCHAR_T_VALUE	32767


// максимальная глубина стека парсера
#define MAX_PARSER_DEEP		256 * 13			// 13 - количество функций парсера


// максимальная глубина вложенности include
#define MAX_INCLUDE_DEEP	256
