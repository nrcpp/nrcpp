// интерфейс для компонентов тела функции - Body.h


// объявлен в ExpressionMaker.h
class ObjectInitElement;

// тип - список элементов инициализации объекта
typedef list<ObjectInitElement> ObjectInitElementList;


// абстрактный класс, представляет собой интерфейс для компонентов выражения. 
// Само выражение, также является операндом и наследует этот класс. 
class Operand
{
public:

	// виртуальный деструктор для уничтожения
	virtual ~Operand() {
	}

	// является ли операнд типом
	virtual bool IsTypeOperand() const {
		return false;
	}

	// является ли операнд выражением (унарным, бинарным или тернарным)
	virtual bool IsExpressionOperand() const {
		return false;
	}

	// является ли операнд основным (идентификатор, литерал, true, false, this)
	virtual bool IsPrimaryOperand() const {
		return false;
	}

	// является ли операнд списком перегруженных функций
	virtual bool IsOverloadOperand() const {
		return false;
	}

	// является ли операнд ошибочным
	virtual bool IsErrorOperand() const {
		return false;
	}

	// вернуть тип выражения. Для OverloadOperand и IsErrorOperand,
	// вызывается внутренняя ошибка компилятора
	virtual const TypyziedEntity &GetType() const = 0;
};


// интеллектуальный указатель на операнд
typedef SmartPtr<Operand> POperand;


// класс-пакет. Используется для хранения выражений в виде пакета.
// требуется там, где существует неоднозначность между декларацией
// и выражением 
class ExpressionPackage : public Package
{
	// само выражение
	POperand expression;

public:

	// конструктор принимает выражение в параметре, код всегда PC_EXPRESSION
	ExpressionPackage( const POperand &exp ) 
		: expression(exp) {
	}

	// пакет-выражение
	bool IsExpressionPackage() const {
		return true;
	}

	// получить выражение
	const POperand &GetExpression() const {
		return expression;
	}

	// получить идентификатор пакета
	int GetPackageID() const {
		return PC_EXPRESSION;
	}

	// вывести отладочную информацию
	void Trace() const {
		cout << "<выражение>\n";
	}
};


// абстрактный класс, представляет собой интерфейс для конкретных выражений. 
class Expression : public Operand
{
	// количество операндов в выражении
	int operandCount;

	// код оператора учавствующего в выражении
	int operatorCode;

	// является ли выражение lvalue
	bool lvalue;

	// если выражение в скобках, задано в true
	bool inCramps;

	// защищаем конструктор, чтобы небыло возможности создать объект
	// Expression, не производным классам
protected:

	// в конструкторе задается информация о выражении
	Expression( int opCnt, int opCode, bool lv ) 
		: operandCount(opCnt), operatorCode(opCode), lvalue(lv), inCramps(false) {
	}

public:

	// является ли операнд выражением (унарным, бинарным или тернарным)
	bool IsExpressionOperand() const {
		return true;
	}

	// если выражение является унарным
	bool IsUnary() const {
		return operandCount == 1;
	}
	
	// если выражение является бинарным
	bool IsBinary() const {
		return operandCount == 2;
	}
	
	// если выражение является тернарным
	bool IsTernary() const {
		return operandCount == 3;
	}

	// если выражение является вызовом функции
	bool IsFunctionCall() const {
		return operatorCode == OC_FUNCTION;
	}

	// если выражение new или new[]
	bool IsNewExpression() const {
		return operatorCode == KWNEW || operatorCode == OC_NEW_ARRAY;
	}

	// явялется ли выражение lvalue
	bool IsLvalue() const {
		return lvalue;
	}

	// если выражение находится в скобках, вернуть true
	bool IsInCramps() const {
		return inCramps;
	}

	// получить код оператора
	int GetOperatorCode() const {
		return operatorCode;
	}

	// задать выражение как rvalue
	void SetRValue() {
		lvalue = false;
	}

	// задать, что выражение в скобках
	void SetCramps() {
		inCramps = true;
	}
};


// унарное выражение
class UnaryExpression : public Expression
{
	// указатель на операнд в унарном выражении
	POperand pOperand;

	// если true, то оператор - это постфиксный -кремент
	bool postfix;

	// результирующий тип выражения
	PTypyziedEntity pType;

public:

	// в конструкторе задаем данные о выражении
	UnaryExpression( int opCode, bool lv, bool pfix, const POperand &pop, 
					 const PTypyziedEntity &pt ) 
		: Expression(1, opCode, lv), postfix(pfix), pOperand(pop), pType(pt) {
	}


	// является ли выражение постфиксным. Выражения с постфиксными операторами 
	// ++, -- могут быть одновременно унарными и постфиксными
	bool IsPostfix() const {
		return postfix;
	}

	// вернуть операнд
	const POperand &GetOperand() const {
		return pOperand;
	}

	// получить тип выражения
	const TypyziedEntity &GetType() const {
		return *pType;
	}
};


// бинарное выражение
class BinaryExpression : public Expression
{
	// операнды бинарного выражения
	POperand pOperand1, pOperand2;

	// результирующий тип выражения
	PTypyziedEntity pType;

public:

	// в конструкторе задаем данные о выражении
	BinaryExpression ( int opCode, bool lv, 
		const POperand &pop1, const POperand &pop2, const PTypyziedEntity &pt ) 

