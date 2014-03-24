// объявление интерфейса к базовому типу, классу, шаблону - Class.h


#ifndef _CLASS_H_INCLUDE
#define _CLASS_H_INCLUDE


// сущность - базовый тип
// этот класс является базвым для всех классов,
// который могут быть базовыми типами
class BaseType
{
public:

	// возможные коды базовых типов
	enum BT {
		BT_NONE,
		BT_BOOL, BT_CHAR, BT_WCHAR_T, BT_INT, BT_FLOAT, BT_DOUBLE, BT_VOID,
		BT_CLASS, BT_STRUCT, BT_ENUM, BT_UNION,	
	};

	// возможные коды модификаторов знака
	enum MSIGN	{
		MN_NONE, MN_SIGNED, MN_UNSIGNED, 
	};

	// возможные коды модификаторов размера
	enum MSIZE	{
		MZ_NONE, MZ_LONG, MZ_SHORT,
	};

private:
	// код базового типа, структура кода
	BT baseTypeCode;

	// модификатор размера (актуален только для double, int)
	MSIZE sizeModifier;

	// модификатор знака (актуален только для char, int)
	MSIGN signModifier;

public:
	
	// в конструкторе должен задаваться код типа
	BaseType( BT btc, MSIZE size = MZ_NONE, MSIGN sign = MN_NONE ) 
		: baseTypeCode(btc), sizeModifier(size), signModifier(sign) {				
	}

	// если тип не имеет знака (актуален только для int и char)
	bool IsUnsigned() const {
		return signModifier == MN_UNSIGNED;
	}

	// если тип имеет знак (актуален только для int и char)
	bool IsSigned() const {
		return signModifier == MN_SIGNED;
	}

	// если тип имеет модификатор размера long (актуален для int, double)
	bool IsLong() const {
		return sizeModifier == MZ_LONG;
	}

	// если тип имеет модификатор размера short (актуален для int)
	bool IsShort() const {
		return sizeModifier == MZ_SHORT;
	}

	// если базовый тип является встроенным, т.е. bool, char, int, float, double, void,	
	bool IsBuiltInType() const {		
		return baseTypeCode == BT_INT || baseTypeCode == BT_CHAR ||
			baseTypeCode == BT_FLOAT || baseTypeCode == BT_DOUBLE ||
			baseTypeCode == BT_BOOL || baseTypeCode == BT_VOID;
	}

	// если базовый тип является классом
	bool IsClassType() const {			
		return baseTypeCode == BT_CLASS || baseTypeCode == BT_STRUCT || 
			baseTypeCode == BT_UNION;
	}

	// если базовый тип является перечислимым типом
	bool IsEnumType() const {
		return baseTypeCode == BT_ENUM ;
	}

	// получить модификатор размера
	MSIZE GetSizeModifier() const {
		return sizeModifier;
	}

	// получить модификатор знака
	MSIGN GetSignModifier() const {
		return signModifier;
	}

	// получить код базового типа
	BT GetBaseTypeCode() const {
		return baseTypeCode;
	}

};


// класс представляет собой перечислимый тип
class EnumType : public Identifier, public BaseType, public ClassMember
{
	// спецификатор доступа, существует если перечислимый тип
	// является членом класса
	AS accessSpecifier;

	// список констант
	EnumConstantList constantList;

	// тип явялется не завершенным
	bool uncomplete;

public:
	// конструктор задает имя перечислимого типа, его область видимости,
	// и список констант
	EnumType( const NRC::CharString &name, SymbolTable *entry, AS as ) : 
	  Identifier(name, entry), BaseType(BT_ENUM), accessSpecifier(as)  {

		uncomplete = true;
	}

	// если перечислимый тип не содержит констант
	bool IsEmpty() const {
		return constantList.IsEmpty();
	}

	// если тип является не завершенным
	bool IsUncomplete() const {

		// тип является завершенным только когда вызывается
		// метод Complete и задаются константы
		return uncomplete;		
	}

