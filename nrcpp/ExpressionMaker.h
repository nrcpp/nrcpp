// интерфейс для строительства выражений - ExpressionMaker.h


// дополнительные функции для строительства выражений
namespace ExpressionMakerUtils
{
	// класс - возвращаемое значение CorrectObjectInitialization,
	// помимо булевого значения, содержит конструктор, который исп. при
	// инициализации
	class InitAnswer {
		bool isCorrect;						// флаг корректности инициализации
		const ConstructorMethod *ictor;		// конструктор используемый при инициализации
	public:
		InitAnswer( bool flag ) 
			: isCorrect(flag), ictor(NULL) {
		}

		// ответ с конструктором. isCorrect всегда true в этом случае
		InitAnswer( const ConstructorMethod &ctor )
			: isCorrect(true), ictor(&ctor) {
		}

		// вернуть конструктор
		const ConstructorMethod *GetCtor() const {
			return ictor;
		}

		// вернуть флаг
		operator bool() const {
			return isCorrect;
		}
	};

	// строитель простого типа, используется в выражениях вида 'int ()',
	// в приведении типа вызовом функции
	POperand MakeSimpleTypeOperand( int tokcode );

	// создать типовой операнд в выражении, исп. в приведении типа,
	// typeid, new
	POperand MakeTypeOperand( const NodePackage &np );

	// проверка, является ли тип целым
	bool IsIntegral( const TypyziedEntity &op );

	// проверка, является ли тип арифметическим
	bool IsArithmetic( const TypyziedEntity &op );

	// проверяка, если тип является указателем rvalue
	bool IsRvaluePointer( const TypyziedEntity &op );

	// проверка, является ли операнд интерпретируемым
	bool IsInterpretable( const POperand &op, double &rval );

	// методы проверяющие, является ли тип классовым или склярным
	bool IsClassType( const TypyziedEntity &type ) ;

	// проверяет, является ли операнд, модифицируемым lvalue, 
	// если нет выводит ошибку и возвращает false
	bool IsModifiableLValue( const POperand &op, const Position &errPos, PCSTR opName );

	// метод проверяет проверяет эквивалентность типов в контексте выражений.
	// провряет только совпадение типов и производных типов, без учета квалификаторов
	bool CompareTypes( const TypyziedEntity &type1, const TypyziedEntity &type2 );

	// склярный, значит не классовый и не функция и не void
	bool IsScalarType( const TypyziedEntity &type );

	// если тип арифметический или указатель
	inline bool IsArithmeticOrPointer( const TypyziedEntity &type ) {
		return IsArithmetic(type) || IsRvaluePointer(type);
	}

	// вернуть true, если тип является функцией, ссылкой на функцию, указателем
	// на функцию или указателем на член-функцию
	bool IsFunctionType( const TypyziedEntity &type );

	// проверка, является ли операнд lvalue
	bool IsLValue( const POperand &op );

	// проверка, является ли тип константным. Т.е. константым
	// на самом верхнем уровне, либо указатель, либо базовый тип
	bool IsConstant( const TypyziedEntity &op );

	// создать вызов функции
	POperand MakeFunctionCall( POperand &fn, PExpressionList &params );

	// проверяет доступность конструктора по умолчанию, конструктора копирования, 
	// деструктора по требованию. Используется при сооздании или инициализации
	// объекта
	bool ObjectCreationIsAccessible( const ClassType &cls, 
		const Position &errPos, bool ctor, bool copyCtor, bool dtor );

	// преобразование к rvalue, необходимо, чтобы тип был полностью объявлен 
	// и не void. Также проверяет, чтобы операнд был Primary или Expression
	bool ToRValue( const POperand &val, const Position &errPos );

	// преобразование к rvalue при копировании. Допускается необъявленный класс,
	// если производный тип ссылка или указатель
	bool ToRvalueCopy( const POperand &val, const Position &errPos );

	// преобразование типа к целому или перечислимому типу. В случае
	// если преобазование возможно, возвращает операнд, иначе выводим ошибку
	// и возвращает NULL. Используется при проверке выражений. Также
	// проверяет возможность преобразования операнда к rvalue
	bool ToIntegerType( POperand &op, const Position &errPos, const string &opName );

	// преобразование типа к арифметическому типу
	bool ToArithmeticType( POperand &op, const Position &errPos,const string &opName );

	// преобразование типа к типу указателя
	bool ToPointerType( POperand &op, const Position &errPos, const string &opName );

	// преобразование типа к склярному типу 
	bool ToScalarType( POperand &op, const Position &errPos, const string &opName );
		
	// преобразование к арифметическому типу или указателю
	bool ToArithmeticOrPointerType( POperand &op, 
		const Position &errPos, const string &opName );

	// создает наибольший тип из двух, при условии что оба арифметические и
	// возвращает результат в виде вновь созданной сущности
	TypyziedEntity *DoResultArithmeticOperand( const TypyziedEntity &op1,
		const TypyziedEntity &op2 );

	// возвратить копию типа, если тип является ссылочным, убрать ссылку
	TypyziedEntity *DoCopyWithIndirection( const TypyziedEntity &type );

	// проверить, если нестатический  член класса, 
	// используется без this, тогда вывести ошибку и вернуть -1. Если операнду this
	// не нужен, вернуть 1, иначе 0.
	int CheckMemberThisVisibility( const POperand &dm, 
		const Position &errPos, bool printError = true );

	// проверить корректность инициализации объекта. Сравнивается только
	// тип и список инициализаторов. Не инициализируемые элементы, такие как
	// функция не учитываются
	InitAnswer CorrectObjectInitialization( const TypyziedEntity &obj,
		const PExpressionList &initList, bool checkDtor, const Position &errPos );

	// проверить корректность задания значения по умолчанию для параметра
	bool CorrectDefaultArgument( const Parametr &param,
		const Operand *defArg, const Position &errPos );
}


// строитель 'this'. Если 'this' в текущей области видимости
// невозможен, выводит ошибку и возвращает 'errorOperand'
class ThisMaker
{
	// позиция для вывода ошибки
	const Position &errPos;

	// выдляет память для 'this'
	const TypyziedEntity *MakeThis( const Method &meth ) const;
		
public:
	// задать позицию
	ThisMaker( const Position &ep ) 
		: errPos(ep) {
	}

	// создаеть указатель 'this', если возможно.
	POperand Make();
};


// строитель литералов. Целые, вещественные, символьные, строковые,
// булевы
class LiteralMaker
{
	// лексема с литералом
	const Lexem &literalLxm;

	// переводит символьный литерал в целое число
	int CharToInt( PCSTR chr, bool wide ) const;

public:
	// задаем литерал
	LiteralMaker( const Lexem &lxm ) 
		: literalLxm(lxm) {
	}

	// создать PrimaryOperand с типизированной сущностью созданной
	// на основе лексемы
	POperand Make();
};


// строитель операнда на основе пакета с идентификатором.
// Результатом работы может быть либо PrimaryOperand, если
// идентификатор является однозначным, либо TypeOperand,
// если идентификатор является типом, либо OverloadOperand, если
// идентификатор является перегруженной функцией с несколькими
// декларациями, либо ErrorOperand, если идентификатор является
// неоднозначным или не найден вовсе. Также проверяется доступность
// идентификатора.
class IdentifierOperandMaker
{
	// пакет с идентификатором
	const NodePackage &idPkg;
	
	// позиция в которой находится идентификатор в файле
	Position errPos;

	// имя идентификатора, как оно задано в программе
	CharString name;
	
	// флаг, который указывает на то, что не следует выводить
	// ошибку в случае если идентификатор не найден. Используется
	// при поиске перегруженных операторов
	bool noErrorIfNotFound;

	// флаг устанавливается если идентификатор не найден
	mutable bool notFound;

	// область видимости, относительно который ищется идентификатор,
	// может не задаваться
	mutable const SymbolTable *curST;

	// объект, в случае если обращение идет к члену через оператор '.' или '->'
	const TypyziedEntity *object;

	// создать переменную, для предотвращения вывода ошибок в дальнейшем.
	// В случае если имя квалифицированное или является оператором,
	// имя не создается
	void MakeTempName( const CharString &nam ) const;