		: Expression(2, opCode, lv), pOperand1(pop1), pOperand2(pop2), pType(pt) {
	}

	// вернуть тип операнда1
	const POperand &GetOperand1() const {
		return pOperand1;
	}

	// вернуть тип операнда2
	const POperand &GetOperand2() const {
		return pOperand2;
	}

	// получить тип выражения
	const TypyziedEntity &GetType() const {
		return *pType;
	}
};

// тернарное выражение
class TernaryExpression : public Expression
{
	// операнды тернарного выражения
	POperand pOperand1, pOperand2, pOperand3;

	// результирующий тип выражения
	PTypyziedEntity pType;

public:

	// в конструкторе задаем данные о выражении
	TernaryExpression ( int opCode, bool lv, const POperand &pop1, 
		const POperand &pop2, const POperand &pop3, const PTypyziedEntity &pt ) 
		: Expression(3, opCode, lv), pOperand1(pop1), pOperand2(pop2), pOperand3(pop3), pType(pt)
	{
	}

	// вернуть тип операнда1
	const POperand &GetOperand1() const {
		return pOperand1;
	}

	// вернуть тип операнда2
	const POperand &GetOperand2() const {
		return pOperand2;
	}

	// вернуть тип операнда3
	const POperand &GetOperand3() const {
		return pOperand3;
	}
	
	// получить тип выражения
	const TypyziedEntity &GetType() const {
		return *pType;
	}
};


// список выражений
typedef vector<POperand> ExpressionList;

// тип - интеллектуальный указатель на список выражений (операндов)
typedef SmartPtr< ExpressionList > PExpressionList;


// выражение вызов функции
class FunctionCallExpression : public Expression
{
	// указатель на функцию. Может быть PrimaryOperand, OverloadOperand,
	// TypeOperand. В послднем случае вызов функции равносилен приведению типа
	POperand pFunction;

	// список параметров функции, может быть пустым
	PExpressionList parametrList;

	// результирующий тип выражения
	PTypyziedEntity pType;

public:

	// задаем функцию и список параметров. Список может быть пустым. Функция
	// может быть любым операндом, в том числе перегруженным и типом
	FunctionCallExpression( bool lv, const POperand &pfn, const PExpressionList &pel,
							const PTypyziedEntity &pt ) 
		: Expression(-1, OC_FUNCTION, lv), pFunction(pfn), parametrList(pel), pType(pt) {
	}

	// вернуть указатель на функцию. К этому моменту функция должна быть
	// выявлена и представлять собой выражение или основной операнд
	const POperand &GetFunctionOperand() const {
		return pFunction;
	}

	// вернуть список параметров
	const PExpressionList &GetParametrList() const {
		return parametrList;
	}

	// получить тип выражения
	const TypyziedEntity &GetType() const {
		return *pType;
	}
};


// Выражения new или new[]. Используется при строительстве new-выражения
class NewExpression : public Expression
{
	// построенный вызов функции выделения памяти
	POperand newOperatorCall;

	// список инициализаторов, которые используются для инициализации
	// созданного объекта
	PExpressionList initializatorList;

	// результирующий тип
	PTypyziedEntity pType;

	// код оператора new или new[]
	int opCode;

public:

	// в конструкторе задаем необх. информацию для хранения
	NewExpression( int opc, const POperand &call, const PExpressionList &il,
				const PTypyziedEntity &pt ) 
		: Expression(-1, opc, false), newOperatorCall(call), initializatorList(il), pType(pt) {

		INTERNAL_IF( !(opc == KWNEW || opc == OC_NEW_ARRAY) );
	}

	// вернуть указатель на функцию. К этому моменту функция должна быть
	// выявлена и представлять собой выражение или основной операнд
	const POperand &GetNewOperatorCall() const {
		return newOperatorCall;
	}

	// вернуть список параметров
	const PExpressionList &GetInitializatorList() const {
		return initializatorList;
	}

	// получить тип выражения
	const TypyziedEntity &GetType() const {
		return *pType;
	}
};


// ошибочный операнд. Если в процессе проверки выражения возникла ошибка,
// все выражение преобразуется в ошибочный операнд. Существует только
// один экземпляр этого операнда
class ErrorOperand : public Operand
{
	// единственный экземпляр ошибочного операнда
	static POperand errorOperand;

	// закрытый конструктор предотвращает создание объекта
	// вне членов класса
	ErrorOperand() { }

	// виртуальный метод из Operand. Вызывает внутреннюю ошибку
	const TypyziedEntity &GetType() const {
		INTERNAL( "'ErrorOperand::GetType' - некорректный вызов" );
		return *(TypyziedEntity *)0;
	}

public:

	// является ли операнд ошибочным
	bool IsErrorOperand() const {
		return true;
	}

	// вернуть представление ошибочного операнда
	static const POperand &GetInstance() {
		if( errorOperand.IsNull() )
			errorOperand = new ErrorOperand;

		return errorOperand;
	}
};


// основной операнд
class PrimaryOperand : public Operand
{
	// указатель на типизированный операнд. Это может быть
	// объект, литерал, true, false, this. В случае если
	// имеем перегруженную функцию, создаем OverloadOperand,
	// если функция единственная создается PrimaryOperand
	const TypyziedEntity &pType;

