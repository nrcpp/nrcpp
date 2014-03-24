// предопределенные коды для пакетов - PackCode.h
// все коды начинаются с префикса 0xC0D


// последовательность спецификаторов типа
#define PC_TYPE_SPECIFIER_LIST	0xC0D0


// составное имя
#define PC_QUALIFIED_NAME		0xC0D1


// определение перечисления
#define PC_ENUM_DEFINATION		0xC0D2


// определение класса
#define PC_CLASS_DEFINATION		0xC0D3


// вторая часть декларации объекта: ptr-операторы и идентификатор
#define PC_DIRECT_DECLARATOR	0xC0D4


// пакет в котором произошла ошибка конструирования
#define PC_ERROR_CHILD_PACKAGE	0xC0D5


// декларатор при объявлении объекта
#define PC_DECLARATOR			0xC0D6


// прототип функции, детьми являются параметры
#define PC_FUNCTION_PROTOTYPE	0xC0D7


// список деклараций
#define PC_PARAMETR				0xC0D8


// указатель на член
#define PC_POINTER_TO_MEMBER	0xC0D9


// имя типа (класс, перечисление, либо typedef)
#define PC_QUALIFIED_TYPENAME	0xC0D9


// throw-спецификация в объявлении прототипа функции
#define PC_THROW_TYPE_LIST		0xC0D10


// массив в декларации 
#define PC_ARRAY				0xC0D11


// перегруженный оператор
#define PC_OVERLOAD_OPERATOR	0xC0D12


// деструктор
#define PC_DESTRUCTOR			0xC0D13


// тип приведения в выражениях и в операторе приведения
#define PC_CAST_TYPE			0xC0D14


// базовый класс, который считывается при определении класса
#define PC_BASE_CLASS			0xC0D15


// перегруженный оператор приведения типа
#define PC_CAST_OPERATOR		0xC0D16


// тип в throw-спецификации
#define PC_THROW_TYPE			0xC0D17


// декларация или абстрактный декларатор, где существует неоднозначность
// между объявлением и выражением
#define PC_DECLARATION			0xC0D18


// выражение, код применим только к ExpressionPackage
#define PC_EXPRESSION			0xC0D19


// декларация класса или перечисления при считывании локальной инструкции
#define PC_CLASS_DECLARATION	0xC0D20

