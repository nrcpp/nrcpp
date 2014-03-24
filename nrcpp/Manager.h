// интерфейсы КЛАССОВ-МЕНЕДЖЕРОВ - Manager.h


// Классы менеджеры. Представляют собой промежуточное звено между 
// входными данными и проверкой этих входных данных на корректность.
// Классы менеджеры расширяют базу знаний объектов-контейнеров.
// Классы-менеджеры можно также назвать инспекторами, так как
// они как правило предоставляют дополнительную информацию по
// запросу, а анализом этой информации занимаются другие классы
// или функции.
// Напимер менеджер имен предоставляет следующие запросы: получить
// список ролей запршенного имени, получить список ролей составного
// имени (представленного в виде пакета), дополнительная информация,
// такая как количество объявленных имен с таким именем, перекрывает
// ли это имя, тип с таким же именем, и др.


// объявлен в Parser.h
class NodePackage;

// объявлен в Parser.h
class Package;

// объявлен в Class.h
class TemplateClassType;


// роли имен
enum Role 
{ 
	R_UNCKNOWN,					// неизвестная роль, возвращается, если объект не найден
	R_OBJECT,					// объект
	R_DATAMEMBER,				// данное-член
	R_PARAMETR,					// параметр функции
	R_ENUM_CONSTANT,			// константа перечисления
	R_CLASS_ENUM_CONSTANT,		// константа перечисления объявленная в классе

	R_FUNCTION,					// функция
	R_METHOD,					// метод
	R_OVERLOAD_OPERATOR,		// глобальный перегруженный оператор
	R_CLASS_OVERLOAD_OPERATOR,	// перегруженный оператор объявленный внутри класса
	R_CONSTRUCTOR,				// конструктор класса

	R_CLASS_TYPE,				// class, struct
	R_ENUM_TYPE,				// enum
	R_UNION_CLASS_TYPE,			// union

	R_TEMPLATE_CLASS,					// шаблонный класс
	R_TEMPLATE_CLASS_SPECIALIZATION,	// специализация шаблонного класса
	R_TEMPLATE_FUNCTION,				// шаблонная функция
	R_TEMPLATE_FUNCTION_SPECIALIZATION,	// специализация шаблонной функции

	R_USING_IDENTIFIER,					// идентификатор из другой ОВ, объявленный как using
	R_TEMPLATE_TYPE_PARAMETR,			// шаблонный параметр типа
	R_TEMPLATE_TEMPLATE_PARAMETR,		// шаблонный параметр шаблона
	R_TEMPLATE_NONTYPE_PARAMETR,		// шаблонный не типовый параметр

	R_NAMESPACE,						// именованная область видимости
	R_NAMESPACE_ALIAS,					// синоним именованной области видимости
};


// пара - идентификатор, роль
typedef pair<Identifier *, Role> RolePair;

// список из пар: идентификатор, роль
typedef list<RolePair> RoleList;


// утилиты менеджеров
namespace ManagerUtils
{
	// получить строковое представление спецификатора хранения объекта
	CharString GetObjectStorageSpecifierName( ::Object::SS ss );


	// получить строковое представление спецификатора функции
	CharString GetFunctionStorageSpecifierName( Function::SS ss );

	// получить спецификатор доступа в виде имени
	PCSTR GetAccessSpecifierName( ClassMember::AS as );

	// получить название области видимости в виде имени, если
	// она является идентификатором, вернуть его
	CharString GetSymbolTableName( const SymbolTable &st );
}


// класс список синонимов. Добавляет к классу стандартного списка метод
// поиска using-декларации по указателю на идентификатор
class SynonymList : public RoleList
{
public:
	// поиск using-идентификатора по указателю на оригинальный идентификатор
	const UsingIdentifier *find_using_identifier( const Identifier *id ) const;
};


// менеджер объявленных имен. При считывании имени нам необходимо знать его 
// семантическое значение в тексте программы. Семантическое значение 
// имени кратко обозначим как роль имени. Этот класс по запросу возвратит 
// список ролей данного имени в заданной области видимости, либо в текущей. 
// А также сопутствующую информацию такую как: перекрытие имен, 
// список не перекрытых имен, явяляется ли найденное имя - именем типа, 
// является ли найденное имя - именем класса, и др.
class NameManager
{
	// запрошенное имя
	CharString queryName;

	// список ролей запршенного имени
	RoleList roleList;
	
