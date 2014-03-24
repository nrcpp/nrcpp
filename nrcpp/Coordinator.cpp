// реализация интерфейса для КЛАССОВ-КООРДИНАТОРОВ - Coordinator.cpp

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
#include "Maker.h"
#include "MemberMaker.h"
#include "Parser.h"
#include "Body.h"
#include "Coordinator.h"
#include "ExpressionMaker.h"


// помимо областей видимости члена, сохраняем 
// закрытый метод, сохраняет менеджер имени члена и области
// видимости члена
void DeclarationCoordinator::StoreMemberScope( const NodePackage &np ) const
{
	INTERNAL_IF( np.GetPackageID() != PC_QUALIFIED_NAME );
	memberQnm = new QualifiedNameManager(&np, &GetCurrentSymbolTable());

	// сохраняем список квалификаторов
	memberStl = memberQnm->GetQualifierList();

	// если областей видимости нет, выходим. Событие происходит при
	// неверном поиске
	if( memberStl.IsEmpty() )
		return;

	// далее все области видимости из списка вставляем в общюю
	// систему ОВ
	
	// если первая область видимости глобальная, вынуть из списка
	if( memberStl[0] == GetScopeSystem().GetFirstSymbolTable() )
		memberStl.PopFront();

	// загружаем области видимости
	GetScopeSystem().PushSymbolTableList(memberStl);
}


// восстановить систему ОВ после определения члена ИОВ или класса
void DeclarationCoordinator::RestoreScopeSystem() const
{
	// вытаскиваем все области, которые были положены в стек 
	for( int i = 0; i<memberStl.GetSymbolTableCount(); i++ )
		::GetScopeSystem().DestroySymbolTable();
	memberStl.Clear();
}


// скоординировать постройку декларации, создать из пакета
// временную структуру и далее на ее основании выбрать строителя
PDeclarationMaker DeclarationCoordinator::Coordinate() const
{
	int ix = declarator->FindPackage(PC_QUALIFIED_NAME);
	INTERNAL_IF( ix < 0 );

	// проверяем, если идет определение члена, т.е. имя составное
	// значит сразу возвращаем строитель определений члена
	if( static_cast<const NodePackage *>(declarator->GetChildPackage(ix))->
			GetChildPackageCount() > 1 )
	{
		// предварительно поместим области видимости члена (класс(ы))
		// в систему ОВ. В деструкторе координатора они вынимаются
		StoreMemberScope( *static_cast<const NodePackage *>(declarator->GetChildPackage(ix)) );
		return new MemberDefinationMaker(typeSpecList, declarator, *memberQnm);
	}

	// иначе создаем временную структуру и собираем в нее декларацию
	PTempObjectContainer toc = 
		new TempObjectContainer ( 
			ParserUtils::GetPackagePosition(declarator->GetChildPackage(ix)),
			ParserUtils::PrintPackageTree((NodePackage*)declarator->GetChildPackage(ix))
		);

	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( typeSpecList, &*toc );

	// далее анализируем декларатор
	MakerUtils::AnalyzeDeclaratorPkg( declarator, &*toc );
	
	// уточняем базовый тип
	MakerUtils::SpecifyBaseType( &*toc );
	
	// выбираем строителя - основное предназначение координатора
	const NodePackage &np = 
		*(NodePackage *)((NodePackage*)declarator->GetChildPackage(ix))->GetChildPackage(0);
	if( np.GetPackageID() == PC_OVERLOAD_OPERATOR )
	{
		TempOverloadOperatorContainer tooc;
		MakerUtils::AnalyzeOverloadOperatorPkg(  np,  tooc);
		return new GlobalOperatorMaker(toc, tooc);
	}

	else if( np.GetPackageID() == PC_CAST_OPERATOR )
	{
		theApp.Error( toc->errPos, "операторы приведения могут быть только членами класса");
		return NULL;
	}

	else if( toc->dtl.IsFunction() && toc->ssCode != KWTYPEDEF )
		return new GlobalFunctionMaker(toc);
	else
		return new GlobalObjectMaker(toc);
}


