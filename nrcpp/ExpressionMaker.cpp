// реализация интрефейса постройки выражения - ExpressionMaker.cpp

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
#include "Body.h"
#include "Checker.h"
#include "ExpressionMaker.h"


// статический член, который представляет собой 0
// используется для передачи постфиксному оператору 
POperand UnaryOverloadOperatorCaller::null = new PrimaryOperand(false,
		*new Literal((BaseType *)&ImplicitTypeManager(KWINT).GetImplicitType(), true, false,
					DerivedTypeList(), 0) );


// строитель простого типа, используется в выражениях вида 'int ()',
// в приведении типа вызовом функции	
POperand ExpressionMakerUtils::MakeSimpleTypeOperand( int tokcode )
{
	// лексема должна быть типом
	INTERNAL_IF( !IS_SIMPLE_TYPE_SPEC(tokcode) );

	TypyziedEntity *te = new TypyziedEntity(
		&const_cast<BaseType &>(ImplicitTypeManager(tokcode).GetImplicitType()), 
		false, false, DerivedTypeList());

	// создаем типовой операнд
	return new TypeOperand( *te );
}


// создать типовой операнд в выражении, исп. в приведении типа,
// typeid, new. Динамическое определение типов невозможно
POperand ExpressionMakerUtils::MakeTypeOperand( const NodePackage &np )
{
	INTERNAL_IF( np.GetPackageID() != PC_DECLARATION || 
		np.GetChildPackageCount() != 2 || np.IsErrorChildPackages() || 
		np.GetChildPackage(0)->GetPackageID() != PC_TYPE_SPECIFIER_LIST ||
		np.GetChildPackage(1)->GetPackageID() != PC_DECLARATOR );	

	// создаем временную структуру
	Position ep = ParserUtils::GetPackagePosition(&np);
	TempObjectContainer toc( ep, "<тип>");

	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( ((NodePackage *)np.GetChildPackage(0)), &toc, false );

	// проверим, если базовый тип не задан, можно возвращать errorOperand
	if( toc.baseType == NULL )
	{
		theApp.Error(ep, "не задан базовый тип");
		return ErrorOperand::GetInstance();
	}

	// далее, если есть декларатор, анализируем и его
	MakerUtils::AnalyzeDeclaratorPkg( ((NodePackage *)np.GetChildPackage(1)), &toc );
	
	// уточняем базовый тип
	MakerUtils::SpecifyBaseType( &toc );

	// теперь проверяем, чтобы в структуре не было лишних квалификаторов и 
	// модификаторов
	if( toc.ssCode != -1 ||	toc.fnSpecCode != -1 || toc.friendSpec )
		theApp.Error(ep, "'%s' - некорректен в данном контексте", 
			toc.friendSpec ? "friend" : 
			GetKeywordName(toc.ssCode != -1 ? toc.ssCode : toc.fnSpecCode));

	// проверяем сформированный тип	
	if( !CheckerUtils::CheckDerivedTypeList(toc)				||
		!CheckerUtils::CheckRelationOfBaseTypeToDerived(toc, true, true) )
		return ErrorOperand::GetInstance();

	// создаем типизированную сущность на основе структуры и возвращаем операнд тип
	return  new TypeOperand(
		*new TypyziedEntity(toc.finalType, toc.constQual, toc.volatileQual, toc.dtl));
}


// проверка, является ли тип целым
bool ExpressionMakerUtils::IsIntegral( const TypyziedEntity &op )
{
	BaseType::BT bt = op.GetBaseType().GetBaseTypeCode();
	if( !(op.GetDerivedTypeList().IsEmpty() ||
		  (op.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		   op.GetDerivedTypeList().IsReference()) ) )
		return false;

	return bt == BaseType::BT_INT || bt == BaseType::BT_CHAR ||
		bt == BaseType::BT_BOOL || bt == BaseType::BT_ENUM || bt == BaseType::BT_WCHAR_T;
}


// проверка, является ли тип арифметическим
bool ExpressionMakerUtils::IsArithmetic( const TypyziedEntity &op )
{
	BaseType::BT bt = op.GetBaseType().GetBaseTypeCode();
	if( !(op.GetDerivedTypeList().IsEmpty() ||
		  (op.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		   op.GetDerivedTypeList().IsReference()) ) )
		return false;

	// любой из целых типов, либо перечислымый, либо float или double
	return bt == BaseType::BT_INT || bt == BaseType::BT_CHAR || bt == BaseType::BT_FLOAT ||
		bt == BaseType::BT_DOUBLE ||
		bt == BaseType::BT_BOOL || bt == BaseType::BT_ENUM || bt == BaseType::BT_WCHAR_T;
}


// проверяка, если тип является указателем rvalue
bool ExpressionMakerUtils::IsRvaluePointer( const TypyziedEntity &op )
{
	// либо указатель, либо массив, либо ссылка на них
	const DerivedTypeList &dtl = op.GetDerivedTypeList();
	if( dtl.IsPointer() || dtl.IsArray() )
		return true;

	else if( dtl.GetDerivedTypeCount() > 1 && dtl.IsReference() &&
		(dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_POINTER ||
		 dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_ARRAY) )
		return true;

	else
		return false;
}


// вернуть true, если тип является функцией, ссылкой на функцию, указателем
// на функцию или указателем на член-функцию
bool ExpressionMakerUtils::IsFunctionType( const TypyziedEntity &type )
{
	const DerivedTypeList &dtl = type.GetDerivedTypeList();
	if( dtl.IsFunction() || 
		(dtl.GetDerivedTypeCount() > 1 &&
		 dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) )
		return true;
	return false;
}


// проверка, является ли операнд интерпретируемым. В rval записывается значение операнда
bool ExpressionMakerUtils::IsInterpretable( const POperand &op, double &rval )
{
	// основной операнд
	if( !op->IsPrimaryOperand() )
		return false;

	const TypyziedEntity &te = static_cast<const PrimaryOperand &>(*op).GetType();
	if( !IsArithmetic(te) )
		return false;

	// теперь операнд должен быть литералом или константой перечисления,
	// или константным объектом, который инициализирован константным значением
	if( te.IsLiteral() )
	{
		rval = atof( static_cast<const Literal &>(te).GetLiteralValue().c_str() );
		return true;
	}

	else if( te.IsEnumConstant() )
	{
		rval = static_cast<const EnumConstant &>(te).GetConstantValue();
		return true;
	}

	// если объект, следует проверить каким значением он инициализирован
	else if( te.IsObject() && te.IsConst() )
	{
		const double *obi = static_cast<const ::Object &>(te).GetObjectInitializer();
		if( obi == NULL )
			return false;
		rval = *obi;
		return true;
	}

	// иначе вернуть false; операнд не является интерпретируемым
	return false;
}


// проверка, является ли операнд lvalue
bool ExpressionMakerUtils::IsLValue( const POperand &op )
{
	if( op->IsPrimaryOperand() )
		return static_cast<const PrimaryOperand &>(*op).IsLvalue();

	else if( op->IsExpressionOperand() )
		return static_cast<const Expression &>(*op).IsLvalue();	

	else
		return false;
}


// проверяет, является ли операнд, модифицируемым lvalue, 
// если нет выводит ошибку и возвращает false
bool ExpressionMakerUtils::IsModifiableLValue( 
				const POperand &op, const Position &errPos, PCSTR opName )
{
	try {
		// сначала проверяем, является ли операнд lvalue вообщ.
		if( !IsLValue(op) )
			throw op;
		
		// теперь проверяем сам тип		
		const TypyziedEntity &type = op->GetType();		

		// тип должен быть, либо указателем, либо арифметическим типом
		if( IsArithmetic(type) )
		{
			if( type.IsConst() )
				throw type;
		}
		
		// иначе если только ссылка или вообще нет типа, проверяем константность
		else if( type.GetDerivedTypeList().IsEmpty()			 ||
			(type.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
			 type.GetDerivedTypeList().IsReference()) )
		{
			if( type.IsConst() )
				throw type;
		}

		// иначе тип должен быть указателем
		else if( IsRvaluePointer(type) )
		{
			bool ref = type.GetDerivedTypeList().IsReference();
			int dtcnt = type.GetDerivedTypeList().GetDerivedTypeCount();

			// если не указатель, выйти
			if( (type.GetDerivedTypeList().GetDerivedType(ref ? 1 : 0))->
				GetDerivedTypeCode() != DerivedType::DT_POINTER )
				throw type;

			// иначе проверяем константность указателя
			if( static_cast<const Pointer&>(
				*type.GetDerivedTypeList().GetDerivedType(ref ? 1 : 0)).IsConst() )
				throw type;
		}

		// если тип указатель на член
		else if( type.GetDerivedTypeList().IsPointerToMember() ||
			(type.GetDerivedTypeList().IsReference()				&&
			 type.GetDerivedTypeList().GetDerivedTypeCount() > 1	&&
			 type.GetDerivedTypeList().GetDerivedType(1)->GetDerivedTypeCode() ==
			 DerivedType::DT_POINTER_TO_MEMBER) )
		{
			// проверяем константность указателя на член
			if( static_cast<const PointerToMember &>(
			 		*type.GetDerivedTypeList().GetDerivedType(
					type.GetDerivedTypeList().IsReference() ? 1 : 0) ).IsConst() )
				throw type;			
		}

		// иначе ошибка. Мы не проверяем тип на ссылку, т.к. он может быть
		// либо указателем, либо арифметическим, и это проверяется при проверке типа
		else
			throw type;
	
	} catch( const POperand &op ) {
		theApp.Error(errPos, "'%s' - операнд не является lvalue", 
			op->IsPrimaryOperand() ? ExpressionPrinter(op).GetExpressionString().c_str() :
			opName );
		return false;

	} catch( const TypyziedEntity &type ) {
		theApp.Error(errPos, 
			"'%s' - тип не является модифицируемым lvalue; '%s' - оператор требует lvalue", 
			type.GetTypyziedEntityName(false).c_str(), opName );
		return false;
	}

	return true;
}


// создает наибольший тип из двух, при условии что оба арифметические и
// возвращает результат в виде вновь созданной сущности
TypyziedEntity *ExpressionMakerUtils::DoResultArithmeticOperand( const TypyziedEntity &op1,
		const TypyziedEntity &op2 )
{
		// далее оба операнда следует преобразовать к наибольшему типу
	const BaseType &bt1 = op1.GetBaseType(), 
				   &bt2 = op2.GetBaseType();
	BaseType::BT btc1 = bt1.GetBaseTypeCode(),
				 btc2 = bt2.GetBaseTypeCode();
	int tbtc = (btc1 == BaseType::BT_FLOAT || btc1 == BaseType::BT_DOUBLE || 
		btc2 == BaseType::BT_FLOAT || btc2 == BaseType::BT_DOUBLE) ? KWDOUBLE : KWINT;
	int sign = -1, size = -1;
	if( tbtc == KWINT && (bt1.IsUnsigned() || bt2.IsUnsigned()) )
		sign = KWUNSIGNED;
		
	// устанавливаем размер
	if( tbtc == KWINT && (bt1.IsLong() || bt2.IsLong()) )
		size = KWLONG;

	// иначе если результирующий тип double, long будет только в том
	// случае, если он у double
	else if( tbtc == KWDOUBLE && 
		((btc1 == BaseType::BT_DOUBLE && bt1.IsLong()) ||
		 (btc2 == BaseType::BT_DOUBLE && bt2.IsLong())) )
		size = KWLONG;

	// создаем сущность
	return new TypyziedEntity( 
		(BaseType*)&ImplicitTypeManager(tbtc, sign, size).GetImplicitType(),
		false, false, DerivedTypeList() );
}


// возвратить копию типа, если тип является ссылочным, убрать ссылку
TypyziedEntity *ExpressionMakerUtils::DoCopyWithIndirection( const TypyziedEntity &type )
{
	TypyziedEntity *rval = new TypyziedEntity(type);
	if( rval->GetDerivedTypeList().IsReference() )
		const_cast<DerivedTypeList &>(rval->GetDerivedTypeList()).PopHeadDerivedType();
	return rval;
}


