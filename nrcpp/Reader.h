// интерфейс для вспомогательных классов парсера - Reader.h


// объявлен в ExpressionMaker.h
class ListInitComponent;


// типы деклараций
enum DeclarationVariant
{
	DV_GLOBAL_DECLARATION,
	DV_PARAMETR ,
	DV_CAST_OPERATION,
	DV_LOCAL_DECLARATION,
	DV_CLASS_MEMBER,
	DV_CAST_OPERATOR_DECLARATION,
	DV_NEW_EXPRESSION = DV_CAST_OPERATOR_DECLARATION,
	DV_CATCH_DECLARATION = DV_PARAMETR,
};


// Считывание декларатора, который в свою очередь может являться декларацией.
// Используется при считывании глобальных деклараций, параметров функции,
// параметров шаблона, операции приведения типа, локальных деклараций, членов класса.
class DeclaratorReader
{
	// тип деклараций
	DeclarationVariant	declType;

	// контейнер лексем, в который сохраняются все считанные лексемы
	LexemContainer	lexemContainer;

	// контейнер лексем, который служит для сохранения лексем в процессе
	// считывания и обеспечивает возможность отката считки на несколько лексем
	LexemContainer	undoContainer;

	// послдняя считанная лексема, неважно откуда из контейнера или из файла
	Lexem lastLxm;

	// результирующий пакет со спецификаторами типа и с декларатором,
	// причем декларатор может изменятся, если декларация содержит
	// несколько деклараторов
	PNodePackage tslPkg, declPkg;

	// лексический анализатор
	LexicalAnalyzer &lexicalAnalyzer;

	// список инициализаторов, в случае если он есть, по умолчанию NULL
	PExpressionList initializatorList;

	// флаг, который указывает, следует ли нам возвращать лексемы
	// из контейнера
	bool canUndo;

	// разрешает считывание выражения, в случае если первая лексема
	// декларации '::', и после нее идет
	bool canBeExpression;

	// устанавливается в true, если было считано имя
	bool nameRead;

	// текущий уровень скобок, необходим для инициализации,
	// если уровень нулевой, можно начинать разрешение неоднознчности между
	// параметрами функции и списком инициализации
	int crampLevel;

public:
	// конструктор принимает только тип декларации
	DeclaratorReader( DeclarationVariant dt, LexicalAnalyzer &l, bool cbe = false ) 
		: declType(dt), lexicalAnalyzer(l), tslPkg(new NodePackage(PC_TYPE_SPECIFIER_LIST)), 
		canBeExpression(cbe), declPkg(NULL), crampLevel(0), 
		nameRead(false), initializatorList(NULL) {

		canUndo = false;
	}

	// загрузить контейнер уже считанных из потока лексем, используется
	// для передачи лексем считанных другим ридером
	void LoadToUndoContainer( const LexemContainer &lc ) {
		undoContainer.insert(undoContainer.end(), lc.begin(), lc.end());
		canUndo = true;		
	}

	// добавить контейнер считанных другим ридером лексем в наш
	// контейнер
	void LoadToLexemContainer( const LexemContainer &lc ) {
		lexemContainer.insert(lexemContainer.end(), lc.begin(), lc.end());
	}

	// считать спецификаторы типа
	void ReadTypeSpecifierList();


	// считать следующий декларатор, используется в случае, когда в
	// декларации может быть несколько деклараторов. Также возбуждает
	// исключительную ситуацию если была синтаксическая ошибка
	PNodePackage ReadNextDeclarator();

	
	// вернуть пакет с типом
	PNodePackage GetTypeSpecPackage() const {
		return tslPkg;
	}

	// получить контейнер лексем
	const LexemContainer &GetLexemContainer() const {
		return lexemContainer;
	}

	// получить контейнер отката
	const LexemContainer &GetUndoContainer() const {
		return undoContainer;
	}