	// ислючает дубликаты из списка, согласно правилам языка. 
	bool ExcludeDuplicates( RoleList &idList ) const;

	// созлать операнд на основании преобразованного списка
	POperand MakeOperand( const QualifiedNameManager &qnm ) const;

	// проверить, перекрывает ли 'srcId' идентификатор 'destId'
	// из другого класс иерархии. Если destId принадлежит классу
	// 'V', который является виртуальным по отношению к curST,
	// а srcId принадлежит к классу 'B' базовому классу 'curST',
	// и 'B' наследует 'V', то возвращается true
	bool HideVirtual( const Identifier *destId, const Identifier *srcId ) const;

public:

	// задается только область видимости, без объекта
	IdentifierOperandMaker( const NodePackage &ip, 
		const SymbolTable *cst = NULL, bool neinf = false ) ;

	// получить пакет с идентификатором. Если область видимости задана,
	// значит будем искать исключительно в ней. Используется в выражении
	// с '.'  и '->'
	IdentifierOperandMaker( const NodePackage &ip, const TypyziedEntity *obj ) ;
	
	// создать операнд. Предварительно проверив идентификатор
	// на существование, доступность и однозначность.
	POperand Make();

	// если идентификатор не найден, вернуть true
	bool IsNotFound() const {
		return notFound ;
	}
};


// интерфейс для преобразователей, содержит в себе чисто виртуальные методы,
// которые используются для преобразования
class Caster
{
public:
	// категория преобразования	
	enum CC { 
		CC_EQUAL,				// полное соотв.
		CC_QUALIFIER,			// преобразование квалификаторов, 
		CC_INCREASE,			// продваижение типа,
		CC_STANDARD,			// стандартное преобразование, 
		CC_USERDEFINED,			// преобразование определенное пользователем 
		CC_NOCAST				// преобразование невозможно,
	} castCategory;

	// виртуальный деструктор, для корректного вызова
	virtual ~Caster()  { 
	}

	// классифицировать преобразование. Логически проверить возможность
	// преобразования одного типа в другой, а также собрать необходимую
	// информацию для физического преобразования
	virtual void ClassifyCast() = 0;

	// проверить возможность физического преобразования одного типа
	// в другой
	virtual bool IsConverted() const = 0;

	// выполнить физическое преобразование, изменив 
	// содержимое дерева выражений. Позиция задается для вывода ошибок,
	// таких как недоступность или explicit-конструктор
	virtual void DoCast( const POperand &destOp, POperand &srcOp, const Position &errPos ) = 0;

	// вернуть сообщение об ошибке
	virtual const CharString &GetErrorMessage( ) const = 0;

	// получить категорию преобразования, необходима при разрешении
	// неоднозначности
	virtual CC GetCastCategory() const = 0;
};


// интеллектуальный указатель на преобразователь
typedef SmartPtr<Caster> PCaster;


// приведение из склярного типа в склярный. Изменяет дерево выражений
// только в случае если имеем С++-приведение (из производного в базовый)
class ScalarToScalarCaster : public Caster
{	
	// задаваемый тип из которого следует преобразовать в нужную
	// категорию
	TypyziedEntity destType, srcType;

	// сохраним оригиналы, на случай проверки на нулевой указатель
	const TypyziedEntity &origDestType, &origSrcType;

	// сообщение об ошибке, в случае ее возникновения 
	// (когда преобразование невозможно)
	CharString errMsg;

	// флаг устанавливается в true, если преобразование возможно
	bool isConverted;

	// если srcType приводим к destType, только в случае копирования
	bool isCopy;
	
	// если было преобразование из производного в базовый
	bool derivedToBase;

	// результирующий тип. В случае копирования, всегда правый. В противном
	// случае наибольший. В случае неудачного копирования - NULL
	const TypyziedEntity *resultType;

	// установлены в true, если второй операнд rvalue
	bool isRefConv1, isRefConv2;	

	// метод выбирает наибольший тип из двух по правилам языка
	const TypyziedEntity *GetBiggerType() ;

	// выполнить стандартные преобразования из массива в указатель,
	// из функции в указатель на функцию, и из метода в указатель на член фукнцию,
	// если требуется
	bool StandardPointerConversion( TypyziedEntity &type, const TypyziedEntity &orig );

	// преобразование производного к базовому, если возможно,
	// преобразовать и вернуть 0. Возвращает -1, если преобразование
	// прошло с ошибкой, Иначе > 0.
	int DerivedToBase( const TypyziedEntity &base, const TypyziedEntity &derived, bool ve ) ;

	// преобразование производного к базовому, только классы уже известны
	int DerivedToBase( const ClassType &base, const ClassType &derived, bool ve );

	// определить категорию преобразования для арифметических типов,
	// учитывается порядок типов в случае копирования
	void SetCastCategory( const BaseType &bt1, const BaseType &bt2 );

	// выполнить преобразование над операндом, внеся изменение в дерево выражений.
	// Преобразование из ссылки в нессылку
	void MakeReferenceConversion( POperand &pop, int op );

	// выполнить преобразование из производного класса в базовый
	// внеся изменение в дерево выражений. Приводит правый операнд (derived),
	// к типу левого (base)
	void MakeDerivedToBaseConversion( const TypyziedEntity &baseClsType,
		const POperand &base, POperand &derived );

	// проверка квалификации, в случае удачного преобразования. Имеет
	// значение только при копировании. Правый операнд должен быть менее
	// квалифицирован, чем правый. В случае, если квалификация 'src',
	// больше чем 'dest' вернет false
	bool QualifiersCheck( TypyziedEntity &dest, TypyziedEntity &src );
	

public:
	// в конструкторе задается два типа и флаг копирования. Если флаг
	// установлен, значит srcType, следует приводить к destType, в
	// противном случае два типа нужно привести к общему
	ScalarToScalarCaster( const TypyziedEntity &dest, const TypyziedEntity &src, bool ic ) 
		: destType(dest), srcType(src), origDestType(dest), origSrcType(src), isCopy(ic) {

		isConverted = false;
		resultType = NULL;		
		castCategory = CC_NOCAST;
		isRefConv1 = destType.GetDerivedTypeList().IsReference();
		isRefConv2 = srcType.GetDerivedTypeList().IsReference();
		derivedToBase = false;
	}
	
	// возможно для 'resultType' было выделение памяти, след. ее
	// необх освободить
	~ScalarToScalarCaster() {
		if( resultType != &destType && resultType != &srcType )
			delete resultType;
	}

	// классифицировать преобразование. Логически проверить возможность
	// преобразования одного типа в другой, а также собрать необходимую
	// информацию для физического преобразования
	void ClassifyCast();

	// выполнить физическое преобразование, изменив 
	// содержимое дерева выражений. Преобразуем из srcOp в destOp, при
	// этом должен изменяться только srcOp
	void DoCast( const POperand &destOp, POperand &srcOp, const Position &errPos  ) ;

	// если преобразование возможно - вернуть true
	bool IsConverted() const {
		return isConverted;
	}

	// вернуть сообщение об ошибке
	const CharString &GetErrorMessage( ) const {
		return errMsg;
	}


	// если было преобразование из производного класса в базовый
	bool IsDerivedToBase() const {
		return derivedToBase;
	}

	// вернуть результирующий тип
	const TypyziedEntity *GetResultType() const {
		return resultType;
	}

	// получить категорию преобразования, необходимо при
	// разрешение неоднозначности
	CC GetCastCategory() const {
		return castCategory;
	}
};


// интеллектуальный указатель
typedef SmartPtr<ScalarToScalarCaster> PScalarToScalarCaster;


// менеджер приведения из классового типа с помощью оператора преобразования класса.
// Можно задавать как конкретный тип, так и категорию (указатели, арифметический)
class OperatorCaster : public Caster
{
public:
	// категории преобразования
	enum ACC { ACC_NONE, ACC_TO_INTEGER, ACC_TO_ARITHMETIC, 
		ACC_TO_POINTER, ACC_TO_ARITHMETIC_OR_POINTER, ACC_TO_SCALAR, };

private:
	// категория задаваемая пользователем
	ACC category;