// проверяет доступность конструктора по умолчанию, конструктора копирования, 
// деструктора по требованию. Используется при сооздании или инициализации
// объекта
bool ExpressionMakerUtils::ObjectCreationIsAccessible( const ClassType &cls, 
		const Position &errPos, bool ctor, bool copyCtor, bool dtor )
{
	bool flgs[] = { ctor, copyCtor, dtor };
	for( int i = 0; i<3; i++ )
	{
		const Method *meth = NULL;
		if( !flgs[i] )
			continue;

		// проверяем деструктор
		if( i == 2 )
		{		
			if( cls.GetDestructor() == NULL )
			{
				theApp.Error( errPos, 
					"'~%s()' - деструктор не объявлен; удаление объекта невозможно",
					cls.GetName().c_str());
				return false;
			}

			meth = cls.GetDestructor();		
		}

		else
		{
			const ConstructorList &ctorLst = cls.GetConstructorList();
		
			// ищем конструктор либо без параметров. либо с копированием
			// Должен быть доступен и однозначен
			for( ConstructorList::const_iterator p = ctorLst.begin(); 
					 p != ctorLst.end(); p++ )
			{
				const ConstructorMethod &cm = **p;
				if( ctor )
				{
					if( cm.GetFunctionPrototype().GetParametrList().
						GetFunctionParametrCount() > 0 && 
						!cm.GetFunctionPrototype().GetParametrList().
						 GetFunctionParametr(0)->IsHaveDefaultValue() )
						continue;
					
					// не может быть неоднозначности
					if( meth )
					{
						theApp.Error( errPos,
							"неоднозначность между '%s' и '%s'; создание объекта невозможно",
							meth->GetTypyziedEntityName().c_str(), 
							cm.GetTypyziedEntityName().c_str() );
						return false;
					}
					else
						meth = &cm;
				}

				else if( copyCtor )
				{
					// к-тор должен принимать один параметр
					int cnt = cm.GetFunctionPrototype().GetParametrList().
							  GetFunctionParametrCount() ;
					const ConstructorMethod *temp = NULL;
					if( (cnt == 1)												||						
						(cnt > 1 && cm.GetFunctionPrototype().GetParametrList().
									GetFunctionParametr(1)->IsHaveDefaultValue()) )
					{
						const Parametr &prm = *cm.GetFunctionPrototype().GetParametrList().
											  GetFunctionParametr(0);
						if( &prm.GetBaseType() == &cls &&
							prm.GetDerivedTypeList().IsReference() &&
							prm.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
							temp = &cm;
					}
					
					else if(cnt == 0 && cm.GetFunctionPrototype().IsHaveEllipse())
						temp = &cm;
						
					else
						continue;

					// проверяем, если имеем конструктор копирования, нужно проверить,
					// чтобы не было неоднозначности
					if( temp )
					{
						if( meth )
						{
							theApp.Error( errPos,
								"неоднозначность между '%s' и '%s'; создание объекта невозможно",
								meth->GetTypyziedEntityName().c_str(), 
								temp->GetTypyziedEntityName().c_str() );

							return false;
						}
						else
							meth = temp;
					}					
				}
			}

			// если конструктор не найден, вывести ошибку
			if( meth == NULL )
			{
				theApp.Error( errPos, 
					"'%s' - %s не объявлен; создание объекта невозможно",
					(string(cls.GetName().c_str()) + 
					(ctor ? "()" : "(const " + string(cls.GetName().c_str()) + "&)")).c_str() , 
					ctor ? "конструктор по умолчанию" : "конструктор копирования");
				return false;
			}		
			
			// иначе, если конструктор, проверяем возможно ли создание объекта
			// абстрактного класса
			else if( meth->IsConstructor() && 
				static_cast<const ClassType&>(meth->GetSymbolTableEntry()).IsAbstract() )
			{
				theApp.Error( errPos, 
					"создание объекта класса '%s' невозможно; класс является абстрактным",
					static_cast<const ClassType&>(meth->GetSymbolTableEntry()).
					GetQualifiedName().c_str());
				return false;			
			}
		}

		INTERNAL_IF( meth == NULL );
		AccessControlChecker acc( 
			GetCurrentSymbolTable().IsLocalSymbolTable() ? 
			GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable(), cls, *meth);
		if( !acc.IsAccessible() )
		{
			theApp.Error( errPos, (string("\'") + meth->GetTypyziedEntityName().c_str() + 
				"\' - недоступен; создание и удаление объекта невозможно").c_str());
			return false;
		}
	}

	return true;
}


// преобразование к rvalue, необходимо, чтобы тип был полностью объявлен 
// и не void. Также проверяет, чтобы операнд был Primary или Expression
bool ExpressionMakerUtils::ToRValue( const POperand &val, const Position &errPos )
{
	if( val->IsErrorOperand() )
		return true;

	// эти операнды не подходят
	if( val->IsOverloadOperand() || val->IsTypeOperand() )
	{
		theApp.Error(errPos, "невозможно получить 'rvalue' из '%s'",
			val->IsTypeOperand() ? "типа" : "перегруженной функции" );
		return false;
	}

	// иначе получаем тип и проверяем
	const BaseType &bt = val->GetType().GetBaseType();
	if( bt.GetBaseTypeCode() == BaseType::BT_VOID ||
		(bt.IsClassType() && static_cast<const ClassType&>(bt).IsUncomplete()) )
	{
		theApp.Error(errPos, "невозможно получить 'rvalue' из '%s'",
			bt.GetBaseTypeCode() == BaseType::BT_VOID ? "void" : 
			"незавершенного класса" );
		return false;
	}

	return true;
}

// преобразование к rvalue при копировании. Допускается необъявленный класс,
// если производный тип ссылка или указатель
bool ExpressionMakerUtils::ToRvalueCopy( const POperand &val, const Position &errPos )
{
	if( val->IsErrorOperand() )
		return true;

	// эти операнды не подходят
	if( val->IsOverloadOperand() || val->IsTypeOperand() )
	{
		theApp.Error(errPos, "невозможно получить 'rvalue' из '%s'",
			val->IsTypeOperand() ? "типа" : "перегруженной функции" );
		return false;
	}

	// иначе получаем тип и проверяем
	const BaseType &bt = val->GetType().GetBaseType();
	if( bt.GetBaseTypeCode() == BaseType::BT_VOID )
	{
		theApp.Error(errPos, "невозможно получить 'rvalue' из 'void'");
		return false;
	}

	else if( bt.IsClassType() && static_cast<const ClassType&>(bt).IsUncomplete() )
	{
		if( val->GetType().GetDerivedTypeList().IsPointer() ||
			val->GetType().GetDerivedTypeList().IsReference() )
			;
		else
		{
			theApp.Error(errPos, "невозможно получить 'rvalue' из 'незавершенного класса'");			
			return false;
		}
	}

	return true;
}


// общая функция преобразования и проверки типа в определенную категорию
static bool ToCastTypeConverter( POperand &op, const Position &errPos, 
	const string &opName, OperatorCaster::ACC castCategory, PCSTR tname, 
	bool categoryChecker(const TypyziedEntity &) )
{
	// входные данные должны обеспечивать получение типа, это условие
	// проверяется перед вызовом этой функции (в координаторе)
	INTERNAL_IF( !(op->IsExpressionOperand() || op->IsPrimaryOperand()) );
	register const TypyziedEntity &type = op->GetType();

	// если тип классовый, пытаемся преобразовать с помощью оператора приведения
	if( ExpressionMakerUtils::IsClassType(type) )
	{
		OperatorCaster opCaster( castCategory, type );
		opCaster.ClassifyCast();

		// если преобразование невозможно выводим ошибку
		if( !opCaster.IsConverted() )
		{
			if( !opCaster.GetErrorMessage().empty() )
				theApp.Error( errPos, opCaster.GetErrorMessage().c_str() );
			else
				theApp.Error( errPos, 
					"'%s' - класс не содержит оператора преобразования в '%s тип'",
				static_cast<const ClassType&>(type.GetBaseType()).GetQualifiedName().c_str(),
				tname);
			return false;
		}

		// иначе выполняем физическое преобразование, 
		opCaster.DoCast(op, op, errPos);	
		return true;		// преобразование успешно выполнено
	}

	// иначе проверяем только, является ли тип верным
	if( !categoryChecker(type) )
	{
		theApp.Error( errPos,
			"'%s' - не %s тип; '%s' - оператор требует %s тип",
			type.GetTypyziedEntityName(false).c_str(), tname, opName.c_str(), tname );
		return false;
	}

	// тип является верным, возвращаем его
	return true;
}


// преобразование типа к целому или перечислимому типу. В случае
// если преобазование возможно, возвращает операнд, иначе выводим ошибку
// и возвращает NULL. Используется при проверке выражений. Также
// проверяет возможность преобразования операнда к rvalue
bool ExpressionMakerUtils::ToIntegerType( 
		POperand &op, const Position &errPos, const string &opName )
{
	return ToCastTypeConverter(op, errPos, opName, OperatorCaster::ACC_TO_INTEGER, 
		"целый", ExpressionMakerUtils::IsIntegral );
}


// преобразование типа к арифметическому типу
bool ExpressionMakerUtils::ToArithmeticType(
		POperand &op, const Position &errPos,  const string &opName )
{
	return ToCastTypeConverter(op, errPos, opName, OperatorCaster::ACC_TO_ARITHMETIC, 
		"арифметический", ExpressionMakerUtils::IsArithmetic );
}


// преобразование типа к типу указателя
bool ExpressionMakerUtils::ToPointerType( 
		POperand &op, const Position &errPos,  const string &opName )
{
	return ToCastTypeConverter(op, errPos, opName, OperatorCaster::ACC_TO_POINTER, 
		"адресный", ExpressionMakerUtils::IsRvaluePointer );
}


// преобразование типа к склярному типу 
bool ExpressionMakerUtils::ToScalarType( 
		POperand &op, const Position &errPos,  const string &opName )
{
	return ToCastTypeConverter(op, errPos, opName, OperatorCaster::ACC_TO_SCALAR, 
		"склярный", ExpressionMakerUtils::IsScalarType);
}

// преобразование к арифметическому типу или указателю
bool ExpressionMakerUtils::ToArithmeticOrPointerType( POperand &op, 
		const Position &errPos, const string &opName )
{
	return ToCastTypeConverter(op, errPos, opName, OperatorCaster::ACC_TO_ARITHMETIC_OR_POINTER, 
		"арифметический или адресный", ExpressionMakerUtils::IsArithmeticOrPointer);
}


// проверить, если нестатический данное-член класса, 
// используется без this, тогда вывести ошибку и вернуть false
int ExpressionMakerUtils::CheckMemberThisVisibility( 
		const POperand &oper, const Position &errPos, bool printError )
{
	// операнд должен быть данным-членом
	const TypyziedEntity &te = oper->GetType().IsDynamicTypyziedEntity()		 ?
		static_cast<const DynamicTypyziedEntity&>(oper->GetType()).GetOriginal() :
		oper->GetType();
	const Identifier *member = NULL;

	// если функция
	if( te.IsFunction() &&
		static_cast<const Function&>(te).IsClassMember() &&
		static_cast<const Function&>(te).GetStorageSpecifier() != Function::SS_STATIC)
		member = &static_cast<const Function &>(te);

	// если объект
	else if( te.IsObject() &&
		static_cast<const ::Object &>(te).IsClassMember() &&
		static_cast<const ::Object &>(te).GetStorageSpecifier() != ::Object::SS_STATIC)
		member = &static_cast<const ::Object &>(te);

	// иначе не член. Соотв. операнду this не нужен, возвр. 1
	else
		return 1;

	// получаем класс члена и проверяем какое отношение имеет 
	// текущая область видимости к этому классу
	const ClassType &mcls = static_cast<const ClassType&>(member->GetSymbolTableEntry());
	const SymbolTable &st = GetCurrentSymbolTable().IsLocalSymbolTable() ?
		GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable();

	// проверяем, если функциональная, то можно продолжать проверку
	if( st.IsFunctionSymbolTable() &&
		static_cast<const FunctionSymbolTable&>(st).GetFunction().IsClassMember() &&
		static_cast<const FunctionSymbolTable&>(st).GetFunction().GetStorageSpecifier() !=
		 Function::SS_STATIC )
	{
		// получаем класс, к которому принадлежит метод в котором мы находимся,
		// класс должен либо совпадать с классом члена, либо
		// быть производным для него
		const Method &curMeth = static_cast<const Method &>(
			static_cast<const FunctionSymbolTable&>(st).GetFunction());
		const ClassType &cc = static_cast<const ClassType &>(curMeth.GetSymbolTableEntry());
		if( &cc == &mcls ||
			DerivationManager( mcls, cc ).IsBase() )
			return 0;
	}

	// если доходим до сюда, значит выводим ошибку, и возвращаем -1,
	// как признак ошибки
	if( printError )
		theApp.Error(errPos, 
		"'%s' - член не может использоваться в текущей области видимости; отсутствует 'this'",
		member->GetQualifiedName().c_str());
	return -1;
}


// проверить корректность инициализации объекта. Сравнивается только
// тип и список инициализаторов. Не инициализируемые элементы, такие как
// функция не учитываются. 
ExpressionMakerUtils::InitAnswer ExpressionMakerUtils::CorrectObjectInitialization( 
	const TypyziedEntity &obj, const PExpressionList &initList, bool checkDtor, 
	const Position &errPos )
{
	INTERNAL_IF( initList.IsNull() );

	// сначала проверим, если тип классовый, значит требуется вызов к-ра.
	// Если имеем массив классовых объектов, значит требуется к-ор по умолчанию
	bool clsType = obj.GetBaseType().IsClassType(), array = false;int i;
	for( int i = 0; i<obj.GetDerivedTypeList().GetDerivedTypeCount(); i++ )
	{
		if( obj.GetDerivedTypeList().GetDerivedType(i)->GetDerivedTypeCode() !=
			DerivedType::DT_ARRAY )
		{
			clsType = false; 
			break;
		}
			
		else if( i == obj.GetDerivedTypeList().GetDerivedTypeCount()-1 )
			array = true;
	}

	// проверяем список инициализаторов, чтобы в нем не было типов, перегруженных
	// функций, error operand'ов, членов класса без this
	for( i = 0; i<initList->size(); i++ )
	{
		const POperand &op = initList->at(i);
		if( op->IsErrorOperand() )
			return false;
	}

	// сначала проверим, если имеем массив, значит список инициализации должен быть пустой
	if( array && !initList->empty() )
	{
		theApp.Error(errPos, "список инициализаторов должен быть пустой для массива");
		return false;
	}

	// далее проверяем, если тип классовый, значит следует проверить наличие конструктора
	if( clsType )
	{	
		// проверяем наличие конструкторов
		const ClassType &cls = static_cast<const ClassType&>(obj.GetBaseType());
		const ConstructorList &ctorLst = cls.GetConstructorList();
		if( ctorLst.empty() )
		{
			// если имеем объект, и он имеет внешнее связывание,
			// то наличие конструкторов не обязательно
			if( obj.IsObject() && static_cast<const ::Object &>(obj).
				GetStorageSpecifier() == ::Object::SS_EXTERN && initList->empty() )
				return true;

			// иначе ошибка
			theApp.Error(errPos, 
				"'%s' - в классе отсутствуют конструкторы; инициализация невозможна",
				cls.GetQualifiedName().c_str());
			return false;
		}

		OverloadFunctionList ofl(ctorLst.size());			
		copy(ctorLst.begin(), ctorLst.end(), ofl.begin());
				
		// проверяем наличие соотв. конструктора
		OverloadResolutor or(ofl, *initList, NULL);
		const Function *fn = or.GetCallableFunction();
	
		// если конструктор не найден, выведем ошибку
		if( fn == NULL )
		{
			theApp.Error(errPos, "%s; инициализация невозможна",
				or.GetErrorMessage().c_str());
			return false;
		}
		
		// выполняем преобразование каждого параметр в целевой тип
		or.DoParametrListCast(errPos);			

		// проверим возможность копирования параметров,				
		FunctionCallBinaryMaker::CheckParametrInitialization( initList,
			fn->GetFunctionPrototype().GetParametrList(), errPos);

		// иначе проверяем доступность этой функции
		AccessControlChecker acc( 
			GetCurrentSymbolTable().IsLocalSymbolTable() ? 
			GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable(),
			cls, *fn);
		if( !acc.IsAccessible() )
			theApp.Error( errPos, (string("\'") + fn->GetTypyziedEntityName().c_str() + 
				"\' - конструктор недоступен; инициализация невозможна").c_str());		

		// проверяем , чтобы класс не был абстарктым. Создание объекта
		// абстрактного класса невозможно
		if( cls.IsAbstract() || cls.IsUncomplete() )
		{
			theApp.Error( errPos,
				"создание объекта класса '%s' невозможно; класс является %s",
				cls.GetQualifiedName().c_str(), cls.IsAbstract() ? 
				"абстрактным" : "неполным" );			
			return false;
		}
		
		// если требуется проверить наличие деструткора
		if( checkDtor )
			ExpressionMakerUtils::ObjectCreationIsAccessible( cls, errPos, false, false, true);

		// была произведена инициализация конструктором, вернуть ответ с конструктором		
		return InitAnswer( static_cast<const ConstructorMethod &>(*fn) );
	}

	// иначе проверяем инициализацию склярного типа не-конструктором
	else
	{
		// если инициализаторов несколько, инициализация невозможна
		if( initList->size() > 1 )
		{
			theApp.Error(errPos, 
				"склярный тип '%s' не может инициализироваться несколькими значениями",
				obj.GetTypyziedEntityName(false).c_str());	
			return false;
		}

		// инициализация по умолчанию
		if( initList->size() == 0 )
			return true;

		// иначе инициализатор один и следует проверить типы
		PCaster pc = AutoCastManager( obj, initList->at(0)->GetType(), true ).RevealCaster();
		pc->ClassifyCast();
		if( !pc->IsConverted() )
		{
			if( pc->GetErrorMessage().empty() )
				theApp.Error(errPos, 
					"невозможно привести '%s' к '%s'; инициализация невозможна",
					obj.GetTypyziedEntityName(false).c_str(), 
					initList->at(0)->GetType().GetTypyziedEntityName(false).c_str());
			else
				theApp.Error(errPos, "'инициализация' - %s", pc->GetErrorMessage().c_str());
			return false;
		}

		pc->DoCast(initList->at(0), const_cast<POperand&>(initList->at(0)), errPos);
		return true;
	}
}


// вернуть построенный вызов, либо NULL, если перегруженный оператор вызвать
// невозможно
POperand UnaryOverloadOperatorCaller::Call() const
{
	// операнд должен иметь тип
	if( !(right->IsPrimaryOperand() || right->IsExpressionOperand()) ) 
		return NULL;

	// оператор должен быть перегружаемым
	if( opCode == KWSIZEOF || opCode == KWTYPEID || opCode == KWTHROW ||
		abs(opCode) == KWDELETE || abs(opCode) == OC_DELETE_ARRAY )
		return NULL;

	const TypyziedEntity &type = right->GetType();
	bool cor = (type.GetBaseType().IsClassType() || type.GetBaseType().IsEnumType()) &&
		(type.GetDerivedTypeList().IsEmpty()  || 
		 (type.GetDerivedTypeList().GetDerivedTypeCount() == 1 && 
		  type.GetDerivedTypeList().IsReference()) );
	// тип также должен быть перегружаемым
	if( !cor )
		return NULL;
	const ClassType *cls = type.GetBaseType().IsClassType() ? 
		&static_cast<const ClassType &>(type.GetBaseType()) : NULL;

	// проверяем, необходим ли поиск оператора в глобальной области видимости	
	OverloadOperatorFounder oof(abs(opCode), opCode != ARROW, cls, errPos);

	// если не найден, вернуть NULL
	if( !oof.IsFound() )
		return NULL;

	// если оператор неоднозначен, вернуть ErrorOperand
	else if( oof.IsAmbigous() )
		return ErrorOperand::GetInstance();

	// иначе оператор однозначен, строим вызов
	// сначала проверим, если оба списка не пусты, выявим в каком из них находится
	// необходимая функция. Если в обоих - неоднозначность
	if( !oof.GetClassOperatorList().empty() && !oof.GetGlobalOperatorList().empty() )
	{
		PExpressionList plist = new ExpressionList;				

		// если оператор постфиксного -кремента, задаем первый параметр как '0'
		if( opCode == INCREMENT || opCode == DECREMENT )
			plist->push_back( null );

		const Function *fn1, *fn2;
		OverloadFunctionList &cofl = const_cast<OverloadFunctionList &>(
			oof.GetClassOperatorList()), &gofl = const_cast<OverloadFunctionList &>(
			oof.GetGlobalOperatorList());

		// если в классе нет подходящего оператора, выбираем только из
		// глобальных
		if( (fn1 = OverloadResolutor(cofl, 
					*plist, &right->GetType()).GetCallableFunction() ) == NULL )					
			cofl.clear();

		// иначе проверяем глобальные
		else
		{
			plist->clear();
			plist->push_back(right);			
			if( opCode == INCREMENT || opCode == DECREMENT )
				plist->push_back( null );

			// если среди глобальных нет подходящего, выбираем только из
			// классовых
			if( (fn2 = OverloadResolutor(gofl, *plist).GetCallableFunction()) == NULL )
			{
				gofl.clear();
				// в другом списке оставляем только одну функцию
				cofl.clear();
				cofl.push_back(fn1);
			}


			// в противном случае неоднозначность
			else
			{
				theApp.Error(errPos,
					"неоднозначность между '%s' и '%s'",
					fn1->GetTypyziedEntityName().c_str(), fn2->GetTypyziedEntityName().c_str());
				return ErrorOperand::GetInstance();
			}
		}					
	}

	// если классовый список пуст, строим вызов обычной функции с одним параметром
	if( oof.GetClassOperatorList().empty() )
	{
		INTERNAL_IF( oof.GetGlobalOperatorList().empty() );
		POperand pol = new OverloadOperand( oof.GetGlobalOperatorList() );

		// передаем два параметра и список функций, строителю вызовов
		PExpressionList plist = new ExpressionList;		
		plist->push_back(right);
		if( opCode == INCREMENT || opCode == DECREMENT )
			plist->push_back( null );

		// строим вызов, последний параметр означает что вызов неявный,
		// если функция не подходит для вызова, вернет NULL
		return FunctionCallBinaryMaker(pol, plist, OC_FUNCTION, errPos, true).Make();
	}

	// иначе если список глобальных операторов пуст, построить вызов метода
	else if( oof.GetGlobalOperatorList().empty() )
	{
		INTERNAL_IF( oof.GetClassOperatorList().empty() );
		POperand pol = new OverloadOperand( oof.GetClassOperatorList() );
		 
		// создаем сначала обращение к члену, следует заметить, что 
		// тип операнда неизвестен, т.к. имеем список функций
		POperand select = new BinaryExpression( '.', false, right, pol, NULL );
		PExpressionList plist = new ExpressionList;		
		if( opCode == INCREMENT || opCode == DECREMENT )
			plist->push_back( null );

		// строим вызов 
		return FunctionCallBinaryMaker(select, plist, OC_FUNCTION, errPos, true).Make();
	}

	else
	{
		INTERNAL( "'BinaryOverloadOperatorCaller::Call' - оба списка не пусты");
		return NULL;	// kill warning
	}
}


// вернуть построенный вызов, либо NULL, если перегруженный оператор вызвать
// невозможно
POperand BinaryOverloadOperatorCaller::Call() const
{
	// в первую очередь оба операнда должны быть выражением или основным
	if( !( (left->IsPrimaryOperand()  || left->IsExpressionOperand()) &&
		   (right->IsPrimaryOperand() || right->IsExpressionOperand()) ) )
		return NULL;

	// далее проверяем, если это оператор, который не может быть перегружен,
	// выходим
	if( opCode == '.' || opCode == DOT_POINT || opCode == OC_CAST ||
		opCode == KWDYNAMIC_CAST || opCode == KWSTATIC_CAST || 
		opCode == KWREINTERPRET_CAST || opCode == KWCONST_CAST )
		return NULL;
	
	// далее один из операндов иметь классовый тип или перечисления
	const TypyziedEntity &te1 = left->GetType(), &te2 = right->GetType();
	bool cor1 = (te1.GetBaseType().IsClassType() || te1.GetBaseType().IsEnumType()) &&
		(te1.GetDerivedTypeList().IsEmpty() || (te1.GetDerivedTypeList().
		 GetDerivedTypeCount() == 1 && te1.GetDerivedTypeList().IsReference()) ),
		 cor2 = (te2.GetBaseType().IsClassType() || te2.GetBaseType().IsEnumType()) &&
		(te2.GetDerivedTypeList().IsEmpty() || (te2.GetDerivedTypeList().
		 GetDerivedTypeCount() == 1 && te2.GetDerivedTypeList().IsReference()) );

	// если оба типа склярные, выйти
	if( !cor1 && !cor2 )
		return NULL;

	// иначе если первый тип классовый, задаем класс
	const ClassType *cls = cor1 && te1.GetBaseType().IsClassType() ? 
		&static_cast<const ClassType &>(te1.GetBaseType()) : NULL;
	bool evrywhere = !(opCode == '=' || opCode == OC_FUNCTION || opCode == ARROW || 
					   opCode == OC_ARRAY);

	// проверяем, необходим ли поиск оператора в глобальной области видимости	
	OverloadOperatorFounder oof(opCode, evrywhere, cls, errPos);

	// если не найден, вернуть NULL
	if( !oof.IsFound() )
		return NULL;

	// если оператор неоднозначен, вернуть ErrorOperand
	else if( oof.IsAmbigous() )
		return ErrorOperand::GetInstance();

	// если один из списков пуст, тогда вызов можно строить с
	// помощью FunctionCallBinaryMaker.

	// сначала проверим, если оба списка не пусты, выявим в каком из них находится
	// необходимая функция. Если в обоих - неоднозначность
	if( !oof.GetClassOperatorList().empty() && !oof.GetGlobalOperatorList().empty() )
	{
		PExpressionList plist = new ExpressionList;				
		plist->push_back(right);

		const Function *fn1, *fn2;
		OverloadFunctionList &cofl = const_cast<OverloadFunctionList &>(
			oof.GetClassOperatorList()), &gofl = const_cast<OverloadFunctionList &>(
			oof.GetGlobalOperatorList());

		// если в классе нет подходящего оператора, выбираем только из
		// глобальных
		if( (fn1 = OverloadResolutor(cofl, 
					*plist, &left->GetType()).GetCallableFunction() ) == NULL )					
			cofl.clear();

		// иначе проверяем глобальные
		else
		{
			plist->front() = left;
			plist->push_back(right);			

			// если среди глобальных нет подходящего, выбираем только из
			// классовых
			if( (fn2 = OverloadResolutor(gofl, *plist).GetCallableFunction()) == NULL )
			{
				gofl.clear();
				// в другом списке оставляем только одну функцию
				cofl.clear();
				cofl.push_back(fn1);
			}


			// в противном случае неоднозначность
			else
			{
				theApp.Error(errPos,
					"неоднозначность между '%s' и '%s'",
					fn1->GetTypyziedEntityName().c_str(), fn2->GetTypyziedEntityName().c_str());
				return ErrorOperand::GetInstance();
			}
		}					
	}

	// если классовый список пуст, строим вызов обычной функции с двумя параметрами
	if( oof.GetClassOperatorList().empty() )
	{
		INTERNAL_IF( oof.GetGlobalOperatorList().empty() );
		POperand pol = new OverloadOperand( oof.GetGlobalOperatorList() );

		// передаем два параметра и список функций, строителю вызовов
		PExpressionList plist = new ExpressionList;
		plist->push_back(left);
		plist->push_back(right);

		// строим вызов 
		return FunctionCallBinaryMaker(pol, plist, OC_FUNCTION, errPos, true).Make();
	}

	// иначе если список глобальных операторов пуст, построить вызов метода
	else if( oof.GetGlobalOperatorList().empty() )
	{
		INTERNAL_IF( oof.GetClassOperatorList().empty() );
		POperand pol = new OverloadOperand( oof.GetClassOperatorList() );
		 
		// создаем сначала обращение к члену, следует заметить, что 
		// тип операнда неизвестен, т.к. имеем список функций
		POperand select = new BinaryExpression( '.', false, left, pol, NULL );
		PExpressionList plist = new ExpressionList;		
		plist->push_back(right);

		// строим вызов 
		return FunctionCallBinaryMaker(select, plist, OC_FUNCTION, errPos, true).Make();
	}

	else
	{
		INTERNAL( "'BinaryOverloadOperatorCaller::Call' - оба списка не пусты");
		return NULL;	// kill warning
	}
}


// возвращает размер базового типа
int SizeofEvaluator::GetBaseTypeSize( const BaseType &bt ) const
{
	BaseType::BT btc = bt.GetBaseTypeCode();
	if( btc == BaseType::BT_CLASS || btc == BaseType::BT_STRUCT ||
		btc == BaseType::BT_UNION )
		return EvalClassSize( static_cast<const ClassType &>(bt) );

	// для перечислимого типа, также возвращается константа
	else if( btc == BaseType::BT_ENUM )
		return ENUM_TYPE_SIZE;

	// размер типа void неизвестен
	else if( btc == BaseType::BT_VOID )
	{
		errMsg = "размер типа void неизвестен";
		return -1;
	}

	// иначе возвращаем размер с помощью менеджера
	else
		return ImplicitTypeManager( bt ).GetImplicitTypeSize();
}


// возвращает размер структуры, класса или перечисления
int SizeofEvaluator::EvalClassSize( const ClassType &cls ) const
{
	bool ucls = cls.GetBaseTypeCode() == BaseType::BT_UNION;
	int sz = 0;

	// если класс не полностью объявлен, это ошибка
	if( cls.IsUncomplete() )
	{
		errMsg = "класс не полностью объявлен";
		return -1;
	}

	// вычисляем размер каждого члена, и после размеры базовых классов
	const ClassMemberList &cml = cls.GetMemberList();
	for( int i = 0; i<cml.GetClassMemberCount(); i++ )
	{
		const ClassMember &cm = *cml.GetClassMember(i);

		// вычисление размера возможно только если имеем данное-член
		if( const DataMember *dm = dynamic_cast<const DataMember *>(&cm) )
		{
			// статические члены и типы не учитываются при подсчете размера
			if( dm->IsStaticMember() || dm->GetStorageSpecifier() == ::Object::SS_TYPEDEF )
				continue;

			SizeofEvaluator se(*dm);
			int msz = se.Evaluate();
			if( msz < 0 )
			{
				errMsg = "один из членов класса имеет некорректный тип"; 
				return -1;
			}

			// иначе увеличиваем общий размер если это класс или выбираем
			// наибольший, если это объединение
			sz = ucls ? (msz > sz ? msz : sz) : (sz + msz);
		}
	}

	// если имеем объединение, то вычислять размеры базовых классов не нужно
	INTERNAL_IF( sz < 0 );
	if( ucls )
		return sz > 0 ? sz : EMPTY_CLASS_SIZE;	

	// вычисляем размер базовых классов
	const BaseClassList &bcl = cls.GetBaseClassList();
	int i;
	for( i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &bcc = *bcl.GetBaseClassCharacteristic(i);

		// виртуальные классы находятся не в самом классе, а за его пределами. 
		// Внутри самого класса хранится только указатель на этот объект. 
		// Поэтому при наследовании виртуального базового класса, производный
		// класс увеличивается на размер указателя, а не на размер самого класса.
		if( bcc.IsVirtualDerivation() )
			sz += DEFAULT_POINTER_SIZE;

		// иначе высчитываем размер всего базового класса
		else
			sz += EvalClassSize( bcc.GetPointerToClass() );		
	}

	// если класс создал собственную vm-таблицу, следоват. его размер 
	// увеличился на 4
	if( cls.IsMadeVmTable() )
		sz += DEFAULT_POINTER_SIZE;

	return sz > 0 ? sz : EMPTY_CLASS_SIZE;
}


// вычислить размер типа, если тип некорректен, вернуть -1
int SizeofEvaluator::Evaluate() const
{
	// если нет производных типов, вернем размер базового типа и все
	const DerivedTypeList &dtl = type.GetDerivedTypeList();
	if( dtl.IsEmpty() )
		return GetBaseTypeSize(type.GetBaseType());

	// иначе проверяем, с каким типом имеем дело
	const DerivedType &dt =  *dtl.GetDerivedType(0);
	DerivedType::DT dc = dt.GetDerivedTypeCode();
			
	if( dc == DerivedType::DT_POINTER )
		return DEFAULT_POINTER_SIZE;

	else if( dc == DerivedType::DT_POINTER_TO_MEMBER )
		return DEFAULT_POINTER_TO_MEMBER_SIZE;

	else if( dc == DerivedType::DT_REFERENCE )
		return DEFAULT_REFERENCE_SIZE;

	else if( dc == DerivedType::DT_FUNCTION_PROTOTYPE )
	{
		errMsg = "функция не имеет размера";
		return -1;
	}

	// если битовое поле
	else if( type.IsObject() && 
		static_cast<const ::Object &>(type).GetStorageSpecifier() == ::Object::SS_BITFIELD )
	{
		errMsg = "битовое поле не имеет размера";
		return -1;		
	}

	// иначе массив
	int sz = 1;
	for( int i = 0; i<dtl.GetDerivedTypeCount(); i++ )
	{
		const DerivedType &dt =  *dtl.GetDerivedType(i);
		DerivedType::DT dc = dt.GetDerivedTypeCode();

		if( dc == DerivedType::DT_ARRAY )
		{	
			int arsz = static_cast<const Array&>(dt).GetArraySize();			
			if( arsz <= 0 )
			{
				errMsg = "массив неизвестного размера";
				return -1;
			}

			else
				sz *= arsz;
		}

		else if( dc ==  DerivedType::DT_POINTER || dc == DerivedType::DT_REFERENCE || 
			dc == DerivedType::DT_POINTER_TO_MEMBER )
			return sz * DEFAULT_POINTER_SIZE;

		else
			INTERNAL("'SizeofEvaluator::Evaluate()' - некорректный производный тип");
	}

	// был тип только массивы, вернуть размер типа * размер массива
	return sz * GetBaseTypeSize(type.GetBaseType());
}


// интерпретировать. Если интерпретация невозможна вернуть NULL,
// иначе указатель на новый операнд
POperand UnaryInterpretator::Interpretate() const
{
	// получаем значение операнда
	double opval = 0;

	// в первую очередь проверяем на sizeof
	if( op == KWSIZEOF )
	{
		const TypyziedEntity &te = static_cast<const PrimaryOperand &>(*cnst).GetType();
		SizeofEvaluator se(te);
		opval = se.Evaluate();
		if( opval < 0 )
			theApp.Error( errPos,
				"'sizeof' не вычислил размер выражения; %s", se.GetErrorMessage());

		// формируем литерал типа 'unsigned int' в качестве результата
		Literal *res = new Literal( 
			(BaseType *)&ImplicitTypeManager(KWINT, KWUNSIGNED).GetImplicitType(),
			true, false, DerivedTypeList(), CharString( int(opval) ) );
				
		return new PrimaryOperand( false, *res );		
	}


	// далее если не sizeof, проверяем операнд на интерпретируемость
	if( !ExpressionMakerUtils::IsInterpretable( cnst, opval ) )
		return NULL;

	// после получения значения, вычисляем. Причем в случае с нектороыми
	// операторами, следует проверять, чтобы тип был целым
	if( op == '!' )
		opval = !opval;

	// для этого операнда, оператор должен быть целым
	else if( op == '~' )
	{
		if( !ExpressionMakerUtils::IsIntegral( 
			static_cast<const PrimaryOperand &>(*cnst).GetType()) )
		{
			theApp.Error( errPos, "оператор '~' применим только к целым типам" );
			opval = 0;
		}

		else
			opval = ~(int)opval;
	}

	else if( op ==  '-' )
		opval = -opval;
	
	// интерпретация данного оператора невозможна
	else
		return NULL;

	// после всех вычислений, создаем новый PrimaryOperand, с параметром
	// литерала
	CharString sval;
	const PrimaryOperand &pop = static_cast<const PrimaryOperand &>(*cnst);
	BaseType::BT bt = pop.GetType().GetBaseType().GetBaseTypeCode();
	if( bt == BaseType::BT_FLOAT ||  bt == BaseType::BT_DOUBLE )
		sval = opval;
	else
		sval = (int)opval;

	// создаем новый операнд
	const TypyziedEntity &te = pop.GetType();
	return new PrimaryOperand( false, *new Literal( 
		const_cast<BaseType *>(&te.GetBaseType()), te.IsConst(), te.IsVolatile(), 
			te.GetDerivedTypeList(), sval) );
			
}


// закрытый метод, создает результат
POperand BinaryInterpretator::MakeResult( const BaseType &bt1, const BaseType & bt2, double res )
{
	// выбираем из двух типов наибольший
	BaseType::BT btc1 = bt1.GetBaseTypeCode(), btc2 = bt2.GetBaseTypeCode();
	const BaseType *rbt = NULL;

	// если один из типов double, значит и другой double
	if( btc1 == BaseType::BT_DOUBLE || btc2 == BaseType::BT_DOUBLE )
	{
		bool lng = bt1.IsLong() || bt2.IsLong();
		rbt = &ImplicitTypeManager(KWDOUBLE, -1, lng ? KWLONG : -1).GetImplicitType();
	}

	// если один из типов float, значит и другой
	else if( btc1 == BaseType::BT_FLOAT || btc2 == BaseType::BT_FLOAT )
		rbt = &ImplicitTypeManager(KWFLOAT).GetImplicitType();

	// иначе int
	else
	{
		bool uns = bt1.IsUnsigned() || bt2.IsUnsigned(),
			 lng = bt1.IsLong() || bt2.IsLong();
		rbt = &ImplicitTypeManager(KWINT, uns ? KWUNSIGNED : -1, 
			lng ? KWLONG : -1).GetImplicitType();
	}

	// возвращаем литерал
	CharString sval;
	if( rbt->GetBaseTypeCode() == BaseType::BT_INT )
		sval = (int)res;
	else
		sval = res;

	return new PrimaryOperand( false, *new Literal( 
		const_cast<BaseType *>(rbt), true, false, DerivedTypeList(), sval) );			
}


// интерпретировать. Если интерпретация невозможна вернуть NULL,
// иначе указатель на новый операнд
POperand BinaryInterpretator::Interpretate() const
{
	double val1, val2, res;
	val1 = val2 = res = 0;

	// проверяем операнды на интерпретируемость
	if( !ExpressionMakerUtils::IsInterpretable( cnst1, val1 ) ||
		!ExpressionMakerUtils::IsInterpretable( cnst2, val2 ) )
		return NULL;

	const PrimaryOperand &pop1 = static_cast<const PrimaryOperand &>(*cnst1), 
			&pop2 = static_cast<const PrimaryOperand &>(*cnst2);

	// выполняем семантические проверки
	if( (op == '%' || op == '/') && val2 == 0 )
	{
		theApp.Error(errPos, op == '/' ? "деление на ноль" : "остаток от нуля");
		return cnst1;
	}

	// в этих случаях операнды должны быть целыми
	else if( op == '%' || op == '^' || op == '|' || op == '&' || 
		     op == LEFT_SHIFT || op == RIGHT_SHIFT )
	{
		BaseType::BT bt1 = pop1.GetType().GetBaseType().GetBaseTypeCode(),
					 bt2 = pop2.GetType().GetBaseType().GetBaseTypeCode();

		if( bt1 == BaseType::BT_DOUBLE || bt2 == BaseType::BT_DOUBLE || 
			bt1 == BaseType::BT_FLOAT  || bt2 == BaseType::BT_FLOAT )
		{
			theApp.Error(errPos, "операнды должны быть целого типа");
			return cnst1;
		}
	}

	// теперь проверяем, какой имеем оператор и интерпретируем	
	switch( op )
	{
	case '+': res = val1 + val2; break;
	case '-': res = val1 - val2; break;
	case '*': res = val1 * val2; break;
	case '/': res = val1 / val2; break;
	case '%': res = (int)val1 % (int)val2; break;
	case '<': res = val1 < val2; break;
	case '>': res = val1 > val2; break;
	case EQUAL:			res = val1 == val2; break;
	case NOT_EQUAL:		res = val1 != val2; break;
	case GREATER_EQU:	res = val1 >= val2; break;
	case LESS_EQU:		res = val1 <= val2; break;
	case LOGIC_AND:		res = val1 && val2; break;
	case LOGIC_OR:		res = val1 || val2; break;
	case '^':			res = (int)val1 ^ (int)val2; break;
	case '|':			res = (int)val1 | (int)val2; break;
	case '&':			res = (int)val1 & (int)val2; break;
	case LEFT_SHIFT:	res = (int)val1 << (int)val2; break;
	case RIGHT_SHIFT:	res = (int)val1 >> (int)val2; break;
	case ',':			res = val2; break;
	default:	
		return NULL;
	}

	return MakeResult(pop1.GetType().GetBaseType(), 
		pop2.GetType().GetBaseType(), res);
}


// интерпретировать. Если интерпретация невозможна вернуть NULL,
// иначе указатель на новый операнд
POperand TernaryInterpretator::Interpretate() const
{
	double val1, val2, val3, res;
	val1 = val2 = val3 = res = 0;

	// проверяем операнды на интерпретируемость
	if( !ExpressionMakerUtils::IsInterpretable( cnst1, val1 ) ||
		!ExpressionMakerUtils::IsInterpretable( cnst2, val2 ) ||
		!ExpressionMakerUtils::IsInterpretable( cnst3, val3 ) )
		return NULL;

	// получаем операнды из которых будет строится результирующий тип
	const PrimaryOperand &pop1 = static_cast<const PrimaryOperand &>(*cnst2), 
			&pop2 = static_cast<const PrimaryOperand &>(*cnst3);

	// вычисляем результат
	res = val1 ? val2 : val3;

	// возвращаем его
	return BinaryInterpretator::MakeResult(pop1.GetType().GetBaseType(),
		pop2.GetType().GetBaseType(), res);
}


// создаеть указатель 'this', если возможно.
POperand ThisMaker::Make()
{
	// если текущая область видимости не локальная и не функциональная,
	// вывести ошибку, вернуть 'errorOperand'
	if( !GetCurrentSymbolTable().IsFunctionSymbolTable() &&
		!GetCurrentSymbolTable().IsLocalSymbolTable() )
	{
		theApp.Error(errPos, "'this' в '%s'", 
			ManagerUtils::GetSymbolTableName(GetCurrentSymbolTable()).c_str());
		return ErrorOperand::GetInstance();
	}

	const SymbolTable &fst = GetScopeSystem().GetFunctionalSymbolTable();
	const Function &fn = static_cast<const FunctionSymbolTable &>(fst).GetFunction();

	// если функция не является методом или является статическим методом,
	// использование 'this' некорректно
	if( !fn.IsClassMember() )
	{
		theApp.Error(errPos, "'this' в 'функции-не члене'");
		return ErrorOperand::GetInstance();
	}

	// статический метод
	if( fn.GetStorageSpecifier() == Function::SS_STATIC )
	{
		theApp.Error(errPos, "'this' в 'статическом методе'");
		return ErrorOperand::GetInstance();
	}
		
	// создаем основной операнд. this не является lvalue, а типизированную сущность
	// создадим
	return new PrimaryOperand(false, 
		*MakeThis( static_cast<const Method &>(fn) ) );
}


// выдляет память для 'this'
const TypyziedEntity *ThisMaker::MakeThis( const Method &meth ) const
{
	// получим прототип
	const FunctionPrototype &fp = meth.GetFunctionPrototype();
	
	// получим класс к которому принадлежит метод
	const ClassType &cls = static_cast<const ClassType &>(meth.GetSymbolTableEntry());

	// создадим константный указатель и вставим его в список произв. типов
	DerivedTypeList dtl;
	dtl.AddDerivedType( new Pointer(true, false) );

	// теперь получаем cv-квалификаторы метода
	bool c = fp.IsConst(), v = fp.IsVolatile();

	// создаем
	return new TypyziedEntity( const_cast<ClassType *>(&cls), c, v, dtl );
}


// переводит символьный литерал в целое число
int LiteralMaker::CharToInt( PCSTR chr, bool wide ) const
{
	register PCSTR p;
	extern int isdigit8( int c );
	int r;
	PCSTR end;	// конец константы, должен указывать на '\''

	p = wide ? chr+2 : chr + 1;	// после '\''
	end = p + 1;
	r = *p;

	if(*p == '\\')
	{
		end = p+2;
		if( *(p+1) == 'x' && *(p+2) == '\'')
		{
			theApp.Error(  literalLxm.GetPos(),
					"отсутствует 16-ричная последовательность после '\\x'");
			return 'x';
		}

		if( *(p+1) == 'x' || isdigit8(*(p+1)) )
		{
			int base = *(p+1) == 'x' ? 16 : 8;
			char *stop;
			PCSTR start = base == 16 ? p+2 : p+1;

			r = strtol( start, &stop, base );

			// произошло переполнение
			if( (errno == ERANGE) || 
				(wide ? r > MAX_WCHAR_T_VALUE : r > MAX_CHAR_VALUE) )
			{
				theApp.Warning( literalLxm.GetPos(),
					"'0x%x' - значение слишком велико для типа '%s'",
					r, wide ? "wchar_t" : "char");
				return r;
			}

			if( *stop != '\'' )
			{
				theApp.Error(  literalLxm.GetPos(),
					"'%x' - неизвестный символ в %d-ричной последовательности", 
					*stop, base );
				
				return *(p+1);
			}

			else
				return r;
		}

		else
		switch( *(p + 1) )
		{
		case 'n':  r = '\n'; break;
		case 't' : r = '\t'; break;
		case 'v' : r = '\v'; break;
		case 'b' : r = '\b'; break;
		case 'r' : r = '\r'; break;
		case 'f' : r = '\f'; break;
		case 'a' : r = '\a'; break;
		case '\\': r = '\\'; break;
		case '?' : r = '\?'; break;
		case '\'': r = '\''; break;
		case '\"': r = '\"'; break;
		default:
			theApp.Error(  literalLxm.GetPos(),
				"'\\%c' - некорректная символная последовательность", *(p+1));
			return *(p+1);
		}
	}

	if( *end == '\'' )
		return r;

	else
	{
		if( wide )
		{
			theApp.Warning( literalLxm.GetPos(),
				"символы кроме первого игнорируются в константе 'wchar_t'" ); 
			return r;
		}

		else
		{
			theApp.Error( literalLxm.GetPos(),
				"%s - некорректная символная последовательность", chr);		
			return 0;
		}
	}	
}


// создать PrimaryOperand с типизированной сущностью созданной
// на основе лексемы
POperand LiteralMaker::Make()
{
	// анализируем тип лексемы
	register int lc = literalLxm;
				  			   
	// резулбтирующий литерал
	const Literal *literal = NULL;
	DerivedTypeList dtl;

	// создаем int, float, double литералы, которые не требуют преобразование
	if( lc == INTEGER10 || lc == UINTEGER10 || lc == LFLOAT || lc == LDOUBLE )
	{
		int btype = lc == LFLOAT ? KWFLOAT : (lc == LDOUBLE ? KWDOUBLE : KWINT) ;
		literal = new Literal( 
			const_cast<BaseType*>(
			&ImplicitTypeManager(btype, lc == UINTEGER10 ? KWUNSIGNED : -1).GetImplicitType()),
			true, false, dtl, literalLxm.GetBuf());
	}

	// создаем строку, с типом const char [N]
	else if( lc == STRING )
	{		
		dtl.AddDerivedType( new Array( literalLxm.GetBuf().length()-1 ) );
		literal = new Literal( 
			const_cast<BaseType*>(&ImplicitTypeManager(KWCHAR).GetImplicitType()),
			false, false, dtl, literalLxm.GetBuf());
	}

	// создаем строку с типом const wchar_t [N]
	else if( lc == WSTRING )
	{
		INTERNAL_IF( literalLxm.GetBuf()[0] != 'L' );
		
		dtl.AddDerivedType( new Array( literalLxm.GetBuf().length()-2 ) );
		literal = new Literal( 
			const_cast<BaseType*>(&ImplicitTypeManager(KWWCHAR_T).GetImplicitType()),
			false, false, dtl, literalLxm.GetBuf());
	}
	
	// 16-ричные и восьмиричные константы
	else if( lc == INTEGER16 || lc == UINTEGER16 ||	lc == INTEGER8 || lc == UINTEGER8 )
	{
		int unsign = lc == INTEGER16 || lc == UINTEGER8 ? KWUNSIGNED : -1;
		int base = lc == INTEGER16 || lc == UINTEGER16 ? 16 : 8;

		CharString val( strtol(literalLxm.GetBuf().c_str(), NULL, base) );
		literal = new Literal( 
			const_cast<BaseType*>(
			&ImplicitTypeManager(KWINT, unsign).GetImplicitType()), true, false, dtl, val );
	}

	// символьные литералы
	else if( lc == CHARACTER || lc == WCHARACTER )
	{
		CharString val( CharToInt(literalLxm.GetBuf().c_str(), lc == WCHARACTER) );
		literal = new Literal( 
			const_cast<BaseType*>(
			&ImplicitTypeManager(lc == CHARACTER ? KWCHAR : KWWCHAR_T).GetImplicitType()), 
			true, false, dtl, val );

	}

	// булевые константы
	else if( lc == KWTRUE || lc == KWFALSE )
	{
		CharString val( lc == KWTRUE ? "1" : "0" );
		literal = new Literal( 
			const_cast<BaseType*>(&ImplicitTypeManager(KWBOOL).GetImplicitType()), 
			true, false, dtl, val );
	}

	// иначе ошибка
	else
		INTERNAL( "'LiteralMaker::Make' принимает лексему с неизвестным кодом");
	
	INTERNAL_IF( literal == NULL );		
	return new PrimaryOperand( false, *literal );
}


// получить пакет с идентификатором
IdentifierOperandMaker::IdentifierOperandMaker( const NodePackage &ip,const TypyziedEntity *obj ) 
	: idPkg(ip), errPos( ParserUtils::GetPackagePosition(&ip) ), 
		object(obj), noErrorIfNotFound(false), notFound(false)
{

	INTERNAL_IF( idPkg.GetPackageID() != PC_QUALIFIED_NAME || obj == NULL );

	// если задан объект, его тип должен быть классовым. В противном случае он обнуляется
	INTERNAL_IF( !obj->GetBaseType().IsClassType() );		
	
	// задаем текущую область видимости, только если имя одиночное,
	// иначе производим поиск начиная от текущей области видимости и
	// далее проверяем принадлежность имени к объекту	
	curST = idPkg.GetChildPackageCount() == 1 ?
			&static_cast<const ClassType&>(obj->GetBaseType()) : NULL;		

	name = ParserUtils::PrintPackageTree( &idPkg );	
}


// задается только область видимости, без объекта
IdentifierOperandMaker::IdentifierOperandMaker( const NodePackage &ip, 
		const SymbolTable *cst, bool neinf ) 
	: idPkg(ip), errPos( ParserUtils::GetPackagePosition(&ip) ), 
		object(NULL), curST(cst), noErrorIfNotFound(neinf), notFound(false)
{
	INTERNAL_IF( idPkg.GetPackageID() != PC_QUALIFIED_NAME );
	name = ParserUtils::PrintPackageTree( &idPkg );
}


// создать переменную, для предотвращения вывода ошибок в дальнейшем.
// В случае если имя квалифицированное или является оператором,
// имя не создается
void IdentifierOperandMaker::MakeTempName( const CharString &nam ) const
{
	// это условие не предохраняет от создания операторов приведения,
	// но это не смертельно
	if( !isalpha(nam[nam.size()-1]) && !isdigit(nam[nam.size()-1]) &&
		(nam[nam.size()-1]) != '_' )
		return;

	// если текущая область видимости классовая, не создаем
	// член, т.к. это повлечет много ошибок
	if( GetCurrentSymbolTable().IsClassSymbolTable() )
		return;

	// создаем идентификатор
	::Object *obj = new ::Object(nam, &GetCurrentSymbolTable(),
		(BaseType *)&ImplicitTypeManager(KWINT).GetImplicitType(), 
		false, false, DerivedTypeList(), ::Object::SS_NONE);

	// вставляем в таблицу
	INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol( obj ) ); 

}


