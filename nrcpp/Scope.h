// интерфейс к таблице символов - Scope.h


#ifndef _SCOPE_H_INCLUDE
#define _SCOPE_H_INCLUDE


// класс объявляется в модуле TypyziedEntity.h
class Identifier;

// используется в классе GeneralSymbolTable
class NameSpace;

// объявлен в Object.h
class Function;

	
// список идентификаторов. Единица хранения в хэш-таблице
typedef list<const Identifier *> IdentifierList;

// список из списков идентификаторов
typedef list<IdentifierList> ListOfIdentifierList;


// используется при поиске
class IdentifierListFunctor 
{
	// искомое имя
	const CharString &name;

public:
	// задаем искомый идентификатор
	IdentifierListFunctor( const CharString &nam )
		: name(nam) {
	}

	// фукнция
	bool operator() ( const IdentifierList &il ) const;		
};


// хэш-таблица
class HashTab
{
	// динамически создаваемая таблица
	ListOfIdentifierList *table;

	// размер таблицы
	unsigned int size;

	// функция хешириования, возвращает индекс по имени
	unsigned Hash( const CharString &key ) const;

public:
	// создадим таблицу
	HashTab( unsigned htsz );

	// деструктор уничтожает таблицу
	~HashTab();

	// найти элемент
	IdentifierList *Find( const CharString &key ) const;

	// вставить элемент в таблицу
	unsigned Insert( const Identifier *id );

	// очистить таблицу символов
	void Clear() {
		table->clear();
	}
};


// интерфейс к таблице символов. В нашей программе,
// понятия области видимости и таблицы символов эквивалентны, 
// поэтому можно считать данный класс базовым для всех
// сущностей, которые представляют собой области видимости
class SymbolTable
{
public:
	// поиск символа. В задаваемй извне список прицепливаются все
	// идентификаторы из данной области видимости, а также из дружеских.
	// Возвращает true, если хотя бы один символ найден
	virtual bool FindSymbol( const NRC::CharString &name, 
		IdentifierList &out ) const = 0;


	// метод FindSymbol интерфейса SymbolTable - метод поиска в данной
	// области видимости, а также в используемых областях (using). Для
	// классов, и именованных областей видимости, это не совсем корректно,
	// так как при явном указании имени ОВ, требуется возвратить
	// символ именно этой ОВ. С этой целью в интерфейсы классов ClassType, 
	// GeneralSymbol и NameSpace переопределяют дополнительный метод - FindInScope,
	// который ищет символ только в своей ОВ и никакой другой. Для 
	// остальных ОВ , вызов FindInScope равносилен вызову FindSymbol
	virtual bool FindInScope( const NRC::CharString &name, IdentifierList &out ) const {
		return FindSymbol(name, out);
	}


	// вставка символа. Вставку каждая таблица проводит по своему, наример
	// в прототипе функции считается ошибкой вставка параметра с тем же 
	// именем, а в глобальной области видимости вставка перегруженной функции
	// или типа считается верным. Поэтому если вставка выполняется, функция
	// возвращает true, в противном случае false. Вызывающая функция 
	// анализирует возвращамое значение
	virtual bool InsertSymbol( Identifier *id ) = 0;


	// очищает всю таблицу
	virtual void ClearTable() = 0;

	// методы-инспекторы, 
	// которые позволяют нам определить тип области видимости.
	// Если мы имеем дело с глобальной областью видимости
	virtual bool IsGlobalSymbolTable() const {
		return false;
	}

	// если таблица символов локальная
	virtual bool IsLocalSymbolTable() const {
		return false;
	}

	// если таблица символов именованная	
	virtual bool IsNamespaceSymbolTable() const {
		return false;
	}

	// если таблица символов класса	
	virtual bool IsClassSymbolTable() const {
		return false;
	}

	// таблица символов функции
	virtual bool IsFunctionSymbolTable() const {
		return false;
	}
};


