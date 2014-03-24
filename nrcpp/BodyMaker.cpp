// реализация строителей тела функции - BodyMaker.cpp

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
#include "Parser.h"
#include "Checker.h"
#include "Body.h"
#include "ExpressionMaker.h"
#include "MemberMaker.h"
#include "Coordinator.h"
#include "Overload.h"
#include "BodyMaker.h"


// создает инициализатор объекта из списка выражений и конструктор.
// Создает инициализатор конструктором
PObjectInitializator BodyMakerUtils::MakeObjectInitializator( 
		const PExpressionList &initList, const DeclarationMaker &dm )
{
	static PExpressionList emptyList = new ExpressionList;

	// если инициализация объекта, возвращаем инициализатор, иначе вернуть NULL
	if( const GlobalObjectMaker *gom = dynamic_cast<const GlobalObjectMaker *>(&dm) )
		return new ConstructorInitializator( 
			initList.IsNull() ? emptyList : initList, gom->GetConstructor() );

	// если инициализация статического члена класса
	else if( const MemberDefinationMaker *mdm = dynamic_cast<const MemberDefinationMaker *>(&dm) )
		return new ConstructorInitializator( 
			initList.IsNull() ? emptyList : initList, mdm->GetConstructor() );
	// иначе 
	return NULL;
}


// создает инициализатор из списка инициализации
PObjectInitializator BodyMakerUtils::MakeObjectInitializator( const PListInitComponent &il )
{
	return new AgregatListInitializator( il );
}


// построить инструкцию условия для конструкций if, for, switch, while
// на основе пакетов с декларацией и инициализатором
PInstruction BodyMakerUtils::MakeCondition( 
		const NodePackage &rpkg, const POperand &iator, const Position &errPos )
{
	INTERNAL_IF( rpkg.GetPackageID() != PC_DECLARATION || rpkg.GetChildPackageCount() != 2 );
	const NodePackage *tsl = static_cast<const NodePackage*>(rpkg.GetChildPackage(0)),
		*decl =static_cast<const NodePackage*>(rpkg.GetChildPackage(1));
	
	AutoDeclarationCoordinator dcoord(tsl, const_cast<NodePackage *>(decl));
	PDeclarationMaker dmak = dcoord.Coordinate();
	
	// строим декларацию
	INTERNAL_IF( dmak.IsNull() );
	dmak->Make();

	// инициализатор объекта, задаем для генерации
	PObjectInitializator objIator = NULL;
	PExpressionList	initList = new ExpressionList;
	initList->push_back(iator);
	dmak->Initialize( *initList ),

	// строитель может вернуть NULL, если идентификатор не является объектом			
	objIator = BodyMakerUtils::MakeObjectInitializator(initList, *dmak);
	return new DeclarationInstruction(
		*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()), objIator, errPos);
}


// проверяет, чтобы условная конструкция преобразовывалась в тип bool.
// Инструкцией может быть декларация, либо выражение. В случае декларации,
// создаем временный основной операнд на основе декларатора и его проверяем
void BodyMakerUtils::ValidCondition( const PInstruction &cond, PCSTR cnam, bool toInt )
{
	INTERNAL_IF( cond.IsNull() || 
		!(cond->GetInstructionID() == Instruction::IC_DECLARATION ||
		  cond->GetInstructionID() == Instruction::IC_EXPRESSION) );

	// позиция ошибки
	const Position &errPos = cond->GetPosition();

	// если выражение, проверяем его напрямую
	if( cond->GetInstructionID() == Instruction::IC_EXPRESSION )
	{
		const POperand &exp = static_cast<const ExpressionInstruction&>(*cond).GetExpression();
		// если операнд не является выражением, вывести ошибку
		if( !(exp->IsExpressionOperand() || exp->IsPrimaryOperand()) )
		{
			if( !exp->IsErrorOperand() )				
				theApp.Error(errPos, "'%s' - выражение должно быть типа 'bool'", cnam);
			return;
		}

		// проверяем, чтобы тип был склярным
		toInt
		? ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(exp), errPos, cnam)		 
		: ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(exp), errPos, cnam);
	}

	// иначе декларация
	else
	{
		const TypyziedEntity &declarator = 
			static_cast<const DeclarationInstruction&>(*cond).GetDeclarator();

		// строим из декларатора основной операнд и проверяем
		POperand prim = new PrimaryOperand(true, declarator);
		toInt
		? ExpressionMakerUtils::ToIntegerType(prim, errPos, cnam)
		: ExpressionMakerUtils::ToScalarType(prim, errPos, cnam);
	}
}


