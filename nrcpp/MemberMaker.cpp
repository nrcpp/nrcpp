// реализация интерфеса строителей членов класса - MemeberMaker.cpp

// реализация интерфейса КЛАССОВ-СТРОИТЕЛЕЙ - Maker.cpp

#pragma warning(disable: 4786)
#include <nrc.h>

using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "LexicalAnalyzer.h"
#include "Object.h"
#include "Scope.h"
#include "Class.h"
#include "Manager.h"
#include "Parser.h"
#include "Maker.h"
#include "Checker.h"
#include "MemberMaker.h"
#include "Overload.h"

using namespace MakerUtils;


// функция создания данного-члена, переопределяет метод класса DeclarationMaker
bool DataMemberMaker::Make()
{
	DataMemberChecker dmc(*toc);
	if( dmc.IsRedeclared() )
		return false;

	// иначе создаем член и вставляем его в класс
	targetDM = new DataMember( toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->ssCode < 0 ? ::Object::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierObj(), curAccessSpec);


	// проверяем, чтобы член не переопределялся
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetDM, nm.GetRoleList(), toc->errPos, R_DATAMEMBER);
	
	// если член не переопределен, вставить его в таблицу
	if( !rchk.IsRedeclared() )
		GetCurrentSymbolTable().InsertSymbol(targetDM);
	else
	{
		delete targetDM;
		targetDM = const_cast<DataMember *>(
			dynamic_cast<const DataMember *>(rchk.GetPrevDeclaration()) );
		return false;
	}

	return true;
}

// абстрактный метод для инициализации члена. Инициализировать можно только,
// статические целые данные-члены
void DataMemberMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{
	INTERNAL_IF( mit != MIT_NONE && mit != MIT_DATA_MEMBER && mit != MIT_BITFIELD );

	// проверяем, если инициализация члена
	if( mit == MIT_DATA_MEMBER )
		CheckDataInit( exp );

	// проверяем, если задается битовое
	else if( mit == MIT_BITFIELD )		
		CheckBitField( exp );
	
	// иначе если не ничего, ошибка
	else if( mit != MIT_NONE )
		INTERNAL( "'DataMemberMaker::Initialize' - неверный тип инициализации" );		
}


// функция создания метода переопределяет метод класса DeclarationMaker
bool MethodMaker::Make()
{
	MethodChecker mc(*toc);
	if( mc.IsRedeclared() )
		return false;

	targetMethod = new Method( toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
		Function::CC_NON, curAccessSpec, false, toc->fnSpecCode == KWVIRTUAL, 
		false, Method::DT_USERDEFINED );
	
	// проверяем, чтобы член не переопределялся
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetMethod, nm.GetRoleList(), toc->errPos, R_METHOD);
	
	// если метод не переопределен, вставить его в таблицу
	if( !rchk.IsRedeclared() )
	{
		CheckerUtils::DefaultArgumentCheck( 
			targetMethod->GetFunctionPrototype(), NULL, toc->errPos);

		GetCurrentSymbolTable().InsertSymbol(targetMethod);
	}

	else
	{
		const Method *meth = dynamic_cast<const Method *>(rchk.GetPrevDeclaration());
		if( meth )		
			theApp.Error(toc->errPos, "'%s' - метод уже объявлен", toc->name.c_str());
			
		delete targetMethod;
		targetMethod = const_cast<Method *>(meth);
		return false;
	}
	
	return true;
}


// абстрактный метод для инициализации члена. Инициализация может быть
// только MIT_PURE_VIRTUAL, exp должен быть равен NULL
void MethodMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{		
	if( mit == MIT_PURE_VIRTUAL )	
		targetMethod->SetAbstract();	// задаем абстракность методу
	
	else if( mit == MIT_BITFIELD )
		theApp.Error( toc->errPos, "битовое поле не может быть методом" );

	else if( mit != MIT_NONE )
		INTERNAL( "'MethodMaker::Initialize' - неверный тип инициализации" );

	// проверить семантику виртуальной функции
	VirtualMethodChecker( *targetMethod, toc->errPos );
}