// список областей видимости. Используется в GeneralSymbolTable,
// и производных от него для хранения using-областей видимости.
// Либо при поиске для сохранения уже обработанных ОВ
class SymbolTableList
{
	// список указателей на области видимости
	vector<const SymbolTable *> symbolTableList;

public:

	// пусто
	bool IsEmpty() const {
		return symbolTableList.empty();
	}

	// добавить область видимости
	void AddSymbolTable( const SymbolTable *st ) {
		symbolTableList.push_back(st);
	}

	// удалить первую область видимости в списке
	void PopFront() {
		if( !symbolTableList.empty() )
			symbolTableList.erase( symbolTableList.begin() );
	}

	// получить количество областей видимости в списке
	int GetSymbolTableCount() const {
		return symbolTableList.size();
	}

	// получить область видимости по индексу,
	// если она существует
	const SymbolTable &GetSymbolTable( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > symbolTableList.size()-1 );
		return *symbolTableList[ix];
	}

	// получить область видимости, если она не существует
	// возвращается NULL
	const SymbolTable *operator[]( int ix ) const {
		return (ix < 0 || ix > symbolTableList.size()-1) ? NULL : symbolTableList[ix];
	}

	// проверить наличие области видимости
	int HasSymbolTable( const SymbolTable *ptab ) const {
		int i = 0;
		for( vector<const SymbolTable *>::const_iterator p = symbolTableList.begin(); 
			 p != symbolTableList.end(); p++, i++ )

			// если указатели на области видимости равны
			if( *p == ptab )
				return i;
		return -1;
	}

	// очистить список
	void Clear() {
		symbolTableList.clear();
	}
};


// глобальная область видимости, а также базовый класс
// для именованной области видимости
class GeneralSymbolTable : public SymbolTable
{
	// сама таблица символов, реализованная в виде хэш-таблицы,
	// создается динамически
	HashTab	*hashTab;

	// список используемых областей видимости, может быть пустым
	SymbolTableList	usingList;

	// указатель на родительсую область видимости, для глобальных
	// равно самой себе
	const SymbolTable *parent;

	// в конструкторе задается размер хэш-таблицы, указатель
	// на родительскую ОВ. В случае если pp == NULL,
	// считается, что создается глобальная область видимости, иначе
	// создается именованная ОВ. Закрытый, т.к. должен создаваться
	// только NameSpace и TranslationUnit
	GeneralSymbolTable( unsigned htsz, SymbolTable *pp ) {
		parent = pp == NULL ? this : pp;
		hashTab = new HashTab(htsz);		
	}

	// производный класс может создавать объекты
	friend class NameSpace;
	
	// модуль трансляции создает глобальную ОВ
	friend class TranslationUnit;

public:
	
	// если таблица символов глобальная, т.е. у нее нет родительской ОВ 
	// и parent указывает сам на себя
	virtual bool IsGlobalSymbolTable() const {
		return parent == this;
	}

	// получить указатель на родительскую область видимости
	const SymbolTable &GetParentSymbolTable() const {
		return *parent;
	}

	// добавить using-область видимости, которая будет использоваться
	// исключительно при поиске
	void AddUsingNamespace( NameSpace *ns );

	// получить список дружеских областей видимости
	const SymbolTableList &GetUsingList() const {
		return usingList;
	}

	// поиск символа в локальной или глобальной области видимости,
	// ищет в своей области видимости и потом к найденному результату
	// прибавляет поиск в дружеских областях видимости (using). При этом
	// следует контролировать, чтобы процесс не зацикливался т.к. 2 области
	// могут быть дружескими по отношению друг к другу. Если не одно из
	// имен не найдено - возвращается копия строки name с NULL указателем
	virtual bool FindSymbol( const NRC::CharString &name, IdentifierList &out ) const;


	// производит поиск без учета using-областей, только глобальной (или локальной) ОВ
	virtual bool FindInScope( const NRC::CharString &name, IdentifierList &out ) const ;


