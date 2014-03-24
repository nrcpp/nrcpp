// интерфейс для строителей членов класса - MemberMaker.h
// примечание: необходимо подключение заголовочного файла Maker.h перед MemberMaker.h


// объявлен в Class.h
class ClassType;

//  объявлен в Maker.h
struct TempObjectContainer;

// объявлен в Body.h
class Operand ;


// тип инициализации члена
enum MemberInitializationType 
{
	MIT_NONE, MIT_PURE_VIRTUAL, MIT_DATA_MEMBER, MIT_BITFIELD
};


// интерфейс для всех классов строителей членов
class MemberDeclarationMaker : public DeclarationMaker
{
protected:
	// члены необходимые для всех классов строителей "членов-класса":
	// ссылка на класс, член которого строится
	ClassType &clsType;

	// текущий спецификатор доступа в этом классе
	ClassMember::AS curAccessSpec;

	// временная структура, которая используется при построении объекта
	PTempObjectContainer toc;	

public:
	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры
	MemberDeclarationMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) 
		: clsType(ct), curAccessSpec(cas), toc(t) {
	}

	// абстрактный метод для инициализации члена
	virtual void Initialize( MemberInitializationType mit, const Operand &exp ) = 0;

private:
	// закрываем доступ к абстрактному методу для инициализации члена из
	// верхней части
	virtual void Initialize( const ExpressionList & ) {
		INTERNAL( "'MemberDeclarationMaker::Initialize'"
			" - метод не может вызываться из данного класса");
	}
};


// строитель данного-члена
class DataMemberMaker : public MemberDeclarationMaker
{
	// результат работы строителя
	DataMember *targetDM;

	// метод проверяет корректность создания битового поля
	void CheckBitField( const Operand &exp );
		
	// метод проверяет корректность инициализации данного-члена значением
	void CheckDataInit( const Operand &exp );

public:

	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры		
	DataMemberMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) : MemberDeclarationMaker(ct, cas, t), targetDM(0) {
	}

	// функция создания данного-члена, переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации члена. Инициализировать можно только,
	// статические целые данные-члены
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetDM;
	}
};


// строитель методов, которые не являются специальными-функциями членами
class MethodMaker : public MemberDeclarationMaker
{
	// результат работы строителя
	Method *targetMethod;

public:

	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры		
	MethodMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) : MemberDeclarationMaker(ct, cas, t), targetMethod(0) {
	}

	// функция создания метода переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации члена. Инициализация может быть
	// только MIT_METHOD, exp должен быть равен NULL
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetMethod;
	}
};


// строитель перегруженных операторов
class OperatorMemberMaker : public MemberDeclarationMaker
{
	// результат работы строителя
	ClassOverloadOperator *targetOO;

	// временная струткура содержащая информаицю об операторе
	TempOverloadOperatorContainer tooc;

public:

	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры		
	OperatorMemberMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t, const TempOverloadOperatorContainer &tc )
		: MemberDeclarationMaker(ct, cas, t), targetOO(0), tooc(tc) {
	}

	// функция создания перегруженного оператора,
	// переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации метода. Инициализация может быть
	// только MIT_METHOD, exp должен быть равен NULL
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetOO;
	}
};


// строитель операторов приведения
class CastOperatorMaker : public MemberDeclarationMaker
{
	// результат
	ClassCastOverloadOperator *targetCCOO;

	// констейнер с информацией об операторе
	TempCastOperatorContainer tcoc;

public:

	// конструктор задающий параметры
	CastOperatorMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t, const TempCastOperatorContainer &tc )
		: MemberDeclarationMaker(ct, cas, t), targetCCOO(0), tcoc(tc) {
	}

	// функция создания оператора приведения,
	// переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации оператора приведения. Инициализация может быть
	// только MIT_METHOD, exp должен быть равен NULL
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetCCOO;
	}
};


// строитель конструкторов
class ConstructorMaker : public MemberDeclarationMaker
{	
	// результат работы строителя
	ConstructorMethod *targetCtor;

public:

	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры		
	ConstructorMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) : MemberDeclarationMaker(ct, cas, t), targetCtor(0) {
	}

	// функция создания конструктора, переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации конструктора. Инициализация 
	// конструктора невозможна
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetCtor;
	}
};


