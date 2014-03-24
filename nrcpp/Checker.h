// интерфейс к КЛАССАМ-ЧЕКЕРАМ  - Checker.h


// объявлен в Maker.h
struct TempObjectContainer ;

// объявлен в Maker.h
struct TempOverloadOperatorContainer;

// объявлен в Maker.h
struct TempCastOperatorContainer;

// объявлен в Class.h
class ClassType;

// объявлен в Object.h
class TypyziedEntity;

// объявлен в Object.h
class FunctionPrototype;

// объявлен в Body.h
class Operand;


// функции используемые для тех проверок, где не требуется классы
namespace CheckerUtils
{	
	// проверить корректность списка производных типов
	bool CheckDerivedTypeList( const TempObjectContainer &object );

	// произвести проверку совместимости базового типа и производных. 
	bool CheckRelationOfBaseTypeToDerived( TempObjectContainer &object,
							 bool declaration, bool expr = false ) ;

	// проверяет, может ли класс быть базовым для другого класса
	bool BaseClassChecker(const ClassType &cls, const SymbolTableList &stl, 
		const Position &errPos, PCSTR cname );

	// проверяет возможность определения класса
	bool ClassDefineChecker( const ClassType &cls, const SymbolTableList &stl, 
		const Position &errPos );	

	// проверяет, если тип typedef, является классом, вернуть класс иначе 0
	const ClassType *TypedefIsClass( const ::Object &obj );

	// проверить достуность имени. Если имя не является членом класса, оно не проверяется
	// на доступность. Если имя недоступно - вывести ошибку
	void CheckAccess( const QualifiedNameManager &qnm, const Identifier &id, 
		const Position &errPos, const SymbolTable &ct = GetCurrentSymbolTable() );

	// проверка доступа для квалифицированного имени, проверяет доступность
	// каждого члена в квалификации и если последний член является классом,
	// вернуть его для проверки вместе с членом в CheckAccess, иначе вернуть 0.
	// Облась видимости 'ct' должна быть корректно преобразована из локальной
	// в функциональную, если требуется. Список квалификаторов в 'qnm' не должен
	// быть пустым.
	const ClassType *CheckQualifiedAccess( const QualifiedNameManager &qnm,
		const Position &errPos, const SymbolTable &ct );

	// проверить корректность объявления параметров по умолчанию у функций.
	// Если второй параметр 0, значит проверяем корректность только у первой
	void DefaultArgumentCheck( const FunctionPrototype &declFn, 
		const FunctionPrototype *fnInTable, const Position &errPos );
}


// проверка корректности декларации глобального объекта
class GlobalDeclarationChecker
{
	// временный контейнер объекта, чекер имеет право 
	// изменять его
	TempObjectContainer &object;

	// если объект переопределяется, установлен в true. Не имеет
	// значения корректно это или нет с точки зрения языка.
	// Этот флаг говорит о необходимости вставки в таблицу
	bool redeclared;

	// если произошла хоть одна ошибка при проверке, устновлен в true
	bool incorrect;

	// если проверяем локальную декларацию, установлен в true
	bool localDeclaration;

	// скрытая функция проверки
	void Check();


	// вывести ошибку, установить флаг
	void Error( PCSTR msg, PCSTR arg );
		
public:
	// задать параметры
	GlobalDeclarationChecker( TempObjectContainer &obj, bool ld = false ) 
		: object(obj), localDeclaration(ld) {
		redeclared = false;
		incorrect = false;
		Check();
	}

	// если переопределяется, имеется в виду, что этот объект
	// не нужно вставлять в таблицу
	bool IsRedeclared() const {
		return redeclared;
	}

	// если при проверке произошла ошибка
	bool IsIncorrect() const {
		return incorrect;
	}
};


// проверяет корректность параметра
class ParametrChecker 
{
	// список уже объявленных параметров функции
	// чтобы можно было выяснить переопределяется параметр или нет
	const FunctionParametrList &fnParamList;

