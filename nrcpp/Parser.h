// интерфейс к синтаксическому анализатору - Parser.h


#ifndef _PARSER_H_INCLUDE
#define _PARSER_H_INCLUDE

// подключаем коды пакетов
#include "PackCode.h"


// объявлены далее
class Package;

// объявлены далее
class NodePackage;

// объявлен в ExpressionMaker.h
class CtorInitListValidator;

// объявлен в Body.h
class Instruction;

// объявлен в BodyMaker.h
class ConstructionController;

// объявлен в Body.h
class ForConstruction;

// объявлен в Body.h
class BodyComponent;

// объявлен в Body.h
class CompoundConstruction;

// объявлен в Body.h
typedef list< SmartPtr<Instruction> > InstructionList;


// вспомогательные функции, относящиеся к интерфейсу синтаксического анализатора
namespace ParserUtils
{
	// вывести синтаксическую ошибку обнаруженную рядом с лексемой 'lxm'
	void SyntaxError( const Lexem &lxm );


	// функция считывает лексемы до тех пор пока не появится ';'
	void IgnoreStreamWhileNotDelim( LexicalAnalyzer &la );

	// проверяет, если входной пакет и лекс. находится в состоянии
	// декларации типа (класс, перечисление), считать тип, выполнить
	// декларацию вернуть указатель на сформированный тип, иначе NULL
	bool LocalTypeDeclaration( LexicalAnalyzer &la, NodePackage *pkg, const BaseType *&outbt );

	// получить позицию пакета
	Position GetPackagePosition( const Package *pkg );

	// распечатать все дерево пакетов, вернуть буфер
	CharString PrintPackageTree( const NodePackage  *pkg );
}


// интерфейс для классов пакетов. 
// пакетирование - это иерархическое упорядочивание лексем в списке 
// для дальнейшего анализа
class Package
{
public:
	// виртуальный деструктор, используется при удалении объектов
	virtual ~Package() {
	}

	// лексемный пакет
	virtual bool IsLexemPackage() const {
		return false;
	}

	// узловой пакет
	virtual bool IsNodePackage() const {
		return false;
	}

	// пакет-выражение
	virtual bool IsExpressionPackage() const {
		return false;
	}

	// получить идентификатор пакета
	virtual int GetPackageID() const = 0;	

	// вывести отладочную информацию
	virtual void Trace() const = 0;
};


// интеллектуальный указатель на пакет
typedef SmartPtr<Package> PPackage;

// лексемный пакет
class LexemPackage : public Package
{
	// лексема
	Lexem lexem;

public:

	// задаем идентификатор лексемы и саму лексему 
	LexemPackage( const Lexem &lxm ) : lexem(lxm) {
	}

	// деструктор вызываемый из Package при удалении
	~LexemPackage() {
	}

	// лексемный пакет
	virtual bool IsLexemPackage() const {
		return true;
	}

	// получить лексему, связанную с пакетом
	const Lexem &GetLexem() const {
		return lexem;
	}

	// получить идентификатор пакета
	int GetPackageID() const {
		return lexem.GetCode();
	}

	// вывести отладочную информацию
	void Trace() const {
		cout << lexem.GetBuf() << "  ";
	}
};


// класс узлового пакета. Узловой пакет как правило содержит дочерний пакет,
// а также обозначается уникальным идентификатором, который не может быть
// лексемой
class NodePackage : public Package
{
	// идентификатор узлового пакета. Должен быть кодом
	// узла имеющего потомков, такой как параметр или составное имя
	int packageID;

	// подсписок детей пакета
	vector<Package *> childPackageList;

	// освободить память занятую дочерними пакетами
	void ClearChildPackageList() {
		for( vector<Package *>::iterator p = childPackageList.begin();
			 p != childPackageList.end(); p++ )
			 delete *p, *p = NULL;	

		childPackageList.clear();
	}

	// дружественная функция создания указателя на член. Ей требуется удалить
	// последних два пакета, для нахождения класса к которому принадлежит
	// указатель
	friend inline PointerToMember *MakePointerToMember( NodePackage & );

public:
	// конструктор с заданием кода пакета 
	NodePackage( int pid ) : packageID(pid) {		
	}

	// деструктор освобождает память занятую детьми
	~NodePackage() {
		ClearChildPackageList();
	}

	// узловой пакет
	virtual bool IsNodePackage() const {
		return true;
	}
	
	// если нет дочерних пакетов
	bool IsNoChildPackages() const {
		return childPackageList.empty();
	}