	// если тип является членом класса
	virtual bool IsClassMember() const {
		return accessSpecifier != NOT_CLASS_MEMBER;
	}

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}

	// получить список констант
	const EnumConstantList &GetConstantList() const {
		return constantList;
	}

	
	// завершить построение типа задав список констант перечисления
	// завершить построение может лишь класс создаетль, который
	// является дружественным для этого класса
	void CompleteCreation( const EnumConstantList &cl ) {
		INTERNAL_IF( !uncomplete );
		constantList = cl;
		uncomplete = false;		// тип является завершенным
	}
};


// при наследовании - характеристика базового класса
class BaseClassCharacteristic
{
	// виртуальное наследование
	bool virtualDerivation;

	// спецификатор доступа при наследовании
	ClassMember::AS accessSpecifier;

	// указатель на класс. 
	const ClassType &pClass;

public:

	// конструктор строит объект
	BaseClassCharacteristic( bool vd, ClassMember::AS as, const ClassType &p ) 
		: virtualDerivation(vd), accessSpecifier(as), pClass(p) {
	}

	// если виртуальное наследование
	bool IsVirtualDerivation() const {
		return virtualDerivation;
	}

	// получить спецификатор досутупа
	ClassMember::AS GetAccessSpecifier() const {
		return accessSpecifier;
	}

	// получить указатель на класс
	const ClassType &GetPointerToClass() const {
		return pClass;
	}
};


// в векторе должен хранится интеллектуальный указатель 
typedef SmartPtr<BaseClassCharacteristic> PBaseClassCharacteristic;


// список базовых классов
class BaseClassList
{
	// список базовых классов
	vector<PBaseClassCharacteristic> baseClassList;

public:

	// если список пуст
	bool IsEmpty() const {
		return baseClassList.empty();
	}

	// характеристику по индексу
	const PBaseClassCharacteristic &GetBaseClassCharacteristic( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > GetBaseClassCount()-1 );
		return baseClassList[ix];
	}

	// получить характеристику по индексу, если она отстутвует, вернуть 0
	PBaseClassCharacteristic operator[]( int ix ) const {
		return (ix < 0 || ix > GetBaseClassCount()-1) ? 
			PBaseClassCharacteristic(NULL) : baseClassList[ix];
	}

	// количество базовых классов
	int GetBaseClassCount() const {
		return baseClassList.size();
	}

	// добавить характеристику, не константный метод, вызывается при
	// построении списка
	void AddBaseClassCharacteristic( const PBaseClassCharacteristic &bcc ) {
		baseClassList.push_back(bcc); 
	}

	// проверяет, имеется ли класс в списке
	int HasBaseClass( const ClassType *cls ) const {
		for( int i = 0; i<baseClassList.size(); i++ )
			if( &baseClassList[i]->GetPointerToClass() == cls )
				return i;

		return -1;
	}

	// очистить список
	void ClearBaseClassList() {		
		baseClassList.clear();
	}
};


// в векторе должен хранится интеллектуальный указатель 
typedef SmartPtr<ClassMember> PClassMember;


// список членов класса. Следует учесть, что класс является областью видимости
// и может содержать перегруженные имена, таким образом члены храняться в таком
// виде:  ListOfIdentifierList
class ClassMemberList
{
	// список членов в виде перегруженных элементов
	mutable ListOfIdentifierList memberList;

	// порядок следования членов класса при объявлении,
	// используется для индексированного доступа
	vector<PClassMember> order;

public:

	// возвращает true, если список пуст
	bool IsEmpty() const {
		return order.empty();
	};

	// получить количество членов
	int GetClassMemberCount() const {
		return order.size() ;
	}	

