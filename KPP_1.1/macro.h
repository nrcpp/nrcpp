// объ€вление таблицы макросов


#include "hash.h"
#define MACROTAB_SIZE	531


// структура параметров макросов
struct Param { 
	string name, val; 
	bool operator==( const Param &ob ) { return ob.name == name; }
};


// структура макросов
struct Macro
{
	string name, val;
	enum { MACROS, FUNCTION } type;
	list<Param> params;
	bool pred;	// если макрос предопределен - true
	
	Macro(string n, string v, bool p = false) { 
		name = n, val = v, type = MACROS;
		pred = p;
	}

	Macro(string n, string v, list<Param> p) { 
		name = n, val = v, params = p, type = FUNCTION;
		pred = false;
	}

	bool operator==( const Macro &ob ) { return ob.name == name; }
};


// таблица макросов
class MacroTable : public HashTab<Macro, MACROTAB_SIZE>
{
	// возвращает итератор на объект
	bool _Find( char *name, list<Macro>::iterator &i ) {
		list<Macro> &p = HashFunc( name );

		return (i = find(p.begin(), p.end(), 
			Macro(string(name), string(""))) ) == p.end() ? false : true;
	}

public:
	// возвращает указатель на объект в табице, или NULL
	Macro *Find( char *name );


	// вставл€ет элемент в таблицу
	void Insert( Macro ob );


	// удал€ет элемент из таблицы
	void Remove( char *name );
};


// функтор дл€ поиска параметра
class FuncParam 
{
	string &str;
public:
	FuncParam( string &s ) : str(s) { }
	bool operator() ( Param &p ) { return p.name == str; }
};


extern MacroTable mtab;


// проходит по строке s и выполн€ет макроподстановку,
// dodef указывает на необходимость обработки оператора defined
// в диреткивах #if/#elif
string Substitution( string buf, bool dodef = false );