// проверить, перекрывает ли 'srcId' идентификатор 'destId'
// из другого класс иерархии. Если destId принадлежит классу
// 'V', который является виртуальным по отношению к curST,
// а srcId принадлежит к классу 'B' базовому классу 'curST',
// и 'B' наследует 'V', то возвращается true
bool IdentifierOperandMaker::HideVirtual( const Identifier *destId, 
										 const Identifier *srcId ) const
{
	INTERNAL_IF( destId == NULL || srcId == NULL || curST == NULL );
	INTERNAL_IF( !curST->IsClassSymbolTable() || 
		!destId->GetSymbolTableEntry().IsClassSymbolTable() ||
		!srcId->GetSymbolTableEntry().IsClassSymbolTable() );

	const ClassType &clsD = *static_cast<const ClassType *>(curST),
					&clsV = static_cast<const ClassType &>(destId->GetSymbolTableEntry()),
					&clsB = static_cast<const ClassType &>(srcId->GetSymbolTableEntry());

	// проверяем, чтобы 'V' виртуальным по отнощению к 'D'
	if( DerivationManager(clsV, clsD).IsVirtual() )
	{
		// проверяем, чтобы 'B' был базовым по отношению к 'D' и
		// 'V' был виртуальным по отношению к 'B'
		DerivationManager dm(clsB, clsD);

		if( !dm.IsBase() || !dm.IsUnambigous() )
			return false;

		// класс 'V' является виртуальным по отношению к 'B'
		if( DerivationManager(clsV, clsB).IsVirtual() )
			return true;
	}

	return false;
}