	// получить член по индексу, индекс не должен выходить за пределы
	// списка
	const PClassMember &GetClassMember( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > GetClassMemberCount() - 1 );
		return order[ix];
	}

	// получить член, если индекс выходит за пределы - вернуть NULL
	PClassMember operator[]( int ix ) const {
		return (ix < 0 || ix > GetClassMemberCount() - 1) ? 
			PClassMember(NULL) : order[ix];
	}

	// получить все члены с именем name в виде списка строк,
	// если такого члена нет, вернуть пустой список
	IdentifierList *FindMember( const CharString &name ) const;
 
	// вставить член в список. При этом если список перегруженных
	// членов не создан, он создается, а если создан, член добавляется
	// в конец этого списка
	void AddClassMember( PClassMember cm );

	// очистить список членов одновременным освобождением памяти
	void ClearMemberList();
};


// представляет сущность друг. Другом может быть либо класс,
// либо функция, либо шаблонный параметр (который также имеет общность с классом)
class ClassFriend
{
	union
	{
		// указатель на класс или шаблонный параметр
		ClassType *pClass;

		// указатель на функцию
		Function *pFunction;
	};

	// распознаватель того, какого типа друг
	bool isClass;

public:

	// создаем друга типа класс
	ClassFriend( ClassType *p ) : pClass(p), isClass(true) {
	}

	// создаем друга функцию
	ClassFriend( Function *p ) : pFunction(p), isClass(false) {
	}

	// если друг является классом
	bool IsClass() const {
		return isClass;
	}

	// получить функцию, но только при условии что в конструкторе была
	// задана функция
	const Function &GetFunction() const {
		INTERNAL_IF( isClass );
		return *pFunction;
	}

	// получить класс, но при условии что в конструкторе был задан
	// класс
	const ClassType &GetClass() const {
		INTERNAL_IF( !isClass );
		return *pClass;
	}
};


// список друзей класса
class ClassFriendList
{
	// список друзей
	vector<ClassFriend> friendList;

public:

	// возвращает true, если список пуст
	bool IsEmpty() const {
		return friendList.empty();
	};

	// получить количество друзей класса
	int GetClassFriendCount() const {
		return friendList.size() ;
	}	

	// получить друга по индексу, индекс не должен выходить за пределы
	// списка
	const ClassFriend &GetClassFriend( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > GetClassFriendCount() - 1 );
		return friendList[ix];
	}

	// получить друга, если индекс выходит за пределы - вернуть NULL
	const ClassFriend *operator[]( int ix ) const {
		return (ix < 0 || ix > GetClassFriendCount() - 1) ? NULL : &friendList[ix];
	}

	// добавить друга класса. Другом может класс, функци
	void AddClassFriend( ClassFriend cf ) {
		friendList.push_back(cf);
	}

	// очистить список членов одновременным освобождением памяти
	void ClearMemberList() {
		friendList.clear();
	}

	// возвращает индекс друга по указателю, если друга нет вернуть -1
	int FindClassFriend( const Identifier *p ) const; 
};


// тип - список конструкторов класса
typedef list<const ConstructorMethod *> ConstructorList;

// тип - список перегруженных операторов
typedef list<const ClassCastOverloadOperator *> CastOperatorList;

// тип - список виртуальных функций
typedef list<const Method *> VirtualFunctionList;


// класс
class ClassType : public Identifier, public SymbolTable, public BaseType, public ClassMember
{
	// спецификатор доступа, может остутствовать, если класс не
	// вложен в другой класс
	ClassMember::AS	accessSpecifier;

	// список базовых классов
	BaseClassList baseClassList;

	// список членов класса
	ClassMemberList memberList;

	// список друзей класса
	ClassFriendList friendList;

	// список (указателей) конструкторов класса. Хранится отдельно,
	// т.к. необходим при приведении типа и конструировании объекта
	ConstructorList constructorList;
	
	// список операторов приведения
	CastOperatorList *castOperatorList;

	// список виртуальных функций
	VirtualFunctionList *virtualFunctionList;

	// указатель на деструктор класса
	const Method *destructor;

	// если класс не полностью объявлен, true
	bool uncomplete;