	// список оригинальных идентификаторов-синонимов. Таковыми являются
	// синонимы именованной области видимости и using-идентификатор.
	// Синонимы помещаются в этот список, т.к. идентификаторы, которые
	// они используют сразу идут в список ролей. Сами синонимы необходимы
	// при проверке доступа
	SynonymList synonymList;

	// область видимости в которой следует производить полный поиск данного имени 
	// причем поиск производится и в дружественных областях видимости
	// (для классов - базовый, для других - using-области). Если область
	// видимости не задана - NULL, производить поиск до первого соотв.
	// по всем
	const SymbolTable *bindTable;

	// производить поиск также и по дружеским областям,
	// для классов - базовые классы, для других - using области
	bool watchFriend;

public:

	// конструктор принимает запрос, и заполняет список ролей
	// согласно запросу. 
	// qn - имя (запрос), bt - если задано, область видимости в которой следует
	// искать имя, watchFriend - производить поиск также и по дружеским областям,
	// для классов - базовые классы, для других - using области
	NameManager( const CharString &qn, const SymbolTable *bt = NULL, bool watchFriend = true ); 

	// если у имени одна роль, т.е. оно уникально - вернуть true
	bool IsUnique() const {
		return GetRoleCount() == 1;
	}

	// если имя является типом
	bool IsTypeName() const ;

	// если имя является типом typedef
	bool IsTypedef() const;

	// получить все роли для заданного имени. Может так получится, что под одним именем
	// объявлено несколько ролей (напр. перегруженные функции, шаблонные ф-ции,
	// класс). Возвращает false, если ролей нет
	const RoleList &GetRoleList( ) const {
		return roleList;
	}		

	// получить количество ролей данного имени
	int GetRoleCount() const {
		return roleList.size();
	}

	// получить список синонимов
	const SynonymList &GetSynonymList() const {
		return synonymList; 
	}

	// получить роль идентификатора
	static Role GetIdentifierRole( const Identifier *id ) ;
};


// менеджер составных имен. Подобен классу NameManager, с той 
// лишь разницей, что менеджмент производится для составного имени, 
// которое специализируется одним или несколькими областями видимости 
// и представлена в виде пакета.
class QualifiedNameManager
{
	// список из областей видимости, которые составляют квалификацию имени
	// области видимости расположены в том порядке, в котором они расположены
	// в пакете
	SymbolTableList qualifierList;

	// список ролей самого вложенного имени, собственно, которое и специфицируется
	RoleList roleList;

	// список синонимов для возможности проверки доступа к using-идентификатору
	SynonymList synonymList;

	// пакет, который является запросом к менеджеру
	const NodePackage *queryPackage;

	// область видимости в которой следует производить поиск данного имени 
	// может быть NULL, тогда поиск производится с текущей
	const SymbolTable *bindTable;

	// проверить, является ли имя областью видимости. Если является - возвращает 
	// указатель на нее, в противном случае - NULL
	const SymbolTable *IsSymbolTable( const NameManager &nm ) const;

	// возвращает имя пакета. Пакет может иметь код NAME, PC_OVERLOAD_OPERATOR,
	// PC_CAST_OPERATOR, PC_DESTRUCTOR. В последних двух случаях вызывается 
	// функция-строитель для получения корректного имени идентификатора
	CharString GetPackageName( const Package &pkg );

public:
	// создать менеджер составного имени,
	// можно задать область видимости, в которой следует искать имя. 	 
	// Эта функция используется для поиска среди составных имен, хотя может
	// использоваться и для одиночных, в случае если np содержит только
	// один под пакет. np - должен иметь заголовок PC_QUALIFIED_NAME
	QualifiedNameManager( const NodePackage *np, const SymbolTable *bt = NULL );

	// получить список квалификаторов
	const SymbolTableList &GetQualifierList() const {
		return qualifierList;
	}

	// получить список ролей
	const RoleList &GetRoleList() const {
		return roleList;
	}

	// получить список синонимов
	const SynonymList &GetSynonymList() const {
		return synonymList; 
	}

	// получить количество ролей
	int GetRoleCount() const {
		return roleList.size();
	}

		// если у имени одна роль, т.е. оно уникально - вернуть true
	bool IsUnique() const {
		return GetRoleCount() == 1;
	}

	// если имя является типом
	bool IsTypeName() const ;

	// если имя является типом typedef
	bool IsTypedef() const;
};


// менеджер встроенных типов. Работает с типами: bool, char, int, float, double, void,
// short int, long int, long double. Основое предназначение - создавать объекты типа 
// BaseType , возвращать размер встроенного типа.
class ImplicitTypeManager
{
	// код базового типа
	BaseType::BT baseTypeCode;

