// реализация методов классов идентификатор, типизированная сущность и 
// производных от него - TypyziedEntity.cpp

#pragma warning(disable: 4786)
#include <nrc.h>
using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "Object.h"
#include "Scope.h"
#include "LexicalAnalyzer.h"
#include "Class.h"
#include "Manager.h"


// утилиты транслятора
namespace TranslatorUtils
{
	// генерирует имя области видимости без добавочных символов. Учитывает
	// след. области видимости: глобальная, именованная, классовая, функциональная (локальная)
	const string &GenerateScopeName( const SymbolTable &scope );

	// сгенерировать имя для безимянного идентификатора
	string GenerateUnnamed( );

	// вернуть строковый эквивалент оператора
	PCSTR GenerateOperatorName( int op );
}


// получить квалифицированное имя идентификатора
NRC::CharString Identifier::GetQualifiedName() const
{
	string res;
	INTERNAL_IF( pTable == NULL );
	
	// если локальная или глобальная, то имя не может быть квалифицированным
	if( pTable->IsGlobalSymbolTable() || pTable->IsLocalSymbolTable()  || 
		pTable->IsFunctionSymbolTable() )
		return name;

	// иначе если класс или именованная область видимости - вернуть рекурсивно
	else if( pTable->IsNamespaceSymbolTable() || pTable->IsClassSymbolTable() )
	{
		const Identifier *id = dynamic_cast<const Identifier *>(pTable);
		INTERNAL_IF( id == NULL );
		return string(id->GetQualifiedName().c_str() + string("::") + name.c_str()).c_str();
	}

	else
		INTERNAL( "'Identifier::GetQualifiedName' идентификатор принадлежит к неизвестной ОВ" );
	return "";
}


// получить строковое представление типа
CharString TypyziedEntity::GetTypyziedEntityName( bool printName ) const
{
	string rval;

	// печатаем квалификаторы
	if( constQualifier )
		rval += "const ";

	if( volatileQualifier )
		rval += "volatile ";
		
	// если имеем конструктор, деструктор или оператор преобразования,
	// не печатаем базовый тип	
	BaseType::BT bt = baseType->GetBaseTypeCode();
	if( IsFunction() && static_cast<const Function *>(this)->IsClassMember() )
	{
		const Method &meth = *static_cast<const Method *>(this);
		if( meth.IsConstructor() || meth.IsDestructor() )
			goto skip_base_type_print;

		// если оператор приведения и нужно печатать имя, то достаточно напечатать имя и '()'
		if(meth.IsOverloadOperator() && 
			((const ClassOverloadOperator &)meth).IsCastOperator() && printName )
		{
			rval += meth.GetQualifiedName().c_str();
			rval += "()";

			if( meth.GetFunctionPrototype().IsConst() )
				rval += " const";
			if( meth.GetFunctionPrototype().IsVolatile() )
				rval += " volatile";

			if( rval[rval.length()-1] == ' ' )
				rval.erase(rval.end()-1);
			return rval.c_str();
		}		
	}

	// если базовый тип встроенный, печатаем его
	if( bt == BaseType::BT_BOOL    || bt == BaseType::BT_CHAR   ||
		bt == BaseType::BT_WCHAR_T || bt == BaseType::BT_INT    || 
		bt == BaseType::BT_FLOAT   || bt == BaseType::BT_DOUBLE ||
		bt == BaseType::BT_VOID )
		rval += ImplicitTypeManager(*baseType).GetImplicitTypeName().c_str();

	// иначе печатаем имя класса
	else
	{
		INTERNAL_IF( bt != BaseType::BT_CLASS	 &&
					 bt != BaseType::BT_STRUCT	 &&
					 bt != BaseType::BT_ENUM	 &&
					 bt != BaseType::BT_UNION );
			
		if( bt == BaseType::BT_ENUM )
			rval += static_cast<EnumType *>(baseType)->GetQualifiedName().c_str();
		else
			rval += static_cast<ClassType *>(baseType)->GetQualifiedName().c_str();
	}

	rval += ' ';

skip_base_type_print:
	if( derivedTypeList.GetDerivedTypeCount() != 0 )
	{
		string dtlBuf;
		int ix = 0;
		bool np = !printName;

		PrintPointer(dtlBuf, ix, np);
		rval += dtlBuf;
	}

	else if( printName )
		if( const Identifier *id = dynamic_cast<const Identifier *>(this) )
			rval += id->GetQualifiedName().c_str();

	// удаляем пробелы
	if( rval[rval.length()-1] == ' ' )
		rval.erase(rval.end()-1);
	if( rval[0] == ' ' )
		rval.erase(rval.begin());

	return rval.c_str();
}