// ислючает дубликаты из списка, согласно правилам языка. В случае если
// в списке произошла ошибка, неоднозначность или некорректный идентификатор,
// возвращает false
bool IdentifierOperandMaker::ExcludeDuplicates( RoleList &idList ) const
{	
	// проверим, чтобы первый идентификатор не был областью видимостью
	if( idList.front().second == R_NAMESPACE || 
		idList.front().second == R_NAMESPACE_ALIAS )
	{
		theApp.Error( errPos, 
			"'%s' - область видимости не может использваться в выражении",
			name.c_str() );
		return false;
	}

	// если идентификатор один, выходим
	if( idList.size() == 1 )
		return true;	
	

	// проходим по списку, проверяем, идентификаторы которые можно исключить
	// задаем первый идентификатор, на основе которого будут проводиться 
	// остальные проверки
	Identifier *first = idList.front().first;
	register RoleList::iterator p = idList.begin();
	// роль идентификатора
	register Role fr = idList.front().second;

	// перехватываем исключения с типом 'Identifier *'. Они
	// возникают при неоднозначности
	try {

	// начинаем обход списка
	p++;
	while( p != idList.end() )	
	{
		// получаем следующий идентификатор и сравниваем его с первым
		Identifier *next = (*p).first;
		
		// если указатели равны, следует проверить, возможно ли удаление
		// 'p' из списка согласно правилам языка
		if( first == next )
		{
			// если оба идентификатора относятся к не классовой
			// области видимости, исключим next из списка и продолжим
			if( !first->GetSymbolTableEntry().IsClassSymbolTable() )
			{
				p = idList.erase(p);
				continue;
			}

			// иначе оба идентификатора являются членами. Если
			// идентификатор 'first' является статическим, константой
			// перечисления или типом, то next можно исключить из
			// списка
			if( fr == R_DATAMEMBER )
			{				
				// спецификаторы хранения могут быть 'typedef' или 'static'				
				const ::Object &dm = static_cast<const ::Object &>(*first);
				if( dm.GetStorageSpecifier() == ::Object::SS_TYPEDEF ||
					dm.GetStorageSpecifier() == ::Object::SS_STATIC )
					p = idList.erase(p);

				// иначе проверяем, принадлежит ли член классу 'V',
				// который является виртуальным базовым классом, относительно 
				// текущей области видимости, которая должна быть классом
				else if( curST && 
					DerivationManager( 
						static_cast<const ClassType &>(first->GetSymbolTableEntry()),
						static_cast<const ClassType &>(*curST)).IsVirtual() )
					p = idList.erase(p);

				// иначе имеем ошибку
				else
					throw next;
			}

			// если имеем метод
			else if( fr == R_METHOD || fr == R_CLASS_OVERLOAD_OPERATOR )
			{
				const Method &mt = static_cast<const Method &>(*first);
				if( mt.GetStorageSpecifier() == Function::SS_STATIC )
					p = idList.erase(p);

				// если метод принадлежит виртуальному классу
				else if( curST && 
					DerivationManager( 
						static_cast<const ClassType &>(first->GetSymbolTableEntry()),
						static_cast<const ClassType &>(*curST)).IsVirtual() )
					p = idList.erase(p);

				// иначе ошибка
				else
					throw next;

			}

			// константа перечисления не вызывает неоднозначности
			// тип также не вызывает неоднозначности
			else if( fr == R_CLASS_TYPE || fr == R_ENUM_TYPE ||
					 fr == R_UNION_CLASS_TYPE || fr == R_CLASS_ENUM_CONSTANT )
				p = idList.erase(p);

	
			// иначе два объекта, либо два метода наследуются производным
			// классом от нескольких базовых
			else 
				throw next;			
		}

		// иначе указатели не равны, и следует проверить может ли
		// один из идентификаторов перекрывать другой
		else
		{
			// если идентификатор является именованной ОВ, выйти с ошибкой
			if( idList.front().second == R_NAMESPACE || 
				idList.front().second == R_NAMESPACE_ALIAS )
			{
				theApp.Error( errPos, 
					"'%s' - область видимости не может использваться в выражении",
					next->GetQualifiedName().c_str() );
				return false;
			}


			// если иденты. относятся к классовой области видимости,
			// проверим их на перекрытие
			const SymbolTable &st1 = first->GetSymbolTableEntry(),
							  &st2 = next->GetSymbolTableEntry();

			// получить роль идентификатора 'next'
			register Role nr = (*p).second;

			// если идентификаторы 'next' и 'first' из одной области видимости
			bool stEq = &st1 == &st2;


			// оба классовые и разные, проверим виртуальное перекрытие
			if( st1.IsClassSymbolTable() && st2.IsClassSymbolTable() && !stEq )
			{
				// если 'next' перекрывает 'first', который принадлежит виртуальному
				// классу, то erase(first), first=next, 
				if( HideVirtual( first, next ) )
				{
					idList.erase(idList.begin());
					first = next;
					p++;
					fr = nr;
					continue;
				}

				// иначе проверим, если first, перекрывает 'next', тогда next
				// удаляется
				else if( HideVirtual( next, first ) )				
				{
					p = idList.erase(p);				
					continue;
				}
			}

			// перекрытия не было проверяем, если next является типом
			if( nr == R_CLASS_TYPE || nr == R_ENUM_TYPE || nr == R_UNION_CLASS_TYPE )
			{
				// если разные области видимости, это неоднозначность
				if( !stEq )
					throw next;

				// иначе fr, должна быть типизированной сущностью
				INTERNAL_IF( !dynamic_cast<const TypyziedEntity *>(first) );
				p = idList.erase(p);
			}

			// если 'next' является ф-ей (методом, оператором)
			else if( nr == R_FUNCTION		   || nr == R_METHOD ||
					 nr == R_OVERLOAD_OPERATOR || nr == R_CLASS_OVERLOAD_OPERATOR ||
					 nr == R_CONSTRUCTOR )
			{
				// если разные области видимости, то приемлимо только если
				// fr будет перегруженной функцией и область видимости не класс
				if( !stEq )
				{
					if( nr != fr || next->GetSymbolTableEntry().IsClassSymbolTable() )
						throw next;					
				}

				else
				{
					// области видимости одинаковые, если 'fr' является типом,
					// перекрываем его функцией
					if( fr == R_CLASS_TYPE || fr == R_ENUM_TYPE || fr == R_UNION_CLASS_TYPE )
						idList.erase(idList.begin()), first = next, fr = nr;

					// иначе возможна только перегруженная функция
					else if( fr != nr )
						throw next;
				}

				p++;
			}

			// если 'next' является объектом (константой)
			else if( nr == R_OBJECT    || nr == R_DATAMEMBER    ||
					 nr == R_PARAMETR  || nr == R_ENUM_CONSTANT ||
					 nr == R_CLASS_ENUM_CONSTANT )
			{
				// если область видимости одинаковая, то возможно
				// перекрытие только типа
				if( stEq )
				{
					if( fr == R_CLASS_TYPE || fr == R_ENUM_TYPE || fr == R_UNION_CLASS_TYPE )
						idList.erase(idList.begin()), first = next, fr = nr, p++;
					else
						throw next;
				}

				// иначе неоднозначность
				else
					throw next;
			}

			// иначе внутренняя ошибка, т.к. семантическое значение неизвестно
			else
				INTERNAL( "'IdentifierOperandMaker::ExcludeDuplicates' принимает"
					" идентификатор неизвестного типа" );

		}	// else

	}	// for 
	
	// перехватываем исключение сгенерированное при неоднозначности
	} catch( const Identifier *id ) {
		
		// выводим ошибку и выходим
		theApp.Error( errPos, "неоднозначность между '%s' и '%s'",
			first->GetQualifiedName().c_str(), id->GetQualifiedName().c_str());
		return false;
	}

	return true;
}


// создать операнд на основании преобразованного списка
POperand IdentifierOperandMaker::MakeOperand( const QualifiedNameManager &qnm ) const
{
	const RoleList &idList = qnm.GetRoleList();
	
	INTERNAL_IF( idList.empty() );
	Role fr = idList.front().second;
	POperand rval = NULL;

	// в этом случае создаем объект или тип
	if( idList.size() == 1 )
	{
		Identifier *id = idList.front().first;

		// если идентификатор является типом, создаем TypeOperand,
		// иначе PrimaryOperand
		if( fr == R_CLASS_TYPE || fr == R_ENUM_TYPE || fr == R_UNION_CLASS_TYPE )
		{
			TypyziedEntity *te = new TypyziedEntity(dynamic_cast<BaseType *>(id),
				false, false, DerivedTypeList() );

			rval = new TypeOperand(*te);
		}

		// иначе идентификатор является типизированной сущностью
		else
		{
			// если идентификатор не является типизированной сущностью,
			// это не предусмотренное поведение (класс, ИОВ)
			TypyziedEntity *te = dynamic_cast<TypyziedEntity *>(id);
			INTERNAL_IF( te == NULL );

			// если идентификатор является объектом и у объекта спецификатор
			// хранения - typedef, создаем операнд тип. 
			if( te->IsObject() )
			{
				const Object &member = static_cast<const ::Object&>(*te);
				
				// если имеем typedef, создаем типовой операнд
				if( member.GetStorageSpecifier() == ::Object::SS_TYPEDEF )
					rval = new TypeOperand(*new TypyziedEntity(member));
				
				// иначе если не статический и не mutable, присваиваем квалификаицю
				else if( !( member.GetStorageSpecifier() == ::Object::SS_STATIC ||
					member.GetStorageSpecifier() == ::Object::SS_MUTABLE) )
				{
					bool c = false, v = false;

					// если задан объект, передаем квалификацию объекта члену
					if( object != NULL )
						c = object->IsConst(), v = object->IsVolatile();

					// иначе если имеем член и идет обращение через this,
					// передаем квалификацию this
					else if( member.IsClassMember() &&
						(GetScopeSystem().GetCurrentSymbolTable()->IsFunctionSymbolTable() ||
						 GetScopeSystem().GetCurrentSymbolTable()->IsLocalSymbolTable()) )
					{
						const Function &curFn = 
							GetScopeSystem().GetFunctionSymbolTable()->GetFunction();
							;
						if( curFn.IsClassMember() &&
							curFn.GetStorageSpecifier() != Function::SS_STATIC )
							c = curFn.GetFunctionPrototype().IsConst(),
							v = curFn.GetFunctionPrototype().IsVolatile();
					}

					// создаем более квалифицированный объект
					if( c > member.IsConst() || v > member.IsVolatile() )
						rval = new PrimaryOperand(true, 
							*new DynamicTypyziedEntity(te->GetBaseType(),
								c, v, te->GetDerivedTypeList(), *te));					
				}
			}
			
			// иначе создаем обычный идентификатор			
			if( rval.IsNull() )	
			{
				// если имеем метод, он должен быть доступен
				if( te->IsFunction() && static_cast<const Function *>(te)->IsClassMember() &&
					static_cast<const Method *>(te)->IsUnavaible() )
					theApp.Error(errPos, 
						"'%s' - метод сгенерирован некорректно", 
						te->GetTypyziedEntityName().c_str());
				rval = new PrimaryOperand(true, *te);
			}
		}

		// проверяем доступность
		if( object )
		{		
			const ClassMember *cm = dynamic_cast<const ClassMember *>(id);
			if( cm && cm->GetAccessSpecifier() != ClassMember::NOT_CLASS_MEMBER )
			{
				AccessControlChecker acc( 
					GetCurrentSymbolTable().IsLocalSymbolTable() ? 
					GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable(),
					static_cast<const ClassType&>(object->GetBaseType()), *cm);

				if( !acc.IsAccessible() )
					theApp.Error( errPos, "'%s' - член недоступен в '%s'",
						id->GetQualifiedName().c_str(), 
						ManagerUtils::GetSymbolTableName(GetCurrentSymbolTable()).c_str());			
			}
		}

		else
			CheckerUtils::CheckAccess( qnm, *id, errPos, 
				curST  ? *curST : GetCurrentSymbolTable());
	}

	// иначе идентификаторов несколько, это должен быть список
	// перегруженных функций, проверим это еще раз
	else
	{
		//
		Identifier *id = idList.front().first;

		// проверяем, чтобы роль была функцией
		if( fr == R_FUNCTION		  || fr == R_METHOD ||
			fr == R_OVERLOAD_OPERATOR || fr == R_CLASS_OVERLOAD_OPERATOR ||
			fr == R_CONSTRUCTOR )
		{
			OverloadFunctionList ofl;

			// загружаем в список функции, предварительно проверив
			// что роли совпадают
			for( RoleList::const_iterator p = idList.begin(); p != idList.end(); p++ )
			{
				// роли должны совпадать
				if( (*p).second != fr )
					goto err;
				ofl.push_back( static_cast<Function *>((*p).first) );
			}

			// создаем операнд - список перегруженных функций
			rval = new OverloadOperand( ofl );
		}

		// иначе неоднозначность
		else
		{
		err:
			theApp.Error( errPos, "неоднозначность между '%s' и '%s'",
				id->GetQualifiedName().c_str(),
				idList.front().first->GetQualifiedName().c_str());					

			return ErrorOperand::GetInstance();
		}		
	}

	return rval;
}


// создать операнд. Предварительно проверив идентификатор
// на существование, доступность и однозначность.
POperand IdentifierOperandMaker::Make()
{
	// в первую очередь ищем имя, третий параметр задает флаг не-декларации
	QualifiedNameManager qnm( &idPkg, curST );

	// если ничего не найдено, выводим ошибку и возвращаем ErrorOperand.
	// есть смысл создания идентификатора для предотвращения
	// вывода ошибок в последующем 
	if( qnm.GetRoleCount() == 0 )
	{
		// при поиске перегруженного оператора, ошибку выводить не следует
		if( !noErrorIfNotFound )
		{
			if( object != NULL )
				theApp.Error(errPos, 
					"'%s' - не является членом класса '%s'",
					name.c_str(),
				static_cast<const ClassType&>(object->GetBaseType()).GetQualifiedName().c_str());
			else
				theApp.Error( errPos, "'%s' - не объявлен", name.c_str());

			// если имя не является квалифицированным, создаем
			// временную переменную типа int, для предотвращения
			// вывоа ошибок в дальнейшем
			if( name.find(':') == string::npos )
				MakeTempName(name);
		}

		// задаем флаг, что ничего не найдено
		notFound = true;
		return ErrorOperand::GetInstance();
	}


	// перед преобразованием списка, следует задать классовую
	// область видимости, т.к. это имеет смысл при преобразовании.
	// Если текущая область видимости не метод и не классовая,
	// curST так и остается равен нулю
	const SymbolTable *st = curST;
	if( curST == NULL )
	{		
		// локальную может содержать метод, который содержит класс
		const SymbolTable &ct = GetCurrentSymbolTable();
		if( ct.IsLocalSymbolTable() )
		{
			const Function *fn = &static_cast<const FunctionSymbolTable &>(
				GetScopeSystem().GetFunctionalSymbolTable()).GetFunction();
			if( fn->IsClassMember() )
				curST = &static_cast<const ClassType &>(fn->GetSymbolTableEntry());
		}

		// если функция, может быть метод
		else if( ct.IsFunctionSymbolTable() )
		{
			const Function *fn = &static_cast<FunctionSymbolTable &>(
				GetCurrentSymbolTable()).GetFunction();

			if( fn->IsClassMember() )
				curST = &static_cast<const ClassType &>(fn->GetSymbolTableEntry());	
		}

		// если класс
		else if( ct.IsClassSymbolTable() )
			curST = &GetCurrentSymbolTable();
	}


	// преобразуем список исключая из него повторяющиеся элементы,
	// которые согласно правилам языка не могут быть однозначными
	if( !ExcludeDuplicates( const_cast<RoleList &>(qnm.GetRoleList()) ) )
		return ErrorOperand::GetInstance();

	// восстанавливаем значение текущей области видимости
	curST = st;

	// после того как список преобразован и из него исключены дубликаты,
	// следует создать операнд. 
	return MakeOperand( qnm );
}