// строитель деструкторов
class DestructorMaker : public MemberDeclarationMaker
{
	// результат работы строителя
	Method *targetDtor;

public:

	// конструктор для задания класса, текущего спецификатора доступа
	// и временной структуры		
	DestructorMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) : MemberDeclarationMaker(ct, cas, t), targetDtor(0) {
	}

	// функция создания деструкторов, переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации деструткора. Инициализация может быть
	// только MIT_METHOD, exp должен быть равен NULL
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetDtor ;
	}
};


// строитель дружеских функций, которые не вставляются в область
// видимости класса, а вставляются в ближайшую глобальную ОВ,
// а также в список друзей класса
class FriendFunctionMaker : public MemberDeclarationMaker
{
	// результат работы строителя
	Function *targetFn;

	// если анализируем оператор, установлен в true
	
	bool isOperator;

	// информация об операторе, если объявляется дружеский оператор
	TempOverloadOperatorContainer tooc;

public:

	// конструктор для обычной функции
	FriendFunctionMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t ) 
		: MemberDeclarationMaker(ct, cas, t), isOperator(false), targetFn(0) {

	}

	// конструктор для перегруженного оператора
	FriendFunctionMaker( ClassType &ct, ClassMember::AS cas,
		const PTempObjectContainer &t, const TempOverloadOperatorContainer &tc ) 
		: MemberDeclarationMaker(ct, cas, t), isOperator(true), tooc(tc), targetFn(0) {

	}

	// функция создания дружеских функций, 
	// переопределяет метод класса DeclarationMaker
	bool Make();

	// абстрактный метод для инициализации функции. Инициализация невозможна	
	void Initialize( MemberInitializationType mit, const Operand &exp ) ;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	const Identifier *GetIdentifier() const {
		return targetFn;
	}
};


// строитель определения членов класса или ИОВ в глобальной области видимости
class MemberDefinationMaker : public DeclarationMaker
{
	// пакет с декларацией. В отличие от других строителей,
	// строителю определений необходимо самому собирать декларацию из пакета
	const NodePackage *tsl, *decl;

	// результирующий идентификатор
	Identifier *targetID;

	// временная структура в которую записывается декларация, изначально равна 0
	PTempObjectContainer toc;

	// роль идентификатора который объявляется, должен быть:
	// R_OBJECT, R_DATAMEMBER, R_FUNCTION,					
	// R_METHOD, R_OVERLOAD_OPERATOR, R_CLASS_OVERLOAD_OPERATOR,	
	// R_CONSTRUCTOR. Изначально R_UNCKNOWN
	Role idRole;

	// менеджер имени члена, передается от координатора
	const QualifiedNameManager &memberQnm;

	// специальная функция уточняющая базовый тип в определение члена,
	// с учетом того, что в конструкторах, деструкторах, операторах приведения
	// базовый тип не задается явно а формируется автоматически
	void SpecifyBaseType( const NodePackage *np, const SymbolTable &st );

	// Определить роль объявляемого идентификатора. Классы, объединения
	// перечисления игнорируются, остальные роли должны совпадать.
	// Должна быть хотя-бы одна объектная роль
	bool InspectIDRole( const RoleList &rl ) ;
	
	// если объект инициализировался конструктором, задать его
	const ConstructorMethod *ictor;

public:

	// конструктор принимает пакет
	MemberDefinationMaker( const NodePackage *t, const NodePackage *d, 
		const QualifiedNameManager &qnm ) 

		: tsl(t), decl(d), memberQnm(qnm), targetID(0), toc(NULL), idRole(R_UNCKNOWN), 
		ictor(NULL) {
		
		INTERNAL_IF( tsl == NULL || decl == NULL );
	}

	// инициализировать объект выражением 
	void Initialize( const ExpressionList & );

	// построить определение
	bool Make();

	// вернуть указатель на определяемый идентификатор, может быть 0
	const Identifier *GetIdentifier() const {
		return targetID;
	}

	// вернуть конструктор
	const ConstructorMethod *GetConstructor() const {
		return ictor;
	}
};