// проверяет условие только для выражения
void BodyMakerUtils::ValidCondition( const POperand &exp, PCSTR cnam, const Position &ep ) 
{
	// если операнд не является выражением, вывести ошибку
	if( !(exp->IsExpressionOperand() || exp->IsPrimaryOperand()) )
	{
		if( !exp->IsErrorOperand() )
			theApp.Error(ep, "'%s' - выражение должно быть типа 'bool'", cnam);
		return;
	}

	// проверяем, чтобы тип был склярным
	ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(exp), ep, cnam);
}


// создать область видимости для новой конструкции ориентируяясь 
// на предыдущую. 
bool BodyMakerUtils::MakeLocalSymbolTable( const Construction &cur )
{
	// если текущая switch, while, for, catch, else
	if( cur.GetConstructionID() == Construction::CC_IF    ||
		cur.GetConstructionID() == Construction::CC_FOR   ||
		cur.GetConstructionID() == Construction::CC_WHILE ||		
		cur.GetConstructionID() == Construction::CC_CATCH ||
		cur.GetConstructionID() == Construction::CC_ELSE  ||
		cur.GetConstructionID() == Construction::CC_DOWHILE )
		return false;

	GetScopeSystem().MakeNewSymbolTable( new LocalSymbolTable(GetCurrentSymbolTable()) );
	return true;
}


// строитель case, с проверкой
CaseLabelBodyComponent *BodyMakerUtils::CaseLabelMaker( const POperand &exp, 
			const BodyComponent &childBc, const Construction &cur, const Position &ep )
{
	// проверяем само выражение. Выражение должно быть целым константным
	double ival = 0;
	if( !ExpressionMakerUtils::IsInterpretable(exp, ival)  ||
		!ExpressionMakerUtils::IsIntegral(exp->GetType()) )
		theApp.Error( ep, "'case' - выражение должно быть целым и константым");

	// находим switch-конструкцию в иерархии
	const Construction *fnd = &cur;
	while( fnd )
	{
		if( fnd->GetConstructionID() == Construction::CC_SWITCH )
			break;
		fnd = fnd->GetParentConstruction();
	}

	if( !fnd )	
		theApp.Error( ep, "case без switch" );

	// иначе выполняем проверку, нет ли такой метки уже
	else
	{
		const SwitchConstruction &sc = static_cast<const SwitchConstruction &>(*fnd);
		for( LabelList::const_iterator p = sc.GetLabelList().begin();
			 p != sc.GetLabelList().end(); p++ )
		{
			if( (*p)->GetLabelID() == LabelBodyComponent::LBC_CASE &&
				static_cast<const CaseLabelBodyComponent &>(**p).GetCaseValue() == ival )
			{
				theApp.Error(ep, "'%d' - case-метка уже задана", (int)ival);
				break;
			}
			
		}

		// задаем метку для switch
		CaseLabelBodyComponent *cbc = new CaseLabelBodyComponent(ival, childBc, ep);
		const_cast<SwitchConstruction &>(sc).AddLabel(cbc);
		return cbc;
	}

	// иначе возвращаем метку, лишб бы не NULL
	return new CaseLabelBodyComponent(ival, childBc, ep);
}


// строитель default, с проверкой
DefaultLabelBodyComponent *BodyMakerUtils::DefaultLabelMaker( 
		const BodyComponent &childBc, const Construction &cur, const Position &ep )
{
	// находим switch-конструкцию в иерархии
	const Construction *fnd = &cur;
	while( fnd )
	{
		if( fnd->GetConstructionID() == Construction::CC_SWITCH )
			break;
		fnd = fnd->GetParentConstruction();
	}

	if( !fnd )	
		theApp.Error( ep, "default без switch" );

	// иначе выполняем проверку, нет ли такой метки уже
	else
	{
		const SwitchConstruction &sc = static_cast<const SwitchConstruction &>(*fnd);
		for( LabelList::const_iterator p = sc.GetLabelList().begin();
			 p != sc.GetLabelList().end(); p++ )
		{
			if( (*p)->GetLabelID() == LabelBodyComponent::LBC_DEFAULT )
			{
				theApp.Error(ep, "default-метка уже задана");
				break;
			}
			
		}

		// задаем метку для switch
		DefaultLabelBodyComponent *dlbc = new DefaultLabelBodyComponent(childBc, ep);
		const_cast<SwitchConstruction &>(sc).AddLabel(dlbc);
		return dlbc;
	}

	// иначе возвращаем метку, лишь не NULL
	return new DefaultLabelBodyComponent(childBc, ep);
}


