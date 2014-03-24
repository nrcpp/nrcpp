// объявление классов идентификатор, типизированная сущность, 
// производный тип и производных от них - Object.h


#ifndef _OBJECT_H_INCLUDE
#define _OBJECT_H_INCLUDE


// класс объявляется в SymbolTable.h
class SymbolTable;

// объявлен в Class.h
class ClassType;

// объявлен в Translator.h
class IdentifierGenerateData;


// базовый класс, для всех классов явлющихся идентификаторами
// объекты, метки, именованные области видимости, функции
// класс и перечислимый тип. 
// Помимо группировки классов содержащих имя, класс введен для
// того, чтобы все именованные сущности можно было хранить
// в таблице как указатели на класс идентификатор
class Identifier
{
	// имя идентификатора
	NRC::CharString name;
	
	// указатель на таблицу символов, к которой принадлежит
	// это имя
	const SymbolTable *pTable;

protected:
	// имя идентификатора используемое при генерации кода. Задается
	// через конструкторы производных типов
	string c_name;

public:

	// конструктор по умолчанию
	Identifier() { 
		pTable = NULL;
	}

	
	// конструктор с заданием параметров
	Identifier( const NRC::CharString &n, const SymbolTable *p )
		: name(n), pTable(p) {		
		
	}

	// виртуальный деструктор для производных классов. Освобаждет
	// память занятую информацией о генерации
	virtual ~Identifier() {
	}

	// получить короткое имя идентификатора
	const NRC::CharString &GetName() const { 
		return name; 
	}

	// получить квалифицированное имя идентификатора
	NRC::CharString GetQualifiedName() const;

	// получить указатель на таблицу символов, в которой находится
	// идентификатор
	const SymbolTable &GetSymbolTableEntry() const { 
		INTERNAL_IF( pTable == NULL );
		return *pTable;
	}
	

	// метод, который должен возвращать второе
	// имя этого идентификатора, которое будет использоваться
	// при генерации кода
	const string &GetC_Name() const {
		return c_name;
	}
}; 

	
// список идентификаторов. Единица хранения в хэш-таблице
typedef list<const Identifier *> IdentifierList;

// список из списков идентификаторов
typedef list<IdentifierList> ListOfIdentifierList;


// интерфейс для членов класса
class ClassMember
{
public:	
	// виртуальный деструктор, необходим т.к. члены класса уничтожаются
	// внутри класса как ClassMember
	virtual ~ClassMember() {		
	}

	// возможные коды доступа
	enum AS {
		NOT_CLASS_MEMBER, AS_PRIVATE, AS_PROTECTED, AS_PUBLIC, 
	};

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return NOT_CLASS_MEMBER;
	}

	// чисто виртуальный метод для переопределения в тех классах,
	// которые могут быть членами класса
	virtual bool IsClassMember() const = 0;
};


// идентификатор объявляемый с помощью using-директивы
class UsingIdentifier : public Identifier, public ClassMember
{
	// спецификатор доступа если идентификатор является членом 
	// класса
	AS accessSpecifier;

	// указатель на список действительных деклараций идентификаторов
	const Identifier *pIdentifier;

public:
	// конструктор задает необходимые параметры
	UsingIdentifier( const NRC::CharString &n, SymbolTable *p, 
		const Identifier *pid, AS as ) :
	  Identifier(n, p), pIdentifier(pid), accessSpecifier(as) {
	}

	// является ли идентификатор членом класса
	virtual bool IsClassMember() const {
		return accessSpecifier != NOT_CLASS_MEMBER;
	}

	// получить область видимости using-декларации,
	// метод введен для ясности
	const SymbolTable &GetDeclarativeRegion() const { 
		return GetSymbolTableEntry();
	}

	// получить действительное объявление
	const Identifier &GetUsingIdentifier() const {
		return *pIdentifier;
	}

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}
};


// неспецифированный идентификатор, у которого отстутствует
// семантическое значение. Используется как член шаблонного параметра
// T::m. Может членом класса, если используется в объявлении using
class UnspecifiedIdentifier : public Identifier, public ClassMember
{
	// может быть типом
	bool type;

	// спецификатор доступа если есть
	AS accessSpecifier;

public:
	// конструктор задает параметры идентификатора
	UnspecifiedIdentifier( const NRC::CharString &n, SymbolTable *p, bool tp,
		AS as ) :
	  Identifier(n, p), type(tp), accessSpecifier(as) {
	}

	// идентификатор возможно является типом
	bool IsTypename() const {
		return type;
	}

	// является ли идентификатор членом класса
	virtual bool IsClassMember() const {
		return accessSpecifier != NOT_CLASS_MEMBER;
	}

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}
};


// метка
class Label : public Identifier
{
	// позиция в которой объявляется метка
	Position definPos;

public:
	// конструктор с заданием имени и указателя на тело функции
	Label( const NRC::CharString &n, SymbolTable *p, const Position &dp ) 
		: Identifier(n,p), definPos(dp) {		
	}

