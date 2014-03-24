// реализиация интерфейсов КЛАССОВ-МЕНЕДЖЕРОВ - Manager.cpp

#pragma warning(disable: 4786)
#include <nrc.h>

using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "LexicalAnalyzer.h"
#include "Object.h"
#include "Scope.h"
#include "Class.h"
#include "Parser.h"
#include "Manager.h"
#include "Maker.h"
#include "Checker.h"
#include "Overload.h"  


// поиск using-идентификатора по указателю на оригинальный идентификатор
const UsingIdentifier *SynonymList::find_using_identifier( const Identifier *id ) const
{
	for( register const_iterator p = begin(); p != end(); p ++ )
	{
		if( (*p).second != R_USING_IDENTIFIER )
			continue;

		const UsingIdentifier *ui = static_cast<const UsingIdentifier *>((*p).first);
		if( &ui->GetUsingIdentifier() == id )
			return ui;
	}

	return NULL;
}


// конструктор принимает запрос, и заполняет список ролей
// согласно запросу. 
// qn - имя (запрос), bt - если задано, область видимости в которой следует
// искать имя, watchFriend - производить поиск также и по дружеским областям,
// для классов - базовые классы, для других - using области
NameManager::NameManager( const CharString &qn, const SymbolTable *bt, bool watchFriend )
		: queryName(qn), bindTable(bt)
{
	// результирующая строка
	IdentifierList foundList;

	// если задана область видимости в которой следует производить поиск
	if( bindTable != NULL )
	{
		// производить поиск с учетом дружеских областей видимости
		if( watchFriend )
			bindTable->FindSymbol( queryName, foundList );

		// производим поиск только в данной области, не учитывая дружеские
		// ОВ		
		else
			bindTable->FindInScope( queryName, foundList );
		
	}
	
	// в противном случае - глубокий поиск по всем областям 
	// видимости до нахождения первого соответствия, начиная с текущего
	else
		theApp.GetTranslationUnit().GetScopeSystem().DeepSearch(queryName, foundList);

	// проверяем найдено ли соотв.
	if( foundList.empty() )
		return;

	// иначе заполняем список out, парами идентификатор-роль
	for( IdentifierList::iterator p = foundList.begin(); p != foundList.end() ; ++p )
	{		
		Identifier *id = const_cast<Identifier *>(*p);
		INTERNAL_IF( id == NULL );

		// создаем пару
		Role role = GetIdentifierRole(id);

		// если имеем роль - using-идентификатор, следует получить
		// указатель на действительный декларатор
		if( role == R_USING_IDENTIFIER )
		{
			// помещаем синоним в список синонимов для возможности проверки доступа
			synonymList.push_back( RolePair(id, role) );
			id = const_cast<Identifier *>(
					&static_cast<UsingIdentifier *>(id)->GetUsingIdentifier());
			role = GetIdentifierRole(id);
			INTERNAL_IF( role == R_USING_IDENTIFIER );
		}
		
		// если имеем синоним области видимости, преобразуем его в область видимости
		else if( role == R_NAMESPACE_ALIAS )
		{
			synonymList.push_back( RolePair(id, role) );
			id = const_cast<NameSpace *>(&static_cast<NameSpaceAlias *>(id)->GetNameSpace()); 
			role = R_NAMESPACE;
		}

		// это приводит к неверной генерации кода
		this->roleList.push_back( RolePair(id, role) );
	}
}

	
// получить роль идентификатора
Role NameManager::GetIdentifierRole( const Identifier *id ) 
{
	INTERNAL_IF( id == NULL );

	// определеяем к какому классу относится id
	// если объект
	if( const ::Object *obj = dynamic_cast<const ::Object *>(id) )
		return obj->IsClassMember() ? R_DATAMEMBER : R_OBJECT;		// может быть данное-член
		
	// иначе если функция
	else if( const Function *fn = dynamic_cast<const Function *>(id) )
	{
		// если шаблонная функция, неважно - конструктор, метод,
		// или перегруженный оператор
		if( fn->IsTemplate() )
			return R_TEMPLATE_FUNCTION;

		// специализация шаблонного класса, конструктора, метода и т.д.
		else if( fn->IsTemplateSpecialization() )
			return R_TEMPLATE_FUNCTION_SPECIALIZATION;	
		
		// если метод
		else if( fn->IsClassMember() )
		{
			// перегруженный оператор класса
			if( fn->IsOverloadOperator() )
				return R_CLASS_OVERLOAD_OPERATOR;

			// если конструктор
			else if( dynamic_cast<const ConstructorMethod *>(fn) != NULL )
				return R_CONSTRUCTOR;

			// если метод
			else
				return R_METHOD;
		}

		// иначе функция, которая может быть перегруженным оператором
		else
		{
			if( fn->IsOverloadOperator() )
				return R_OVERLOAD_OPERATOR;
			else
				return R_FUNCTION;
		}
	}

	// иначе если класс
	else if( const ClassType *cls = dynamic_cast<const ClassType *>(id) )
	{
		// тип объединения
		if( cls->GetBaseTypeCode() == BaseType::BT_UNION )
			return R_UNION_CLASS_TYPE;			

		else
			return R_CLASS_TYPE;
	}	

	// иначе если тип перечисления
	else if( const EnumType *enumT = dynamic_cast<const EnumType *>(id) )
		return R_ENUM_TYPE;
	
	
	// иначе если шаблонный класс
	else if( const TemplateClassType *tmptCls = dynamic_cast<const TemplateClassType *>(id) )
		return R_TEMPLATE_CLASS;
	
	// иначе если шаблонный параметр
	else if( const TemplateParametr *param = dynamic_cast<const TemplateParametr *>(id) )
	{		
		if( param->GetTemplateParametrType() == TemplateParametr::TP_TYPE )
			return R_TEMPLATE_TYPE_PARAMETR;

		else if( param->GetTemplateParametrType() == TemplateParametr::TP_NONTYPE  )
			return R_TEMPLATE_NONTYPE_PARAMETR;

		else
			return R_TEMPLATE_TEMPLATE_PARAMETR;
	}

	// иначе если именованная область видимости
	else if( const NameSpace *ns = dynamic_cast<const NameSpace *>(id) )
		return R_NAMESPACE;

	// иначе если параметр функции
	else if( const Parametr *param = dynamic_cast<const Parametr *>(id) )
		return R_PARAMETR;


	// иначе если константа перечисления
	else if( const EnumConstant *ecnst = dynamic_cast<const EnumConstant *>(id) )
		return ecnst->IsClassMember() ? R_CLASS_ENUM_CONSTANT : R_ENUM_CONSTANT;

	// иначе если using-идентификатор
	else if( const UsingIdentifier *ui = dynamic_cast<const UsingIdentifier *>(id) )
		return R_USING_IDENTIFIER;

	// иначе если синоним области видимости
	else if( const NameSpaceAlias *nsa = dynamic_cast<const NameSpaceAlias *>(id) )
		return R_NAMESPACE_ALIAS;

	// иначе неизвестно
	else
		return R_UNCKNOWN;
}