	// может быть lvalue, если это идентификатор
	bool lvalue;
	
public:
	// задаем информацию об операнде
	PrimaryOperand( bool lv, const TypyziedEntity &pt )
		: lvalue(lv), pType(pt) {
	}

	// удаляет память, занимаемую типизированной сущностью,
	// только если она не является идентификатором. В этом случае
	// объектом владеет другой объект
	~PrimaryOperand() {
		if( dynamic_cast<const Identifier *>(&pType) == NULL ) 
			delete &pType;		
	}

	// является ли операнд основным (идентификатор, литерал, true, false, this)
	bool IsPrimaryOperand() const {
		return true;
	}

	// явялется ли выражение lvalue
	bool IsLvalue() const {
		return lvalue;
	}

	// получить тип операнда
	const TypyziedEntity &GetType() const {
		return pType;
	}

	// задать выражение как rvalue
	void SetRValue() {
		lvalue = false;
	}
};


// операнд-тип
class TypeOperand : public Operand
{
	// указатель на типизированный операнд
	const TypyziedEntity &pType;
	
public:
	// задаем информацию об операнде
	TypeOperand( const TypyziedEntity &pt )
		: pType(pt) {
	}

	// освобождаем память занятую типом. 
	// Тип всегда создается динамически
	~TypeOperand() {
		delete &pType;
	}

	// является ли операнд типом
	bool IsTypeOperand() const {
		return true;
	}

	// получить тип операнда
	const TypyziedEntity &GetType() const {
		return pType;
	}
};


// указатель на типовой операнд
typedef SmartPtr<TypeOperand> PTypeOperand;


// список перегруженных функций
typedef vector<const Function *> OverloadFunctionList;


// перегруженный операнд
class OverloadOperand : public Operand
{
	// список перегруженных функций
	OverloadFunctionList overloadList;
		
	// виртуальный метод из Operand. Вызывает внутреннюю ошибку
	const TypyziedEntity &GetType() const {
		INTERNAL( "'OverloadOperand::GetType' - некорректный вызов" );
		return *(TypyziedEntity *)0;
	}

	// изначально перегруженный оператор является lvalue. Но при
	// взятии адреса, он становится rvalue
	bool lvalue;

public:

	// получить список перегруженных функций
	OverloadOperand( const OverloadFunctionList &ovl )
		:   overloadList(ovl), lvalue(true)  {		
	}

	// если rvalue
	bool IsLvalue() const {
		return lvalue;
	}

	// является ли операнд списком перегруженных функций
	bool IsOverloadOperand() const {
		return true;
	}

	// вернуть список перегруженных функций
	const OverloadFunctionList &GetOverloadList() const {
		return overloadList;
	}

	
	// задать выражение как rvalue
	void SetRValue() {
		lvalue = false;
	}
};


// объявления компонентов инициализации списком
//
// базовый класс для компонентов инициализации. Компонентов
// всего два: выражение и список. Введен для того, чтобы обеспечить
// хранение подсписоков в списках инициализации. 
class InitComponent
{
	// если компонент является атомом, установлен в true
	bool isAtom;

public:
	// конструктор с задаем информации о компоненте
	InitComponent( bool isa ) 
		: isAtom(isa) {
	}

	// деструктор для уничтожения производных объектов
	virtual ~InitComponent() = 0;

	// вернуть позицию
	virtual const Position &GetPosition() const = 0;

	// если компонент атом
	bool IsAtom() const {
		return isAtom;
	}

	// если компонент список
	bool IsList() const {
		return !isAtom;
	}
};


// атомный компонент инициализации. Другими словами выражение.
class AtomInitComponent : public InitComponent
{
	// ссылка на выражение
	POperand pExpr;

	// позиция для вывода ошибок
	Position errPos;

public:
	// задаем выражение и позицию
	AtomInitComponent( const POperand &pe, const Position &ep ) 
		: pExpr(pe), errPos(ep), InitComponent(true) {
	}

	// перекрываем абстрактный метод
	~AtomInitComponent() {
	}

	// вернуть выражение
	const POperand &GetExpression() const {
		return pExpr;
	}

	// вернуть позицию
	const Position &GetPosition() const {
		return errPos;
	}
};


// список инициализации, содержит компоненты инициализации,
// выражения или подсписки. Может быть пустым.
class ListInitComponent : public InitComponent
{	
public:
	// тип списка
	typedef list<const InitComponent *> ICList;

private:
	// список компонентов инициализации
	ICList icList;

	// установлен в true, если список сформирован, и возможно только
	// его инспектирование
	bool isDone;

	// позиция начала списка	
	Position errPos;

public:
	// по умолчанию
	ListInitComponent(const Position &ep) 
		: errPos(ep), isDone(false), InitComponent(false) {
	}

	// освободить память занятую компонентами
	~ListInitComponent() {
		for( ICList::iterator p = icList.begin(); p != icList.end(); ++p )
			delete *p;
		icList.clear();
	}

	// получить список компонентов
	const ICList &GetICList() const {
		return icList;
	}

	// добавить компонент в список
	void AddInitComponent( const InitComponent *ic ) {
		// нельзя добавлять если список сформирован
		INTERNAL_IF( isDone );	
		icList.push_back(ic);
	}