	// если дочерние пакеты ошибочны
	bool IsErrorChildPackages() const {
		if( IsNoChildPackages() ) 
			return false;
		return GetChildPackage(0)->GetPackageID() == PC_ERROR_CHILD_PACKAGE;
	}

	// задать новый идентификатор пакета, в случае если 
	// обнаруживается, что пакет имеет другое семантическое значение
	void SetPackageID( int pid ) {
		packageID = pid;
	}

	// получить идентификатор пакета
	int GetPackageID() const {
		return packageID;
	}

	// получить дочерний пакет
	const Package *GetChildPackage( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > childPackageList.size()-1 );
		return childPackageList[ix];
	}

	// получить последний дочерний пакет
	const Package *GetLastChildPackage( ) const {
		INTERNAL_IF( childPackageList.empty() );
		return childPackageList.back();
	}

	// получить количество дочерних пакетов
	int GetChildPackageCount() const {
		return childPackageList.size();
	}

	// присоединить дочерний пакет. Добавление происходит 
	// исключительно в конец списка
	void AddChildPackage( Package *pkg ) {
		
		// добавление дочернего пакета в пакет с ошибкой конструирования
		// невозможно
		if( !childPackageList.empty() &&
			childPackageList.front()->GetPackageID() == PC_ERROR_CHILD_PACKAGE )
			return;
		
		childPackageList.push_back(pkg);
	}

	// добавить пакет в начало
	void AddChildPackageToFront( Package *pkg ) {
		if( !childPackageList.empty() &&
			childPackageList.front()->GetPackageID() == PC_ERROR_CHILD_PACKAGE )
			return;
		
		childPackageList.insert(childPackageList.begin(), pkg);
	}
	
	// поиск пакета с загловком pid, возвращает его индекс в случае удачного
	// поиска и -1 в случае неудачи.
	// spos - позиция с которой следует начинать поиск
	int FindPackage( int pid, int spos = 0 ) const {
		
		for( int i = spos; i< childPackageList.size(); i++ )
			if( childPackageList[i]->GetPackageID() == pid )
				return i;
		return -1;
	}
	

	// ошибка конструирования пакета, ошибки возникают в следствии внешних,
	// обстоятельств. Для инспектирования пакетов с такими ошибками выполняются
	// след. действия: удаляются все дочерние пакет, на их место создается
	// один с заголовком PC_ERROR_CHILD_PACKAGE 
	void BuildError() {		
		ClearChildPackageList();		// освобождаем память занятую детьми
		AddChildPackage( new NodePackage(PC_ERROR_CHILD_PACKAGE) );
	}


	// печать отладочной информации о пакете
	void Trace() const {
		cout << "\"" << GetChildPackageCount() << ", " << hex << packageID << "\""
			 << endl;
		
		if( GetChildPackageCount() == 0 )
			return;

		cout << "---------------------\n";
		for( vector<Package *>::const_iterator p = childPackageList.begin();
			 p != childPackageList.end(); p++ )
				(*p)->Trace();

		cout << "\n-------------------\n";
	}
};


// класс контролирующий переполнение стека, может использоваться
// для контроля только за одной конструкцией
class OverflowController
{
	// счетчик размера стека
	static int counter;

public:
	// в конструкторе счетчик увеличивается и если становится
	// равным барьеру - фатальная ошибка
	OverflowController( int barrier ) {
		if( counter >= barrier )
			theApp.Fatal("переполнение стека парсера");
		counter++;
	}

	// деструктор уменьшает счетчик
	~OverflowController() {
		counter--;
	}
};


// класс управляющий работой синтаксического анализатора
class Parser
{
public:
	// спецификации связывания
	enum LS { LS_CPP, LS_C };

private:
	// текущая спецификация связывания
	LS linkSpec;

	// лексический анализатор для парсера, 
	// задается извне
	LexicalAnalyzer	&lexicalAnalyzer;

	// вектор содержит контроллеры областей видимости. Если
	// элемент вектора установлен в 0, значит текущая фигурная
	// скобка принадлежит namespace, если 1, значит extern "C",
	// иначе extern "C++"
	vector<int> crampControl;

public:

	// конструктор с заданием лексического анализатора
	Parser( LexicalAnalyzer &la ) : lexicalAnalyzer(la), linkSpec(LS_CPP) {		
	}

	// метод запуска синтаксического анализатора
	void Run();

	// получить спецификацию линковки
	LS GetLinkSpecification() const {
		return linkSpec;
	}
	