	// временный контейнер параметра, чекер имеет право 
	// изменять его
	TempObjectContainer &parametr;

	// если произошла хоть одна ошибка при проверке, устновлен в true
	bool incorrect;

	// скрытая функция проверки параметра, выполняет основную работу
	// объекта 
	void Check();

public:

	// конструктор сразу же вызывает проверку после инициализации
	ParametrChecker( TempObjectContainer &prm, const FunctionParametrList &fp ) 
		: parametr(prm), fnParamList(fp) {

		// параметру не нужен флаг redeclared, т.к. даже если
		// уже объявлен параметр с таким именем, мы автоматически
		// делаем его безимянным, для сохранения сигнатуры функции
		incorrect = false;
		Check();
	}

	// если при проверке произошла ошибка
	bool IsIncorrect() const {
		return incorrect;
	}
};


// проверка типа throw-спецификации
class ThrowTypeChecker  
{
	// временный контейнер типа
	TempObjectContainer &throwType;

	// скрытая функция проверки типа, выполняет основную работу
	// объекта 
	void Check();

public:

	// конструктор сразу же вызывает проверку после инициализации
	ThrowTypeChecker( TempObjectContainer &tt ) 
		: throwType(tt)  {

		Check();
	}
};


// проверка catch-декларации
class CatchDeclarationChecker
{
	// временный контейнер типа
	TempObjectContainer &toc;

	// скрытая функция проверки типа, выполняет основную работу
	// объекта 
	void Check();

public:

	// конструктор сразу же вызывает проверку после инициализации
	CatchDeclarationChecker( TempObjectContainer &ct ) 
		: toc(ct)  {

		Check();
	}
};


// проверка декларации данного-члена
class DataMemberChecker
{
	// временная структура из которой строится член
	TempObjectContainer &dm;

	// устанавливается в true, если член переопредлен
	bool redeclared;

	// устанавливается в true, если член некорректен семантически
	bool incorrect;

	// метод проверки члена перед вставкой его в таблицу
	void Check();

	// возвращает true, если объект является константым
	bool ConstantMember();

	// возвращает не-NULL, если класс содержит не тривиальные к-тор,
	// к-тор копирования, деструктор, оператор копирования
	PCSTR HasNonTrivialSMF( const ClassType &cls );

public:
	// в конструкторе инициализируются поля
	DataMemberChecker( TempObjectContainer &m ) : dm(m), redeclared(false), incorrect(false) {
		Check();
	}

	// если переопределен
	bool IsRedeclared() const {
		return redeclared;
	}

	// если некорректен
	bool IsIncorrect() const {
		return incorrect;
	}

};


// проверка декларации данного-члена
class MethodChecker
{
	// временная структура из которой строится метод
	TempObjectContainer &method;

	// устанавливается в true, если метод переопредлен
	bool redeclared;

	// устанавливается в true, если метод некорректен семантически
	bool incorrect;

	// метод проверки метода перед вставкой его в таблицу
	void Check();
	

public:
	// в конструкторе инициализируются поля
	MethodChecker( TempObjectContainer &m ) : method(m), redeclared(false), incorrect(false) {
		Check();
	}

	// если переопределен
	bool IsRedeclared() const {
		return redeclared;
	}

	// если некорректен
	bool IsIncorrect() const {
		return incorrect;
	}

};


// проверка перегруженного оператора класса
class ClassOperatorChecker
{
	// временная структура в которой содержится декларатор и тип
	TempObjectContainer &op;

	// временная структура с кодом оператора и именем
	const TempOverloadOperatorContainer &tooc;

	// если оператор переопределен внутри класса
	bool redeclared;

	
	// метод проверки
	void Check();

	// метод возвращающий true, если параметр является целым
	bool IsInteger( const Parametr &prm ) const;

public:

	// задаем параметры
	ClassOperatorChecker( TempObjectContainer &_op, const TempOverloadOperatorContainer &t )
		: op(_op), tooc(t), redeclared(false) {
		Check();
	}

	// если переопределен
	bool IsRedeclared() const {
		return redeclared;
	}
};


// проверка оператора приведения типа
class CastOperatorChecker
{
	// временная структура в которой содержится декларатор и тип
	TempObjectContainer &cop;

	// временная структура, которая содержит информацию об операторе
	const TempCastOperatorContainer &tcoc;


	// метод проверки
	void Check();

	// если оператор не подлежит созданию, установлен в true
	bool incorrect;

public:
	
	// задаем параметры
	CastOperatorChecker( TempObjectContainer &op, const TempCastOperatorContainer &t )
		: cop(op), tcoc(t), incorrect(false)  {
		Check();
	}

	// вернуть true если оператор не подлежит созданию
	bool IsIncorrect() const {
		return incorrect;
	}
};


// проверка конструктора
class ConstructorChecker
{
	// временная структура в которой содержится декларатор и тип
	TempObjectContainer &ctor;

	// ссылка на класс в котором объявляется конструктор
	const ClassType &cls;

	// флаг устанавлвается в true, если вызывающему методу не нужно
	// вставляться конструктор в таблицу
	bool incorrect;

	// метод проверки
	void Check();

public:

	// инициализация временной структуры
	ConstructorChecker( TempObjectContainer &c, const ClassType &cl );
		
	// следует ли вставлять конструктор в таблицу
	bool IsIncorrect() const {
		return incorrect;
	}
};


// проверяет корректность глобального перегруженного оператора
class GlobalOperatorChecker
{
	// временная структура в которой содержится декларатор и тип
	TempObjectContainer &op;

	// временная структура в которой содержится информация об операторе
	const TempOverloadOperatorContainer &tooc;

	// функция проверки
	void Check();

	// метод возвращающий true, если параметр является целым
	bool IsInteger( const Parametr &prm ) const;

	// проверить, чтобы параметр был классом, ссылкой на класс, перечислением,
	// ссылкой на перечисление
	bool IsCompoundType( const Parametr &prm ) const;

public:

	// сохраним временную структуру
	GlobalOperatorChecker( TempObjectContainer &_op, const TempOverloadOperatorContainer &tc ) 
		: op(_op), tooc(tc) {

		Check();
	}
};


// проверка доступа к членам
class AccessControlChecker
{
	// область видимости для которой следует проверять возможность доступа
	const SymbolTable &curST;

	// класс через который производится доступ к члену
	const ClassType &memberCls;

	// сам член класс, который проверяется на доступность
	const ClassMember &member;	

	// устанавливается в true, если член доступен
	bool accessible;

	// закрытая функция, которая выполняет основную работу класса
	void Check();

public:

	// в конструкторе принимается текущая область видимости, на основе которой проверяется
	// доступ, класс, через который производится доступ к члену, и сам член
	AccessControlChecker( const SymbolTable &ct, const ClassType &mcls, const ClassMember &cm )
		: curST(ct), memberCls(mcls), member(cm), accessible(false) {

		Check();
	}

	// возвращает true, если член доступен
	bool IsAccessible() const {
		return accessible;
	}

private:

	// вспомагательная структура, описывающая действительный 
	// спецификатор доступа члена
	struct RealAccessSpecifier
	{
		// спецификатор может принимать все 4 значения предопределенные
		// типом AS. В последнем случае, когда значение NOT_CLASS_MEMBER,
		// означает, что к члену нет доступа (т.к. он находится
		// в закрытом базовом классе)
		ClassMember::AS realAs;

		// сам член
		const ClassMember *pMember;

		// класс к которому принадлежит член,
		// задается как только класс будет выявлен в следствии поиска
		const ClassType *pClass;

		// флаг устанавливается, если класс выявлен в следствии
		// анализа иерархии
		bool isClassFound;