	// количество абстрактных методов содержащихся в классе,
	// в том числе и в базовых. Если количество методов = 0,
	// класс не является абстрактным
	short abstractMethodCount;

	// количество виртуальных методов в классе. Увеличивается
	// из Method::SetVirtual
	short virtualMethodCount;

	// если класс полиморфный, т.е. если базовый класс или
	// сам класс содержит хотя-бы одну виртуальную функцию
	bool polymorphic;

	// установлен в true, если класс создает собственную vm-таблицу,
	// это происходит, когда в классе объявляется новая виртуальная функция,
	// которой еще нет в иерархии
	bool madeVmTable;

	// дружеский класс, который учавствует в конструировании класса
	friend class ClassParserImpl;

public:

	// конструктор с заданием первоначальных параметров, т.е. объект
	// класса создается при его объявлении, а не определении
	ClassType( const NRC::CharString &name, SymbolTable *entry, BT bt, AS as );

	// освобождаем память занятую списком
	~ClassType() {
		delete castOperatorList;
	}

	// если класс абстрактный (имеет хотя бы одну абстрактную функцию)
	bool IsAbstract() const {
		return abstractMethodCount != 0;
	}

	// если класс полиморфный
	bool IsPolymorphic() const {
		return polymorphic;
	}

	// если класс производный, непустой список базовых классов
	bool IsDerived() const {
		return !baseClassList.IsEmpty();
	}

	// если класс не полностью объявлен
	bool IsUncomplete() const {
		return uncomplete;
	}

	// если класс локальный 
	bool IsLocal() const {
		const SymbolTable *st = &GetSymbolTableEntry();
		while(st->IsClassSymbolTable())
			st = &(static_cast<const ClassType &>(GetSymbolTableEntry()).GetSymbolTableEntry());
		
		return st->IsFunctionSymbolTable() || st->IsLocalSymbolTable();
	}

	// если таблица символов класса	
	virtual bool IsClassSymbolTable() const {
		return true;
	}

	// если создал vm-таблицу (важно для определения размера и генерации кода)
	bool IsMadeVmTable() const {
		return madeVmTable;
	}

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}


	// если класс содержит спецификатор доступа
	virtual bool IsClassMember() const {
		return accessSpecifier != NOT_CLASS_MEMBER;
	}

	// получить список членов
	const ClassMemberList &GetMemberList() const {
		return memberList;
	}

	// получить список базовых классов
	const BaseClassList &GetBaseClassList() const {
		return baseClassList;
	}

	// получить список друзей класса
	const ClassFriendList &GetFriendList() const {
		return friendList;
	}

	// вернуть список конструкторов, может возвращать NULL
	const ConstructorList &GetConstructorList() const {
		return constructorList;
	}

	// получить список операторов приведения
	const CastOperatorList *GetCastOperatorList() const {
		return castOperatorList;
	}

	// получить деструктор, может возвращать NULL
	const Method *GetDestructor() const {
		return destructor;
	}

	// уменьшить количество абстрактных методов в классе
	void DecreaseAbstractMethods() {
		INTERNAL_IF( !abstractMethodCount );
		abstractMethodCount--;
	}

	// задать класс как полиморфный
	void MakeVmTable() {
		polymorphic = true;		
		madeVmTable = true;
	}

	// вернуть количество виртуальных функций
	int GetVirtualFunctionCount() const {
		return virtualMethodCount;
	}

	// увеличить счетчик виртуальных функций класса
	void IncVirtualFunctionCount() {
		virtualMethodCount++;
	}

	// добавить виртуальную функцию в список
	void AddVirtualFunction( const Method &vf ) {
		INTERNAL_IF( !vf.IsVirtual() );
		if( !virtualFunctionList )
			virtualFunctionList = new VirtualFunctionList;
		virtualFunctionList->push_back(&vf);
	}

	// вернуть список виртуальных функций класса
	const VirtualFunctionList &GetVirtualFunctionList() const {
		static VirtualFunctionList vfl;
		return virtualFunctionList ? *virtualFunctionList : vfl;
	}
	
	// добавить базовый класс
	void AddBaseClass( const PBaseClassCharacteristic &bcc );

	// поиск члена в классе, а также в базовых классах, в случае успешного поиска,
	// возвращает список членов, в противном случае, список пуст. 
	// friend-декларации членами не являются и поэтому в поиске не учавствуют
	bool FindSymbol( const NRC::CharString &name, IdentifierList &out ) const ;


	// поиск только внутри класса, без учета базовых классов
	bool FindInScope( const NRC::CharString &name, IdentifierList &out ) const ;

	// вставка члена в таблицу
	bool InsertSymbol( Identifier *id ) ;


	// очищает всю таблицу
	void ClearTable() ;
};