// процедура сбрасывает квалификацию у выражения и возвращает новый тип
PTypyziedEntity TypeCastBinaryMaker::UnsetQualification()
{
	const TypyziedEntity &et = right->GetType();
	DerivedTypeList rdtl;

	// копируем список производных типов
	for( int i = 0; i<et.GetDerivedTypeList().GetDerivedTypeCount(); i++ )
	{
		const DerivedType &dt = *et.GetDerivedTypeList().GetDerivedType(i);

		// если указатель, копируем без квалиф.
		if( dt.GetDerivedTypeCode() == DerivedType::DT_POINTER )
			rdtl.AddDerivedType( new Pointer(false,false) );

		// также с ук-лем на член
		else if( dt.GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
			rdtl.AddDerivedType( new PointerToMember( 
				&static_cast<const PointerToMember&>(dt).GetMemberClassType(),
				false,false) );

		// иначе копируем как есть
		else
			rdtl.AddDerivedType(et.GetDerivedTypeList().GetDerivedType(i));
	}

	// создаем тип
	return new TypyziedEntity( (BaseType*)&et.GetBaseType(), false, false, rdtl );
}


// проверить приведение с помощью static_cast. Вернуть 0 в случае успеха,
// вернуть 1 в случае успеха и физическое преобразование не требуется, 
// -1 - в случае неудачи, -2 в случае если приведение нужно прекратить,
// вернуть 
int TypeCastBinaryMaker::CheckStaticCastConversion( const TypyziedEntity &expType )
{
	// с самого начала проверяем, если оба операнда указатели на класс 
	// выполняем преобразование, т.к. менеджер проверяет,
	// чтобы класс был доступным, а нам это не нужно
	const TypyziedEntity &toType = left->GetType(), &fromType = expType;

	// нельзя приводить к типу массива или функции
	if( toType.GetDerivedTypeList().IsArray() || toType.GetDerivedTypeList().IsFunction() ) 
		return -2;
	
	if( toType.GetBaseType().IsClassType()						&&
		toType.GetDerivedTypeList().IsPointer()					&& 
		toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
	{
		// проверяем, чтобы и второй тип был указателем на класс
		if( fromType.GetBaseType().IsClassType() &&
			((fromType.GetDerivedTypeList().IsPointer()					&& 
			  fromType.GetDerivedTypeList().GetDerivedTypeCount() == 1)     ||
			 (fromType.GetDerivedTypeList().GetDerivedTypeCount() == 2	&&
			  fromType.GetDerivedTypeList().IsReference()				&&
			  fromType.GetDerivedTypeList().GetDerivedType(1)->GetDerivedTypeCode() ==
			  DerivedType::DT_POINTER))
		   )
		{
			// проверяем, является ли один из классом базовым или производным
			const ClassType &toCls = static_cast<const ClassType&>(toType.GetBaseType()),
				&fromCls = static_cast<const ClassType&>(fromType.GetBaseType());

			// сначала пробуем преобразовать из производного в базовый
			DerivationManager dtbm(toCls, fromCls);			
			if( dtbm.IsBase() )
			{
				// проверим только, чтобы класс был однозначным
				if( !dtbm.IsUnambigous() )
				{
					theApp.Error(errPos, "'(тип)' - базовый класс '%s' неоднозначен",
						toCls.GetQualifiedName().c_str());
					const_cast<POperand&>(right) = ErrorOperand::GetInstance();
					return 1;		// возвращаем 1, чтобы не выводить доп. ошибок
				}

				// иначе строим преобразование
				const_cast<POperand&>(right) = 
					new BinaryExpression( GOC_DERIVED_TO_BASE_CONVERSION, 
						false, left, right, new TypyziedEntity(left->GetType()) );
				return 1;

			}

			// иначе пытаемся из базового в производный
			DerivationManager btdm(fromCls, toCls);
			if( btdm.IsBase() )
			{
				// проверим только, чтобы класс был однозначным
				if( !btdm.IsUnambigous() || btdm.IsVirtual() )
				{
					theApp.Error(errPos, 
						"'(тип)' - базовый класс '%s' %s",
						fromCls.GetQualifiedName().c_str(), 
						btdm.IsVirtual() ? "является виртуальным" : "неоднозначен" );
					const_cast<POperand&>(right) = ErrorOperand::GetInstance();
					return 1;		// возвращаем 1, чтобы не выводить доп. ошибок
				}

				// иначе строим преобразование
				const_cast<POperand&>(right) = 
					new BinaryExpression( GOC_BASE_TO_DERIVED_CONVERSION, 
						false, left, right, new TypyziedEntity(left->GetType()) );
				return 1;
			}
		}		
	}

	// сначала пробуем, преобразовать с помощью автоматического преобразования,
	// вида "T t(e)"
	PCaster pCaster= 
		AutoCastManager(left->GetType(), expType, true, true).RevealCaster();
	Caster &caster = *pCaster;
	caster.ClassifyCast();

	// если преобразование возможно, преобразуем операнды физически и
	// возвращаем 1
	if( caster.IsConverted() )
	{
		caster.DoCast(left, const_cast<POperand&>(right), errPos);
		return 1;
	}

	// иначе пытаемся преобразовать согласно другим правилам
	// возможно преобразование в void	
	if( toType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		toType.GetDerivedTypeList().IsEmpty() )
		return 0;

	// из lvalue 'B', в 'cv D &'
	if( ExpressionMakerUtils::IsClassType(toType) && 
		ExpressionMakerUtils::IsClassType(fromType) &&
		toType.GetDerivedTypeList().IsReference()   &&
		toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
	{		
		if( !ExpressionMakerUtils::IsLValue(right) )
			return -2;

		// проверяем, чтобы 'B' был однозначным не виртуальным базовым классом 'D'
		DerivationManager dm( static_cast<const ClassType &>(fromType.GetBaseType()),
			static_cast<const ClassType &>(toType.GetBaseType()) );
		
		// проверяем, чтобы был базовым, не виртуальным, однозначным,
		if( !dm.IsBase() || dm.IsVirtual() || !dm.IsUnambigous() )
			return -2;

		// сразу строим выражение, преобразование в производный класс
		const_cast<POperand&>(right) = 
			new BinaryExpression( GOC_BASE_TO_DERIVED_CONVERSION, 
				false, left, right, new TypyziedEntity(left->GetType()) );
			
		// иначе возвращаем 1, т.к. выражение уже построено
		return 1;
	}

	// если оба типа целых, целый тип можно преобразовать в перечислимый
	if( ExpressionMakerUtils::IsIntegral(toType) && ExpressionMakerUtils::IsIntegral(fromType) )
		return 0;

	// указатель на void, можно преобразовать в указатель на др. тип
	if( fromType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		fromType.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		fromType.GetDerivedTypeList().IsPointer() ) 
	{
		if( toType.GetDerivedTypeList().IsPointer() &&
			toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
			return 0;
	}

	// иначе неудача, но будем пытаться через reinterpret_cast
	return -1;
}

 
// проверить приведение с помощью reinterpret_cast. Вернуть true в случае успеха	
bool TypeCastBinaryMaker::CheckReinterpretCastConversion( const TypyziedEntity &expType )
{
	const TypyziedEntity &fromType = expType, &toType = left->GetType();
	const DerivedTypeList &fromDtl = fromType.GetDerivedTypeList(), 
						  &toDtl = toType.GetDerivedTypeList();
	
	// нельзя приводить к типу массива или функции
	if( toDtl.IsArray() || toDtl.IsFunction() ) 
		return false;
	
	// Если один тип целый, а второй указатель или наоборот, либо
	// оба указатели
	if( (ExpressionMakerUtils::IsIntegral(fromType) &&
		 ExpressionMakerUtils::IsRvaluePointer(toType))     ||
		(ExpressionMakerUtils::IsIntegral(toType) &&
		 ExpressionMakerUtils::IsRvaluePointer(fromType))   ||
		(ExpressionMakerUtils::IsRvaluePointer(fromType) &&
		 ExpressionMakerUtils::IsRvaluePointer(toType))	)
		 ;

	// иначе если оба указатели на член, проверяем, чтобы оба были
	// объекты или функции
	else if( toDtl.IsPointerToMember() && fromDtl.IsPointerToMember() &&
			 ((toDtl.GetDerivedTypeCount() > 1 && toDtl.GetDerivedType(1)->
			   GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) +
			  (fromDtl.GetDerivedTypeCount() > 1 && fromDtl.GetDerivedType(1)->
			   GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE)) != 1 )
		;

	// Если правый операнд lvalue, а конечный тип - ссылка,
	// преобразование возможно.
	else if( toType.GetDerivedTypeList().IsReference() &&
		((right->IsExpressionOperand() && static_cast<const Expression&>(*right).IsLvalue()) ||
		 (right->IsPrimaryOperand() && static_cast<const PrimaryOperand&>(*right).IsLvalue())) )
		;

	else
		return false;

	return true;
}


// создать выражение (тип)
POperand TypeCastBinaryMaker::Make()
{
	// сначала снимаем квалификацию с типа
	PTypyziedEntity unqual = UnsetQualification();
	
	// далее пытаемся преобразовать с помощью static_cast
	int r = CheckStaticCastConversion(*unqual);
	if( r == 1 )
		return right;

	// если 0, строим выражение
	else if( r == 0 || ( r == -1 && CheckReinterpretCastConversion(*unqual)) )		
		return new BinaryExpression( OC_CAST, left->GetType().GetDerivedTypeList().IsReference(),
			left, right, new TypyziedEntity(left->GetType()));
	
	// в случае, если оба преобразования невозможны, выводим ошибку
	theApp.Error(errPos, "'(тип)' - невозможно преобразовать '%s' к '%s'",
		right->GetType().GetTypyziedEntityName(false).c_str(),
		left->GetType().GetTypyziedEntityName(false).c_str());
	return ErrorOperand::GetInstance();
}


// метод, выявляет наилучшую функцию из списка и проверяет ее доступность.
// Если функция неоднозначна или не найдена, вернуть NULL
const Function *FunctionCallBinaryMaker::CheckBestViableFunction( 
	const OverloadFunctionList &ofl, const ExpressionList &pl, const TypyziedEntity *obj ) const
{
	OverloadResolutor or(ofl, pl, obj);
	const Function *fn = or.GetCallableFunction();
	
	// если функция не найдена, выведем ошибку, если вызов неявный.
	if( fn == NULL )
	{		
		// если функция не неоднозначна, значит ее не существует
		if( !or.IsAmbigous() )
			noFunction = true;

		// если вызов явный или неоднозначность, вывести ошибку
		if( !implicit || !noFunction )
			theApp.Error(errPos, or.GetErrorMessage().c_str());

		// выходим
		return NULL;
	}

	// иначе проверяем доступность этой функции
	if( fn->IsClassMember() )
	{
		AccessControlChecker acc( 
			GetCurrentSymbolTable().IsLocalSymbolTable() ? 
			GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable(),
			obj ? static_cast<const ClassType&>(obj->GetBaseType())
				: static_cast<const ClassType&>(fn->GetSymbolTableEntry()), *fn);

		if( !acc.IsAccessible() )
			theApp.Error( errPos, 
				"'%s' - метод недоступен; вызов невозможен", 
				fn->GetTypyziedEntityName().c_str() );		
	}

	// в конце выполняем преобразование каждого параметр в целевой тип
	or.DoParametrListCast(errPos); 
	return fn;
}


// если 'fn' - это объект, выявить для него перегруженный оператора '()',
// либо оператор приведения к типу указатель на функцию. Если приведение
// удалось, вернет новый операнд, иначе NULL
POperand FunctionCallBinaryMaker::FoundFunctor( const ClassType &cls ) const
{
	// ищем оператор
	OverloadOperatorFounder oof(OC_FUNCTION, false, &cls, errPos);
	// если перегруженный оператор найден, рекурсивно вызываем строитель функции. 
	if( oof.IsFound() )
	{
		// если неоднозначен, вернуть ошибку
		if( oof.IsAmbigous() )
			return ErrorOperand::GetInstance();

		// иначе рекурсивно вызываем строителя функции для списка
		// операторов и параметров
		return FunctionCallBinaryMaker(
			new BinaryExpression('.', false, fn, new OverloadOperand(oof.GetClassOperatorList()),
			  new TypyziedEntity(fn->GetType()) ),
			  parametrList, OC_FUNCTION, errPos, false).Make();
	}
	
	// иначе возвращаем NULL, как показатель, что оператор не найден
	else
		return NULL;
}

// проверяем возможность копирования и уничтожения каждого параметра.
// Требуется чтобы конструктор копирования и деструктор были доступны
void FunctionCallBinaryMaker::CheckParametrInitialization( const PExpressionList &parametrList, 
	const FunctionParametrList &formalList, const Position &errPos ) 
{
	// проверяем исключительно классовые параметры, при передачи параметров
	// которым требуется вызов конструктора. Проверяем, чтобы это был
	// полностью объявленный класс и доступны конструктор копирования и деструктор.
	// Те параметры, которые уже преобразовались с помощью 
	// конструктор (явно или неявно), проверять не нужно
	int pcnt = parametrList->size() < formalList.GetFunctionParametrCount() ?
		parametrList->size() : formalList.GetFunctionParametrCount();

	for( int i = 0; i<pcnt; i++ )
	{
		const Parametr &prm = *formalList[i];
		if( !(prm.GetBaseType().IsClassType() && prm.GetDerivedTypeList().IsEmpty()) )
			continue;

		// имеем классовый параметр, 
		const ClassType &cls = static_cast<const ClassType&>(prm.GetBaseType());
		
		// проверяем, чтобы класс был полностью объявлен
		if( cls.IsUncomplete() )
		{
			theApp.Error( errPos, 
				"'%i-ый' параметр имеет тип '%s', который не определен",
				i+1, cls.GetName().c_str());
			continue;
		}

		// проверяем не было ли уже преобразование
		const POperand &exp = (*parametrList)[i];
		if( exp->IsExpressionOperand() &&
			static_cast<const Expression&>(*exp).IsFunctionCall() )
		{
			const FunctionCallExpression &fnc = static_cast<const FunctionCallExpression&>(*exp);
			if( &fnc.GetFunctionOperand()->GetType().GetBaseType() == &cls	&&
				dynamic_cast<const ConstructorMethod *>(
					&fnc.GetFunctionOperand()->GetType()) != NULL )
				continue;
		}

		// в противном случае, проверяем, чтобы у объекта был доступен к-тор копирования
		// и деструктор
		ExpressionMakerUtils::ObjectCreationIsAccessible(cls, errPos, false, true, true);
	}
}


// проверить является ли функция членом текущего класса, если она
// вызывается напрямую без this, и совпадает ли ее квалификация с объектом
void FunctionCallBinaryMaker::CheckMemberCall( const Method &m ) const
{
	// статический метод или конструктор можно вызывать без 'this'
	if( m.GetStorageSpecifier() == Function::SS_STATIC ||
		m.IsConstructor() )
		return;

	// получаем класс метода и проверяем какое отношение имеет 
	// текущая область видимости к этому классу
	const ClassType &mcls = static_cast<const ClassType &>(m.GetSymbolTableEntry());
	const SymbolTable &st = GetCurrentSymbolTable().IsLocalSymbolTable() ?
		GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable();

	// проверяем, если функциональная, то можно продолжать проверку
	if( st.IsFunctionSymbolTable() &&
		static_cast<const FunctionSymbolTable&>(st).GetFunction().IsClassMember() )
	{
		// получаем класс, к которому принадлежит метод в котором мы находимся,
		// класс должен либо совпадать с классом вызываемого метода, либо
		// быть производным для него
		const Method &curMeth = static_cast<const Method &>(
			static_cast<const FunctionSymbolTable&>(st).GetFunction());
		const ClassType &cc = static_cast<const ClassType &>(curMeth.GetSymbolTableEntry());
		if( &cc == &mcls ||
			DerivationManager( mcls, cc ).IsBase() )
		{
			// проверяем квалификаицю, чтобы метод вызывался в более 
			// квалифицированном методе
			if( curMeth.GetFunctionPrototype().IsConst() > m.GetFunctionPrototype().IsConst() ||
				curMeth.GetFunctionPrototype().IsVolatile() > 
						m.GetFunctionPrototype().IsVolatile() )
				theApp.Error( errPos,
					"'%s' - метод менее квалифицирован, чем this; вызов невозможен",
					m.GetQualifiedName().c_str() );
			return;
		}		
	}

	// если доходим до сюда, значит выводим ошибку
	theApp.Error(errPos, 
		"'%s' - метод не может вызываться в текущей области видимости; отсутствует 'this'",
		m.GetQualifiedName().c_str());
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand FunctionCallBinaryMaker::Make()
{
	// выполнить стандартные проверки перед вызовом функции. Есть
	// ли ошибочные параметры или функция, поиск операторов вызова функции,
	// если слева не функция, а объект
	if( fn->IsErrorOperand() )
		return fn;

	// проверим, есть ли ошибочные параметры или параметры типа,
	// а также если есть параметры - перегруженные функции, разрешить
	// неоднозначность
	for( int i = 0; i<parametrList->size(); i++ )	
		if( (*parametrList)[i]->IsErrorOperand() )
			return ErrorOperand::GetInstance();

	// если функция является основным операндом или выражением
	if( fn->IsPrimaryOperand() || fn->IsExpressionOperand() )
	{
		// имеем дело, либо с объектом, либо с функциональным типом,
		// но для начала проверим, если выражение и оператор выбора,
		// то второй операнд может быть списком перегруженных функций
		// который надо разрешить
		if( fn->IsExpressionOperand() )
		{
			const Expression &exp = static_cast<const Expression &>(*fn);
			if( (exp.GetOperatorCode() == '.' || exp.GetOperatorCode() == ARROW) &&
				 static_cast<const BinaryExpression &>(exp).GetOperand2()->IsOverloadOperand() )
			{
				const Function *vf = CheckBestViableFunction(
				   static_cast<const OverloadOperand &>(
					*static_cast<const BinaryExpression &>(exp).GetOperand2()).GetOverloadList(),
					*parametrList, 
					&static_cast<const BinaryExpression &>(exp).GetOperand1()->GetType()
				);

				// если функция не найдена, выходим
				if( vf == NULL )
				{
					if( implicit && noFunction )
						return NULL;
					return ErrorOperand::GetInstance();
				}

				// найдена однозначная функция, проверим возможность копирования параметров,				
				CheckParametrInitialization(vf->GetFunctionPrototype().GetParametrList());
					
				// иначе строим новое выражение с конкретной функцией				
				const_cast<POperand&>(fn) = new BinaryExpression(exp.GetOperatorCode(), false,
					static_cast<const BinaryExpression &>(exp).GetOperand1(), 
					new PrimaryOperand(false, *vf), new TypyziedEntity(*vf));

				// строим вызов функции
				PTypyziedEntity rtype = new TypyziedEntity(*vf);
				const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
				return new FunctionCallExpression( vf->GetDerivedTypeList().IsReference(),
					fn, parametrList, rtype);
			}			
		}
		
		// в противном случае операнд однозначен. Сначала проверяем, если
		// имеет классовый тип, пытаемся построить вызов перегруженного оператора '()'
		if( ExpressionMakerUtils::IsClassType(fn->GetType()) )
		{
			// ищем перегруженный оператор ()
			POperand p = 
				FoundFunctor(static_cast<const ClassType&>(fn->GetType().GetBaseType()));

			// если не найден, выведем ошибку
			if( p.IsNull() )
			{
				if( fn->IsPrimaryOperand() )
					theApp.Error(errPos, "'%s' - объект не является функцией",
						ExpressionPrinter(fn).GetExpressionString().c_str());
				else
					theApp.Error( errPos, 
						"'%s' - класс не содержит перегруженного оператора '()'",
						static_cast<const ClassType&>(fn->GetType().GetBaseType()).
							GetQualifiedName().c_str());
				return ErrorOperand::GetInstance();
			}

			// иначе вернем операнд
			return p;
		}

		// иначе операнд должен быть функцией, указателем на функцию или 
		// указателем на член-функцию		
		const DerivedTypeList &dtl = fn->GetType().GetDerivedTypeList();
		if( dtl.IsFunction() || 
			(dtl.GetDerivedTypeCount() > 1 && dtl.IsPointer() &&
			 dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE ))
		{			
			bool isFunction = dynamic_cast<const Function *>(&fn->GetType()) != NULL;
			const FunctionPrototype &fpt = dtl.IsFunction() ?
				static_cast<const FunctionPrototype &>(*dtl.GetDerivedType(0)) :
				static_cast<const FunctionPrototype &>(*dtl.GetDerivedType(1)) ;
		
			// имеем функциональный тип, проверяем типы параметров,
			OverloadFunctionList ofl;			
			const SmartPtr<Function> tempf = isFunction ? NULL :
			(	!dtl.IsFunction() ? ((DerivedTypeList&)dtl).PopHeadDerivedType() : (void)0,
				new Function( "", &GetCurrentSymbolTable(), 
					(BaseType *)&fn->GetType().GetBaseType(), false, false,
					fn->GetType().GetDerivedTypeList(), false, Function::SS_NONE,
					Function::CC_NON )
			 );
			ofl.push_back( tempf.IsNull() ?
				static_cast<const Function*>(&fn->GetType()) : &*tempf );
					
			// выполняем проверку соотв. параметров функции
			const TypyziedEntity *obj = NULL;
			int opc = ((Expression&)*fn).GetOperatorCode();
			if( fn->IsExpressionOperand() && 
				(opc == '.' || opc == ARROW || opc == DOT_POINT || opc == ARROW_POINT) )
				obj = &((BinaryExpression&)*fn).GetOperand1()->GetType();
			
			OverloadResolutor or( ofl, *parametrList, obj );
			if( or.GetCallableFunction() == NULL )
			{
				if( implicit && noFunction )
					return NULL;

				theApp.Error(errPos, or.GetErrorMessage().c_str());
				return ErrorOperand::GetInstance();
			}

			// приводим параметры физически
			or.DoParametrListCast(errPos);
			
			// после того как типы параметров совпали, проверяем возможность их
			// копирования			
			CheckParametrInitialization(fpt.GetParametrList());
		
			// если имеем функцию-член, следует проверить, множно ли ее
			// вызывать напрямую без 'this' в текущей области видимости
			if( fn->IsPrimaryOperand()						 &&	
				tempf.IsNull()								 && 
				static_cast<const Function &>(fn->GetType()).IsClassMember() )
				CheckMemberCall( static_cast<const Method &>(fn->GetType()) );
			
			// создаем результирующий тип
			PTypyziedEntity rtype = new TypyziedEntity(fn->GetType());
			const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();

			// возвращаем вызов
			return new FunctionCallExpression( rtype->GetDerivedTypeList().IsReference(),
					fn, parametrList, rtype );
		}

		// иначе тип не является функцией, вызов невозможен
		else
		{
			if( fn->IsPrimaryOperand() )
				theApp.Error(errPos, "'%s' - операнд не является функцией",
					ExpressionPrinter(fn).GetExpressionString().c_str());
			else
				theApp.Error(errPos, "'%s' - не функциональный тип",
					fn->GetType().GetTypyziedEntityName().c_str());
			return ErrorOperand::GetInstance();
		}
			
	}

	// если функция является перегруженным операндом
	else if( fn->IsOverloadOperand() )
	{
		// сначала проверим, если вызов через this, строим выражение обращения 
		// и рекурсивно вызываем строителя
		const SymbolTable &st = GetCurrentSymbolTable().IsLocalSymbolTable() ?
			GetScopeSystem().GetFunctionalSymbolTable() : GetCurrentSymbolTable();
	
		// выявляем наилучшую
		const Function *vf = CheckBestViableFunction(
			static_cast<const OverloadOperand &>(*fn).GetOverloadList(),*parametrList, NULL);
					
		// если функция не найдена, выходим
		if( vf == NULL )
		{
			if( implicit && noFunction )
				return NULL;
			return ErrorOperand::GetInstance();
		}

		// найдена однозначная функция, проверим возможность копирования параметров,				
		CheckParametrInitialization(vf->GetFunctionPrototype().GetParametrList());

		// если имеем функцию-член, следует проверить, множно ли ее
		// вызывать напрямую без 'this' в текущей области видимости
		if(	vf->IsClassMember() )
			CheckMemberCall( static_cast<const Method&>(*vf) );

		// строим новое выражение с конкретной функцией										
		PTypyziedEntity rtype = new TypyziedEntity(*vf);
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
		return new FunctionCallExpression( vf->GetDerivedTypeList().IsReference(),
			new PrimaryOperand(false, *vf), parametrList, rtype);
	}
	
	// если функциональное приведение типа
	else if( fn->IsTypeOperand() ) 
	{
		// если имеем классовый тип, значит строим вызов конструктора
		if( fn->GetType().GetBaseType().IsClassType() && 
			fn->GetType().GetDerivedTypeList().IsEmpty() )
		{
			const ClassType &cls = static_cast<const ClassType&>(fn->GetType().GetBaseType());
			const ConstructorList &ctorLst = cls.GetConstructorList();

			// список конструкторов может быть пустой
			if( ctorLst.empty() )
			{
				theApp.Error(errPos, "'%s' - класс не имеет конструкторов", 
					cls.GetQualifiedName().c_str());
				return ErrorOperand::GetInstance();
			}

			OverloadFunctionList ofl(ctorLst.size());			
			copy(ctorLst.begin(), ctorLst.end(), ofl.begin());

			// выбираем наилучший из списка конструкторов, если есть такой,
			// строим вызов, иначе выходим
			if( const Function *ctor = CheckBestViableFunction(ofl, *parametrList, NULL) )
			{
				// проверим возможность копирования параметров,				
				CheckParametrInitialization(ctor->GetFunctionPrototype().GetParametrList());
				const ClassType &ctorCls = 
					static_cast<const ClassType&>(ctor->GetSymbolTableEntry());
				ExpressionMakerUtils::ObjectCreationIsAccessible(
					ctorCls, errPos, false, false, true);

				// проверяем , чтобы класс не был абстарктым. Создание объекта
				// абстрактного класса невозможно
				if( ctorCls.IsAbstract() )
					theApp.Error( errPos,
						"создание объекта класса '%s' невозможно; класс является абстрактным",
						ctorCls.GetQualifiedName().c_str() );
			
				// строим новое выражение с конкретной функцией										
				PTypyziedEntity rtype = new TypyziedEntity(*ctor);
				const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
				return new FunctionCallExpression( false,
					new PrimaryOperand(false, *ctor), parametrList, rtype);
			}

			else
				return ErrorOperand::GetInstance();			
		}

		// иначе тип не является классом, значит переправляем 
		// преобразование CastMaker'у
		else
		{
			// сначала проверим, чтобы параметр был 1
			if( parametrList->size() > 1)
			{
				theApp.Error(errPos, 
					"'%s' - конструктор типа не может принимать '%i' параметр(а)",
					ExpressionPrinter(fn).GetExpressionString().c_str(),
					parametrList->size());
				return ErrorOperand::GetInstance();
			}

			// если нет параметров, значит заменяем вызов на null
			else if( parametrList->empty() )
			{
				return new PrimaryOperand(false, *new Literal(
					(BaseType*)&fn->GetType().GetBaseType(), true, false, 
					fn->GetType().GetDerivedTypeList(), "0") );
			}

			return TypeCastBinaryMaker(fn, (*parametrList)[0], OC_CAST, errPos).Make();
		}
	}

	// иначе неизвестный операнд
	else
		INTERNAL( "'FunctionCallBinaryMaker::Make' - неизвестный операнд");
	return NULL;
}


// создать выражение []
POperand ArrayBinaryMaker::Make()
{
	// проверяем, если второй тип - указатель, меняем местами операнды
	if( ExpressionMakerUtils::IsRvaluePointer(right->GetType()) )
		swap(const_cast<POperand&>(left), const_cast<POperand&>(right));

	// один из типов должен быть адресным, второй целым
	if( ExpressionMakerUtils::ToPointerType(const_cast<POperand&>(left), errPos, "[]") )
	{
		// проверяем, чтобы второй был целым
		if( !ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(right), errPos, "[]") )	
			return ErrorOperand::GetInstance();		

		// иначе строим
		else
		{
			ExpressionMakerUtils::ToRValue(left, errPos);
			TypyziedEntity *rtype = 
				ExpressionMakerUtils::DoCopyWithIndirection(left->GetType());

			// снимаем первый тип
			INTERNAL_IF( rtype->GetDerivedTypeList().IsEmpty() );
			const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
			return new BinaryExpression(OC_ARRAY, true, left, right, rtype);
		}
	}

	else
		return ErrorOperand::GetInstance();
}


// проверим, чтобы член принадлежал данному классу, проверка
// осуществляется только при обращении к квалифицированному имени
bool SelectorBinaryMaker::CheckMemberVisibility( const Identifier &id, const ClassType &objCls )
{
	if( !id.GetSymbolTableEntry().IsClassSymbolTable() )
	{
		theApp.Error(errPos, "'%s' - не является членом класса '%s'",
			id.GetQualifiedName().c_str(), objCls.GetQualifiedName().c_str());
		return false;
	}

	const ClassType &memCls = static_cast<const ClassType&>(id.GetSymbolTableEntry());
	DerivationManager dm(memCls, objCls);
	if( dm.IsBase() || &memCls == &objCls )
		return true;
	else
	{
		theApp.Error(errPos, "'%s' - не является членом класса '%s'",
			id.GetQualifiedName().c_str(), objCls.GetQualifiedName().c_str());
		return false;
	}
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand SelectorBinaryMaker::Make()
{
	PCSTR opName = op == '.' ? "." : "->";

	// для начала проверим, чтобы операнд слева был выражением или идентификатором
	if( left->IsErrorOperand() )
		return ErrorOperand::GetInstance();
	if( !(left->IsExpressionOperand() || left->IsPrimaryOperand()) )
	{
		theApp.Error(errPos, 
			"'%s' - оператор требует, чтобы слева был объект", opName );			
		return ErrorOperand::GetInstance();
	}

	// далее, проверяем тип операнда
	if( op == '.' )
	{
		if( !ExpressionMakerUtils::IsClassType(left->GetType()) )
		{
			theApp.Error(errPos, 
				"'.' - операнд слева должен иметь классовый тип");				
			return ErrorOperand::GetInstance();
		}
	}

	// иначе должен быть указатель на класс
	else
	{
		// сначала проверим, возможно имеется перегруженный оператор '->',
		// тогда найдем сначала его
		POperand rval = UnaryOverloadOperatorCaller(left, op, errPos).Call();
		if( !rval.IsNull() )
			const_cast<POperand&>(left) = rval;

		const DerivedTypeList &dtl = left->GetType().GetDerivedTypeList();
		bool ptr = (dtl.IsPointer() && dtl.GetDerivedTypeCount() == 1) ||
			(dtl.GetDerivedTypeCount() == 2 && dtl.IsReference() && 
			dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_POINTER);
		if( !ptr || !left->GetType().GetBaseType().IsClassType() )
		{
			theApp.Error(errPos, 
				"'->' - операнд слева должен иметь тип 'указатель на класс'");				
			return ErrorOperand::GetInstance();
		}		
	}

	// после проверки левого операнда, создаем идентификатор
	bool qualified = idPkg.GetChildPackageCount() > 1;
	POperand right = IdentifierOperandMaker( idPkg, &left->GetType() ).Make();

	if( right->IsErrorOperand() )
		return ErrorOperand::GetInstance();

	// иначе если перегруженная функция
	else if( right->IsOverloadOperand() )
	{
		if( qualified )
		{
			const OverloadFunctionList &ofl =
				static_cast<OverloadOperand &>(*right).GetOverloadList();
			for( int i = 0; i<ofl.size(); i++ )
			if( !CheckMemberVisibility( *ofl[i],				
				static_cast<const ClassType&>(left->GetType().GetBaseType()) ) )
				return ErrorOperand::GetInstance();
		}

		// создаем выражение
		return new BinaryExpression( op, true, left, right, NULL );
	}

	// иначе если обычный член
	else if( right->IsPrimaryOperand() )
	{
		if( qualified )
		{
			const Identifier &id = 
				dynamic_cast<const Identifier &>(right->GetType().IsDynamicTypyziedEntity() ?
					static_cast<const DynamicTypyziedEntity &>(right->GetType()).GetOriginal() :
					right->GetType());
			if( !CheckMemberVisibility( id,
				static_cast<const ClassType&>(left->GetType().GetBaseType()) ) )
				return ErrorOperand::GetInstance();
		}

		// возвращаем результат выражения
		return new BinaryExpression( op, !right->GetType().IsEnumConstant(),
			left, right, new TypyziedEntity(right->GetType()) );
	}

	// иначе мы не можем обратиться к члену через оператор
	else
	{
		theApp.Error(errPos,
			"'%s' - обращение к члену невозможно через '%s'",
			ParserUtils::PrintPackageTree(&idPkg).c_str(), opName );
		return ErrorOperand::GetInstance();
	}
}


// создать выражение '!'
POperand LogicalUnaryMaker::Make()
{
	// требуется склярный операнд
	if( !ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(right), errPos, "!") )
		return ErrorOperand::GetInstance();

	// иначе строим унарное выражение, результатом будет bool
	return new UnaryExpression('!', false, false, right, 
		new TypyziedEntity((BaseType*)&ImplicitTypeManager(KWBOOL).GetImplicitType(),
			false, false, DerivedTypeList()) );
}


// создать выражение '~'. Может вернуть ErrorOperand, если приведение невозможно
POperand BitReverseUnaryMaker::Make()
{
	// требуется целый операнд
	if( !ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(right), errPos, "~") )	
		return ErrorOperand::GetInstance();

	// иначе строим унарное выражение, результатом будет int
	return new UnaryExpression('~', false, false, right, 
		new TypyziedEntity((BaseType*)&ImplicitTypeManager(KWINT).GetImplicitType(),
			false, false, DerivedTypeList()) );
}


// создать выражение % << >> ^ | &
POperand IntegralBinaryMaker::Make()
{
	string opName = ExpressionPrinter::GetOperatorName(op);
	
	// оба операнда должны быть целыми	
	if( !ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(left), errPos, opName) ||
		!ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(right), errPos, opName) )	
		return ErrorOperand::GetInstance();

	// иначе строим бинарное выражение, результатом будет int
	return new BinaryExpression(op, false, left, right, 
		new TypyziedEntity((BaseType*)&ImplicitTypeManager(KWINT).GetImplicitType(),
			false, false, DerivedTypeList()) );	
}


// создать выражение *  /
POperand MulDivBinaryMaker::Make()
{
	PCSTR opName = op == '*' ? "*" : "/";
	// оба операнда должны быть арифметическими
	if( !ExpressionMakerUtils::ToArithmeticType(const_cast<POperand&>(left), errPos, opName) ||
		!ExpressionMakerUtils::ToArithmeticType(const_cast<POperand&>(right), errPos, opName) )	
		return ErrorOperand::GetInstance();

	// иначе строим бинарное выражение
	return new BinaryExpression(op, false, left, right, 
		ExpressionMakerUtils::DoResultArithmeticOperand(
			left->GetType(), right->GetType()) );
}


// создать выражение +  - 
POperand ArithmeticUnaryMaker::Make()
{
	PCSTR opName = op == '+' ? "+" : "-";
	// в случае, если операнд '+', нужен склярный тип. У '-', только арифметический
	if( !(op == '+' ? ExpressionMakerUtils::ToArithmeticOrPointerType(
			const_cast<POperand&>(right), errPos, opName) :
		 ExpressionMakerUtils::ToIntegerType(const_cast<POperand&>(right), errPos, opName)) )	
		return ErrorOperand::GetInstance();

	// иначе строим унарное выражение 
	return new UnaryExpression(op, false, false, right, 
		ExpressionMakerUtils::DoCopyWithIndirection(right->GetType()) );
}


// создать выражение '*'
POperand IndirectionUnaryMaker::Make()
{	
	if( !ExpressionMakerUtils::ToPointerType(const_cast<POperand&>(right), errPos, "*") )	
		return ErrorOperand::GetInstance();

	// тип будет такой же, только удаляем указатель
	TypyziedEntity *rtype = new TypyziedEntity(right->GetType());

	// если первый тип ссылка, снимаем ее
	if( rtype->GetDerivedTypeList().IsReference() )
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
	INTERNAL_IF( !rtype->GetDerivedTypeList().IsPointer() );
	const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();

	// иначе строим унарное выражение, которое является lvalue
	return new UnaryExpression('*', true, false, right, rtype);
}


// создать выражение постфиксный и префиксный кремент
POperand IncDecUnaryMaker::Make()
{
	PCSTR opName = abs(op) == INCREMENT ? "++" : "--";
	// сначала проверим, чтобы тип был склярным
	if( !ExpressionMakerUtils::ToArithmeticOrPointerType(
				const_cast<POperand&>(right), errPos, opName) )	
		return ErrorOperand::GetInstance();

	// после проверки типа, проверяем, чтобы операнд был модифицируемым lvalue,
	// т.е. указателем или арифметическим не константным типом
	if( !ExpressionMakerUtils::IsModifiableLValue(right, errPos, opName) )
		return ErrorOperand::GetInstance();

	// проверяем, если тип является функцией, это ошибка
	if( ExpressionMakerUtils::IsFunctionType(right->GetType()) )
	{
		theApp.Error(errPos, "'%s' - не применим к типу '%s'", opName,
			right->GetType().GetTypyziedEntityName(false).c_str());
		return ErrorOperand::GetInstance();
	}

	// после чего проверим на полностью объявленный тип
	ExpressionMakerUtils::ToRValue(right, errPos);

	// последнее, если операция декремента, тип не может быть bool
	if( abs(op) == DECREMENT && ExpressionMakerUtils::IsIntegral(right->GetType()) &&
		right->GetType().GetBaseType().GetBaseTypeCode() == BaseType::BT_BOOL )
	{
		theApp.Error(errPos, "'--' - не применим к типу 'bool'");
		return ErrorOperand::GetInstance();
	}

	// создаем выражение, если оператора были префиксные (меньше нуля), результат lvalue	
	return new UnaryExpression(op, op < 0, op > 0, right, 
		ExpressionMakerUtils::DoCopyWithIndirection(right->GetType()) );
}


// создать выражение &
POperand AddressUnaryMaker::Make()
{
	// взятие адреса - полиморфная операция. Она может применятся не только к
	// выражениям или основным операндам, но и к перегруженным функциям.
	// Также следует учитывать, что это единственная операция, которая применима
	// к членам класса, без использования 'this'.
	// Единственная проверка - проверка на lvalue, в том числе и массив и функция
	try {

		// если операнд перегруженный, единственная проверка, чтобы он был lvalue,
		// и задаем ему rvalue
		if( right->IsOverloadOperand() )
		{
			OverloadOperand &ovop = const_cast<OverloadOperand &>(
				static_cast<const OverloadOperand &>(*right));
			if( !ovop.IsLvalue() ) 
				throw 0;
		
			// иначе задаем rvalue и выходим
			ovop.SetRValue();
			return right;
		}

		// если выражение, то оно также должно быть rvalue и добавляем указатель,
		// если тип не является функцией
		else if( right->IsExpressionOperand() )
		{
			if( !static_cast<const Expression &>(*right).IsLvalue() )
				throw 1;
		}

		// если основной операнд
		else if( right->IsPrimaryOperand() )
		{
			if( !static_cast<const PrimaryOperand &>(*right).IsLvalue() )
				throw 2;				
		}

		// иначе неизвестный операнд
		else
			INTERNAL( "'AddressUnaryMaker::Make()' - получает неизвестный операнд");

	} catch( int ) {
		// получаем ошибку, операнд не является lvalue
		theApp.Error(errPos, "'&' - операнд не является lvalue");
		return ErrorOperand::GetInstance(); 
	}

	// тип будет такой же, только добавляем указатель
	TypyziedEntity *rtype = new TypyziedEntity(right->GetType());

	// если первый тип ссылка, снимаем ее
	if( rtype->GetDerivedTypeList().IsReference() )
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
	
	// проверяем, если имеем член класса без this, получаем из него указатель
	// на член, иначе обычный указатель	
	if( right->IsPrimaryOperand() && 		
		ExpressionMakerUtils::CheckMemberThisVisibility(right, errPos, false) < 0 )
	{
		const ClassType &mcls = static_cast<const ClassType &>(
			dynamic_cast<const Identifier &>(right->GetType()).GetSymbolTableEntry());

		// наконец добавляем указатель на член
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PushHeadDerivedType(
			new PointerToMember(&mcls, false, false));
	}

	// иначе добавляем обычный указатель
	else	
	{
		// перед этим проверим, чтобы не был взят адрес из битового поля
		// проверка, адресс нельзя получить из битового поля
		const TypyziedEntity *te = NULL;
		if( right->IsExpressionOperand() )
		{
			if( static_cast<const Expression&>(*right).GetOperatorCode() == '.' ||
				static_cast<const Expression&>(*right).GetOperatorCode() == ARROW )
				te = &static_cast<const BinaryExpression&>(*right).GetOperand2()->GetType();
		}

		else if( right->IsPrimaryOperand() )
			te = &right->GetType();

		if( te && te->IsObject() && static_cast<const ::Object*>(te)->
			GetStorageSpecifier() == ::Object::SS_BITFIELD )	
			theApp.Error(errPos, "'&' - оператор неприменим к битовому полю");	

		// наконец добавляем указатель
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PushHeadDerivedType(
			new Pointer(false, false));	
	}

	// иначе строим унарное выражение, которое является rvalue
	return new UnaryExpression('&', false, false, right, rtype);	
}


// создать выражение +
POperand PlusBinaryMaker::Make()
{
	// получаем тип первого операнда
	if( !ExpressionMakerUtils::ToArithmeticOrPointerType(
					const_cast<POperand&>(left), errPos, "+") ||
	   !ExpressionMakerUtils::ToArithmeticOrPointerType(
				const_cast<POperand&>(right), errPos, "+") )	
		return ErrorOperand::GetInstance();

	// иначе проверяем, какие типы получены. Должны быть, либо оба
	// арифметических, либо один целый, а второй указатель
	if( ExpressionMakerUtils::IsArithmetic(left->GetType()) &&
		ExpressionMakerUtils::IsArithmetic(right->GetType()) )
	{
		// строим выражение
		return new BinaryExpression('+', false, left, right, 
			ExpressionMakerUtils::DoResultArithmeticOperand(
				left->GetType(), right->GetType()) );
	}

	// иначе проверяем, чтобы один был целым, а второй указателем
	else
	{
		// если первый - указатель
		if( ExpressionMakerUtils::IsRvaluePointer(left->GetType()) )
		{
			// проверяем, чтобы второй был целым и указатель был
			// полностью объявленный тип
			if( !ExpressionMakerUtils::IsIntegral(right->GetType()) )
			{
				theApp.Error(errPos, "'+' - второй операнд должен иметь целый тип");
				return ErrorOperand::GetInstance(); 
			}

			// проверяем также, чтобы не было указателя на функцию
			if( ExpressionMakerUtils::IsFunctionType(left->GetType()) )
			{
				theApp.Error(errPos, "'+' - неприменим к указателю на функцию");
				return ErrorOperand::GetInstance(); 
			}

			// проверяем, чтобы указатель был полностью объявленным типом			
			ExpressionMakerUtils::ToRValue(left, errPos);

			// строим выражение
			return new BinaryExpression('+', false, left, right, 
				ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );
		}

		// если второй указатель, значит первый должен быть целым
		else if( ExpressionMakerUtils::IsRvaluePointer(right->GetType()) )
		{
			// проверяем, чтобы второй был целым и указатель был
			// полностью объявленный тип
			if( !ExpressionMakerUtils::IsIntegral(left->GetType()) )
			{
				theApp.Error(errPos, "'+' - первый операнд должен иметь целый тип");
				return ErrorOperand::GetInstance(); 
			}
			
			// проверяем также, чтобы не было указателя на функцию
			if( ExpressionMakerUtils::IsFunctionType(right->GetType()) )
			{
				theApp.Error(errPos, "'+' - неприменим к указателю на функцию");
				return ErrorOperand::GetInstance(); 
			}

			// проверяем, чтобы указатель был полностью объявленным типом			
			ExpressionMakerUtils::ToRValue(right, errPos);

			// строим выражение
			return new BinaryExpression('+', false, left, right, 
				ExpressionMakerUtils::DoCopyWithIndirection(right->GetType()) );
		}

		// иначе, ошибка
		else
		{
			theApp.Error(errPos, "'+' - неприменим к типам '%s' и '%s'",
				left->GetType().GetTypyziedEntityName(false).c_str(),
				right->GetType().GetTypyziedEntityName(false).c_str());
			return ErrorOperand::GetInstance(); 
		}
	}		
}


// создать выражение -
POperand MinusBinaryMaker::Make()
{	
	// получаем тип первого операнда
	if( !ExpressionMakerUtils::ToArithmeticOrPointerType(
			const_cast<POperand&>(left), errPos, "-") ||
		!ExpressionMakerUtils::ToArithmeticOrPointerType(
			const_cast<POperand&>(right), errPos, "-") )
		return ErrorOperand::GetInstance();

	// иначе проверяем, какие типы получены. Должны быть, либо оба
	// арифметических, либо первый указатель, а второй целый, либо
	// оба указателя
	if( ExpressionMakerUtils::IsArithmetic(left->GetType()) &&
		ExpressionMakerUtils::IsArithmetic(right->GetType()) )
	{
		// строим выражение
		return new BinaryExpression('-', false, left, right, 
			ExpressionMakerUtils::DoResultArithmeticOperand(
				left->GetType(), right->GetType()) );
	}

	// иначе проверяем, если первый указатель, то второй должен быть
	// либо целым, либо указателем
	else if( ExpressionMakerUtils::IsRvaluePointer(left->GetType()) )
	{
		// если второй - указатель
		if( ExpressionMakerUtils::IsRvaluePointer(right->GetType()) )
		{		
			// проверяем также, чтобы не было указателя на функцию
			if( ExpressionMakerUtils::IsFunctionType(left->GetType()) ||
				ExpressionMakerUtils::IsFunctionType(right->GetType()) )
			{
				theApp.Error(errPos, "'-' - неприменим к указателю на функцию");
				return ErrorOperand::GetInstance(); 
			}

			// проверяем, чтобы типы были одинаковые
			ScalarToScalarCaster stsc(left->GetType(), right->GetType(), false);
			stsc.ClassifyCast();
			if( !stsc.IsConverted() )
			{
				theApp.Error(errPos, "'-' - %s", stsc.GetErrorMessage().c_str());
				return ErrorOperand::GetInstance(); 
			}

			// выполняем физическое преобразование, возможно требуется из
			// производного в базовый
			stsc.DoCast(left, const_cast<POperand&>(right), errPos);

			// проверяем, чтобы указатель был полностью объявленным типом			
			ExpressionMakerUtils::ToRValue(left, errPos);
			ExpressionMakerUtils::ToRValue(right, errPos);

			// строим выражение, результирующий тип - unsigned int
			return new BinaryExpression('-', false, left, right, 
				new TypyziedEntity(
					(BaseType*)&ImplicitTypeManager(KWINT, KWUNSIGNED).GetImplicitType(),
					false, false, DerivedTypeList()) );
		}

		// иначе, если второй целый
		else if( ExpressionMakerUtils::IsIntegral(right->GetType()) )
		{				
			// проверяем также, чтобы не было указателя на функцию
			if( ExpressionMakerUtils::IsFunctionType(left->GetType()) )
			{
				theApp.Error(errPos, "'-' - неприменим к указателю на функцию");
				return ErrorOperand::GetInstance(); 
			}

			// проверяем, чтобы указатель был полностью объявленным типом			
			ExpressionMakerUtils::ToRValue(left, errPos);

			// строим выражение, результатом будет указатель
			return new BinaryExpression('-', false, left, right, 
				ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );
		}

		// иначе, ошибка
		else
		{
			theApp.Error(errPos, "'-' - неприменим к типам '%s' и '%s'",
				left->GetType().GetTypyziedEntityName(false).c_str(),
				right->GetType().GetTypyziedEntityName(false).c_str());
			return ErrorOperand::GetInstance(); 
		}
	}		

	// если оба типа не арифметические и первый не указатель,
	// значит указатель - второй тип, выводим ошибку
	else
	{
		theApp.Error(errPos, "'-' - первый операнд должен иметь адресный тип");
		return ErrorOperand::GetInstance();
	}
}


// создать выражение  <   <=   >=  >  ==  !=
POperand ConditionBinaryMaker::Make()
{
	string opName = ExpressionPrinter::GetOperatorName(op);
	
	// проверяем, чтобы тип соотв. правилам
	if( !(op == EQUAL || op == NOT_EQUAL ?
		ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(left), errPos, opName) :
		ExpressionMakerUtils::ToArithmeticOrPointerType(
			const_cast<POperand&>(left), errPos, opName)) )
		return ErrorOperand::GetInstance();
		
	if( !(op == EQUAL || op == NOT_EQUAL ?
		ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(right), errPos, opName) :
		ExpressionMakerUtils::ToArithmeticOrPointerType(
			const_cast<POperand&>(right), errPos, opName)) )
		return ErrorOperand::GetInstance();

	// теперь проверяем, можно ли преобразовать один тип к другому
	ScalarToScalarCaster stsc(left->GetType(), right->GetType(), false);
	stsc.ClassifyCast();

	// если преобразование невозможно, выйти
	if( !stsc.IsConverted() )
	{
		theApp.Error(errPos, "'%s' - %s", opName.c_str(), stsc.GetErrorMessage().c_str());
		return ErrorOperand::GetInstance();
	}

	// результирующий тип - bool
	stsc.DoCast(left, const_cast<POperand&>(right), errPos);
	return new BinaryExpression(op, false, left, right, 
		new TypyziedEntity(
			(BaseType*)&ImplicitTypeManager(KWBOOL).GetImplicitType(),
			false, false, DerivedTypeList()) );	
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand LogicalBinaryMaker::Make()
{
	string opName = op == LOGIC_AND ? "&&" : "||";
	if( !ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(left), errPos, opName) ||
		!ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(right), errPos, opName) )
		return ErrorOperand::GetInstance();
	return new BinaryExpression(op, false, left, right, 
		new TypyziedEntity(
			(BaseType*)&ImplicitTypeManager(KWBOOL).GetImplicitType(),
			false, false, DerivedTypeList()) );	
}