// функция создания перегруженного оператора,
// переопределяет метод класса DeclarationMaker
bool OperatorMemberMaker::Make()
{
	ClassOperatorChecker coc(*toc, tooc);

	targetOO = new ClassOverloadOperator( toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
		Function::CC_NON, curAccessSpec, false, toc->fnSpecCode == KWVIRTUAL, 
		tooc.opCode, tooc.opString, Method::DT_USERDEFINED
		);
	
	// проверяем, чтобы оператор не переопределялся
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetOO, nm.GetRoleList(), toc->errPos, R_CLASS_OVERLOAD_OPERATOR);
	
	// если метод не переопределен, вставить его в таблицу
	if( !rchk.IsRedeclared() )
	{
		// если оператор не функция, что могло получиться в следствии синтаксической
		// ошибки, удалим его
		if( !toc->dtl.IsFunction() )
		{
			delete targetOO;
			targetOO = NULL;
			return false;
		}

		// иначе проверяем параметры по умолчанию и вставляем символ
		else
		{
			CheckerUtils::DefaultArgumentCheck( 
				targetOO->GetFunctionPrototype(), NULL, toc->errPos);
			GetCurrentSymbolTable().InsertSymbol(targetOO);		
		}
	}

	else
	{
		const ClassOverloadOperator *coo = 
				dynamic_cast<const ClassOverloadOperator *>(rchk.GetPrevDeclaration());
		if( coo )
			theApp.Error(toc->errPos, "'%s' - перегруженный оператор уже объявлен", 
				toc->name.c_str());

		delete targetOO;
		targetOO = const_cast<ClassOverloadOperator *>(coo);
		return false;
	}
	
	return true;
}


// абстрактный метод для инициализации метода. Инициализация может быть
// только MIT_PURE_VIRTUAL, exp должен быть равен NULL
void OperatorMemberMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{	
	if( mit == MIT_PURE_VIRTUAL )
		targetOO->SetAbstract();	// задаем абстракность методу
	
	else if( mit == MIT_BITFIELD )
		theApp.Error( toc->errPos, "битовое поле не может быть методом" );

	else if( mit != MIT_NONE )
		INTERNAL( "'OperatorMemberMaker::Initialize' - неверный тип инициализации" );

	// проверить семантику виртуального оператора
	VirtualMethodChecker( *targetOO, toc->errPos );
}


// функция создания оператора приведения,
// переопределяет метод класса DeclarationMaker
bool CastOperatorMaker::Make()
{
	// выполняем проверку
	CastOperatorChecker coc(*toc, tcoc);

	// если оператор не подлежит созданию выйти
	if( coc.IsIncorrect() )
		return false;

	targetCCOO = new ClassCastOverloadOperator( toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
		Function::CC_NON, curAccessSpec, false, toc->fnSpecCode == KWVIRTUAL, 
		tcoc.opCode, tcoc.opString, *tcoc.castType, Method::DT_USERDEFINED);

	// проверяем, чтобы член не переопределялся
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetCCOO, nm.GetRoleList(), toc->errPos, R_CLASS_OVERLOAD_OPERATOR);
	
	// если член не переопределен, вставить его в таблицу
	if( !rchk.IsRedeclared() )
		GetCurrentSymbolTable().InsertSymbol(targetCCOO);
	else
	{
		delete targetCCOO;	
		theApp.Error(toc->errPos, "'%s' - оператор приведения уже объявлен", 
				toc->name.c_str());

		targetCCOO = const_cast<ClassCastOverloadOperator *>(
			dynamic_cast<const ClassCastOverloadOperator *>(rchk.GetPrevDeclaration()) );
	
		return false;
	}

	return true;
}


// абстрактный метод для инициализации метода. Инициализация может быть
// только MIT_PURE_VIRTUAL, exp должен быть равен NULL
void CastOperatorMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{	
	if( mit == MIT_PURE_VIRTUAL )
		targetCCOO->SetAbstract();	// задаем абстракность методу

	else if( mit == MIT_BITFIELD )
		theApp.Error( toc->errPos, "битовое поле не может быть методом" );

	else if( mit != MIT_NONE )
		INTERNAL( "'CastOperatorMaker::Initialize' - неверный тип инициализации" );

	// проверить семантику виртуального оператора
	VirtualMethodChecker( *targetCCOO, toc->errPos );	
}


