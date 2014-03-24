// интерфейс транслятора в C-код - Translator.h


#ifndef _TRANSLATOR_H_INCLUDE
#define _TRANSLATOR_H_INCLUDE


// утилиты транслятора
namespace TranslatorUtils
{
	// генерирует имя области видимости без добавочных символов. Учитывает
	// след. области видимости: глобальная, именованная, классовая, функциональная (локальная)
	const string &GenerateScopeName( const SymbolTable &scope );

	// сгенерировать имя для безимянного идентификатора
	string GenerateUnnamed( );

	// вернуть строковый эквивалент оператора
	PCSTR GenerateOperatorName( int op );
	
	// транслировать декларацию, только если она не typedef и 
	// режим приложения не диагностичный
	void TranslateDeclaration( const TypyziedEntity &declarator, 
		const PObjectInitializator &iator, bool global );

	// транслировать декларацию класса
	void TranslateClass( const ClassType &cls );

	// сгенерировать заголовок класса
	inline string GenerateClassHeader( const ClassType &cls ) {
		return (cls.GetBaseTypeCode() == BaseType::BT_UNION ? "union " : "struct ") +
			cls.GetC_Name();
	}
}


// класс для генерации специальных функций членов, в
// случае их отсутствия
class SMFGenegator
{
	// класс для которого выполняется обработка
	ClassType &pClass;

	// список зависимых классов. Данные классы являются базовыми типами
	// нестатических данных-членов нашего класса, а также являются базовыми
	// классами нашего класса. В списке не может быть двух одинаковых классов
	typedef list<const ClassType *> ClassTypeList;
	ClassTypeList dependClsList;

	// менеджер специальных функций членов, находит специальные
	// функции члены для нашего класса
	typedef list<SMFManager> SMFManagerList;
	SMFManager smfManager;

	// флаг установлен в true, если класс имеет виртуальные базовые классы
	bool haveVBC;

	// установлен в true, если в классе есть члены, требующие явной инициализации,
	// это ссылки и константы. Если этот флаг установлен, конструкторы и оператор
	// копирования не генерируются
	bool explicitInit;

	// промежуточная структура, в которую записываются данные из
	// зависимых элементов. Т.е. если все элементы производных классов
	// тривиальные, то значение в этой структуре будет true для поля trivial,
	// так и для остальных полей
	struct DependInfo {
		bool trivial, paramIsConst;
		DependInfo() { trivial = paramIsConst = true; }
	};

	// добавить класс в список зависимых классов, 
	// только если он там не находится
	void AddDependClass( const ClassType *cls ) {
		if( find(dependClsList.begin(), dependClsList.end(), cls) == dependClsList.end() )
			dependClsList.push_back(cls);
	}

	// заполнить список зависимых классов, заполняется в методе Generate
	void FillDependClassList();

	// возвращает true, если член имеет классовый тип
	bool NeedConstructor( const DataMember &dm );