	// получить позицию в которой объявлялась метка
	const Position &GetDefinPos() const { 
		return definPos;
	}
};


// базовый класс для 5 разновидностей производных типов
class DerivedType
{
public:

	// возможные коды производного типа
	enum DT { 
		DT_ARRAY, DT_POINTER, DT_POINTER_TO_MEMBER, DT_REFERENCE, 
		DT_FUNCTION_PROTOTYPE 			
	};

private:
	// код производного типа
	DT  derivedTypeCode;

public:

	// конструктор с заданием кода или без него
	DerivedType( DT dtc ) {
		derivedTypeCode = dtc;
	}


	// виртуальный деструктор
	virtual ~DerivedType() { }

	// получить код производного типа
	DT GetDerivedTypeCode() const {		
		return derivedTypeCode;
	}

	// абстрактный метод получения размера в байтах проихводного типа
	virtual int GetDerivedTypeSize() const = 0;
};


// интеллектуальный указатель на производный тип
typedef SmartPtr<DerivedType> PDerivedType;


// список производных типов
class DerivedTypeList
{
	// список указателей на объекты производные от DerivedType
	vector<PDerivedType> derivedTypeList;

public:

	// вернуть true, если список пуст
	bool IsEmpty() const {
		return derivedTypeList.empty();
	}

	// если список производных типов начинается с функции
	bool IsFunction() const {
		return !derivedTypeList.empty() && 
			derivedTypeList[0]->GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE;
	}

	// если список производных типов начинается с массива
	bool IsArray() const {
		return !derivedTypeList.empty() && 
			derivedTypeList[0]->GetDerivedTypeCode() == DerivedType::DT_ARRAY;
	}

	// если список производных типов начинается с указателя
	bool IsPointer() const {
		return !derivedTypeList.empty() && 
			derivedTypeList[0]->GetDerivedTypeCode() == DerivedType::DT_POINTER;
	}

	// если список производных типов начинается с указателя на член
	bool IsPointerToMember() const {
		return !derivedTypeList.empty() && 
			derivedTypeList[0]->GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER;
	}

	// если список производных типов начинается с функции
	bool IsReference() const {
		return !derivedTypeList.empty() && 
			derivedTypeList[0]->GetDerivedTypeCode() == DerivedType::DT_REFERENCE;
	}


	// получить количество производных типов
	int GetDerivedTypeCount() const { 
		return derivedTypeList.size();
	}

	// получить производный тип по индексу,
	// индекс выходит за пределы, происходит внутренняя ошибка
	const PDerivedType &GetDerivedType( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > derivedTypeList.size()-1 );
		return derivedTypeList[ix];
	}

	// получить производный тип по индексу, если
	// индекс выходит за пределы, возвращается 0
	const PDerivedType operator[]( int ix ) const {
		return (ix < 0 || ix > derivedTypeList.size()-1) ? 
			PDerivedType(NULL) : derivedTypeList[ix];
	}

	// получить голову списка
	const PDerivedType GetHeadDerivedType() const {
		return this->operator[](0);
	}

	// получить конец списка
	const PDerivedType GetTailDerivedType() const {
		return IsEmpty() ? NULL : this->operator[](derivedTypeList.size()-1);
	}

	// вставить производный тип в начало
	void PushHeadDerivedType( PDerivedType dt ) {
		derivedTypeList.insert(derivedTypeList.begin(), dt);
	}

	// удалить производный тип с головы
	void PopHeadDerivedType() {
		derivedTypeList.erase(derivedTypeList.begin());
	}

	// добавить производный тип в список
	// метод не константный, поэтому может вызываться только при
	// построении списка
	void AddDerivedType( PDerivedType dt ) {
		derivedTypeList.push_back(dt);
	}

	// присоединить список производных типов к текущему
	void AddDerivedTypeList( const DerivedTypeList &dtl ) {
		derivedTypeList.insert( derivedTypeList.end(),
			dtl.derivedTypeList.begin(), dtl.derivedTypeList.end() );
	}

	// присоединить производный список типов с cv-квалификацией первого
	// производного типа, только если он '*', 'ptr-to-member', '()', 
	// при этом этот производный тип полностью копируетс
	bool AddDerivedTypeListCV( const DerivedTypeList &dtl, bool c, bool v );

	// очистить список с освобождением памяти
	void ClearDerivedTypeList() {
		derivedTypeList.clear();
	}
};


// представление сущности "массив". Если размер массива не задан,
// он равен -1
class Array : public DerivedType
{
	// указатель на выражение вычисляющее размер массива
	// если в массиве не задан размер, выражение == NULL
	int size;

public:
	// создание массива
	Array(int sz = -1) : DerivedType(DT_ARRAY), size(sz) {		
	}

	
	// вернуть true, если размер массива не задан
	bool IsUncknownSize() const { 
		return size < 1;
	}

	// получить размер массива. Вызывает внутреннюю ошибку, если
	// размер не задан. В массиве следует сначала проверить, задан ли
	// размер вообще
	int GetDerivedTypeSize() const {		
		return size;
	}
	
