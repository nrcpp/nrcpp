// реализация транслятора в С-код - Translator.cpp

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
#include "Body.h"
#include "Checker.h"
#include "ExpressionMaker.h"
#include "Translator.h"



// счетчик временных объектов
int TemporaryObject::temporaryCounter = 0;

// генерирует имя области видимости без добавочных символов. Учитывает
// след. области видимости: глобальная, именованная, классовая, функциональная (локальная)
const string &TranslatorUtils::GenerateScopeName( const SymbolTable &scope )
{
	static string rbuf;	
	const SymbolTable *psc = &scope;
	
	// переходим от локальной области видимости к функциональной
	while( psc->IsLocalSymbolTable() )
	{
		psc = &static_cast<const LocalSymbolTable *>(psc)->GetParentSymbolTable();
		INTERNAL_IF( psc->IsGlobalSymbolTable() );
	}

	for( rbuf = "";; )
	{		
		// проверяем, если функциональная, подставляем имя функции к общему имени
		if( psc->IsFunctionSymbolTable() )
		{
			rbuf = static_cast<const FunctionSymbolTable *>(psc)->
				GetFunction().GetName() + '_' + rbuf;
			psc = &static_cast<const FunctionSymbolTable *>(psc)->GetParentSymbolTable();
		}

		// если классовая
		else if( psc->IsClassSymbolTable() )
		{
			rbuf = static_cast<const ClassType *>(psc)->GetName() + '_' + rbuf;
			psc = &static_cast<const ClassType *>(psc)->GetSymbolTableEntry();
		}

		// если именованная
		else if( psc->IsNamespaceSymbolTable() )
		{
			rbuf = static_cast<const NameSpace *>(psc)->GetName() + '_' + rbuf;
			psc = &static_cast<const NameSpace *>(psc)->GetSymbolTableEntry();
		}

		// если глобальная, выходим ничего не добавляя
		else if( psc->IsGlobalSymbolTable() )
			return rbuf;
		
		// иначе неизвестная
		else
			INTERNAL("'TranslatorUtils::GenerateScopeName' - неизвестная область видимости");
	}

	return rbuf;	// kill warning
}


// сгенерировать имя для безимянного идентификатора
string TranslatorUtils::GenerateUnnamed( )
{
	static int unnamedCount = 1;
	CharString dig( unnamedCount++ );
	return string("unnamed") + dig.c_str();
}


// вернуть строковый эквивалент оператора
PCSTR TranslatorUtils::GenerateOperatorName( int op )
{	
	PCSTR opBuf;
	switch( op )
	{
	case KWNEW:				opBuf = "new";		  break;
	case -KWDELETE:		
	case KWDELETE:			opBuf = "delete";	  break;
	case OC_NEW_ARRAY:		opBuf = "newar";	  break;
	case -OC_DELETE_ARRAY:  
	case OC_DELETE_ARRAY:	opBuf = "deletear";	  break;
	case OC_FUNCTION:		opBuf = "fn";		  break;
	case OC_ARRAY:			opBuf = "ar";		  break;
	case PLUS_ASSIGN:		opBuf = "plusasgn";   break;
	case MINUS_ASSIGN:		opBuf = "minasgn";       break;
	case MUL_ASSIGN:		opBuf = "mulasgn";       break;
	case DIV_ASSIGN:		opBuf = "divasgn";       break;
	case PERCENT_ASSIGN:	opBuf = "percasgn";      break;
	case LEFT_SHIFT_ASSIGN:	opBuf = "leftshftasgn";  break;
	case RIGHT_SHIFT_ASSIGN:opBuf = "rightshftasgn"; break;
	case AND_ASSIGN:		opBuf = "andasgn";       break;
	case XOR_ASSIGN:		opBuf = "xorasgn";       break;
	case OR_ASSIGN:			opBuf = "orasgn";		 break;
	case LEFT_SHIFT:		opBuf = "lshft";	     break;
	case RIGHT_SHIFT:		opBuf = "rshft";		 break;
	case LESS_EQU:			opBuf = "lessequ";		 break;
	case GREATER_EQU:		opBuf = "grequ";		 break;
	case EQUAL:				opBuf = "equ";			 break;
	case NOT_EQUAL:			opBuf = "notequ";		 break;
	case LOGIC_AND:			opBuf = "logand";		 break;
	case LOGIC_OR:			opBuf = "logor";		 break;
	case -INCREMENT:
	case INCREMENT:			opBuf = "inc";      break;
	case -DECREMENT:
	case DECREMENT:			opBuf = "dec";      break;
	case ARROW:				opBuf = "arrow";    break;
	case ARROW_POINT:		opBuf = "arpoint";  break;
	case DOT_POINT:			opBuf = "dotpoint"; break;
	case '+':				opBuf = "plus";		break;
	case '-':				opBuf = "minus";	break;
	case '*':				opBuf = "mul";		break;
	case '/':				opBuf = "div";		break;
	case '%':				opBuf = "perc";		break;
	case '^':				opBuf = "xor";		break;
	case '!':				opBuf = "not";		break;
	case '=':				opBuf = "asgn";		break;
	case '<':				opBuf = "less";		break;
	case '>':				opBuf = "greater";  break;	
	case '&':				opBuf = "and";		break;
	case '|':				opBuf = "or";		break;
	case '~':				opBuf = "inv";		break;
	case ',':				opBuf = "coma";		break;
	default:
		INTERNAL("'TranslatorUtils::GenerateOverloadOperatorName' - неизвестный оператор");
		break;
	}
	
	return opBuf;
}


// сгенерировать и вывести декларацию, только если она не typedef и не
// режим приложения не диагностичный
void TranslatorUtils::TranslateDeclaration( const TypyziedEntity &declarator, 
		const PObjectInitializator &iator, bool global )
{
	// если не требуется, не генерируем
	if( theApp.IsDiagnostic()				||
		(declarator.IsObject() &&
		 static_cast<const ::Object &>(declarator).GetStorageSpecifier() ==
		 ::Object::SS_TYPEDEF) )
		return;

	DeclarationGenerator dgen(declarator, iator, global);
	dgen.Generate();
	theApp.GetGenerator().GenerateToCurrentBuffer(dgen.GetOutBuffer());
	theApp.GetGenerator().FlushCurrentBuffer();
}