// создать выражение ?:
POperand IfTernaryMaker::Make()
{
	string opName = "?:";

	// первое выражение длжно иметь склярный тип
	if( !ExpressionMakerUtils::ToScalarType(const_cast<POperand&>(cond), errPos, opName) )
		return ErrorOperand::GetInstance();

	// если первый тип void, значит результат будет второй
	if( left->GetType().GetDerivedTypeList().IsEmpty() &&
		left->GetType().GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID )
		return new TernaryExpression('?', false, cond, left, right, 
			ExpressionMakerUtils::DoCopyWithIndirection(right->GetType()) );

	// если второй void, значит результат первый
	else if( right->GetType().GetDerivedTypeList().IsEmpty() &&
		right->GetType().GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID )
		return new TernaryExpression('?', false, cond, left, right, 
			ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );

	// иначе пытаемся преобразовать оба типа
	PCaster caster1 = AutoCastManager(left->GetType(), right->GetType(), true).RevealCaster(),
			caster2 = AutoCastManager(right->GetType(), left->GetType(), true).RevealCaster();
	caster1->ClassifyCast();
	caster2->ClassifyCast();

	// если оба преобразования успешны, это не катит
	if( caster1->IsConverted() && caster2->IsConverted() &&
		caster1->GetCastCategory() > Caster::CC_EQUAL )
	{
		theApp.Error(errPos,
			"'?:' - преобразование из '%s' в '%s' неоднозначно",
			left->GetType().GetTypyziedEntityName().c_str(),
			right->GetType().GetTypyziedEntityName().c_str());
		return ErrorOperand::GetInstance();
	}

	// если типы одинаковы, есть маза получить lvalue
	if( caster1->GetCastCategory() == Caster::CC_EQUAL &&
		caster2->GetCastCategory() == Caster::CC_EQUAL )
	{
		bool lv = ExpressionMakerUtils::IsLValue(left) && ExpressionMakerUtils::IsLValue(right);
		return new TernaryExpression('?', lv, cond, left, right, 
			ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );
	}

	// если первое успешно, результатом будет первый тип
	if( caster1->IsConverted() )
	{
		caster1->DoCast(left, const_cast<POperand&>(right), errPos);
		return new TernaryExpression('?', false, cond, left, right, 
			ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );
	}

	else if( caster2->IsConverted() )
	{
		caster2->DoCast(right, const_cast<POperand&>(left), errPos);
		return new TernaryExpression('?', false, cond, left, right, 
			ExpressionMakerUtils::DoCopyWithIndirection(right->GetType()) );
	}

	// иначе преобразование невозможно
	else
	{
		theApp.Error(errPos,
			"'?:' - невозможно преобразовать '%s' к '%s'",
			left->GetType().GetTypyziedEntityName(false).c_str(),
			right->GetType().GetTypyziedEntityName(false).c_str());
		return ErrorOperand::GetInstance();
	}	
}