	// получить размер массива, предпочтительней использовать, 
	// чем  GetDerivedTypeSize, когда работаем с объектом типа Array
	int GetArraySize() const {
		return size;
	}

	// задать размер при инициализации
	void SetArraySize( int sz ) {
		size = sz;
	}
};


// представление сущности "указатель"
class Pointer : public DerivedType
{
	// квалификаторы указателя
	bool constQualifier, volatileQualifier;

public:

	// конструктор с заданием квалификаторов
	Pointer( bool cq, bool vq ) 
		: DerivedType(DT_POINTER), constQualifier(cq), volatileQualifier(vq) {
	}

	// получить значение константного квалификатора
	bool IsConst() const {
		return constQualifier;
	}

	// получить значение квалификатора volatile
	bool IsVolatile() const {
		return volatileQualifier;
	}

	// получить cv-квалификацию указателя
	int CV_Qualified() const {
		return (int)constQualifier + volatileQualifier;
	}
		
	// получить размер указателя, переопределяет виртуальный метод
	// базового класса
	int GetDerivedTypeSize() const {
		return DEFAULT_POINTER_SIZE;
	}
};


// представление сущности "указатель на член класса"
class PointerToMember : public DerivedType
{
	// указатель на класс к которому принадлежит указатель
	const ClassType *pClass;

	// квалификаторы указателя на член
	bool constQualifier, volatileQualifier;

public:

	// конструктор с заданием параметров указателя на член
	PointerToMember( const ClassType *p, bool cq, bool vq ) 
		: DerivedType(DT_POINTER_TO_MEMBER), pClass(p), constQualifier(cq), 
		volatileQualifier(vq) {

		INTERNAL_IF( pClass == NULL );
	}

	// получить значение константного квалификатора
	bool IsConst() const {
		return constQualifier;
	}

	// получить значение квалификатора volatile
	bool IsVolatile() const {
		return volatileQualifier;
	}

	// получить cv-квалификацию указателя
	int CV_Qualified() const {
		return (int)constQualifier + volatileQualifier;
	}

	// получить указатель на класс к которому принадлежит указатель
	const ClassType &GetMemberClassType() const {
		return *pClass;
	}
	
	// получить размер указателя, переопределяет виртуальный метод
	// базового класса
	int GetDerivedTypeSize() const {
		return DEFAULT_POINTER_TO_MEMBER_SIZE;
	}
};


// представление сущности "ссылка"
class Reference : public DerivedType
{
public:

	// задаем код
	Reference() : DerivedType(DT_REFERENCE) {
	}


	// получить размер ссылки
	int GetDerivedTypeSize() const {
		return DEFAULT_REFERENCE_SIZE;
	}
};


// объявлен далее, так как требуется предварительное объявление 
// типизированной сущности и объектов
class FunctionPrototype;


// этот класс необходим для объявления типизированной сущности,
// сам класс объявляется в модуле BaseType.h
class BaseType;


// класс типизированная сущность, является базовым для всех
// классов, которые содержат тип
class TypyziedEntity
{
	// указатель на базовый тип. Может быть встроенным типом, 
	// классом, перечислением, шаблонным параметром типа, шаблонным
	// параметром шаблона
	BaseType *baseType;

	// cv-квалификаторы
	bool constQualifier, volatileQualifier;

	// список производных типов типизированной сущности
	DerivedTypeList derivedTypeList;

public:	

	// в конструкторе должны задаваться параметры типипзированной сущности
	TypyziedEntity( BaseType *bt, bool cq, bool vq, const DerivedTypeList &dtl ) {		
		baseType = bt;
		constQualifier = cq;
		volatileQualifier = vq;
		derivedTypeList = dtl;

		INTERNAL_IF( baseType == NULL );
	}

	// деструктор освобождает память занятую списком производных типов
	virtual ~TypyziedEntity() {
		derivedTypeList.ClearDerivedTypeList();
	}

	// получить значение константного квалификатора
	bool IsConst() const {
		return constQualifier;
	}

	// получить значение квалификатора volatile
	bool IsVolatile() const {
		return volatileQualifier;
	}

	// получить cv-квалификацию типизированной сущности
	int CV_Qualified() const {
		return (int)constQualifier + volatileQualifier;
	}

	// функции инспекторы
	// ---
	// если объект имеет тип Literal, который является производным классом
	// TypyziedEntity. В классе Literal эта функция должна переопределяться
	// и возвращать true
	virtual bool IsLiteral() const {
		return false;
	}

	// если объект имеет тип Object
	virtual bool IsObject() const {
		return false;
	}

	// если объект имеет тип EnumConstant
	virtual bool IsEnumConstant() const {
		return false;
	}

	// если объект имеет тип Function
	virtual bool IsFunction() const {
		return false;
	}

	// если объект имеет тип Parametr
	virtual bool IsParametr() const {
		return false;
	}

	// если динамическая типизированная сущность
	virtual bool IsDynamicTypyziedEntity() const {
		return false;
	}