// если имя является типом
bool NameManager::IsTypeName() const
{
	return AmbiguityChecker(GetRoleList()).IsTypeName(false) != NULL;
}


// если имя является типом typedef
bool NameManager::IsTypedef() const
{
	return AmbiguityChecker(GetRoleList()).IsTypedef() != NULL;
}


// если имя является типом
bool QualifiedNameManager::IsTypeName() const
{
	return AmbiguityChecker(GetRoleList()).IsTypeName(false) != NULL;
}


// если имя является типом typedef
bool QualifiedNameManager::IsTypedef() const
{
	return AmbiguityChecker(GetRoleList()).IsTypedef() != NULL;
}


// создать менеджер составного имени,
// можно задать область видимости, в которой следует искать имя. 	 
// Эта функция используется для поиска среди составных имен, хотя может
// использоваться и для одиночных, в случае если np содержит только
// один под пакет. np - должен иметь заголовок PC_QUALIFIED_NAME или PC_QUALIFIED_TYPENAME
QualifiedNameManager::QualifiedNameManager( const NodePackage *np, const SymbolTable *bt )
	: queryPackage(np), bindTable(bt)
{   
	// проверка корректности входных параметров
	INTERNAL_IF( np == NULL || (np->GetPackageID() != PC_QUALIFIED_NAME &&
		np->GetPackageID() != PC_QUALIFIED_TYPENAME) );

	// если пакетов нет, то и делать ничего не надо
	if( np->GetChildPackageCount() == 0 )
		return;

	// если мы имеем всего один пакет
	if( np->GetChildPackageCount() == 1 )
	{
		// возможно был передан пакет с ошибкой
		if( np->GetChildPackage(0)->GetPackageID() == PC_ERROR_CHILD_PACKAGE )
			return;

		// в противном случае пакет должен содержать лексему с кодом NAME
		register int pid = np->GetChildPackage(0)->GetPackageID() ;
		INTERNAL_IF( pid != NAME && pid != PC_OVERLOAD_OPERATOR && pid != PC_CAST_OPERATOR &&
			 pid != PC_DESTRUCTOR );

		CharString nam = GetPackageName(*np->GetChildPackage(0));			

		// получаем роли для одиночного имени
		NameManager nm( nam, this->bindTable );
		roleList = nm.GetRoleList();
		synonymList = nm.GetSynonymList();
		return;
	}

	// иначе имеем составное имя, которое следует извлечь из пакета
	// используя класс NameManager
	// ---  ШАБЛОННЫЕ КЛАССЫ В КАЧЕСТВЕ ОБЛАСТЕЙ ВИДИМОСТИ НЕ РАССМАТРИВАЮТСЯ,
	// ---  ИХ ПОДДЕРЖКУ СЛЕДУЕТ ДОБАВИТЬ, КОГДА БУДЕТ ИЗВЕСТНА САМА СИСТЕМА
	// ---  ОБРАБОТКИ ШАБЛОНОВ

	const SymbolTable *entry = NULL;	// стартовая таблица символов с которой начинаем поиск
	int i = 0;						// индекс пакета с которого начинается цикл обработки

	// ошибки возникающие при обработке составного имени возникают когда
	// имя не является областью видимости. для перехвата этих ошибок мы и создаем try-блок
	try {

	// если код пакета - '::', произведем поиск первого имени в 
	// глобальной области видимости
	if( np->GetChildPackage(0)->GetPackageID() == COLON_COLON )
	{
		// следующей лексемой должен быть идентификатор
		register int pid = np->GetChildPackage(1)->GetPackageID() ;
		INTERNAL_IF( pid != NAME && pid != PC_OVERLOAD_OPERATOR && pid != PC_CAST_OPERATOR &&
			 pid != PC_DESTRUCTOR );

		// получаем глобальную область видимости
		const SymbolTable *globalST = GetScopeSystem().GetFirstSymbolTable();

		// производим поиск имени в глобальной области видимости без учета
		// дружеских областей видимости
		NameManager nm( GetPackageName(*np->GetChildPackage(1)), globalST, false );

		// далее если пакета всего 2, то имя может и не быть областью видимости,
		// оно является конечным, в противном случае имя должно быть областью видимости
		if( np->GetChildPackageCount() == 2 )
		{
			roleList = nm.GetRoleList();
			synonymList = nm.GetSynonymList();
			qualifierList.AddSymbolTable(globalST);		// добавляем ОВ учавствующую в поиске
			return;
		}

		else
		{
			entry = IsSymbolTable(nm);
			if( !entry )
				throw 1;	// ошибка, имя не является областью видимости
			qualifierList.AddSymbolTable(entry);
			i = 2;			// цикл обработки начнется со второго пакета
		}
	}

	// в противном случае поиск следует производить с текущей области видимости,
	// до первого соответствия
	else
	{
		// первым пакетом в нашем случае должно быть имя
		INTERNAL_IF( np->GetChildPackage(0)->GetPackageID() != NAME );
		NameManager nm( GetPackageName(*np->GetChildPackage(0)), bindTable );		

		entry = IsSymbolTable(nm);
		if( !entry )
			throw 0;
		qualifierList.AddSymbolTable(entry);
		i = 1;
	}

		
	// цикл спецификации имени: сохраняем области видимости из
	// пакетов в список и получаем список ролей последнего имени
	// 'i' задается выше, при считывании начальной области видимости 
	for( ;; )
	{
		// пакет должен быть '::' и следом за ним должно идти имя
		INTERNAL_IF( np->GetChildPackage(i)->GetPackageID() != COLON_COLON );	
		i++;
		register int pid = np->GetChildPackage(i)->GetPackageID();
		INTERNAL_IF( i == np->GetChildPackageCount() || 
			(pid != NAME && pid != PC_OVERLOAD_OPERATOR && pid != PC_CAST_OPERATOR &&
			 pid != PC_DESTRUCTOR) );
		
		// получаем имя из последней полученной области видимости,
		// причем поиск нового имени производится именно в ней, без учета дружеских
		// областей видимости
		CharString name = GetPackageName(*np->GetChildPackage(i));
		const SymbolTable *lastSt = 
			&qualifierList.GetSymbolTable(qualifierList.GetSymbolTableCount()-1);

		// есть один момент. Если lastSt - классовая область видимости и 
		// name - такое же как имя класса, просто возвращаем список конструкторов
		// класса и выходим
		if( lastSt->IsClassSymbolTable() && 
			i == np->GetChildPackageCount()-1 &&
			static_cast<const ClassType *>(lastSt)->GetName() == name )
		{
			INTERNAL_IF( !roleList.empty() );

			// загружаем список конструкторов в список ролей
			const ConstructorList &cl =
				static_cast<const ClassType *>(lastSt)->GetConstructorList();
			for( ConstructorList::const_iterator p = cl.begin(); p != cl.end(); p++ )
				roleList.push_back( RolePair((ConstructorMethod*)*p, R_CONSTRUCTOR) );

			// если список пуст, ошибка
			if( cl.empty() )
				theApp.Error(
					ParserUtils::GetPackagePosition((NodePackage*)np->GetChildPackage(i)),
					"'%s' - конструктор еще не объявлен", name.c_str());
			break;
		}

		NameManager stm( name, lastSt, false );		// получаем список ролей имени
	
		// если мы имеем последний идентификатор, т.е.
		// то, что нам нужно в конечном итоге найти - получаем список 
		// его ролей и выходим. В случае если у имени нет ролей (не найдено),
		// все равно цикл обработки считается успешным, но выводится ошибка
		if( i == np->GetChildPackageCount()-1 )
		{
			roleList = stm.GetRoleList();
			synonymList = stm.GetSynonymList();
			if( roleList.empty() )
				theApp.Error( 
					ParserUtils::GetPackagePosition((NodePackage*)np->GetChildPackage(i)),
					"'%s' - идентификатор не найден в области видимости '%s'",
					name.c_str(),
					dynamic_cast<const Identifier *>(lastSt)->GetQualifiedName().c_str() );
			
			break;	// цикл окончен
		}

		// иначе имя должно быть областью видимости, добавляем ее в 
		// список квалификаторов имени
		else if( const SymbolTable *st = IsSymbolTable(stm) )
			qualifierList.AddSymbolTable(st);				

		// иначе возбуждаем исключительную ситуацию для вывода ошибки
		else
			throw i;	
		i++;
	}

	// ошибки типа имя не является областью видимости, в параметре
	// индекс пакета в котором содержится имя
	} catch( int pkgIx ) {

		// освобождаем память занятую списками и выводим ошибку
		roleList.clear();
		qualifierList.Clear();
		synonymList.clear();

		LexemPackage *lp = (LexemPackage *)np->GetChildPackage(pkgIx);
		theApp.Error( lp->GetLexem().GetPos(), 
			"'%s' не является квалификатором области видимости",
			lp->GetLexem().GetBuf().c_str() );
	}	
}