// печатать префиксные производные типы и сохранять их в буфер
void TypyziedEntity::PrintPointer( string &buf, int &ix, bool &namePrint ) const
{
	bool isPrint = false;
	for( ; ix < derivedTypeList.GetDerivedTypeCount(); ix++, isPrint++ )
	{
		const DerivedType &dt = *derivedTypeList.GetDerivedType(ix);
		DerivedType::DT dtc = dt.GetDerivedTypeCode();

		if( dtc == DerivedType::DT_REFERENCE )
			buf = '&' + buf;

		else if( dtc == DerivedType::DT_POINTER )
		{
			const Pointer &ptr = static_cast<const Pointer &>(dt);
			string temp;			
			if( ptr.IsConst() ) 
				temp = "const ";
			
			if( ptr.IsVolatile() ) 
				temp = temp + "volatile ";

			buf = '*' + temp + buf;
		}

		else if( dtc == DerivedType::DT_POINTER_TO_MEMBER )
		{
			const PointerToMember &ptm = static_cast<const PointerToMember &>(dt);
			string temp = buf;
			buf = ptm.GetMemberClassType().GetQualifiedName().c_str() + 
				string("::") + '*';			

			if( ptm.IsConst() ) 
				buf += "const ";

			if( ptm.IsVolatile() ) 
				buf += "volatile ";			
			buf += temp;
		}

		else
		{
			if( !namePrint )
			{
				if( const Identifier *id = dynamic_cast<const Identifier *>(this) )
					buf = buf + id->GetQualifiedName().c_str();
				namePrint = true;
			}

			if( isPrint )			
				buf = '(' + buf + ')';			

			PrintPostfix( buf, ix );			
		}
	}

	// если имя, так и не было напечатано, печатаем
	if( !namePrint )
	{
		if( const Identifier *id = dynamic_cast<const Identifier *>(this) )
			buf = buf + id->GetQualifiedName().c_str();
		namePrint = true;
	}
}