// метод, проверяет выражение если операция сдвоенная;
// -1 - ошибка, 0 - продолжит проверку, 1 - создать выражение и выйти
int AssignBinaryMaker::CheckOperation( const string &opName )
{
	INTERNAL_IF( op == '=' );

	// проверяем, если левый операнд, является классовым типом,
	// то операция не может построится
	if( ExpressionMakerUtils::IsClassType(left->GetType()) )
	{
		theApp.Error(errPos, "'%s::operator %s(%s)' - не объявлен",
			static_cast<const ClassType&>(
				left->GetType().GetBaseType()).GetQualifiedName().c_str(),
			opName.c_str(), 
			right->GetType().GetTypyziedEntityName(false).c_str() );
			
		return -1;
	}

	// иначе если += или -=, тип может быть указателем или арифметическим,
	// иначе только арифметическим
	if( op == PLUS_ASSIGN || op == MINUS_ASSIGN )
	{
		// если указатель
		if( ExpressionMakerUtils::IsRvaluePointer(left->GetType()) )
		{
			// проверяем, чтобы второй был целым и указатель был
			// полностью объявленный тип
			if( !ExpressionMakerUtils::IsIntegral(right->GetType()) )
			{
				theApp.Error(errPos, 
					"'%s' - второй операнд должен иметь целый тип", opName.c_str());
				return -1; 
			}

			// проверяем также, чтобы не было указателя на функцию
			if( ExpressionMakerUtils::IsFunctionType(left->GetType()) )
			{
				theApp.Error(errPos, 
					"'%s' - неприменим к указателю на функцию", opName.c_str());
				return -1; 
			}

			// проверяем, чтобы указатель был полностью объявленным типом			
			ExpressionMakerUtils::ToRValue(left, errPos);
			return 1;
		}
		
		// иначе если не арифметический, ошибка
		else if( !ExpressionMakerUtils::IsArithmetic(left->GetType()) ||
			left->GetType().GetBaseType().GetBaseTypeCode() == BaseType::BT_ENUM )
			goto err;		
	}

	// проверим, чтобы тип был арифметическим
	else
	{
		// нельзя также, чтобы тип был перечислимый
		if( !ExpressionMakerUtils::IsArithmetic(left->GetType()) ||
			left->GetType().GetBaseType().GetBaseTypeCode() == BaseType::BT_ENUM )
		{
		err:
			theApp.Error(errPos, 
				"'%s' - не арифметический тип; '%s' - оператор требует арифметический тип",
				left->GetType().GetTypyziedEntityName().c_str(), opName.c_str());
			return -1;
		}
	}

	return 0;
}


// создать выражение -  =   +=   -=   *=   /=   %=   >>=   <<=  |=  &=  ^=
POperand AssignBinaryMaker::Make()
{
	// если операция присвоения сдвоенная, создать промежуточную операцию,
	// которую в последствии использовать как правый операнд	
	string opName = ExpressionPrinter::GetOperatorName(op);

	// проверяем, чтобы операнд был модифицируемым lvalue в первую очередь
	if( !ExpressionMakerUtils::IsModifiableLValue(left, errPos, opName.c_str()) )
		return ErrorOperand::GetInstance();

	// проверка на сдвоенную операцию
	if( op != '=' )
	{		
		if( int r = CheckOperation(opName) )
			return r < 0 ? ErrorOperand::GetInstance() :
				new BinaryExpression(op, true, left, right, 
					ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );
	
	}

	// если left, имеет классовый тип, проверяем, чтобы класс был полностью объявлен
	if( ExpressionMakerUtils::IsClassType(left->GetType()) &&
		static_cast<const ClassType&>(left->GetType().GetBaseType()).IsUncomplete() )
	{
		theApp.Error(errPos, "'%s' - незавершенный класс; присвоение невозможно",
			static_cast<const ClassType&>(left->GetType().GetBaseType()).
			GetQualifiedName().c_str());
		return ErrorOperand::GetInstance();
	}

	// перед преобразованием, делаем неявное преобразование из ссылки в не ссылку,
	// т.к. преобразователь выводит ошибку по поводу меньшей квалифицированности
	if( left->GetType().GetDerivedTypeList().IsReference() )	
		const_cast<POperand&>(left) = 
			new UnaryExpression(GOC_REFERENCE_CONVERSION, true, false,
				left, ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()) );	

	// выполняем автоматическое преобразование
	PCaster caster = AutoCastManager(left->GetType(), right->GetType(), true).RevealCaster();
	INTERNAL_IF( caster.IsNull() );
	caster->ClassifyCast();

	// если преобразование не удалось, выйти
	if( !caster->IsConverted() )
	{
		if( caster->GetErrorMessage().empty() )
			theApp.Error(errPos, 
				"'%s' - невозможно преобразовать '%s' к '%s'",
				opName.c_str(), right->GetType().GetTypyziedEntityName(false).c_str(),
				left->GetType().GetTypyziedEntityName(false).c_str() );
		else
			theApp.Error(errPos, 
				"'%s' - %s",
				opName.c_str(), caster->GetErrorMessage().c_str());
		return ErrorOperand::GetInstance();
	}

	// иначе выполняем преобразование
	caster->DoCast(left, const_cast<POperand&>(right), errPos);

	// строим выражение. Оператор '=', т.к. сдвоенные операции строятся по отдельности,
	// результат lvalue
	return new BinaryExpression(op, true, left, right, 
		ExpressionMakerUtils::DoCopyWithIndirection(left->GetType()));
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand ComaBinaryMaker::Make()
{
	// оба операнда должны быть выражением или основным операндом
	INTERNAL_IF( !( (left->IsPrimaryOperand()  || left->IsExpressionOperand()) &&
					(right->IsPrimaryOperand() || right->IsExpressionOperand()) ) );

	bool lv = right->IsPrimaryOperand()
		? static_cast<const PrimaryOperand &>(*right).IsLvalue()
		: static_cast<const Expression &>(*right).IsLvalue();

	return new BinaryExpression(',', lv, left, right, new TypyziedEntity(right->GetType()));
}


// перед тем как начать строительство выражения, следует преобразовать
// пакет с типом в выражение sizeof, которое строится на основании пакета
void NewTernaryMaker::MakeSizeofExpression( const NodePackage &typePkg )
{
	INTERNAL_IF( typePkg.GetPackageID() != PC_DECLARATION || 
		typePkg.GetChildPackageCount() != 2 || typePkg.IsErrorChildPackages() || 
		typePkg.GetChildPackage(1)->GetPackageID() != PC_DECLARATOR );
	INTERNAL_IF( !allocType->IsTypeOperand() );

	// сразу создадим результирующий тип выражения
	PTypyziedEntity sizeofExpType = new TypyziedEntity( 
		(BaseType*)&ImplicitTypeManager(KWINT, KWUNSIGNED).GetImplicitType(),
		false, false, DerivedTypeList() );

	// далее проверяем, если есть список производных типов у декларатора
	const NodePackage &dtr = static_cast<const NodePackage &>(*typePkg.GetChildPackage(1));
	if( dtr.GetChildPackageCount() > 0	&&
		dtr.GetChildPackage(0)->GetPackageID() == PC_ARRAY )
	{
		const NodePackage &ar = *static_cast<const NodePackage *>(dtr.GetChildPackage(0));
		// получаем выражение, только если количество дочерних пакетов равно 3
		if( ar.GetChildPackageCount() == 3 )
		{
			const POperand &arraySz = static_cast<const ExpressionPackage *>(
					ar.GetChildPackage(1))->GetExpression();

			// проверяем, чтобы выражение было целым
			if( !ExpressionMakerUtils::IsIntegral(arraySz->GetType()) )
				theApp.Error(errPos, "'new[]' - размер массива не целый");

			// если выражение интерпретируемое
			double asz;
			if( ExpressionMakerUtils::IsInterpretable(arraySz, asz) )			
				if( asz < 1 )
					theApp.Error(errPos, "'new[]' - некорректный размер выделяемой памяти");			

			// снимаем с выделяемого типа верхний массив
			TypyziedEntity *destType = new TypyziedEntity(allocType->GetType());
			INTERNAL_IF( !destType->GetDerivedTypeList().IsArray() );
			const_cast<DerivedTypeList &>(destType->GetDerivedTypeList()).PopHeadDerivedType();

			// создаем выражение выходим
			POperand szofExp = new UnaryExpression( KWSIZEOF, false, false, 
					new TypeOperand(*destType), sizeofExpType );

			// умножаем размер на выражение
			allocSize = new BinaryExpression('*', false, arraySz, szofExp, sizeofExpType );
			return ;
		}
	}

	// в противном случае просто высчитываем размер типа
	allocSize = new UnaryExpression( KWSIZEOF, false, false, allocType, sizeofExpType );
}


// метод производит поиск оператора new и строит вызов
POperand NewTernaryMaker::MakeNewCall( bool array, bool clsType )
{
	PExpressionList paramList = placementList;

	// добавляем в список параметров, первым параметром выражение sizeof
	paramList->insert(paramList->begin(), allocSize);

	// далее выполняем поиск оператора. Сначала, если тип классовый,
	// ищем внутри класса
	if( clsType && !globalOp )
	{
		const ClassType &cls = 
			static_cast<const ClassType &>(allocType->GetType().GetBaseType());
		OverloadOperatorFounder oof( array ? OC_NEW_ARRAY : KWNEW, false, &cls, errPos);

		// если оператор найден, вернем вызов функции
		if( oof.IsFound() )
		{
			INTERNAL_IF( oof.GetClassOperatorList().empty() || oof.IsAmbigous() );
			POperand pol = new OverloadOperand( oof.GetClassOperatorList() );
			return FunctionCallBinaryMaker(pol, paramList, OC_FUNCTION, errPos).Make();
		}

		// если неоднозначен, также возвращаем errorOperand
		else if( oof.IsAmbigous() )
			return ErrorOperand::GetInstance();
	}

	// в противном случае выполняем поиск глобального оператора
	OverloadOperatorFounder oof( array ? OC_NEW_ARRAY : KWNEW, true, NULL, errPos);

	// если оператор найден, вернем вызов функции
	if( oof.IsFound() )
	{
		INTERNAL_IF( oof.GetGlobalOperatorList().empty() || oof.IsAmbigous() );
		POperand pol = new OverloadOperand( oof.GetGlobalOperatorList() );				 
		return FunctionCallBinaryMaker(pol, paramList, OC_FUNCTION, errPos).Make();
	}

	// в этой точке следует выводить ошибку
	theApp.Fatal(errPos, "'void *operator new(unsigned)' - не объявлен");
	return ErrorOperand::GetInstance();
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand NewTernaryMaker::Make()
{
	// проверяем, если хоть один из элементов ошибочен, вернуть
	// errorOperand
	int i;
	for( i = 0; i<placementList->size(); i++ )
		if( placementList->at(i)->IsErrorOperand() )
			return ErrorOperand::GetInstance();

	for( i = 0; i<initializatorList->size(); i++ )
		if( initializatorList->at(i)->IsErrorOperand() )
			return ErrorOperand::GetInstance();

	// если тип является ошибочным
	if( allocType->IsErrorOperand() )
		return ErrorOperand::GetInstance();
	INTERNAL_IF( !allocType->IsTypeOperand() );

	// все операнды являются корректными. Проверяем выделяемый тип.
	// Не может быть ссылкой, void, незвершенный или абстрактный класс,
	// функцией
	const TypyziedEntity &at = allocType->GetType();
	if( at.GetDerivedTypeList().IsReference() || at.GetDerivedTypeList().IsFunction() ||
		(at.GetDerivedTypeList().IsEmpty() && 
		 at.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID) )
	{
		theApp.Error(errPos, "'new' - тип '%s' не является объектным",
			at.GetTypyziedEntityName().c_str());
		return ErrorOperand::GetInstance();
	}

	// проверяем, чтобы класс не был абстрактным или незавершенным
	PTypyziedEntity rtype = new TypyziedEntity(at);
	bool array = at.GetDerivedTypeList().IsArray();
	while( rtype->GetDerivedTypeList().IsArray() )
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();
	
	// теперь проверяем, если результирующий тип - абстрактный или незавершенный
	// класс, это ошибка
	if( rtype->GetDerivedTypeList().IsEmpty() &&
		(at.GetBaseType().IsClassType() &&  
		 (static_cast<const ClassType&>(at.GetBaseType()).IsUncomplete() ||
		  static_cast<const ClassType&>(at.GetBaseType()).IsAbstract()) ) )
	{
		theApp.Error(errPos, "'new%s' - создание объекта %s класса невозможно",
			array ? "[]" : "", static_cast<const ClassType&>(at.GetBaseType()).
				IsUncomplete() ? "незавершенного" : "абстрактного");
		return ErrorOperand::GetInstance();
	}
	
	// объявляем операнд-функцию, которая автоматически вызывается при выделении памяти
	bool cls = rtype->GetDerivedTypeList().IsEmpty() && rtype->GetBaseType().IsClassType();
	POperand call = MakeNewCall(array, cls);
	if( call->IsErrorOperand() )
		return ErrorOperand::GetInstance();

	// далее проверяем инициализаторы
	ExpressionMakerUtils::CorrectObjectInitialization( at, 
		const_cast<PExpressionList&>(initializatorList), false, errPos);

	const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).
		PushHeadDerivedType(new Pointer(false, false) );
	// возвращаем результат
	return new NewExpression( array ? OC_NEW_ARRAY : KWNEW, call, initializatorList, rtype );
}


// метод строит вызова оператора delete
POperand DeleteUnaryMaker::MakeDeleteCall( bool clsType )
{
	// формируем список параметров
	PExpressionList paramList = new ExpressionList;
	paramList->push_back( right );

	// далее выполняем поиск оператора. Сначала, если тип классовый,
	// ищем внутри класса
	if( clsType && op > 0 )
	{
		const ClassType &cls = 
			static_cast<const ClassType &>(right->GetType().GetBaseType());
		OverloadOperatorFounder oof( op, false, &cls, errPos);

		// если оператор найден, вернем вызов функции
		if( oof.IsFound() )
		{
			INTERNAL_IF( oof.GetClassOperatorList().empty() || oof.IsAmbigous() );
			POperand pol = new OverloadOperand( oof.GetClassOperatorList() );
			return FunctionCallBinaryMaker(pol, paramList, OC_FUNCTION, errPos).Make();
		}

		// если неоднозначен, также возвращаем errorOperand
		else if( oof.IsAmbigous() )
			return ErrorOperand::GetInstance();
	}

	// в противном случае выполняем поиск глобального оператора
	OverloadOperatorFounder oof( op, true, NULL, errPos);

	// если оператор найден, вернем вызов функции
	if( oof.IsFound() )
	{
		INTERNAL_IF( oof.GetGlobalOperatorList().empty() || oof.IsAmbigous() );
		POperand pol = new OverloadOperand( oof.GetGlobalOperatorList() );				 
		return FunctionCallBinaryMaker(pol, paramList, OC_FUNCTION, errPos).Make();
	}

	// в этой точке следует выводить ошибку
	theApp.Fatal(errPos, "'void operator delete(void *)' - не объявлен");
	return ErrorOperand::GetInstance();
}