	// завершить построение списка
	void Done() {
		isDone = true;
	}

	// вернуть позицию
	const Position &GetPosition() const {
		return errPos;
	}
};


// интеллектуальный указатель на список инициализации
typedef SmartPtr<ListInitComponent> PListInitComponent;


// абстрактный класс инициализатора объекта. Конкретизируется двумя классами,
// инициализация списком выражений (конструктором), инициализация агрегатным списком
class ObjectInitializator
{
public:
	// абстрактный класс
	virtual ~ObjectInitializator() = 0;
	
	// вернуть true, если метод вызывается из класса ConstructorInitializator
	virtual bool IsConstructorInitializator() const {
		return false;
	}

	// вернуть true, если метод вызывается из класса AgregatListInitializator
	virtual bool IsAgregatListInitializator() const {
		return false;
	}
};


// интеллектуальный указатель на инициализатор объекта
typedef SmartPtr<ObjectInitializator> PObjectInitializator;


// конкретизация инициализатора объекта. Инициализация конструктором,
// принимает список выражений, либо пустой список, но не NULL и ук-ль
// на конструктор, либо NULL, если тип не классовый
class ConstructorInitializator : public ObjectInitializator
{
	// список инициализаторов, может быть пустым, но не NULL
	PExpressionList expList;

	// указатель на конструктор, который вызывается при инициализации,
	// может быть NULL, если тип не классовый
	const ConstructorMethod *pCtor;

public:
	// задать список выражений и конструктор
	ConstructorInitializator( const PExpressionList &el, const ConstructorMethod *pc )
		: expList(el), pCtor(pc) {
		INTERNAL_IF( expList.IsNull() );
	}

	// вернуть список инициализаторов
	const PExpressionList &GetExpressionList() const {
		return expList;
	}

	// вернуть указатель на конструктор, который вызывается при инициализации объекта,
	// может быть NULL
	const ConstructorMethod *GetConstructor() const {
		return pCtor;
	}

	// вернуть true, если метод вызывается из класса ConstructorInitializator
	bool IsConstructorInitializator() const {
		return true;
	}
};


// конкретизация инициализатора объекта. Инициализация агрегатным списком
class AgregatListInitializator : public ObjectInitializator
{
	// указатель на список инициализации
	PListInitComponent initList;

public:
	// задать список инициализации
	AgregatListInitializator( const PListInitComponent &il )
		: initList(il) {
		INTERNAL_IF( initList.IsNull() );
	}

	// вернуть true, если метод вызывается из класса AgregatListInitializator
	bool IsAgregatListInitializator() const {
		return true;
	}
};


// далее объявляются компоненты тела функции
//
// базовый класс для всех компонентов функции
class BodyComponent
{
public:
	// коды абстракций компонентов функции
	enum BCC { BCC_INSTRUCTION, BCC_CONSTRUCTION, BCC_LABEL, BCC_ADDITIONAL_OPERATION };

private:
	// код компонента для идентификации
	BCC bodyComponentId;

	// позиция
	Position errPos;

public:
	// задать код и позицию
	BodyComponent( BCC bci, const Position &ep )
		: bodyComponentId(bci), errPos(ep) {
	}

	// абстрактный деструктор для абстрактного класса
	virtual ~BodyComponent() = 0;

	// вернуть позицию
	const Position &GetPosition() const {
		return errPos;
	}

	// вернуть код компонента
	BCC GetComponentID() const {
		return bodyComponentId;
	}
};


// указатель на компонент функции
typedef SmartPtr<BodyComponent> PBodyComponent;


// базовый класс для инструкций. Инструкцией является выражение,
// пустое выражение, декларация, декларация класса, блок деклараций
class Instruction : public BodyComponent
{
public:
	// коды инструкций
	enum IC { 
		IC_EXPRESSION, IC_EMPTY, IC_DECLARATION, 
		IC_CLASS_DECLARATION, IC_DECLARATION_BLOCK
	};

private:
	// код инструкции
	IC instructionId;

public:
	// задать код инструкции и позицию
	Instruction( IC ic, const Position &ep )
		: instructionId(ic), BodyComponent(BCC_INSTRUCTION, ep) {
	}

	// абстрактный класс
	virtual ~Instruction() = 0;

	// вернуть код инструкции
	IC GetInstructionID() const {
		return instructionId;
	}
};


// указатель на инструкцию
typedef SmartPtr<Instruction> PInstruction;


// базовый класс для конструкций. Конструкцией является компонент,
// который содержит в себе другие подкомпоненты
class Construction : public BodyComponent
{
public:
	// коды конструкций
	enum CC {  
		CC_IF, CC_ELSE, CC_WHILE, CC_FOR, CC_DOWHILE, 
		CC_SWITCH, CC_TRY, CC_CATCH, CC_COMPOUND 
	};

private:
	// код конструкции
	CC constructionId;

	// указатель на родительскую конструкцию
	const Construction *parentConstruction;

public:
	// задать код коннструкции, указатель на родительскую конструкцию и позицию
	Construction( CC cc, const Construction *pc, const Position &ep )
		: constructionId(cc), parentConstruction(pc), BodyComponent(BCC_CONSTRUCTION, ep) {
	}