// объединение
class UnionClassType : public ClassType
{
	// возможные спецификаторы хранения для объединения
	::Object::SS	storageSpecifier;

	// если объединение анонимное, т.е. без имени и нет
	// объявления объектов после определения объединения
	bool anonymous;

public:
	// конструктор с заданием первоначальных параметров, т.е. объект
	// класса создается при его объявлении, а не определении
	UnionClassType( const NRC::CharString &name, SymbolTable *entry, 
		AS as, bool a, ::Object::SS ss ) 
	
		: ClassType(name, entry, BT_UNION, as), anonymous(a), storageSpecifier(ss) {		
	 }

	// если объединение безимянное
	bool IsAnonymous() const {
		return anonymous;
	}

	// если объединение статическое
	bool IsStaticUnion() const {
		return storageSpecifier == ::Object::SS_STATIC;
	}

	// получить спецификатор досутпа объединения, причем
	// спецификатор может отсутствовать
	::Object::SS GetStorageSpecifier() const {
		return storageSpecifier;
	}
};


// абстрактный класс представляющий сущность "шаблонный параметр",
// шаблонный параметр имеет три разновидности, шаблонный параметр типа,
// шаблонный параметр не типа, шаблонный параметр шаблона
class TemplateParametr : public Identifier
{
public:
	// разновидность шаблонного параметра
	enum TP	{ TP_TYPE, TP_NONTYPE, TP_TEMPLATE };

private:
	// тип параметра (рановидность)
	TP templateParametrType;

public:
	// в конструкторе задается разновидность шаблонного параметра
	TemplateParametr( const NRC::CharString &name, SymbolTable *entry, TP tpt ) : 
	  Identifier(name, entry), templateParametrType(tpt) {
	}


	// абстрактный метод, возвращает true, если шаблонный параметр
	// имеет значение по умолчанию
	virtual bool IsHaveDefaultValue() const = 0;

	// абстрактый метод, возвращает true, если шаблонный параметр
	// специалзирован. Возвращает true только если шаблонный параметр
	// является членом TemplateClassSpecialization
	virtual bool IsSpecialized() const = 0;

	// получить разновидность шаблонного параметра
	TP GetTemplateParametrType() const {
		return templateParametrType;
	}
};

// в векторе должен хранится интеллектуальный указатель 
typedef SmartPtr<TemplateParametr> PTemplateParametr;


// список шаблонных параметров
class TemplateParametrList
{
	// список шаблонных параметров
	vector<PTemplateParametr> templateParametrList;

public:
	// вернуть true, если список пуст
	bool IsEmpty() const {
		return templateParametrList.empty();
	}

	// получить количество шаблонных параметров перечисления
	int GetTemplateParametrCount() const { 
		return templateParametrList.size();
	}