	// модификатор знака 
	BaseType::MSIGN modSign;

	// модификатор размера
	BaseType::MSIZE modSize;

public:

	// менеджер встроенных типов. В параметрах код типа, код модификаторов,
	// если модификаторы не заданы они равны -1
	ImplicitTypeManager( int lcode, int msgn = -1, int msz = -1 );

	// конструктор для уже созданного типа
	ImplicitTypeManager( const BaseType &bt );

	// конструктор для создаваемого типа
	ImplicitTypeManager( BaseType::BT btc, BaseType::MSIZE mz = BaseType::MZ_NONE,
		BaseType::MSIGN mn = BaseType::MN_NONE ) : baseTypeCode(btc), modSize(mz), modSign(mn)
	{
	}

	// по коду, возвращает указатель на уже созданный базовый тип
	const BaseType &GetImplicitType() ; 

	// получить код
	BaseType::BT GetImplicitTypeCode() const {
		return baseTypeCode;
	}
	

	// получить размер типа
	int GetImplicitTypeSize() const ;

	// получить строковое представление имени
	CharString GetImplicitTypeName() const ;
};


// менеджер спецификаторов типа, активно используется при создании
// объекта-контейнера из пакета
class TypeSpecifierManager
{
	// возможные группы спецификаторов (type specifier group)
	enum {
		TSG_UNCKNOWN,		// неизвестная, значение по умолчанию
		TSG_BASETYPE,		// базовый тип (bool, char, wchar_t, int, float, double, void)
		TSG_CLASSSPEC,		// спецификатор класса (enum, class, union, struct)
		TSG_CVQUALIFIER,	// cv-квалификатор (const, volatile)
		TSG_SIGNMODIFIER,	// модификатор знака (unsigned, signed)
		TSG_SIZEMODIFIER,	// модификатор размера (short, long)
		TSG_STORAGESPEC,	// спецификатор хранения (auto, extern, static, 
							// register, mutable, typedef)
		TSG_FRIEND,			// спецификатор дружбы
		TSG_FUNCTIONSPEC,	// спецификатор функции (inline, virtual, explicit)		
	} group;

	// код спецификатора
	int code;

public:

	// конструктор определяет к какой группе относится код
	TypeSpecifierManager( int c );

	// если код является незвестным
	bool IsUncknown() const {
		return group == TSG_UNCKNOWN;
	}

	// если код является базовым типом
	bool IsBaseType() const {
		return group == TSG_BASETYPE;
	}

	// если код является базовым типом
	bool IsClassSpec() const {
		return group == TSG_CLASSSPEC;
	}
	
	// если код является cv-квалификатором
	bool IsCVQualifier() const {
		return group == TSG_CVQUALIFIER;
	}

	// если код является  модификатором знака
	bool IsSignModifier() const {
		return group == TSG_SIGNMODIFIER;
	}

	// если код является  модификатором размера
	bool IsSizeModifier() const {
		return group == TSG_SIZEMODIFIER;
	}
	
	// если код является  модификатором размера
	bool IsStorageSpecifier() const {
		return group == TSG_STORAGESPEC;
	}

	// если код является другом
	bool IsFriend() const {
		return group == TSG_FRIEND;
	}

	// если код является спецификатором функции
	bool IsFunctionSpecifier() const {
		return group == TSG_FUNCTIONSPEC;
	}

	// --- все методы CodeToXXX() преобразуют целое число в код для хранения
	//	   в контейнере. Если группа не соотв. коду, вызывается внутренняя ошибка	
	// вернуть базовый тип, в случае если спецификатор является базовым типом	
	BaseType::BT CodeToBaseType() const ;

	// вернуть спецификатор класса
	BaseType::BT CodeToClassSpec() const ;

	// вернуть модификатор знака
	BaseType::MSIGN CodeToSignModifier() const ;

	// вернуть модификатор размера
	BaseType::MSIZE CodeToSizeModifier() const ;

	// вернуть спецификатор хранения объекта
	::Object::SS CodeToStorageSpecifierObj() const ;
	
	// вернуть спецификатор хранения функции
	Function::SS CodeToStorageSpecifierFn() const ;

	// получить имя кода
	CharString GetKeywordName() const {
		return group == TSG_UNCKNOWN ? "<uncknown>" : ::GetKeywordName(code);
	}