// печатать постфиксные производные типы и сохранять в буфер
void TypyziedEntity::PrintPostfix( string &buf, int &ix ) const
{
	for( ; ix < derivedTypeList.GetDerivedTypeCount(); ix++)
	{
		const DerivedType &dt = *derivedTypeList.GetDerivedType(ix);
		DerivedType ::DT dtc = dt.GetDerivedTypeCode();

		if( dtc == DerivedType::DT_FUNCTION_PROTOTYPE )
		{
			const FunctionPrototype &fp =  static_cast<const FunctionPrototype &>(dt);
			buf += '('; int i;
			for( i = 0;	i<fp.GetParametrList().GetFunctionParametrCount(); i++ )
			{
				buf += fp.GetParametrList().GetFunctionParametr(i)->
					GetTypyziedEntityName(false).c_str();
				if( i < fp.GetParametrList().GetFunctionParametrCount()-1 )
						buf += ", ";
			}						
			
			if( fp.IsHaveEllipse() )			
				buf += i == 0 ? "..." : ", ...";
			buf += ')';

			if( fp.IsConst() )
				buf += " const";

			if( fp.IsVolatile() )
				buf += " volatile";

			if( !fp.CanThrowExceptions() )
				buf += " throw()";

			else if( fp.GetThrowTypeList().GetThrowTypeCount() != 0 )
			{
				buf += " throw(";
				for( int i = 0; i<fp.GetThrowTypeList().GetThrowTypeCount();i++ )
				{
					buf += fp.GetThrowTypeList().GetThrowType(i)->
						GetTypyziedEntityName(false).c_str();
					if( i < fp.GetThrowTypeList().GetThrowTypeCount()-1 )
						buf += ", ";
				}

				buf += ')';
			}
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


// если целая константа, char, int с модификаторами. Подразумевается,
// что char, wchar_t уже храняться в виде целого числа
bool Literal::IsIntegerLiteral() const 
{
	register BaseType::BT bt = GetBaseType().GetBaseTypeCode();
	return (bt == BaseType::BT_INT || 
			bt == BaseType::BT_CHAR   ||
			bt == BaseType::BT_WCHAR_T) && GetDerivedTypeList().IsEmpty();
}

// если вещественная 
bool Literal::IsRealLiteral() const 
{
	register BaseType::BT bt = GetBaseType().GetBaseTypeCode();
	return (bt == BaseType::BT_FLOAT || 
			bt == BaseType::BT_DOUBLE) && GetDerivedTypeList().IsEmpty();
}


// если строковый литерал
bool Literal::IsStringLiteral() const 
{
	register BaseType::BT bt = GetBaseType().GetBaseTypeCode();
	return bt == BaseType::BT_CHAR && GetDerivedTypeList().IsArray();
}


// если wide-строка
bool Literal::IsWideStringLiteral() const 
{
	register BaseType::BT bt = GetBaseType().GetBaseTypeCode();
	return bt == BaseType::BT_WCHAR_T && GetDerivedTypeList().IsArray();
}


// конструктор задает те параметры константы перечисления, которые
// ей необходимы
EnumConstant::EnumConstant( const NRC::CharString &name, SymbolTable *entry, 
		int v, EnumType *pEnumType ) : Identifier(name, entry),
	TypyziedEntity( pEnumType, true, false, DerivedTypeList() ), value(v) 
{
}


// получить перечислимый тип к которому принадлежит константа
const EnumType &EnumConstant::GetEnumType() const 
{
	return static_cast<const EnumType &>(GetBaseType());
}


// конструктор задает параметры объекта и генерирует С-имя
::Object::Object( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, SS ss, bool ls  ) 
	:  Identifier(name, entry), TypyziedEntity(bt, cq, vq, dtl), storageSpecifier(ss), 
		pInitialValue(NULL), clinkSpec(ls) 
{
	// для типа и для заданного имени ничего не генерируем
	if( !c_name.empty() || ss == SS_TYPEDEF || entry == NULL )
		return;

	// если задана С-связывание, либо объект локальный, оставляем имя без 
	// изменений
	if( clinkSpec || entry->IsLocalSymbolTable() || entry->IsFunctionSymbolTable() ||
		(entry->IsClassSymbolTable() && ss != SS_STATIC) )
		c_name = name.c_str();
	else
		c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) + name.c_str();
}


// конструктор задает необходимые параметры функции
Function::Function( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, SS ss, CC cc ) 
		: Identifier(name, entry), TypyziedEntity(bt, cq, vq, dtl), inlineSpecifier(inl),
	storageSpecifier(ss), callingConvention(cc), isHaveBody(false) 
{ 
	// генерируем имя для функции
	if( !entry->IsClassSymbolTable() )
		c_name = ( cc == CC_CDECL ) ? name.c_str() : 
			"__" + TranslatorUtils::GenerateScopeName( *entry ) + name.c_str();
}	


// функция не возвращает значения
bool Function::IsProcedure() const 
{
	return GetBaseType().GetBaseTypeCode() == BaseType::BT_VOID &&
			GetDerivedTypeList().GetDerivedTypeCount() == 1;
}


// задаем параметры метода
Method::Method( const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool am, bool vm, bool dm, DT dt ) 

	: Function(name, entry, bt, cq, vq, dtl, inl, ss, cc), accessSpecifier(as),
	abstractMethod(am), virtualMethod(vm), destructorMethod(dm), declarationType(dt),
	vfData(NULL)
{
	if( destructorMethod )
		c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) + "destructor";
	else
		c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) + name.c_str();
}


// освобождаем память занятую информацией виртуального метода
Method::~Method()
{
	if( vfData && &GetSymbolTableEntry() == &vfData->GetRootVfClass() )
		delete const_cast<VFData *>(vfData);
}


// задать виртуальность методу, т.к. она может выясниться не сразу
// после конструирования метода
void Method::SetVirtual( const Method *rootMeth ) 
{
	INTERNAL_IF( vfData != NULL );	

	// если корневой метод задан, задаем его информацию
	if( rootMeth )
		vfData = &rootMeth->GetVFData();
	// иначе создаем собственную, увеличиваем счетчик виртуальных методов класса
	else
	{
		ClassType &cls = (ClassType &)GetSymbolTableEntry();
		vfData = new VFData(cls.GetVirtualFunctionCount(), *this);
		cls.IncVirtualFunctionCount();
	}

	virtualMethod = true;
}


// задаем параметры конструктора
OverloadOperator::OverloadOperator( 
		const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, int opc, const NRC::CharString &opn ) :

		Function(name, entry, bt, cq, vq, dtl, inl, ss, cc),
		opCode( tolower(opc) ), opName(opn) 	
{
	c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) +
		"operator" + TranslatorUtils::GenerateOperatorName(opc);
}