// функция создания конструктора, переопределяет метод класса DeclarationMaker
bool ConstructorMaker::Make()
{
	ConstructorChecker cm(*toc, clsType);

	if( cm.IsIncorrect() )
		return false;

	// если у конструткора не задана ссылка, задать
	if( toc->dtl.IsFunction() && toc->dtl.GetDerivedTypeCount() != 2 )
		toc->dtl.AddDerivedType(new Reference);

	targetCtor = new ConstructorMethod( toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
		Function::CC_NON, curAccessSpec, toc->fnSpecCode == KWEXPLICIT, Method::DT_USERDEFINED );

	// проверяем, чтобы член не переопределялся
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetCtor, nm.GetRoleList(), toc->errPos, R_CONSTRUCTOR);
	
	// если член не переопределен, вставить его в таблицу
	if( !rchk.IsRedeclared() )
	{
		CheckerUtils::DefaultArgumentCheck( 
			targetCtor->GetFunctionPrototype(), NULL, toc->errPos);
		GetCurrentSymbolTable().InsertSymbol(targetCtor);		
	}

	else
	{
		delete targetCtor;	
		theApp.Error(toc->errPos, "'%s' - конструктор уже объявлен", 
				toc->name.c_str());

		targetCtor = const_cast<ConstructorMethod *>(
			dynamic_cast<const ConstructorMethod *>(rchk.GetPrevDeclaration()) );
		return false;
	}
	return true;
}


// абстрактный метод для инициализации конструктора. Инициализация 
// конструктора невозможна
void ConstructorMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{		
	if( mit == MIT_PURE_VIRTUAL )
		theApp.Error( toc->errPos, 
			"'%s' - конструктор не может иметь спецификатора чисто-виртуальной функции",
			toc->name.c_str());

	else if( mit == MIT_BITFIELD )
		theApp.Error( toc->errPos, "битовое поле не может быть методом" );

	else if( mit != MIT_NONE )
		INTERNAL( "'ConstructorMaker::Initialize' - неверный тип инициализации" );
}


// функция создания деструкторов, переопределяет метод класса DeclarationMaker
bool DestructorMaker::Make()
{
	// Деструктор должен быть функцией, без спецификаторов типа и 
	// дополнительных производных типов. Деструктор
	// не возвращает значения. Деструктор должен объявляться без параметров.
	if( !toc->dtl.IsFunction() || toc->dtl.GetDerivedTypeCount() != 1 )
		theApp.Error(toc->errPos, "'%s' - деструктор должен быть функцией", 
				toc->name.c_str());

	// базовый тип, cv-квалификаторы, спецификаторы хранения, friend-специф.
	// должны отсутствовать в деструкторе
	if( toc->finalType != NULL || toc->constQual || toc->volatileQual ||
		toc->friendSpec || toc->fnSpecCode == KWEXPLICIT )
		theApp.Error(toc->errPos, "спецификаторы типа в объявлении деструктора", 
				toc->name.c_str());

	// деструктор должен объявляться без параметров и без cv-квалификаторов
	if( toc->dtl.IsFunction() )
	{
		const FunctionPrototype &fp = static_cast<const FunctionPrototype&>(
			*toc->dtl.GetHeadDerivedType());

		if( fp.GetParametrList().GetFunctionParametrCount() != 0 ||
			fp.IsHaveEllipse() )
			theApp.Error(toc->errPos, "'%s' - деструктор должен объявляться без параметров", 
				toc->name.c_str());

		if( fp.IsConst() || fp.IsVolatile() )
			theApp.Error(toc->errPos, "'%s' - деструктор не может содержать cv-квалификаторы", 
				toc->name.c_str());
	}

	// имя деструктора должно совпадать с именем класса
	INTERNAL_IF( toc->name.at(0) != '~' );
	if( toc->name != ('~' + clsType.GetName()) )
	{
		theApp.Error(toc->errPos, "'%s' - имя деструктора не совпадает с именем класса", 
			toc->name.c_str());
		return false;
	}

	// последнее, деструктор должен быть один во всем классе
	NameManager nm(toc->name, (SymbolTable*)&GetCurrentSymbolTable(), false);
	if( nm.GetRoleCount() != 0 )
	{
		theApp.Error(toc->errPos, "'%s' - деструктор уже объявлен", 
			toc->name.c_str());
		return false;
	}

	// если базовый тип не задан - задаем void
	if( toc->finalType == NULL )
		toc->finalType = (BaseType*)&ImplicitTypeManager(BaseType::BT_VOID).GetImplicitType();

	// остается создать деструткор
	targetDtor = new Method(toc->name, &clsType, toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
		Function::CC_NON, curAccessSpec, false, toc->fnSpecCode == KWVIRTUAL, 
		true, Method::DT_USERDEFINED);

	GetCurrentSymbolTable().InsertSymbol(targetDtor);
	return true;
}