// проверить, является ли имя областью видимости. Если является - возвращает 
// указатель на нее, в противном случае - NULL
const SymbolTable *QualifiedNameManager::IsSymbolTable( const NameManager &nm ) const 
{
	// имя должно быть классом, именованованной областью видимости,
	// либо typedef - типом, который определяет класс
	const SymbolTable *rval = NULL;
	AmbiguityChecker achk(nm.GetRoleList(), ParserUtils::GetPackagePosition(queryPackage), true);

	if( const ClassType *cls = achk.IsClassType(true) )
		return cls;

	else if( const NameSpace *ns = achk.IsNameSpace() )
		return ns;

	// в typedef'е, может быть класс
	else if( const ::Object *td = achk.IsTypedef() )
	{
		if( const ClassType *cls = CheckerUtils::TypedefIsClass(*td) )
			return cls;
		return NULL;
	}
	
	else
		return NULL;
}


// возвращает имя пакета. Пакет может иметь код NAME, PC_OVERLOAD_OPERATOR,
// PC_CAST_OPERATOR, PC_DESTRUCTOR. В последних двух случаях вызывается 
// функция-строитель для получения корректного имени идентификатора
CharString QualifiedNameManager::GetPackageName( const Package &pkg )
{
	register int pid = pkg.GetPackageID();

	if( pid == NAME )	
		return static_cast<const LexemPackage &>(pkg).GetLexem().GetBuf();	

	else if( pid == PC_OVERLOAD_OPERATOR )
	{
		const NodePackage &np = static_cast<const NodePackage &>(pkg);
		TempOverloadOperatorContainer tooc;
		MakerUtils::AnalyzeOverloadOperatorPkg( np, tooc);
		return tooc.opFullName;
	}

	else if( pid == PC_CAST_OPERATOR )
	{
		const NodePackage &np = static_cast<const NodePackage &>(pkg);
		TempCastOperatorContainer tcoc;
		MakerUtils::AnalyzeCastOperatorPkg( np, tcoc);
		return tcoc.opFullName;
	}

	else if( pid == PC_DESTRUCTOR )
	{
		const NodePackage &np = static_cast<const NodePackage &>(pkg);
		INTERNAL_IF( np.GetChildPackageCount() != 2 || 
			np.GetChildPackage(0)->GetPackageID() != '~' || 
			np.GetChildPackage(1)->GetPackageID() != NAME );
		
		string dn =
			static_cast<const LexemPackage &>(*np.GetChildPackage(1)).GetLexem().GetBuf().c_str();	
		return ('~' + dn).c_str();
	}

	else
		INTERNAL( 
			"'QualifiedNameManager::GetPackageName' принимает пакет с некорректным кодом");
	return "";
}


