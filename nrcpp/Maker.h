// интерфейс КЛАССОВ-СТРОИТЕЛЕЙ - Maker.h


// Классы-строители существуют для того, чтобы создавать объекты-контейнеры
// которые представляют собой сжатую форму конструкций языка. Классы-строители
// явялются связующим звеном между синтаксическим анализатором и семантической
// проверкой. Классы-строители получают на вход набор данных, как правило
// это набор лексем, упакованных с помощью интерфейса Package. На выходе
// получается готовый объект-контейнер. Объект-контейнер создается только
// в случае его корректного объявления.
// Синтаксические конструкции разбираются в одном месте, но представляют
// собой семантически разные объекты, как например объект и функция.
// Классы-конструкции отличаются от синтаксических классов тем, что
// каждый класс создает и проверяет только похожие идентификаторы.
// Таким образом, сами синтаксические классы должны выбирать какой класс-строитель
// необходимо использовать для полученного набора пакетов. Структура пакетов
// для каждого случая должна быть четко оговорена и задокументирована


// объявлен в Object.h
class Identifier;

// объявлен в Object.h
class EnumConstant;

// объявлен в Parser.h
class NodePackage;

// объявлен в Parser.h
class LexemPackage;

// объявлен в Manager.h
class QualifiedNameManager;

// объявлена ниже
struct TempObjectContainer;

// объявлена ниже
struct TempOverloadOperatorContainer;

// объявлена ниже
struct TempCastOperatorContainer;

// объявлен в Body.h
class Operand;
typedef SmartPtr<Operand> POperand;
typedef vector<POperand> ExpressionList;


// функции используемые при строительстве объектов
namespace MakerUtils
{
	// функция сбора информации из пакета во временную структуру,
	// используется при сборе информации при декларации объектов и функций,
	// последний параметр, запрещает или разрешает автоматическое определение
	// класса
	bool AnalyzeTypeSpecifierPkg( const NodePackage *typeSpecList, 
			TempObjectContainer *tempObjectContainer, bool defineClassImplicity = true );

	// функция сбора информации из пакета с декларатором
	// во временную структуру tempObjectContainer
	void AnalyzeDeclaratorPkg( const NodePackage *declarator, 
			TempObjectContainer *tempObjectContainer );

	// анализировать пакет с перегруженным оператором и поместить
	//  его во временную структуру
	void AnalyzeOverloadOperatorPkg( const NodePackage &op, 
			TempOverloadOperatorContainer &tooc );


	// анализировать пакет с оператором приведения и поместить
	// его во временную структуру
	void AnalyzeCastOperatorPkg( const NodePackage &op, TempCastOperatorContainer &tcoc );


	// уточнить базовый тип временного объекта: 1. определить есть ли базовый тип,
	// и если нет то добавить тип по умолчанию, 2. если базовый тип задан как
	// синоним типа typedef, преобразовать 
	void SpecifyBaseType( TempObjectContainer *tempObjectContainer );

	
	// проверить и создать характеристику базового класса, если
	// базовый класс не найден, либо он не доступен, возвращается 0
	PBaseClassCharacteristic MakeBaseClass( const NodePackage *bc, bool defaultIsPrivate );

	
	// создает новую область видимости, если она не создана, вставляет
	// ее в стек областей видимости, и делаем необходимые проверки.
	// В параметре передается пакет с именем, который может быть NULL,
	// если создается безимянная ОВ
	bool MakeNamepsaceDeclRegion( const NodePackage *nn );

	
	// проверить, создать и вставить в таблицу синоном области видимости
	// в параметрах принимаются имя синонима и имя области видимости для
	// которов синоним создается
	void MakeNamespaceAlias( const NodePackage *al, const NodePackage *ns );

	
	// создать и проверить декларацию дружеской области видимости
	void MakeUsingNamespace( const NodePackage *ns );


	// создать дружественный класс
	void MakeFriendClass( const NodePackage *tsl );

	// создать using-декларацию член
	void MakeUsingMember( const NodePackage *npkg, ClassMember::AS as );

	// создать using-декларацию не член
	void MakeUsingNotMember( const NodePackage *npkg );

	// создать константу перечисления и вставить ее в текущую область видимости
	EnumConstant *MakeEnumConstant(
		const CharString &name, ClassMember::AS curAccessSpec,
		int lastVal, const Position &errPos, EnumType *enumType);
}