// абстрактный метод для инициализации метода. Инициализация может быть
// только MIT_PURE_VIRTUAL, exp должен быть равен NULL
void DestructorMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{
	INTERNAL_IF( mit == MIT_DATA_MEMBER );
	if( mit == MIT_PURE_VIRTUAL )
		targetDtor->SetAbstract();	// задаем абстракность методу

	else if( mit == MIT_BITFIELD )
		theApp.Error( toc->errPos, "битовое поле не может быть методом" );

	else if( mit != MIT_NONE )
		INTERNAL( "'DestructorMaker::Initialize' - неверный тип инициализации" );

	// проверить семантику виртуального деструктора
	VirtualMethodChecker( *targetDtor, toc->errPos );
}


// функция создания дружеских функций, 
// переопределяет метод класса DeclarationMaker
bool FriendFunctionMaker::Make()
{
	// убираем спецификатор дружбы для проверки
	INTERNAL_IF( !toc->friendSpec );
	toc->friendSpec = false;

	if( isOperator )
		GlobalOperatorChecker g(*toc, tooc); 
	else
		GlobalDeclarationChecker g(*toc); 

	if( toc->ssCode != -1 )
		theApp.Error(toc->errPos, "'%s' - использование %s во friend-объявлении",
			toc->name.c_str(), GetKeywordName(toc->ssCode));

	// проверяем, чтобы член не переопределялся
	SymbolTable *destST = const_cast<SymbolTable *>(&::GetScopeSystem().GetGlobalSymbolTable());
	NameManager nm(toc->name, destST, true);

	// создаем объекь для проверки на переопределяемость
	if( isOperator )
		targetFn = new OverloadOperator(toc->name, destST,
			toc->finalType,	toc->constQual, toc->volatileQual, 
			toc->dtl, toc->fnSpecCode == KWINLINE,
			toc->ssCode < 0 ? Function::SS_NONE : 
			TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
				Function::CC_NON, tooc.opCode, tooc.opString );

	else
		targetFn = new Function(toc->name, destST,
			toc->finalType,	toc->constQual, toc->volatileQual, 
			toc->dtl, toc->fnSpecCode == KWINLINE,
			toc->ssCode < 0 ? Function::SS_NONE : 
			TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(), Function::CC_NON  );
							
	// проверяем на переопределение
	RedeclaredChecker rchk(*targetFn, nm.GetRoleList(), toc->errPos, 
		isOperator ? R_OVERLOAD_OPERATOR : R_FUNCTION);

	// если переопределен, сохраняем функцию как результат работы
	if( rchk.IsRedeclared() )
	{
		Function *prevFn = const_cast<Function *>(
			dynamic_cast<const Function *>(rchk.GetPrevDeclaration()) );
		if( prevFn != NULL )
			CheckerUtils::DefaultArgumentCheck( targetFn->GetFunctionPrototype(), 
				&prevFn->GetFunctionPrototype(), toc->errPos);
		delete targetFn;
		targetFn = prevFn;	
	}
	
	// иначе вставляем ее в ближайшую глобальную область видимости
	else	
	{
		CheckerUtils::DefaultArgumentCheck( targetFn->GetFunctionPrototype(), NULL, toc->errPos);
		INTERNAL_IF( !destST->InsertSymbol(targetFn));	
	}


	// в конечном итоге вставляем функцию в список друзей класса
	const_cast<ClassFriendList &>(clsType.GetFriendList()).
		AddClassFriend( ClassFriend(targetFn) );

	return true;
}