	// получить список инициализаторов, может быть NULL
	const PExpressionList &GetInitializatorList() const {
		return initializatorList;
	}

	// методы считывания лексем специфичные для данного ридера	
private:

	// считывает декларатор
	void ReadDeclarator( );


	// считать прототип функции
	PNodePackage ReadFunctionPrototype();

	// считываем хвостовую часть декларатора, если не приведение типа
	void ReadDeclaratorTailPart();

	// считать последовательность из cv-квалификаторов, и сохраняет их 
	// в пакете
	void ReadCVQualifierSequence( PNodePackage &np );

	// считать тип в throw-спецификации и вернуть его
	PNodePackage ReadThrowSpecType();

	// считать список спецификаторов типа, при 
	// получить следующую лексему, сохранить ее в контейнере
	const Lexem &NextLexem();


	// вернуть предыдущую лексему в поток
	void BackLexem();
};


// Класс используется при считывании конструкций, которые могут быть 
// квалифицированными с помощью явного указания областей видимости. 
// Это может быть обычное имя, шаблон, перегруженный оператор, деструктор, 
// указатель на член. Класс может использоваться для считывания и 
// неквалифицированных имен
class QualifiedConstructionReader
{
	// лекс
	LexicalAnalyzer &lexicalAnalyzer;

	// контейнер, куда сохраняются все считанные лексе
	LexemContainer lexemContainer;

	// контейнер лексем, который служит для сохранения лексем в процессе
	// считывания и обеспечивает возможность отката считки на несколько лексем
	PLexemContainer	undoContainer;

	// послдняя считанная лексема, неважно откуда из контейнера или из файла
	Lexem lastLxm;

	// пакет со считанной конструкцией
	PNodePackage resultPackage;

	// флаг, который указывает, следует ли нам возвращать лексемы
	// из контейнера
	bool canUndo;

	// флаг считывания квалифицированных конструкций, если true,
	// считыватся только одиночные конструкции и указатель на член
	// в противном случае - синтскчиеская ошибка
	bool noQualified;

	// флаг считывания операторов и деструкторов, если true,
	// считываются только имена и указатель на член
	bool noSpecial;

	// в случае считывания конструкции, которая начинается с '::',
	// может идти выражение '::new' или '::delete'. Когда этот флаг
	// установлен и появляются лексемы new или delete после первого '::',
	// синтаксической ошибки не возникает
	bool noErrorOnExp;

	// флаг устанавливается, в случае если было считано выражение
	// '::new' или '::delete'
	bool readExpression;

public:
	// задаем параметры считки
	QualifiedConstructionReader( LexicalAnalyzer &la, 
		bool noq = false, bool nos = false,
		const PLexemContainer &puc = PLexemContainer(0), bool neoe = false ) 

		: noQualified(noq), noSpecial(nos), lexicalAnalyzer(la),
		resultPackage(new NodePackage(PC_QUALIFIED_NAME)),
		undoContainer(puc), noErrorOnExp(neoe), readExpression(false)  {

		canUndo = !undoContainer.IsNull();
		if( undoContainer.IsNull() )
			undoContainer = new LexemContainer;
	}

	// получить контейнер лексем
	const LexemContainer &GetLexemContainer() const {
		return lexemContainer;
	}

	// получить контейнер отката
	const LexemContainer &GetUndoContainer() const {
		return *undoContainer;
	}

	// считать квалифицированную конструкцию
	PNodePackage ReadQualifiedConstruction();

	// загрузить контейнер уже считанных из потока лексем, используется
	// для передачи лексем считанных другим ридером
	void LoadToUndoContainer( const LexemContainer &lc ) {
		if( undoContainer.IsNull() )
			undoContainer = new LexemContainer;

		INTERNAL_IF( !undoContainer->empty() );
		undoContainer->insert(undoContainer->end(), lc.begin(), lc.end());
		canUndo = true;		
	}