	// если объект имеет тип - не типизированный шаблонный параметр
	virtual bool IsNonTypeTemplateParametr() const {
		return false;
	}

	// получить базовый тип
	const BaseType &GetBaseType() const {
		return *baseType;
	}

	// получить список производных типов
	const DerivedTypeList &GetDerivedTypeList() const {
		return derivedTypeList;
	}

	// получить строковое представление типа
	CharString GetTypyziedEntityName( bool printName = true ) const;

private:

	// печатать префиксные производные типы и сохранять их в буфер
	void PrintPointer( string &buf, int &ix, bool &namePrint ) const ;

	// печатать постфиксные производные типы и сохранять в буфер
	void PrintPostfix( string &buf, int &ix ) const;
};


// класс - динамическая типизированная сущность. Используется
// в выражениях как оболочка для идентификаторов, которые динамически
// изменяют свой тип. Например член в константном методе становится константным
class DynamicTypyziedEntity : public TypyziedEntity
{
	// действительная сущность
	const TypyziedEntity &original;

public:
	// конструктор такой как у TypyziedEntity, только принимает ссылку на действительную
	// сущность
	DynamicTypyziedEntity( const BaseType &bt, bool cq, bool vq, 
		const DerivedTypeList &dtl, const TypyziedEntity &orig ) 
		: TypyziedEntity((BaseType*)&bt, cq, vq, dtl), original(orig) {		
	}

	// конструктор для удобства
	DynamicTypyziedEntity( const TypyziedEntity &cop, const TypyziedEntity &orig )
		: TypyziedEntity(cop),  original(orig) {		
	}

	// получить оригинал
	const TypyziedEntity &GetOriginal() const {
		return original;
	}

	// если динамическая типизированная сущность
	bool IsDynamicTypyziedEntity() const {
		return true;
	}
};


// класс объявляется в модуле Body
class Operand;


// класс представляет сущность - параметр функции
class Parametr : public Identifier, public TypyziedEntity
{
	// выражение вычисляющее значение параметра по умолчанию,
	// может быть NULL
	const Operand *defaultValue;

	// параметр может иметь спецификатор хранения register
	bool registerStorageSpecifier;

public:

	// в конструкторе задаются параметры класса
	Parametr( BaseType *bt, bool cq, bool vq, const DerivedTypeList &dtl, 
		const NRC::CharString &n, SymbolTable *p, const Operand *dv, bool rss ) : 
			Identifier(n, p), TypyziedEntity(bt, cq, vq, dtl) {

		defaultValue = dv;
		registerStorageSpecifier = rss;

		// задаем С-имя. Если имя сгенерировано как временное, задаем пустое
		// иначе оставляем как есть
		c_name = n[0] == '<' ? "" : n.c_str();
	}

	// в деструкторе уничтожается значение по умолчанию
	~Parametr() ;

	// если объект имеет тип Parametr
	bool IsParametr() const {
		return true;
	}

	// имеет ли параметр значение по умолчанию. В нашем случае, если
	// выражение задано
	bool IsHaveDefaultValue() const {
		return defaultValue != NULL;
	}
	
	// если параметр имеет спецификатор хранения register
	bool IsHaveRegister() const {
		return registerStorageSpecifier;
	}

	// если у параметра не задано имя
	bool IsUnnamed() const {
		return GetName().empty();
	}

	// если параметр ссылочный
	bool IsReferenced() const {
		return GetDerivedTypeList().IsReference();
	}

	// получить значение по умолчанию
	const Operand *GetDefaultValue() const {
		return defaultValue;
	}

	// задать значение по умолчанию в случае переопределения
	void SetDefaultValue( const Operand *dv ) {
		INTERNAL_IF( defaultValue != NULL );
		defaultValue = dv;
	}
};


// интеллектуальный указатель на типизированную сущность
typedef SmartPtr<TypyziedEntity> PTypyziedEntity;

// интеллектуальный указатель на параметр
typedef SmartPtr<Parametr> PParametr;


// список возбуждаемых функцией исключительных ситуаций
class FunctionThrowTypeList
{
	// список указателей на объекты типа TypyziedEntity
	vector<PTypyziedEntity> throwTypeList;

public:

	// вернуть true, если список пуст
	bool IsEmpty() const {
		return throwTypeList.empty();
	}

	// получить количество производных типов
	int GetThrowTypeCount() const { 
		return throwTypeList.size();
	}

	// получить тип исключительной ситуации по индексу,
	// индекс выходит за пределы, происходит внутренняя ошибка
	const PTypyziedEntity &GetThrowType( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > throwTypeList.size()-1 );
		return throwTypeList[ix];
	}

	// получить производный тип по индексу, если
	// индекс выходит за пределы, возвращается 0
	const PTypyziedEntity operator[]( int ix ) const {
		return (ix < 0 || ix > throwTypeList.size()-1) ? 
					PTypyziedEntity(NULL) : throwTypeList[ix];
	}

	// добавить тип в список
	// метод не константный, поэтому может вызываться только при
	// построении списка
	void AddThrowType( PTypyziedEntity &dt ) {
		throwTypeList.push_back(dt);
	}

	// очистить список с освобождением памяти
	void ClearThrowTypeList() {
		throwTypeList.clear();
	}
};