	// вернуть код конструкции
	CC GetConstructionID() const {
		return constructionId;
	}

	// вернуть родительский узел, может возвращать NULL, если отсутствует
	const Construction *GetParentConstruction() const {
		return parentConstruction;
	}

	
	// добавить дочерний компонент к текущей конструкции. Следует учитывать,
	// что метод может вызываться только однажды для всех конструкций, кроме
	// CompoundConstruction
	virtual void AddChildComponent( const BodyComponent *childComponent ) = 0;
	
	// вернуть дочерний компонент, может возвращать NULL, если он еще не задан.
	// Если конструкция составная, вызов метода приведет к внутренней ошибке
	virtual const BodyComponent *GetChildComponent() const = 0;
};


// базовый класс для меток. Метка в корректно сформированной программе
// должна обязательно содержать в себе компонент
class LabelBodyComponent : public BodyComponent
{
public:
	// коды меток
	enum LBC { LBC_LABEL, LBC_CASE, LBC_DEFAULT };

private:
	// код метки
	LBC labelId;

	// указатель на следующий компонент, стоящий после метки
	const BodyComponent *nextComponent;

public:
	// задать код метки и позицию
	LabelBodyComponent( const BodyComponent &nc, LBC lbc, const Position &ep )
		: labelId(lbc), nextComponent(&nc), BodyComponent(BCC_LABEL, ep) {
	}

	// абстрактный класс
	virtual ~LabelBodyComponent() = 0;

	// вернуть код метки
	LBC GetLabelID() const {
		return labelId;
	}

	// вернуть следующий компонент стоящий после метки, в корректно
	// сформированной программе должен быть не NULL
	const BodyComponent &GetNextComponent() const {
		return *nextComponent;
	}
};


// дополнительные операции. Не содержат в себе подкомпонентов, поэтому
// не являются конструкциями
class AdditionalOperation : public BodyComponent
{
public:
	// коды дополнительных операций
	enum AOC { AOC_RETURN, AOC_CONTINUE, AOC_BREAK, AOC_ASM, AOC_GOTO };

private:
	// код дополнительной операции
	AOC aoc;

public:
	// задать код операции и позицию
	AdditionalOperation( AOC aoc_, const Position &ep )
		: aoc(aoc_), BodyComponent(BCC_ADDITIONAL_OPERATION, ep) {
	}

	// абстрактный класс
	virtual ~AdditionalOperation() = 0;

	// вернуть код операции
	AOC GetAOID() const {
		return aoc;
	}
};


// выражение является инструкцией
class ExpressionInstruction : public Instruction
{
	// операнд, содержит все сформированное выражение, не может быть NULL
	POperand pExpr;

public:
	// задать операнд и позицию
	ExpressionInstruction( const POperand &pe,  const Position &ep ) 
		: pExpr(pe), Instruction(IC_EXPRESSION, ep) {
		INTERNAL_IF( pExpr.IsNull() );
	}

	// вернуть выражение
	const POperand &GetExpression() const {
		return pExpr;
	}
};


// пустая инструкция, ';'
class EmptyInstruction : public Instruction
{
public:
	// задать позицию
	EmptyInstruction( const Position &ep ) 
		: Instruction(IC_EMPTY, ep) {		
	}
};


// декларация является инструкцией
class DeclarationInstruction : public Instruction
{
	// ссылка на объект или функцию, типизированная сущность
	// обязательно должна быть либо объектом либо функцией
	const TypyziedEntity &declarator;

	// инициализатор, может быть NULL, тогда подразумевается,
	// что объект инициализируется по умолчанию
	PObjectInitializator initializator;

public:
	// задать декларатор, инициализатор и позицию
	DeclarationInstruction( const TypyziedEntity &d, 
		const PObjectInitializator &iator, const Position &ep ) 
		: declarator(d), initializator(iator), Instruction(IC_DECLARATION, ep) {		

		// декларатор должен быть объектом или функцией
		INTERNAL_IF( !(d.IsObject() || d.IsFunction()) );
	}

	// если не имеем инициализатор, вернуть true
	bool IsNoInitializator() const {
		return initializator.IsNull();
	}

	// вернуть декларатор
	const TypyziedEntity &GetDeclarator() const {
		return declarator;
	}

	// вернуть инициализатор
	const PObjectInitializator &GetInitializator() const {
		return initializator;
	}
};


// декларация класса
class ClassDeclarationInstruction : public Instruction
{
	// объявляемый класс. Следует учесть, что класс может быть
	// не полностью объявленным
	const ClassType &pClass;

public:
	// задать класс и позицию
	ClassDeclarationInstruction( const ClassType &cls, const Position &ep )
		: pClass(cls), Instruction(IC_CLASS_DECLARATION, ep) {
	}
	
	// вернуть класс
	const ClassType &GetClass() const {
		return pClass;
	}
};


// блок деклараций. Может быть декларация класса и несколько деклараций объектов.
// В случае если декларация одна в тексте, испольузется DeclarationInstruction
class DeclarationBlockInstruction : public Instruction
{
	// список инструкций
	InstructionList declBlock;

public:
	// задаем список инструкций
	DeclarationBlockInstruction( const InstructionList &il, const Position &ep ) 
		: declBlock(il), Instruction(IC_DECLARATION_BLOCK, ep) {
	}