// менеджер встроенных типов. В параметрах код типа, код модификаторов,
// если модификаторы не заданы они равны -1
ImplicitTypeManager::ImplicitTypeManager( int lcode, int msgn, int msz )
{
	// структура для перевода из кодов лексем во
	// внутренние коды базовых типов
	struct LxmToBT
	{
		// внутренний код
		BaseType::BT btCode;
		
		// код лексемы
		int lxmCode;
	} codes[] = {
		BaseType::BT_BOOL,    KWBOOL,
		BaseType::BT_CHAR,    KWCHAR,
		BaseType::BT_WCHAR_T, KWWCHAR_T,
		BaseType::BT_INT,     KWINT,
		BaseType::BT_FLOAT,   KWFLOAT,
		BaseType::BT_DOUBLE,  KWDOUBLE,
		BaseType::BT_VOID,	  KWVOID
	};

	baseTypeCode = BaseType::BT_NONE;
	for( int i = 0; i< sizeof(codes)/ sizeof(LxmToBT) ; i++ )
		if( codes[i].lxmCode == lcode ) 
		{
			baseTypeCode = codes[i].btCode;			
			break;
		}

	// код базового типа должен быть задан
	INTERNAL_IF( baseTypeCode == BaseType::BT_NONE );
	modSign = BaseType::MN_NONE;
	modSize = BaseType::MZ_NONE;

	// задаем модификатор знака
	if( msgn != -1 )
	{
		INTERNAL_IF( msgn != KWSIGNED && msgn != KWUNSIGNED );
		modSign = (msgn == KWUNSIGNED ? BaseType::MN_UNSIGNED : BaseType::MN_SIGNED);
	}

	// задаем модификатор размера
	if( msz != -1 )
	{
		INTERNAL_IF( msz != KWSHORT && msz != KWLONG );
		modSize = (msz == KWSHORT ? BaseType::MZ_SHORT : BaseType::MZ_LONG);
	}
}