// транслировать декларацию класса
void TranslatorUtils::TranslateClass( const ClassType &cls )
{
	if( theApp.IsDiagnostic() )
		return;
	
	ClassGenerator cgen( cls );
	cgen.Generate();
	theApp.GetGenerator().GenerateToCurrentBuffer(cgen.GetClassBuffer());
	theApp.GetGenerator().GenerateToCurrentBuffer(cgen.GetOutSideBuffer());				
	theApp.GetGenerator().FlushCurrentBuffer();	
}


// возвращает true, если член имеет классовый тип
bool SMFGenegator::NeedConstructor( const DataMember &dm )
{
	// если не классовый тип, либо спецификатор хранения 
	// указывает, что конструирование члена не требуется внутри класса
	if( !dm.GetBaseType().IsClassType()					||
		dm.GetStorageSpecifier() == ::Object::SS_STATIC ||
		dm.GetStorageSpecifier() == ::Object::SS_TYPEDEF )
		return false;
	
	for( int i = 0; i<dm.GetDerivedTypeList().GetDerivedTypeCount(); i++ )
		if( dm.GetDerivedTypeList().GetDerivedType(i)->
			GetDerivedTypeCode() != DerivedType::DT_ARRAY )
			return false;

	// иначе тип классовый и конструктор необходим
	return true;
}


// заполнить список зависимых классов, заполняется в методе Generate
void SMFGenegator::FillDependClassList()
{
	// сначала вставляем базовые классы	
	int i = 0;
	for( i = 0; i<pClass.GetBaseClassList().GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &pBase = 
			*pClass.GetBaseClassList().GetBaseClassCharacteristic(i);
		
		AddDependClass( &pBase.GetPointerToClass() );
		if( pBase.IsVirtualDerivation() )
			haveVBC = true;
	}

	// после этого заполняем список нестатическими (и не типами) данными членами,
	// которые имеют классовый тип
	for( i = 0; i<pClass.GetMemberList().GetClassMemberCount(); i++ )
		if( const DataMember *dm = dynamic_cast<const DataMember *>(
				&*pClass.GetMemberList().GetClassMember(i)) )
		{
			if( NeedConstructor(*dm) )	
				AddDependClass( static_cast<const ClassType *>(&dm->GetBaseType()) );

			// устанавливаем флаг если в классе есть объекты требующие
			// явной инициализация
			if( (dm->GetStorageSpecifier() != ::Object::SS_STATIC   &&
				 dm->GetStorageSpecifier() != ::Object::SS_TYPEDEF) && 
				(dm->GetDerivedTypeList().IsReference() || 
				 ExpressionMakerUtils::IsConstant(*dm)) )
				explicitInit = true;		
		}
}