// список параметров функции
class FunctionParametrList
{
	// список указателей на объекты типа Parametr
	vector<PParametr> parametrList;

public:

	// вернуть true, если список пуст
	bool IsEmpty() const {
		return parametrList.empty();
	}

	// получить количество производных типов
	int GetFunctionParametrCount() const { 
		return parametrList.size();
	}

	// получить тип исключительной ситуации по индексу,
	// индекс выходит за пределы, происходит внутренняя ошибка
	const PParametr &GetFunctionParametr( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > parametrList.size()-1 );
		return parametrList[ix];
	}

	// получить производный тип по индексу, если
	// индекс выходит за пределы, возвращается 0
	const PParametr operator[]( int ix ) const {
		return (ix < 0 || ix > parametrList.size()-1) ? PParametr(NULL) : parametrList[ix];
	}

	// добавить тип в список
	// метод не константный, поэтому может вызываться только при
	// построении списка
	void AddFunctionParametr( PParametr dt ) {
		parametrList.push_back(dt);
	}

	// очистить список с освобождением памяти
	void ClearFunctionParametrList() {
		parametrList.clear();
	}

	// метод возвращает индекс параметра, если в списке имеется параметр с именем name
	int HasParametr( const NRC::CharString &name ) const {
		int i = 0;
		for( vector<PParametr>::const_iterator p = parametrList.begin(); 
			 p != parametrList.end(); p++, i++ )
			if( (*p)->GetName() == name ) 
				return i;
		return -1;
	}
};


// размер функции по умолчанию (отсутствует)
extern const unsigned int defualt_function_size ;


// представление сущности "прототип функции"
class FunctionPrototype : public DerivedType
{
	// квалификаторы прототипа функции
	bool constQualifier, volatileQualifier;

	// список параметров функции
	FunctionParametrList parametrList;

	// список возбуждаемых функцией исключительных ситуаций
	FunctionThrowTypeList throwTypeList;

	// true, если функция может возбуждать исключительные ситуации
	bool canThrowExceptions;

	// true, если прототип имеет '...'
	bool haveEllipse;

public:

	// конструктор с заданием параметров прототипа
	FunctionPrototype( bool cq, bool vq, const FunctionParametrList &fpl, 
		const FunctionThrowTypeList &fttl, bool cte, bool he ) 
		: DerivedType(DT_FUNCTION_PROTOTYPE), constQualifier(cq), volatileQualifier(vq) {

		parametrList = fpl;
		throwTypeList = fttl;
		canThrowExceptions = cte;
		haveEllipse = he;
	}

	
	// получить значение константного квалификатора
	bool IsConst() const {
		return constQualifier;
	}

	// получить значение квалификатора volatile
	bool IsVolatile() const {
		return volatileQualifier;
	}

	// получить cv-квалификацию указателя
	int CV_Qualified() const {
		return (int)constQualifier + volatileQualifier;
	}

	// имеет ли функция '...'
	bool IsHaveEllipse() const {
		return haveEllipse;
	}


	// может ли функция возбуждать исключительные ситуации
	bool CanThrowExceptions() const {
		return canThrowExceptions;
	}


	// получить список параметров
	const FunctionParametrList &GetParametrList() const {
		return parametrList;
	}

	// получить список исключительных ситуаций
	const FunctionThrowTypeList &GetThrowTypeList() const {
		return throwTypeList;
	}


	// получить размер прототипа функции
	int GetDerivedTypeSize() const {
		return DEFUALT_FUNCTION_SIZE;
	}
};


// константый литерал: символьный, целый, вещественный, строковый
class Literal : public TypyziedEntity
{
	// значение литерала 
	NRC::CharString value;

public:

	// конструктор с заданием параметров литерала
	Literal( BaseType *bt, bool cq, bool vq, const DerivedTypeList &dtl, 
		const NRC::CharString &v ) : TypyziedEntity(bt, cq, vq, dtl) {

		value = v;
	}

	// получить значение литерала
	const NRC::CharString &GetLiteralValue() const {
		return value;
	}

	// если целая константа, char, int с модификаторами. Подразумевается,
	// что char, wchar_t уже храняться в виде целого числа
	bool IsIntegerLiteral() const;

	// если вещественная 
	bool IsRealLiteral() const;

	// если строковый литерал
	bool IsStringLiteral() const;

	// если wide-строка
	bool IsWideStringLiteral() const;

	// объект является литералом
	virtual bool IsLiteral() const {
		return true;
	}
};


// класс представляет сущность объект. Объектом является сущность
// языка С++, под которую выделяется память, либо не выделяется 
// при наличии у объекта спецификаторов хранения extern, typedef
class Object : public Identifier, public TypyziedEntity, public ClassMember
{
public:
	// разновидность спецификаторов хранения для объекта и данного члена
	enum SS {
		SS_AUTO, SS_REGISTER, SS_EXTERN, SS_STATIC, SS_TYPEDEF, 
		SS_BITFIELD, SS_MUTABLE, SS_NONE,
	};

private:

	// спецификатор хранения
	SS storageSpecifier;

	// инициализируемое значение в случае если инициализатор константный
	// У битовых полей это размер
	const double  *pInitialValue;

	// установлен в true, если спецификация связывания у объекта 
	// задана как "extern C"
	bool clinkSpec;

public:

	// конструктор задает параметры объекта
	Object( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, SS ss, bool ls = false );

	// объект не может являться членом класса
	virtual bool IsClassMember() const {
		return false;
	}

	// если объект имеет тип Object
	virtual bool IsObject() const {
		return true;
	}

	// вернуть true, если объект имеет инициализируемое значение
	bool IsHaveInitialValue() const {
		return pInitialValue != NULL;
	}

	// вернуть true, если объект имеет спецификацию свзявания языка C
	bool IsCLinkSpecification() const {
		return clinkSpec;
	}

	// получить спецификатор хранения
	SS GetStorageSpecifier() const {
		return storageSpecifier;
	}	

	// получить указатель на инициализируемое значение, может
	// быть NULL
	const double *GetObjectInitializer() const {
		return pInitialValue;
	}

	// задать инициализатор объекта. Второй параметр обозначает,
	// что задается значение битового поля
	void SetObjectInitializer( double dv, bool bf = false ) {
		INTERNAL_IF( pInitialValue != NULL );
		pInitialValue = new double(dv);
		if( bf )
			storageSpecifier = SS_BITFIELD;
	}
};


// класс представляет сущность - данное-член класса 
class DataMember : public ::Object
{
	// спецификатор доступа к данному-члену
	AS	accessSpecifier;

public:
	// данное-член
	DataMember( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, SS ss, AS as ) 
		: Object(name, entry, bt, cq, vq, dtl, ss, false ), accessSpecifier(as) {
	}
	
	// если данное-член является статическим
	bool IsStaticMember() const {
		return GetStorageSpecifier() == SS_STATIC;
	}

	// если данное член является битовым полем
	bool IsBitField() const {
		return GetStorageSpecifier() == SS_BITFIELD;
	}

	// являться членом класса
	bool IsClassMember() const {
		return true;
	}
	
	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}

};


// класс объявляется в Statement.h
class FunctionBody;


// сущность функция. Функция, это объект, у которого в голове
// списка производных типов содержится производный тип FUNCTION_PROTOTYPE.
// Функция может содержать тело
class Function : public Identifier, public TypyziedEntity, public ClassMember
{
public:
	// возможные спецификаторы хранения функции
	enum SS {
		SS_STATIC, SS_EXTERN, SS_TYPEDEF, SS_NONE
	};

	// возможные правила вызова функции
	enum CC {
		CC_PASCAL, CC_CDECL, CC_STDCALL, CC_NON
	};

private:

	// спецификатор inline
	bool inlineSpecifier;

	// спецификатор хранения функции
	SS storageSpecifier;
	
	// правила вызова. Если функция задана как "extern C", то
	// правило вызова задано CC_CDECL
	CC callingConvention;

	// установлен в true, если у функции есть тело
	bool isHaveBody;

public:
	// конструктор задает необходимые параметры функции
	Function( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc ) ;

	// виртуальный деструктор для производных классов
	virtual ~Function() {
	}

	// если функция подставляемая, необязательно это выполняется физически,
	// просто если функция объявлена как inline, возвращает true
	bool IsInline() const {
		return inlineSpecifier;
	}

	// функция не возвращает значения
	bool IsProcedure() const; 

	// если функция имеет тело
	bool IsHaveBody() const {
		return isHaveBody;
	}

	// если функция, на самом деле является перегруженным оператором
	virtual bool IsOverloadOperator() const {
		return false;
	}
		
	// если функция на самом деле является шаблонной
	virtual bool IsTemplate() const {
		return false;
	}

	// если функция на самом деле является специализацией шаблонной функции
	virtual bool IsTemplateSpecialization() const {
		return false;
	}
	// имеет тип функции
	virtual bool IsFunction() const {
		return true;
	}

	// не член класса
	virtual bool IsClassMember() const {
		return false;
	}	

	// получить спецификатор хранения функции
	SS GetStorageSpecifier() const {
		return storageSpecifier;
	}

	// получить правило вызова
	CC GetCallingConvention() const {
		return callingConvention;
	}

	// получить прототип функции
	const FunctionPrototype &GetFunctionPrototype() const {
		return 
			static_cast<const FunctionPrototype &>(*GetDerivedTypeList().GetHeadDerivedType());				
	}

	// задать тело функции, тело может задаваться только один раз
	void SetFunctionBody( ) {
		INTERNAL_IF( isHaveBody );
		isHaveBody = true;
	}
};