	// если конструкция квалифицированная
	bool IsQualified() const {
		// если шаблон или деструктор или оператор, тоже вернет тру, фиксми!
		return resultPackage->GetChildPackageCount() > 1;	
	}

	// если конструкция одиночная
	bool IsSimple() const {
		return !IsQualified();
	}

	// если считан указатель на член
	bool IsPointerToMember() const {
		return resultPackage->GetPackageID() == PC_POINTER_TO_MEMBER;
	}

	// если считан тип - класс, перечисление, typedef, специализированный шаблон
	bool IsTypeName() const;

	// если считано простое имя, а не оператор или деструктор
	bool IsIdentifier() const {
		return resultPackage->GetChildPackage(
			resultPackage->GetChildPackageCount()-1)->GetPackageID() == NAME;
	}

	// если было считано выражение '::new' или '::delete'
	bool IsExpression() const {
		return readExpression;
	}

private:
	// считать перегруженный оператор
	PNodePackage ReadOverloadOperator( );

	// получить следующую лексему и сохранить ее в контейнер
	const Lexem &NextLexem();


	// вернуть лексему в поток
	void BackLexem() ;
};


// Считывает конструкцию начиная с '{' и до соответствующей ей '}'. В параметре
// можно задать сохранение считанных лексем в контейнере, либо просто игнорирование потока.
class CompoundStatementReader
{
	// если true, лексемы в контейнере не сохраняются и поток
	// просто игнорируется от { до }
	bool ignoreStream;

	// контейнер, в который сохраняются считанные лексемы, если
	// ignoreStream == false
	LexemContainer lexemContainer;

	// лекс
	LexicalAnalyzer &lexicalAnalyzer;	

public:
	// задаем параметры
	CompoundStatementReader( LexicalAnalyzer &la, bool ignore ) 
		: lexicalAnalyzer(la), ignoreStream(ignore) {

		INTERNAL_IF( lexicalAnalyzer.LastLexem() != '{' );
	}

	// функция считывания конструкции от { до соотв. ей }
	void Read();

	// получить контейнер
	const LexemContainer &GetLexemContainer() const {
		return lexemContainer;
	}
};


// считывание тела функции. Похоже на работу CompoundStatementReader,
// с той лишь разницей, что поддерживаются try-блоки и список 
// инициализаторов у конструктора
class FunctionBodyReader
{
	// игнорировать поток, не сохранять лексемы в контейнере
	bool ignoreStream;

	// контейнер в котором могут сохраняться лексемы
	PLexemContainer lexemContainer;

	// лекс
	LexicalAnalyzer &lexicalAnalyzer;

	
	// закрытая функция, которая считывает try-блок
	void ReadTryBlock();

	// закрытая функция считывает список инициализации
	void ReadInitList();

public:

	// задать параметры
	FunctionBodyReader( LexicalAnalyzer &la, bool ignore ) 
		: lexicalAnalyzer(la), ignoreStream(ignore), 
		lexemContainer( ignore ? NULL : new LexemContainer ) {

	}

	// метод считывания тела
	void Read();

	// получить контейнер
	PLexemContainer GetLexemContainer() const {
		return lexemContainer;
	}
};


// считать список инициализации
class InitializationListReader
{
	// анализатор
	LexicalAnalyzer &lexicalAnalyzer;

	// результирующий список, который строится
	ListInitComponent *listInitComponent;
	
	// считывает список, возвращает указатель на сформированный.
	// Рекурсивная ф-ция
	void ReadList( ListInitComponent *lic );

public:
	// задать анализатор. Следующей считываемой лексемой должна быть '{'
	InitializationListReader( LexicalAnalyzer &la ) 
		: lexicalAnalyzer(la), listInitComponent(NULL) {
	}

	// считать 
	void Read();

	// получить сформированный список
	const ListInitComponent *GetListInitComponent() const {
		return listInitComponent;
	}
};