// метод проходит по списку зависимых классов и проверяет,
// является ли доступным, однозначным и присутствует ли вообще
// специальный метод. Специальный метод получаем через ук-ль на функцию
// член
bool SMFGenegator::CanGenerate( const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const
{
	// проверяем, каждый класс в списке
	for( SMFManagerList::const_iterator p = sml.begin(); p != sml.end(); p++ )
	{
		const SMFManager::SmfPair &smp = (*p.*GetMethod)();
		
		// проверяем сначала чтобы метод был объявлен (first != NULL)
		// и однозначен (second == false)
		if( !smp.first || smp.second )
			return false;

		// в последнюю очередь проверяем доступность метода
		if( !AccessControlChecker( pClass, static_cast<const ClassType&>(
				smp.first->GetSymbolTableEntry()), *smp.first ).IsAccessible() )
			return false;
	}

	return true;
}



// возвращает true, если метод может быть сгенерирован как тривиальный.
// Вывод деляется на основе списка зависимых классов и полиморфности собственного
SMFGenegator::DependInfo SMFGenegator::GetDependInfo( const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const
{
	DependInfo di;

	// проверяем, каждый класс в списке
	for( SMFManagerList::const_iterator p = sml.begin(); p != sml.end(); p++ )
	{
		const Method &meth = *(*p.*GetMethod)().first;
		INTERNAL_IF( !&meth );
		if( !meth.IsTrivial() )
			di.trivial = false;
					
		if( !meth.GetFunctionPrototype().GetParametrList().IsEmpty() &&
			!meth.GetFunctionPrototype().GetParametrList().GetFunctionParametr(0)->IsConst() )
			di.paramIsConst = false;
	}

	// для тривиальности также требуется, чтобы у класс не был полиморфным,
	// т.е. отсутствовала виртуальная таблица
	di.trivial = di.trivial && !pClass.IsPolymorphic() && !haveVBC;
	return di;
}


// вернуть true, если метод требуется объявить виртуальным
bool SMFGenegator::IsDeclareVirtual(const SMFManagerList &sml, 
			const SMFManager::SmfPair &(SMFManager::*GetMethod)() const ) const
{
	const BaseClassList &bcl = pClass.GetBaseClassList();
	int i = 0;
	for( SMFManagerList::const_iterator p = sml.begin(); 
		 i<bcl.GetBaseClassCount(); i++, p++ )
	{
		INTERNAL_IF( p == sml.end() );
		if( (*p.*GetMethod)().first->IsVirtual() )
			return true;
	}

	return false;
}


// метод возвращает список производных типов с функией без параметров
const DerivedTypeList &SMFGenegator::MakeDTL0() const 
{
	static FunctionParametrList fpl;
	static FunctionThrowTypeList fttl;
	static PDerivedType pfn = new FunctionPrototype(false, false, fpl, fttl, true, false);
	static DerivedTypeList dtl;
	if( dtl.IsEmpty() )
		dtl.AddDerivedType( pfn );
	return dtl;
}


// метод возвращает список производных типов с функией у которой
// в параметре константный или не константая ссылка на этот класс
// и возвращаемым типом ссылкой
const DerivedTypeList &SMFGenegator::MakeDTL1( bool isConst ) const
{		
	static FunctionParametrList fpl;
		
	// создаем параметр
	static DerivedTypeList prmDtl;
	static PDerivedType ref = new Reference;
	if( prmDtl.IsEmpty() )
		prmDtl.AddDerivedType(ref);		

	// вставляем параметр в список
	fpl.ClearFunctionParametrList();
	fpl.AddFunctionParametr( new Parametr(&pClass, isConst, false, prmDtl, "src",
		&pClass, NULL, false) );

	// создаем производный тип функции
	static FunctionThrowTypeList fttl;	
	static DerivedTypeList dtl;
	dtl.ClearDerivedTypeList();
	dtl.AddDerivedType( new FunctionPrototype(false, false, fpl, fttl, true, false) );
	dtl.AddDerivedType( new Reference );
	return dtl;
}


// построить конструктор по умолчанию
ConstructorMethod *SMFGenegator::MakeDefCtor( bool trivial ) const 
{
	CharString name(".");	// имя конструктора начинается всегда с точки
	name += pClass.GetName().c_str();		
		
	DerivedTypeList dtl = MakeDTL0();
	dtl.AddDerivedType( new Reference );
	return new ConstructorMethod( name, &pClass, &pClass,
		false, false, dtl, true,	// inline - true
		Function::SS_NONE, Function::CC_NON, ClassMember::AS_PUBLIC, false, 
		trivial ? ConstructorMethod::DT_TRIVIAL : ConstructorMethod::DT_IMPLICIT );
}


// построить конструктор копирования
ConstructorMethod *SMFGenegator::MakeCopyCtor( bool trivial, bool isConst ) const 
{
	CharString name(".");	// имя конструктора начинается всегда с точки
	name += pClass.GetName().c_str();		
	
	return new ConstructorMethod( name, &pClass, &pClass,
		false, false, MakeDTL1(isConst), true,	// inline - true
		Function::SS_NONE, Function::CC_NON, ClassMember::AS_PUBLIC, false, 
		trivial ? Method::DT_TRIVIAL : Method::DT_IMPLICIT );
}


// построить деструктор
Method *SMFGenegator::MakeDtor( bool trivial, bool isVirtual ) const 
{
	CharString name("~");
	name += pClass.GetName().c_str();

	return new Method( name, &pClass, 
		(BaseType *)&ImplicitTypeManager(KWVOID).GetImplicitType(), false, false, MakeDTL0(),
		true, Function::SS_NONE, Function::CC_NON, ClassMember::AS_PUBLIC, false,
		isVirtual, true, trivial ? Method::DT_TRIVIAL : Method::DT_IMPLICIT );
}


// построить оператор присваивания
ClassOverloadOperator *SMFGenegator::MakeCopyOperator( 
				Method::DT dt, bool isConst, bool isVirtual ) const 
{
	return new ClassOverloadOperator("operator =", &pClass, &pClass, false, false,
		MakeDTL1(isConst), true, Function::SS_NONE, Function::CC_NON, ClassMember::AS_PUBLIC,
		false, isVirtual, '=', "=", dt);
}


// выводит отладочную информацию по методу
void SMFGenegator::DebugMethod( const Method &meth )
{
	string outs = meth.GetTypyziedEntityName().c_str();
	if( meth.IsVirtual() )
		outs = "virtual " + outs;
	if( meth.IsTrivial() )
		outs += ": trivial";

	cout << "* " << outs << endl;
}


// метод генерирует для класса специальные функции члены, если
// это необходимо
void SMFGenegator::Generate()
{
	// проверяем, если нам не нужно ничего генерировать, выходим
	if( !pClass.GetConstructorList().empty()  &&
		smfManager.GetCopyConstructor().first &&
		smfManager.GetDestructor().first	  &&
		smfManager.GetCopyOperator().first  )
		return;

	// иначе заполняем список зависимых классов текущего класса
	FillDependClassList();

	// после заполнения списка зависимых классов, формируем на его
	// основе список менеджеров этих классов
	SMFManagerList dependClsManagers;
	for( ClassTypeList::const_iterator p = dependClsList.begin();
		 p != dependClsList.end(); p++ )
		dependClsManagers.push_back( SMFManager(**p) );

	// проверяем, нужен ли нам конструктор по умолчанию,
	// в случае если конструкторов не объявлено, продолжаем генерацию
	if( pClass.GetConstructorList().empty() )
	{
		// проверяем, можно ли сгенерировать к-ор по умолчанию для нашего класса
		if( CanGenerate(dependClsManagers, SMFManager::GetDefaultConstructor) && !explicitInit )
			pClass.InsertSymbol( MakeDefCtor( 
				GetDependInfo(dependClsManagers, SMFManager::GetDefaultConstructor).trivial) );
	}

	// если копирующий конструктор не сгенерирован, генерируем
	if( smfManager.GetCopyConstructor().first == NULL )
	{
		if( CanGenerate(dependClsManagers, SMFManager::GetCopyConstructor) && !explicitInit )
		{
			DependInfo di = GetDependInfo(dependClsManagers, SMFManager::GetCopyConstructor);
			pClass.InsertSymbol( MakeCopyCtor(di.trivial, di.paramIsConst) );
		}
	}

	// если необх. сгенерировать деструктор
	if( smfManager.GetDestructor().first == NULL )
	{
		if( CanGenerate(dependClsManagers, SMFManager::GetDestructor) )
		{
			DependInfo di = GetDependInfo(dependClsManagers, SMFManager::GetDestructor);
			bool isVirtual = IsDeclareVirtual(dependClsManagers, SMFManager::GetDestructor);
			pClass.InsertSymbol( MakeDtor(di.trivial, isVirtual) );
		}
	}

	// если необх. сгенерировать оператор присваивания
	if( smfManager.GetCopyOperator().first == NULL )
	{
		// помимо стандартных проверок, также проверяем, чтобы в классе
		// не было ссылок и константных объектов. В этом случае оператор
		// копирования не генерируется
		if( CanGenerate(dependClsManagers, SMFManager::GetCopyOperator) && !explicitInit )
		{						
			DependInfo di = GetDependInfo(dependClsManagers, SMFManager::GetCopyOperator);			
			bool isVirtual = IsDeclareVirtual(dependClsManagers, SMFManager::GetCopyOperator);
			pClass.InsertSymbol( MakeCopyOperator( (di.trivial ?
				Method::DT_TRIVIAL : Method::DT_IMPLICIT), di.paramIsConst, isVirtual) );
		}

		// иначе нам все равно следует сгенерировать оператор, но пометить
		// его как недоступный
		else
			pClass.InsertSymbol( MakeCopyOperator(Method::DT_UNAVAIBLE, false, false) );
	}
}


// сгенерировать специальные функции члены, которые не заданы
// явно пользователем
void ClassParserImpl::GenerateSMF()
{
	SMFGenegator(*clsType).Generate();
}


// сгенерировать С-тип в выходной буфер
void CTypePrinter::Generate( )
{	
	BaseType::BT bt = type.GetBaseType().GetBaseTypeCode();
	
	// const - игнорируем, volatile оставляем
	if( type.IsVolatile() )
		baseType = "volatile ";

	// если базовый тип встроенный, печатаем его. bool в начале программы 
	// подключается через файл stdbool.h
	if( bt == BaseType::BT_BOOL    || bt == BaseType::BT_CHAR   ||
		bt == BaseType::BT_WCHAR_T || bt == BaseType::BT_INT    || 
		bt == BaseType::BT_FLOAT   || bt == BaseType::BT_DOUBLE ||
		bt == BaseType::BT_VOID )
		baseType += ImplicitTypeManager(type.GetBaseType()).GetImplicitTypeName().c_str();

	// иначе печатаем имя класса
	else
	{
		INTERNAL_IF( bt != BaseType::BT_CLASS	 &&
					 bt != BaseType::BT_STRUCT	 &&
					 bt != BaseType::BT_ENUM	 &&
					 bt != BaseType::BT_UNION );
		
		// если перечисление, заменяем на int
		if( bt == BaseType::BT_ENUM )
			baseType += "int";

		// иначе структура или объединение
		else
		{
			baseType += (bt == BaseType::BT_UNION ? "union " : "struct ");
		
			// прибавляем С-имя. Учитываем, что С-имя сгенерировано в любом случае
			baseType += static_cast<const ClassType &>(type.GetBaseType()).GetC_Name();
		}
	}
	
	// если имеем конструктор, конструктор возвращает указатель на передаваемый
	// объект. Соотв. в каждом конструкторе следует добавить "return this;" 
	// в конце тела и там где операция return
	if( type.IsFunction() && static_cast<const Function &>(type).IsClassMember() &&
		static_cast<const Method &>(type).IsConstructor() )
		baseType += " *";

	if( type.GetDerivedTypeList().GetDerivedTypeCount() != 0 )
	{
		string dtlBuf;
		int ix = 0;
		bool namePrint = id == NULL;

		PrintPointer(dtlBuf, ix, namePrint);		
		outBuffer = baseType + ' ' + dtlBuf;
	}

	else if( id != NULL )		
		outBuffer = baseType + ' ' + id->GetC_Name();

	// удаляем пробелы
	if( outBuffer[outBuffer.length()-1] == ' ' )
		outBuffer.erase(outBuffer.end()-1);
	if( outBuffer[0] == ' ' )
		outBuffer.erase(outBuffer.begin());	
}


// печатать префиксные производные типы и сохранять их в буфер
void CTypePrinter::PrintPointer( string &buf, int &ix, bool &namePrint )
{
	bool isPrint = false;
	for( ; ix < type.GetDerivedTypeList().GetDerivedTypeCount(); ix++, isPrint++ )
	{
		const DerivedType &dt = *type.GetDerivedTypeList().GetDerivedType(ix);
		DerivedType::DT dtc = dt.GetDerivedTypeCode();

		// ссылка и указатель обозначаются одним символом
		if( dtc == DerivedType::DT_REFERENCE || dtc == DerivedType::DT_POINTER )
			buf = '*' + buf;

		// меняем базовый тип на int и выходим из цикла
		else if( dtc == DerivedType::DT_POINTER_TO_MEMBER )
		{	
			// если имеем указатель на член-функцию, преобразуем его в указатель
			// на функцию, 
			if( ix != type.GetDerivedTypeList().GetDerivedTypeCount()-1 &&
				type.GetDerivedTypeList().GetDerivedType(ix+1)->GetDerivedTypeCode() ==
				DerivedType::DT_FUNCTION_PROTOTYPE )
			{
				buf = '*' + buf;
				INTERNAL_IF( !thisBuf.empty() );
				PrintThis( static_cast<const PointerToMember&>(dt).GetMemberClassType(),thisBuf);
			}
			
			// иначе преобразуем тип в int и выходим
			else
			{
				baseType = "int";
				ix = type.GetDerivedTypeList().GetDerivedTypeCount();
				break;
			}
		}

		else
		{
			// если имя еще не было распечатано, печатаем его
			if( !namePrint )
			{				
				buf = buf + id->GetC_Name();
				namePrint = true;
			}

			if( isPrint )			
				buf = '(' + buf + ')';			

			PrintPostfix( buf, ix );			
		}
	}

	// если имя так и не было напечатано, печатаем	
	if( !namePrint )
	{				
		buf = buf + id->GetC_Name();
		namePrint = true;
	}
}


// печатать постфиксные производные типы и сохранять в буфер
void CTypePrinter::PrintPostfix( string &buf, int &ix )
{
	for( ; ix < type.GetDerivedTypeList().GetDerivedTypeCount(); ix++)
	{
		const DerivedType &dt = *type.GetDerivedTypeList().GetDerivedType(ix);
		DerivedType ::DT dtc = dt.GetDerivedTypeCode();

		if( dtc == DerivedType::DT_FUNCTION_PROTOTYPE )
		{
			const FunctionPrototype &fp =  static_cast<const FunctionPrototype &>(dt);
			buf += '(';

			// если задан this-буфер, значит следует вывести параметр this для
			// метода и очистить буфер
			if( !thisBuf.empty() )
			{
				buf += thisBuf;
				thisBuf = "";
				if( !fp.GetParametrList().IsEmpty() || fp.IsHaveEllipse() )
					buf += ", ";
			}

			for( int i = 0;	i<fp.GetParametrList().GetFunctionParametrCount(); i++ )
			{
				const Parametr &prm = *fp.GetParametrList().GetFunctionParametr(i);

				// если печатаем декларацию (задано имя), проверяем, чтобы у параметра
				// также было задано имя				
				CTypePrinter prmPrinter(prm, id ? &prm : NULL);
				prmPrinter.Generate();

				// прибавляем распечатанный параметр
				if( prm.IsHaveRegister() )
					buf += "register ";
				buf += prmPrinter.GetOutBuffer();
							
				if( i < fp.GetParametrList().GetFunctionParametrCount()-1 )
					buf += ", ";
			}						
			
			if( fp.IsHaveEllipse() )			
				buf += i == 0 ? "..." : ", ...";
			buf += ')';		
		}

		else if( dtc == DerivedType::DT_ARRAY )		
		{
			if( dt.GetDerivedTypeSize() > 0 )
				buf = buf + '[' + CharString( dt.GetDerivedTypeSize() ).c_str() + ']';
			else
				buf += "[]";
		}

		// иначе опять следует печатать указатели, только сначала
		else
		{
			INTERNAL_IF( dtc != DerivedType::DT_REFERENCE &&
						 dtc != DerivedType::DT_POINTER &&
						 dtc != DerivedType::DT_POINTER_TO_MEMBER );

			bool nm = true;
			PrintPointer(buf, ix, nm);						 
		}
	}
}


// задать информацию генерации для виртуального метода
Method::VFData::VFData( int vtIndex, const Method &thisMeth )
	: vtIndex(vtIndex), 
	rootVfCls(static_cast<const ClassType &>(thisMeth.GetSymbolTableEntry())) 
{
	INTERNAL_IF( !thisMeth.IsVirtual() );
	PTypyziedEntity ent = new TypyziedEntity(thisMeth);
	INTERNAL_IF( !ent->GetDerivedTypeList().IsFunction() );
	const_cast<DerivedTypeList &>(ent->GetDerivedTypeList()).PushHeadDerivedType(
		new Pointer(false, false));
	
	CTypePrinter ctp(*ent, thisMeth);
	castBuf = '(' + (ctp.Generate(), ctp.GetOutBuffer()) + ')';
}


// сгенерировать заголовок класса
string ClassGenerator::GenerateHeader( const ClassType &cls )
{
	string header;
	header = cls.GetBaseTypeCode() == BaseType::BT_UNION ? "union " : "struct ";
	header += cls.GetC_Name();

	// добавить заголовок к выходному буферу
	return header;
}


// сгенерировать базовые подобъекты (классы)
void ClassGenerator::GenerateBaseClassList( const BaseClassList &bcl )
{
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &bcc = *bcl.GetBaseClassCharacteristic(i);
		const ClassType &pbcls = bcc.GetPointerToClass();
		string subObjBuf = '\t' + GenerateHeader(pbcls);

		// если виртуальное наследование, прибавляем указатель
		if( bcc.IsVirtualDerivation() )
			subObjBuf += " *";

		// генерируем имя подобъекта
		subObjBuf += ' ' + pbcls.GetC_Name() + "so;\n";
		clsBuffer += subObjBuf;
	}
}


// сгенерировать список данных-членов и методов
void ClassGenerator::GenerateMemberList( const ClassMemberList &cml )
{
	for( int i = 0; i<cml.GetClassMemberCount(); i++ )
	{
		const ClassMember &member = *cml.GetClassMember(i);		

		// если данное-член, генерируем член
		if( const DataMember *pobj = dynamic_cast<const DataMember *>(&member) )
		{
			// typedef-не генерируем
			if( pobj->GetStorageSpecifier() == ::Object::SS_TYPEDEF )
				continue;

			// иначе сгенерируем определение			
			CTypePrinter tp( *pobj, pobj );
			tp.Generate();

			// если static, генерируем за пределами класса, как extern-член
			if( pobj->GetStorageSpecifier() == ::Object::SS_STATIC )			
			{
				string mbuf = tp.GetOutBuffer();
				// если есть инициализатор, задаем
				if( pobj->IsConst() && pobj->GetObjectInitializer() )
					mbuf = "const " + mbuf + " = " + 
						CharString( (int)*pobj->GetObjectInitializer() );

				// иначе объявляем как extern
				else
					mbuf = "extern " + mbuf;

				outSideBuffer += mbuf + ";\n";
			}

			// если битовое поле
			else if( pobj->IsBitField() )
			{
				const double *v = pobj->GetObjectInitializer();
				INTERNAL_IF( !v );
				string mbuf = '\t' + tp.GetOutBuffer() + " : " + 
					CharString( (int)*v ).c_str() + ";\n";
				clsBuffer += mbuf;
			}

			// иначе генерируем как обычный член
			else
				clsBuffer += '\t' + tp.GetOutBuffer() + ";\n";				
		}

		// если метод, генерируем его за пределами класса
		else if( const Method *pmeth = dynamic_cast<const Method *>(&member) )
		{
			// если метод тривиальный или недоступный или абстрактный,
			// не генерируем его
			if( pmeth->IsTrivial() || pmeth->IsAbstract() || pmeth->IsUnavaible() )
				continue;

			CTypePrinter tp( *pmeth, *pmeth, 
				pmeth->GetStorageSpecifier() != Function::SS_STATIC );
			tp.Generate();
			const string &mbuf = tp.GetOutBuffer();
			if( pmeth->IsInline() )
				const_cast<string &>(mbuf) = "inline " + mbuf;
			outSideBuffer += tp.GetOutBuffer() + ";\n";
		}
	}
}


// рекурсиный метод вызываемый из GenerateVTable
void ClassGenerator::GenerateVTable( const ClassType &cls, string *fnArray, int fnArraySize )
{
	const VirtualFunctionList &vfl = cls.GetVirtualFunctionList();
	for( VirtualFunctionList::const_iterator p = vfl.begin(); p != vfl.end(); p++ )
	{
		INTERNAL_IF( *p == NULL );
		const Method::VFData &vd = (*p)->GetVFData();
		INTERNAL_IF( vd.GetVTableIndex() >= fnArraySize );

	}
}


// сгенерировать таблицу виртуальных функций для данного класса,
// учитывая базовые классы
void ClassGenerator::GenerateVTable( )
{
	const int vfcnt = pClass.GetVirtualFunctionCount();
	// если виртуальных функций в классе нет, строить виртуальную 
	// таблицу не требуется
	if( vfcnt == 0 )
		return;
	string *fnBuf = new string[vfcnt];
	GenerateVTable( pClass, fnBuf, vfcnt);

	// постусловие, чтобы все элементы буфера были заполнены
	for( int i = 0; i<vfcnt; i++ )
		INTERNAL_IF( fnBuf[i].empty() );
	delete [] fnBuf;
}


// сгенерировать определение класса и зависимую информацию
void ClassGenerator::Generate()
{
	// генерируем заголовок
	clsBuffer = GenerateHeader(pClass);

	// если класс не полностью объявлен, генерируем только заголовок и выходим
	if( pClass.IsUncomplete() )
	{
		clsBuffer += ';';
		return;
	}

	// добавляем '{'
	clsBuffer += "\n{\n";
	int prevBufSize = clsBuffer.size();

	// генерируем базовые подобъекты
	GenerateBaseClassList( pClass.GetBaseClassList() );
	// генерируем члены
	GenerateMemberList( pClass.GetMemberList() );

	// здесь, проверяем, если буфер содержит только "\n{\n",
	// значит в него ничего не сгенерировано и следует сгенерировать
	// поле, чтобы класс не был пустым
	if( clsBuffer.size() == prevBufSize )
		clsBuffer += "\tint _;";

	// закрываем скобку
	clsBuffer += "\n};\n\n";
}


// сгенерировать, инициализацию конструктором. Возвращает буфер с вызовом
// конструктора. Не рассматривает тривиальные конструкторы
void DeclarationGenerator::GenerateConstructorCall( const ::Object &obj,
		const ConstructorInitializator &ci, string &out )
{
	INTERNAL("!!! распечатать вызов конструктора и регистрацию объекта");
	INTERNAL_IF( !ci.GetConstructor() || ci.GetConstructor()->IsTrivial() );
}


// сгенерировать выражение из инициализатора. В списке выражение должно быть одно.
// Если выражение является интерпретируемым и может использоваться как прямой
// инициализатор глобального объекта, вернуть true, иначе false
bool DeclarationGenerator::GenerateExpressionInit(  const ::Object &obj,
		const ConstructorInitializator &ci, string &out )
{
	INTERNAL_IF( ci.GetExpressionList().IsNull() || ci.GetExpressionList()->size() > 1 );
	if( ci.GetExpressionList()->empty() )
		return true;

	// иначе имеем один инициализатор, распечатаем его
	const POperand &exp = ci.GetExpressionList()->front();
	INTERNAL_IF( !(exp->IsExpressionOperand() || exp->IsPrimaryOperand()) );

	// если инициализатор может использоваться как прямой
	// для глобального объекта установлен в true
	bool directIator = false;

	// если имеем основной операнд, проанализируем, является ли он интерпретируемым
	if( exp->IsPrimaryOperand() )
	{
		// проверка, является ли операнд интерпретируемым
		double rval;
		if( ExpressionMakerUtils::IsInterpretable(exp, rval) ||
			exp->GetType().IsLiteral() )
			directIator = true;		
	}

	INTERNAL(" !!!! распечатаем все выражение в выходной буфер");

	return directIator;
}


// сгенерировать инициализатор для объекта. Если инициализатор возможно
// сгенерировать как прямой, генерируем. Иначе генерируем в буфер косвенной
// инициализации
void DeclarationGenerator::GenerateInitialization( const ::Object &obj )
{
	INTERNAL_IF( iator.IsNull() );

	// если инициализатор конструктором (либо значением)
	if( iator->IsConstructorInitializator() )
	{
		const ConstructorInitializator &oci = 
			static_cast<const ConstructorInitializator &>(*iator);
		string out;

		// если инициализация выражением
		if( !oci.GetConstructor() || oci.GetConstructor()->IsTrivial() )
		{			
			bool directIator = GenerateExpressionInit(obj, oci, out);
			if( !out.empty() )
			{
				// если сгенерирован прямой инициализатор или имеем локальный объект,
				// генерируем инициализацию через равно
				if( directIator || !global )
					outBuffer += " = " + out;			

				// иначе генерируем в буфер косвенной инициализации
				else
					indirectInitBuf = obj.GetC_Name() + " = " + out;
			}
		}

		// иначе, инициализируем конструктором
		else
			GenerateConstructorCall(obj, oci, out);		
	}

	// иначе инициализатор является списочным
	else
	{
		INTERNAL("не реализовано");
	}
}


// сгенерировать декларацию
void DeclarationGenerator::Generate()
{
	// распечатаем декларацию объекта
	if( declarator.IsObject() )
	{
		const ::Object &obj = static_cast<const ::Object &>(declarator);
		CTypePrinter ctp( declarator, &obj );
		ctp.Generate();
		outBuffer += ctp.GetOutBuffer();

		// добавим к декларации спецификатор хранения, если есть.
		// Для выходного кода имеет значение только extern, static и register
		if( (obj.GetStorageSpecifier() == ::Object::SS_STATIC &&
			 !obj.GetSymbolTableEntry().IsClassSymbolTable())  || 
			obj.GetStorageSpecifier() == ::Object::SS_EXTERN   ||
			(!global && obj.GetStorageSpecifier() == ::Object::SS_REGISTER) )
			outBuffer = ManagerUtils::GetObjectStorageSpecifierName( 
				obj.GetStorageSpecifier() ) + ' ' + outBuffer;

		// если инициализатор присутствует, сгенерируем инициализацию для объекта
		if( !iator.IsNull() )
			GenerateInitialization(obj);
	}
		
	// распечатаем декларацию функции
	else
	{
		// у функции не должно быть инициализатора
		INTERNAL_IF( !iator.IsNull() );
		const Function &fn = static_cast<const Function &>(declarator);
		CTypePrinter ctp( declarator, fn, false );
		ctp.Generate();
		outBuffer += ctp.GetOutBuffer();

		if( fn.GetStorageSpecifier() == Function::SS_STATIC )
			outBuffer = "static " + outBuffer;
		else if( fn.GetStorageSpecifier() == Function::SS_STATIC )
			outBuffer = "extern " + outBuffer;

		// добавим спецификатор inline
		if( fn.IsInline() )
			outBuffer = "inline " + outBuffer;
	}

	// в конце добавляем ';'
	outBuffer += ";\n";
}


// создать новый временный объект
const TemporaryObject &TemporaryManager::CreateTemporaryObject( const TypyziedEntity &type )
{
	// если имеем неиспользуемые объекты, вернем их
	if( unUsed > 0 )
	{
		for( TemporaryList::iterator p = temporaryList.begin();
			 p != temporaryList.end(); p++ )
			if( !(*p).IsUsed() )
				return (*p).SetUsed(), unUsed--, (*p);
		INTERNAL("ошибка в подсчете неиспользуемых временных объектов");
	}

	// иначе создаем временный объект вручную и генерируем декларацию
	INTERNAL_IF( !genBuffer.empty() );
	TemporaryObject temporary(type);
	static LocalSymbolTable lst(GetScopeSystem().GetGlobalSymbolTable());
	Identifier id( temporary.GetName().c_str(), &lst );

	// генерируем декларацию
	CTypePrinter ctp( temporary.GetType(), &id );
	genBuffer = (ctp.Generate(), ctp.GetOutBuffer());

	// вставляем временный объект в список
	temporaryList.push_front(temporary);
	return temporaryList.front();
}


// освободить временный объект
void TemporaryManager::FreeTemporaryObject( TemporaryObject &tobj ) 
{
	tobj.SetUnUsed();
	unUsed++;

	// если объект имеет классовый тип и не тривиальный деструктор,
	// вызовем функцию уничтожения объекта
	const TypyziedEntity &type = tobj.GetType();
	if( type.GetBaseType().IsClassType() && type.GetDerivedTypeList().IsEmpty() &&
		static_cast<const ClassType &>(type.GetBaseType()).GetDestructor() &&
		!static_cast<const ClassType &>(type.GetBaseType()).GetDestructor()->IsTrivial() )
	{
		// буфер должен быть пустым при освобождении объекта
		INTERNAL_IF( !genBuffer.empty() );
		genBuffer = "__destroy_last_registered_object();";
	}
}


// рекурсивный метод генерации пути
bool ExpressionGenerator::PrintPathToBase( 
		const ClassType &cur, const ClassType &base, string &out )
{
	const BaseClassList &bcl = cur.GetBaseClassList();
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &bcc = *bcl.GetBaseClassCharacteristic(i);

		// если классы совпали, печатаем и выходим
		if( &bcc.GetPointerToClass() == &base )
		{			
			out += base.GetC_Name() + "so" + (bcc.IsVirtualDerivation() ? "->" : ".");
			return true;
		}

		// иначе проверяем, если класс находится выше в иерархии, также добавляем его		
		else if( PrintPathToBase(bcc.GetPointerToClass(), base, out) )
		{
			// добавляем подобъект спереди
			out = bcc.GetPointerToClass().GetC_Name() + "so" + 
				(bcc.IsVirtualDerivation() ? "->" : ".") + out;			
			return true;
		}
	}

	return false;
}