// временная структура в которую записывается собранная из пакета
// информация. В числовых полях, если поле не задано его значение < 0
struct TempObjectContainer
{
public:
	// имеется 3 разновидности считываемого типа:
	// встроенный тип, составной тип (класс или перечисление), 
	// синоним тип typedef. У всех 3 разное представление, этот
	// класс обобщает эти представления
	class VariantType
	{
	public:
		virtual ~VariantType() {
		}

		virtual bool IsImplicit() const {
			return false;
		}

		virtual bool IsCompound() const {
			return false;
		}

		virtual bool IsSynonym() const {
			return false;
		}
	};

	// встроенный тип
	class ImplicitType : public VariantType 
	{
	public:
		// код базового типа
		BaseType::BT baseTypeCode;

		// модификаторы базового типа
		BaseType::MSIGN signMod;

		// модификатор размера
		BaseType::MSIZE sizeMod;

		// встроенный тип
		ImplicitType( BaseType::BT bt = BaseType::BT_NONE ) 
			: baseTypeCode(bt), signMod(BaseType::MN_NONE),
			sizeMod(BaseType::MZ_NONE)  {
		}

		bool IsImplicit() const {
			return true;
		}
	};

	// класс или перечисление - составной тип
	class CompoundType : public VariantType
	{
		// указатель на базовый тип (класс или перечисление)
		const BaseType *baseType;

	public:
		// 
		CompoundType( const BaseType &bt ) : baseType(&bt) {
		}

		// получить базовый тип
		const BaseType *GetBaseType() const {
			return baseType;
		}

		bool IsCompound() const {
			return true;
		}
	};

	// typedef
	class SynonymType : public VariantType
	{
		// список ролей имени, должно быть только одно typedef
		const ::Object &typedefName;

	public:
		// на вход поступает пакет с именем, на выходе - список ролей
		SynonymType( const ::Object &tname ) : typedefName(tname) {			
		}

		// получить имя typedef
		const ::Object &GetTypedefName() const {
			return typedefName;
		}

		bool IsSynonym() const {
			return true;
		}
	};

public:
	// имя
	CharString name;

	// список ролей имени, а также его области видимости,
	// может так оказаться, что идет определение статического
	// данного члена
	QualifiedNameManager *nameManager;
	
	// на этапе анализа декларации используется baseType,
	// т.к. позволяет представить 3 разновидности базовых типов.
	// После анализа, базовый тип уточнаяется на основе baseType и
	// преобразуется в finalType
	union
	{
		// предусматривает любой вариант возможного базового типа:
		// встроенный, составной, typedef
		VariantType *baseType;

		// базовый тип после заполнения всей структуры
		BaseType *finalType;
	};


	// квалификаторы, если есть установлены в true
	bool constQual, volatileQual;

	// список производных типов
	DerivedTypeList dtl;

	// спецификатор хранения, если не задан равен -1
	int ssCode;

	// спецификатор функции, у объектов не должен задаваться
	int fnSpecCode;

	// спецификатор дружбы
	bool friendSpec;

	// спецификатор связывания языка С. Если задан - true
	bool clinkSpec;

	// позиция имени, для вывода ошибок
	Position errPos;

	// текущий спецификатор доступа, если декларации строятся внутри класса,
	// т.е. строятся члены класса
	ClassMember::AS	curAccessSpec;

	// конструктор принимает пакет, для нахождения в нем имени и 
	// инициализации nameManager
	TempObjectContainer( const NodePackage *qualName, 
			ClassMember::AS cas = ClassMember::NOT_CLASS_MEMBER );

	// конструктор для объектов, которые не могут содержать составные
	// имена, либо могут не иметь имени вовсе
	TempObjectContainer( const Position &ep, const CharString &n, 
		ClassMember::AS cas = ClassMember::NOT_CLASS_MEMBER );

	// очищает список производных типов, удаляем занятую память
	~TempObjectContainer();
};


// временная структура для перегруженных операторов
struct TempOverloadOperatorContainer
{
	// код оператора
	int opCode;

	// строковое представление оператора
	CharString opString;

	// полное имя перегруженного оператора
	CharString opFullName;

	// инициализация кода
	TempOverloadOperatorContainer() : opCode(-1) {
	}
};	