	// получить шаблонный параметр по индексу,
	// если он существует
	const PTemplateParametr &GetTemplateParametr( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > templateParametrList.size()-1 );
		return templateParametrList[ix];
	}

	// получить шаблонный параметр, если онн не существует
	// возвращается NULL
	const PTemplateParametr operator[]( int ix ) const {
		return (ix < 0 || ix > templateParametrList.size()-1) ? NULL : templateParametrList[ix];
	}

	// добавить шаблонный параметр в список
	// метод не константный, поэтому может вызываться только при
	// построении списка
	void AddTemplateParametr( PTemplateParametr tp ) {
		templateParametrList.push_back(tp);
	}

	// очистить список с освобождением памяти
	void ClearTemplateParametrList() {
		templateParametrList.clear();
	}

	// метод возвращает индекс шаблонного параметра, 
	// если в списке имеется шаблонный параметр с именем name
	int HasTemplateParametr( const NRC::CharString &name ) const {
		int i = 0;
		for( vector<PTemplateParametr>::const_iterator p = templateParametrList.begin(); 
			 p != templateParametrList.end(); p++, i++ )
			if( (*p)->GetName() == name ) 
				return i;
		return -1;
	}
};



// шаблонный параметр типа. Удобная форма представления шаблонного параметра
// типа при инстантинации
class TemplateTypeParametr : public TemplateParametr
{
	// значение параметра по умолчанию, если не задано - равно 0
	// типизированная сущность, т.к. может быть базовый тип и 
	// квалификаторы и модификаторы
	const TypyziedEntity *defaultValue;
		
public:
	// в конструктор загружаем необходимые значения для объекта
	TemplateTypeParametr( const NRC::CharString &name, SymbolTable *entry,
		const TypyziedEntity *dv ) : 
		
		TemplateParametr(name, entry, TP_TYPE), 
		defaultValue(dv) 	
	{			
	}

	// если параметр имеет параметр по умолчанию		
	virtual bool IsHaveDefaultValue() const {
		return defaultValue != NULL;
	}


	// абстрактый метод, возвращает true, если шаблонный параметр
	// специалзирован. 
	virtual bool IsSpecialized() const {
		return false;		// ???
	}
};


// шаблонный параметр шаблона
class TemplateTemplateParametr :  public TemplateParametr
{
public:
	
	// в конструктор загружаем необходимые значения для объекта
	TemplateTemplateParametr( const NRC::CharString &name, SymbolTable *entry ) : 
		TemplateParametr(name, entry, TP_TEMPLATE)	{			
	}

	// если параметр имеет параметр по умолчанию		
	virtual bool IsHaveDefaultValue() const {
		return false;	//defaultValue != NULL;
	}


	// абстрактый метод, возвращает true, если шаблонный параметр
	// специалзирован. 
	virtual bool IsSpecialized() const {
		return false;		// ???
	}
};


// не типовой шаблонный параметр. Эквивалентен типизированной сущности
class TemplateNonTypeParametr : public TemplateParametr
{
	// значение параметра по умолчанию, если не задано - равно 0
	const TypyziedEntity *defaultValue;
		
	// тип параметра
	TypyziedEntity parametrType;

public:
	// в конструктор загружаем необходимые значения для объекта
	TemplateNonTypeParametr( const NRC::CharString &name, SymbolTable *entry,
		BaseType *bt, bool cq, bool vq, const DerivedTypeList &dtl, const TypyziedEntity *dv ) : 

		TemplateParametr(name, entry, TP_NONTYPE), 
		parametrType(bt, cq, vq, dtl),
		defaultValue(dv) 	
	{			
	}


	// если параметр имеет параметр по умолчанию		
	virtual bool IsHaveDefaultValue() const {
		return defaultValue != NULL;
	}

	// абстрактый метод, возвращает true, если шаблонный параметр
	// специалзирован. 
	virtual bool IsSpecialized() const {
		return false;		// ???
	}
};


// шаблонный класс
class TemplateClassType : public Identifier, public ClassMember, public SymbolTable
{
	// спецификатор доступа
	AS accessSpecifier;

public:

	// в конструктор загружаем необходимые значения для объекта
	TemplateClassType( const NRC::CharString &name, SymbolTable *entry,	AS as ) 
		: Identifier(name, entry), accessSpecifier(as) {
	}
	
};


#endif	// end _CLASS_H_INCLUDE
