// строители тела функции, проверяют и строят - BodyMaker.h


// утилиты для постройки тела функции
namespace BodyMakerUtils
{
	// создает инициализатор объекта из списка выражений и конструктор.
	// Создает инициализатор конструктором
	PObjectInitializator MakeObjectInitializator( 
		const PExpressionList &initList, const DeclarationMaker &dm );

	// создает инициализатор из списка инициализации
	PObjectInitializator MakeObjectInitializator( const PListInitComponent &il );

	// построить инструкцию условия для конструкций if, for, switch, while
	// на основе пакетов с декларацией и инициализатором
	PInstruction MakeCondition( const NodePackage &decl, 
				const POperand &iator, const Position &errPos );

	// проверяет, чтобы условная конструкция преобразовывалась в склярный или целый тип
	// Инструкцией может быть декларация, либо выражение. В случае декларации,
	// создаем временный основной операнд на основе декларатора и его проверяем.
	// Флаг toInt, указывает, что преобразование должно производится в целый тип,
	// в противном случае в склярный
	void ValidCondition( const PInstruction &cond, PCSTR cnam, bool toInt = false );

	// проверяет условие только для выражения
	void ValidCondition( const POperand &exp, PCSTR cnam, const Position &ep );

	// создать область видимости для новой конструкции ориентируяясь 
	// на предыдущую, если создана возвращает true
	bool MakeLocalSymbolTable( const Construction &cur );

	// шаблонная функция создания компонента, который требует только позицию
	template <class Component>
	inline Component *SimpleComponentMaker( const Position &ep ) {
		return new Component(ep);
	}

	// шаблонная функция создания компонента, который требует 
	// родительскую конструкцию и позицию
	template <class Cons>
	inline Cons *SimpleConstructionMaker( Construction &parent, const Position &ep ) {
		return new Cons(&parent, ep);
	}

	// создание конструкций: if, switch, while
	template <class Cons>
	inline Cons *ConditionConstructionMaker( const PInstruction &cond, 
		Construction &parent, const Position &ep ) {	
		return new Cons(cond, &parent, ep);
	}

	// создать for-конструкцию, с проверкой выражения если есть
	inline ForConstruction *ForConstructionMaker( const ForInitSection &fic, 
		const PInstruction &cond, const POperand &iter, Construction &ppc, const Position &ep ) {
		if( !cond.IsNull() )
			BodyMakerUtils::ValidCondition( cond, "for" );	
		return new ForConstruction(fic, cond, iter, &ppc, ep);;
	}

	// строитель выражения
	inline ExpressionInstruction *ExpressionInstructionMaker( 
		const POperand &exp, const Position &ep ) {

		return new ExpressionInstruction(exp, ep);
	}

	// строитель декларации
	inline DeclarationInstruction *DeclarationInstructionMaker( const TypyziedEntity &dator, 
		const PObjectInitializator &iator, const Position &ep ) {
		// декларатор должен быть объектом или функцией
		INTERNAL_IF( &dator == NULL ||
			!(dator.IsObject() || dator.IsFunction()) ); 

		return new DeclarationInstruction(dator, iator, ep);
	}

	// формируем декларацию класса
	inline ClassDeclarationInstruction *ClassInstructionMaker( 
		const ClassType &cls, const Position &ep ) {
		return new ClassDeclarationInstruction(cls, ep);
	}

	// строитель инструкции декларации класса
	inline ClassDeclarationInstruction *ClassDeclarationInstructionMaker( 
		const ClassType &cls, const Position &ep ) {
		return new ClassDeclarationInstruction(cls, ep);			
	}

	// строитель обычной метки
	SimpleLabelBodyComponent *SimpleLabelMaker( 
		const Label &lab, const BodyComponent &nc, FunctionBody &fnBody, const Position &ep );
		
	// строитель case, с проверкой
	CaseLabelBodyComponent *CaseLabelMaker( const POperand &exp, 
		const BodyComponent &childBc, const Construction &cur, const Position &ep );