// создать выражение delete, delete[], ::delete, ::delete[]
POperand DeleteUnaryMaker::Make()
{
	string opName = ExpressionPrinter::GetOperatorName(op);
	if( !ExpressionMakerUtils::ToPointerType(const_cast<POperand&>(right), errPos, opName) )
		return ErrorOperand::GetInstance();
		
	// далле проверяем, чтобы указатель не был функцией
	if( ExpressionMakerUtils::IsFunctionType(right->GetType()) )
	{
		theApp.Error(errPos, 
			"'%s' - невозможно удалить указатель на функцию",
			opName.c_str());
		return ErrorOperand::GetInstance();
	}

	// последнее, проверяем доступность деструктора, если тип классовый
	const DerivedTypeList &dtl = right->GetType().GetDerivedTypeList();
	bool clsType = right->GetType().GetBaseType().IsClassType() &&
		(dtl.GetDerivedTypeCount() == 1 || 
		 (dtl.IsReference() && dtl.GetDerivedTypeCount() == 2) );
	if( clsType )
		ExpressionMakerUtils::ObjectCreationIsAccessible(static_cast<const ClassType&>(
			right->GetType().GetBaseType()), errPos, false, false, true);
	
	return MakeDeleteCall(clsType);
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand PointerToMemberBinaryMaker::Make()
{
	const ClassType *lcls = NULL;	// класс, к члену которого обращаются
	string opName = op == DOT_POINT ? ".*" : "->*";

	// проверяем тип левого операнда,
	if( op == DOT_POINT )
	{
		if( !ExpressionMakerUtils::IsClassType(left->GetType()) )
		{
			theApp.Error(errPos, "'%s' - не классовый тип; '%s' - требует классовый тип",
				left->GetType().GetTypyziedEntityName().c_str(), opName.c_str());
			return ErrorOperand::GetInstance();
		}

		// иначе получаем класс
		lcls = &static_cast<const ClassType&>(left->GetType().GetBaseType());
	}

	// иначе должен быть указатель на класс
	else
	{
		// базовый тип должен быть классовый
		if( !left->GetType().GetBaseType().IsClassType() )
		{
			theApp.Error(errPos, "'%s' - не классовый тип; '%s' - требует классовый тип",
				left->GetType().GetTypyziedEntityName().c_str(), opName.c_str());
			return ErrorOperand::GetInstance();
		}

		// проверяем указатель
		const DerivedTypeList &dtl = left->GetType().GetDerivedTypeList();
		int cnt = dtl.GetDerivedTypeCount();
		if( (dtl.IsPointer() && cnt == 1) || 
			( cnt == 2 && dtl.IsReference() && dtl.GetDerivedType(1)->
			   GetDerivedTypeCode() == DerivedType::DT_POINTER ) )
			lcls = &static_cast<const ClassType&>(left->GetType().GetBaseType());

		// иначе ошибка
		else
		{
			theApp.Error(errPos, "'%s' - левый операнд должен иметь тип '%s'", opName.c_str(),
				(static_cast<const ClassType&>(left->GetType().GetBaseType()).
				 GetQualifiedName().c_str() + string(" *")).c_str());
			return ErrorOperand::GetInstance();
		}
	}

	// после чего проверяем, чтобы второй операнд, был указателем на член
	const DerivedTypeList &dtl = right->GetType().GetDerivedTypeList();
	int cnt = dtl.GetDerivedTypeCount();
	const ClassType *mcls = NULL;
	if( dtl.IsPointerToMember() )
		mcls = &static_cast<const PointerToMember &>(*dtl.GetDerivedType(0)).GetMemberClassType();

	else if( cnt > 1 && dtl.IsReference() && dtl.GetDerivedType(1)->
		   GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
		mcls = &static_cast<const PointerToMember &>(*dtl.GetDerivedType(1)).GetMemberClassType();

	// иначе ошибка
	else
	{
		theApp.Error(errPos, 
			"'%s' - правый операнд должен иметь тип 'указатель на член'", opName.c_str());				
		return ErrorOperand::GetInstance();
	}

	// после чего проверяем, чтобы указатель на член, имел класс такой же как у объекта,
	// либо однозначный доступный базовый
	INTERNAL_IF( mcls == NULL || lcls == NULL );
	if( mcls != lcls )
	{
		DerivationManager dm(*mcls, *lcls);
		if( dm.IsBase() && dm.IsUnambigous() && dm.IsAccessible() )
			;

		// иначе ошибка
		else
		{
			theApp.Error(errPos,
				"'%s' - не является доступным однозначным базовым классом для '%s'",
				mcls->GetQualifiedName().c_str(), lcls->GetQualifiedName().c_str());
			return ErrorOperand::GetInstance();
		}
	}

	// сначала выявляем квалификаторы, от объекта могут передаваться к результирующему типу
	bool rc = right->GetType().IsConst() || left->GetType().IsConst(),
		 rv = right->GetType().IsVolatile() || left->GetType().IsVolatile();

	// строим бинарное выражение
	TypyziedEntity *rtype = new TypyziedEntity((BaseType*)&right->GetType().GetBaseType(),
		rc, rv, right->GetType().GetDerivedTypeList());

	// если есть ссылка, убираем ссылку
	if( rtype->GetDerivedTypeList().IsReference() )
		const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();

	// убираем указатель на член
	const_cast<DerivedTypeList&>(rtype->GetDerivedTypeList()).PopHeadDerivedType();

	// возвращаем выражение
	return new BinaryExpression(op, true, left, right, rtype);
}

// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand TypeidUnaryMaker::Make()
{
	// ищем предопределенную структуру std::type_info
	const ClassType *clsTI = NULL;
	try {
		NameManager nm("std", &GetScopeSystem().GetGlobalSymbolTable(), false);
		if( nm.GetRoleCount() == 0 )
			throw 0;
		INTERNAL_IF( nm.GetRoleCount() != 1 );
		if( nm.GetRoleList().front().second != R_NAMESPACE )
			throw 1;

		// далее в этой области видимости ищем структуру type_info
		NameManager tnm("type_info", 
			static_cast<const NameSpace *>(nm.GetRoleList().front().first), false);
		if( tnm.GetRoleCount() != 1 ||
			tnm.GetRoleList().front().second != R_CLASS_TYPE )
			throw 2;

		// иначе получаем класс и переходим к постройке выражения
		clsTI = dynamic_cast<const ClassType *>(tnm.GetRoleList().front().first);
		INTERNAL_IF( clsTI == NULL );

	} catch( int ) {
		theApp.Error(errPos, 
			"'typeid' - структура std::type_info не объявлена; подключите файл 'typeinfo'");
		return ErrorOperand::GetInstance();
	}

	return new UnaryExpression(KWTYPEID, false, false, right,
		new TypyziedEntity((BaseType*)clsTI, false, false, DerivedTypeList()) );
}


// создать выражение. Может вернуть ErrorOperand, если приведение невозможно
POperand DynamicCastBinaryMaker::Make()
{
	// dynamic_cast<T>(v);
	// T - должен быть ссылкой или указателем на полностью объявленный класс.
	// либо указателем на void.
	// v - должен быть lvalue  или указателем на ПОК соответственно
	const TypyziedEntity &toType = left->GetType(),
						 &fromType = right->GetType();
	
	// проверяем, чтобы квалификация была не меньше
	if( toType.IsConst() < fromType.IsConst() ||
		toType.IsVolatile() < fromType.IsVolatile() )
		theApp.Error(errPos, 
		"'dynamic_cast' - '%s' менее квалифицирован, чем '%s'; преобразование невозможно",
			toType.GetTypyziedEntityName(false).c_str(),
			fromType.GetTypyziedEntityName(false).c_str());	

	// проверим, если левый операнд 'void *', сразу выполним преобразование
	// без дополнительных проверок
	if( toType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID )
	{
		// проверяем, чтобы был указатель на void, и правый операнд был указатель
		// на полностью объявленный полиморфный класс
		const DerivedTypeList &dtl = fromType.GetDerivedTypeList();
		if( toType.GetDerivedTypeList().IsPointer()				&&
			toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
			fromType.GetBaseType().IsClassType()					&&
			( (dtl.GetDerivedTypeCount() == 1 && dtl.IsPointer())	||
			  (dtl.GetDerivedTypeCount() == 2 && dtl.IsReference() &&
			   dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_POINTER) ) )
		{
			// класс должен быть полиморфным и полностью объявлен
			const ClassType &cls = static_cast<const ClassType&>(fromType.GetBaseType());
			if( cls.IsUncomplete() || !cls.IsPolymorphic() )
			{
				theApp.Error(errPos, 
					"'dynamic_cast' - класс '%s' не является %s",
					cls.GetQualifiedName().c_str(), 
					cls.IsUncomplete() ? "завершенным" : "полиморфным");				
				return ErrorOperand::GetInstance();
			}

			// иначе строим выражение
			return new BinaryExpression(KWDYNAMIC_CAST, false, left, right,
				new TypyziedEntity(toType) );	
		}

		// иначе ощибка
		else
		{
			theApp.Error(errPos, 
				"'dynamic_cast' - левый операнд должен быть 'void *'; "
				"правый операнд должен быть указателем на класс");
			return ErrorOperand::GetInstance();
		}
	}

	// иначе проверяем уже классы
	if( !toType.GetBaseType().IsClassType()  ||
		!fromType.GetBaseType().IsClassType() )
	{
		theApp.Error(errPos, "'dynamic_cast' - операнды должны иметь классовый тип");
		return ErrorOperand::GetInstance();
	}

	// проверяем, если ук-ль
	if( toType.GetDerivedTypeList().IsPointer() &&
		toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
	{
		const DerivedTypeList &dtl = fromType.GetDerivedTypeList();
		if( (dtl.GetDerivedTypeCount() == 1 && dtl.IsPointer())	||
			(dtl.GetDerivedTypeCount() == 2 && dtl.IsReference() &&
			 dtl.GetDerivedType(1)->GetDerivedTypeCode() == DerivedType::DT_POINTER) )
			 ;
		else
		{
			theApp.Error(errPos, "'dynamic_cast' - правый операнд должен быть указателем");
			return ErrorOperand::GetInstance();
		}
	}

	// иначе левый должен быть ссылкой, а правый lvalue
	else if( toType.GetDerivedTypeList().IsReference() &&
			  toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		((right->IsExpressionOperand() && static_cast<const Expression&>(*right).IsLvalue()) ||
		 (right->IsPrimaryOperand() && static_cast<const PrimaryOperand&>(*right).IsLvalue()))
		&&  
		fromType.GetDerivedTypeList().GetDerivedTypeCount() <= 1 )
		;

	// иначе ошибка
	else
	{
		theApp.Error(errPos, "'dynamic_cast' - невозможно преобразовать '%s' к '%s'",
			fromType.GetTypyziedEntityName(false).c_str(),
			toType.GetTypyziedEntityName(false).c_str());		
		return ErrorOperand::GetInstance();
	}

	// далее проверяем классы
	const ClassType &fromCls = static_cast<const ClassType&>(fromType.GetBaseType()),
					&toCls = static_cast<const ClassType&>(toType.GetBaseType());

	// проверим, чтобы класс правого операнда был полностью объявлен
	if( fromCls.IsUncomplete() )
	{
		theApp.Error(errPos, 
			"'dynamic_cast' - класс '%s' не полностью объявлен; приведение невозможно",
			fromCls.GetQualifiedName().c_str());
		return ErrorOperand::GetInstance();
	}

	// теперь проверяем возможно статической проверки
	if( toType.GetBaseType().IsClassType() )
	{
		// проверим в первую очередь, чтобы результирующий класс был объявлен
		if( toCls.IsUncomplete() )
		{
			theApp.Error(errPos, 
				"'dynamic_cast' - класс '%s' не полностью объявлен; приведение невозможно",
				toCls.GetQualifiedName().c_str());
			return ErrorOperand::GetInstance();
		}

		// если toCls, является для fromCls однозначным доступным базовым, то 
		// строим явное приведение, через static_cast, иначе продолжаем проверки
		DerivationManager dm(toCls, fromCls);
		if( dm.IsBase() )
		{
			if( !dm.IsAccessible() || !dm.IsUnambigous() )
				theApp.Warning(errPos, 
				"'dynamic_cast' - преобразование в недоступный или неоднозначный базовый класс");

			// строим статическое приведение, т.к. динамическое не требуется
			else
				return new BinaryExpression(KWSTATIC_CAST, 
					toType.GetDerivedTypeList().IsReference(), left, right,
					new TypyziedEntity(toType) );				
		}
	}

	// иначе идет динамическое преобразование, проверяем, чтобы операнд
	// имел тип полиморфного класса
	if( !fromCls.IsPolymorphic() )
	{
		theApp.Error(errPos,
			"'dynamic_cast' - класс '%s' не является полиморфным",
			fromCls.GetQualifiedName().c_str());
		return ErrorOperand::GetInstance(); 
	}

	return new BinaryExpression(KWDYNAMIC_CAST, 
		toType.GetDerivedTypeList().IsReference(), 
		left, right, new TypyziedEntity(toType) );	
}


// возвращает true, если возможно преобразование из lvalue B к cv D&,
// либо из B * к D *
bool StaticCastBinaryMaker::BaseToDerivedExist( 
					const TypyziedEntity &toType, const TypyziedEntity &fromType )
{
	if( !(toType.GetBaseType().IsClassType() && fromType.GetBaseType().IsClassType()) )
		return false;

	// если ссылка, тогда выражение должно быть lvalue
	if( toType.GetDerivedTypeList().IsReference() && 
		toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
		return ExpressionMakerUtils::IsLValue(right);

	// иначе должен быть один указатель
	else if( toType.GetDerivedTypeList().IsPointer() &&
		toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
	{
		return (fromType.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
			   fromType.GetDerivedTypeList().IsPointer()) ||
			   (fromType.GetDerivedTypeList().GetDerivedTypeCount() == 2 &&
			   fromType.GetDerivedTypeList().IsReference() &&
			   fromType.GetDerivedTypeList().GetDerivedType(1)->GetDerivedTypeCode() ==
			   DerivedType::DT_POINTER) ;
	}

	// иначе невозможно
	else
		return false;
}


// проверить преобразование
int StaticCastBinaryMaker::CheckConversion( )
{
	// нельзя приводить к типу массива или функции
	if( left->GetType().GetDerivedTypeList().IsArray() || 
		left->GetType().GetDerivedTypeList().IsFunction() ) 
		return -1;
	
	// сначала пробуем, преобразовать с помощью автоматического преобразования,
	// вида "T t(e)"
	PCaster pCaster= 
		AutoCastManager(left->GetType(), right->GetType(), true, true).RevealCaster();
	Caster &caster = *pCaster;
	caster.ClassifyCast();

	// если преобразование возможно, преобразуем операнды физически и
	// возвращаем 1
	if( caster.IsConverted() )
	{
		caster.DoCast(left, const_cast<POperand&>(right), errPos);
		return 1;
	}

	// иначе пытаемся преобразовать согласно другим правилам
	// возможно преобразование в void
	const TypyziedEntity &toType = left->GetType(), &fromType = right->GetType();
	if( toType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		toType.GetDerivedTypeList().IsEmpty() )
		return 0;

	// из lvalue 'B', в 'cv D &', либо из B * в D *
	if( BaseToDerivedExist(toType, fromType) )
	{	
		// проверяем, чтобы квалификация выражения была меньше
		if( fromType.IsConst() > toType.IsConst() ||
			fromType.IsVolatile() > toType.IsVolatile() )
			return -1;

		// проверяем, чтобы 'B' был однозначным доступным, не виртуальным базовым классом 'D'
		DerivationManager dm( static_cast<const ClassType &>(fromType.GetBaseType()),
			static_cast<const ClassType &>(toType.GetBaseType()) );
		
		// проверяем, чтобы был базовым, не виртуальным, однозначным, доступным
		if( !dm.IsBase() || dm.IsVirtual() || !dm.IsUnambigous() || !dm.IsAccessible() )
			return -1;

		// иначе строим выражение и возвращаем 1
		const_cast<POperand&>(right) = 
			new BinaryExpression( GOC_BASE_TO_DERIVED_CONVERSION, 
				false, left, right, new TypyziedEntity(left->GetType()) );
		return 1;	
	}

	// если оба типа целых, целый тип можно преобразовать в перечислимый
	if( ExpressionMakerUtils::IsIntegral(toType) && ExpressionMakerUtils::IsIntegral(fromType) )
	{
		if( toType.GetBaseType().GetBaseTypeCode() == BaseType::BT_ENUM )
			return 0;
		return -1;
	}

	// указатель на void, можно преобразовать в указатель на др. тип
	if( fromType.GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
		fromType.GetDerivedTypeList().GetDerivedTypeCount() == 1 &&
		fromType.GetDerivedTypeList().IsPointer() ) 
	{
		if( toType.GetDerivedTypeList().IsPointer() &&
			toType.GetDerivedTypeList().GetDerivedTypeCount() == 1 )
			return 0;
	}

	return -1;
}

	
// создать выражение static_cast
POperand StaticCastBinaryMaker::Make()
{
	if( int r = CheckConversion() )
	{
		// если r==1, физическое преобразование не требуется,
		// возвращаем right
		if( r == 1 )
			return right;

		theApp.Error(errPos, "'static_cast' - невозможно преобразовать '%s' к '%s'",
			right->GetType().GetTypyziedEntityName(false).c_str(),
			left->GetType().GetTypyziedEntityName(false).c_str());
		return ErrorOperand::GetInstance();
	}

	bool llv = 
		static_cast<const TypeOperand&>(*left).GetType().GetDerivedTypeList().IsReference();
	return new BinaryExpression
		(KWSTATIC_CAST, llv, left, right, new TypyziedEntity(left->GetType()) );
}


// создать выражение reinterpret_cast
POperand ReinterpretCastBinaryMaker::Make()
{
	const TypyziedEntity &fromType = right->GetType(), &toType = left->GetType();
	const DerivedTypeList &fromDtl = fromType.GetDerivedTypeList(), 
						  &toDtl = toType.GetDerivedTypeList();

	// нельзя приводить к типу массива или функции
	if( toDtl.IsArray() || toDtl.IsFunction() ) 
	{
		theApp.Error(errPos, 
			"'reinterpret_cast' - результирующий тип не может быть массивом или функицей");
		return ErrorOperand::GetInstance();
	}

	// Если один тип целый, а второй указатель или наоборот, либо
	// оба указатели
	if( (ExpressionMakerUtils::IsIntegral(fromType) &&
		 ExpressionMakerUtils::IsRvaluePointer(toType))     ||
		(ExpressionMakerUtils::IsIntegral(toType) &&
		 ExpressionMakerUtils::IsRvaluePointer(fromType))   ||
		(ExpressionMakerUtils::IsRvaluePointer(fromType) &&
		 ExpressionMakerUtils::IsRvaluePointer(toType))	)
		 ;

	// иначе если оба указатели на член, проверяем, чтобы оба были
	// объекты или функции
	else if( toDtl.IsPointerToMember() && fromDtl.IsPointerToMember() &&
			 ((toDtl.GetDerivedTypeCount() > 1 && toDtl.GetDerivedType(1)->
			   GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) +
			  (fromDtl.GetDerivedTypeCount() > 1 && fromDtl.GetDerivedType(1)->
			   GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE)) != 1 )
		;

	// Если правый операнд lvalue, а конечный тип - ссылка,
	// преобразование возможно.
	else if( toType.GetDerivedTypeList().IsReference() &&
		((right->IsExpressionOperand() && static_cast<const Expression&>(*right).IsLvalue()) ||
		 (right->IsPrimaryOperand() && static_cast<const PrimaryOperand&>(*right).IsLvalue())) )
		;

	// иначе приведение невозможно
	else
	{
		theApp.Error(errPos, "'reinterpret_cast' - невозможно преобразовать '%s' к '%s'",
			right->GetType().GetTypyziedEntityName(false).c_str(),
			left->GetType().GetTypyziedEntityName(false).c_str());
		return ErrorOperand::GetInstance();
	}

	// В конце, если результирующий тип ссылка, результат lvalue
	return new BinaryExpression(KWREINTERPRET_CAST, toType.GetDerivedTypeList().IsReference(),
		left, right, new TypyziedEntity(left->GetType()));
}


// создать выражение const_cast. Может вернуть ErrorOperand, если приведение невозможно
POperand ConstCastBinaryMaker::Make()
{ 
	// сравниваем типы
	ScalarToScalarCaster stsc(left->GetType(), right->GetType(), false);
	stsc.ClassifyCast();
	if( !stsc.IsConverted() || stsc.GetCastCategory() != Caster::CC_EQUAL )
	{
		theApp.Error(errPos, "'const_cast' - невозможно преобразовать '%s' к '%s'",
			right->GetType().GetTypyziedEntityName(false).c_str(),
			left->GetType().GetTypyziedEntityName(false).c_str());
		return ErrorOperand::GetInstance();
	}

	// невозможно rvalue, преобразовать к lvalue
	bool llv = 
		static_cast<const TypeOperand&>(*left).GetType().GetDerivedTypeList().IsReference(),
		rlv = ExpressionMakerUtils::IsLValue(right);

	// невозможно преобразовать, если левый lvalue, а правый rvalue
	if( llv && !rlv )
	{
		theApp.Error(errPos, "'const_cast' - невозможно преобразовать rvalue к lvalue");
		return ErrorOperand::GetInstance();
	}

	// результирующий тип - левый
	return new BinaryExpression(KWCONST_CAST, llv, left, right, 
		new TypyziedEntity(left->GetType()) );
}


// создать выражение throw
POperand ThrowUnaryMaker::Make()
{	
	// строим без проверок. Следует заметить, что throw может быть
	// без параметра, тогда right - создается как операнд с типом void
	return new UnaryExpression(KWTHROW, false, false,
		right, new TypyziedEntity(
		(BaseType*)&ImplicitTypeManager(KWVOID).GetImplicitType(), false, false,
		DerivedTypeList()) );	
}