// сгенерировать путь от текущего класса к базовому. Если классы совпадают,
// ничего не генерировать. Если базовый класс отсутствует в иерархии - внутренняя ошибка
const string &ExpressionGenerator::PrintPathToBase( const ClassType &cur, const ClassType &base )
{
	static string out;
	out = "";
	
	// если изначально заданные классы совпали, ничего не печатаем
	if( &cur == &base )	
		return out;	

	INTERNAL_IF( !PrintPathToBase(cur, base, out) );
	return out;
}


// распечатать основной операнд. Флаг printThis указывает на автоматическую
// подстановку указателя this для нестатических и не typedef данных членов.
// Возвращает действительную типизированную сущность для дальнейшего анализа
const TypyziedEntity &ExpressionGenerator::PrintPrimaryOperand( 
		const PrimaryOperand &po, bool printThis, string &out )
{
	const TypyziedEntity &entity = po.GetType().IsDynamicTypyziedEntity() ?
		static_cast<const DynamicTypyziedEntity &>(po.GetType()).GetOriginal() : po.GetType();

	// если объект
	if( entity.IsObject() )
	{
		const ::Object &obj = static_cast<const ::Object &>(entity);

		// в любом случае распечатаем полный путь к члену и если требуется
		// распечатаем this
		if( obj.IsClassMember()										&& 
			!static_cast<const DataMember &>(obj).IsStaticMember()  )
		{
			INTERNAL_IF( thisCls == NULL );
			out = (printThis ? "this->" : "") + PrintPathToBase(*thisCls, 
				static_cast<const ClassType &>(obj.GetSymbolTableEntry()) ) + obj.GetC_Name();
		}

		// иначе распечатываем просто имя
		else
			out = obj.GetC_Name();
	}

	// если функция, просто подставляем имя
	else if( entity.IsFunction() )
		out = static_cast<const Function &>(entity).GetC_Name();
	
	// если параметр подставляем имя
	else if( entity.IsParametr() )
		out = static_cast<const Parametr &>(entity).GetC_Name();

	// если литерал, подставляем буфер с литералом
	else if( entity.IsLiteral() )	
		out = static_cast<const Literal &>(entity).GetLiteralValue().c_str();		

	// если константа перечисления, подставляем целое значение
	else if( entity.IsEnumConstant() )	
		out = CharString(static_cast<const EnumConstant &>(entity).GetConstantValue()).c_str();	

	// в противном случае остается только this. Проверим, чтобы this-класс 
	// присутствовл и типы совпадали
	else
	{
		INTERNAL_IF( thisCls == NULL || &entity.GetBaseType() != thisCls ||
			!entity.GetDerivedTypeList().IsPointer() || 
			entity.GetDerivedTypeList().GetDerivedTypeCount() != 1 );
		out = "this";
	}

	return entity;
}