// абстрактный метод для дружеской функции. Инициализация невозможна
void FriendFunctionMaker::Initialize( MemberInitializationType mit, const Operand &exp )
{	
	if( mit != MIT_NONE )
		theApp.Error( toc->errPos,
			"'%s' - у дружественной функции не может быть инициализатора",
			toc->name.c_str());
}


// специальная функция уточняющая базовый тип в определение члена,
// с учетом того, что в конструкторах, деструкторах, операторах приведения
// базовый тип не задается явно а формируется автоматически
void MemberDefinationMaker::SpecifyBaseType( const NodePackage *np, const SymbolTable &st )
{
	// получаем последнее имя
	const NodePackage *last = static_cast<const NodePackage *>(np->GetLastChildPackage());

	// должно быть имя, оператор, деструктор
	register int pid = last->GetPackageID();
	bool special = pid == PC_DESTRUCTOR || pid == PC_CAST_OPERATOR ||idRole == R_CONSTRUCTOR;
	INTERNAL_IF( pid != NAME && pid != PC_OVERLOAD_OPERATOR && !special );

	// если базовый тип задан, либо имеем не деструктор, конструктор или оператор приведения
	if( toc->baseType != NULL || !special )		
	{
		if( special ) 		
			theApp.Error(toc->errPos, 
				"'%s' - базовый тип не может задаваться при определении %s", 
				toc->name.c_str(), pid == PC_DESTRUCTOR ? "деструктора" : 
				(pid == PC_CAST_OPERATOR ? "оператора приведения" : "конструктора"));
			
		MakerUtils::SpecifyBaseType( &*toc );
		return;	
	}
		

	// если базовый тип не задан, то уточнять его не будем, т.к.
	// в деструкторах, конструкторах, операторах приведения тип формируется 
	// автоматически и явно не задается. Соотв. (тип класс, void, тип приведения без ф-ции)

	// если деструктор, базовый тип по умолчанию void
	if( pid == PC_DESTRUCTOR )
		toc->finalType = 
			const_cast<BaseType*>(&ImplicitTypeManager(BaseType::BT_VOID).GetImplicitType());	

	// если конструктор
	else if( idRole == R_CONSTRUCTOR )
	{
		// у конструктора по умолчанию тип класса к которому он принадлежит
		INTERNAL_IF( !st.IsClassSymbolTable() );
		const ClassType &cls = static_cast<const ClassType&>(st);
		toc->finalType = const_cast<ClassType *>(&cls);
	}

	// иначе если оператор приведения
	else if( pid == PC_CAST_OPERATOR )
	{
		// здесь ничего выполнять не требуется т.к. чекер все выполнит сам
		TempCastOperatorContainer tcoc;
		AnalyzeCastOperatorPkg( *last, tcoc );									
		CastOperatorChecker(*toc, tcoc);
	}

	// иначе внутренняяя ошибка
	else
		INTERNAL( "неизвестный тип идентификатора" );
}


// Определить роль объявляемого идентификатора. Классы, объединения
// перечисления игнорируются, остальные роли должны совпадать.
// Должна быть хотя-бы одна объектная роль. Возвращает false
// если роль не определена
bool MemberDefinationMaker::InspectIDRole( const RoleList &rl )
{
	if( rl.empty() )
		return false;
	
	for( RoleList::const_iterator p = rl.begin(); p != rl.end(); p++ )
	{
		register Role r = (*p).second;
		if( r == R_CLASS_TYPE || r == R_UNION_CLASS_TYPE || r == R_ENUM_TYPE )
			continue;

		// в противном случае проверяем правильность роли
		INTERNAL_IF( idRole != R_UNCKNOWN && idRole != r );
		if( r != R_OBJECT && r != R_DATAMEMBER &&
			!(r >= R_FUNCTION && r <= R_CONSTRUCTOR ) )
		{
			theApp.Error( toc->errPos, "'%s' - член не может определяться в данном месте",
				toc->name.c_str() );
			return false;
		}

		// если роль не задана, задаем
		if( idRole == R_UNCKNOWN )
			idRole = r;
	}

	// если роль не задана, значит были только классы
	if( idRole == R_UNCKNOWN )
	{
		theApp.Error( toc->errPos, 
			"'%s' - член не является объектом или функцией", toc->name.c_str() );
		return false;
	}

	return true;
}