// временная структура для операторов приведения
struct TempCastOperatorContainer
{
	// код оператора
	int opCode;

	// строковое представление оператора
	CharString opString;

	// полное имя перегруженного оператора
	CharString opFullName;

	// типизированная сущность, которая характеризует приводимый тип
	PTypyziedEntity castType;

	// инициализация кода и типа
	TempCastOperatorContainer() : opCode(-1), castType(NULL) {
	}
};


// интерфейс для классов строителей деклараций. Содержит единственный абстрактный метод
// для создания контейнера. Введен для использования классами-координаторами
class DeclarationMaker
{
public:
	virtual ~DeclarationMaker( ) { }

	// если идентификатор не создается, вернуть false
	virtual bool Make() = 0;

	// инициализировать объект, у функций всего лишь выводит ошибку
	virtual void Initialize( const ExpressionList & ) = 0;

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан
	virtual const Identifier *GetIdentifier() const = 0;
};


// интеллектуальный указатель
typedef SmartPtr<TempObjectContainer> PTempObjectContainer;


// строитель глобальных объектов. Под глобальными объектами понимаются
// объекты объявленные в глобальной области видимости и именованной области видимости
class GlobalObjectMaker : public DeclarationMaker
{
	// временная структура, которая используется при построении объекта
	PTempObjectContainer tempObjectContainer;

	// идентификатор, который необходимо создать
	::Object *targetObject;

	// устанавливается в true, если объект переопределяется
	bool redeclared;

	// установлен в true, если локальная декларация
	bool localDeclaration;

	// если объект инициализировался конструктором, задать его
	const ConstructorMethod *ictor;

public:
	// в конструкторе задаем временную структуру,которая будет
	// использоваться при построении объекта
	GlobalObjectMaker( const PTempObjectContainer &toc, bool ld = false ) ;

	// проверить корректность входного пакета, собрать информацию во временную структуру,
	// создать объект-контейнер, произвести строгую проверку объекта-контейнра,
	// вставить контейнер в таблицу 
	bool Make();

	// инициализировать объект выражением 
	void Initialize( const ExpressionList & );

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан. Сперва требуется вызов Make
	const Identifier *GetIdentifier() const {
		return targetObject;
	}

	// вернуть конструктор
	const ConstructorMethod *GetConstructor() const {
		return ictor;
	}
};


// строитель глобальных функций, работает идентично GlobalObjectMaker,
// за исключением того, что создает объект типа Function
class GlobalFunctionMaker : public DeclarationMaker
{
	// временная структура, которая используется при построении функции
	PTempObjectContainer tempObjectContainer;

	// функция, которую необходимо создать
	Function *targetFn;

	// устанавливается в true, если при построении была семантическая ошибка
	bool errorFlag;

public:
	// в конструкторе задаем временную структуру,которая будет
	// использоваться при построении функции
	GlobalFunctionMaker( const PTempObjectContainer &toc ) ;

	// создает функцию из временной структуры
	bool Make();

	// вывести ошибку, что инициализация функций запрещена
	void Initialize( const ExpressionList &il ) {		
		if( &il != NULL )
			theApp.Error( tempObjectContainer->errPos,
				"инициализация функций недопустима" );
	}

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан. Сперва требуется вызов Make
	const Identifier *GetIdentifier() const {
		return targetFn;
	}
};


// строитель перегруженного оператора
class GlobalOperatorMaker :  public DeclarationMaker
{
	// временная структура, которая используется при построении оператора
	PTempObjectContainer tempObjectContainer;

	// структура с информацией об операторе
	TempOverloadOperatorContainer tooc;

	// оператор, который необходимо создать
	OverloadOperator *targetOP;

public:
	// в конструкторе задаем временную структуру,которая будет
	// использоваться при построении оператора
	GlobalOperatorMaker( const PTempObjectContainer &toc, 
		const TempOverloadOperatorContainer &tc ) ;

	// создает оператор из временной структуры
	bool Make();

	// вывести ошибку, что инициализация функций запрещена
	void Initialize( const ExpressionList &il ) {		
		if( &il != NULL )
			theApp.Error( tempObjectContainer->errPos,
				"инициализация перегруженных операторов недопустима" );
	}

	// получить идентификатор, может возвращать NULL, если
	// идентификатор не создан. Сперва требуется вызов Make
	const Identifier *GetIdentifier() const {
		return targetOP;
	}
};