// конструктор для уже созданного типа
ImplicitTypeManager::ImplicitTypeManager( const BaseType &bt )
{
	baseTypeCode = bt.GetBaseTypeCode();
	modSign = bt.GetSignModifier();
	modSize = bt.GetSizeModifier();
}


// по коду, возвращает указатель на уже созданный базовый тип
const BaseType &ImplicitTypeManager::GetImplicitType()
{
	// инкапсулированная таблица базовых типов
	static BaseType *btTable[] = {
		new BaseType(BaseType::BT_BOOL),
		new BaseType(BaseType::BT_CHAR),
		new BaseType(BaseType::BT_CHAR, BaseType::MZ_NONE, BaseType::MN_UNSIGNED),
		new BaseType(BaseType::BT_CHAR, BaseType::MZ_NONE, BaseType::MN_SIGNED),
		new BaseType(BaseType::BT_WCHAR_T),
		new BaseType(BaseType::BT_INT),
		new BaseType(BaseType::BT_INT, BaseType::MZ_SHORT),
		new BaseType(BaseType::BT_INT, BaseType::MZ_NONE, BaseType::MN_UNSIGNED),
		new BaseType(BaseType::BT_INT, BaseType::MZ_NONE, BaseType::MN_SIGNED ),
		new BaseType(BaseType::BT_INT, BaseType::MZ_LONG),
		new BaseType(BaseType::BT_INT, BaseType::MZ_SHORT, BaseType::MN_UNSIGNED ),
		new BaseType(BaseType::BT_INT, BaseType::MZ_SHORT, BaseType::MN_SIGNED ),
		new BaseType(BaseType::BT_INT, BaseType::MZ_LONG,  BaseType::MN_UNSIGNED),
		new BaseType(BaseType::BT_INT, BaseType::MZ_LONG,  BaseType::MN_SIGNED ),
		new BaseType(BaseType::BT_FLOAT),
		new BaseType(BaseType::BT_DOUBLE),
		new BaseType(BaseType::BT_DOUBLE, BaseType::MZ_LONG),
		new BaseType(BaseType::BT_VOID),

	};


	if( baseTypeCode != BaseType::BT_INT && baseTypeCode != BaseType::BT_CHAR )	
		modSign = BaseType::MN_NONE;		

	if( baseTypeCode != BaseType::BT_INT && baseTypeCode != BaseType::BT_DOUBLE )
		modSize = BaseType::MZ_NONE;

	for( int i = 0; i<sizeof(btTable)/sizeof(BaseType*); i++ )
		if( btTable[i]->GetBaseTypeCode() == baseTypeCode &&
			btTable[i]->GetSizeModifier() == modSize	  &&
			btTable[i]->GetSignModifier() == modSign	  )
			return *btTable[i];
		
	INTERNAL( "код базового типа не рассматривается в методе 'GetImplicitType'" );
	return *new BaseType(BaseType::BT_NONE);	// убить предупр.
}