// скоординировать постройку декларации, создать из пакета
// временную структуру и далее на ее основании выбрать строителя
PDeclarationMaker AutoDeclarationCoordinator::Coordinate() const
{
	int ix = declarator->FindPackage(PC_QUALIFIED_NAME);
	INTERNAL_IF( ix < 0 );
	Position errPos = ParserUtils::GetPackagePosition(declarator->GetChildPackage(ix));

	// проверяем, если идет определение члена, т.е. имя составное
	// значит это является ошибкой
	if( static_cast<const NodePackage *>(declarator->GetChildPackage(ix))->
			GetChildPackageCount() > 1 )
	{
		theApp.Error(errPos,
			"декларация члена невозможна в локальной области видимости");
		return NULL;
	}

	// иначе создаем временную структуру и собираем в нее декларацию
	PTempObjectContainer toc = 
		new TempObjectContainer ( 
			errPos,
			ParserUtils::PrintPackageTree((NodePackage*)declarator->GetChildPackage(ix))
		);

	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( typeSpecList, &*toc );

	// далее анализируем декларатор
	MakerUtils::AnalyzeDeclaratorPkg( declarator, &*toc );
	
	// уточняем базовый тип
	MakerUtils::SpecifyBaseType( &*toc );
	
	// выбираем строителя - основное предназначение координатора
	const NodePackage &np = 
		*(NodePackage *)((NodePackage*)declarator->GetChildPackage(ix))->GetChildPackage(0);
	if( np.GetPackageID() == PC_OVERLOAD_OPERATOR )
	{
		TempOverloadOperatorContainer tooc;
		MakerUtils::AnalyzeOverloadOperatorPkg(  np,  tooc);
		return new GlobalOperatorMaker(toc, tooc);
	}

	else if( np.GetPackageID() == PC_CAST_OPERATOR )
	{
		theApp.Error( toc->errPos, "операторы приведения могут быть только членами класса");
		return NULL;
	}

	else if( toc->dtl.IsFunction() && toc->ssCode != KWTYPEDEF )
		return new GlobalFunctionMaker(toc);
	else
		return new GlobalObjectMaker(toc, true);
}