	// задаваемый тип из которого следует преобразовать в нужную
	// категорию
	const TypyziedEntity &clsType, &scalarType;

	// сообщение об ошибке, в случае ее возникновения 
	// (когда преобразование невозможно)
	CharString errMsg;

	// флаг устанавливается в true, если преобразование возможно
	bool isConverted;

	// если флаг установлен, значит происходит копирование
	bool isCopy;

	// оптимальный оператор для преобразования из классового в склярный.
	// Может быть NULL
	const ClassCastOverloadOperator *castOperator;

	// склярный преобразователь, из результирующего типа оператора преобразования
	// в конечный тип
	PScalarToScalarCaster scalarCaster;

	// стратегии сравнения операторов преобразования с категориями
	// возвращают 0, если оператор подходит для преобразования, возвращают
	// 1 если оператор не подходит и -1, если оператор неоднозначен с другим
	// оператором (Cast Operator Strategy)

	// сравнение квалификаций у кандидата и текущего рассматриваемого оператора,
	// по квалификации выявляется возвращаемое значение
	int CVcmp( const ClassCastOverloadOperator &cur ); 

	// проверяет соотв. типа оператора с задаваемым типом, основан на явном
	// сравнении типов с помощью ScalarToScalarCaster
	int COSCast( const ClassCastOverloadOperator &ccoo );

	// проверяет, является ли тип оператора арифметическим
	int COSArith( const ClassCastOverloadOperator &ccoo );

	// проверяет, является ли тип оператора указателем
	int COSPointer( const ClassCastOverloadOperator &ccoo );

	// проверяет, является ли тип оператора целым
	int COSIntegral( const ClassCastOverloadOperator &ccoo );

	// проверяет, является ли операнд склярным типом
	int COSScalar( const ClassCastOverloadOperator &ccoo );

	// проверяет, является ли операнд арифметическим или указателем
	int COSArithmeticOrPointer( const ClassCastOverloadOperator &ccoo );

public:
	// задаем явное приведение, к конкретному типу
	OperatorCaster( const TypyziedEntity &dest, const TypyziedEntity &src, bool ic ) 
		: category(ACC_NONE), clsType(src.GetBaseType().IsClassType() ? src : dest),
		scalarType(&src == &clsType ? dest : src), isCopy(ic), 
		isConverted(false), scalarCaster(NULL), castOperator(NULL) {

		castOperator = NULL;				
	}

	// задаем необходимую информацию для выполнения преобразвания
	OperatorCaster( ACC acc, const TypyziedEntity &ct )
		: category(acc), clsType(ct), scalarType(ct), isConverted(false),
		  scalarCaster(NULL), castOperator(NULL) {		
	}

	// конструктор используется для преобразования из clsType в определенную категорию,
	// scalarType используется как вспомагательный. Используется при преобразовании
	// из классового в в указатель на функцию. В st хранится список параметров
	OperatorCaster( ACC acc, const TypyziedEntity &ct, const TypyziedEntity &st )
		: category(acc), clsType(ct), scalarType(st), isConverted(false),
		  scalarCaster(NULL), castOperator(NULL) {		
	}

	// выявить оператор класса который оптимально подходит для преобразования
	// в заданный тип или категорию
	void ClassifyCast();

	// выполнить физическое преобразование, изменив 
	// содержимое дерева выражений. SrcOp - классовый тип, который,
	// содержит оператор приведения, destOp - тип к которому приводим выражение.
	// В srcOp будет приведенное к типу destOp выражение
	void DoCast( const POperand &destOp, POperand &srcOp, const Position &errPos  ) ;

	// классифицировать преобразование
	// если преобразование невозможно
	bool IsConverted() const {
		return isConverted;
	}

	// вернуть сообщение об ошибке
	const CharString &GetErrorMessage( ) const {
		return errMsg;
	}

	// вернуть оператор, может быть NULL
	const ClassCastOverloadOperator *GetCastOperator() const {
		return castOperator;
	}

	// получить категорию преобразования, необходима при разрешении
	// неоднозначности
	CC GetCastCategory() const {
		return isConverted ? CC_USERDEFINED : CC_NOCAST;
	}
};


// приведение из классового типа в склярный. Достигается путем вызова
// конструткора класса. Задача преобразователя найти однозначный конструткор
// для имеющегося склярного типа
class ConstructorCaster : public Caster
{	
	// два типа: один классовый, другой склярный
	const TypyziedEntity &lvalue, &rvalue;

	// сообщение об ошибке, в случае ее возникновения 
	// (когда преобразование невозможно)
	CharString errMsg;

	// флаг устанавливается в true, если преобразование возможно
	bool isConverted;

	// если установлен в true, значит используется явное приведение и
	// можно использовать explicit-конструктор
	bool explicitCast;

	// метод возвращает false, если требуется прекратить цикл
	// поиска подходящего конструктора. В случае если конструктор
	// подходит для преобразования, изменить соотв. поля класса
	bool AnalyzeConstructor( const ConstructorMethod &srcCtor );
	
	// оптимальный конструктор для преобразования из склярного в классовый.
	// Может быть NULL
	const ConstructorMethod *constructor;

	// склярный преобразователь, из результирующего типа оператора преобразования
	// в конечный тип
	PScalarToScalarCaster scalarCaster;

	// проверка доступности конструктора или деструктора
	bool CheckCtorAccess(  const POperand &destOp, const Method &meth   ) ;

public:
	// задаем два типа. rv должен иметь склядный тип, а lv классовый. Задача
	// класса преобразовать склярный тип в классовый с помощью конструктора
	// класса.
	ConstructorCaster( const TypyziedEntity &lv, const TypyziedEntity &rv, bool ec ) 
		: lvalue(lv), rvalue(rv), constructor(NULL), scalarCaster(NULL), explicitCast(ec) {
		isConverted = false;
	}
		
	// классифицировать преобразование. Логически проверить возможность
	// преобразования одного типа в другой, а также собрать необходимую
	// информацию для физического преобразования
	void ClassifyCast();

	// выполнить физическое преобразование, изменив 
	// содержимое дерева выражений. SrcOp - классовый тип, который,
	// содержит оператор приведения, destOp - тип к которому приводим выражение.
	// В srcOp будет приведенное к типу destOp выражение
	void DoCast( const POperand &destOp, POperand &srcOp, const Position &errPos  ) ;

	// если преобразование возможно - вернуть true
	bool IsConverted() const {
		return isConverted;
	}

	// вернуть сообщение об ошибке
	const CharString &GetErrorMessage( ) const {
		return errMsg;
	}

	// вернуть конструктор, может быть NULL
	const ConstructorMethod *GetConstructor() const {
		return constructor;
	}

	// получить категорию преобразования, необходима при разрешении
	// неоднозначности
	CC GetCastCategory() const {
		return isConverted ? CC_USERDEFINED : CC_NOCAST;
	}
};


// интеллектуальный указатель на конструкторный преобразователь
typedef SmartPtr<ConstructorCaster> PConstructorCaster;

// интеллектуальный указатель на операторный преобразователь
typedef SmartPtr<OperatorCaster> POperatorCaster;


// приведение из классового типа в классовый. Для приведения из одного классового
// в другой можно использовать конструткор, оператор приведения, а также
// преобразование из производного класса в базовый
class ClassToClassCaster : public Caster
{	
	// два типа, оба классовых
	const TypyziedEntity &cls1, &cls2;

	// сообщение об ошибке, в случае ее возникновения 
	// (когда преобразование невозможно)
	CharString errMsg;

	// флаг устанавливается в true, если преобразование возможно
	bool isConverted;

	// если установлен в true, значит используется явное приведение и
	// можно использовать explicit-конструктор
	bool explicitCast;

	// категория преобразования, может быть: 
	// CC_NOCAST, CC_EQUAL, CC_STANDARD, CC_USERDEIFNED
	CC category;

	// метод, который выполняет преобразование - конструктор или оператор преобразования.
	// может остутсвовать, в случае преобразования базового типа в производный
	const Method *conversionMethod;

	// класс преобразователь, который вызывает метод преобразователь,
	// либо cls1, либо cls2
	const ClassType *conversionClass;
	