// вспомагательный предикат для поиска метки
class LabelPredicat
{
	// имя метки
	const CharString &lname;

public:
	// задаем имя
	LabelPredicat( const CharString &lname ) 
		: lname(lname) {
	}

	// предикат
	bool operator()( const Label &lbc ) {
		return lbc.GetName() == lname;
	}
};

// строитель обычной метки
SimpleLabelBodyComponent *BodyMakerUtils::SimpleLabelMaker( 
	const Label &lab, const BodyComponent &nc, FunctionBody &fnBody, const Position &ep ) 
{
	const FunctionBody::DefinedLabelList &pll = fnBody.GetDefinedLabelList();	
	SimpleLabelBodyComponent *nl = new SimpleLabelBodyComponent(lab, nc, ep);

	if( find_if(pll.begin(), pll.end(), LabelPredicat(lab.GetName()) ) != pll.end() )
		theApp.Error(ep, "'%s' - метка уже объявлена", lab.GetName().c_str());
	else
		fnBody.AddDefinedLabel(lab);
	return nl;
}


// вспомагательная функция, проверяет корректность возвращаемого значения для return
static void ValidateReturnValue( const POperand &rval, 
				const TypyziedEntity &rtype, const Position &ep )
{
	// проверим, если преобразование из классового в классовый,
	// значит к-ор копирования должен быть доступен
	if( !(rtype.GetBaseType().IsClassType() && rtype.GetDerivedTypeList().IsEmpty()) )
		return;
		
	// имеем классовое выражение
	const ClassType &cls = static_cast<const ClassType&>(rtype.GetBaseType());
	
	// проверяем не было ли уже преобразование
	const POperand &exp = rval;
	if( exp->IsExpressionOperand() &&
		static_cast<const Expression&>(*exp).IsFunctionCall() )
	{
		const FunctionCallExpression &fnc = 
			static_cast<const FunctionCallExpression&>(*exp);
		if( &fnc.GetFunctionOperand()->GetType().GetBaseType() == &cls	&&
			dynamic_cast<const ConstructorMethod *>(
			&fnc.GetFunctionOperand()->GetType()) != NULL )
			return;
	}
	
	// в противном случае, проверяем, чтобы у объекта был доступен к-тор копирования
	// и деструктор
	ExpressionMakerUtils::ObjectCreationIsAccessible(cls, ep, false, true, true);
}


// строитель return-операции. Конструктор не может возвращать значения, также
// как деструктор
ReturnAdditionalOperation *BodyMakerUtils::ReturnOperationMaker( 
		const POperand &rval, const Function &fn, const Position &ep )
{
	bool procedure = fn.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		fn.GetDerivedTypeList().GetDerivedTypeCount() == 1;

	// если имеем конструктор, то возвращаемого значения не должно быть
	if( fn.IsClassMember() && static_cast<const Method &>(fn).IsConstructor() )
	{
		if( !rval.IsNull() )
			theApp.Error( ep, "конструктор не может иметь возвращаемого значения" );
	}

	// выражение отсутствует, функция должна возвращать void
	else if( rval.IsNull() )
	{
		if( !procedure )
			theApp.Error( ep, "return должен возвращать значение" );
	}

	// иначе сравниваем тип возвращаемого значения и тип функции
	else if( !rval->IsErrorOperand() )
	{
		// получаем не функциональный ти
		PTypyziedEntity rtype = new TypyziedEntity(fn);
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();

		// приводим типы
		PCaster pc = AutoCastManager( *rtype, rval->GetType(), true ).RevealCaster();
		pc->ClassifyCast();
		if( !pc->IsConverted() )
		{
			if( pc->GetErrorMessage().empty() )
				theApp.Error(ep, 
					"'return' - невозможно преобразовать '%s' к '%s'",
					rval->GetType().GetTypyziedEntityName(false).c_str(),
					rtype->GetTypyziedEntityName(false).c_str());
			else
				theApp.Error(ep, "'return' - %s", pc->GetErrorMessage().c_str());			;
		}

		else
		{
			const TypyziedEntity &rt = *rtype.Release();
			POperand fnOp = new PrimaryOperand(true, rt);
			pc->DoCast(fnOp, const_cast<POperand&>(rval), ep);

			// проверяем доступность к-ра копирования, если требуется
			ValidateReturnValue(rval, rt, ep);
		}
	}
	
	// возвращаем результат
	return new ReturnAdditionalOperation(rval, ep);
}