	// вставка символа таблицу
	virtual bool InsertSymbol( Identifier *id ) ;

	// функция поиска, специально вызываемая при поиске с учетом используемых
	// областей видимости. Должна переопределеяться в NameSpace
	bool FindSymbolWithUsing( const CharString &name,
							 SymbolTableList &tested, IdentifierList &out ) const;
	// очищает всю таблицу
	virtual void ClearTable() {
		hashTab->Clear();
	}
};


// таблица символов функции
class FunctionSymbolTable : public SymbolTable
{
	// функция
	const Function &pFunction;
	
	// родительская область видимости
	const SymbolTable &parentST;
	
	// список используемых областей видимости, может быть пустым
	SymbolTableList	usingList;

	// список локальных идентификаторов
	ListOfIdentifierList localIdList;

	// функция поиска, специально вызываемая при поиске с учетом используемых
	// областей видимости. Должна переопределеяться в NameSpace
	bool FindSymbolWithUsing( const CharString &name,
					   SymbolTableList &tested, IdentifierList &out ) const;
public:

	// в конструкторе задается указатель на функцию и указатель на 
	// родительскую область видимости
	FunctionSymbolTable( const Function &fn, const SymbolTable &p )
		: pFunction(fn), parentST(p) {
	}

	// таблица символов функции
	bool IsFunctionSymbolTable() const {
		return true;
	}

	// получить саму функцию
	const Function &GetFunction() const {
		return pFunction;
	}

	// получить родительскую область видимости	
	const SymbolTable &GetParentSymbolTable() const {
		return parentST;
	}

	// добавить using-область видимости, которая будет использоваться
	// исключительно при поиске
	void AddUsingNamespace( NameSpace *ns );

	// получить список дружеских областей видимости
	const SymbolTableList &GetUsingList() const {
		return usingList;
	}

	// поиск символа в функциональной области видимости, потом в списке параметров функции
	// ищет в своей области видимости и потом к найденному результату
	// прибавляет поиск в дружеских областях видимости (using). 
	virtual bool FindSymbol( const NRC::CharString &name, IdentifierList &out ) const ;


	// производит поиск без учета using-областей, только глобальной (или локальной) ОВ
	virtual bool FindInScope( const NRC::CharString &name, IdentifierList &out ) const ;


	// вставка символа таблицу
	virtual bool InsertSymbol( Identifier *id ) ;


	// очищает всю таблицу
	virtual void ClearTable();
};


// таблица символов для блока и для других конструкций
class LocalSymbolTable : public SymbolTable
{
	// список списков идентификаторов
	ListOfIdentifierList *table;

	// родительская область видимости
	const SymbolTable &parentST;

public:
	// задаем родительскую ОВ
	LocalSymbolTable( const SymbolTable &pst )
		: table(NULL), parentST(pst) {
	}

	// освобождаем память занятую таблицы
	~LocalSymbolTable() {
		delete table;
	}

	// вернуть родительскую ОВ
	const SymbolTable &GetParentSymbolTable() const {
		return parentST;
	}

	// поиск символа	
	bool FindSymbol( const NRC::CharString &name, IdentifierList &out ) const;

	// вставка символа таблицу
	bool InsertSymbol( Identifier *id );

	// очищает всю таблицу
	void ClearTable() {
		// delete headId;
	}

	// возвращает true, если таблица символов локальная
	bool IsLocalSymbolTable() const {
		return true;
	}
};


// именованная область видимости, которая может хранится в таблице символов
// другой области видимости
class NameSpace : public Identifier, public GeneralSymbolTable
{	
	// если безимянная
	bool unnamed;

public:

	// задать параметры именованной области видимости
	NameSpace( const NRC::CharString &name, SymbolTable *entry, bool u ) 
		: Identifier(name, entry), 
		GeneralSymbolTable(DEFAULT_NAMESPACE_HASHTAB_SIZE, entry), unnamed(u) {
		 c_name = name.c_str();
	}