// определить строителя декларации 
PMemberDeclarationMaker MemberDeclarationCoordinator::Coordinate()
{
	int ix = declarator->FindPackage(PC_QUALIFIED_NAME);
	const NodePackage *np = ix >= 0 ? (const NodePackage *)declarator->GetChildPackage(ix) : 0;
	Position ep;
	CharString name;
	
	
	ep = ParserUtils::GetPackagePosition(ix < 0 ? typeSpecList : np);
	name = ix < 0 ? "<без имени>" : ParserUtils::PrintPackageTree(np);

	// декларатор должен обязательно присутствовать
	if( declarator->IsNoChildPackages() )
	{
		theApp.Error(ep, "пропущен декларатор в объявлении члена");
		return NULL;
	}

	// сразу проверяем, чтобы имя не было квалифицированным
	if( np && np->GetChildPackageCount() > 1 )
	{
		theApp.Error(ep, "'%s' - нельзя объявлять квалифицированное имя внутри класса",
			name.c_str());
		return NULL;
	}

	// обработка оператора приведения, отличается от обработки остальных членов
	if( np && np->GetChildPackage(0)->GetPackageID() == PC_CAST_OPERATOR )
	{		
		TempCastOperatorContainer tcoc;
		MakerUtils::AnalyzeCastOperatorPkg(*(const NodePackage *)np->GetChildPackage(0), tcoc);		

		// конструктор для объектов, которые не могут содержать составные
		// имена, либо могут не иметь имени вовсе
		PTempObjectContainer toc = new TempObjectContainer( ep, tcoc.opFullName, curAccessSpec );
		
		// начинаем анализ спецификаторов типа
		MakerUtils::AnalyzeTypeSpecifierPkg( typeSpecList, &*toc );

		// далее анализируем декларатор
		MakerUtils::AnalyzeDeclaratorPkg( declarator, &*toc );
	
		// уточняем базовый тип, но только если список не пустой
		if( !typeSpecList->IsNoChildPackages() )
		{
			// если содержится один пакет, и это 'virtual',
			// то это допустимо, иначе следует вывести ошибку
			if( typeSpecList->GetChildPackageCount() == 1 && 
				typeSpecList->GetChildPackage(0)->GetPackageID() == KWVIRTUAL )
				toc->fnSpecCode = KWVIRTUAL;

			// иначе уточняем
			else
				MakerUtils::SpecifyBaseType( &*toc );	
		}

		return new CastOperatorMaker(clsType, curAccessSpec, toc, tcoc);		
	}

	// конструктор для объектов, которые не могут содержать составные
	// имена, либо могут не иметь имени вовсе
	PTempObjectContainer toc = new TempObjectContainer( ep, name, curAccessSpec );

	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( typeSpecList, &*toc );

	// далее анализируем декларатор
	MakerUtils::AnalyzeDeclaratorPkg( declarator, &*toc );
	
	// уточняем базовый тип, но только если это не деструктор и не конструктор
	if( np														 && 
		(np->GetChildPackage(0)->GetPackageID() == PC_DESTRUCTOR &&
		 (typeSpecList->IsNoChildPackages() || (typeSpecList->GetChildPackageCount() == 1 &&
		  typeSpecList->GetChildPackage(0)->GetPackageID() == KWVIRTUAL) )	||
		 toc->name == clsType.GetName() )
		 )
		;

	else
		MakerUtils::SpecifyBaseType( &*toc );

	// если имени нет - значит это первое объявление конструктора,
	// внутри класса. Если имя совпадает с именем класса, значит это
	// не первое объявление конструктора. В любом случае вызываем
	// строитель конструктора
	if( ix < 0 || toc->name == clsType.GetName() )
	{
		// имя конструктора не должно совпадать с именем класса. У конструктора
		// не должно быть имени вообще, поэтому меняем его
		toc->name = ('.' +
			string(clsType.GetName().c_str()) ).c_str();	
		return new ConstructorMaker(clsType, curAccessSpec, toc);
	}

	// иначе если заголовок имени - PC_OVERLOAD_OPERATOR, возвращаем
	// строитель перегруженного оператора
	else if( np->GetChildPackage(0)->GetPackageID() == PC_OVERLOAD_OPERATOR )
	{
		TempOverloadOperatorContainer tooc;
		MakerUtils::AnalyzeOverloadOperatorPkg(
			*(const NodePackage *)np->GetChildPackage(0), tooc);
		toc->name = tooc.opFullName;

		// может быть friend-декларация, тогда следует вызвать строителя
		// перегруженных операторов, в противном случае обычный строитель
		// операторов-членов
		if( toc->friendSpec )
			return new FriendFunctionMaker(clsType, curAccessSpec, toc, tooc) ;			
		else
			return new OperatorMemberMaker(clsType, curAccessSpec, toc, tooc) ;
	}

	// иначе если заголовок имени - PC_DESTRUCTOR, возвращаем
	// строитель деструктора
	else if( np->GetChildPackage(0)->GetPackageID() == PC_DESTRUCTOR )
		return new DestructorMaker(clsType, curAccessSpec, toc);

	// иначе если список производных типов начинается с функции, значит
	// это либо метод, либо дружеская функция
	else if( toc->dtl.IsFunction() && toc->ssCode != KWTYPEDEF )
	{
		// если задан спецификатор дружбы - значит возвратить строителя
		// дружеских функций
		if( toc->friendSpec )
			return new FriendFunctionMaker(clsType, curAccessSpec, toc);
		else
			return new MethodMaker(clsType, curAccessSpec, toc);
	}

	// иначе возвращаем строитель данного-члена
	else 
		return new DataMemberMaker(clsType, curAccessSpec, toc);		
}