// считывает декларацию, если обнаруживается, что декларация некорректна,
// считывает выражение. Используется в size, typeid, параметрах шаблона,
// локальных инструкциях, конструкциях if, while, switch 
class TypeExpressionReader
{
	// флаги для выражения
	bool noComa, noGT, ignore;

	// если флаг установлен, 
	// лекс.
	LexicalAnalyzer &lexicalAnalyzer;

	// контейнер 
	PLexemContainer lexemContainer;

	// результирующий пакет с кодом PC_DECLARATION, если была считана
	// декларация или с кодом PC_EXPRESSION, если считано выражение
	Package *resultPkg;

	// список инициализаторов, в случае если он есть, по умолчанию NULL.
	// Передается из DeclaratorReader'а
	PExpressionList initializatorList;

	// в случае, если было считано объявление класса, сохраняем его
	const ClassType *redClass;
	
public:

	// конструктор задает поля класса
	TypeExpressionReader( LexicalAnalyzer &la, 
		bool nc = false, bool ng = false, bool ign = true )
		: lexicalAnalyzer(la), noComa(nc), noGT(ng), ignore(ign), 
		lexemContainer(0), resultPkg(NULL), initializatorList(NULL), redClass(NULL) {
	}


	// получить контейнер
	PLexemContainer GetLexemContainer() const {
		return lexemContainer;
	}

	// получить считанный класс, может быть NULL
	const ClassType *GetRedClass() const {
		return redClass;
	}
	
	// вернуть результирующий пакет
	const Package *GetResultPackage() const {
		return resultPkg;
	}

	// получить список инициализаторов, может быть NULL
	const PExpressionList &GetInitializatorList() const {
		return initializatorList;
	}

	// считать декларацию, если декларация некорректно сформирована,
	// считать выражение. Параметр установлен в true, если считываем
	// либо список инициализаторов, либо параметр функции в прототипе
	void Read( bool fnParam = false, bool readTypeDecl = true );
};


// ридер выражения
class ExpressionReader
{
	// лексический анализатор
	LexicalAnalyzer &lexicalAnalyzer;

	// может быть задан указатель на контейнер, из которого следует 
	// производить считывание. В противном случае равен 0
	LexemContainer *undoContainer;

	// на верхнем уровне запрещено выражение через запятую. Задается
	// в true, в параметрах функции, шаблона, инициализаторе.
	bool noComa;

	// запрещает использование '>' на верхнем уровне. Используется
	// при считывании параметров шаблона
	bool noGT;

	// если установлен в true, значит происходит возврат лексем из
	// контейнера. При этом контейнер не может быть пустым или NULL
	bool canUndo;

	// текущий уровень вложенности выражения. Если выражение берется в
	// скобки, уровень увеличивается на 1
	int level;

	// если установлен в true, значит лексемы не сохраняются в контейнере,
	// иначе создается контейнер и каждая считанная лексема записывается в него.
	// Устанавливается в false, только при считывании шаблонных параметров
	bool ignore;

	// контейнер в котором могут сохраняться лексемы
	PLexemContainer lexemContainer;

	// операнд, в который упаковано все выражение
	POperand resultOperand;

	// если true, значит undoContainer следует удалить
	bool deleteUndoContainer;

public:

	// в конструкторе задаются необх. параметры
	ExpressionReader( LexicalAnalyzer &la, LexemContainer *uc = NULL,
		bool nc = false, bool ngt = false, bool ig = true )
		: lexicalAnalyzer(la), undoContainer(uc), noComa(nc), noGT(ngt), level(0), ignore(ig), 
		canUndo(uc != NULL), lexemContainer( ignore ? NULL : new LexemContainer ),
		resultOperand(NULL), deleteUndoContainer(false) {
		
		if( undoContainer == NULL )
			undoContainer = new LexemContainer, deleteUndoContainer = true;
	}

	// в деструкторе можно освободить контейнер
	~ExpressionReader() {
		if( deleteUndoContainer )
			delete undoContainer;
	}