// построить определение
bool MemberDefinationMaker::Make()
{
	// сначала нам необходимо найти идентификатор, который
	// соотв. определению
	int ix = decl->FindPackage(PC_QUALIFIED_NAME);
	INTERNAL_IF( ix < 0 );

	const NodePackage *np = static_cast<const NodePackage *>(decl->GetChildPackage(ix));
	INTERNAL_IF( np->GetChildPackageCount() <= 1 );

	// иначе создаем временную структуру и собираем в нее декларацию
	toc = new TempObjectContainer(
		ParserUtils::GetPackagePosition(np), ParserUtils::PrintPackageTree(np) );

	// если соотв. не найдено, выйти
	if( memberQnm.GetRoleCount() == 0 )
	{
		theApp.Error(toc->errPos, "'%s' - член не найден", toc->name.c_str() );
		return false;
	}

	// выявляем роль идентификатора
	if( !InspectIDRole( memberQnm.GetRoleList() ) )
		return false;

	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( tsl, &*toc );

	// далее анализируем декларатор
	MakerUtils::AnalyzeDeclaratorPkg( decl, &*toc );

	// уточняем базовый тип специальным методом
	const SymbolTable *st = 
		memberQnm.GetQualifierList()[memberQnm.GetQualifierList().GetSymbolTableCount()-1];
	SpecifyBaseType( np , *st );

	// сейчас проверяем, чтобы не было ненужных спецификаторов в определении
	if( toc->fnSpecCode != -1 || toc->ssCode != -1 || toc->friendSpec )
		theApp.Error(toc->errPos, "'%s' - %s некорректно при определении члена", 
			toc->name.c_str(), toc->friendSpec ? "friend" : 
			GetKeywordName(toc->ssCode != -1 ? toc->ssCode : toc->fnSpecCode));
		
	// теперь остается проверить, соотв. определения и объявления
	RedeclaredChecker rchk( 
		TypyziedEntity(toc->finalType, toc->constQual, toc->volatileQual, toc->dtl),
		memberQnm.GetRoleList(), toc->errPos, idRole);

	if( !rchk.IsRedeclared() )
	{
		theApp.Error(toc->errPos, "'%s' - член не найден", toc->name.c_str() );
		return false;
	}

	// иначе идентификатор найден, и если он функция, проверим параметры по умолчанию
	else	
	{
		targetID = const_cast<Identifier *>(rchk.GetPrevDeclaration());	
		if( Function *prevFn = dynamic_cast<Function *>(targetID) )
		{
			INTERNAL_IF( !toc->dtl.IsFunction() );
			CheckerUtils::DefaultArgumentCheck( static_cast<const FunctionPrototype&>(
				*toc->dtl.GetDerivedType(0)), &prevFn->GetFunctionPrototype(), toc->errPos);

			// также проверим, чтобы не объявлялся метод сгенерированный
			// компилятором
			if( prevFn->IsClassMember() && 
				!static_cast<const Method *>(prevFn)->IsUserDefined() )
				theApp.Error(toc->errPos, 
					"'%s' - невозможно определить метод сгенерированный компилятором",
					prevFn->GetQualifiedName().c_str() );
		}
	}

	// проверяем, члены из каких областей можно определять
	if( st->IsClassSymbolTable() )
	{
		if( ::Object *ob = dynamic_cast<::Object *>(targetID) )
		{
			if( ob->GetStorageSpecifier() != ::Object::SS_STATIC )
				theApp.Error(toc->errPos, 
					"'%s' - не статический член не может определяться",
					toc->name.c_str() );
			// проверить на инициализацию
		}
	}
	
	else if( st->IsNamespaceSymbolTable() )
		;
	// иначе ошибка
	else
		theApp.Error(toc->errPos, 
			"можно определять только члены класса и члены именованной области видимости",
			toc->name.c_str() );

	INTERNAL_IF( targetID == NULL );
	return true;
}