// получить размер типа
int ImplicitTypeManager::GetImplicitTypeSize() const 
{
	switch( baseTypeCode )
	{
	case BaseType::BT_BOOL:	
		return BOOL_TYPE_SIZE;

	case BaseType::BT_CHAR:
		return CHAR_TYPE_SIZE;

	case BaseType::BT_WCHAR_T:
		return WCHAR_T_TYPE_SIZE;

	case BaseType::BT_INT:	
		if( modSize == BaseType::MZ_SHORT )
			return SHORT_INT_TYPE_SIZE;

		else if( modSize == BaseType::MZ_LONG )
			return LONG_INT_TYPE_SIZE;

		else
			return INT_TYPE_SIZE;

	case BaseType::BT_FLOAT:
		return FLOAT_TYPE_SIZE;

	case BaseType::BT_DOUBLE:
		return modSize == BaseType::MZ_LONG ? LONG_DOUBLE_TYPE_SIZE : DOUBLE_TYPE_SIZE;

		// размер типа void не должен братся, на совести вызывающей функции
	case BaseType::BT_VOID:		
		return VOID_TYPE_SIZE;
	
		// в противном случае, внутренняя ошибка
	default:
		INTERNAL( "код базового типа не рассматривается в методе 'GetImplicitTypeSize'" );
	}

	return -1;		// убить warning
}

// получить строковое представление имени
CharString ImplicitTypeManager::GetImplicitTypeName() const 
{
	switch( baseTypeCode )
	{
	case BaseType::BT_BOOL:	
		return "bool";

	case BaseType::BT_CHAR:
		if( modSign == BaseType::MN_SIGNED )
			return "signed char";
		
		else if( modSign == BaseType::MN_UNSIGNED )
			return "unsigned char";

		else
			return "char";

	case BaseType::BT_WCHAR_T:
		return "wchar_t";

	case BaseType::BT_INT:	
		{
			CharString intnam;
			
			if( modSign == BaseType::MN_SIGNED )
				intnam += "signed ";
		
			else if( modSign == BaseType::MN_UNSIGNED )
				intnam += "unsigned ";

			if( modSize == BaseType::MZ_SHORT )
				intnam += "short ";

			else if( modSize == BaseType::MZ_LONG )
				intnam += "long "; 
			
			intnam += "int";
			return intnam;
		}

	case BaseType::BT_FLOAT:
		return "float";

	case BaseType::BT_DOUBLE:
		return modSize == BaseType::MZ_LONG ? "long double" : "double";

		// размер типа void не должен братся, на совести вызывающей функции
	case BaseType::BT_VOID:		
		return "void";
	
		// в противном случае, внутренняя ошибка
	default:
		INTERNAL( "код базового типа не рассматривается в методе 'GetImplicitTypeName'" );
	}

	return CharString();		// убить warning
}


// конструктор определяет к какой группе относится код
TypeSpecifierManager::TypeSpecifierManager( int c ) : code(c), group(TSG_UNCKNOWN)
{
	if( c == KWBOOL || c == KWCHAR  || c == KWWCHAR_T ||
		c == KWINT  || c == KWFLOAT || c == KWDOUBLE  || c == KWVOID )
		group = TSG_BASETYPE;

	else if( c == KWCLASS || c == KWSTRUCT || c == KWUNION || c == KWENUM )
		group = TSG_CLASSSPEC;

	else if( c == KWCONST ||  c == KWVOLATILE )
		group = TSG_CVQUALIFIER;

	else if( c == KWUNSIGNED || c == KWSIGNED )
		group = TSG_SIGNMODIFIER;

	else if( c == KWSHORT || c == KWLONG )
		group = TSG_SIZEMODIFIER;

	else if( c == KWAUTO || c == KWEXTERN || c == KWSTATIC || 
		c == KWREGISTER || c == KWTYPEDEF || c == KWMUTABLE )
		group = TSG_STORAGESPEC;

	else if( c == KWFRIEND )
		group = TSG_FRIEND;

	else if( c == KWINLINE || c == KWVIRTUAL || c == KWEXPLICIT )
		group = TSG_FUNCTIONSPEC;
}