	// получить блок деклараций
	const InstructionList &GetDeclarationBlock() const {
		return declBlock;
	}
};


// обычная метка
class SimpleLabelBodyComponent : public LabelBodyComponent
{
	// ссылка на метку
	Label label;

public:
	// задаем метку и позицию
	SimpleLabelBodyComponent( const Label &lab, const BodyComponent &nc, const Position &ep )
		: label(lab), LabelBodyComponent(nc, LBC_LABEL, ep) {
	}

	// вернуть метку
	const Label &GetLabel() const {
		return label;
	}
};


// case-метка
class CaseLabelBodyComponent : public LabelBodyComponent
{
	// значение метки
	int caseValue;

public:
	// задаем значение и позицию
	CaseLabelBodyComponent( int cv, const BodyComponent &nc, const Position &ep )
		: caseValue(cv), LabelBodyComponent(nc, LBC_CASE, ep) {
	}

	// вернуть значение case-метки
	int GetCaseValue() const {
		return caseValue;
	}
};


// default-метка
class DefaultLabelBodyComponent : public LabelBodyComponent
{
public:
	// задаем позицию
	DefaultLabelBodyComponent( const BodyComponent &nc, const Position &ep )
		: LabelBodyComponent(nc, LBC_DEFAULT, ep) {
	}
};


// else конструкция
class ElseConstruction : public Construction
{
	// дочерний компонент конструкции else
	const BodyComponent *childElseComponent;

public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	ElseConstruction( const Construction *pc, const Position &ep )
		: childElseComponent(NULL), Construction(CC_ELSE, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childElseComponent != NULL );
		childElseComponent = childComponent ;
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childElseComponent;
	}
};


// if конструкция
class IfConstruction : public Construction
{
	// дочерний компонент конструкции if
	const BodyComponent *childIfComponent;

	// указатель на else конструкцию, может быть NULL
	const ElseConstruction *elseConstruction;

	// инструкция условия, либо выражение либо декларация
	PInstruction condition;

public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	IfConstruction( const PInstruction &cond, const Construction *pc, const Position &ep )
		: condition(cond), Construction(CC_IF, pc, ep), 
		childIfComponent(NULL), elseConstruction(NULL) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childIfComponent != NULL );
		childIfComponent = childComponent ;
	}

	// задать else-конструкцию
	void SetElseConstruction( const ElseConstruction *ec ) {
		INTERNAL_IF( elseConstruction != NULL );
		elseConstruction = ec;
	}

	// вернуть else
	const ElseConstruction *GetElseConstruction() const {
		return elseConstruction;
	}

	// вернуть дочерний компонент if
	const BodyComponent *GetChildComponent() const {
		return childIfComponent;
	}

	// вернуть условие
	const PInstruction &GetCondition() const {
		return condition;
	}
};


// while конструкция
class WhileConstruction : public Construction
{
	// дочерний компонент конструкции while
	const BodyComponent *childWhileComponent;

	// инструкция условия, либо выражение либо декларация
	PInstruction condition;

public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	WhileConstruction( const PInstruction &cond, const Construction *pc, const Position &ep )
		: condition(cond), childWhileComponent(NULL), Construction(CC_WHILE, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childWhileComponent != NULL );
		childWhileComponent = childComponent ;
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childWhileComponent;
	}

	// вернуть условие
	const PInstruction &GetCondition() const {
		return condition;
	}
};


// список инструкций
typedef list<PInstruction> InstructionList;


// секция инициализации for-конструкции
class ForInitSection
{
	// может быть список деклараций, либо одно выражение,
	// либо вообще пустой список
	InstructionList initList;

public:
	// по умолчанию
	ForInitSection( const InstructionList &il )
		: initList(il) {
	}

	// если секция пустая, вернуть true
	bool IsEmptySection() const {
		return initList.empty();
	}

	// вернуть список инструкций
	const InstructionList &GetInitList() const {
		return initList;
	}
};


// for конструкция
class ForConstruction : public Construction
{
	// дочерний компонент конструкции for
	const BodyComponent *childForComponent;

	// инструкции инициализации, может быть выражение, может
	// быть список деклараций, может вообще отсутствовать
	ForInitSection init;

	// инструкция условия, может отсутствовать  (NULL)
	PInstruction condition;

	// выражение итерации, может отсутствовать  (NULL)
	POperand iteration;

public:
	// задаем инструкцию инициализации, условия, итерации, родительскую конструкцию, позицию
	ForConstruction( const ForInitSection &in, const PInstruction &cond, const POperand &iter,
		const Construction *pc, const Position &ep )
		: init(in), condition(cond), iteration(iter),
		childForComponent(NULL), Construction(CC_FOR, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childForComponent != NULL );
		childForComponent = childComponent ;
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childForComponent;
	}

	// вернуть секцию инициализации
	const ForInitSection &GetInitSection() const {
		return init;
	}

	// вернуть инструкцию условия
	const PInstruction &GetCondition() const {
		return condition;
	}

	// вернуть выражение итерации
	const POperand &GetIteration() const {
		return iteration;
	}
};