	// задать текущую спецификацию линковки 
	void SetLinkSpecification( LS ls ) {
		linkSpec = ls;
		crampControl.push_back( linkSpec == LS_C ? 1 : 2 );
	}
};


// тип - интеллекутальный указатель на пакет
typedef SmartPtr<NodePackage> PNodePackage;


// класс представляющий собой компонент синтаксического анализатора,
// который используется при разборе простых деклараций не имеющих
// блоков (класс или функция)
class DeclareParserImpl
{
	// лексический анализатор для парсера, 
	// задается извне
	LexicalAnalyzer	&lexicalAnalyzer;

public:
	
	// конструктор с заданием лексического анализатора
	DeclareParserImpl( LexicalAnalyzer &la ) : lexicalAnalyzer(la) {		
	}

	// метод разбора деклараций, возвращает пакет, по заголовку которго 
	// можно определить что делать дальше: создавать парсер класс, 
	// создавать парсер функции, либо прекратить, т.к. декларация проанализирована. 
	// В случае если прекратить, интеллектуальный указатель сам освободит память
	void Parse();
};


// указатель на контейнер
typedef SmartPtr<LexemContainer> PLexemContainer;


// класс, также как и Parser является управляющим,
// сам по себе он разбирает только спецификаторы доступа, а также
// сохраняет inline-функции в контейнерах. Основная задача класса,
// управлять реализациями, для разбора членов класса
class ClassParserImpl
{
	// тип - пара из указателя на функцию и контейнера
	typedef pair<Function *, PLexemContainer> FnContainerPair;

	// лекс
	LexicalAnalyzer &lexicalAnalyzer;

	// список контейнеров тел inline-функций
	list<FnContainerPair> methodBodyList;

	// пакет, последние лексемы которого представляют собой имя класса и 
	// ключ класса, либо просто ключ класса
	NodePackage &typePkg;

	// текущий спецификатор доступа
	ClassMember::AS  curAccessSpec;

	// указатель на класс, который мы формируем
	ClassType *clsType;

	// список квалификаторов области видимости класса, если
	// класс был объявлен в другой области видимости, а определяется 
	// в текущей. Список может быть пустым
	SymbolTableList qualifierList;

	// установлен в true, если формируем объединение
	bool isUnion;

	// устанавливается в true, если была ошибка при конструировании класса
	bool wasRead;

public:
	// в конструктор получаем пакет со списком типов, последним
	// в котором должен быть класс, а также лексический анализатор
	// третим параметром задается спецификатор доступа, если класс объявляется
	// как член другого класса
	ClassParserImpl( LexicalAnalyzer &la, NodePackage *np, ClassMember::AS as );

	// произвести разбор членов класса
	void Parse();

	// вернуть true, если класс считан и построен
	bool IsBuilt() const {
		return clsType != NULL;
	}

	// вернуть построенный класс
	const ClassType &GetClassType() const {
		INTERNAL_IF( clsType == NULL );
		return *clsType;
	}

private:

	// считать список базовых типов и сохранить их в классе,
	// также проверить их корректность
	void ReadBaseClassList();	

	// разобрать член
	void ParseMember( );

	// сгенерировать специальные функции члены, которые не заданы
	// явно пользователем
	void GenerateSMF();

	// по завершении определения класса, разбираем inline-функции, которые
	// находятся в контейнере
	void LoadInlineFunctions();

	// загружаем дружественные функции, считанные в процессе постройки класса
	void LoadFriendFunctions();
};


// парсер перечисления, считывает все константы до '}', сохраняет
// их в таблице, вставляет само перечисление
class EnumParserImpl
{
	// анализатор
	LexicalAnalyzer &lexicalAnalyzer;

	// пакет, последние лексемы которого представляют собой имя перечисления	
	NodePackage &typePkg;

	// текущий спецификатор доступа
	ClassMember::AS  curAccessSpec;

	// указатель на класс, который мы формируем
	EnumType *enumType;

	// позиция для вывода ошибок
	Position errPos;

public:
	// анализатор, пакет с перечислением, спецификатор доступа если находимся внутри класса
	EnumParserImpl( LexicalAnalyzer &la, NodePackage *np, ClassMember::AS as )
		: lexicalAnalyzer(la), typePkg(*np), curAccessSpec(as), enumType(NULL) {
		errPos = ParserUtils::GetPackagePosition(np);
	}

	// выполнить основную работу парсера
	void Parse();

	// вернуть построенное перечисление
	const EnumType &GetEnumType() const {
		INTERNAL_IF( enumType == NULL );
		return *enumType;
	}
};


