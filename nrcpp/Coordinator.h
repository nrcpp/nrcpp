// реализация интерфейса к КЛАССАМ-КООРДИНАТОРАМ - Coordinator.h


// Классы-координаторы - это надстройки над классами-строителями. 
// Координаторы определяют, какой класс строитель использовать для 
// входного пакета, также учитывая внешние факторы, такие как текущая
// область видимости и др. Координаторы можно разделить на две 
// основные части: координаторы деклараций и координаторы 
// выражений. В первом случае для деклараций определяются строители,
// которые смогут создать и проверить на семантику контейнеры. Во
// втором случае также выбирается строитель подвыражения, и также
// проверяется корректность и выражение сохраняется.


// тип - интеллектуальный указатель на строителя
typedef SmartPtr<DeclarationMaker> PDeclarationMaker;


// класс-координатор для деклараций
class DeclarationCoordinator
{
	// входной пакет, который используется для определения 
	// класса конструктора
	const NodePackage *typeSpecList, *declarator;
	
	// области видимости, которые вставляются в систему ОВ при
	// определении члена класса или именованной ОВ
	mutable SymbolTableList memberStl;

	// менеджер члена, сохраняется для передачи строителю члена
	mutable QualifiedNameManager *memberQnm;

	// помимо областей видимости члена, сохраняем 
	// закрытый метод, сохраняет менеджер имени члена и области
	// видимости члена
	void StoreMemberScope( const NodePackage &np ) const;

public:
	// в конструкторе задется 2 пакета для анализа:
	// 1. список спецификаторов функции, 2. декларатор (имя, список производных типов)
	DeclarationCoordinator( const NodePackage *tsl, const NodePackage *dcl ) 
		: typeSpecList(tsl), declarator(dcl), memberQnm(NULL) {
		INTERNAL_IF( typeSpecList == NULL || declarator == NULL );
	}

	// в деструкторе уничтожается менеджер имени
	~DeclarationCoordinator() {
		delete memberQnm;
		RestoreScopeSystem();
	}

	// скоординировать постройку декларации, создать из пакета
	// временную структуру и далее на ее основании выбрать строителя
	PDeclarationMaker Coordinate() const;

	// восстановить систему ОВ после определения члена ИОВ или класса
	void RestoreScopeSystem() const;
};


// координатор локальных деклараций
class AutoDeclarationCoordinator
{
	// входной пакет, который используется для определения 
	// класса конструктора
	const NodePackage *typeSpecList, *declarator;
	
public:
	// в конструкторе задется 2 пакета для анализа:
	// 1. список спецификаторов функции, 2. декларатор (имя, список производных типов)
	AutoDeclarationCoordinator( const NodePackage *tsl, const NodePackage *dcl ) 
		: typeSpecList(tsl), declarator(dcl) {
		INTERNAL_IF( typeSpecList == NULL || declarator == NULL );
	}

	// скоординировать постройку декларации, создать из пакета
	// временную структуру и далее на ее основании выбрать строителя
	PDeclarationMaker Coordinate() const;
};


// тип - интеллектуальный указатель на строителя члена класса
typedef SmartPtr<MemberDeclarationMaker> PMemberDeclarationMaker;


// класс-координатор для деклараций членов класса
class MemberDeclarationCoordinator
{
	// список типов и декларатор для формирования временной структуры
	// и определения строителя
	const NodePackage *typeSpecList, *declarator;

	// ссылка на класс, член которого строится
	ClassType &clsType;

	// текущий спецификатор доступа в этом классе
	ClassMember::AS curAccessSpec;

public:
	// в конструкторе задаются пакеты, а также класс и 
	// текущий спецификатор доступа
	MemberDeclarationCoordinator( const NodePackage *tsl, const NodePackage *dcl,
		ClassType &ct, ClassMember::AS cas ) : typeSpecList(tsl), declarator(dcl), 
											   clsType(ct), curAccessSpec(cas) 
	{
		INTERNAL_IF( typeSpecList == NULL || declarator == NULL );
	}

	// определить строителя декларации 
	PMemberDeclarationMaker Coordinate();	
};