	// считать выражение
	void Read();

	// получить контейнер
	PLexemContainer GetLexemContainer() const {
		return lexemContainer;
	}

	// вернуть результат считывания выражения
	const POperand &GetResultOperand() const {
		return resultOperand;
	}
		
private:

	// последняя считанная лексема
	Lexem lastLxm;

	// возвращает следующую лексему из потока, либо из контейнера, если тот не пуст
	Lexem NextLexem();

	// возвращает в поток или в контейнер последнюю считанную лексему
	void BackLexem();

	// вернуть в undoContainer лексему. При этом если режим восстановления
	// не включен, он включается
	void UndoLexem( const Lexem &lxm );

	// восстановить считанные лексемы в undoContainer и переключиться в режим 
	// считывания из контейнера
	void UndoLexemS( const LexemContainer &lc ) {
		INTERNAL_IF( !undoContainer->empty() );
		undoContainer->insert( undoContainer->end(), lc.begin(), lc.end() );
		canUndo = true;
	}

	// считать абстрактный декларатор, вернуть пакет с типом. Если 
	// noError - true, значит считанные лексемы загружаются в undoContainer
	PNodePackage ReadAbstractDeclarator( bool noError, 
		DeclarationVariant dv = DV_CAST_OPERATION );

	// считать список выражений, вернуть список в упакованном виде,
	// Условием завершения считывания является лексема ')'. 
	// Результирующий список может быть пустым
	PExpressionList ReadExpressionList( );

	// проверить результирующий операнд, он не должен быть типом, 
	// перегруженной функцией, а также основным операндом, который
	// является нестатическим членом класса
	void CheckResultOperand( POperand &pop );

	// функции вычисления выражения с учетом приоритетов операций
private:	

	// оператор ','
	void EvalExpr1( POperand &result );

	// операторы присвоения '=', '+=','-=','*=','/=','%=','>>=','<<=','|=','&=','^=',
	// throw
	void EvalExpr2( POperand &result );

	// оператор '?:'
	void EvalExpr3( POperand &result );

	// оператор ||
	void EvalExpr4( POperand &result );

	// оператор &&
	void EvalExpr5( POperand &result );

	// оператор |
	void EvalExpr6( POperand &result );

	// оператор '^'
	void EvalExpr7( POperand &result );

	// оператор '&'
	void EvalExpr8( POperand &result );
	
	// операторы ==, !=
	void EvalExpr9( POperand &result );

	// операторы <=, <, >, >=
	void EvalExpr10( POperand &result );

	// операторы <<, >>
	void EvalExpr11( POperand &result );

	// операторы +, -
	void EvalExpr12( POperand &result );

	// операторы *, /, %
	void EvalExpr13( POperand &result );

	// операторы .*, ->*
	void EvalExpr14( POperand &result );

	// операторы приведения типа '(тип)'
	void EvalExpr15( POperand &result );

	// унарные операторы !, ~, +, -, *, &. size, ++, --, new, delete
	void EvalExpr16( POperand &result );

	// операторы '()', '[]', '->', '.', постфиксные ++, --,
	// dynamic_cast, static_cast, const_cast, reinterpret_cast, typeid
	void EvalExpr17( POperand &result );

	// литерал, true, false, this,
	// идентификатор (перегруженный оператор, деструктор, оператор приведения)
	// или выражение в скобках
	void EvalExpr18( POperand &result );

private:

	// класс контролирующий переполнение стека
	class StackOverflowCatcher
	{
		// счетчик стека
		static int stackDeep;

	public:
		StackOverflowCatcher( ) {
			if( stackDeep == MAX_PARSER_STACK_DEEP )
				theApp.Fatal( "стек переполнен; слишком сложное выражение" );
			stackDeep++;
		}

		~StackOverflowCatcher() {
			stackDeep--;
		}
	};
};