// создаем перечисление по имени
class EnumTypeMaker
{
	// список спецификаторов типа, последним из которых должен
	// быть KWENUM и имя, либо просто ключ
	NodePackage *typePkg;

	// спецификатор доступа, если класс является членом
	ClassMember::AS as;

	// результат работы строителя
	EnumType *resultEnum;

	// список квалификаторов перечислением, не пустой только в случае
	// если пакет с именем пакета содержит квалифицированное имя
	SymbolTableList stList;

	// установлен в true, если класс определяется. Необходимо для 
	// проверки доступа, если true, проверка не выполняется
	bool defination;

public:
	//  к-тор принимает пакет и проверяет его правильность
	EnumTypeMaker( NodePackage *np, ClassMember::AS a, bool def = false );

	// создать класс, если он еще не создан, а также проверить 
	// возможность его создания
	EnumType *Make();

	// получить список областей видимости, которыми квалифицирован класс
	const class SymbolTableList &GetQualifierList() const {
		return stList;
	}
};


// Создает класс по ключу и имени. Если класс уже существует 
// возвращает его. Если класс безимянный, создает для него имя и присоединяет
// к списку пакетов. Если класс не существует, либо существует тип
// с таким именем - возвращает NULL
class ClassTypeMaker
{
	// список спецификаторов типа, последним из которых должен
	// быть ключ класса и имя, либо просто ключ
	NodePackage *typePkg;

	// спецификатор доступа, если класс является членом
	ClassMember::AS as;

	// результат работы строителя
	ClassType *resultCls;

	// список квалификаторов класса, не пустой только в случае
	// если пакет с именем класса содержит квалифицированное имя
	SymbolTableList stList;

	// область видимости в которую вставляется класс
	SymbolTable &destST;

	// установлен в true, если класс определяется. Необходимо для 
	// проверки доступа, если true, проверка не выполняется
	bool defination;

public:
	//  к-тор принимает пакет и проверяет его правильность
	ClassTypeMaker( NodePackage *np, ClassMember::AS a, 
		SymbolTable &d = GetCurrentSymbolTable(), bool def = false );

	// создать класс, если он еще не создан, а также проверить 
	// возможность его создания
	ClassType *Make();

	// получить список областей видимости, которыми квалифицирован класс
	const class SymbolTableList &GetQualifierList() const {
		return stList;
	}


	// методы создания
private:

	// создать класс по ключу и по имени
	void MakeUncompleteClass();

	// создать безимянный класс
	void MakeUnnamedClass();
};


// строитель прототипа функции. Основная задача состоит в создании и проверке 
// списка параметров, списка исключительных ситуаций
class FunctionPrototypeMaker
{
	// формируемый список параметров функции
	FunctionParametrList parametrList;

	// список исключительных ситуаций
	FunctionThrowTypeList throwTypeList;

	// true - если не требуются имена в прототипе, т.е.
	// когда создается прототип не в декларации, а в выражении
	bool noNames;

	// анализируемый пакет
	const NodePackage &protoPkg;

	// создать параметр и добавить его в список из пакета
	void MakeParametr( const NodePackage &pp, int pnum );

	// создать тип throw-спецификации
	void MakeThrowType( const NodePackage &tt );

	// создать полную throw-спецификацию
	void MakeThrowSpecification( const NodePackage &ts );

	// квалификаторы функции
	bool constQual, volatileQual;

	// '...'
	bool ellipse;

	// если функция может возбуждать исключительные ситуации,
	// установлен в true
	bool canThrow;
		
public:
	// конструктор принимает пакет и флаг имен
	FunctionPrototypeMaker( const NodePackage &pp, bool nn = false ) ;

	// метод создающий прототип функции из пакета и возвращающий его
	// в качестве результата работы объекта
	FunctionPrototype *Make();
};


// строитель catch-декларации, возвращает объект
class CatchDeclarationMaker
{
	// пакеты
	const NodePackage &typeSpec, &declPkg;

	// позиция 
	Position errPos;

public:
	// задаем пакеты
	CatchDeclarationMaker( const NodePackage &ts, const NodePackage &dp, const Position &ep )
		: typeSpec(ts), declPkg(dp), errPos(ep) {
	}

	// построить catch-декларацию, вернуть объект
	::Object *Make();
};