// do-while конструкция
class DoWhileConstruction : public Construction
{
	// дочерний компонент конструкции do-while
	const BodyComponent *childDoWhileComponent;

	// инструкция условия
	POperand condition;

public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	DoWhileConstruction( const Construction *pc, const Position &ep )
		: condition(NULL), childDoWhileComponent(NULL), Construction(CC_DOWHILE, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childDoWhileComponent != NULL );
		childDoWhileComponent = childComponent ;
	}

	// задать инструкцию условия, после считывания while
	void SetCondition( const POperand &cond ) {
		INTERNAL_IF( !condition.IsNull() );
		condition = cond;
	}


	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childDoWhileComponent;
	}

	// вернуть условие
	const POperand &GetCondition() const {
		return condition;
	}
};


// тип списка меток
typedef list<const LabelBodyComponent *> LabelList;


// switch конструкция
class SwitchConstruction : public Construction
{
	// дочерний компонент конструкции while
	const BodyComponent *childSwitchComponent;

	// инструкция условия, либо выражение либо декларация
	PInstruction condition;	

private:
	// список меток, для контроля за корректностью. Метки могут
	// быть только case или default
	LabelList caseLabs;
	
public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	SwitchConstruction( const PInstruction &cond, const Construction *pc, const Position &ep )
		: condition(cond), childSwitchComponent(NULL), Construction(CC_SWITCH, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childSwitchComponent != NULL );
		childSwitchComponent = childComponent ;
	}

	// добавить case или defaul-метку в список меток
	void AddLabel( const LabelBodyComponent *lab ) {
		caseLabs.push_back(lab);
	}

	// вернуть список меток для проверки
	const LabelList &GetLabelList() const {
		return caseLabs;
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childSwitchComponent;
	}

	// вернуть условие
	const PInstruction &GetCondition() const {
		return condition;
	}
};


// catch-конструкция, может быть только частью try-catch конструкции
class CatchConstruction : public Construction
{
	// указатель на принимаемый тип, объект. Если NULL,
	// значит используется '...'
	PTypyziedEntity catchType;

	// указатель на дочернюю составную конструкцию, должен быть не NULL
	const BodyComponent *childCatchConstruction;
	
	// позиция
	const Position errPos;

public:
	// задать объект, дочернюю конструкцию и позицию
	CatchConstruction( const PTypyziedEntity &ct, const Construction *pc, const Position &ep )
		: catchType(ct), Construction(CC_CATCH, pc, ep), childCatchConstruction(NULL) {
	}

	// если перехватывает все сообщения "catch(...)",
	// возвращает true
	bool IsCatchAll() const {
		return catchType.IsNull();
	}

	// вернуть перехватываемый тип
	const PTypyziedEntity &GetCatchType() const {
		return catchType;
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childCatchConstruction;
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childCatchConstruction != NULL );
		childCatchConstruction = (childComponent);
	}

};


// список catch-конструкций
typedef list<const CatchConstruction *> CatchConstructionList;


// try-catch конструкция
class TryCatchConstruction : public Construction
{
	// дочерний компонент конструкции try, всегда составная конструкция
	const Construction *childTryCatchComponent;

	// список catch-конструкций
	CatchConstructionList catchList;

public:
	// задаем инструкцию условия, родительскую конструкцию, позицию
	TryCatchConstruction( const Construction *pc, const Position &ep )
		: childTryCatchComponent(NULL), Construction(CC_TRY, pc, ep) {
	}

	// задать дочерний компонент
	void AddChildComponent( const BodyComponent *childComponent ) {
		INTERNAL_IF( childTryCatchComponent != NULL || childComponent == NULL ||
			childComponent->GetComponentID() != BCC_CONSTRUCTION );
		childTryCatchComponent = static_cast<const Construction *>(childComponent);
	}

	// вернуть дочерний компонент 
	const BodyComponent *GetChildComponent() const {
		return childTryCatchComponent;
	}

	// добавить catch-конструкцию
	void AddCatchConstruction( const CatchConstruction &cc ) {
		catchList.push_back(&cc);
	}

	// вернуть список catch-конструкций
	const CatchConstructionList &GetCatchList() const {
		return catchList;
	}
};


// список компонентов функции
typedef list<PBodyComponent> BodyComponentList;


// составная конструкция
class CompoundConstruction : public Construction
{
	// содержит список компонентов функции
	BodyComponentList componentList;

	// установлен в true, если составная конструкция создавалась неявно
	// для сохранения меток. В тексте программы она отсутствует
	bool mnemonic;

public:
	// задаем родительскую конструкцию, позицию
	CompoundConstruction( const Construction *pc, const Position &ep, bool mn = false )
		: Construction(CC_COMPOUND, pc, ep), mnemonic(mn) {
	}

	// вернуть true, если составная конструкция мнимая и создана неявно
	bool IsMnemonic() const {
		return mnemonic;
	}

	// метод добавляет дочерний компонент в список компонентов
	void AddChildComponent( const BodyComponent *childComponent ) {
		componentList.push_back(const_cast<BodyComponent *>(childComponent));
	}

	// не может вызываться, т.к. не содержит дочернего компонента, а содержит список
	const BodyComponent *GetChildComponent() const {
		INTERNAL( "'CompoundConstruction::GetChildComponent' не может вызываться" );
		return NULL;
	}