// координатор унарных выражений, выполняет предварительные проверки
// корректности выражения, а также проверяет необходимость интерпретации 
// выражения или вызова перегруженного оператора. После выполнения
// этих действий выбирает строителя унарных выражений на основании оператора.
// Результатом работы координатора является новый операнд
class UnaryExpressionCoordinator
{
	// позиция для вывода ошибок
	const Position &errPos;

	// операнд унарного выражения
	const POperand &right;

	// код оператора
	int op;

public:
	// задаем информацию для координации
	UnaryExpressionCoordinator( const POperand &r, int op_, const Position &ep ) 
		: right(r), op(op_), errPos(ep) {
	}

	// скоординировать и построить выражение
	POperand Coordinate() const;
};


// координатор бинарных выражений. По сравнению с унарным координатором,
// является шаблонным, т.к. при считывании оператор как правило известен и
// поэтому параметром класса можно сразу задать класс строитель
template <class Maker>
class BinaryExpressionCoordinator
{
	// позиция для вывода ошибок
	const Position &errPos;

	// левый операнд бинарного выражения
	const POperand &left;

	// правый операнд бинарного выражения
	const POperand &right;

	// код оператора
	int op;

public:
	// задаем информацию для координации
	BinaryExpressionCoordinator( const POperand &l, const POperand &r, 
		int op_, const Position &ep ) 

		: left(l), right(r), op(op_), errPos(ep) {
	}

	// скоординировать и построить выражение
	POperand Coordinate() const {

		// выражение должно присутствовать
		INTERNAL_IF( left.IsNull() || right.IsNull() );

		// сначала проверяем, если выражение ошибочно, вернуть его же
		if( left->IsErrorOperand() || right->IsErrorOperand() )
			return ErrorOperand::GetInstance();		

		// операнд не может быть типом, только если это не операция приведения
		if( right->IsTypeOperand() || 
			(left->IsTypeOperand() && 
			 !(op == OC_CAST || op == KWSTATIC_CAST || op == KWDYNAMIC_CAST ||
			   op == KWCONST_CAST || op == KWREINTERPRET_CAST) ) )
		{
			theApp.Error(errPos, "тип не может быть операндом в выражении");
			return ErrorOperand::GetInstance();
		}

		// не может быть перегруженного операнда
		else if( left->IsOverloadOperand() || right->IsOverloadOperand() )
		{
			theApp.Error(errPos, "перегруженная функция не может быть операндом в выражении");
			return ErrorOperand::GetInstance();
		}

		// проверяем, если имеем нестатический данное-член, чтобы 
		// обращение к нему было через 'this'
		if( left->IsPrimaryOperand() )
			ExpressionMakerUtils::CheckMemberThisVisibility(left, errPos);
		if( right->IsPrimaryOperand() )
			ExpressionMakerUtils::CheckMemberThisVisibility(right, errPos);		

		// далее пытаемся проверить операнд на интерпретируемость
		POperand rval = BinaryInterpretator(left, right, op, errPos).Interpretate();
		if( !rval.IsNull() )
			return rval;

		// пытаемся выполнить вызов перегруженного оператора для двух операндов
		rval = BinaryOverloadOperatorCaller(left, right, op, errPos).Call();
		if( !rval.IsNull() )
			return rval;

		// далее строим выражение, строитель уже задан как шаблонный
		// параметр
		return Maker(left, right, op, errPos).Make();
	}
};


// координатор тернарных выражений. Тернарное выражение одно '?:',
// координатор необходим для предварительных проверок корректности выражения
// и возможно выполнения интерпретации
class TernaryExpressionCoordinator
{
	// позиция для вывода ошибок
	const Position &errPos;

	// условие
	const POperand &cond;

	// левый операнд от ':'
	const POperand &left;

	// правый операнд от ':'
	const POperand &right;

	// код оператора
	int op;

public:
	// задаем информацию для координации
	TernaryExpressionCoordinator( const POperand &c, const POperand &l, const POperand &r, 
		int op_, const Position &ep ) 

		: cond(c), left(l), right(r), op(op_), errPos(ep) {
	}

	// скоординировать и построить выражение
	POperand Coordinate() const;
};