// управляющий класс, также как Parser и ClassParserImpl
// Управляет созданием реализаций для разбора инструкций возможных в
// теле функции.
class FunctionParserImpl
{
	// лекс
	LexicalAnalyzer &lexicalAnalyzer;

	// тело функции, которое строится в процессе разбора
	FunctionBody *fnBody;

	// считать список инициализации конструктора и выполнить необх.
	// проверки
	void ReadContructorInitList( CtorInitListValidator &civl );

public:

	// конструктор принимает функцию и лексический анализатор и 
	// список областей видимости для восстановления. Последий параметр может
	// быть равен 0
	FunctionParserImpl( LexicalAnalyzer &la, Function &fn );

	// достаем функциональную область видимости
	~FunctionParserImpl() {
		// снимаем области видимости пока локальные, это может потребоваться 
		// в следствии ошибок
		while( GetCurrentSymbolTable().IsLocalSymbolTable() )
			GetScopeSystem().DestroySymbolTable();

		// последняя должна быть функциональная
		INTERNAL_IF( !GetCurrentSymbolTable().IsFunctionSymbolTable() );
		GetScopeSystem().DestroySymbolTable();
	}

	// разбор тела функции
	void Parse();
};


// разбор тела функции
class StatementParserImpl
{
	// лекс
	LexicalAnalyzer &la;

	// контроллер конструкций
	ConstructionController &controller;

	// тело функции
	FunctionBody &fnBody;


	// считать блок
	CompoundConstruction *ParseBlock( );

	// считать компонент функции, вернуть его
	BodyComponent *ParseComponent( );

	// считать и создать using конструкцию
	void ReadUsingStatement( );

	// считать for конструкцию
	ForConstruction *ReadForConstruction( ConstructionController &cntCtrl );

	// считать catch-декларацию
	PTypyziedEntity ReadCatchDeclaration( );

	// считать выражение для конструкций, вернуть результат
	SmartPtr<Operand> ReadExpression( bool noComa = false );
	
	// считать условие. Инструкцией является декларация с инициализацией
	// или выражение. Используется в if, switch, while, for
	SmartPtr<Instruction> ReadCondition(  );

	// считать список catch-обработчиков
	void ReadCatchList();

	// создать для конструкций область видмости
	void MakeLST() {
		GetScopeSystem().MakeNewSymbolTable( new LocalSymbolTable(GetCurrentSymbolTable()) );
	}


	// контролирует переполеннеие стека
	class OverflowStackController {
		// глубина вложенности
		static int deep;
	public:
		// увеличиваем счетчик
		OverflowStackController( LexicalAnalyzer &la ) {
			deep ++;
			if( deep == MAX_CONSTRUCTION_DEEP )
				theApp.Fatal(la.LastLexem().GetPos(), 
					"слишком вложенная конструкция; переполнение стека");
		}

		// уменьшаем
		~OverflowStackController() {
			deep--;
		}
	};

public:
	// задаем необх. информацию
	StatementParserImpl( LexicalAnalyzer &la, 
		ConstructionController &cntrl, FunctionBody &fnBody ) 

		: la(la), controller(cntrl), fnBody(fnBody) {
	}

	// считать блок или try-блок, в зависимости от корневой конструкции
	void Parse();
};


// лексема метки, если была считана метка в TypeExpressionReader,
// генерируется исключительная ситуация с этим типом
class LabelLexem : public Lexem 
{
public:
	LabelLexem( const Lexem &lxm ) : Lexem(lxm) {
	}
};


// разбор локальных локальных деклараций
class InstructionParserImpl
{
	// лексический анализатор для парсера, 
	// задается извне
	LexicalAnalyzer	&lexicalAnalyzer;

	// список построенных инструкций. Либо одно выражение,
	// либо список деклараций, в случае ошибки пустой
	InstructionList &insList;

	// флаг указывает на то, что считывание происходит внутри блока
	bool inBlock;

public:
	
	// конструктор с заданием лексического анализатора. Задаем также
	// входной список инструкций, в который добавляются инструкции
	InstructionParserImpl( LexicalAnalyzer &la, InstructionList &il, bool inBlock = false ) 
		: lexicalAnalyzer(la), insList(il), inBlock(inBlock) {		
	}

	// метод разбора инструкций
	void Parse();

	// получить список конструкций
	const InstructionList &GetInstructionList() const {
		return insList;
	}
};


#endif // end  _PARSER_H_INCLUDE