	// строитель default, с проверкой
	DefaultLabelBodyComponent *DefaultLabelMaker( 
		const BodyComponent &childBc, const Construction &cur, const Position &ep );

	// строитель asm
	inline AsmAdditionalOperation *AsmOperationMaker( 
							const CharString &lit, const Position &ep ) {
		return new AsmAdditionalOperation(lit.c_str(), ep);
	}

	// строитель else, с проверкой
	ElseConstruction *ElseContructionMaker( const Construction &cur, const Position &ep );

	// строитель break-операции с семантической проверкой 
	BreakAdditionalOperation *BreakOperationMaker( Construction &ppc, const Position &ep );

	// строитель continue-операции
	ContinueAdditionalOperation *ContinueOperationMaker( Construction &ppc, const Position &ep );
	
	// строитель return-операции. Конструктор не может возвращать значения, также
	// как деструктор
	ReturnAdditionalOperation *ReturnOperationMaker( 
		const POperand &rval, const Function &fn, const Position &ep );

	// строитель goto-операции
	GotoAdditionalOperation *GotoOperationMaker( 
		const CharString &labName, FunctionBody &fnBody, const Position &ep );

	// строитель списка инструкций. Может быть блок деклараций, либо выражение
	Instruction *InstructionListMaker( const InstructionList &insList, const Position &ep );
}


// контроллер конструкций. Следит за сменой текущей конструкции, удалением
// областей видимости и контролем за порядком строительства тела функции
class ConstructionController
{
	// текущая конструкция в теле обрабатываемой функции
	const Construction *pCurrentConstruction;

public:
	// задать текущую конструкцию
	ConstructionController( const Construction &pCur )
		: pCurrentConstruction(&pCur) {
	}

	// задать текущую конструкцию извне. Требуется при создании мнимой
	// составной области видимости для меток
	void SetCurrentConstruction( const Construction *pCur ) {
		INTERNAL_IF( pCur == NULL );
		pCurrentConstruction = pCur;
	}

	// получить текущую конструкцию
	Construction &GetCurrentConstruction() const {
		return const_cast<Construction &>(*pCurrentConstruction); 
	}
};


// строитель catch-конструкции с проверкой
class CatchConstructionMaker
{
	// перехватываемый объект
	const PTypyziedEntity &catchObj;

	// родительская try-конструкция
	Construction &parent;

	// позиция
	const Position &ep;

	// проверяет, являются ли обработчики одинаковыми
	bool EqualCatchers( const CatchConstruction &cc1, const CatchConstruction &cc2 ) const;

public:
	// принимаем объект (может быть 0, в случае ...), 
	// родительскую конструкцию (try) и позицию
	CatchConstructionMaker( const PTypyziedEntity &catchObj, Construction &parent,
		const Position &ep ) 
		: catchObj(catchObj), parent(parent), ep(ep) {
		INTERNAL_IF( parent.GetConstructionID() != Construction::CC_TRY );
	}

	// строить
	CatchConstruction *Make();
};


// проверки тела функции после постройки. Проверяет все метки, к которым
// идет обращение через goto. Проверяет чтобы goto, case, default не перескакивали
// через декларации
class PostBuildingChecks
{
	// ссылка на тело функции
	const FunctionBody &body;

	// класс предикат, используется при поиске метки в списке
	class LabelPr {
		// имя искомой метки
		const string &lname;

	public:
		// задать имя
		LabelPr( const string &ln )
			: lname(ln) {
		}

		// если имя совпало, вернуть true
		bool operator()( const Label &lab ) {
			return lname == lab.GetName().c_str();
		}
	};

	// проверим, чтобы все метки, к которым идет обращение через
	// goto, были объявлены
	void CheckLabels( const FunctionBody::DefinedLabelList &dll, 
		const FunctionBody::QueryLabelList &qll );

public:
	// задать тело
	PostBuildingChecks( const FunctionBody &body )
		: body(body) {
	}

	// выполнить проверки
	void DoChecks() {
		CheckLabels(body.GetDefinedLabelList(), body.GetQueryLabelList());
	}
};