	// получить имя группы на русском языке
	CharString GetGroupNameRU() const ;

	// получить имя группы на английском
	CharString GetGroupNameENG() const;
};


// xарактеризует класс 'Base' по отношению к классу 'Derived' по
// таким параметрам: виртуальность, однозначность, доступность,
// а также является ли 'Base' базовым классом для 'Derived'
class DerivationManager
{
	// если класс 'Base' является доступным для 'Derived', установлен в true
	bool accessible;

	// если класс 'Base' является виртуальным для 'Derived', установлен в true
	bool virtualDerivation;

	// количество базовых классов 'Base' для 'Derived'
	int baseCount;

	// метод характеризует класс 'base' по отношению к 'derived'
	void Characterize( const ClassType &base, const ClassType &curCls, bool ac );

public:

	// в конструкторе задается два класса и сразу же выполняются
	// характеризация класса 'Base' по отношению к 'Derived'
	DerivationManager( const ClassType &base, const ClassType &derived )
		: accessible(false), virtualDerivation(false), baseCount(0) {

		Characterize( base, derived, true );
	}

	// базовый, в том случае если есть в иерархии 'base'
	bool IsBase() const {
		return baseCount > 0;
	}

	// однозначный, если класс один или класс виртуален
	bool IsUnambigous() const {
		return baseCount == 1 || virtualDerivation;
	}

	// виртуальное наследование
	bool IsVirtual() const {
		return virtualDerivation;
	}

	// доступность
	bool IsAccessible() const {
		return accessible;
	}
};


// менеджер специальных функций-членов, для заданного класса
// находит к-ор по умолчанию, к-ор копирования, деструктор, оператор копирования
class SMFManager
{
public:
	// пара - у-ль на метод и флаг однозначности, если метод
	// неоднозначен, флаг установлен в true
	typedef pair<const Method *, bool> SmfPair;

private:
	// четыре специфальных функций членов
	SmfPair ctorDef, ctorCopy, dtor, copyOperator;

	// класс для которого выполняется поиск
	const ClassType &pClass;

	// методы-стратегии, для выявляения нужного спец. функции члена
	// если к-ор по умолчанию вернет true
	bool IsDefaultConstructor( const Method &meth, const ClassType &cls ) const {
		const FunctionParametrList &pl = meth.GetFunctionPrototype().GetParametrList();
		return pl.GetFunctionParametrCount() == 0 ||
			   pl.GetFunctionParametr(0)->IsHaveDefaultValue();
	}

	// если к-ор копирования, вернет true
	bool IsCopyConstructor( const Method &meth, const ClassType &cls ) const {
		const FunctionParametrList &pl = meth.GetFunctionPrototype().GetParametrList();
		if( pl.IsEmpty() )
			return false;

		const Parametr &prm = *pl.GetFunctionParametr(0);
		if( &prm.GetBaseType() == &cls && prm.GetDerivedTypeList().IsReference() &&
			prm.GetDerivedTypeList().GetDerivedTypeCount() == 1 )		
			return pl.GetFunctionParametrCount() == 1 ||
				pl.GetFunctionParametr(1)->IsHaveDefaultValue();
		return false;		
	}	

	// если оператор копирования
	bool IsCopyOperator( const Method &meth, const ClassType &cls ) const {
		const FunctionParametrList &pl = meth.GetFunctionPrototype().GetParametrList();
		if( pl.GetFunctionParametrCount() != 1 )
			return false;

		const Parametr &prm = *pl.GetFunctionParametr(0);	
		return ( &prm.GetBaseType() == &cls && prm.GetDerivedTypeList().IsReference() &&
			prm.GetDerivedTypeList().GetDerivedTypeCount() == 1 );
	}

	// функция поиска всех четырех функций членов
	void FoundSMF();

public:
	// задаем класс
	SMFManager( const ClassType &pcls )
		: pClass(pcls), ctorDef(NULL, false), ctorCopy(NULL, false),
		  dtor(NULL, false), copyOperator(NULL, false) {

		FoundSMF();
	}

	// вернуть конструктор по умолчанию
	const SmfPair &GetDefaultConstructor() const {
		return ctorDef;
	}

	// вернуть конструктор копирования
	const SmfPair &GetCopyConstructor() const {
		return ctorCopy;
	}

	// вернуть деструктор
	const SmfPair &GetDestructor() const {
		return dtor;
	}

	// вернуть оператор копирования
	const SmfPair &GetCopyOperator() const {
		return copyOperator;
	}
};