// метод класса
class Method : public Function
{
public:
	// вид декларации метода: объявлено пользователем, сгенерировано
	// компилятором, тривиальный метод (используется для деструкторов,
	// конструкторов, операторов копирования), и недоступный метод,
	// используется только для оператора копирования, который не может быть
	// сгенерирован
	enum DT { DT_USERDEFINED, DT_IMPLICIT, DT_TRIVIAL, DT_UNAVAIBLE };
	
	// класс описывает информацию генерации для виртуального метода
	// класс корневого метода и 
	class VFData {
		// индекс в виртуальной таблице
		int vtIndex;
	
		// класс корневой виртуальной функции, если такой нет,
		// ссылка на собственный класс
		const ClassType &rootVfCls;

		// буфер с C-типом функции '(тип)', который используется
		// для приведения указателя на функцию в v-таблице при вызове
		string castBuf;

	public:
		// задать информацию извне
		VFData( int vtIndex, const Method &thisMeth );

		// вернуть индекс
		int GetVTableIndex() const {
			return vtIndex;
		}

		// вернуть класс корневой виртуальной функции
		const ClassType &GetRootVfClass() const {
			return rootVfCls;
		}

		// вернуть буфер С-типа функции
		const string &GetCastBuf() const {
			return castBuf;
		}
	};

private:
	// тип декларации
	DT declarationType;

	// спецификатор доступа в классе
	AS accessSpecifier;

	// если метод абстрактный - true
	bool abstractMethod;

	// если метод виртуальный - true
	bool virtualMethod;

	// если метод является деструктором - true
	bool destructorMethod;

	// указатель на информацию генерации для виртуального метода
	const VFData *vfData;

public:

	// задаем параметры метода
	Method( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool am, bool vm, bool dm, DT dt);

	// освобождаем память занятую информацией виртуального метода
	~Method();

	// если метод статический
	bool IsStaticMember() const {
		return GetStorageSpecifier() == SS_STATIC;
	}		

	// если метод абстрактный
	bool IsAbstract() const {
		return abstractMethod;
	}

	// если метод виртуальный (он виртуальный и если абстрактный)
	bool IsVirtual() const {
		return virtualMethod;
	}
	
	// если метод является деструктором
	bool IsDestructor() const {
		return destructorMethod;
	}

	// если метод объявлен пользователем
	bool IsUserDefined() const {
		return declarationType == DT_USERDEFINED;
	}

	// если метод тривиальный
	bool IsTrivial() const {
		return declarationType == DT_TRIVIAL;
	}

	// если метод не объявлен
	bool IsUnavaible() const {
		return declarationType == DT_UNAVAIBLE;
	}

	// если метод сгенерирован компилятором, но требует явной
	// генерации (не тривиальный)
	bool IsImplicit() const {
		return declarationType == DT_IMPLICIT;
	}

	// если метод является конструктором. Требует переопределения в ConstructorMethod
	virtual bool IsConstructor() const {
		return false;
	}

	//  член класса
	virtual bool IsClassMember() const {
		return true;
	}

	// получить спецификатор доступа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}

	// получить информацию о виртуальной функции. Может быть NULL
	const VFData &GetVFData() const {
		INTERNAL_IF( vfData == NULL );
		return *vfData;
	}

	// задать виртуальность методу, т.к. она может выясниться не сразу
	// после конструирования метода
	void SetVirtual( const Method *rootMeth );

	// задать абстрактность методу, т.к. абстрактность выясняется
	// после считывания '= 0'
	void SetAbstract( ) {
		abstractMethod = true;
	}
};


// конструткор - уточняем сущность Method
class ConstructorMethod : public Method
{
	// спецификатор явного вызова
	bool explicitSpecifier;

public:
	// задаем параметры конструктора
	ConstructorMethod( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool es, DT dt );

	// если конструктор требует явного вызова
	bool IsExplicit() const {
		return explicitSpecifier;
	}

	// метод является конструктором
	virtual bool IsConstructor() const {
		return true;
	}
};


// перегруженный оператор не член класса
class OverloadOperator : public Function
{
	// код оператора
	int opCode;

	// имя оператора. В случае оператора приведения вставляется полное
	// имя типа
	NRC::CharString opName;

public:

	// задаем параметры конструктора
	OverloadOperator( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, int opc, const NRC::CharString &opn );

	// если оператор является оператором приведения
	bool IsCastOperator() const {
		return opCode == 'c';
	}

	// получить код оператора
	int GetOperatorCode() const {
		return opCode;
	}

	// получить имя оператора
	const NRC::CharString &GetOperatorName() const {
		return opName;
	}

	// является перегруженным оператором
	virtual bool IsOverloadOperator() const {
		return true;
	}
};


// коды операторов, которые состоят из нескольких лексем,
enum OverloadOperatorCode
{
	OC_NEW_ARRAY,
	OC_DELETE_ARRAY,
	OC_FUNCTION,
	OC_ARRAY,
	OC_CAST,
};


// коды операторов, которые используются при генерации кода,
// для распознавания тех конструкций, которые явно не задаются в С++
enum GeneratorOperatorCode
{
	GOC_DERIVED_TO_BASE_CONVERSION = 1300,
	GOC_BASE_TO_DERIVED_CONVERSION,
	GOC_REFERENCE_CONVERSION,
};