// получить имя группы на русском языке
CharString TypeSpecifierManager::GetGroupNameRU() const
{
	switch( group )
	{	
	case TSG_BASETYPE:		return "базовый тип";
	case TSG_CLASSSPEC:		return "спецификатор класса";
	case TSG_CVQUALIFIER:	return "cv-квалификатор";
	case TSG_SIGNMODIFIER:	return "модификатор знака";
	case TSG_SIZEMODIFIER:	return "модификатор размера";
	case TSG_STORAGESPEC:	return "спецификатор хранения";
	case TSG_FRIEND:		return "спецификатор дружбы";
	case TSG_FUNCTIONSPEC:	return "спецификатор функции";
	}

	return "<неизвестная группа>";
}


// получить имя группы на английском
CharString TypeSpecifierManager::GetGroupNameENG() const
{
	switch( group )
	{	
	case TSG_BASETYPE:		return "base type";
	case TSG_CLASSSPEC:		return "class specifier";
	case TSG_CVQUALIFIER:	return "cv-qualifier";
	case TSG_SIGNMODIFIER:	return "sign modifier";
	case TSG_SIZEMODIFIER:	return "size modifier";
	case TSG_STORAGESPEC:	return "storage specifier";
	case TSG_FRIEND:		return "friend specifier";
	case TSG_FUNCTIONSPEC:	return "function specifier";
	}

	return "<uncknown group>";
}


// вернуть базовый тип, в случае если спецификатор является базовым типом	
BaseType::BT TypeSpecifierManager::CodeToBaseType() const 
{
	INTERNAL_IF( !IsBaseType() );
	return ImplicitTypeManager(code).GetImplicitTypeCode();
}


// вернуть спецификатор класса
BaseType::BT TypeSpecifierManager::CodeToClassSpec() const
{
	INTERNAL_IF( !IsClassSpec() );		
	if( code == KWCLASS )
		return BaseType::BT_CLASS;

	else if( code == KWSTRUCT )
		return BaseType::BT_STRUCT;

	else if( code == KWUNION )
		return BaseType::BT_UNION;

	else 
		return BaseType::BT_ENUM;

}

// вернуть модификатор знака
BaseType::MSIGN TypeSpecifierManager::CodeToSignModifier() const
{
	INTERNAL_IF( !IsSignModifier() );
	return code == KWSIGNED ? BaseType::MN_SIGNED : BaseType::MN_UNSIGNED;			
}

// вернуть модификатор размера
BaseType::MSIZE TypeSpecifierManager::CodeToSizeModifier() const 
{
	INTERNAL_IF( !IsSizeModifier() );	
	return code == KWLONG ? BaseType::MZ_LONG : BaseType::MZ_SHORT;			
}


// вернуть спецификатор хранения
::Object::SS TypeSpecifierManager::CodeToStorageSpecifierObj() const
{
	INTERNAL_IF( !IsStorageSpecifier() );	
	if( code == KWAUTO )
		return ::Object::SS_AUTO;

	else if( code == KWEXTERN )
		return ::Object::SS_EXTERN;

	else if( code == KWSTATIC )
		return ::Object::SS_STATIC;
	
	else if( code == KWREGISTER )
		return ::Object::SS_REGISTER;

	else  if( code == KWTYPEDEF )
		return ::Object::SS_TYPEDEF;

	else if( code == KWMUTABLE )
		return ::Object::SS_MUTABLE;

	INTERNAL( "'CodeToStorageSpecifierObj' получил не корректный код" );
	return ::Object::SS_NONE;	// этого не должно быть
}

	
// вернуть спецификатор хранения функции
Function::SS TypeSpecifierManager::CodeToStorageSpecifierFn() const
{
	INTERNAL_IF( !IsStorageSpecifier() );
	if( code == KWSTATIC )
		return Function::SS_STATIC;
	
	else if( code ==  KWEXTERN )
		return Function::SS_EXTERN;
	
	else if( code == KWTYPEDEF )
		return Function::SS_TYPEDEF;
	
	INTERNAL( "'CodeToStorageSpecifierFn' получил не корректный код" ); 
	return Function::SS_NONE;
}


// получить строковое представление спецификатора хранения объекта
CharString ManagerUtils::GetObjectStorageSpecifierName( ::Object::SS ss )
{
	switch( ss )
	{
	case ::Object::SS_AUTO:			return "auto";
	case ::Object::SS_REGISTER:		return "register";
	case ::Object::SS_EXTERN:		return "extern";
	case ::Object::SS_STATIC:		return "static";
	case ::Object::SS_TYPEDEF:		return "typedef";
	case ::Object::SS_BITFIELD:		return "<bitfield>";
	case ::Object::SS_MUTABLE:		return "mutable";
	case ::Object::SS_NONE:			return "<none>";
	}

	INTERNAL("'GetObjectStorageSpecifierName' функция получает неизвестный код");
	return "";
}