// скоординировать и построить унарное выражение
POperand UnaryExpressionCoordinator::Coordinate() const
{
	// выражение должно присутствовать
	INTERNAL_IF( right.IsNull() );

	// сначала проверяем, если выражение ошибочно, вернуть его же
	if( right->IsErrorOperand() )
		return right;

	// если операнд тип, вернуть ошибку
	if( right->IsTypeOperand() && op != KWSIZEOF && op != KWTYPEID )
	{
		theApp.Error(errPos, "тип не может быть операндом в выражении");
		return ErrorOperand::GetInstance();
	}

	// если операнд перегруженная функция, возможно только применение
	// оператора '&'
	if( right->IsOverloadOperand() && op != '&' )
	{
		theApp.Error(errPos, 
			"'%s' - перегруженная функция не может быть операндом в выражении",
			static_cast<const OverloadOperand &>(*right).GetOverloadList().
			front()->GetName().c_str());
		return ErrorOperand::GetInstance();
	}

	// проверяем, если мы имеем член, то он должен использоваться только через
	// this, за исключением выражения '&'.
	if( right->IsPrimaryOperand() && op != '&' )
		ExpressionMakerUtils::CheckMemberThisVisibility(right, errPos);

	// далее пытаемся проверить операнд на интерпретируемость
	POperand rval = UnaryInterpretator(right, op, errPos).Interpretate();
	if( !rval.IsNull() )
		return rval;

	// пытаемся вызвать перегруженный оператор, если получается,
	// результат преобразуется в вызов функции
	rval = UnaryOverloadOperatorCaller(right, op, errPos).Call();
	if( !rval.IsNull() )
		return rval;

	// далее проверяем допустимые операторы
	switch( op )
	{
	case '!':  return LogicalUnaryMaker(right, op, errPos).Make();
	case '~':  return BitReverseUnaryMaker(right, op, errPos).Make();
	case '+':  
	case '-':  return ArithmeticUnaryMaker(right, op, errPos).Make();
	case '*':  return IndirectionUnaryMaker(right, op, errPos).Make();
	case INCREMENT:
	case DECREMENT:
	case -INCREMENT:
	case -DECREMENT: return IncDecUnaryMaker(right, op, errPos).Make();
	case '&':		 return AddressUnaryMaker(right, op, errPos).Make();
	case KWDELETE:
	case OC_DELETE_ARRAY:  
	case -KWDELETE:
	case -OC_DELETE_ARRAY:
		return DeleteUnaryMaker(right, op, errPos).Make();
	case KWTYPEID:  return TypeidUnaryMaker(right, op, errPos).Make();
	case KWTHROW:	return ThrowUnaryMaker(right, op, errPos).Make();

		// иначе неизвестный унарный оператор
	default:
		INTERNAL( "'UnaryExpressionCoordinator::Coordinate': неизвестный унарный оператор");
	}

	return NULL;		// kill warning
}


// выполнить предварительные проверки корректности выражения
// и построить его
POperand TernaryExpressionCoordinator::Coordinate() const
{
	// выражение должно присутствовать
	INTERNAL_IF( cond.IsNull() || left.IsNull() || right.IsNull() );

	// каждый операнд должен быть корректным
	if( cond->IsErrorOperand() || left->IsErrorOperand() || right->IsErrorOperand() )
		return ErrorOperand::GetInstance();

	// если операнд тип, вернуть ошибку
	if( cond->IsTypeOperand() || left->IsTypeOperand() || right->IsTypeOperand() )
	{
		theApp.Error(errPos, "тип не может быть операндом в выражении");
		return ErrorOperand::GetInstance();
	}

	// если операнд перегруженная функция
	if( cond->IsOverloadOperand() || left->IsOverloadOperand() || right->IsOverloadOperand() )
	{
		theApp.Error(errPos, 
			"перегруженная функция не может быть операндом в выражении");
		return ErrorOperand::GetInstance();
	}

	// проверка на наличие 'this' в текущей области видимости, если
	// операнд является нестатическим данным-членом класса
	if( left->IsPrimaryOperand() )
		ExpressionMakerUtils::CheckMemberThisVisibility(left, errPos);
	if( right->IsPrimaryOperand() )
		ExpressionMakerUtils::CheckMemberThisVisibility(right, errPos);		
	if( cond->IsPrimaryOperand() )
		ExpressionMakerUtils::CheckMemberThisVisibility(cond, errPos);		

	// далее пытаемся проверить операнд на интерпретируемость
	POperand rval = TernaryInterpretator(cond, left, right).Interpretate();
	if( !rval.IsNull() )
		return rval;

	// иначе строим выражение
	return IfTernaryMaker(cond, left, right, op, errPos).Make();
}