// распечатать унарное выражение
void ExpressionGenerator::PrintUnary( const UnaryExpression &ue, string &out )
{
	int op = ue.GetOperatorCode();
	const POperand &opr = ue.GetOperand();
	switch( op )
	{
	case '!':  
	case '~':  
	case '+':  
	case '-': 
	case '*':
		out = (char)op + PrintExpression(opr);
		break;
	case INCREMENT:
		out = PrintExpression(opr) + "++";
		break;
	case DECREMENT:
		out = PrintExpression(opr) + "--";
		break;
	case -INCREMENT:
		out = "++" + PrintExpression(opr);
		break;
	case -DECREMENT:
		out = "--" + PrintExpression(opr);
		break;	
	case GOC_REFERENCE_CONVERSION:		
		// пропускаем разименование, т.к. генератор сделает это автоматически
		out = PrintExpression(opr);
		break;
	case '&':
		// если имеем взятие указателя на член, генерируем offsetof,		
		if( opr->IsPrimaryOperand() && 
			ue.GetType().GetDerivedTypeList().IsPointerToMember() )
		{
			const DataMember *dm = dynamic_cast<const DataMember *>(&opr->GetType());
			INTERNAL_IF( dm == NULL );
			out = "offsetof(" + TranslatorUtils::GenerateClassHeader(
				static_cast<const ClassType &>(dm->GetSymbolTableEntry()) ) + ", ";

			string dmBuf;
			PrintPrimaryOperand( static_cast<const PrimaryOperand&>(*opr), false, dmBuf);
			out += dmBuf + ')';
		}

		// иначе оставляем без изменений
		else
			out = "&" + PrintExpression(opr);
		break;
	case KWTYPEID:
	case KWTHROW:
	default:
		INTERNAL( "'ExpressionGenerator::PrintUnary': неизвестный унарный оператор");
	}
}