// конструктор задает все параметры вверх по иерархии
ClassOverloadOperator::ClassOverloadOperator(
	const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
	bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
	SS ss, CC cc, AS as, bool am, bool vm, 
	int opc, const NRC::CharString &opn, DT dt )  

	: Method(name, entry, bt, cq, vq, dtl, inl, ss, cc, as, am, vm, false, dt),
	opCode(opc), opName(opn)
{
	if( opCode == OC_CAST )
		return;
	c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) +
		"operator" + TranslatorUtils::GenerateOperatorName(opc);
}


// задаем параметры конструктора
ConstructorMethod::ConstructorMethod( 
		const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool es, DT dt ) :
		
		Method(name, entry, bt, cq, vq, dtl, inl, ss, cc, as, false, false, false, dt),
		explicitSpecifier(es) 
{
	INTERNAL_IF( dtl.GetDerivedTypeCount() != 2 || 
		dtl.GetDerivedType(1)->GetDerivedTypeCode() != DerivedType::DT_REFERENCE );
	// задаем генерируемое имя конструктора
	c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) + "constructor";
}


// конструктор принимает все параметры
ClassCastOverloadOperator::ClassCastOverloadOperator( 	
		const NRC::CharString &name, SymbolTable *entry, BaseType *bt,
		bool cq, bool vq, const DerivedTypeList &dtl, bool inl, 
		SS ss, CC cc, AS as, bool am, bool vm, 
		int opc, const NRC::CharString &opn, const TypyziedEntity &ctp, DT dt )

	: ClassOverloadOperator(name, entry, bt, cq, vq, dtl, inl, ss, cc, as, am, vm, opc, opn, dt),
		castType(ctp) 
{
	// к имени прибавляется порядковый номер при вставке, для разрешения конфликта имен
	c_name = "__" + TranslatorUtils::GenerateScopeName( *entry ) + "cast_operator";
}


// присоединить производный список типов с cv-квалификацией первого
// производного типа, только если он '*', 'ptr-to-member', '()', 
// при этом этот производный тип полностью копируется
bool DerivedTypeList::AddDerivedTypeListCV( const DerivedTypeList &dtl, bool c, bool v )
{
	bool was_qual = false;
	vector<PDerivedType>::const_iterator p = dtl.derivedTypeList.begin();

	for( ;p != dtl.derivedTypeList.end(); p++ )
	{
		// только если массив, продолжаем 
		if( (*p)->GetDerivedTypeCode() == DerivedType::DT_ARRAY )			
		{
			AddDerivedType( *p );
			continue;
		}

		// если ссылка, копируем и выходим, т.к. константной ссылка быть
		// не может
		else if( (*p)->GetDerivedTypeCode() == DerivedType::DT_REFERENCE )		
			AddDerivedType( *p );					

		// если имеем дело с указателем
		else if( (*p)->GetDerivedTypeCode() == DerivedType::DT_POINTER )
		{
			bool pc = c || ((Pointer &)(**p)).IsConst(),
				 pv = v || ((Pointer &)(**p)).IsVolatile();
			AddDerivedType( PDerivedType(new Pointer(pc, pv)) );
		}

		// если имеем дело с указателем на член
		else if( (*p)->GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
		{
			bool pc = c || ((PointerToMember &)(**p)).IsConst(),
				 pv = v || ((PointerToMember &)(**p)).IsVolatile();
			const ClassType &cls = ((PointerToMember &)(**p)).GetMemberClassType();
			AddDerivedType( PDerivedType(new PointerToMember(&cls, pc, pv)) );
		}

		// если имеем дело с функцией
		else if( (*p)->GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE )
		{
			FunctionPrototype &fp = ((FunctionPrototype &)**p);
			bool pc = c || fp.IsConst(),
				 pv = v || fp.IsVolatile();
			AddDerivedType( PDerivedType(new FunctionPrototype(
				pc, pv, fp.GetParametrList(), fp.GetThrowTypeList(), fp.CanThrowExceptions(),
				fp.IsHaveEllipse()) ) );

		}

		else
			INTERNAL( "'AddDerivedTypeListCV' получила неизвестный код");
				
		was_qual = true;
		break;		
	}

	if( was_qual )
		derivedTypeList.insert( derivedTypeList.end(), p+1, dtl.derivedTypeList.end());
	return was_qual;
}