// получить строковое представление спецификатора функции
CharString ManagerUtils::GetFunctionStorageSpecifierName( Function::SS ss )
{
	switch( ss )
	{	
	case Function::SS_EXTERN:		return "extern";
	case Function::SS_STATIC:		return "static";
	case Function::SS_TYPEDEF:		return "typedef";
	case ::Object::SS_NONE:			return "<none>";
	}

	INTERNAL("'GetFunctionStorageSpecifierName' функция получает неизвестный код");
	return "";
}


// получить спецификатор доступа в виде имени
PCSTR ManagerUtils::GetAccessSpecifierName( ClassMember::AS as )
{
	switch( as )
	{
	case ClassMember::AS_PUBLIC:	return "открытый";
	case ClassMember::AS_PRIVATE:	return "закрытый";
	case ClassMember::AS_PROTECTED:	return "защищенный";
	case ClassMember::NOT_CLASS_MEMBER: return "<не член>";
	}

	INTERNAL( "'GetAccessSpecifierName' получила неизвестный код спецификатора доступа" );
	return "";
}


// получить название области видимости в виде имени, если
// она является идентификатором, вернуть его
CharString ManagerUtils::GetSymbolTableName( const SymbolTable &st )
{
	if( st.IsGlobalSymbolTable() )
		return "глобальная область видимости";
	
	else if( st.IsNamespaceSymbolTable() )
		return static_cast<const NameSpace &>(st).GetQualifiedName();
	
	else if( st.IsLocalSymbolTable() )
		return 
			(GetScopeSystem().GetFunctionSymbolTable())->GetFunction().GetQualifiedName();
	
	else if( st.IsFunctionSymbolTable() )
		return static_cast<const FunctionSymbolTable &>(st).GetFunction().GetQualifiedName();
	
	else if( st.IsClassSymbolTable() )
		return static_cast<const ClassType &>(st).GetQualifiedName();
	
	else
		INTERNAL( "'GetAccessSpecifierName' получила неизвестную область видимости" );
	return "";
}


// метод характеризует класс 'base' по отношению к 'derived'
void DerivationManager::Characterize( const ClassType &base, 
							const ClassType &curCls, bool ac )
{
	register const BaseClassList &bcl = curCls.GetBaseClassList();
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &clh = *bcl.GetBaseClassCharacteristic(i);
		const ClassType &bcls = clh.GetPointerToClass();

		// ac учитываем доступность как предыдущих классов так и текущих
		ac = ac && clh.GetAccessSpecifier() == ClassMember::AS_PUBLIC;

		// если полученный класс является 'base', выполним операции и
		// выйдем из цикла, т.к. класс не может наследовать сам себя
		if( &base == &bcls )
		{
			baseCount++;

			// если первый класс, проверяем его на виртуальность и на доступность
			if( baseCount == 1 )
			{
				virtualDerivation = clh.IsVirtualDerivation();
				accessible = ac;
			}

			else
			{
				virtualDerivation = virtualDerivation && clh.IsVirtualDerivation();

				// из нескольких базовых классов можно выбрать самый доступный,
				// если он виртуален
				accessible = accessible || ac;
			}				   			
		}


		// иначе вызываем рекурсию двигаясь вверх по иерархии
		else
			Characterize( base, bcls, ac );
	}		
}


// функция поиска всех четырех функций членов
void SMFManager::FoundSMF()
{
	// сначала проходим по списку конструкторов
	for( ConstructorList::const_iterator p = pClass.GetConstructorList().begin();
		 p != pClass.GetConstructorList().end(); p++ )
	{
		if( IsDefaultConstructor(**p, pClass) )		
			ctorDef.first ? (ctorDef.second = true) : (void)(ctorDef.first = *p);
		
		else if( IsCopyConstructor(**p, pClass) )
			ctorCopy.first ? (ctorCopy.second = true) : (void)(ctorCopy.first = *p);
	}

	// сохраняем деструктор
	dtor.first = pClass.GetDestructor();

	// наконец ищем оператор копирования
	NameManager nm("operator =", &pClass, false);	
	if( nm.GetRoleCount() != 0 )
	{
		for( RoleList::const_iterator p = nm.GetRoleList().begin();
			 p != nm.GetRoleList().end(); p++ )
		{
			INTERNAL_IF( (*p).second != R_CLASS_OVERLOAD_OPERATOR );
			const ClassOverloadOperator &coo = 
				static_cast<const ClassOverloadOperator &>( * (*p).first );

			// проверяем, если оператор копирования, сохраняем его
			if( IsCopyOperator(coo, pClass) )		
				copyOperator.first ? 
					(void)(copyOperator.second = true) : (copyOperator.first = &coo);
		}
	}
}