	// преобразователь. Может быть склярным, в случае преобразования производного
	// класса к базовому, операторным, в случае преобразования правого операнда
	// к левому с помощью оператора преобразования правого и конструкторным,
	// если у левого операнда есть конструктор принимающий правый
	PCaster caster;

public:
	// задаем два типа. rv должен иметь склядный тип, а lv классовый. Задача
	// класса преобразовать склярный тип в классовый с помощью конструктора
	// класса.
	ClassToClassCaster( const TypyziedEntity &c1, const TypyziedEntity &c2, bool ec ) 
		: cls1(c1), cls2(c2), caster(NULL), explicitCast(ec) {
		isConverted = false;
		conversionMethod = NULL;
		conversionClass = NULL;
		category = CC_NOCAST;
	}
		
	// преобразовать один классовый тип к другому. Преобразование возможно тремя путями:
	// ссылка на производный класс преобразуется в базовый,
	// с помощью оператора приведения, с помощью конструктора, 
	void ClassifyCast();

	// выполнить физическое преобразование, из srcOp в destOp,
	// при этом изменяется srcOp
	void DoCast( const POperand &destOp, POperand &srcOp, const Position &errPos  ) ;

	// если преобразование возможно - вернуть true
	bool IsConverted() const {
		return isConverted;
	}

	// вернуть сообщение об ошибке
	const CharString &GetErrorMessage( ) const {
		return errMsg;
	}

	// получить метод, который выполняет преобразование
	const Method *GetConversionMethod() const {
		return conversionMethod;
	}

	// получить категорию преобразования, необходима при разрешении
	// неоднозначности
	CC GetCastCategory() const {
		return category;
	}
};


// менеджер приведения из одного типа в другой. Используется в операциях
// копирования, а также в любых других операциях. Его задачей, является
// выявление того преобразователя, который подходит для двух типов
class AutoCastManager
{
	// два типа
	const TypyziedEntity &destType, &srcType;

	// флаг копирования
	bool isCopy;

	// если установлен в true, значит имеем явное приведение типа
	// и можно использовать explicit-конструктор
	bool explicitCast;

	// преобразователь, память для которого выделяется динамически,
	// при выявлении его на основе задаваемых типов
	PCaster caster;

public:

	// задаем два типа, флаг копирования, флаг явного приведения
	AutoCastManager( const TypyziedEntity &dt, const TypyziedEntity &st, 
		bool ic, bool ec = false )
		: destType(dt), srcType(st), isCopy(ic), caster(NULL), explicitCast(ec) {
	}

	// выявить преобразователя
	PCaster &RevealCaster();
};


// разрешитель перегрузки функции. На основе списка функций и списка параметров,
// выявляет единственную функцию, которая является наилучшей для заданного списка.
// Если таких функций несколько, задает соотв. сообщение об ошибке. Ошибка
// возникает также если нет не одного соотв.
class OverloadResolutor
{
	// текущая функция кандидат
	const Function *candidate;

	// список преобразователей актуальных параметров в формальные
	typedef list<PCaster> CasterList;
	CasterList candidateCasterList;

	// список преобразователей, которые были получены из
	// метода ViableFunction, он будет использоваться для нахождения
	// наиболее оптимальной функции среди двух
	CasterList viableCasterList;

	// сообщение задается в случае возникновения ошибки
	CharString errMsg;

	// установлен в true, если функция неоднозначна. Если нет подходящей
	// функции не устанавливается
	bool ambigous;

	// задаваемый список перегруженных функций
	// (overload function list)
	const OverloadFunctionList &ofl;

	// задаваемый список имеющихся фактических параметров
	// (actual parametr list)
	const ExpressionList &apl;

	// объект, через который вызывается метод. Может быть NULL
	const TypyziedEntity *object;

	// метод выявляет единственную функцию, которая подходит под список параметров
	void PermitUnambigousFunction();

	// вернуть true, если функция 'fn' может принимать 'pcnt' параметров
	bool CompareParametrCount( const Function &fn, int pcnt );

	// проверяет, является ли функция корректной (кандидатом) для 
	// вызова, на основе имеющегося списка параметров.
	bool ViableFunction( const Function &fn );

	// сравнивает 'fn' с текущим кандидатом и проверяет, какая 
	// из функций наиболее подходит для списка параметров, если
	// подходят обе, вызывает неоднозначность и возвращает false
	bool SetBestViableFunction( const Function &fn );

public:

	// задаем список функций и список параметров. Список
	// функций должен содержать хотя бы одну функцию. Третим необязательным параметром
	// может идти указатель на объект, через который вызывается функция
	OverloadResolutor( const OverloadFunctionList &fl, const ExpressionList &pl, 
		const TypyziedEntity *obj = NULL )
		: ofl(fl), apl(pl), candidate(NULL), object(obj), ambigous(false) {

		INTERNAL_IF( fl.empty() );
		PermitUnambigousFunction();
	}

	// если функция неоднозначна
	bool IsAmbigous() const {
		return ambigous;
	}

	// получить однозначную функцию, которую можно вызвать с заданным
	// списком параметров. Может возвращать NULL, если функция неоднозначна,
	// либо вообще нет подходящей функции
	const Function *GetCallableFunction() const {
		return candidate;
	}

	// получить сообщение об ошибке
	const CharString &GetErrorMessage() const {
		return errMsg;
	}

	// выполнить приведение каждого параметра в списке к необходимому типу
	void DoParametrListCast( const Position &errPos );
};


// класс используется при строительстве выражений для поиска 
// перегруженного оператора на основании кода оператора и арности
// выражения. 
// Результатом работы класса является список операторов, либо
// пустой список в случае если оператор не найден.
class OverloadOperatorFounder
{
	// если оператор найден устанавливается в true
	bool found;

	// если оператор неоднозначен, установлен в true
	bool ambigous;

	// список операторов которые найдены в КЛАССЕ,
	// если не один оператор не найден - список пуст
	OverloadFunctionList clsOperatorList;

	// список операторов которые найдены в НЕ КЛАССОВОЙ ОВ,
	// если не один оператор не найден - список пуст
	OverloadFunctionList globalOperatorList;

	// закрытый метод, который вызывается конструктором для
	// поиска операторов
	void Found( int opCode, bool evrywhere, const ClassType *cls, const Position &ep  );

public:
	// в конструкторе задаются параметры для поиска и сразу же
	// передаются методу, который выполняет поиск
	OverloadOperatorFounder( int oc, bool evrywhere, const ClassType *cls, const Position &ep  )
		: found(false), ambigous(false) {
		Found(oc, evrywhere, cls, ep);
	}

	// если оператор найден, вернуть true
	bool IsFound() const {
		return found;
	}

	// если неоднозначен
	bool IsAmbigous() const {
		return ambigous;
	}

	// вернуть список перегруженных операторов. Список может быть пустой
	const OverloadFunctionList &GetClassOperatorList() const {
		return clsOperatorList;
	}

	// вернуть список глобальных перегруженных операторов
	const OverloadFunctionList &GetGlobalOperatorList() const {
		return globalOperatorList;
	}
};


// на основе типа операнда, находит унарный перегруженный оператор и
// строит на основе его вызов
class UnaryOverloadOperatorCaller
{
	// операнд
	const POperand &right;

	// код унарного оператора
	int opCode;

	// позиция для вывода ошибок
	const Position &errPos;

	// статический член, который представляет собой 0
	// используется для передачи постфиксному оператору
	static POperand null;

public:
	// задаем информацию для поиска оператора
	UnaryOverloadOperatorCaller( const POperand &r, int op, const Position &ep )
		: right(r), opCode(op), errPos(ep) {
	}

	// вернуть построенный вызов, либо NULL, если перегруженный оператор вызвать
	// невозможно
	POperand Call() const;
};


// на основе типов двух операндов, пытается найти перегруженный оператор
// на основе кода 'op'
class BinaryOverloadOperatorCaller
{
	// два операнда 
	const POperand &left, &right;

	// код оператора
	int opCode;

	// позиция для вывода ошибок
	const Position &errPos;

public:
	// в конструкторе задаем параметры, которые необходимы для вызова
	// оператора
	BinaryOverloadOperatorCaller( const POperand &l, const POperand &r, int op, 
		const Position &ep ) 
		: left(l), right(r), opCode(op), errPos(ep) {
	}