// распечатать унарное выражение
void ExpressionGenerator::PrintBinary( const BinaryExpression &be, string &out )
{
	int op = be.GetOperatorCode();	
	switch( op )
	{
	case '.':
	case ARROW:
		{
		INTERNAL_IF( !be.GetOperand2()->IsPrimaryOperand() );
		out = PrintExpression(be.GetOperand1()) + (op == '.' ? "." : "->");
		string idBuf;
		PrintPrimaryOperand( static_cast<const PrimaryOperand &>(*be.GetOperand2()), 
			false, idBuf);
		out += idBuf;
		break;
		}
	case OC_CAST:
	case GOC_BASE_TO_DERIVED_CONVERSION:
	case GOC_DERIVED_TO_BASE_CONVERSION:
	case KWREINTERPRET_CAST:
	case KWSTATIC_CAST:
	case KWCONST_CAST:
	case KWDYNAMIC_CAST:
	case ARROW_POINT:
	case DOT_POINT:
		INTERNAL("!! не реализовано");
	case OC_ARRAY:
		out = '(' + PrintExpression(be.GetOperand1()) + ")[" +
			PrintExpression(be.GetOperand2() ) + ']';
		break;
	default:
		{
		string opbuf = ExpressionPrinter::GetOperatorName(op);
		out = PrintExpression(be.GetOperand1() ) + ' ' + 
			opbuf + ' ' + PrintExpression(be.GetOperand2());
		}
		break;
	}
}