		// в конструкторе задаются начальные значения
		RealAccessSpecifier( ClassMember::AS as, const ClassMember *p )
			: realAs(as), pMember(p), isClassFound(false) {

			pClass = static_cast<const ClassType *>(
				&dynamic_cast<const Identifier *>(pMember)->GetSymbolTableEntry());
		}
	};
	

	// рекурсивная функция, проходит по всему дереву базовых классов и 
	// изменяет спецификатор доступа члена в зависимости от спецификатора
	// наследования по правилу:  public-(public, protected, no_access), 
	// protected-(protected, protected, no_access), private-(no_access, no_access, no_access).
	// Функция анализирует всю иерархию так как возможна ситуация когда член
	// достижим по нескольким путям, в этом случае выбирается наиболее доступный член
	void AnalyzeClassHierarhy( RealAccessSpecifier &ras, 
		ClassMember::AS curAS, const ClassType &curCls, int level );

	
	// функция возвращает true, если d является производным классом b
	bool DerivedFrom( const ClassType &d, const ClassType &b );
};


// выявляет, является ли метод виртуальным и если является, то совпадают
// ли типы возвращаемых значений виртуальных методов
class VirtualMethodChecker
{
	// объявляемый метод, на основании которого выполняются проверки
	Method &method;

	// роль: может быть метод, перегруженный оператор, оператор 
	// оператор приведения или деструткор
	Role destRole;

	// позиция, в которую следует выводить ошибку в случае ее возникновения
	const Position &errPos;

	// тип - список виртуальных методов
	typedef list<const Method *> VML;

	// список виртуальных функций из базовых классов
	VML vml;

	// рекурсивный метод, выполняет наполнение списка, методами,
	// которые совпадают по сигнатуре с имеющимся методом
	void FillVML( const ClassType &curCls );

	// если сигнатуры у метода 'vm', такая же как у 'method',
	// вернуть true. При этом 'vm' должен быть виртуальным
	bool EqualSignature( const Method *vm );

	// закрытая ф-ция выполняет проверку
	void Check();

	// выполнить проверку деструткоров ближайших
	// базовых классов
	void CheckDestructor( const ClassType &curCls );

	// класс функтор для поиска метода в списке виртуальных функций
	class VMFunctor {
		// имя метода
		string mname;
	public:
		// задать имя метода
		VMFunctor( const string &mn )
			: mname(mn) {
		}

		// сравнить метод и имя
		bool operator()( const Method *m ) const {
			return mname == m->GetName().c_str();
		}
	};

public:

	// в конструкторе задаются параметры и сразу идет проверка.
	// Если будет выявлено, что функция виртуальная, ей будет задана
	// виртуальность. Если будет выявлено, что метод перекрывает
	// чисто-виртуальный метод из базового класса, будет уменьшен
	// счетчик абстрактных методов класса.
	VirtualMethodChecker( Method &dm, const Position &ep )
		: method(dm), errPos(ep), destRole(R_UNCKNOWN) {
				
		// выполнить проверку, и если требуется задать соотв. флаги
		Check();

		// если метод виртуален и у него нет соотв. в базовых классах,
		// создать виртуальную таблицу, если она не создана и задать 
		// виртуальность методу
		if( method.IsVirtual())
		{
			ClassType &mcls = ((ClassType &)method.GetSymbolTableEntry());
			if( vml.empty() )
			{				
				if( mcls.GetVirtualFunctionCount() == 0 )
					mcls.MakeVmTable();
				method.SetVirtual(NULL);
			}

			mcls.AddVirtualFunction(method);
		}

		// последняя проверка - если после выявления виртуального метода
		// метод является абстрактным и не является виртуальным - вывести
		// ошибку
		if( method.IsAbstract() && !method.IsVirtual() )
			theApp.Error( errPos, 
				"'%s' - чисто-виртуальный метод должен объявляться со спецификатором 'virtual'",
				method.GetName().c_str());
	}
};