	// вернуть построенный вызов, либо NULL, если перегруженный оператор вызвать
	// невозможно
	POperand Call() const;
};


// интерпритатор. Вычисляет размер объекта или типа. Принимает в качестве
// параметра типизированную сущность и вычисляет ее размер. Размер вычисляется
// с учетом генерируемых структур внутри класса (виртуальные таблицы, базовые классы).
// Размер пустого класса по умолчанию равен 4. В случае если входной тип некорректен,
// например функция или необъявленный класс, возвращает -1, при этом можно запретить
// вывод ошибки
class SizeofEvaluator
{
	// тип, размер которого вычисляется 
	const TypyziedEntity &type;

	// в случае возникновения ошибки в этом члене сохраняется
	// причина ощибки в виде строки
	mutable PCSTR errMsg;

	// возвращает размер базового типа
	int GetBaseTypeSize( const BaseType &bt ) const;

	// возвращает размер структуры, класса или перечисления
	int EvalClassSize( const ClassType &cls ) const;

public:
	// задаем информацию, необходимую для вычисления размера типа
	SizeofEvaluator( const TypyziedEntity &t )
		: type(t), errMsg("") {
	}

	// вычислить размер типа, если тип некорректен, вернуть -1
	int Evaluate() const;

	// получить причину ошибки, в случае если размер не вычислен
	PCSTR GetErrorMessage() const {
		return errMsg;
	}
};


// интерпретатор унарных выражений
class UnaryInterpretator
{
	// операнд 
	const POperand &cnst;

	// оператор
	int op;

	// позиция для вывода ошибок
	const Position &errPos;

public:
	
	// в конструкторе задаем параметры. Интерпретации может не быть,
	// если операнд не константный или оператор не может быть интерпретирован
	UnaryInterpretator( const POperand &c, int op_, const Position &ep )
		: cnst(c), op(op_), errPos(ep) {
	}

	// интерпретировать. Если интерпретация невозможна вернуть NULL,
	// иначе указатель на новый операнд
	POperand Interpretate() const;
};


// интерпретатор бинарных выражений
class BinaryInterpretator
{
	// операнд1 и операнд2
	const POperand &cnst1, &cnst2;

	// оператор
	int op;

	// позиция для вывода ошибок
	const Position &errPos;
	
public:
	
	// в конструкторе задаем параметры. Интерпретации может не быть,
	// если операнд не константный или оператор не может быть интерпретирован
	BinaryInterpretator( const POperand &c1, const POperand &c2, int op_, const Position &ep )
		: cnst1(c1), cnst2(c2), errPos(ep), op(op_) {
	}

	// интерпретировать. Если интерпретация невозможна вернуть NULL,
	// иначе указатель на новый операнд
	POperand Interpretate() const;

	// метод, создает результат на основе двух операндов
	static POperand MakeResult( const BaseType &bt1, const BaseType & bt2, double res );
};


// интерпретатор тернаных выражений '?:'
class TernaryInterpretator
{
	// операнд1, операнд2, операнд3
	const POperand &cnst1, &cnst2, &cnst3;

public:
	
	// в конструкторе задаем параметры. Интерпретации может не быть,
	// если операнд не константный или оператор не может быть интерпретирован
	TernaryInterpretator( const POperand &c1, const POperand &c2, const POperand &c3 )
		: cnst1(c1), cnst2(c2), cnst3(c3) {
	}

	// интерпретировать. Если интерпретация невозможна вернуть NULL,
	// иначе указатель на новый операнд
	POperand Interpretate() const;
};


// класс распечатывает дерево выражений
class ExpressionPrinter
{
	// строка с выражением
	string resultStr;

	// распечатать выражение
	string PrintExpression( const POperand &expr );

	// распечатать бинарное выражение
	string PrintBinaryExpression( const BinaryExpression &expr );

public:
	// задать операнд
	ExpressionPrinter( const POperand &e ) 	{
		resultStr = PrintExpression(e);
	}

	// получить строку с распечатаным выражением
	const string &GetExpressionString() const {
		return resultStr;
	}

	// получить строковое представление оператора по коду
	static PCSTR GetOperatorName( int op );
};	


// проверка корректности инициализации параметра аргументом по умолчанию
class DefaultArgumentChecker
{
	// проверяемый параметр
	const Parametr &parametr;

	// аргумент по умолчанию
	const POperand &defArg;

	// позиция для вывода ошибки
	const Position &errPos;

public:

	// в конструкторе задается информация для проверки
	DefaultArgumentChecker( const Parametr &prm, const POperand &da, const Position &ep ) 
		: parametr(prm), defArg(da), errPos(ep) {
	}

	// проверка, с возвратом того же аргмента в случае успеха, либо
	// error operand в случае неудачи
	const POperand &Check() const;
};


// указатель на контроллер
class AgregatController;
typedef SmartPtr<AgregatController> PAgregatController;


// интерфейс для классов контроллеров агрегатов
class AgregatController
{
public:
	// для удаления производных классов через ук-ль на базовый
	virtual ~AgregatController() {
	}

	// класс является идентификатором события - "отсутствие элементов".
	// Иключение с типом этого класса возбуждается в случае, когда
	// инициализаторов больше, чем элементов
	struct NoElement {
		const Position errPos;
		NoElement( const Position &ep ) : errPos(ep) {}
	};

	// класс является идентификатором события - "тип не является агрегатом".
	struct TypeNotArgegat {
		const TypyziedEntity &type;
		TypeNotArgegat( const TypyziedEntity &t ) : type(t) {}
	};

	
	// конкретный контроллер должен возвращать новый подконтроллер,
	// если его элемент является агрегатом. Если не является вернуть this.
	// Сообщение обозначает появляение списка среди компонентов инициализации
	virtual AgregatController *DoList( const ListInitComponent &ilist ) = 0;

	// Появление атома (выражения) в списке. Конкретный контроллер возвращает
	// новый подконктроллер, если текущий инициализируемый элемент является 
	// агрегатом. Либо, если в текущем контроллере кончились элементы 
	// (данные-члены у структуры, размер у массива), возвращает родительский
	// контроллер, но только если он есть, иначе ошибка. И в последнем случае,
	// если текущий инициализируемый элемент не является агрегатом, то он
	// проверяется обычным образом на инициализацию и возвращается NULL.
	// NULL - возвращается только в случае если текущий элемент можно инициализировать
	virtual AgregatController *DoAtom( const AtomInitComponent &iator, bool ) = 0;

	// сообщение посылается, в случае если все элементы списка пройдены. Конкретный
	// контроллер должен проверить возможность инициализации по умолчанию для всех
	// своих элементов (для структуры оставшиеся члены, для массива один текущий элемент).
	virtual void EndList( const Position &errPos ) = 0;

protected:
	// выполняет инициализацию по умолчанию для имеющегося типа,
	// вызывается когда остались лишние элементы в агрегате
	void DefaultInit( const TypyziedEntity &type, const Position &errPos );
};


// контроллер инициализации массива
class ArrayAgregatController : public AgregatController
{
	// проверяемый тип элемента, если массив типа T, то elementType,
	// имеет тип T
	PTypyziedEntity elementType;

	// ссылка на массив объекта, для изменения размера
	Array &pArray;

	// действительный размер массива, может быть < 1, в случае
	// если он неизвестен
	const int arraySize;

	// количество уже инициализированных элементов, изменяется при каждом
	// вызове DoList или DoAtom
	int initElementCount;

	// указатель на родительский контроллер
	const AgregatController *parentController;

public:
	// в конструкторе задается тип иниц. элемента, и указатель
	// на родительский контроллер, а также ссылку на массив объекта 
	// для изменения его размера в случае необходимости
	ArrayAgregatController( const PTypyziedEntity &et, 
		const AgregatController *parent, Array &ar ) 

		: elementType(et), parentController(parent), initElementCount(0),
		  pArray(ar), arraySize(pArray.GetArraySize()) {
		
	}

	// обработать список
	AgregatController *DoList( const ListInitComponent &ilist );