	// метод проходит по списку зависимых классов и проверяет,
	// является ли доступным, однозначным и присутствует ли вообще
	// специальный метод. Специальный метод получаем через ук-ль на функцию
	// член
	bool CanGenerate( const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const;

	// помимо стандартных проверок, проверяет чтобы в проверяемом классе не было
	// константых данных членов и ссылок
	bool CanGenerateCopyOperator( const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const;

	// возвращает true, если метод может быть сгенерирован как тривиальный.
	// Вывод деляется на основе списка зависимых классов и полиморфности собственного
	DependInfo GetDependInfo( const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const ;

	// вернуть true, если метод требуется объявить виртуальным
	bool IsDeclareVirtual(const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const ;

	// метод возвращает список производных типов с функией без параметров
	const DerivedTypeList &MakeDTL0() const;

	// метод возвращает список производных типов с функией у которой
	// в параметре константный или не константая ссылка на этот класс
	const DerivedTypeList &MakeDTL1( bool isConst ) const;

	// построить конструктор по умолчанию
	ConstructorMethod *MakeDefCtor( bool trivial ) const;

	// построить конструктор копирования
	ConstructorMethod *MakeCopyCtor( bool trivial, bool isConst ) const;

	// построить деструктор
	Method *MakeDtor( bool trivial, bool isVirtual ) const;

	// построить оператор присваивания
	ClassOverloadOperator *MakeCopyOperator( Method::DT dt, bool isConst, bool isVirtual ) const;

	// выводит отладочную информацию по методу
	void DebugMethod( const Method &meth );

public:
	// задаем класс
	SMFGenegator( ClassType &pcls )
		: pClass(pcls), smfManager(pClass), haveVBC(false), explicitInit(false) {
	}

	// метод генерирует для класса специальные функции члены, если
	// это необходимо
	void Generate() ;
};


// печать С-типа на основании С++-типа. Если задано имя, значит распечатываем
// с именем. const-квалификаторы, спецификаторы хранения, спецификаторы функций игнорируются
class CTypePrinter
{
	// С++-тип
	const TypyziedEntity &type;

	// выходной буфер
	string outBuffer;

	// распечататнный базовый тип
	string baseType;

	// идентификатор, если задан, значит печатаем его имя
	const Identifier *id;

	// буфер заполняется в случае когда требуется печать 'this'
	string thisBuf;

	// флаг устанавливается, если идентификатор является функцией
	// печатать префиксные производные типы и сохранять их в буфер
	void PrintPointer( string &buf, int &ix, bool &namePrint );

	// печатать постфиксные производные типы и сохранять в буфер
	void PrintPostfix( string &buf, int &ix );

	// вернуть буфер с this, для заданного класса
	void PrintThis( const ClassType &cls, string &buf, bool addThis = true ) {	
		buf = (cls.GetBaseTypeCode() == BaseType::BT_UNION ? "union " : "struct ")
			+ cls.GetC_Name() + (addThis ? " *this" : "*");
	}

public:
	// конструктор для типа, либо для обычного идентификатора не функции
	CTypePrinter( const TypyziedEntity &type, const Identifier *id = NULL )
		: type(type), id(id) {
		
	}

	// конструктор для функции или метода
	CTypePrinter( const TypyziedEntity &type, const Function &fn, bool printThis )
		: type(type), id(&fn) {

		// если нестатический метод, распечатаем this. Если метод виртуален,
		// распечатаем класс корневого метода
		if( printThis )
		{
			const ClassType *cls = &static_cast<const ClassType &>(fn.GetSymbolTableEntry());
			if( fn.IsClassMember() && static_cast<const Method &>(fn).IsVirtual() )
				cls = &static_cast<const Method &>(fn).GetVFData().GetRootVfClass();
			PrintThis( *cls, thisBuf );
		}
	}
	
	// конструктор для распечатки С-типа виртуального метода, без имени
	CTypePrinter( const TypyziedEntity &type, const Method &vmethod )
		: type(type), id(NULL) {
		INTERNAL_IF( !vmethod.IsVirtual() );
		PrintThis( static_cast<const ClassType &>(vmethod.GetSymbolTableEntry()),
			thisBuf, false );
	}

	// генерировать
	void Generate();

	// вернуть сгенерированный буфер
	const string &GetOutBuffer() const {
		return outBuffer;
	}
};


// сгенерировать определение класса. Если класс не полностью объявлен,
// сгенерировать только заголовок класса.
class ClassGenerator
{
	// основной буфер класса. Определение класса 
	string clsBuffer;

	// дополнительный буфер класса, содержит информацию генерируемую 
	// за пределами класса: методы, статические данные-члены
	string outSideBuffer;

	// ссылка на генерируемый класс
	const ClassType &pClass;

	// счетчик операторов приведения класса, для подавления конфликта имен.
	// Остальные перегруженные методы генерируются относительно количества их в списке
	int castOperatorCounter;


	// сгенерировать заголовок класса
	string GenerateHeader( const ClassType &cls );

	// сгенерировать базовые подобъекты (классы)
	void GenerateBaseClassList( const BaseClassList &bcl );

	// сгенерировать список данных-членов и методов
	void GenerateMemberList( const ClassMemberList &cml );

	// сгенерировать таблицу виртуальных функций для данного класса,
	// учитывая базовые классы
	void GenerateVTable( );

	// рекурсиный метод вызываемый из GenerateVTable
	void GenerateVTable( const ClassType &cls, string *fnArray, int fnArraySize );

public:
	// задать класс
	ClassGenerator( const ClassType &pClass )
		: pClass(pClass) {
		INTERNAL_IF( theApp.IsDiagnostic() );
	}

	// сгенерировать определение класса и зависимую информацию
	void Generate();

	// вернуть дополнительный буфер
	const string &GetOutSideBuffer() const {
		return outSideBuffer;
	}

	// вернуть буфер со сгенерированным классом
	const string &GetClassBuffer() const {
		return clsBuffer;
	}
};


// генерация глобальной декларации. Генерирует декларацию объекта, либо функции
class DeclarationGenerator
{
	// ссылка на декларатор
	const TypyziedEntity &declarator;

	// возможный инициализатор, может быть NULL
	const PObjectInitializator &iator;

	// если декларация является глобальной установлен в true
	bool global;

	// выходной буфер с декларацией
	string outBuffer;

	// вспомагательный буфер в который генерируется дополнительная
	// информация о косвенной инициализации декларатора. Косвенная
	// инициализация генерируется для классовых объектов с нетривиальным
	// конструктором, либо глобальных объектов с неинтерпретируемым
	// инициализатором
	string indirectInitBuf;

	// сгенерировать, инициализацию конструктором. Возвращает буфер с вызовом
	// конструктора. Не рассматривает тривиальные конструкторы
	void GenerateConstructorCall( const ::Object &obj,
		const ConstructorInitializator &ci, string &out );

	// сгенерировать выражение из инициализатора. В списке выражение должно быть одно.
	// Если выражение является интерпретируемым и может использоваться как прямой
	// инициализатор глобального объекта, вернуть true, иначе false
	bool GenerateExpressionInit(  const ::Object &obj,
		const ConstructorInitializator &ci, string &out );

	// сгенерировать инициализатор для объекта. Если инициализатор возможно
	// сгенерировать как прямой, генерируем. Иначе генерируем в буфер косвенной
	// инициализации
	void GenerateInitialization( const ::Object &obj );

public:
	// задать декларатор и инициализатор
	DeclarationGenerator( const TypyziedEntity &d, const PObjectInitializator &ir, bool global )
		: declarator(d), iator(ir), global(global) {
		INTERNAL_IF( !(declarator.IsObject() || declarator.IsFunction()) );
	}

	// сгенерировать декларацию
	void Generate();

	// вернуть буфер с декларацией
	const string &GetOutBuffer() const {
		return outBuffer;
	}

	// вернуть буфер с косвенной декларацией
	const string &GetIndirectInitBuf() const {
		return indirectInitBuf;
	}

};


// описание временного объекта
class TemporaryObject
{
	// C++ тип объекта
	TypyziedEntity type;

	// метка использования объекта
	bool isUsed;

	// имя объекта
	string name;

	// счетчик временных объектов
	static int temporaryCounter;

public:
	// задать тип, при создании объект используется по умолчанию
	TemporaryObject( const TypyziedEntity &type ) 
		: type(type), isUsed(true) {
		const static string t = "__temporary";
		name = t + CharString(temporaryCounter++).c_str();
	}

	// в деструкторе уменьшаем счетчик временных объектов
	~TemporaryObject() {
		temporaryCounter--;
	}

	// вернуть имя
	const string &GetName() const {
		return name;
	}

	// вернуть тип объекта
	const TypyziedEntity &GetType() const {
		return type;
	}

	// если объект используется, вернуть true
	bool IsUsed() const {
		return isUsed;
	}

	// пометить объект как неиспользуемый
	void SetUnUsed() {
		isUsed = false;
	}

	// пометить объект как используемый
	void SetUsed() {
		isUsed = true;
	}
};


// менеджер временных объектов
class TemporaryManager
{
	// тип - список временных объектов
	typedef list<TemporaryObject> TemporaryList;

	// список временных объектов
	TemporaryList temporaryList;

	// буфер генерации. Заполняется при создании нового объекта С-декларацией
	// и при уничтожении классового объекта с нетривиальным деструктором
	string genBuffer;

	// количество неиспользуемых объектов на данный момент
	int unUsed;

public:
	//
	TemporaryManager()
		: unUsed(0) {
	}

	// создать новый временный объект
	const TemporaryObject &CreateTemporaryObject( const TypyziedEntity &type );

	// освободить временный объект
	void FreeTemporaryObject( TemporaryObject &tobj );

	// выгрузить буфер генерации
	const string &FlushOutBuffer() {
		static string rbuf;
		rbuf = genBuffer;
		genBuffer = "";
		return rbuf;
	}
};


// генератор выражения
class ExpressionGenerator
{
	// выражение которое следует распечатать
	const POperand &exp;

	// указатель на класс к которому принадлежит this, если 
	// генерируем выражение внутри нестатического метода
	const ClassType *thisCls;

	// выходной буфер генерации
	string outBuffer;

	// буфер с декларациями и конструированием временных объектов,
	// если временные объекты не требуются, буфер пустой
	string temporaryBuf;

	// буфер с вызововами деструкторов временных объектов. Может быть
	// пустой, если временные объекты не требуют вызова конструктора, либо
	// их вообще нет
	string destroyBuf;

	// если выражение на одной линии (не требует дополнительных вызовов)
	// установлен в true
	bool oneLineExpression;

	// рекурсивный метод
	static bool PrintPathToBase( const ClassType &cur, const ClassType &base, string &out );

	// сгенерировать путь от текущего класса к базовому. Если классы совпадают,
	// ничего не генерировать. Если базовый класс отсутствует в иерархии - внутренняя ошибка
	static const string &PrintPathToBase( const ClassType &cur, const ClassType &base );

	// распечатать основной операнд. Флаг printThis указывает на автоматическую
	// подстановку указателя this для нестатических и не typedef данных членов
	const TypyziedEntity &PrintPrimaryOperand( 
		const PrimaryOperand &po, bool printThis, string &out );

	// координатор распечатки выражений, вызывает метод соответствующий оператору.
	// prvOp - код предыдущего оператора, если оператора не было -1
	string PrintExpression( const POperand &exp );

	// распечатать унарное выражение
	void PrintUnary( const UnaryExpression &ue,  string &out );

	// распечатать бинарное выражение
	void PrintBinary( const BinaryExpression &be, string &out );

	// распечатать тернарное выражение
	void PrintTernary( const TernaryExpression &te, string &out );

	// распечатать вызов функции
	void PrintFunctionCall( const FunctionCallExpression &fce, string &out );

	// распечатать new-операцию
	void PrintNewExpression( const NewExpression &ne, string &out );

public:
	// задаем выражение и thisCls, если требуется
	ExpressionGenerator( const POperand &exp, const ClassType *thisCls )
		: exp(exp), thisCls(thisCls), oneLineExpression(true) {
		INTERNAL_IF( theApp.IsDiagnostic() );
		INTERNAL_IF( exp.IsNull() || !(exp->IsExpressionOperand() || exp->IsPrimaryOperand()) );
	}

	// сгенерировать С-выражение
	void Generate();

	// если выражение не надо разбивать на несколько, в случае 
	// вызова конструктора, возвратит true
	bool IsOneLineExpression() const {
		return oneLineExpression;
	}
	
	// если требуется создание временных объектов, вернет true
	bool IsNeedTemporary() const {
		return !temporaryBuf.empty();
	}

	// если требуется вызов деструкторов
	bool IsNeedDestructorCall() const {
		return !destroyBuf.empty();
	}

	// вернуть сгенерированный буфер
	const string &GetOutBuffer() const {
		return outBuffer;
	}

	// вернуть буфер создания и конструирования временных объектов
	const string &GetTemporaryBuffer() const {
		return temporaryBuf;
	}

	// вернуть буфер вызова деструкторов для временных объектов
	const string &GetDestroyBuffer() const {
		return destroyBuf;
	}
};


#endif