// распечатать унарное выражение
void ExpressionGenerator::PrintTernary( const TernaryExpression &te, string &out )
{
	INTERNAL_IF( te.GetOperatorCode() != '?' );
	out = PrintExpression(te.GetOperand1()) + " ? " + 
		PrintExpression(te.GetOperand2()) + " : " + PrintExpression(te.GetOperand3());
}


// распечатать унарное выражение
void ExpressionGenerator::PrintFunctionCall( const FunctionCallExpression &fce, string &out )
{
	const POperand &fn = fce.GetFunctionOperand();
	string objBuf;

	// если выражение - обращение к члену, поместить объект первым параметром
	if( fn->IsExpressionOperand() )
	{
		int op = static_cast<const Expression &>(*fn).GetOperatorCode();
		if( op == '.' || op == ARROW || op == DOT_POINT || op == ARROW_POINT )
			objBuf = PrintExpression(static_cast<const BinaryExpression &>(*fn).GetOperand1());
	}
}


// распечатать унарное выражение
void ExpressionGenerator::PrintNewExpression( const NewExpression &ne, string &out )
{
}


// координатор распечатки выражений, вызывает метод соответствующий оператору.
// prvOp - код предыдущего оператора, если оператора не было -1
string ExpressionGenerator::PrintExpression( const POperand &opr )
{
	INTERNAL_IF( opr.IsNull() || !(opr->IsExpressionOperand() || opr->IsPrimaryOperand()) );
	string out;

	// если был оператор обращения к члену или взятия адреса на член,
	// этот фрагмент выполняется при распечатке этих операторов
	if( opr->IsPrimaryOperand() )	
		PrintPrimaryOperand( static_cast<const PrimaryOperand &>(*opr), true, out );		

	// иначе ориентируемся на тип выражения
	else 
	{
		const Expression &exp = static_cast<const Expression &>(*opr);
		if( exp.IsUnary() )
			PrintUnary( static_cast<const UnaryExpression &>(exp), out );
		else if( exp.IsBinary() )
			PrintBinary( static_cast<const BinaryExpression &>(exp), out );
		else if( exp.IsTernary() )
			PrintTernary( static_cast<const TernaryExpression &>(exp), out );
		else if( exp.IsFunctionCall() )
			PrintFunctionCall( static_cast<const FunctionCallExpression&>(exp), out );
		else if( exp.IsNewExpression() )
			PrintNewExpression( static_cast<const NewExpression &>(exp), out );
		else
			INTERNAL( "неизвестный тип выражения" );

		// если выражение в скобках, поместить его
		if( exp.IsInCramps() )
			out = '(' + out + ')';
	}

	// если была ссылка, разименовываем
	if( opr->GetType().GetDerivedTypeList().IsReference() )
		out = "(*" + out + ')';
	return out;
}


// сгенерировать выражение
void ExpressionGenerator::Generate()
{
	// если выражение является основным операндом, сразу генерируем
	if( exp->IsPrimaryOperand() )
		PrintPrimaryOperand( static_cast<const PrimaryOperand &>(*exp), true, outBuffer );
	else
		outBuffer = PrintExpression(exp);
}