// перегруженный оператор класса
class ClassOverloadOperator : public Method
{
	// код оператора
	int opCode;

	// имя оператора. В случае оператора приведения вставляется полное
	// имя типа
	NRC::CharString opName;

public:

	// конструктор задает все параметры вверх по иерархии
	ClassOverloadOperator(
		const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool am, bool vm, 
		int opc, const NRC::CharString &opn, DT dt );

	// если оператор является оператором приведения
	virtual bool IsCastOperator() const {
		return false;
	}

	// получить код оператора
	int GetOperatorCode() const {
		return opCode;
	}

	// получить имя оператора
	const NRC::CharString &GetOperatorName() const {
		return opName;
	}

	// перегружаем виртуальную функцию для устранения неоднозначности
	virtual bool IsOverloadOperator() const {
		return true;
	}	
};


// перегруженный оператор приведения типа
class ClassCastOverloadOperator : public ClassOverloadOperator
{
	// тип к которому приводит этот перегруженный оператор
	TypyziedEntity castType;

public:
	// конструктор принимает все параметры
	ClassCastOverloadOperator( 	const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool am, bool vm, 
		int opc, const NRC::CharString &opn, const TypyziedEntity &ctp, DT dt );

	// если оператор является оператором приведения
	bool IsCastOperator() const {
		return true;
	}

	// получить тип приведения
	const TypyziedEntity &GetCastType() const {
		return castType;
	}
};


// класс объявляется в модуле Class.h
class EnumType;


// класс представляющий константу перечисления, которая не является
// членом класса
class EnumConstant : public Identifier, public TypyziedEntity, public ClassMember
{
	// значение константы перечисления
	int value;

public:

	// конструктор задает те параметры константы перечисления, которые
	// ей необходимы
	EnumConstant( const NRC::CharString &name, SymbolTable *entry, 
		int v, EnumType *pEnumType );

	// виртуальный деструктор для производных классов
	virtual ~EnumConstant() { 
	}


	// константа не является членом класса, для этого существует класс
	// ClassEnumConstant
	virtual bool IsClassMember() const {
		return false;
	}

	// если объект имеет тип EnumConstant
	virtual bool IsEnumConstant() const {
		return true;
	}

	// получить целое значение константы
	// функцию можно вызывать, только если значение шаблонно независимо
	int GetConstantValue() const {
		return value;
	}

	// получить перечислимый тип к которому принадлежит константа
	const EnumType &GetEnumType() const ;
};


// константа перечисления принадлежащая классу
class ClassEnumConstant : public EnumConstant
{
	// спецификатор доступа константы
	AS	accessSpecifier;

public:

	// задать параметры константы, а также спецификатор доступа
	ClassEnumConstant( const NRC::CharString &name, SymbolTable *entry, 
		int val, EnumType *pEnumType, AS as ) : EnumConstant(name, entry, val, pEnumType),
		accessSpecifier(as) {
	}

	// является членом класса
	virtual bool IsClassMember() const {
		return true;
	}

	// получить спецификатор досупа
	virtual AS GetAccessSpecifier() const {
		return accessSpecifier;
	}
};


// интеллектуальный указатель
typedef SmartPtr<EnumConstant> PEnumConstant;

// список констант перечисления. Класс может содержать как константы перечисления,
// так и константы перечисления, которые являются членами класса
class EnumConstantList
{
	// список указателей на объекты типа EnumConstant или производные от него
	vector<PEnumConstant> enumConstantList;

public:

	// вернуть true, если список пуст
	bool IsEmpty() const {
		return enumConstantList.empty();
	}

	// получить количество констант перечисления
	int GetEnumConstantCount() const { 
		return enumConstantList.size();
	}

	// получить константу перечисления по индексу,
	// если она существует
	const PEnumConstant &GetEnumConstant( int ix ) const {
		INTERNAL_IF( ix < 0 || ix > enumConstantList.size()-1 );
		return enumConstantList[ix];
	}

	// получить константу перечисления, если она не существует
	// возвращается NULL
	PEnumConstant operator[]( int ix ) const {
		return (ix < 0 || ix > enumConstantList.size()-1) ? NULL : enumConstantList[ix];
	}

	// добавить константу в список
	// метод не константный, поэтому может вызываться только при
	// построении списка
	void AddEnumConstant( PEnumConstant dt ) {
		enumConstantList.push_back(dt);
	}

	// очистить список с освобождением памяти
	void ClearEnumConstantList() {
		enumConstantList.clear();
	}

	// метод возвращает индекс константы перечисления, 
	// если в списке имеется константа перечисления с именем name
	int HasEnumConstant( const NRC::CharString &name ) const {
		int i = 0;
		for( vector<PEnumConstant>::const_iterator p = enumConstantList.begin(); 
			 p != enumConstantList.end(); p++, i++ )
			if( (*p)->GetName() == name ) 
				return i;
		return -1;
	}
};



#endif // end  _OBJECT_H_INCLUDE