	// если именованная область видимости, без имени
	bool IsUnnamed() const {
		return unnamed;
	}

	// если именованная область видимости, виртуальный член
	virtual bool IsNamespaceSymbolTable() const {
		return true;
	}
};


// синоним области видимости
class NameSpaceAlias : public Identifier
{
	// именованная область видимости, синонимом которой является
	// данный алиас
	const NameSpace &ns;

public:
	// конструктор задает параметры
	NameSpaceAlias( const NRC::CharString &name, SymbolTable *entry, const NameSpace &n )
		: Identifier(name, entry), ns(n) {
	}

	// получить ссылку на область видимости
	const NameSpace &GetNameSpace() const {
		return ns;
	}
};

 
// система управления областями видимости
class Scope
{
	// список из таблиц символов, последний считается текущим
	list<SymbolTable *> symbolTableStack;

public:
	// создаем систему управления областями видимости, в параметре
	// уже созданная глобальная таблица 
	Scope( GeneralSymbolTable *gst ) {
		INTERNAL_IF( !gst || !gst->IsGlobalSymbolTable() );
		symbolTableStack.push_back(gst);
	}

	// получить текущую область видимости
	const SymbolTable *GetCurrentSymbolTable() const {
		return symbolTableStack.back();
	}

	// полуить первую область видимости, т.е. глобальную
	const SymbolTable *GetFirstSymbolTable() const {
		return symbolTableStack.front();
	}	

	// получить функциональную область видимости если это возможно,
	// иначе вернуть NULL
	const FunctionSymbolTable *GetFunctionSymbolTable() const {		
		if( GetCurrentSymbolTable()->IsLocalSymbolTable() ||
			GetCurrentSymbolTable()->IsFunctionSymbolTable() )
			return static_cast<const FunctionSymbolTable*>(&GetFunctionalSymbolTable());
		return NULL;
	}

	// получить ближайщую глобальную область видимости
	const SymbolTable &GetGlobalSymbolTable() const ;

	// получить ближайшую функциональную область видимости. 
	// Текущая область видимости обязательно должна быть локальной
	const SymbolTable &GetFunctionalSymbolTable() const;

	// создать новую область видимости и поместить ее в стек
	void MakeNewSymbolTable( SymbolTable *st ) {
		symbolTableStack.push_back(st);
	}

	// выталкнуть таблицу символов из стека, причем удалением
	// символов внутри таблицы должен заниматься вызывающий объект
	void DestroySymbolTable() {
		symbolTableStack.pop_back();
	}

	// вставить несколько областей видимости
	void PushSymbolTableList( SymbolTableList &stl ) {
		for( int i = 0; i<stl.GetSymbolTableCount(); i++ )
			symbolTableStack.push_back( (SymbolTable *)stl[i]);
	}

	// перегруженная
	void PushSymbolTableList( const list<SymbolTable *> &stl ) {
		for( list<SymbolTable *>::const_iterator p = stl.begin(); 
			 p != stl.end(); p++ )
			symbolTableStack.push_back( (SymbolTable *)(*p) );	
	}

	// глубокий поиск по всем областям видимости, 
	// поиск начинается с конца, т.е. с текущей ОВ и возвращается
	// первое соответствие - список идентификаторов имеющих
	// заданное имя. Если соотв. нет - false
	bool DeepSearch( const CharString &name, IdentifierList &out ) const;
};


// получить всю систему работы с областями видимости
inline Scope &GetScopeSystem() 
{
	return (Scope &)theApp.GetTranslationUnit().GetScopeSystem();
}


// получить текущую область видимости
inline SymbolTable &GetCurrentSymbolTable() 
{
	return 
		*(SymbolTable *)theApp.GetTranslationUnit().GetScopeSystem().GetCurrentSymbolTable();
}

#endif // end  _SCOPE_H_INCLUDE
