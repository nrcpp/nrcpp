// проверка корректности выражений - ExpressionChecker.cpp

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
#include "Checker.h"
#include "ExpressionMaker.h"


// статический список контроллеров
list<AgregatController *> ListInitializationValidator::allocatedControllers;


// метод проверяет является ли тип классовым
bool ExpressionMakerUtils::IsClassType( const TypyziedEntity &type )
{
	if( !type.GetBaseType().IsClassType() ||
		type.GetDerivedTypeList().GetDerivedTypeCount() > 1 )
		return false;

	return type.GetDerivedTypeList().IsEmpty() || type.GetDerivedTypeList().IsReference();
}


// склярный, значит не классовый и не функция и не void
bool ExpressionMakerUtils::IsScalarType( const TypyziedEntity &type )
{
	if( IsClassType(type) )
		return false;

	// тип функции
	if( type.GetDerivedTypeList().IsFunction() ||
		(type.GetDerivedTypeList().GetDerivedTypeCount() > 1 &&
		type.GetDerivedTypeList().IsReference() && type.GetDerivedTypeList().GetDerivedType(1)->
		GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) )
		return false;

	// тип void
	if( type.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		type.GetDerivedTypeList().IsEmpty() )
		return false;

	// иначе тип склярный
	return true;
}


// проверка, является ли операнд константным. Т.е. константым
// на самом верхнем уровне, либо указатель, либо базовый тип
bool ExpressionMakerUtils::IsConstant( const TypyziedEntity &op )
{	
	// проходим по списку производных типов сначала
	for( int i = 0; i < op.GetDerivedTypeList().GetDerivedTypeCount(); i++ )
	{
		const DerivedType &dt = *op.GetDerivedTypeList().GetDerivedType(i);
		if( dt.GetDerivedTypeCode() == DerivedType::DT_POINTER )
			return static_cast<const Pointer &>(dt).IsConst();

		else if( dt.GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
			return static_cast<const PointerToMember &>(dt).IsConst();
	}

	// иначе прошли весь список, проверяем базовый тип
	return op.IsConst();
}


// метод проверяет проверяет эквивалентность типов в контексте выражений.
// провряет только совпадение типов и производных типов, без учета квалификаторов
bool ExpressionMakerUtils::CompareTypes( 
		const TypyziedEntity &type1, const TypyziedEntity &type2 )
{
	// 1. проверяем базовые типы
	const BaseType &bt1 = type1.GetBaseType(),
				   &bt2 = type2.GetBaseType();

	// проверяем сначала коды, все коды должны совпадать
	if( &bt1 != &bt2								   ||
		bt1.GetSignModifier() != bt2.GetSignModifier() ||
		bt1.GetSizeModifier() != bt2.GetSizeModifier() )
		return false;

	// 3. проверяем список производных типов
	const DerivedTypeList &dtl1 = type1.GetDerivedTypeList(),
						  &dtl2 = type2.GetDerivedTypeList();

	// 3а. размеры списков должны совпадать
	if( dtl1.GetDerivedTypeCount() != dtl2.GetDerivedTypeCount() )
		return false;

	// проходим по всему списку проверяя каждый производный тип по отдельности
	for( int i = 0; i<dtl1.GetDerivedTypeCount(); i++ )
	{
		const DerivedType &dt1 = *dtl1.GetDerivedType(i),
						  &dt2 = *dtl2.GetDerivedType(i);

		// если коды производных типов не равны выходим
		if( dt1.GetDerivedTypeCode() != dt2.GetDerivedTypeCode() )
			return false;

		// иначе проверяем семантическое значение производного типа
		DerivedType::DT dc1 = dt1.GetDerivedTypeCode(), 
						dc2 = dt2.GetDerivedTypeCode();

		// если ссылка или указатель, то проверять нечего
		if( dc1 == DerivedType::DT_REFERENCE || dc2 == DerivedType::DT_POINTER )
			;

		// если указатель на член проверим квалификаторы и класс к которому
		// принадлежит указатель
		else if( dc1 == DerivedType::DT_POINTER_TO_MEMBER )
		{
			const PointerToMember &ptm1 = static_cast<const PointerToMember &>(dt1),
								  &ptm2 = static_cast<const PointerToMember &>(dt2);

			// проверим, чтобы классы совпадали
			if( &ptm1.GetMemberClassType() != &ptm2.GetMemberClassType() )
				return false;
		}

		// если массив, проверим размеры двух массивов
		else if( dc1 == DerivedType::DT_ARRAY )
		{
			if( dt1.GetDerivedTypeSize() != dt2.GetDerivedTypeSize() )
				return false;
		}

		// если прототип функции, то применим данную функцию для каждого параметра,
		// а также проверим cv-квалификаторы и throw-спецификацию
		else if( dc1 == DerivedType::DT_FUNCTION_PROTOTYPE )
		{
			const FunctionPrototype &fp1 = static_cast<const FunctionPrototype &>(dt1),
								    &fp2 = static_cast<const FunctionPrototype &>(dt2);	
			
			// количество параметров должно совпадать
			const FunctionParametrList &fpl1 = fp1.GetParametrList(),
									   &fpl2 = fp2.GetParametrList();

			if( fpl1.GetFunctionParametrCount() != fpl2.GetFunctionParametrCount() )
				return false;

			// проверяем каждый параметр в списке на соответствие
			for( int i = 0; i<fpl1.GetFunctionParametrCount(); i++ )
				if( !CompareTypes(*fpl1[i], *fpl2[i]) )
					return false;					
		}

		// иначе неизвестный код
		else
			INTERNAL("'ExpressionMakerUtils::CompareTypes' неизвестный код производного типа");
	}

	return true;	
}


// создать вызов функции
POperand ExpressionMakerUtils::MakeFunctionCall( POperand &fn, PExpressionList &params )
{
	// создаем результуриующий тип вызова функции
	PTypyziedEntity rt = new TypyziedEntity(fn->GetType());	
	const_cast<DerivedTypeList &>(rt->GetDerivedTypeList()).PopHeadDerivedType();
	
	// возвращаем вызов функции
	return new FunctionCallExpression( rt->GetDerivedTypeList().IsReference(),
		fn, params, rt);
}


// получить строковое представление оператора по коду
PCSTR ExpressionPrinter::GetOperatorName( int opCode )
{	
	// если оператор требует считывания []
	switch( opCode )
	{
	case KWNEW:				return "new";		
	case -KWDELETE:		
	case KWDELETE:			return "delete";	
	case OC_NEW_ARRAY:		return "new[]";	
	case -OC_DELETE_ARRAY:  
	case OC_DELETE_ARRAY:	return "delete[]";	
	case OC_FUNCTION:		return "()";		
	case OC_ARRAY:			return "[]";		
	case PLUS_ASSIGN:		return "+=";		
	case MINUS_ASSIGN:		return "-=";		
	case MUL_ASSIGN:		return "*=";		
	case DIV_ASSIGN:		return "/=";		
	case PERCENT_ASSIGN:	return "%=";		
	case LEFT_SHIFT_ASSIGN:	return "<<=";		
	case RIGHT_SHIFT_ASSIGN:return ">>=";		
	case AND_ASSIGN:		return "&=";		
	case XOR_ASSIGN:		return "^=";		
	case OR_ASSIGN:			return "|=";		
	case LEFT_SHIFT:		return "<<";		
	case RIGHT_SHIFT:		return ">>";		
	case LESS_EQU:			return "<=";		
	case GREATER_EQU:		return ">=";		
	case EQUAL:				return "==";		
	case NOT_EQUAL:			return "!=";		
	case LOGIC_AND:			return "&&";		
	case LOGIC_OR:			return "||";		
	case -INCREMENT:
	case INCREMENT:			return "++";		
	case -DECREMENT:
	case DECREMENT:			return "--";		
	case ARROW:				return "->";		
	case ARROW_POINT:		return "->*";		
	case DOT_POINT:			return ".*";		
	case KWTHROW:			return "throw";
	case KWTYPEID:			return "typeid";
	case KWSIZEOF:			return "sizeof";
	case KWREINTERPRET_CAST: return "reinterpret_cast";
	case KWSTATIC_CAST:		 return "static_cast";
	case KWCONST_CAST:	 	 return "const_cast";
	case KWDYNAMIC_CAST:     return "dynamic_cast";	
	case GOC_REFERENCE_CONVERSION: return "*";	
	default:
		{
			static char buf[2] = " ";
			buf[0] = opCode;
			return buf;
		}		
	}	
}


// распечатать бинарное выражение
string ExpressionPrinter::PrintBinaryExpression( const BinaryExpression &expr )
{
	string op = GetOperatorName(expr.GetOperatorCode());

	// проверяем какой оператор
	switch( expr.GetOperatorCode() ) 
	{
	case OC_CAST:
	case GOC_BASE_TO_DERIVED_CONVERSION:
	case GOC_DERIVED_TO_BASE_CONVERSION:
		return " ((" + PrintExpression(expr.GetOperand1()) + ')' +
				PrintExpression(expr.GetOperand2()) + ") ";

	case KWREINTERPRET_CAST:
	case KWSTATIC_CAST:
	case KWCONST_CAST:
	case KWDYNAMIC_CAST:
		return op + '<' + PrintExpression(expr.GetOperand1()) + ">(" +
				PrintExpression(expr.GetOperand2()) + ')';
	case OC_ARRAY:
		return PrintExpression(expr.GetOperand1()) + '[' +
			   PrintExpression(expr.GetOperand2()) + ']';
	default:
		return PrintExpression(expr.GetOperand1()) + ' ' + op + ' ' +
			   PrintExpression(expr.GetOperand2());
	}
}


// распечатать выражение
string ExpressionPrinter::PrintExpression( const POperand &expr )
{
	// если выражение, распечатаем операнды и оператор рекурсивно
	if( expr->IsExpressionOperand() )
	{
		const Expression &expop = static_cast<const Expression&>(*expr);
		
		// если выражение является унарным
		if( expop.IsUnary() )
		{
			string op = GetOperatorName(expop.GetOperatorCode()),
				   temp = PrintExpression( 
							static_cast<const UnaryExpression&>(expop).GetOperand() );

			int opC = expop.GetOperatorCode();
			if( opC == INCREMENT || opC == DECREMENT )
				return temp + op;

			else if( opC == KWSIZEOF || opC == KWTYPEID )
				return op + '(' + temp + ')';

			else if( abs(opC) == KWDELETE ||  opC == KWTHROW )
				return op + ' ' + temp;
			else
				return op + temp;
		}

		else if( expop.IsBinary() )
			return PrintBinaryExpression( static_cast<const BinaryExpression&>(expop) );

		else if( expop.IsTernary() )
		{
			const TernaryExpression &tern = static_cast<const TernaryExpression&>(expop);
			return PrintExpression(tern.GetOperand1()) + " ? " +
				   PrintExpression(tern.GetOperand2()) + " : " +
				   PrintExpression(tern.GetOperand3());
		}

		else if( expop.IsFunctionCall() || expop.IsNewExpression() )
		{
			const Operand *pfce = &expop;
			if( expop.IsNewExpression() )
				pfce = &*static_cast<const NewExpression &>(expop).GetNewOperatorCall();
			const FunctionCallExpression &fce = 
				static_cast<const FunctionCallExpression&>(*pfce);
			string temp = PrintExpression(fce.GetFunctionOperand()) + '(';
			for( int i = 0; i<fce.GetParametrList()->size(); i++ )
			{
				temp += PrintExpression(fce.GetParametrList()->at(i));
				if( i != fce.GetParametrList()->size()-1 )
					temp += ", ";
			}

			temp += ')';
			return temp;
		}

		else
			return "<неизвестное выражение>";
	}

	else if( expr->IsPrimaryOperand() )
	{
		const TypyziedEntity &te = expr->GetType().IsDynamicTypyziedEntity()		 ?
			static_cast<const DynamicTypyziedEntity&>(expr->GetType()).GetOriginal() : 
			expr->GetType();
		if( te.IsLiteral() )
			return static_cast<const Literal &>(te).GetLiteralValue().c_str();

		else if( const Identifier *id = dynamic_cast<const Identifier *>(&te) )
			return id->GetQualifiedName().c_str();

		else if( &te == &expr->GetType() )
			return " this ";

		else
			return te.GetTypyziedEntityName(false).c_str();
	}

	else if( expr->IsTypeOperand() )
		return expr->GetType().GetTypyziedEntityName(false).c_str();

	else if( expr->IsErrorOperand() )
		return "<error operand>";

	else if( expr->IsOverloadOperand() )
		return "<overload operand>";

	else
	{
		INTERNAL( "'ExpressionMakerUtils::PrintExpression' - неизвестный операнд" );
		return "";
	}
}


// выявить преобразователя на основе двух типов
PCaster &AutoCastManager::RevealCaster()
{
	register bool cls1 = ExpressionMakerUtils::IsClassType(destType), 
				  cls2 = ExpressionMakerUtils::IsClassType(srcType);

	// для начала, если имеем копирование, то возможно потребуется
	// преобразование из склярного в классовый
	if( isCopy && cls1 && !cls2 )
		return caster = new ConstructorCaster(destType, srcType, explicitCast);	

	// на основе имеющихся данных, выявляем 
	if( cls1 )
		return caster = (cls2 ? (Caster *)new ClassToClassCaster(destType, srcType, explicitCast) 
					          : (Caster *)new OperatorCaster(destType, srcType, isCopy));
	else if( cls2 )
		return caster = new OperatorCaster(destType, srcType, isCopy);

	// иначе склярный приводится к склярному
	else
		return caster = new ScalarToScalarCaster(destType, srcType, isCopy);
}


// метод выбирает наибольший тип из двух по правилам языка
const TypyziedEntity *ScalarToScalarCaster::GetBiggerType()
{
	// сначала проверяем, если оба типа одинаковы
	if( &destType.GetBaseType() == &srcType.GetBaseType() )
	{		
		castCategory = CC_EQUAL;
		return &destType;
	}

	const BaseType &bt1 = destType.GetBaseType(), &bt2 = srcType.GetBaseType();
	BaseType::BT btc1 = bt1.GetBaseTypeCode(), 
			     btc2 = bt2.GetBaseTypeCode();

	// если один из типов double, значит и другой double
	if( btc1 == BaseType::BT_DOUBLE )
	{
		if( btc2 == BaseType::BT_DOUBLE )
			return bt1.IsLong() ? &destType : &srcType;
		else
			return &destType;
	}

	else if( btc2 == BaseType::BT_DOUBLE )
		return &srcType;

	// если один из типов float, значит и другой
	else if( btc1 == BaseType::BT_FLOAT )
		return &destType;
		
	else if( btc2 == BaseType::BT_FLOAT )
		return &srcType;
	
	// иначе int
	else
	{
		bool uns = bt1.IsUnsigned() || bt2.IsUnsigned(),
			 lng = bt1.IsLong() || bt2.IsLong();
		BaseType *rbt = (BaseType *)&ImplicitTypeManager(KWINT, uns ? KWUNSIGNED : -1, 
							lng ? KWLONG : -1).GetImplicitType();
		resultType = new TypyziedEntity(rbt, false, false, DerivedTypeList());
		return resultType ;
	}
}

// выполнить стандартные преобразования из массива в указатель,
// из функции в указатель на функцию, и из метода в указатель на член фукнцию,
// если требуется
bool ScalarToScalarCaster::StandardPointerConversion( TypyziedEntity &type,
									const TypyziedEntity &orig )
{
	DerivedTypeList &dtl = const_cast<DerivedTypeList &>(type.GetDerivedTypeList());

	// если имеем массив, преобразуем его в указатель
	if( dtl.IsArray() )
		dtl.PopHeadDerivedType(),
		dtl.PushHeadDerivedType( new Pointer(false,false) );
	
	// преобразуем в указатель на функцию или в указатель на функцию-член
	else if( dtl.IsFunction() )		
	{
		const Function &fn = *dynamic_cast<const Function *>(&orig);

		// если нестатическа функция-член, преобразуем в указатель на ф-ч
		if( &fn && fn.IsClassMember() && fn.GetStorageSpecifier() != Function::SS_STATIC )
			dtl.PushHeadDerivedType( new PointerToMember(
				&static_cast<const ClassType&>(fn.GetSymbolTableEntry()), false, false) );
		else
			dtl.PushHeadDerivedType( new Pointer(false,false) );
	}	

	return true;
}


// преобразование производного к базовому, только классы уже известны
int ScalarToScalarCaster::DerivedToBase( 
					const ClassType &base, const ClassType &derived, bool ve )
{
	DerivationManager dm( base, derived );
	if( dm.IsBase() )
	{
		// если базовый класс неоднозначен
		if( !dm.IsUnambigous() )
		{
			errMsg = ('\'' + string(base.GetQualifiedName().c_str()) +
				"\' неоднозначный базовый класс; приведение типа невозможно").c_str();
			return -1;
		}
		
		// если виртуальный запрещен и класс виртуален
		else if( ve && dm.IsVirtual() )
		{
			errMsg = ('\'' + string(base.GetQualifiedName().c_str()) +
				"\' виртуальный базовый класс; приведение типа невозможно").c_str();
			return -1;
		}


		// если класс недоступен, сохраним сообщение об этом
		else if( !dm.IsAccessible() )
		{
			errMsg = ('\'' + string(base.GetQualifiedName().c_str()) +
				"\' недоступный базовый класс; приведение типа невозможно").c_str();
			return -1;
		}

		// иначе приведение успешно
		else
		{
			isConverted = true;	
			castCategory = CC_STANDARD;
			derivedToBase = true;
			return 0;
		}
	}

	return 1;
}


// преобразование производного к базовому, если возможно,
// преобразовать и вернуть 0. Возвращает -1, если преобразование
// прошло с ошибкой, Иначе > 0.
int ScalarToScalarCaster::DerivedToBase( 
		const TypyziedEntity &base, const TypyziedEntity &derived, bool ve )
{
	// оба типа должны быть классовыми
	if( !base.GetBaseType().IsClassType() || !derived.GetBaseType().IsClassType() )
		return 1;

	const ClassType &cls1 = static_cast<const ClassType&>(base.GetBaseType()),
					&cls2 = static_cast<const ClassType&>(derived.GetBaseType());

	int r = DerivedToBase(cls1, cls2, ve);
	if( !r )
		resultType = &base;
	return r;
}


// определить категорию преобразования для арифметических типов,
// учитывается порядок типов в случае копирования
void ScalarToScalarCaster::SetCastCategory( const BaseType &dbt, const BaseType &sbt )
{
	INTERNAL_IF( resultType == NULL );
	if( castCategory != CC_NOCAST )
		return;

	// если оба типа одинаковы, задать это
	if( &dbt == &sbt )
	{
		castCategory = CC_EQUAL;
		return;
	}

	// категорию нужно задавать только для разрешения неоднозначности,
	// поэтому если не копирование, задаем стандартное преобразование и все
	if( !isCopy )
	{
		castCategory = CC_STANDARD;
		return;
	}

	// sbt приводился к dbt, выясняем какое это преобразование. 
	// Наша задача выявить было ли продвижение, если не было значит
	// преобразование стандартное. Продвижение тольк в случае если оба типа
	// интегральные и sbt меньше dbt, либо если sbt - float, а dbt - double
	register BaseType::BT dc = dbt.GetBaseTypeCode(), sc = sbt.GetBaseTypeCode();
	if( ExpressionMakerUtils::IsIntegral(destType) && 
		ExpressionMakerUtils::IsIntegral(srcType) )
	{
		if( dc > sc )
			castCategory = CC_INCREASE;

		else if( dc == sc )
		{
		    if( dbt.GetSizeModifier() == sbt.GetSizeModifier() )
				castCategory = dbt.GetSignModifier() >= sbt.GetSignModifier() ?
					CC_INCREASE : CC_STANDARD;
			else
		       castCategory = 
				(sbt.IsShort() && dbt.GetSizeModifier() == BaseType::MZ_NONE &&
				 dbt.GetSignModifier() == sbt.GetSignModifier()) ?
					CC_INCREASE : CC_STANDARD;			
		}
		    
		else
			castCategory = CC_STANDARD;
	}

	// иначе если sbt-float, dbt-double
	else if( sc == BaseType::BT_FLOAT && dc == BaseType::BT_DOUBLE )
		castCategory = CC_INCREASE;

	// иначе стандартное преобразование
	else
		castCategory = CC_STANDARD;
}


// проверка квалификации, в случае удачного преобразования. Имеет
// значение только при копировании. Правый операнд должен быть менее
// квалифицирован, чем правый. В случае, если квалификация 'src',
// больше чем 'dest' вернет false
bool ScalarToScalarCaster::QualifiersCheck( TypyziedEntity &dest, TypyziedEntity &src )
{
	DerivedTypeList &dtl1 = const_cast<DerivedTypeList &>(dest.GetDerivedTypeList()),
					&dtl2 = const_cast<DerivedTypeList &>(src.GetDerivedTypeList());
	
	if( !origDestType.GetDerivedTypeList().IsEmpty() &&
		(origDestType.IsConst() < src.IsConst() || 
		origDestType.IsVolatile() < src.IsVolatile())  )
		return false;

	// сначала проверяем, производных типов нет
	bool notEq = dest.IsConst() != src.IsConst() || dest.IsVolatile() != src.IsVolatile();
	for( int i = 1; i<dtl1.GetDerivedTypeCount(); i++ )
	{
		const DerivedType &d1 = *dtl1.GetDerivedType(i), &d2 = *dtl2.GetDerivedType(i);
		if( d1.GetDerivedTypeCode() != d2.GetDerivedTypeCode() )
			return true;

		bool c1, c2, v1, v2;
		if( d1.GetDerivedTypeCode() == DerivedType::DT_POINTER )
		{
			c1 = static_cast<const Pointer &>(d1).IsConst();
			c2 = static_cast<const Pointer &>(d2).IsConst();
			v1 = static_cast<const Pointer &>(d1).IsVolatile();
			v2 = static_cast<const Pointer &>(d2).IsVolatile();
		}

		else if( d1.GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
		{
			c1 = static_cast<const PointerToMember &>(d1).IsConst();
			c2 = static_cast<const PointerToMember &>(d2).IsConst();
			v1 = static_cast<const PointerToMember &>(d1).IsVolatile();
			v2 = static_cast<const PointerToMember &>(d2).IsVolatile();
		}

		else
			return true;

		// если не равно, const должен присутствовать
		if( notEq )
			if( !c1 )
				return false;
			else
				continue;

		// иначе проверяем чтобы квалификация совпадала
		else
			notEq = c1 != c2 || v1 != v2;
	}

	// если квалификация не равна, было преобразование,
	// следовательно нужно изменить категорию
	if( notEq && castCategory == CC_EQUAL )		
		castCategory = CC_QUALIFIER;

	return true;
}


// классифицировать преобразование. Логически проверить возможность
// преобразования одного типа в другой, а также собрать необходимую
// информацию для физического преобразования
void ScalarToScalarCaster::ClassifyCast()
{
	// получаем списки производных типов
	DerivedTypeList &ddtl = const_cast<DerivedTypeList &>(destType.GetDerivedTypeList()),
				    &sdtl = const_cast<DerivedTypeList &>(srcType.GetDerivedTypeList());

	// сначала преобразуем ссылочный тип в не-ссылочный, если требуется
	if( isRefConv1 )
		const_cast<DerivedTypeList &>(ddtl).PopHeadDerivedType();

	if( isRefConv2 )
		const_cast<DerivedTypeList &>(sdtl).PopHeadDerivedType();

	// выполняем стандартные преобразования указателей. Из массива, функции, метода
	if( (isCopy && !StandardPointerConversion(destType, origDestType)) ||
		 !StandardPointerConversion(srcType, origSrcType) )
		return;

	// если оба типа арифметические
	if( ExpressionMakerUtils::IsArithmetic(destType) &&
		ExpressionMakerUtils::IsArithmetic(srcType) )
	{
		// если копирование, то сохраняем правый тип в качестве результирующего.
		// Но есть один момент. Если слева находится перечислимый тип, то и справа
		// должен быть такой же перечислимый тип
		if( isCopy )
		{
			// ошибка, перечислимые типы не совпадают
			if( destType.GetBaseType().IsEnumType() && 
				&destType.GetBaseType() != &srcType.GetBaseType() )
				;
			else			
				resultType = &destType,
				isConverted = true,
				SetCastCategory( destType.GetBaseType(), srcType.GetBaseType() );			
		}

		else
		{
			resultType = GetBiggerType();
			isConverted = true;
			SetCastCategory( destType.GetBaseType(), srcType.GetBaseType() );
		}
	}

	// если один из типов указатель или указатель на член, а второй
	// целый
	else if( ( (ddtl.IsPointer() || ddtl.IsPointerToMember()) && 
				ExpressionMakerUtils::IsIntegral(srcType) ) 			||
			 ( (sdtl.IsPointer() || sdtl.IsPointerToMember()) && 
			    ExpressionMakerUtils::IsIntegral(destType) ) )
	{
		const TypyziedEntity &lit = destType.GetDerivedTypeList().IsPointer() || 
			destType.GetDerivedTypeList().IsPointerToMember() ? origSrcType : origDestType;

		// пробуем преобразовать в нулевой указатель
		if( lit.IsLiteral() && 
			atoi(static_cast<const Literal&>(lit).GetLiteralValue().c_str()) == 0 )
		{
			resultType = &lit == &origSrcType ? &destType : &srcType;
			castCategory = CC_STANDARD;
			isConverted = true;

			// при преобразовании в нулевой указатель, проверки на квалифицированность
			// не выполняются и можно сразу выйти
			return;
		}
	}

	// если оба типа указатели, пытаемся преобразовать базовый тип к производному,
	// либо один указатель к типу void *, либо оба указателя имеют один тип
	else if( ddtl.IsPointer() && sdtl.IsPointer()  )
	{
		// проверяем, если один из указателей, указатель на 'void',
		// то преобразование успешно
		if( (ddtl.GetDerivedTypeCount() == 1 && 
			 destType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID) )			
			isConverted = true, resultType = &destType,
			castCategory = CC_STANDARD;

		// если не копирование, то и в случае со вторым операндом преобр. успешно
		else if( !isCopy							&&
			sdtl.GetDerivedTypeCount() == 1			&&
			srcType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID )			
			isConverted = true, resultType = &srcType,
			castCategory = CC_STANDARD;				


		// если количество производных типов разное, дальнейшие проверки
		// не имеют смысла
		else if( ddtl.GetDerivedTypeCount() != sdtl.GetDerivedTypeCount() )
			;

		// если количество производных типов больше одного,
		// то проверим лишь их сходство
		else if( ddtl.GetDerivedTypeCount() > 1 )
		{
			isConverted = ExpressionMakerUtils::CompareTypes(destType, srcType);
			if( isConverted )			
				resultType = &destType,
				castCategory = CC_EQUAL;				
		}

		// если базовые типы равны, то приведение успешно в любом случае
		else if( &destType.GetBaseType() == &srcType.GetBaseType() )
			isConverted = true,	resultType = &destType,
			castCategory = CC_EQUAL;

		// иначе имеем два указателя. Если операция копирования, правый
		// тип можно преобразовать к левому если он 'void *'
		else if( isCopy )
		{					
			// если левый и правый операнд имеет тип класса, пытаемся преобразовать
			// правый к левому, если он производный класс
			if( !DerivedToBase(destType, srcType, false) )
				castCategory = CC_STANDARD;		

		}	// приведение указателей, только в случае копирования

		// иначе проверка обоих типов на базовый-производный и на void
		else
		{
			// сначала пытаемся преобразовать один класс к другому,
			// потом наоброт			
			if( DerivedToBase(destType, srcType, false) > 0 )
				DerivedToBase(srcType, destType, false);			
		}
	}

	// если оба типа - указатели на член, проверим их соотв.
	else if( ddtl.IsPointerToMember() && sdtl.IsPointerToMember() )
	{
		// проверим, соотв. двух типов без указателей на член
		PDerivedType ptm1( ddtl[0] ), ptm2( sdtl[0] );
		ddtl.PopHeadDerivedType();
		sdtl.PopHeadDerivedType();

		if( ExpressionMakerUtils::CompareTypes(destType, srcType) )
		{	
			// типы указателей на член совпали, проверим классы	
			const ClassType &cls1 = static_cast<PointerToMember &>(*ptm1).GetMemberClassType(), 
							&cls2 = static_cast<PointerToMember &>(*ptm2).GetMemberClassType();

			// если классы совпадают, значит типы одинаковы
			if( &cls1 == &cls2 )
				isConverted = true,
				resultType = &destType,
				castCategory = CC_EQUAL;

			// иначе проверяем, является ли один из классов производным
			// другого
			else if( int r = DerivedToBase(cls1, cls2, true) )
			{
				if( r > 0 && !isCopy && !DerivedToBase(cls2, cls1, true) )
					resultType = &srcType;
			}

			else
				resultType = &destType;
		}

		ddtl.PushHeadDerivedType(ptm1);
		sdtl.PushHeadDerivedType(ptm2);

		// преобразование указателя на член, не требует явного преобразования
		// при генерации кода, поэтому не будем изменять дерево выражений
		derivedToBase = false;		
	}

	// в последнюю очередь проверяем, совпадают ли типы
	else
	{
		// в случае копирования, когда оба типа классовые следует проверить
		// является ли правый тип производным классом левого
		if( isCopy && ExpressionMakerUtils::IsClassType(destType) &&
			ExpressionMakerUtils::IsClassType(srcType) )
		{
			const ClassType &cls1 = static_cast<const ClassType &>(destType.GetBaseType()), 
							&cls2 = static_cast<const ClassType &>(srcType.GetBaseType());

			// если оба типа совпадают, приведение успешно
			if( &cls1 == &cls2 )
				isConverted = true,
				resultType = &destType, castCategory = CC_EQUAL;

			// иначе если правый является производным классом левого
			else if( !DerivedToBase(cls1, cls2, false) )
				resultType = &destType;
		}

		else if( ExpressionMakerUtils::CompareTypes(destType, srcType) )
		{
			isConverted = true; 
			resultType = &destType;
			castCategory = CC_EQUAL;
		}
	}

	// если преобразование невозможно, и сообщение не задано, задаем
	if( !isConverted )
	{
		if( errMsg.empty() )
			errMsg = ("невозможно преобразовать '" + string(
				srcType.GetTypyziedEntityName().c_str()) + "' к '" +
				destType.GetTypyziedEntityName().c_str() + '\'').c_str();
	}

	// результирующий тип должен задаваться в любом случае
	else
	{
		INTERNAL_IF( resultType == NULL || castCategory == CC_NOCAST );

		// в последнюю очередь проверяем квалификацию
		if( isCopy && !QualifiersCheck(destType, srcType) )
		{
			errMsg = ("'" + string(
				srcType.GetTypyziedEntityName().c_str()) + "' менее квалифицирован чем '" +
				destType.GetTypyziedEntityName().c_str() + '\'').c_str();
			isConverted = false;
			resultType = NULL;
			castCategory = CC_NOCAST;
		}
	}	
}


// выполнить преобразование из производного класса в базовый
// внеся изменение в дерево выражений. Приводит правый операнд (derived),
// к типу левого (base)
void ScalarToScalarCaster::MakeDerivedToBaseConversion( 
  const TypyziedEntity &baseClsType, const POperand &base, POperand &derived )
{
	// создаем новый тип, на основе предыдущего
	TypyziedEntity *rt = new TypyziedEntity(baseClsType);
	POperand pop = new TypeOperand( *new TypyziedEntity(baseClsType) );
	derived = new BinaryExpression( GOC_DERIVED_TO_BASE_CONVERSION, 
		baseClsType.GetDerivedTypeList().IsReference(), pop, derived, rt );		
}


// выполнить физическое преобразование, изменив 
// содержимое дерева выражений
void ScalarToScalarCaster::DoCast( const POperand &destOp, POperand &srcOp, const Position &ep )
{
	INTERNAL_IF( resultType == NULL || castCategory == CC_NOCAST || !isConverted );

	// невозможно преобразовать rvalue в lvalue при копировании
	if( isCopy && isRefConv1 &&
		!ExpressionMakerUtils::IsConstant(destOp->GetType())	&&
		!ExpressionMakerUtils::IsLValue(srcOp) && errMsg.empty() )
		theApp.Error(ep, "невозможно преобразовать 'rvalue' в 'lvalue'");

	// оформляем преобразование, только в случае если было преобразование
	// из производного класса в базовый
	if( derivedToBase )
	{
		if( resultType == &destType )
			MakeDerivedToBaseConversion( origDestType, destOp, srcOp );
		else
		{
			// при копировании не может изменяться destOp
			INTERNAL_IF( isCopy );	
			MakeDerivedToBaseConversion( origSrcType, srcOp, const_cast<POperand &>(destOp) );
		}		
	}
}


// проверяет соотв. типа оператора с задаваемым типом, основан на явном
// сравнении типов с помощью ScalarToScalarCaster
int OperatorCaster::COSCast( const ClassCastOverloadOperator &ccoo )
{
	// сразу проверяем, чтобы перегруженный оператор был более квалифицирован
	// чем тип (объект). А также чтобы тип был склярным
	if( ccoo.GetFunctionPrototype().IsConst() < clsType.IsConst() ||
		ccoo.GetFunctionPrototype().IsVolatile() < clsType.IsVolatile() )
		return 1;

	// пытаемся привести склярный тип оператора к необходимому склярному типу	
	PScalarToScalarCaster stsc =
		 new ScalarToScalarCaster(scalarType, ccoo.GetCastType(), true);
	stsc->ClassifyCast();

	// если преобразование невозможно - выходим
	if( !stsc->IsConverted() )
		return 1;

	// если это первое удачное преобразование, сохраняем его
	// без дополнительных проверок
	if( castOperator == NULL )
	{
		castOperator = &ccoo;
		scalarCaster = stsc;
		return 0;
	}

	// иначе проверяем, какое соотв. наилучшее. Может быть неоднозначность
	else
	{
		// если у текущего преобразования уровень больше, значит ничего не делаем
		if( stsc->GetCastCategory() > scalarCaster->GetCastCategory() )
			return 1;

		// иначе если у текущего преобразования уровень меньше, значит задаем его
		else if( stsc->GetCastCategory() < scalarCaster->GetCastCategory() )
		{	
			castOperator = &ccoo;			
			scalarCaster = stsc;
			return 0;
		}

		// иначе неоднозначность, оба преобразования имеют один уровень.
		// Хотя операторы с одним именем, находящиеся водной иерархии,
		// могут перекрывать друг друга, проверим это
		else
		{						
			if( ccoo.GetName() == castOperator->GetName() )
			{
				// сначала проверим, если квалификаторы разные, то более квалифицированный
				// выбирается, в случае, если объект также имеет квалифицированный тип
				bool c1 = castOperator->GetFunctionPrototype().IsConst(),
					 v1 = castOperator->GetFunctionPrototype().IsVolatile(),
					 c2 = ccoo.GetFunctionPrototype().IsConst(),
					 v2 = ccoo.GetFunctionPrototype().IsVolatile();

				if( c1 != c2 || v1 != v2 )					
				{
					int cv1 = (c1 == scalarType.IsConst()) + (v1 == scalarType.IsVolatile());
					int cv2 = (c2 == scalarType.IsConst()) + (v2 == scalarType.IsVolatile());

					// если первый оператор более квалифицирован чем второй,
					// выходим
					if( cv1 >= cv2 )
						return 1;

					// иначе меняем оператор
					else
					{
						castOperator = &ccoo;			
						scalarCaster = stsc;
						return 0;
					}
				}

				const SymbolTable *st = dynamic_cast<const SymbolTable *>(
					static_cast<const ClassType *>(&clsType.GetBaseType()) );
				INTERNAL_IF( st == NULL );
				NameManager nm(ccoo.GetName(), st);
				INTERNAL_IF( nm.GetRoleCount() == 0 );

				// только если имя уникально и указатель на него равен нашему оператору
				if( nm.IsUnique() )
				{
					// если оператор найден, следует его заменить
					if( nm.GetRoleList().front().first == (Identifier *)&ccoo )
					{
						castOperator = &ccoo;			
						scalarCaster = stsc;
						return 0;
					}
		

					// иначе имеющ. оператор находится ниже в иерархии
					else
						return 1;
				}				
			}

			// иначе неоднозначность
			return -1;
		}
	}

	return 1;
}


// сравнение квалификаций у кандидата и текущего рассматриваемого оператора,
// по квалификации выявляется возвращаемое значение
int OperatorCaster::CVcmp( const ClassCastOverloadOperator &ccoo )
{
	bool curC = ccoo.GetFunctionPrototype().IsConst(),
		 curV = ccoo.GetFunctionPrototype().IsVolatile();

	// если квалификация меньше, чем у объекта - оператор не подходит
	if( curC < clsType.IsConst() || curV < clsType.IsVolatile() )
		return 1;

	// если квалификация больше соотв. классу, чем текущий кандитат
	if( castOperator != NULL )
	{
		int candCV = (castOperator->GetFunctionPrototype().IsConst() == clsType.IsConst()) +
				(castOperator->GetFunctionPrototype().IsVolatile() == clsType.IsVolatile()),
			curCV = (curC == clsType.IsConst()) + (curV == clsType.IsVolatile());
				 
		// если у текущего квалификация лучше, вернуть 0
		if( curCV > candCV )
			return 0;

		// если равны, значит неоднозначность, но мы еще проверим,
		// возможно один оператор перекрывает другой
		else if( curCV == candCV )
		{
			// если имена операторов не равны, сразу выйдем
			if( castOperator->GetName() != ccoo.GetName() )
				return -1;

			// иначе ищем оператор начиная с его области видимости
			const SymbolTable *st = dynamic_cast<const SymbolTable *>(
					static_cast<const ClassType *>(&clsType.GetBaseType()) );
			INTERNAL_IF( st == NULL );
			NameManager nm(ccoo.GetName(), st);
			INTERNAL_IF( nm.GetRoleCount() == 0 );

			// только если имя уникально и указатель на него равен нашему оператору
			if( nm.IsUnique() )
			{
				// если оператор найден, следует его заменить
				if( nm.GetRoleList().front().first == (Identifier *)&ccoo )				
					return 0;

				// иначе имеющ. оператор находится ниже в иерархии
				else
					return 1;
			}				
			
			// иначе неоднозначность
			return -1;
		}

		// иначе оставляем все как есть
		else
			return 1;
	}

	else
		return 0;
}


// проверяет, является ли тип оператора арифметическим
int OperatorCaster::COSArith( const ClassCastOverloadOperator &ccoo )
{
	// если тип арифметический, выполняем дальнейшие проверки
	if( ExpressionMakerUtils::IsArithmetic(ccoo.GetCastType()) )
		return CVcmp(ccoo);
	else
		return 1;
}


// проверяет, является ли тип оператора указателем
int OperatorCaster::COSPointer( const ClassCastOverloadOperator &ccoo )
{
	// если тип указатель, выполняем дальнейшие проверки
	if( ccoo.GetCastType().GetDerivedTypeList().IsPointer() )
		return CVcmp(ccoo);
	else
		return 1;
}


// проверяет, является ли тип оператора целым
int OperatorCaster::COSIntegral( const ClassCastOverloadOperator &ccoo )
{
	// если тип целый, выполняем дальнейшие проверки
	if( ExpressionMakerUtils::IsIntegral(ccoo.GetCastType()) )
		return CVcmp(ccoo);
	else
		return 1;
}

// проверяет, является ли операнд арифметическим или указателем
int OperatorCaster::COSArithmeticOrPointer( const ClassCastOverloadOperator &ccoo )
{
		// если тип целый, выполняем дальнейшие проверки
	if( ExpressionMakerUtils::IsArithmeticOrPointer(ccoo.GetCastType()) )
		return CVcmp(ccoo);
	else
		return 1;
}


// проверяет, является ли операнд склярным типом. Арифметическим или указателем
int OperatorCaster::COSScalar( const ClassCastOverloadOperator &ccoo )
{
	// если тип целый, выполняем дальнейшие проверки
	if( ExpressionMakerUtils::IsScalarType(ccoo.GetCastType()) )
		return CVcmp(ccoo);	
	else
		return 1;
}


// выявить оператор класса который оптимально подходит для преобразования
// в заданный тип или категорию
void OperatorCaster::ClassifyCast()
{
	const ClassType &cls = static_cast<const ClassType &>(clsType.GetBaseType());
	
	// если в классе нет операторов преобразования, можно сразу выходить
	if( cls.GetCastOperatorList() == NULL || cls.GetCastOperatorList()->empty() )
		return;

	// проходим по списку формируя список допустимых операторов, находя наилучший
	int (OperatorCaster::*cos)( const ClassCastOverloadOperator &ccoo );
	switch( category ) 
	{
	case ACC_NONE:			cos = COSCast;		break;
	case ACC_TO_ARITHMETIC: cos = COSArith;		break;
	case ACC_TO_INTEGER:	cos = COSIntegral;	break;
	case ACC_TO_POINTER:	cos = COSPointer;	break;
	case ACC_TO_SCALAR:		cos = COSScalar;    break;
	case ACC_TO_ARITHMETIC_OR_POINTER: cos = COSArithmeticOrPointer; break;
	default: INTERNAL( "'OperatorCaster::ClassifyCast' - неизвестная категория");
	}

	for( CastOperatorList::const_iterator p = cls.GetCastOperatorList()->begin();
		 p != cls.GetCastOperatorList()->end(); p++ )
	{		
		int r = (this->*cos)(**p);
		if( r == -1 )
		{
			errMsg = ("неоднозначность между '" + string((*p)->GetTypyziedEntityName().c_str()) +
				"' и '" + castOperator->GetTypyziedEntityName().c_str() + 
				"'; приведение невозможно").c_str();
			castOperator = NULL;
			break;
		}

		else if( r == 0 )
			castOperator = *p;
	}

	// если оператор задан, задаем, что преобразование возможно, 
	// и проверяем доступность оператора
	if( castOperator != NULL )	
		isConverted = true;	

	// иначе оператор не задан, но сообщение также может не задаваться.
	// Сообщение задается только в случае неоднозначности, или недоступности,
	// иначе вызывающая функция должна вывести, что оператор не перегружен
}

// выполнить физическое преобразование, изменив 
// содержимое дерева выражений. SrcOp - классовый тип, который,
// содержит оператор приведения, destOp - тип к которому приводим выражение.
// В srcOp будет приведенное к типу destOp выражение
void OperatorCaster::DoCast( const POperand &destOp, POperand &srcOp, const Position &ep )
{
	INTERNAL_IF( castOperator == NULL || !isConverted || 
		!ExpressionMakerUtils::IsClassType(srcOp->GetType()) );

	// проверяем оператор на доступность
	AccessControlChecker acc( 
		GetCurrentSymbolTable().IsLocalSymbolTable() ? 
		GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable(),
		static_cast<const ClassType&>(srcOp->GetType().GetBaseType()), *castOperator );
	if( !acc.IsAccessible() )
		theApp.Error( ep, (string("\'") + castOperator->GetTypyziedEntityName().c_str() + 
			"\' - оператор недоступен; приведение невозможно").c_str());

	// создаем сначала обращение к члену оператору
	POperand cop = new PrimaryOperand(false, *castOperator);
	POperand select = new BinaryExpression( '.', false, srcOp, cop, 
		new TypyziedEntity(*castOperator));
	
	// далее создаем вызов функции
	POperand call = ExpressionMakerUtils::MakeFunctionCall(select, 
		PExpressionList(new ExpressionList));

	// наконец преобразуем склярно
	if( !scalarCaster.IsNull() )
		scalarCaster->DoCast(destOp, call, ep);
	srcOp = call;
}


// метод возвращает false, если требуется прекратить цикл
// поиска подходящего конструктора. В случае если конструктор
// подходит для преобразования, изменить соотв. поля класса
bool ConstructorCaster::AnalyzeConstructor( const ConstructorMethod &srcCtor )
{
	// сначала анализируем количество параметров, конструктор должен
	// принимать один параметр, либо больше, но остальные должны быть по умолчанию
	int pcnt = srcCtor.GetFunctionPrototype().GetParametrList().GetFunctionParametrCount();

	// если не одного, значит проверяем есть ли '...'
	if( pcnt == 0 )
	{
		// если нет '...' или конструктор уже задан, значит выходим
		if( !srcCtor.GetFunctionPrototype().IsHaveEllipse() ||
			constructor != NULL )
			return true;

		// конструктор еще не задан, задаем. Преобразователь не задается,
		// об этом следует помнить, когда будет генерироваться приведение
		constructor = &srcCtor;
		return true;
	}

	// иначе если больше одного, проверяем, чтобы остальные были по умолчанию
	else if( pcnt > 1 )
	{
		const Parametr &prm = 
			*srcCtor.GetFunctionPrototype().GetParametrList().GetFunctionParametr(1);

		// проверяем чтобы второй параметр был по умолчанию, если не по умолчанию,
		// выходим
		if( !prm.IsHaveDefaultValue() )
			return true;
	}

	// имеем один параметр, получаем его тип и пытаемся привести
	const TypyziedEntity &ptype = 
		*srcCtor.GetFunctionPrototype().GetParametrList().GetFunctionParametr(0);

	// пытаемся преобразовать 'rvalue', к типу первого параметра конструктора.
	PScalarToScalarCaster stsc = new ScalarToScalarCaster(ptype, rvalue, true);
	stsc->ClassifyCast();

	// если преобразование невозможно - выходим
	if( !stsc->IsConverted() )
		return true;

	// если это первое удачное преобразование, сохраняем его
	// без дополнительных проверок
	if( constructor == NULL )
	{
		constructor = &srcCtor;
		scalarCaster = stsc;
		return true;
	}

	// иначе проверяем преобразование. Если его уровень ниже текущего,
	// значит сохраняем его, иначе если уровень выше выходим, иначе
	// уровни равны - имеем неоднозначность
	else
	{
		// если у текущего преобразования уровень меньше, значит задаем его
		// или предыдущий преобразователь был '...'
		if( scalarCaster.IsNull() ||
			stsc->GetCastCategory() < scalarCaster->GetCastCategory() )
		{		
			constructor = &srcCtor;
			scalarCaster = stsc;
			return true;
		}

		// иначе если у текущего преобразования уровень больше, значит ничего не делаем
		else if( stsc->GetCastCategory() > scalarCaster->GetCastCategory() )
			return true;
		
		// иначе уровни одинаковы, проверяем квалификацию, если квалификация
		// разная, выбираем наилучшую, иначе неоднозначность
		else
		{		
			// сначала проверим, если квалификаторы разные, то более квалифицированный
			// выбирается, в случае, если объект также имеет квалифицированный тип
			bool c1 = constructor->GetFunctionPrototype().IsConst(),
				 v1 = constructor->GetFunctionPrototype().IsVolatile(), 
				 c2 = ptype.IsConst(), v2 = ptype.IsVolatile();
				 
			if( c1 != c2 || v1 != v2 )					
			{
				int cv1 = (c1 == rvalue.IsConst()) + (v1 == rvalue.IsVolatile());
				int cv2 = (c2 == rvalue.IsConst()) + (v2 == rvalue.IsVolatile());

				// если имеющийся конструктор более квалифицирован чем,
				// текущий проверяемый выходим
				if( cv1 >= cv2 )
					return true;

				// иначе меняем конструктор
				else
				{
					constructor = &srcCtor;
					scalarCaster = stsc;
					return true;				
				}
			}

			// иначе неоднозначность
			errMsg = ("неоднозначность между '" + string(
				srcCtor.GetTypyziedEntityName().c_str()) +
				"' и '" + constructor->GetTypyziedEntityName().c_str() + 
				"'; приведение невозможно").c_str();
			constructor = NULL;
			return false;
		}
	}		
}


// классифицировать преобразование. Логически проверить возможность
// преобразования одного типа в другой, а также собрать необходимую
// информацию для физического преобразования
void ConstructorCaster::ClassifyCast()
{
	INTERNAL_IF( !lvalue.GetBaseType().IsClassType() );
	const ClassType &cls = static_cast<const ClassType &>(lvalue.GetBaseType());

	// в самом начале проверяем, если левый операнд, является
	// не константой ссылкой, тогда приведение невозможно
	if( lvalue.GetDerivedTypeList().IsReference() && !lvalue.IsConst() )
	{
		errMsg = (string("невозможно создать временный объект класса '") + 
			cls.GetQualifiedName().c_str() + "'; требуется константная ссылка").c_str();
		return;			
	}
	
	// далее проходим по всему списку конструкторов выбирая наилучший
	for( ConstructorList::const_iterator p = cls.GetConstructorList().begin();
		 p != cls.GetConstructorList().end(); p++ )
		if( !AnalyzeConstructor(**p) )
			break;

	// если задан конструктор, значит преобразование возможно
	if( constructor != NULL )
		isConverted = true;

	// иначе задаем сообщ. об ошибке, если оно еще не задано
	else if( errMsg.empty() )
		errMsg = string("конструктор '" + string(cls.GetQualifiedName().c_str()) + "( " +
			rvalue.GetTypyziedEntityName(false).c_str() + 
			" )' не объявлен; приведение невозможно").c_str();
}


// проверка доступности конструктора или деструктора
bool ConstructorCaster::CheckCtorAccess( const POperand &destOp, const Method &meth )
{
	const SymbolTable *st = &GetCurrentSymbolTable();
	if( st->IsLocalSymbolTable() )
		st = &GetScopeSystem().GetFunctionalSymbolTable();

	// проверяем конструктор на доступность
	AccessControlChecker acc( *st, 
		static_cast<const ClassType&>(destOp->GetType().GetBaseType()), meth );
	return acc.IsAccessible();
}


// выполнить физическое преобразование, изменив 
// содержимое дерева выражений. SrcOp - любой тип, который,
// передается конструктору, destOp - классовый тип к которому приводим выражение.
// В srcOp будет приведенное к типу destOp выражение
void ConstructorCaster::DoCast( const POperand &destOp, POperand &srcOp, const Position &ep )
{
	INTERNAL_IF( constructor == NULL || !isConverted || 
		!ExpressionMakerUtils::IsClassType(destOp->GetType()) );

	// проверяем конструктор на explicit
	if( constructor->IsExplicit() && !explicitCast )
		theApp.Error( ep, (string("\'") + constructor->GetTypyziedEntityName().c_str() + 
			"\' - конструктор объявлен как explicit; неявное приведение невозможно").c_str());

	// проверяем доступность конструктора
	if( !CheckCtorAccess(destOp, *constructor) )
		theApp.Error( ep,  
			"'%s' - конструктор недоступен; приведение невозможно",
			constructor->GetTypyziedEntityName().c_str());

	// проверяем, объявлен ли деструктор и доступен ли он
	const Method *dtor = 
		static_cast<const ClassType &>(constructor->GetSymbolTableEntry()).GetDestructor();
	if( dtor == NULL )
		theApp.Error( ep,  
			"'~%s()' - деструктор не объявлен; приведение с помощью конструктора невозможно",
			static_cast<const ClassType &>(constructor->GetSymbolTableEntry()).
			GetQualifiedName().c_str());

	else if( !CheckCtorAccess(destOp, *dtor) )
		theApp.Error( ep,  
			"'%s' - деструктор недоступен; приведение с помощью конструктора невозможно",
			dtor->GetTypyziedEntityName().c_str());

	// проверяем также, чтобы класс не был абстарктым. Создание объекта
	// абстрактного класса невозможно
	if( static_cast<const ClassType&>(constructor->GetSymbolTableEntry()).IsAbstract() )
		theApp.Error( ep,
			"создание объекта класса '%s' невозможно; класс является абстрактным",
			static_cast<const ClassType&>(constructor->GetSymbolTableEntry()).
			GetQualifiedName().c_str());

	// приводим srcOp, к необходимому типу, если требуется
	scalarCaster->DoCast(destOp, srcOp, ep);

	// создаем конструктор операнд
	POperand cop = new PrimaryOperand(false, *constructor);
	PExpressionList pl = new ExpressionList;
	pl->push_back(srcOp);

	// далее создаем вызов конструктора и записываем его на место srcOp
	srcOp = ExpressionMakerUtils::MakeFunctionCall(cop, pl);
}


// преобразовать один классовый тип к другому. Преобразование возможно тремя путями:
// ссылка на производный класс преобразуется в базовый,
// с помощью оператора приведения, с помощью конструктора, 
void ClassToClassCaster::ClassifyCast()
{
	INTERNAL_IF( !ExpressionMakerUtils::IsClassType(cls1) ||
		!ExpressionMakerUtils::IsClassType(cls2) );
	const ClassType &ct1 = static_cast<const ClassType &>(cls1.GetBaseType()),
			&ct2 = static_cast<const ClassType &>(cls2.GetBaseType());

	// проверяем, можно ли преобразовать правый операнд к левому с
	// помощью преобразования производного класса к базовому,
	// либо если полное соотв.
	PScalarToScalarCaster stsc = new ScalarToScalarCaster(cls1, cls2, true);
	stsc->ClassifyCast();

	// преобразование из производного класса в базовый возможно
	if( stsc->IsConverted() )
	{
		// проверяем, какую категорию задать
		category = ( &ct1 == &ct2 ) ? CC_EQUAL : CC_STANDARD;			
		isConverted = true;
		caster = stsc.Release();
		return;
	}

	// пытаемся выполнить преобразование с помощью конструктора класса 'cls1'
	PConstructorCaster cc = new ConstructorCaster(cls1, cls2, explicitCast);
	cc->ClassifyCast();
	
	// пытаемся выполнить преобразование с помощью оператора преобразования 'cls2'
	POperatorCaster oc = new OperatorCaster(cls1, cls2, true);
	oc->ClassifyCast();

	// если оба преобразования успешны, имеем неоднозначность
	if( cc->IsConverted() && oc->IsConverted() )
	{
		INTERNAL_IF( cc->GetConstructor() == NULL || oc->GetCastOperator() == NULL );
		errMsg = ("неоднозначность между '" + string(
				cc->GetConstructor()->GetTypyziedEntityName().c_str()) +
				"' и '" + oc->GetCastOperator()->GetTypyziedEntityName().c_str() + 
				"'; приведение невозможно").c_str();
		return;
	}

	// иначе если конструкторное успешно, сохраняем его
	else if( cc->IsConverted() )
	{		
		isConverted = true;
		conversionMethod = cc->GetConstructor();
		conversionClass = &static_cast<const ClassType &>(cls1.GetBaseType());
		caster = cc.Release();
		category = CC_USERDEFINED;
		return;
	}

	// иначе если операторное успешно, сохряняем его
	else if( oc->IsConverted() )
	{
		isConverted = true;
		conversionMethod = oc->GetCastOperator();
		conversionClass = &static_cast<const ClassType &>(cls2.GetBaseType());
		caster = oc.Release();
		category = CC_USERDEFINED;
		return;
	}

	// иначе ошибка
	else
	{
		if( stsc->IsDerivedToBase() && !errMsg.empty() )
			;
		else
			errMsg = 
				!oc->GetErrorMessage().empty() ? oc->GetErrorMessage() : cc->GetErrorMessage();
		INTERNAL_IF( errMsg.empty() );
	}
}


// выполнить физическое преобразование, из srcOp в destOp,
// при этом изменяется srcOp
void ClassToClassCaster::DoCast( const POperand &destOp, POperand &srcOp, const Position &ep )
{
	// conversionMethod может быть равен нулю, если было преобразование из
	// производного класса в базовый
	INTERNAL_IF( !isConverted || caster.IsNull() );

	// преобразование выполнит один из трех преобразователей
	caster->DoCast(destOp, srcOp, ep);
}


// вернуть true, если функция 'fn' может принимать 'pcnt' параметров
bool OverloadResolutor::CompareParametrCount( const Function &fn, int pcnt )
{
	const FunctionParametrList &pl = fn.GetFunctionPrototype().GetParametrList();

	// если параметров такое же количество, вернуть true
	if( pl.GetFunctionParametrCount() == pcnt )
		return true;

	// иначе если параметров меньше, проверяем есть ли '...'
	else if( pl.GetFunctionParametrCount() < pcnt )
		return fn.GetFunctionPrototype().IsHaveEllipse();

	// иначе если больше, проверяем чтобы 'pcnt+1-ый' параметр был 
	// по умолчанию
	else
		return pl.GetFunctionParametr(pcnt)->IsHaveDefaultValue();
}


// проверяет, является ли функция корректной (кандидатом) для 
// вызова, на основе имеющегося списка параметров.
bool OverloadResolutor::ViableFunction( const Function &fn )
{
	viableCasterList.clear();

	const FunctionParametrList &pl = fn.GetFunctionPrototype().GetParametrList();
	
	// получаем количество параметров, которые нам необходимо обработать
	int pcnt = pl.GetFunctionParametrCount();

	// если параметров больше, обрабатываем только первые N
	if( pcnt > apl.size() )
		pcnt = apl.size();

	// выполняем цикл для каждого параметра
	ExpressionList::const_iterator p = apl.begin();
	for( int i = 0; i<pcnt; i++, p++ )
	{
		PCaster caster = 
			AutoCastManager( *pl.GetFunctionParametr(i), (*p)->GetType(), true).RevealCaster();
		caster->ClassifyCast();

		// если преобразование невозможно, выходим из цикла и задаем
		// сообщение об ошибке если требуется
		if( !caster->IsConverted() )
		{
			if( candidate == NULL && errMsg.empty() )
				errMsg = (string("несоответствие типов '") + CharString(i+1).c_str() +
					"-го параметра'; '" + (*p)->GetType().GetTypyziedEntityName(false).c_str() +
						"' невозможно привести к '" + 
				pl.GetFunctionParametr(i)->GetTypyziedEntityName(false).c_str() + '\'').c_str();
			return false;
		}

		// иначе, вставляем преобразователь в список
		viableCasterList.push_back(caster);
	}

	return true;
}


// сравнивает 'fn' с текущим кандидатом и проверяет, какая 
// из функций наиболее подходит для списка параметров, если
// подходят обе, вызывает неоднозначность и возвращает false
bool OverloadResolutor::SetBestViableFunction( const Function &fn )
{
	// если кандидат еще не задан, задать его и выйти
	if( candidate == NULL )
	{
		candidate = &fn;
		candidateCasterList = viableCasterList;		// задаем список преобразователей
		return true;
	}

	// иначе сравниваем списки преобразователей.
	int bt = 0, bc = 0, i = 0, 		
		fnpcnt = fn.GetFunctionPrototype().GetParametrList().GetFunctionParametrCount(),
		cpcnt = candidate->GetFunctionPrototype().GetParametrList().GetFunctionParametrCount(),
		// рассматриваем, только те параметры, которые реально передаются
		mcnt = fnpcnt < cpcnt ? fnpcnt : cpcnt;		

	mcnt = apl.size() < mcnt ? apl.size() : mcnt;
	try {

	// проходим по списку параметров, выявляя неоднозначность
	for( CasterList::iterator pc = candidateCasterList.begin(), pfn = viableCasterList.begin();
		 i<mcnt; i++, pc++, pfn++ )
	{
		INTERNAL_IF( pc == candidateCasterList.end() || pfn == viableCasterList.end() );

		// получаем разность между категориями преобразования кандидата и 
		// текущей функции. Если категории равны - 0, если функция лучше кандидата - >0,
		// если функция хуже кандадата - <0
		bc = (*pc)->GetCastCategory() - (*pfn)->GetCastCategory();

		// теперь проверяем, чтобы не было неоднозначности. Если bt == 0, то bc 
		// можно задавать, если bt < 0 и bc < 0, функия по прежнему хуже кандидата,
		// если bt > 0 и bc > 0, то функция по прежнему лучше кандидата, иначе неоднознач
		if( bt == 0 )
			bt = bc;
		else if( (bt < 0 && bc <= 0) || (bt > 0 && bc >= 0) )
			;
		// иначе код разный и это неоднозначность
		else
			throw i;
	}

	// проверяем '...', если количество параметров разное. У кого есть
	// '...', тот и меньше
	if( fnpcnt != cpcnt )
	{
		if( i == fnpcnt && fn.GetFunctionPrototype().IsHaveEllipse() )
			bc = -1;
		else if( i == cpcnt && candidate->GetFunctionPrototype().IsHaveEllipse() )
			bc = 1;
		else
			bc = 0;

		// выполняем очередную проверку, вызвана ли неоднозначность
		if( bt == 0 || (bt < 0 && bc <= 0) || (bt > 0 && bc >= 0) )
			bt = bc;

		// иначе код разный и это неоднозначность
		else
			throw i;
	}

	// последнее, если bt, равен 0, значит неоднозначность
	if( bt == 0 )
		throw i;

	// а если bt > 0, значит кандидата следует заменить
	else if( bt > 0 )
	{
		candidate = &fn;
		candidateCasterList = viableCasterList;		// задаем список преобразователей	
	}

	// перехватываем исключительную ситуацию вызванную неоднозначностью
	} catch( int ) {
		
		errMsg = (string("неоднозначность между '") + fn.GetTypyziedEntityName().c_str() +
					"' и '" + candidate->GetTypyziedEntityName().c_str() + '\'').c_str();
		candidate = NULL;
		candidateCasterList.clear();
		ambigous = true;
		return false;
	}
	
	return true;
}


// выполнить приведение каждого параметра в списке к необходимому типу
void OverloadResolutor::DoParametrListCast( const Position &errPos )
{
	if( candidate == NULL )
		return;

	int i = 0;
	for( CasterList::iterator p = candidateCasterList.begin(); 
		 p != candidateCasterList.end(); p++, i++ )
	{
		POperand pop = new PrimaryOperand( true, *candidate->GetFunctionPrototype().
			GetParametrList().GetFunctionParametr(i) );	
		(*p)->DoCast( pop, const_cast<POperand &>(apl[i]), errPos );
	}
}


// метод выявляет единственную функцию, которая подходит под список параметров
void OverloadResolutor::PermitUnambigousFunction()
{
	// проверяем каждую функцию в списке
	for( OverloadFunctionList::const_iterator p = ofl.begin(); p != ofl.end(); p++ )
	{
		const Function &fn = *(*p);

		// сначала проверяем, может ли функция принимать 
		// заданное количество параметров
		if( !CompareParametrCount(fn, apl.size()) )
			continue;

		// далее если задан объект, проверяем, чтобы квалифицикация функции
		// была не меньше, чем квалификация объекта		
		if( object != NULL && fn.GetStorageSpecifier() != Function::SS_STATIC )
		{
			if( object->IsConst() > fn.GetFunctionPrototype().IsConst() ||
				object->IsVolatile() > fn.GetFunctionPrototype().IsVolatile() )
			{
				if( errMsg.empty() )
					errMsg = (string("метод '") + fn.GetTypyziedEntityName().c_str() +
						"' менее квалифицирован чем объект; вызов невозможен").c_str();
				continue;
			}
		}

		// наконец сопоставляем список параметров со списком формальных
		// параметров функции. Если список подходит - вернуть true.
		if( !ViableFunction( fn ) )
			continue;

		// имеем две функции, 'fn' и 'candidate'. Выявляем какая функция
		// более подходит, может быть неоднозначность, тогда возвращается false.
		// Если candidate еще нет, fn становится им без дополнительных проверок
		if( !SetBestViableFunction( fn ) )
			break;
	}

	// если кандидат не задан и сообщение не задано, задаем сообщение
	if( candidate == NULL && errMsg.empty() )
	{
		const Function &fn = *(*ofl.begin());

		// либо нет подходящей функции
		if( ofl.size() > 1 )
			errMsg = (string("\'") + fn.GetQualifiedName().c_str()  + 
				"' - нет подходящей функции, которая принимает '" + 
				CharString((int)apl.size()).c_str() + "' параметр(а)").c_str();

		// либо функция принимает не то кол-во параметров
		else
			errMsg = (string("\'") + fn.GetTypyziedEntityName().c_str()  + 
				"' - функция принимает '" + CharString(
				fn.GetFunctionPrototype().GetParametrList().GetFunctionParametrCount()).c_str() +
				"' параметр(а) вместо '" + CharString((int)apl.size()).c_str() + '\'').c_str();
	}
}


// закрытый метод, который вызывается конструктором для
// поиска операторов
void OverloadOperatorFounder::Found( int opCode, bool evrywhere, 
				const ClassType *cls, const Position &ep  )
{
	// следует учитывать, что операторы следует искать не только
	// в классе, но и в остальных областях видимости
	const CharString &opName = 
		(string("operator ") + ExpressionPrinter::GetOperatorName(opCode)).c_str();

	// теперь из позиции и имени формируем пакет
	Lexem lxm( opName, NAME, ep );
	NodePackage np( PC_QUALIFIED_NAME );
	np.AddChildPackage( new LexemPackage(lxm) );

	// теперь ищем сначала в классе, если он задан
	if( cls != NULL )
	{
		IdentifierOperandMaker iom( np, cls, true );
		POperand op = iom.Make();

		// если ошибка, но оператор найден, выйти т.к. неоднозначность
		if( op->IsErrorOperand() )
		{
			if( !iom.IsNotFound() )
			{
				ambigous = true;
				return;
			}
		}

		// иначе если один оператор, добавляем его в список
		else if( op->IsPrimaryOperand() )
			clsOperatorList.push_back( &static_cast<const Function &>(op->GetType()) );

		// иначе если несколько операторов, вставляем их
		else if( op->IsOverloadOperand() )		
			clsOperatorList = static_cast<OverloadOperand &>(*op).GetOverloadList();
		
		// иначе тип или выражение и это внутренняя ошибка
		else
			INTERNAL( "'OverloadOperatorFounder::Found' - неизвестный операнд" );
	}

	// теперь проделываем ту же операцию, если требуется искать за пределами классовой
	// области видимости
	if( evrywhere )
	{
		// временно сохраняем те области, которые не относятся к классовым
		list<SymbolTable *> stl;

		// сначала следует поднятся вверх за класс, если мы находимся внутри
		// метода или класса
		if( GetCurrentSymbolTable().IsClassSymbolTable()				  ||
			(GetCurrentSymbolTable().IsFunctionSymbolTable() &&
				static_cast<const FunctionSymbolTable &>(
				GetCurrentSymbolTable()).GetFunction().IsClassMember())   ||
			(GetCurrentSymbolTable().IsLocalSymbolTable() && 
			    static_cast<const FunctionSymbolTable &>(
				GetScopeSystem().GetFunctionalSymbolTable()).GetFunction().IsClassMember()) )
		
			// сохрянем все области видимости влючая классовую
			for( ;; )
			{
				if( !GetCurrentSymbolTable().IsClassSymbolTable() &&
					!GetCurrentSymbolTable().IsFunctionSymbolTable() &&
					!GetCurrentSymbolTable().IsLocalSymbolTable() )				
					break;				
				
				stl.push_front( &GetCurrentSymbolTable());
				GetScopeSystem().DestroySymbolTable();
			}				  
		

		IdentifierOperandMaker iom( np, NULL, true );
		POperand op = iom.Make();

		// создали операнд, восстанавливаем области видимости
		GetScopeSystem().PushSymbolTableList(stl);

		// если ошибка, но оператор найден, выйти т.к. неоднозначность
		if( op->IsErrorOperand() )
		{
			if( !iom.IsNotFound() )
			{			
				ambigous = true;
				return;
			}
		}

		// иначе если один оператор, добавляем его в список
		else if( op->IsPrimaryOperand() )
			globalOperatorList.push_back( &static_cast<const Function &>(op->GetType()) );

		// иначе если несколько операторов, вставляем их
		else if( op->IsOverloadOperand() )		
			globalOperatorList = static_cast<const OverloadOperand &>(*op).GetOverloadList();
		
		// иначе тип или выражение и это внутренняя ошибка
		else
			INTERNAL( "'OverloadOperatorFounder::Found' - неизвестный операнд" );
	}

	// в конце задаем флаг поиска
	found = !clsOperatorList.empty() || !globalOperatorList.empty();
}


// проверка, с возвратом того же аргмента в случае успеха, либо
// error operand в случае неудачи
const POperand &DefaultArgumentChecker::Check() const
{
	// если выражение изначально ошибочно, выходим
	if( defArg->IsErrorOperand() )
		return defArg;

	INTERNAL_IF( !(defArg->IsPrimaryOperand() || defArg->IsExpressionOperand()) );
	
	// в этом месте, сравниваем типы значения и параметра
	PCaster caster = AutoCastManager( parametr, defArg->GetType(), true ).RevealCaster();
	caster->ClassifyCast();

	// если преобразование невозможно, выводим ошибку и возвращаем error operand
	if( !caster->IsConverted() )
	{
		if( caster->GetErrorMessage().empty() )
		{
			theApp.Error(errPos,
				"'%s' - невозможно преобразовать '%s' к '%s' в значении по умолчанию",
				parametr.GetName().c_str(), 
				defArg->GetType().GetTypyziedEntityName(false).c_str(),
				parametr.GetTypyziedEntityName(false).c_str());
			return ErrorOperand::GetInstance();
		}
		else
		{
			theApp.Error(errPos,
				"'%s' - %s", parametr.GetName().c_str(), caster->GetErrorMessage().c_str());
			return ErrorOperand::GetInstance();
		}
	}

	// иначе выполняем преобразование физически
	caster->DoCast( new PrimaryOperand(true, parametr), const_cast<POperand&>(defArg), errPos);

	// если тип аргумента по умолчанию классовый и он не является явным
	// вызовом конструктора, проверим, чтобы к-тор копирования был доступен
	if( parametr.GetBaseType().IsClassType() && parametr.GetDerivedTypeList().IsEmpty() )
	{
		// проверяем, если вызов ф-ции
		if( defArg->IsExpressionOperand() &&
			static_cast<const Expression&>(*defArg).IsFunctionCall() )
		{
			const FunctionCallExpression &fnc = 
					static_cast<const FunctionCallExpression&>(*defArg);

			// здесь определили, что имеем вызов конструктора, проверку на к-тор 
			// копирования не требуется
			if( (&fnc.GetFunctionOperand()->GetType().GetBaseType() ==
				&static_cast<const ClassType&>(parametr.GetBaseType()) )	&&
				(dynamic_cast<const ConstructorMethod *>(
					&fnc.GetFunctionOperand()->GetType()) != NULL) )
				return defArg;
		}

		// в противном случае, проверяем, чтобы у объекта был доступен к-тор копирования
		// и деструктор
		ExpressionMakerUtils::ObjectCreationIsAccessible(
			static_cast<const ClassType&>(parametr.GetBaseType()), errPos, false, true, true);
	}

	return defArg;
}


// выполняет инициализацию по умолчанию для имеющегося типа,
// вызывается когда остались лишние элементы в агрегате
void AgregatController::DefaultInit( const TypyziedEntity &type, const Position &errPos )
{	
	::Object ob( "инициализация", 0, (BaseType *)&type.GetBaseType(), type.IsConst(),
		type.IsVolatile(), type.GetDerivedTypeList(), ::Object::SS_NONE );
	static PExpressionList el = new ExpressionList;

	// передаем пустой список выражений, это является показателем иниц. по умолчанию
	InitializationValidator( el, ob, errPos ).Validate();
}


// обработать список
AgregatController *ArrayAgregatController::DoList( const ListInitComponent &ilist )
{
	// если тип элемента не является агрегатом, сгенерировать исключение
	if( !ListInitializationValidator::IsAgregatType(*elementType) )
	{
		// если есть один элемент, это ничего не меняет, возвращаем
		// текущий контроллер
		if( ilist.GetICList().size() == 1 )
			return this;

		// если элементов нет, производим инициализацию по умолчанию
		else if( ilist.GetICList().size() == 0 )
		{
			DefaultInit(*elementType, ilist.GetPosition());
			initElementCount++;			
			return this;
		}

		// иначе тип не является агрегатом, ошибка
		else
			throw TypeNotArgegat(*elementType);
	}

	// иначе имеем агрегат, возвращаем новый контроллер для него. У
	// этого контроллера нет родителя
	initElementCount++;
	return ListInitializationValidator::MakeNewController(*elementType, NULL);
}


// обработать выражение, либо вернуть родтельский или новый контроллер,
// в случае если элементов не осталось или текущий элемент агрегат
AgregatController *ArrayAgregatController::DoAtom( const AtomInitComponent &iator, bool endList )
{
	// иначе если количество элементов не вмещается в массив, 
	// вернуть родительский контроллер
	if( initElementCount == arraySize )
	{	
		// если родителя нет, это ошибка
		if( parentController == NULL )
			throw NoElement(iator.GetPosition());

		return const_cast<AgregatController *>(parentController);
	}

	// если текущий элемент агрегат, создать для него свой контроллер,
	// вернуть его
	if( ListInitializationValidator::IsAgregatType(*elementType) &&
		!ListInitializationValidator::IsCharArrayInit(*elementType, iator.GetExpression()) )
	{
		initElementCount++;
		return ListInitializationValidator::MakeNewController(*elementType, this);
	}
	
	initElementCount++;

	// в противном случае, проверяем выражения
	static PExpressionList el = new ExpressionList;
	el->clear();
	el->push_back(iator.GetExpression());

	// проверяем
	const TypyziedEntity &etype = *elementType;
	::Object ob( "инициализация", NULL, (BaseType *)&etype.GetBaseType(), etype.IsConst(),
		etype.IsVolatile(), etype.GetDerivedTypeList(), ::Object::SS_NONE );
	InitializationValidator( el, ob, iator.GetPosition() ).Validate();

	// возвращаем NULL, как результат правильной работы
	return NULL;
}


// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
// для одного элемента, либо игнорировать если все элементы инициализированы
// либо размер массива неизвестен
void ArrayAgregatController::EndList( const Position &errPos )
{		
	// если размер массива неизвестен, задаем его
	if( pArray.IsUncknownSize() )
	{
		if( initElementCount < 1 )
			theApp.Error(errPos, "размер массива остался неизвестным после инициализации"),
			initElementCount = 1;
		pArray.SetArraySize(initElementCount);
	}

	// если все элементы инициализированы, проверять инициализацию
	// по умолчанию не требуется, также и при неизвестном размере
	if( initElementCount == arraySize || arraySize < 1 )	
		return; 	
	
	DefaultInit(*elementType, errPos);

	// устанавливаем, что все элементы проинициализированы, для следующих
	// сообщений
	initElementCount = arraySize;
}


// получить ук-ль на следующий инициализируемый не статический данный-член,
// если такого нет в структуре, возвращает NULL
const DataMember *StructureAgregatController::NextDataMember( )
{
	const ClassMemberList &cml = pClass.GetMemberList();
	for( int i = curDMindex+1; i<cml.GetClassMemberCount(); i++ )
	{
		if( const DataMember *dm = 
				dynamic_cast<const DataMember *>(&*cml.GetClassMember(i)) )
		{
			if( dm->GetStorageSpecifier() == ::Object::SS_STATIC ||
				dm->GetStorageSpecifier() == ::Object::SS_TYPEDEF )
				continue;
			curDMindex = i;
			return dm;
		}
	}

	return NULL;
}


// обработать список 
AgregatController *StructureAgregatController::DoList( const ListInitComponent &ilist )
{
	// получаем следующий элемент
	const DataMember *dm = NextDataMember();
	if( !dm )
		throw NoElement(ilist.GetPosition());

	// если тип элемента не является агрегатом, сгенерировать исключение
	if( !ListInitializationValidator::IsAgregatType(*dm) )
	{
		// если есть один элемент, это ничего не меняет, возвращаем
		// текущий контроллер
		if( ilist.GetICList().size() == 1 )
			return this;

		// если элементов нет, производим инициализацию по умолчанию
		else if( ilist.GetICList().size() == 0 )
		{
			DefaultInit(*dm, ilist.GetPosition());			
			return this;
		}

		// иначе тип не является агрегатом, ошибка
		else
			throw TypeNotArgegat(*dm);
	}

	// иначе имеем агрегат, возвращаем новый контроллер для него. У
	// этого контроллера нет родителя
	return ListInitializationValidator::MakeNewController(*dm, NULL);
}


// обработать выражение, либо вернуть родтельский или новый контроллер,
// в случае если элементов не осталось или текущий элемент агрегат
AgregatController *StructureAgregatController::DoAtom( 
					const AtomInitComponent &iator, bool endList  )
{
	// получаем следующий элемент
	const DataMember *dm = NextDataMember();
	if( !dm )
	{
		// элемента нет, либо возвращаем родителя, либо генерируем ошибку
		if( parentController == NULL )
			throw NoElement(iator.GetPosition());

		return const_cast<AgregatController *>(parentController);
	}

	// если текущий элемент агрегат, создать для него свой контроллер,
	// вернуть его
	if( ListInitializationValidator::IsAgregatType(*dm) &&
		!ListInitializationValidator::IsCharArrayInit(*dm, iator.GetExpression()) )	
		return ListInitializationValidator::MakeNewController(*dm, this);

	// в противном случае, проверяем выражения
	static PExpressionList el = new ExpressionList;
	el->clear();
	el->push_back(iator.GetExpression());

	// проверяем
	InitializationValidator( el, const_cast<DataMember&>(*dm), iator.GetPosition() ).Validate();

	// возвращаем NULL, как результат правильной работы
	return NULL;
}


// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
// для оставшихся членов
void StructureAgregatController::EndList( const Position &errPos )
{
	// пока поступают новые элементы, проверяем их на инициализацию по умолчанию
	while( const DataMember *dm = NextDataMember() )
		DefaultInit(*dm, errPos);
}


// получить первый данный член, если он еще не получен
const DataMember *UnionAgregatController::GetFirstDataMember()
{
	if( memberGot )
		return NULL;

	memberGot = true;
	const ClassMemberList &cml = pUnion.GetMemberList();
	for( int i = 0; i<cml.GetClassMemberCount(); i++ )
	{
		if( const DataMember *dm = 
				dynamic_cast<const DataMember *>(&*cml.GetClassMember(i)) )
		{
			if( dm->GetStorageSpecifier() == ::Object::SS_STATIC ||
				dm->GetStorageSpecifier() == ::Object::SS_TYPEDEF )
				continue;			
			return dm;
		}
	}

	return NULL;
}


// обработать список 
AgregatController *UnionAgregatController::DoList( const ListInitComponent &ilist )
{
	// получаем первый элемент, если он еще не получен
	const DataMember *dm = GetFirstDataMember();
	if( !dm )
		throw NoElement(ilist.GetPosition());

	// если тип элемента не является агрегатом, сгенерировать исключение
	if( !ListInitializationValidator::IsAgregatType(*dm) )
	{
		// если есть один элемент, это ничего не меняет, возвращаем
		// текущий контроллер
		if( ilist.GetICList().size() == 1 )
			return this;

		// если элементов нет, производим инициализацию по умолчанию
		else if( ilist.GetICList().size() == 0 )
		{
			DefaultInit(*dm, ilist.GetPosition());			
			return this;
		}

		// иначе тип не является агрегатом, ошибка
		else
			throw TypeNotArgegat(*dm);
	}

	// иначе имеем агрегат, возвращаем новый контроллер для него. У
	// этого контроллера нет родителя
	return ListInitializationValidator::MakeNewController(*dm, NULL);
}


// обработать выражение, либо вернуть родтельский или новый контроллер,
// в случае если элементов не осталось или текущий элемент агрегат
AgregatController *UnionAgregatController::DoAtom( const AtomInitComponent &iator, bool endList )
{
	// получаем следующий элемент
	const DataMember *dm = GetFirstDataMember();
	if( !dm )
	{
		// элемента нет, либо возвращаем родителя, либо генерируем ошибку
		if( parentController == NULL )
			throw NoElement(iator.GetPosition());

		return const_cast<AgregatController *>(parentController);
	}

	// если текущий элемент агрегат, создать для него свой контроллер,
	// вернуть его
	if( ListInitializationValidator::IsAgregatType(*dm) &&
		!ListInitializationValidator::IsCharArrayInit(*dm, iator.GetExpression()) )	
		return ListInitializationValidator::MakeNewController(*dm, this);

	// в противном случае, проверяем выражения
	static PExpressionList el = new ExpressionList;
	el->clear();
	el->push_back(iator.GetExpression());

	// проверяем
	InitializationValidator( el, const_cast<DataMember&>(*dm), iator.GetPosition() ).Validate();

	// возвращаем NULL, как результат правильной работы
	return NULL;
}


// обработать сообщение - конец списка, выполнить инициализацию по умолчанию
// для оставшихся членов
void UnionAgregatController::EndList( const Position &errPos )
{
	if( const DataMember *dm = GetFirstDataMember() )
		DefaultInit(*dm, errPos);
}


// вернуть true, если тип является агрегатом (массивом или структурой),
// иначе false. В случае если структура не является агргатом (не-POD),
// сгенерировать исключение типа ClassType
bool ListInitializationValidator::IsAgregatType( const TypyziedEntity &type )
{
	if( type.GetDerivedTypeList().IsArray() )
		return true;

	// проверяем, чтобы структура была агрегатом
	else if( type.GetDerivedTypeList().IsEmpty() &&
			 type.GetBaseType().IsClassType() )
	{
		const ClassType &cls = static_cast<const ClassType &>(type.GetBaseType());
		// у класса не должно быть базовых классов, виртуальных функций,
		// закрытых/защищенных нестатических данных-членов, определенных
		// пользователем конструткоров
		if( !cls.GetBaseClassList().IsEmpty() ||
			 cls.IsPolymorphic() )
			 return false;

		// проверяем, чтобы не статические данные-члены были открытые
		int i;
		for( i = 0; i<cls.GetMemberList().GetClassMemberCount(); i++ )
			if( const DataMember *dm = 
					dynamic_cast<const DataMember *>(&*cls.GetMemberList().GetClassMember(i)) )
				if( dm->GetAccessSpecifier()  != ClassMember::AS_PUBLIC &&
					dm->GetStorageSpecifier() != ::Object::SS_STATIC	&&
					dm->GetStorageSpecifier() != ::Object::SS_TYPEDEF )
					return false;

		// проверяем конструкторы
		for( ConstructorList::const_iterator p = cls.GetConstructorList().begin();
			 p != cls.GetConstructorList().end(); p++  )
			if( (*p)->IsUserDefined() )
				return false;

		// все проверки пройдены, вернуть true
		return true;
	}

	else
		return false;
}


// метод возвращает true, если тип является массивом типа char или wchar_t,
// а операнд является строковым литералом
bool ListInitializationValidator::IsCharArrayInit( 
			const TypyziedEntity &type, const POperand &iator )
{
	return type.GetDerivedTypeList().IsArray() &&
		type.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		(type.GetBaseType().GetBaseTypeCode() == BaseType::BT_CHAR ||
		 type.GetBaseType().GetBaseTypeCode() == BaseType::BT_WCHAR_T) &&
		iator->IsPrimaryOperand()		&&
		iator->GetType().IsLiteral()	&&
		(static_cast<const Literal&>(iator->GetType()).IsStringLiteral() ||
		 static_cast<const Literal&>(iator->GetType()).IsWideStringLiteral());
}


// создать новый контроллер агрегата,для входного типа. Тип
// должен быть также агрегатом
AgregatController *ListInitializationValidator::MakeNewController( 
			const TypyziedEntity &elemType, const AgregatController *prntCntrl )
{
	AgregatController *controller = NULL;

	// подразумевается, что elemType, уже проверен на агрегатность
	// создаем контроллер массива, если тип элемента массив
	if( elemType.GetDerivedTypeList().IsArray() )
	{
		PTypyziedEntity et = new TypyziedEntity(elemType);

		// преобразуем 'массив типа T' в 'T'
		const_cast<DerivedTypeList &>(et->GetDerivedTypeList()).PopHeadDerivedType();
		controller = new ArrayAgregatController(et, prntCntrl, const_cast<Array &>(
			static_cast<const Array &>(*elemType.GetDerivedTypeList().GetHeadDerivedType()) ) );
	}

	// иначе, тип должен быть структурой или объединением
	else
	{
		INTERNAL_IF( !elemType.GetBaseType().IsClassType() ||
			!elemType.GetDerivedTypeList().IsEmpty() );
		const ClassType &cls = static_cast<const ClassType &>(elemType.GetBaseType());
		if( cls.GetBaseTypeCode() == BaseType::BT_UNION )
			controller = new UnionAgregatController(
				static_cast<const UnionClassType &>(cls), prntCntrl);
		
		// иначе обычная структура или класс
		else
			controller = new StructureAgregatController(cls, prntCntrl);		
	}

	// поместить контроллер в список
	allocatedControllers.push_back(controller);
	return controller;
}


// рекурсивный метод, проходит по списку инициализации, в случае
// появления подсписка вызывает рекурсию для подсписка
void ListInitializationValidator::InspectInitList( const ListInitComponent &ilist )
{
	int cnt = 0;

	// проходим по списку
	for( ListInitComponent::ICList::const_iterator p = ilist.GetICList().begin();
		 p != ilist.GetICList().end(); ++p, cnt++ )
	{
		// если имеем атом, посылаем это сообщение контроллеру, пока
		// он не вернет NULL
		if( (*p)->IsAtom() )
		{
			const AtomInitComponent &aic = *static_cast<const AtomInitComponent *>(*p);
			bool isLastElement = cnt == ilist.GetICList().size()-1;
			AgregatController *temp = pCurrentController->DoAtom(aic, isLastElement);				
			int i = 0;

			// пока текущий контроллер возвращает не NULL, значит
			// он не может самостоятельно обработать инициализатор и создает
			// новый, либо возвращает родительский
			while( temp != NULL )
			{
				pCurrentController = temp,
				temp = pCurrentController->DoAtom(aic, isLastElement);
				INTERNAL_IF( i++ == MAX_ITERATION_COUNT	);
			}
		}

		// иначе список, посылаем сообщение текущему контроллеру,
		// вызываем рекурсию
		else
		{
			// сохраняем текущий контроллер, т.к. DoList, вернет либо
			// текущий, либо новый, но без предка, поэтому после прохождения
			// списка текущий контроллер следует восстановтиь
			AgregatController *pPrev = pCurrentController;
			const ListInitComponent &lic = *static_cast<const ListInitComponent *>(*p);

			// посылаем сообщение о появлении следующего списка, проходим
			// список рекурсивно
			pCurrentController = pCurrentController->DoList(lic);
			InspectInitList( lic );

			// здесь достигнут конец списка, посылаем об этом сообщение контроллеру.
			// Либо подымаемся к родительскому контроллеру, либо остаемся в этом,
			// если он корневой
			if( pCurrentController != pPrev )
			{
				pCurrentController->EndList(errPos);
				pCurrentController = pPrev;
			}
		}
	}
}


// проверить валидность инициализации списком
void ListInitializationValidator::Validate()
{
	try {
		// проверяем, если передаваемый объект не агрегат,
		if( !IsAgregatType(object) )
		{
			// в списке должно быть <= 1 элементов
			if( listInitComponent.GetICList().size() > 1 )
			{
				theApp.Error(errPos, "слишком много инициализаторов");
				return;
			}

			// иначе формируем список выражений
			ExpressionList el;
			if( listInitComponent.GetICList().size() == 1 )
			{
				const InitComponent *icomp = listInitComponent.GetICList().front();
				while( icomp->IsList() )
					icomp = static_cast<const ListInitComponent *>(icomp)->GetICList().front();
				el.push_back(static_cast<const AtomInitComponent *>(icomp)->GetExpression());
			}

			PExpressionList pel = &el;
			InitializationValidator(pel, object, errPos).Validate();
			// освобождаем указатель во избежание ошибки
			pel.Release();

			// выходим
			return;
		}

		// иначе имеем агрегат, следует проверить инициализацию списком
		// создаем контроллер для агрегата
		pCurrentController = MakeNewController(object, NULL);
		INTERNAL_IF( pCurrentController == NULL );

		// проходим по списку
		AgregatController *prev = pCurrentController;
		InspectInitList(listInitComponent);
		prev->EndList(errPos);

	// перехватываем сообщение, которое возникает в случае если 
	// количество инициализаторов не вмещается в агрегат
	} catch( const AgregatController::NoElement &nl ) {
		theApp.Error(nl.errPos, "слишком много инициализаторов в списке");

	// перехватываем событие - тип не является агрегатом
	} catch( const AgregatController::TypeNotArgegat &t ) {
		theApp.Error(errPos, 
			"'%s' - тип не является агрегатом; инициализация списком невозможна",
			t.type.GetTypyziedEntityName(false).c_str());
	}


}


// проверяет инициализацию конструктором или одним значением,
// или без значения
void InitializationValidator::ValidateCtorInit()
{
	// предусловия
	INTERNAL_IF( &expList == NULL );
	const DerivedTypeList &dtl = object.GetDerivedTypeList();

	// если список инициализации пуст, выполняем инициализацию по умолчанию,	
	if( expList->empty() )
	{		
		// если тип или функция, или extern выходим 
		if( object.GetStorageSpecifier() == ::Object::SS_TYPEDEF ||
			object.GetStorageSpecifier() == ::Object::SS_EXTERN  ||
			dtl.IsFunction() )
			return;

		// если имеем ссылку инициализация обязательна
		if( dtl.IsReference()  )
			theApp.Error(errPos,
				"'%s' - инициализация ссылки обязательна",
				object.GetName().c_str());
		
		// если имеем константный объект
		else if( ExpressionMakerUtils::IsConstant(object) )
		{
			// если имеем класс, тогда он инициализируется конструктором,
			// иначе ошибка
			if( !(object.GetBaseType().IsClassType() && dtl.IsEmpty()) )
				theApp.Error(errPos,
					"'%s' - инициализация константного объекта обязательна",
					object.GetName().c_str());
		}

		// проверяем, чтобы размер массива был задан
		if( dtl.IsArray() && 
			static_cast<const Array &>(*dtl.GetHeadDerivedType()).IsUncknownSize() )
		{
			theApp.Error(errPos,
				"'%s' - массив неизвестного размера",
				object.GetName().c_str());

			// задаем размер
			const_cast<Array&>
				(static_cast<const Array &>(*dtl.GetHeadDerivedType())).SetArraySize(1);
		}

		// проверяем инициализацию
		ictor = ExpressionMakerUtils::CorrectObjectInitialization(
			object, expList, true, errPos).GetCtor();
	}

	// иначе инициализация списком 
	else
	{		
		// если имеем тип или функцию, инициализация невозможна.
		if( object.GetStorageSpecifier() == ::Object::SS_TYPEDEF ||
			dtl.IsFunction() )
		{
			theApp.Error(errPos,
				"инициализация %s невозможна", dtl.IsFunction() ? 
				"функции" : "типа");
			return;
		}

		// если массив типа char или wchar_t, а справа инициализатор
		// литерал, то проверяем инициализацию
		if( dtl.IsArray() && dtl.GetDerivedTypeCount() == 1 &&
			expList->size() == 1 && 
			expList->at(0)->IsPrimaryOperand() && expList->at(0)->GetType().IsLiteral() )
		{
			const Literal &lit = static_cast<const Literal &>(expList->at(0)->GetType());
			int lsz = -1;
			Array &obar = const_cast<Array&>
				( static_cast<const Array &>(*dtl.GetHeadDerivedType()) );

			// проверим, чтобы литерал имел тип char [], и массив был типа
			// char, либо литерал wchar_t и массив такого же типа
			if( (object.GetBaseType().GetBaseTypeCode() == BaseType::BT_CHAR &&
				 lit.IsStringLiteral())  || 
				(object.GetBaseType().GetBaseTypeCode() == BaseType::BT_WCHAR_T &&
				 lit.IsWideStringLiteral()) )
			{			
				lsz = lit.GetDerivedTypeList().GetHeadDerivedType()->GetDerivedTypeSize();
				// если размер массива неизвестен, задаем его
				if( obar.IsUncknownSize() )
					obar.SetArraySize(lsz);

				// иначе, если известен, он должен быть не меньше размера строки
				else if( lsz > obar.GetArraySize() )
					theApp.Error(errPos, 
						"'%s' - массив не может вместить всю строку; "
						"требуется как минимум '%d' байт(а)",
						object.GetName().c_str(), lsz);

				// прекращаем проверку
				return;
			}
			
			// иначе обычная проверка продолжается			
		}

		ExpressionMakerUtils::InitAnswer answer =
			ExpressionMakerUtils::CorrectObjectInitialization( object, expList, true, errPos);
		ictor = answer.GetCtor();

		// если инициализация корректна, и мы имеем константный инициализатор,
		// и мы имеем константый объект арифметического типа, выполним
		// задание инициализатора		
		if( answer )
		{
			double ival;
			if( expList->size() == 1										   && 
				ExpressionMakerUtils::IsInterpretable( expList->at(0), ival )  &&
				object.IsConst()									       &&
				ExpressionMakerUtils::IsArithmetic(object)				   &&
				dtl.IsEmpty() )
				object.SetObjectInitializer(ival);
		}		
	}
}


// пройти вверх по иерархии классов, задав виртуальные классы
void CtorInitListValidator::SelectVirtualBaseClasses( 
				const BaseClassList &bcl, unsigned &orderNum )
{
	static PExpressionList emptyList = NULL;
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &bcc = *bcl.GetBaseClassCharacteristic(i);
		SelectVirtualBaseClasses(bcc.GetPointerToClass().GetBaseClassList(), orderNum);

		// проверяем, если класс виртуально наследуется, задаем его в списке
		if( bcc.IsVirtualDerivation() )
		{
			const ClassType &cls = bcc.GetPointerToClass();

			// проверяем, чтобы класса не было в списке, т.к. виртуальный
			// класс следует инициализировать однажды
			if( find( oieList.begin(), oieList.end(), ObjectInitElement(cls, 
					emptyList, ObjectInitElement::IV_VIRTUALBC, 0)) == oieList.end() )
			{
				oieList.push_back( ObjectInitElement(cls, emptyList, 
					ObjectInitElement::IV_VIRTUALBC, orderNum) );
				orderNum++;
			}
		}
	}
}


// заполнить список зависимыми элементами инициализации,
// виртуальными базовыми классами, прямыми базовыми классами, 
// нестатическими данными-членами
void CtorInitListValidator::FillOIEList( )
{
	unsigned orderNum = 1, i;

	// сначала проходим по иерархии базовых классов, выбирая
	// виртуальные базовые классы. Следует учесть
	const BaseClassList &bcl = pClass.GetBaseClassList();
	SelectVirtualBaseClasses(bcl, orderNum);

	// далее загружаем прямые базовые классы
	for( i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &bcc = *bcl.GetBaseClassCharacteristic(i);
		
		// не загружаем виртуальные базовые классы, т.к. они уже загружены
		if( bcc.IsVirtualDerivation() )
			continue;
		oieList.push_back( ObjectInitElement(bcc.GetPointerToClass(), NULL, 
			ObjectInitElement::IV_DIRECTBC, orderNum++) );		
	}

	// в последнюю очередь, загружаем нестатические данные члены
	const ClassMemberList &cml = pClass.GetMemberList();
	for( i = 0; i<cml.GetClassMemberCount(); i++ )
	{
		if( const DataMember *dm = dynamic_cast<const DataMember *>(&*cml.GetClassMember(i)) )
		{
			if( dm->GetStorageSpecifier() != ::Object::SS_STATIC &&
				dm->GetStorageSpecifier() != ::Object::SS_TYPEDEF )
				oieList.push_back( ObjectInitElement(*dm, NULL, 
					ObjectInitElement::IV_DATAMEMBER, orderNum++) );
		}
	}
}


// добавить явно инициализируемый элемент, если он не присутствует в списке,
// вывести ошибку. Также выводит ошибку если присутствует два таких элемента
// в списке, что означает неоднозначность. 
void CtorInitListValidator::AddInitElement( const POperand &id, 
			const PExpressionList &expList, const Position &errPos, unsigned &orderNum )
{
	INTERNAL_IF( expList.IsNull() );

	// проверяем, чтобы идентификатор был основным или типом
	if( id->IsErrorOperand() )
		return;

	else if( id->IsOverloadOperand() )
	{
		theApp.Error(errPos, 
			"'%s' - не является членом класса '%s'",
			static_cast<const OverloadOperand&>(*id).GetOverloadList().front()->
			GetQualifiedName().c_str(), pClass.GetQualifiedName().c_str());
		return;
	}

	// основной операнд должен быть объектом
	else if( id->IsPrimaryOperand() )
	{
		const Identifier *obj = dynamic_cast<const Identifier *>(&id->GetType());
		INTERNAL_IF( !obj );
		ObjectInitElementList::iterator p = find( 
			oieList.begin(), oieList.end(), ObjectInitElement(*obj, 
			NULL, ObjectInitElement::IV_DATAMEMBER, 0));
		
		// элемент в списке не найден
		if( p == oieList.end() )
		{
			theApp.Error(errPos, 
				"'%s' - не является нестатическим данным-членом класса '%s'",
				obj->GetQualifiedName().c_str(), pClass.GetQualifiedName().c_str());
			return;
		}

		// иначе имеем объект, проверяем, чтобы он не был проинициализирован
		INTERNAL_IF( !(*p).IsDataMember() );
		if( (*p).IsInitialized() )
		{
			theApp.Error(errPos, 
				"'%s' - уже проинициализирован", obj->GetQualifiedName().c_str());
			return;
		}

		// проверяем корректность инициализации
		DataMember &dm = const_cast<DataMember &>(
			static_cast<const DataMember &>(id->GetType()) );

		// для массивов список инициализаторов должен быть пустой,
		// запрещаем инициализацию строковыми литералами
		if( dm.GetDerivedTypeList().IsArray() && !expList->empty() )
			theApp.Error(errPos, 
				"'%s' - список инициализаторов для массива должен быть пустой",
				obj->GetQualifiedName().c_str());
		
		// иначе выполняем простую проверку соответствия типов
		else
			InitializationValidator(expList, dm, errPos).Validate();

		// задаем инициализаторы и порядковый номер
		(*p).SetExpressionList( expList );
		(*p).SetOrderNum( orderNum );
		explicitInitCounter = orderNum++;
	}

	// типовой операнд должен быть либо классом, либо синонимом имени класса
	else if( id->IsTypeOperand() )
	{
		const TypyziedEntity &type = id->GetType();
		if( !(type.GetBaseType().IsClassType() && type.GetDerivedTypeList().IsEmpty()) )
		{
			theApp.Error(errPos, 
				"'%s' - тип не может использоваться в списке инициализации класса '%s'",
				type.GetTypyziedEntityName(false).c_str(), pClass.GetQualifiedName().c_str());
			return;
		}

		const ClassType &cls = static_cast<const ClassType &>(type.GetBaseType());				
		ObjectInitElement oie(cls, NULL, ObjectInitElement::IV_DIRECTBC, 0);
		ObjectInitElementList::iterator p = find( oieList.begin(), oieList.end(), oie);
		
		// элемент в списке не найден
		if( p == oieList.end() )
		{
			theApp.Error(errPos, 
				"'%s' - не является прямым или виртуальным базовым классом класса '%s'",
				cls.GetQualifiedName().c_str(), pClass.GetQualifiedName().c_str());
			return;
		}

		// проходим по списку еще раз с текущей позиции для того, чтобы проверить класс
		// на однозначность
		if( find( ++ObjectInitElementList::iterator(p), oieList.end(), oie ) != oieList.end() )
		{
			theApp.Error(errPos, 
				"'%s' - прямой и виртуальный базовый класс одновременно; "
				"инициализация невозможна из-за неоднозначности",
				cls.GetQualifiedName().c_str());
			return;
		}			

		// иначе имеем класс, проверяем, чтобы он не был проинициализирован
		INTERNAL_IF( (*p).IsDataMember() );
		if( (*p).IsInitialized() )
		{
			theApp.Error(errPos, 
				"'%s' - уже проинициализирован", cls.GetQualifiedName().c_str());
			return;
		}

		// проверка инициализации
		ExpressionMakerUtils::CorrectObjectInitialization( 
			TypyziedEntity( const_cast<ClassType *>(&cls), false, false, DerivedTypeList()),
			expList, false, errPos );
		
		(*p).SetExpressionList( expList );
		(*p).SetOrderNum( orderNum );
		explicitInitCounter = orderNum++;
	}

	// выражение не допускается
	else
		INTERNAL( "'CtorInitListValidator::AddInitElement' - неизвестный операнд");
}


// выполнить заключительную проверку, после того как весь список считан.
// Проверить неинициализированные элементы. Задать правильный порядок инициализации
void CtorInitListValidator::Validate()
{
	unsigned nonum = explicitInitCounter+1;
	static PExpressionList emptyList = new ExpressionList;

	// проходим по списку, если остались неициализированные элементы,
	// проверить на инициализацию по умолчанию, задать номер инициализации
	for( ObjectInitElementList::iterator p = oieList.begin(); p != oieList.end(); p++ )
	{
		if( (*p).IsInitialized() )
			continue;

		// если это член, выполняем инициализацию по умолчанию
		if( (*p).IsDataMember() )
			InitializationValidator(emptyList, const_cast<DataMember &>(
				static_cast<const DataMember &>((*p).GetIdentifier()) ), errPos).Validate();

		// иначе это класс
		else
			ExpressionMakerUtils::CorrectObjectInitialization( 
				TypyziedEntity( &const_cast<ClassType &>(
					static_cast<const ClassType &>((*p).GetIdentifier()) ),
					false, false, DerivedTypeList() ),
				emptyList, false, errPos );
	
		(*p).SetExpressionList(emptyList);
		(*p).SetOrderNum(nonum++);
	}
}


// инициализировать объект выражением 
void GlobalObjectMaker::Initialize( const ExpressionList &initList )
{
	INTERNAL_IF( targetObject == NULL );
	// пустой список, передается чекеру, если initList == NULL
	static PExpressionList emptyList = new ExpressionList;

	// если объект переопределен, не выполняем проверку инициализации
	if( redeclared )
		return;

	// создаем список выражений
	PExpressionList pl = const_cast<ExpressionList *>(&initList);
	if( pl.IsNull() )
		pl = emptyList;

	// проверяем выражение
	InitializationValidator validator(pl, *targetObject, tempObjectContainer->errPos);
	validator.Validate();

	// сохраняем конструктор, которым инициализировали объект. В случае
	// если инициализировался классовый объект
	ictor = validator.GetConstructor();		

	// освобождаем указатель
	if( &*pl != &*emptyList )
		pl.Release();
}


// инициализировать объект при определении выражением 
void MemberDefinationMaker::Initialize( const ExpressionList &initList )
{
	INTERNAL_IF( targetID == NULL );
	// проверяем, чтобы targetID, был статическим данным членом
	::Object *dm = dynamic_cast<::Object *>(targetID);
	if( !dm ||
		(dm->GetSymbolTableEntry().IsClassSymbolTable() &&
		 dm->GetStorageSpecifier() != ::Object::SS_STATIC) )
	{
		if( &initList != NULL )
			theApp.Error(toc->errPos, 
				"инициализация не статического члена класса невозможна");	
		return;
	}

	// если у объекта уже есть инициализатор
	if( dm->IsHaveInitialValue() )
	{
		theApp.Error(toc->errPos, 
			"'%s' - уже инициализирован", dm->GetQualifiedName().c_str());
		return;
	}

	// создаем список выражений
	static PExpressionList emptyList = new ExpressionList;
	PExpressionList pl = const_cast<ExpressionList *>(&initList);
	if( pl.IsNull() )
		pl = emptyList;

	// проверяем выражение
	InitializationValidator iv(pl, *dm, toc->errPos);
	iv.Validate();
	ictor = iv.GetConstructor();

	// задаем, что объект инициализирован
	if( !dm->IsHaveInitialValue() )
		dm->SetObjectInitializer(0);

	// освобождаем указатель
	if( &*pl != &*emptyList )
		pl.Release();
}


// метод проверяет корректность создания битового поля
void DataMemberMaker::CheckBitField( const Operand &exp )
{	
	// проверяем, чтобы битовое поле было целое, не статическое,
	// не тип. Проверим, чтобы выражение было целым, интерпретируемым > 0,
	if( !ExpressionMakerUtils::IsIntegral(*targetDM) ||
		!targetDM->GetDerivedTypeList().IsEmpty() )	
		theApp.Error(toc->errPos,
			"'%s' - битовое поле должно иметь целый тип",
			targetDM->GetName().c_str());
	
	// далее проверяем, чтобы корректный спецификатор хранения
	if( targetDM->GetStorageSpecifier() != ::Object::SS_NONE )
		theApp.Error(toc->errPos,
			"'%s' - битовое поле не может иметь спецификатор хранения '%s'",
			targetDM->GetName().c_str(), 
			ManagerUtils::GetObjectStorageSpecifierName(targetDM->GetStorageSpecifier()).c_str());

	// далее проверяем, чтобы выражение было целым интерпретируемым положительным
	double ival = 1;
	POperand pexp = const_cast<Operand *>(&exp);
	if( !ExpressionMakerUtils::IsInterpretable( pexp, ival ) ||
		!ExpressionMakerUtils::IsIntegral(exp.GetType())	 ) 
		theApp.Error(toc->errPos, 
			"размер битового поля должен задаваться целым константым выражением");
	if( ival <= 0 )
	{
		theApp.Error(toc->errPos, "размер битового поля должен быть больше нуля");
		ival = 1;
	}

	pexp.Release();

	// в последнюю очередь задаем размер битового поля
	targetDM->SetObjectInitializer(ival, true);
}


// метод проверяет корректность инициализации данного-члена значением
void DataMemberMaker::CheckDataInit( const Operand &exp )
{
	// проверяем, чтобы член был целым статическим константым
	if( !ExpressionMakerUtils::IsIntegral(*targetDM) ||
		!targetDM->GetDerivedTypeList().IsEmpty()	 ||
		targetDM->GetStorageSpecifier() != ::Object::SS_STATIC ||
		!targetDM->IsConst() )
	{
		theApp.Error(toc->errPos,
			"'%s' - инициализация члена невозможна внутри класса",
			targetDM->GetName().c_str());
		return;
	}
	
	// далее проверяем, чтобы выражение было целым интерпретируемым 
	double ival = 1;
	POperand pexp = const_cast<Operand *>(&exp);
	if( !ExpressionMakerUtils::IsInterpretable( pexp, ival ) ||
		!ExpressionMakerUtils::IsIntegral(exp.GetType())	 )	
		theApp.Error(toc->errPos, 
			"инициализатор должен быть целым константым выражением");				

	pexp.Release();

	// в последнюю очередь задаем инициализатор
	targetDM->SetObjectInitializer(ival);
}


// абстрактный деструктор
InitComponent::~InitComponent()
{
}