	// обработать выражение, либо вернуть родтельский или новый контроллер,
	// в случае если элементов не осталось или текущий элемент агрегат
	AgregatController *DoAtom( const AtomInitComponent &iator, bool endList );

	// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
	// для одного элемента, либо игнорировать если все элементы инициализированы
	// либо размер массива неизвестен
	void EndList( const Position &errPos ) ;
};


// контроллер инициализации структуры
class StructureAgregatController : public AgregatController
{
	// указатель на родительский контроллер
	const AgregatController *parentController;

	// указатель на сам класс (структуру)
	const ClassType &pClass;
		
	// текущий индекс проверяемого элемента в списке членов класса,
	// изначально -1
	int curDMindex;

	// получить ук-ль на следующий инициализируемый не статический данный-член,
	// если такого нет в структуре, возвращает -1
	const DataMember *NextDataMember( ) ;

public:
	// в конструкторе задается инициализируемый объект, и указатель
	// на родительский контроллер, остальную информацию следует брать из
	// объекта
	StructureAgregatController( const ClassType &pc, const AgregatController *parent ) 
		: pClass(pc), parentController(parent), curDMindex(-1)  {
	}

	// обработать список
	AgregatController *DoList( const ListInitComponent &ilist );

	// обработать выражение, либо вернуть родтельский или новый контроллер,
	// в случае если элементов не осталось или текущий элемент агрегат
	AgregatController *DoAtom( const AtomInitComponent &iator, bool endList );

	// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
	// для оставшихся членов
	void EndList( const Position &errPos ) ;
};


// контроллер инициализации объединения
class UnionAgregatController : public AgregatController
{
	// указатель на родительский контроллер
	const AgregatController *parentController;

	// указатель на сам класс (структуру)
	const UnionClassType &pUnion;

	// установлен в true, если первый член уже был получен
	bool memberGot;

	// получить первый данный член, если он еще не получен
	const DataMember *GetFirstDataMember();

public:
	// в конструкторе задается инициализируемый объект, и указатель
	// на родительский контроллер, остальную информацию следует брать из
	// объекта
	UnionAgregatController( const UnionClassType &pu, const AgregatController *parent ) 
		: pUnion(pu), parentController(parent), memberGot(false) {
	}

	// обработать список
	AgregatController *DoList( const ListInitComponent &ilist );

	// обработать выражение, либо вернуть родтельский или новый контроллер,
	// в случае если элементов не осталось или текущий элемент агрегат
	AgregatController *DoAtom( const AtomInitComponent &iator, bool endList );

	// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
	// для оставшихся членов
	void EndList( const Position &errPos ) ;

	// перейти к следующему элементу
	bool IncCounter() {
		return false;
	}
};


// обрабатывает сообщения поступающие от списка инициализации и
// передает их соотв. контроллеру
class ListInitializationValidator
{
	// указатель на текущий контроллер
	AgregatController *pCurrentController;

	// список инициализации
	const ListInitComponent &listInitComponent;

	// позиция конца списка для вывода ошибки
	const Position &errPos;

	// ссылка на инициализируемый объект
	::Object &object;

	// список созданных объектов-контроллеров. Используется
	// для удаления созданных контроллеров, заполняется при вызове
	// MakeNewController
	static list<AgregatController *> allocatedControllers;

	// создать новый контроллер агрегата,для входного типа. Тип
	// должен быть также агрегатом
	static AgregatController *MakeNewController( 
		const TypyziedEntity &elemType, const AgregatController *prntCntrl );

	// проверить, является ли тип агрегатом. Если структура (класс) -
	// не может быть агрегатом, генерирует исключение типа ClassType 
	static bool IsAgregatType( const TypyziedEntity &type );

	// метод возвращает true, если тип является массивом типа char или wchar_t,
	// а операнд является строковым литералом
	static bool IsCharArrayInit( const TypyziedEntity &type, const POperand &iator );

	// рекурсивный метод, проходит по списку инициализации, в случае
	// появления подсписка вызывает рекурсию для подсписка
	void InspectInitList( const ListInitComponent &ilist );

public:
	// задаем ссылку на список и позицию конца списка
	ListInitializationValidator( const ListInitComponent &lic, 
		const Position &ep, ::Object &ob ) 
		: listInitComponent(lic), errPos(ep), pCurrentController(NULL), object(ob) {
		INTERNAL_IF( !allocatedControllers.empty() ); 
	}

	// освобождает память занятую контроллерами
	~ListInitializationValidator() {
		for( list<AgregatController *>::iterator p = allocatedControllers.begin();
			 p != allocatedControllers.end(); p++ )
			delete *p;
		allocatedControllers.clear();
	}

	// проверить валидность инициализации списком
	void Validate();

	// дружеские классы для доступа к статическим методам обработки агрегатов
	friend class ArrayAgregatController;
	friend class StructureAgregatController;
	friend class UnionAgregatController;
};


// проверка инициализации глобальной или локальной переменной. Работает
// как с одним инициализатором, так и с несколькими
class InitializationValidator
{
	// список аргументов конструктора объекта
	const PExpressionList &expList;	

	// ссылка на инициализируемый объект. Объект не константный,
	// т.к. его компоненты могут изменяться, такие как инициализируемое
	// значение или размер массива
	::Object &object;

	// позиция для вывода ошибок
	const Position &errPos;

	// в случае, если инициализация была конструктором, сохраняет его.
	// Информация необходима для генерации
	const ConstructorMethod *ictor;

	// проверяет инициализацию конструктором или одним значением,
	// или без значения
	void ValidateCtorInit();

public:
	// задаем входную информацию
	InitializationValidator( const PExpressionList &el, ::Object &obj, const Position &ep )
		: expList(el), object(obj), ictor(NULL), errPos(ep) {
	}

	// проверить инициализацию
	void Validate() {
		ValidateCtorInit();
	}

	// получить конструктор, может возвращать NULL, если инициализация
	// проходила без конструктора, либо была некорректна
	const ConstructorMethod *GetConstructor() const {
		return ictor;
	}
};


// структура хранящая элемент инициализации объекта при
// помощи списка инициализации конструткора
class ObjectInitElement
{
public:
	// тип разновидностей идентификатора
	enum IV { IV_DATAMEMBER, IV_VIRTUALBC, IV_DIRECTBC };

private:
	// инициализируемый данный-член или класс
	const Identifier &id;

	// выражение инициализации, может быть NULL
	PExpressionList expList;
	
	// разновидность идентификатора: данное-член, виртуальный базовый
	// класс, прямой базовый класс
	IV iv;

	// порядковый номер инициализации элемента
	unsigned int orderNum;

public:
	
	// задаем в конструкторе необх. информацию.
	ObjectInitElement( const Identifier &id_, const PExpressionList &el, IV iv_, unsigned on )
		: id(id_), expList(el), iv(iv_), orderNum(on) {
	}

	// получить идентфикатор
	const Identifier &GetIdentifier() const {
		return id;
	}

	// получить список выражений
	const PExpressionList &GetExpressionList() const {
		return expList;
	}

	// получить номер инициализации
	unsigned GetOrderNum() const {
		return orderNum;
	}

	// вернуть true, если идентификатор нестатический данное член
	bool IsDataMember() const {
		return iv == IV_DATAMEMBER;
	}

	// вернуть true, если идентификатор - виртуальный базовый класс
	bool IsVirtualBaseClass() const {
		return iv == IV_VIRTUALBC;
	}

	// вернуть true, если идентификатор - прямой базовый класс
	bool IsDirectBaseClass() const {
		return iv == IV_DIRECTBC;
	}

	// вернуть true, если элемент проинициализирован
	bool IsInitialized() const {
		return !expList.IsNull();
	}

	// задать список выражений, корректно только в случае если
	// этот список не задан
	void SetExpressionList( const PExpressionList &el ) {
		INTERNAL_IF( !expList.IsNull() );
		expList = el;
	}

	// задать порядковый номер инициализации, изменяется в случае
	// если пользователь задает инициализатор явно
	void SetOrderNum( unsigned on ) {
		orderNum = on;
	}

	// оператор сравнения для поиска идентификатора
	bool operator==( const ObjectInitElement &oie ) const {
		return &id == &oie.GetIdentifier();
	}