	// вернуть список компонентов функции
	const BodyComponentList &GetBodyComponentList() const {
		return componentList;
	}
};


// операция return, содержит выражение
class ReturnAdditionalOperation : public AdditionalOperation
{
	// возвращаемое значение, может быть NULL. Если NULL,
	// значит return без выражения
	POperand pExpr;

public:
	// задать выражение и позицию
	ReturnAdditionalOperation( const POperand &exp, const Position &ep )
		: pExpr(exp), AdditionalOperation( AOC_RETURN, ep ) {
	}

	// если return без выражения, вернуть true
	bool IsNoExpression() const {
		return pExpr.IsNull();
	}

	// вернуть выражение
	const POperand &GetExpression() const {
		return pExpr;
	}
};


// операция continue
class ContinueAdditionalOperation : public AdditionalOperation
{
public:
	// задать позицию
	ContinueAdditionalOperation( const Position &ep )
		: AdditionalOperation( AOC_CONTINUE, ep ) {
	}
};


// операция break
class BreakAdditionalOperation : public AdditionalOperation
{
public:
	// задать позицию
	BreakAdditionalOperation( const Position &ep )
		: AdditionalOperation( AOC_BREAK, ep ) {
	}
};


// операция asm, хранит строковый литерал с инструкцией
class AsmAdditionalOperation : public AdditionalOperation
{
	// строковый литерал хранящий асм-инструкции
	string stringLiteral;

public:
	// задать строковый литерал и позицию
	AsmAdditionalOperation ( const string &sl, const Position &ep )
		: stringLiteral(sl), AdditionalOperation( AOC_ASM, ep ) {
	}

	// вернуть строковый литерал
	const string &GetStringLiteral() const {
		return stringLiteral;
	}
};


// операция goto, хранит имя метки
class GotoAdditionalOperation : public AdditionalOperation
{
	// имя метки
	string labelName;

public:
	// задать имя метки и позицию
	GotoAdditionalOperation( PCSTR labelName, const Position &ep )
		: labelName(labelName), AdditionalOperation( AOC_GOTO, ep ) {
	}

	// вернуть имя метки
	const string &GetLabelName() const {
		return labelName;
	}
};


// тело функции. Содержит список инструкций и другую вспомагательную
// информацию
class FunctionBody
{
	// функция, к которой принадлежит тело
	const Function &pFunction;

	// конструкция принадлежащая телу, может быть составная, может быть try-блок
	Construction *construction;

public:
	// список запрашиваемых меток
	typedef pair<string, Position> QueryLabel;
	typedef list< QueryLabel > QueryLabelList;

	// список объявленных
	typedef list<Label> DefinedLabelList;

private:
	// список запрашиваемых меток
	QueryLabelList queryLabelList;

	// список объявленных меток
	DefinedLabelList definedLabelList;

public:

	// задать функцию и создать составную конструкцию
	FunctionBody( const Function &pFn, const Position &ccPos ) 
		: pFunction(pFn), construction( NULL ) {
	}

	// удалить составную конструкцию
	~FunctionBody() {
		delete construction;
	}
		
	// вирутальный метод, должен переопределяться в ConstructorFunctionBody
	virtual bool IsConstructorBody() const {
		return false;
	}

	// получить функцию
	const Function &GetFunction() const {
		return pFunction;
	}

	// добавить запрашиваемую метку
	void AddQueryLabel( const string &ql, const Position &pos ) {		
		queryLabelList.push_back( QueryLabel(ql, pos) );
	}

	// добавить объявленную метку, предварительно следует проверить,
	// не объявлена ли она уже
	void AddDefinedLabel( const Label &lbc ) {
		definedLabelList.push_back(lbc);
	}

	// получить список объявленных меток
	const DefinedLabelList &GetDefinedLabelList() const {
		return definedLabelList;
	}

	// получить список запрошенных меток
	const QueryLabelList &GetQueryLabelList() const {
		return queryLabelList;
	}

	// задать корневую конструкцию тела, задавать можно только один раз
	void SetBodyConstruction( Construction *rootConstr ) {
		INTERNAL_IF( construction != NULL );
		construction = rootConstr;
	}

	// вернуть составную конструкцию принадлежащую телу
	const Construction &GetBodyConstruction() const {
		return *construction;
	}

	// вернуть конструкцию принадлежащую телу, для изменения
	Construction &GetBodyConstruction() {
		return *construction;
	}
};


// тело конструктора, производный от FunctionBody
// класс. Помимо списка инструкций, содержит также список 
// инициализации
class ConstructorFunctionBody : public FunctionBody
{
	// список инициализации конструктора, после всех проверок задается
	ObjectInitElementList *oieList;

public:
	// задаем функцию, создаем список инициализации
	ConstructorFunctionBody( const Function &pFn, const Position &ccPos );
		
	// удалить список инициализации конструктора
	~ConstructorFunctionBody();

	// вирутальный метод
	bool IsConstructorBody() const {
		return true;
	}

	// задать список инициализации конструктора
	void SetConstructorInitList( const ObjectInitElementList &ol );

	// вернуть список инициализации
	const ObjectInitElementList &GetConstructorInitList() const {
		return *oieList;
	}
};