// строитель break-операции с семантической проверкой 
BreakAdditionalOperation *BodyMakerUtils::BreakOperationMaker( 
								Construction &ppc, const Position &ep )
{
	const Construction *fnd = &ppc;
	while( fnd )
	{
		Construction::CC cc = fnd->GetConstructionID();
		if( cc == Construction::CC_SWITCH || cc == Construction::CC_FOR ||
			cc == Construction::CC_WHILE  || cc == Construction::CC_DOWHILE )
			break;
		fnd = fnd->GetParentConstruction();
	}

	if( !fnd )	
		theApp.Error( ep, "break без цикла или switch" );
	return new BreakAdditionalOperation(ep);
}


// строитель continue-операции с семантической проверкой 
ContinueAdditionalOperation *BodyMakerUtils::ContinueOperationMaker( 
								Construction &ppc, const Position &ep )
{
	const Construction *fnd = &ppc;
	while( fnd )
	{
		Construction::CC cc = fnd->GetConstructionID();
		if( cc == Construction::CC_FOR    ||
			cc == Construction::CC_WHILE  || cc == Construction::CC_DOWHILE )
			break;
		fnd = fnd->GetParentConstruction();
	}

	if( !fnd )	
		theApp.Error( ep, "continue без цикла" );
	return new ContinueAdditionalOperation(ep);
}


// строитель goto-операции
GotoAdditionalOperation *BodyMakerUtils::GotoOperationMaker( 
		const CharString &labName, FunctionBody &fnBody, const Position &ep )
{
	// задать телу функции обращение к метке
	fnBody.AddQueryLabel( labName.c_str(), ep );
	return new GotoAdditionalOperation(labName.c_str(), ep);
}


// проверяет, являются ли обработчики одинаковыми
bool CatchConstructionMaker::EqualCatchers( 
					const CatchConstruction &cc1, const CatchConstruction &cc2 ) const
{
	if( cc1.GetCatchType().IsNull() || cc2.GetCatchType().IsNull() )	
		return cc1.GetCatchType().IsNull() && cc2.GetCatchType().IsNull();
	
	ScalarToScalarCaster stsc(*cc1.GetCatchType(), *cc2.GetCatchType(), false);
	stsc.ClassifyCast();
	return stsc.IsConverted() && stsc.GetCastCategory() == Caster::CC_EQUAL;
}


// строить
CatchConstruction *CatchConstructionMaker::Make()
{
	TryCatchConstruction &tcc = static_cast<TryCatchConstruction &>(parent);	
	CatchConstruction *cc = new CatchConstruction(catchObj, &parent, ep);

	// проверяем, чтобы try не содержал одинаковых обработчиков
	for( CatchConstructionList::const_iterator p = tcc.GetCatchList().begin(); 
		 p != tcc.GetCatchList().end(); p++ )
		if( EqualCatchers( **p, *cc ) )
		{
			theApp.Error(ep, "catch-блок с типом '%s' уже присутствует",
				catchObj.IsNull() ? "..." : catchObj->GetTypyziedEntityName(false).c_str());
			break;
		}
	tcc.AddCatchConstruction( *cc );
	return cc;
}


// строитель списка инструкций. Может быть блок деклараций, либо выражение
Instruction *BodyMakerUtils::InstructionListMaker( 
				const InstructionList &insList, const Position &ep )
{
	// если список пустой, была ошибка, но в любом случае следует вернуть
	// не NULL, возвращаем пустую инструкцию
	if( insList.empty() )
		return SimpleComponentMaker<EmptyInstruction>(ep);

	// если инструкция одна, возвращаем ее саму
	else if( insList.size() == 1 )
		return const_cast<PInstruction &>(insList.front()).Release();

	// иначе инструкций несколько, формируем блок деклараций
	else
		return new DeclarationBlockInstruction(insList, ep);
}


// проверим, чтобы все метки, к которым идет обращение через
// goto, были объявлены
void PostBuildingChecks::CheckLabels( const FunctionBody::DefinedLabelList &dll, 
		const FunctionBody::QueryLabelList &qll )
{
	for( FunctionBody::QueryLabelList::const_iterator p = qll.begin(); p != qll.end(); p++ )
		if( find_if(dll.begin(), dll.end(), LabelPr( (*p).first ) ) == dll.end() )
			theApp.Error( (*p).second, "'%s' - метка не объявлена",
				(*p).first.c_str() );
}