	// оператор сравнения для поиска идентификатора
	bool operator!=( const ObjectInitElement &oie ) const {
		return &id != &oie.GetIdentifier();
	}

	void operator=(const ObjectInitElement &oie ) const {
		return *this = oie;
	}
};


// тип - список элементов инициализации объекта
typedef list<ObjectInitElement> ObjectInitElementList;


// проверяет корректность инициализации объекта списком
// инициализации конструктора
class CtorInitListValidator
{
	// ссылка на конструктор
	const ConstructorMethod &pCtor;

	// ссылка на инициализируемый класс
	const ClassType &pClass;

	// позиция для вывода ошибок
	Position errPos;

	// список элементов инициализации. Сначала заполняется
	// теми элементами, которые должны быть проинициализированы,
	// после чего добавляет к ним инициализаторы, которые заданы явно
	ObjectInitElementList oieList;

	// считает количество явно инициализированных элементов, для
	// того чтобы после явной инициализации выстроить элементы в
	// правильном порядке
	unsigned explicitInitCounter;
	
	// пройти вверх по иерархии классов, задав виртуальные классы
	void SelectVirtualBaseClasses( const BaseClassList &bcl, unsigned &orderNum );

	// заполнить список зависимыми элементами инициализации,
	// виртуальными базовыми классами, прямыми базовыми классами, 
	// нестатическими данными-членами
	void FillOIEList( );

public:
	// задать конструктор для проверки инициализации
	CtorInitListValidator( const ConstructorMethod &pc, const Position &ep )
		: pCtor(pc), pClass( static_cast<const ClassType &>(pc.GetSymbolTableEntry()) ),
		errPos(ep), explicitInitCounter(0) {

		FillOIEList( );		// заполним список инициализируемых элементов
	}

	// добавить явно инициализируемый элемент, если он не присутствует в списке,
	// вывести ошибку. Также выводит ошибку если присутствует два таких элемента
	// в списке, что означает неоднозначность. 
	void AddInitElement( const POperand &id, 
		const PExpressionList &expList, const Position &errPos, unsigned &orderNum ) ;

	// выполнить заключительную проверку, после того как весь список считан.
	// Проверить неинициализированные элементы. Задать правильный порядок
	// инициализации
	void Validate();

	// возвратить список инициализации для генератора
	const ObjectInitElementList &GetInitElementList() const {
		return oieList;
	}
};


// далее идут классы строители унарных, бинарных и тернарных выражений.
// Каждый класс имеет конструктор для получения операндов и оператора


// явное приведение типа - ( type )
class TypeCastBinaryMaker
{
	// тип к которому приводится выражение
	const POperand &left;

	// само выражение
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

	// процедура сбрасывает квалификацию у выражения и возвращает новый тип
	PTypyziedEntity UnsetQualification();

	// проверить приведение с помощью static_cast. Вернуть 0 в случае успеха,
	// вернуть 1 в случае неудачи, -1 в случае если приведение нужно прекратить
	int CheckStaticCastConversion( const TypyziedEntity &expType );

	// проверить приведение с помощью reinterpret_cast. Вернуть true в случае успеха	
	bool CheckReinterpretCastConversion( const TypyziedEntity &expType );

public:

	// задаем два операнда и оператор
	TypeCastBinaryMaker( const POperand &tp, const POperand &exp, int op, const Position &ep  )
		: left(tp), right(exp), errPos(ep) {

		INTERNAL_IF( op != OC_CAST );
		INTERNAL_IF( !tp->IsTypeOperand()  || 
			(!exp->IsExpressionOperand() && !exp->IsPrimaryOperand()) );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// вызов функции  - ( )
class FunctionCallBinaryMaker
{
	// функция, либо список функций, либо тип, либо операнд с типом функции
	const POperand &fn;

	// список параметров
	const PExpressionList &parametrList;

	// для вывода ошибок
	const Position &errPos;

	// если вызов неявный, установлен в true
	bool implicit;

	// флаг, устанавливается в true, если выясняется, что нет функции
	// которая бы подходила для вызова. Это может быть несовпадение типов.
	// Если функция неоднозначна, флаг не устанавливается
	mutable bool noFunction;

	// метод, выявляет наилучшую функцию из списка и проверяет ее доступность.
	// Если функция неоднозначна или не найдена, вернуть NULL
	const Function *CheckBestViableFunction( const OverloadFunctionList &ofl,
		const ExpressionList &pl, const TypyziedEntity *obj = NULL ) const;

	// если 'fn' - это объект, выявить для него перегруженный оператора '()',
	// либо оператор приведения к типу указатель на функцию. Если приведение
	// удалось, вернет новый операнд, иначе NULL
	POperand FoundFunctor( const ClassType &cls ) const;
	
	// проверяем возможность копирования и уничтожения каждого параметра.
	// Требуется чтобы конструктор копирования и деструктор были доступны
	static void CheckParametrInitialization( const PExpressionList &parametrList, 
		const FunctionParametrList &formalList, const Position &errPos ) ;

	// вызывает статический метод для проверки
	void CheckParametrInitialization( const FunctionParametrList &formalList ) const {
		CheckParametrInitialization(parametrList, formalList, errPos);
	}

	// объявляем дружественной функция, чтобы она имела доступ к
	// статическому методу проверки параметров
	friend 	ExpressionMakerUtils::InitAnswer ExpressionMakerUtils::CorrectObjectInitialization( 
		const TypyziedEntity &obj,const PExpressionList &initList, bool checkDtor, 
		const Position &errPos );

	// проверить является ли функция членом текущего класса, если она
	// вызывается напрямую без this, и совпадает ли ее квалификация с объектом
	void CheckMemberCall( const Method &m ) const;

public:

	// задаем два операнда и оператор. Последний параметр указывает на неявный
	// вызов если true. Если вызов неявный, строитель возвращает NULL, в случае
	// остутствия необх. функции и не выводит ошибки. Используется при неявном
	// вызове перегруженных операторов
	FunctionCallBinaryMaker( const POperand &f, const PExpressionList &pl, 
		int op, const Position &ep, bool imp = false )

		: fn(f), parametrList(pl), errPos(ep), implicit(imp), noFunction(false) {

		INTERNAL_IF( op != OC_FUNCTION );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();

};


// получение элемента массива - [ ]
class ArrayBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	ArrayBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != OC_ARRAY );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// селектор члена класса - .  ->
class SelectorBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа (имя в виде пакета)
	const NodePackage &idPkg;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

	// проверим, чтобы член принадлежал данному классу, проверка
	// осуществляется только при обращении к квалифицированному имени
	bool CheckMemberVisibility( const Identifier &id, const ClassType &objCls );

public:

	// задаем два операнда и оператор
	SelectorBinaryMaker( const POperand &l, const NodePackage &id, int op_, const Position &ep )
		: left(l), idPkg(id), op(op_), errPos(ep) {

		INTERNAL_IF( op != ARROW && op != '.' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// логическое НЕ - !
class LogicalUnaryMaker
{
	// операнд справа от оператора
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	LogicalUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != '!' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// переворот бит - ~
class BitReverseUnaryMaker
{
	// операнд справа от оператора
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнд и оператор
	BitReverseUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != '~' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// бинарные, проверки одинаковые для работы с целыми - % << >> ^ | &
class IntegralBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	IntegralBinaryMaker( const POperand &l, const POperand &r, int op_, const Position &ep )
		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != '%' && op != '^' && op != '|' && op != '&' && 
			op != LEFT_SHIFT && op != RIGHT_SHIFT );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


//  бинарное умножение и деление - *  /
class MulDivBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	MulDivBinaryMaker( const POperand &l, const POperand &r, int op_, const Position &ep )
		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != '*' && op != '/' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// унарный плюс и минус -   +  - 
class ArithmeticUnaryMaker
{
	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнда и оператор
	ArithmeticUnaryMaker( const POperand &r, int op_, const Position &ep )
		: right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != '+' && op != '-' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// возбуждение исключительной ситуации - throw
class ThrowUnaryMaker
{
	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнда и оператор
	ThrowUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != KWTHROW );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// разименование - *			
class IndirectionUnaryMaker	
{
	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнда и оператор
	IndirectionUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != '*' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// постфиксные и префиксные инкремент и декремент -  ++ --
class IncDecUnaryMaker
{
	// операнд справа или слева, в зависимости от того какой операнд,
	// постфиксный или префиксный
	const POperand &right;

	// код оператора может быть INCREMENT, DECREMENT и отрицательные
	// аналоги, в случае с отрицательными, означает, что операторы префиксные,
	// т.е. перед операндом
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнда и оператор
	IncDecUnaryMaker( const POperand &r, int op_, const Position &ep )
		: right(r), op(op_), errPos(ep) {

		INTERNAL_IF( abs(op) != INCREMENT && abs(op) != DECREMENT );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// взятие адреса -  &
class AddressUnaryMaker
{
	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнд
	AddressUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != '&' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// бинарное сложение -  +
class PlusBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	PlusBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != '+' );
	}

	// создать выражение. Может вернуть ErrorOperand, если создание невозможно
	POperand Make();
};


// бинарное вычитание -  -
class MinusBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	MinusBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != '-' );
	}

	// создать выражение. Может вернуть ErrorOperand, если создание невозможно
	POperand Make();
};


// операции сравнения -  <   <=   >=  >  ==  !=
class ConditionBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	ConditionBinaryMaker( const POperand &l, const POperand &r, int op_, const Position &ep )
		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != '<' && op != '>' && op != EQUAL && op != NOT_EQUAL &&
			op != GREATER_EQU && op != LESS_EQU );
	}

	// создать выражение. Может вернуть ErrorOperand, если создание невозможно
	POperand Make();
};


// 	логические операции - &&   ||
class LogicalBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	LogicalBinaryMaker( const POperand &l, const POperand &r, int op_, const Position &ep )
		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != LOGIC_AND && op != LOGIC_OR );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// условная операция -  ?:
class IfTernaryMaker
{
	// условие 
	const POperand &cond;

	// операнд слева от ':'
	const POperand &left;

	// операнд справа от ':'
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	IfTernaryMaker( const POperand &c, const POperand &l, const POperand &r, 
		int op, const Position &ep )

		: cond(c), left(l), right(r), errPos(ep)  {

		INTERNAL_IF( op != '?' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// присвоение -  =   +=   -=   *=   /=   %=   >>=   <<=  |=  &=  ^=
class AssignBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// оператор
	int op;

	// для вывода ошибок
	const Position &errPos;

	// метод, проверяет выражение если операция сдвоенная;
	// -1 - ошибка, 0 - продолжит проверку, 1 - создать выражение и выйти
	int CheckOperation( const string &opName );

public:

	// задаем два операнда и оператор
	AssignBinaryMaker( const POperand &l, const POperand &r, int op_, const Position &ep )
		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != '=' && op != MUL_ASSIGN && op != DIV_ASSIGN && op != PERCENT_ASSIGN &&
			op != PLUS_ASSIGN && op != MINUS_ASSIGN &&  op != LEFT_SHIFT_ASSIGN && 
			op != RIGHT_SHIFT_ASSIGN && op != AND_ASSIGN &&  op != XOR_ASSIGN &&
			op != OR_ASSIGN );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// запятая -  ,
class ComaBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	ComaBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != ',' );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// выделение памяти -  new  new[]
class NewTernaryMaker
{
	// sizeof выражение определяющее размер выделяемой памяти
	POperand allocSize;

	// тип выделяемой памяти
	POperand allocType;

	// список выражений размещения. Может быть NULL
	const PExpressionList &placementList;

	// список инициализаторов. Может быть NULL
	const PExpressionList &initializatorList;

	// если оператор глобальный, установить в true. new или new[],
	// выясним в зависимости от входного типа
	bool globalOp;

	// для вывода ошибок
	const Position &errPos;

	// перед тем как начать строительство выражения, следует преобразовать
	// пакет с типом в выражение sizeof, которое строится на основании пакета
	void MakeSizeofExpression( const NodePackage &typePkg );

	// метод производит поиск оператора new и строит вызов
	POperand MakeNewCall( bool array, bool clsType );

public:
	// задаем компоненты выражения и оператор
	NewTernaryMaker( const NodePackage &typePkg, const PExpressionList &pl, 
		const PExpressionList &il, bool glob, const Position &ep ) 
		
		: allocType(ExpressionMakerUtils::MakeTypeOperand(typePkg)), placementList(pl), 
		initializatorList(il), allocSize(NULL), globalOp(glob), errPos(ep) {

		// все операнды должны быть не нулевые. Списки размещения и
		// инициализаторов могут быть пустые
		INTERNAL_IF( placementList.IsNull() || initializatorList.IsNull() );
		
		// задаем allocSize, на основе пакета. Может быть ErrorOperand
		if( allocType->IsTypeOperand() )
			MakeSizeofExpression( typePkg );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// освобождение памяти -  delete  delete[]	
class DeleteUnaryMaker
{
	// удаляемый объект
	const POperand &right;

	// код оператора delete или delete[]
	int op;

	// для вывода ошибок
	const Position &errPos;

	// метод строит вызова оператора delete
	POperand MakeDeleteCall(  bool clsType  );

public:

	// задаем операнд и код оператора
	DeleteUnaryMaker( const POperand &r, int op_, const Position &ep )
		: right(r), op(op_), errPos(ep) {

		INTERNAL_IF( abs(op) != KWDELETE && abs(op) != OC_DELETE_ARRAY );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// доступ к указателю на член -  .*   ->*
class PointerToMemberBinaryMaker
{
	// операнд слева
	const POperand &left;

	// операнд справа
	const POperand &right;

	// код оператора
	int op;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	PointerToMemberBinaryMaker( const POperand &l, const POperand &r, 
		int op_, const Position &ep )

		: left(l), right(r), op(op_), errPos(ep) {

		INTERNAL_IF( op != DOT_POINT && op != ARROW_POINT );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// идентификация типа - typeid		
class TypeidUnaryMaker
{
	// удаляемый объект
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем операнд и код оператора
	TypeidUnaryMaker( const POperand &r, int op, const Position &ep )
		: right(r), errPos(ep) {

		INTERNAL_IF( op != KWTYPEID );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// динамическое приведение типа - dynamic_cast
class DynamicCastBinaryMaker
{
	// тип к которому приводится выражение (правый операнд)
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	DynamicCastBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != KWDYNAMIC_CAST );
	}


	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// статическое приведение типа - static_cast
class StaticCastBinaryMaker	
{
	// тип к которому приводится выражение (правый операнд)
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

	// проверить преобразование. Вернуть -1, если преобразование невозможно,
	// вернуть 1, если преобразование возможно и следует вернуть right, т.к.
	// физически оно не требуется, вернуть 0, в случае если следует построить
	// выражение
	int CheckConversion( );

	// возвращает true, если возможно преобразование из lvalue B к cv D&,
	// либо из B * к D *
	bool BaseToDerivedExist( const TypyziedEntity &toCls, const TypyziedEntity &fromCls );

public:

	// задаем два операнда и оператор
	StaticCastBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != KWSTATIC_CAST );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// приведение разных типов - reinterpret_cast
class ReinterpretCastBinaryMaker
{
	// тип к которому приводится выражение (правый операнд)
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	ReinterpretCastBinaryMaker( const POperand &l, const POperand &r, 
		int op, const Position &ep )

		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != KWREINTERPRET_CAST );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};


// сброс константности с типа - const_cast
class ConstCastBinaryMaker
{
	// тип к которому приводится выражение (правый операнд)
	const POperand &left;

	// операнд справа
	const POperand &right;

	// для вывода ошибок
	const Position &errPos;

public:

	// задаем два операнда и оператор
	ConstCastBinaryMaker( const POperand &l, const POperand &r, int op, const Position &ep )
		: left(l), right(r), errPos(ep) {

		INTERNAL_IF( op != KWCONST_CAST );
		INTERNAL_IF( !(left->IsTypeOperand() && 
			(right->IsExpressionOperand() || right->IsPrimaryOperand()) ) );
	}

	// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
	POperand Make();
};
