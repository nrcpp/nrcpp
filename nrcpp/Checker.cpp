// реализация методов КЛАССОВ-ЧЕКЕРОВ - Checker.h

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
#include "Overload.h"


// использовать утилиты проверки
using namespace CheckerUtils;


// проверяет, может ли класс быть базовым для другого класса
bool CheckerUtils::BaseClassChecker(const ClassType &cls, const SymbolTableList &stl, 
									const Position &errPos, PCSTR cname )
{
	// 1. класс должен быть полностью объявленным
	// 2. класс не должен быть объединением
	// 3. класс должен быть доступным в данной области видимости
	if( cls.IsUncomplete() )
	{
		theApp.Error(errPos, 
			"'%s' - не полностью объявленный класс используется в качестве базового",
			cname);
		return false;
	}

	if( cls.GetBaseTypeCode() == BaseType::BT_UNION )
	{
		theApp.Error(errPos, 
			"'%s' - объединение не может быть базовым классом",
			cname);
		return false;
	}

	return true;
}


// проверяет возможность определения класса
bool CheckerUtils::ClassDefineChecker( const ClassType &cls, const SymbolTableList &stl, 
									   const Position &errPos )
{		
	// 1. класс нельзя определять, если он уже определен
	// 2. класс нельзя определять, если текущая область видимости не глобальная,
	//	  а класс квалифицирован другими областями видимости
	if( !cls.IsUncomplete() )
	{
		theApp.Error(errPos, "'%s' - класс уже определен", cls.GetName().c_str());	
		return false;
	}

	if( !stl.IsEmpty() && 
		!(GetCurrentSymbolTable().IsGlobalSymbolTable() ||
		  GetCurrentSymbolTable().IsNamespaceSymbolTable()) )		 
	{ 
		theApp.Error(errPos, 
			"'%s' - класс должен определяться в глобальной области видимости", 
			cls.GetName().c_str());	
		return false;		
	}

	return true;
}


// проверяет, если тип typedef, является классом, вернуть класс иначе 0
const ClassType *CheckerUtils::TypedefIsClass( const ::Object &obj )
{
	INTERNAL_IF( obj.GetStorageSpecifier() != ::Object::SS_TYPEDEF );

	// если список производных типов не пустой, базовый имеет код
	// не класса, присутствуют cv-квалификаторы, 
	BaseType::BT bt = obj.GetBaseType().GetBaseTypeCode();
	if( !obj.GetDerivedTypeList().IsEmpty() ||
		(bt != BaseType::BT_CLASS && bt != BaseType::BT_STRUCT && bt != BaseType::BT_UNION) ||
		obj.IsConst() || obj.IsVolatile() )
		return NULL;

	return static_cast<const ClassType *>(&obj.GetBaseType());
}


// проверить достуность имени. Если имя не является членом класса, оно не проверяется
// на доступность
void CheckerUtils::CheckAccess( const QualifiedNameManager &qnm, const Identifier &id, 
				const Position &errPos, const SymbolTable &ct )
{
	if( !id.GetSymbolTableEntry().IsClassSymbolTable() )
		return;

	// если идентификатор содержится в списке синонимов, значит его необходимо
	// сохранить задать как основной
	const ClassMember *cm = NULL;
	if( const Identifier *ui = qnm.GetSynonymList().find_using_identifier(&id) )
		cm = dynamic_cast<const ClassMember *>(ui);
	else
		cm = dynamic_cast<const ClassMember *>(&id);

	INTERNAL_IF( cm == NULL );


	// вспомагательная структура для генерации исключительных ситуаций
	// и вывода ошибки
	struct ENoAccess
	{
		// информация необходимая для вывода ошибки
		CharString stName, memName, asName;

		// на основании параметров задаем имена
		ENoAccess( const SymbolTable &ct, const ClassType &mcls, const ClassMember &cm ) {
			stName = ManagerUtils::GetSymbolTableName(ct);			
			memName = dynamic_cast<const Identifier &>(cm).GetQualifiedName();
			asName = ManagerUtils::GetAccessSpecifierName(cm.GetAccessSpecifier());								
		}
	};


	// выявляем текущую область видимости. Если она является локальной,
	// значит требуется подняться до функциональной
	const SymbolTable *curST = &ct;
	if( curST->IsLocalSymbolTable() )	
		curST = &GetScopeSystem().GetFunctionalSymbolTable();	

	// в блоке могут генерироваться исключительные ситуации типа ENoAccess
	try {

	// имя одиночное и требует конкретной проверки на основании
	// текущей области видимости
	if( qnm.GetQualifierList().IsEmpty() )
	{
		// если текущая область видимости функциональная, она обязательно
		// должна быть функцией членом
		if( curST->IsFunctionSymbolTable() )
		{
			const Function &fn = static_cast<const FunctionSymbolTable *>(curST)->GetFunction();
			INTERNAL_IF( !fn.IsClassMember() );

			// получаем класс к которому принадлежит функция-член,
			// моделируем обращение к члену через 'this'
			const ClassType &fnCls = 
				static_cast<const ClassType &>(fn.GetSymbolTableEntry());

			AccessControlChecker achk( *curST, fnCls, *cm );

			// если член недоступен, генерируем ситуацию выводя ошибку
			if( !achk.IsAccessible() )
				throw ENoAccess( *curST, fnCls, *cm );
		}

		// иначе текущая область видимости должна быть классом
		else if( curST->IsClassSymbolTable() )
		{
			// имя одиночное, соотв. доступ к нему совершается через 'this',
			// абстрактно. Т.е. через текущий класс
			const ClassType &memCls = static_cast<const ClassType &>(*curST);
			AccessControlChecker achk( *curST, memCls , *cm );

			// если член недоступен, генерируем ситуацию выводя ошибку
			if( !achk.IsAccessible() )
				throw ENoAccess( *curST, memCls , *cm );
		}

		// иначе остается глобальная и именованная области, а они
		// не могут напрямую обратиться к члену класса без квалификации,
		// поэтому внутренняя ошибка
		else
			INTERNAL( "'CheckerUtils::CheckAccess' текущая область видимости "
					  "некорректна для проверки члена класса" );
	}

	// далее проверяем, если имя квалифицированное, значит требуется проверить
	// всю квалификацию и в качестве результата получить указатель на последний класс
	else if( const ClassType *cls = CheckQualifiedAccess(qnm, errPos, *curST) )
	{
		// проверяем доступ к члену через класс
		AccessControlChecker achk( *curST, *cls, *cm );

		// если член недоступен, генерируем ситуацию выводя ошибку
		if( !achk.IsAccessible() )
			throw ENoAccess( *curST, *cls, *cm );
	}

	
	// была ошибка доступа, перехватываем информацию для вывода ошибки
	} catch( const ENoAccess &einfo ) {
		theApp.Error( errPos, "'%s' - %s член недоступен в '%s'",
			einfo.memName.c_str(), einfo.asName.c_str(), 
			einfo.stName.c_str() );
	}
}


// проверка доступа для квалифицированного имени, проверяет доступность
// каждого члена в квалификации и если последний член является классом,
// вернуть его для проверки вместе с членом в CheckAccess, иначе вернуть 0.
// Облась видимости 'ct' должна быть корректно преобразована из локальной
// в функциональную, если требуется. Список квалификаторов в 'qnm' не должен
// быть пустым.	
const ClassType *CheckerUtils::CheckQualifiedAccess( const QualifiedNameManager &qnm,
		const Position &errPos, const SymbolTable &ct )
{
	INTERNAL_IF( ct.IsLocalSymbolTable() );

	// список квалификаторов имени должен быть непустой
	INTERNAL_IF( qnm.GetQualifierList().IsEmpty() );
	const SymbolTableList &qualList = qnm.GetQualifierList();

	for( int i = 0; i<qualList.GetSymbolTableCount(); i++ )
	{
		const SymbolTable &qst = qualList.GetSymbolTable(i);

		// если квалификатор является последним, проверим, если
		// он является классом, вернуть его иначе 0
		if( i == qualList.GetSymbolTableCount()-1 )
		{		
			if( qst.IsClassSymbolTable() )
				return &static_cast<const ClassType &>(qst);
			return NULL;
		}


		// если область видимости является классом, получаем следующую
		// и проверяем ее на доступность
		if( qst.IsClassSymbolTable() )
		{
			const ClassMember *mem = 
				dynamic_cast<const ClassMember *>(&qualList.GetSymbolTable(i));

			INTERNAL_IF( mem == NULL );
			AccessControlChecker achk( ct, static_cast<const ClassType &>(qst), *mem);

			// если член не является доступным, выводим ошибку
			if( !achk.IsAccessible() )
			{
				theApp.Error( errPos,
					"'%s' - %s член недоступен в '%s'",
					dynamic_cast<const Identifier *>(mem)->GetQualifiedName().c_str(),
					ManagerUtils::GetAccessSpecifierName(mem->GetAccessSpecifier()),
					ManagerUtils::GetSymbolTableName(ct).c_str() );

				return NULL;
			}
		}
	}

	return NULL;
}


// проверить корректность списка производных типов
bool CheckerUtils::CheckDerivedTypeList( const TempObjectContainer &object )
{
	// произвести стандартную проверку производных типов. 
	// Не может быть указателя на ссылку, ссылки на ссылку, массивов ссылок, 
	// массива функций, функции возвращающей массив, указатель на член-ссылку.
	// У функции не может быть cv-квалификаторов в глобальной 
	// области видимости, только если не задан спецификатор хранения typedef.
	for( int i = 0; i<object.dtl.GetDerivedTypeCount()-1; i++ )
	{
		const DerivedType &dt1 = *object.dtl.GetDerivedType(i),
						  &dt2 = *object.dtl.GetDerivedType(i+1);

		if( dt1.GetDerivedTypeCode() == DerivedType::DT_POINTER &&
			dt2.GetDerivedTypeCode() == DerivedType::DT_REFERENCE )
		{
			theApp.Error(object.errPos, "'%s' - некорректный тип 'указатель на ссылку'",
				object.name.c_str());
			return false;
		}

		if( dt1.GetDerivedTypeCode() == DerivedType::DT_REFERENCE &&
			dt2.GetDerivedTypeCode() == DerivedType::DT_REFERENCE )
		{
			theApp.Error(object.errPos, "'%s' - некорректный тип 'ссылка на ссылку'",
				object.name.c_str());
			return false;
		}

		if( dt1.GetDerivedTypeCode() == DerivedType::DT_ARRAY && 
			( dt2.GetDerivedTypeCode() == DerivedType::DT_REFERENCE ||
			  dt2.GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) )
		{
			
			theApp.Error(object.errPos, "'%s' - некорректный тип 'массив %s'",
				object.name.c_str(), 
				dt2.GetDerivedTypeCode() == DerivedType::DT_REFERENCE ? "ссылок" : "функций");

			return false;
		}

		if( dt1.GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER &&
			dt2.GetDerivedTypeCode() == DerivedType::DT_REFERENCE )
		{
			theApp.Error(object.errPos, "'%s' - некорректный тип 'указатель на член-ссылку'",
				object.name.c_str());
			return false;
		}

	
		if( dt1.GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE &&
			(dt2.GetDerivedTypeCode() == DerivedType::DT_ARRAY || 
			 dt2.GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE) )
		{
			theApp.Error(object.errPos, "'%s' - некорректный тип 'функция возвращающая %s'",
				object.name.c_str(), 
				dt2.GetDerivedTypeCode() == DerivedType::DT_ARRAY ? "массив" : "функцию");
			return false;
		}

		// если массив, который не является головой списка - без размера,
		// это ошибка
		if( dt2.GetDerivedTypeCode() == DerivedType::DT_ARRAY &&
			dt2.GetDerivedTypeSize() <= 0 )
		{
			theApp.Error(object.errPos, "'%s' - неизвестный или нулевой размер массива",
				object.name.c_str());
			return false;
		}

		// последняя проверка, прототип функции не может иметь cv-квалификаторов,
		// если это не указатель на член функцию. 
		// Следует отметить, что если прототип функции является головой списка, 
		// тогда его проверкой занимается вызывающая функция
		if( dt2.GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE )
		{
			if( dt1.GetDerivedTypeCode() != DerivedType::DT_POINTER_TO_MEMBER &&
				((FunctionPrototype &)dt2).CV_Qualified() != 0 )
			{
				theApp.Error(object.errPos, 
					"'%s' - некорректное использование cv-квалификаторов функции",
					object.name.c_str());
				return false;
			}
		}
	}

	return true;
}


// произвести проверку совместимости базового типа и производных. 
// Не может быть объекта, массива, указателя на член, ссылки типа void. 
// Не может быть объекта или массива объектов абстрактного класса, 
// только если это не typedef декларация. 
// Не может быть объекта или массива из незавершенного класса, только если
// это не объявление
bool CheckerUtils::CheckRelationOfBaseTypeToDerived( TempObjectContainer &object,
							 bool declaration, bool expr ) 
{
	// если базовый тип void
	if( object.finalType->GetBaseTypeCode() == BaseType::BT_VOID )
	{
		// если нет производных типов и декларация не typedef, ошибка
		if( object.dtl.IsEmpty() )
		{
			if( object.ssCode == KWTYPEDEF || expr )
				return true;
			else
			{
				theApp.Error(object.errPos,
					"'%s' - объект не может иметь тип 'void'", object.name.c_str());
				return false;
			}
		}

		// иначе есть производные типы, теперь необходимо, чтобы это был
		// указатель или функция
		const PDerivedType &ldt = object.dtl.GetTailDerivedType();
		if( ldt->GetDerivedTypeCode() == DerivedType::DT_POINTER   ||
			ldt->GetDerivedTypeCode() == DerivedType::DT_FUNCTION_PROTOTYPE )
			return true;

		// иначе ошибка
		else
		{
			PCSTR msg;
			if( ldt->GetDerivedTypeCode() == DerivedType::DT_ARRAY )
				msg = "массив типа void";
			else if( ldt->GetDerivedTypeCode() == DerivedType::DT_REFERENCE )
				msg = "ссылка на void";
			else if( ldt->GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER )
				msg = "указатель на член типа void";
			else 
				INTERNAL("'CheckRelationOfBaseTypeToDerived' "
						 "принимает некорректный код производного типа");

			theApp.Error(object.errPos,
					"'%s' - объект не может иметь тип '%s'", object.name.c_str(), msg);
				return false;
		}
	}

	// иначе если базовый тип это класс
	else if( object.finalType->IsClassType() )
	{
		ClassType *cls = static_cast<ClassType *>(object.finalType);
		

		// если класс не полностью объявлен или абстрактный, он может базовым типом
		// только для ссылок, указателей, указателей на члены
		if( cls->IsUncomplete() || cls->IsAbstract() ) 
		{
			bool incomplete = cls->IsUncomplete();

			// список производных типов пустой, поэтому должна
			// быть только объявление (typedef, extern, либо static, typedef у членов)
			if( object.dtl.IsEmpty() )
				if( declaration )				
					return true;
			
				else
				{
					theApp.Error(object.errPos,
						"'%s' - класс '%s' является %s",						 
						object.name.c_str(), cls->GetQualifiedName().c_str(), 
							incomplete ? "незавершенным" : "абстрактным");

					return false;
				}
	
			DerivedType::DT dtc = object.dtl.GetTailDerivedType()->GetDerivedTypeCode();
			if( (dtc == DerivedType::DT_ARRAY || dtc == DerivedType::DT_FUNCTION_PROTOTYPE) &&
				!declaration )
			{
				theApp.Error(object.errPos,
						"'%s' - класс '%s' является %s",						 
						object.name.c_str(), cls->GetQualifiedName().c_str(), 
							incomplete ? "незавершенным" : "абстрактным");
				return false;
			}
			
			return true;
		}		
	}

	return true;
}


// проверить корректность объявления параметров по умолчанию у функций.
// Если второй параметр 0, значит проверяем корректность только у первой
void CheckerUtils::DefaultArgumentCheck( const FunctionPrototype &declFn, 
				const FunctionPrototype *fnInTable, const Position &errPos )
{
	// проверяем, только чтобы параметры по умолчанию шли по порядку
	if( fnInTable == NULL )
	{
		const FunctionParametrList &fpl = declFn.GetParametrList();
		bool haveDP = false;
		for( int i = 0; i<fpl.GetFunctionParametrCount(); i++ )
			if( fpl[i]->IsHaveDefaultValue() )
				haveDP = true;
			else if( haveDP )
			{
				theApp.Error(errPos,
					"'%s' - пропущено значение по умолчанию",
					fpl[i]->GetName().c_str());
				break;
			}
	}

	// иначе сравниваем две функции с заданием аргументов по умолчанию
	// функции из таблицы
	else
	{
		const FunctionParametrList &dFpl = declFn.GetParametrList(),
			&inFpl = fnInTable->GetParametrList();
		INTERNAL_IF( dFpl.GetFunctionParametrCount() != inFpl.GetFunctionParametrCount() );

		bool haveDP = false;
		for( int i = 0; i<dFpl.GetFunctionParametrCount(); i++ )
		{
			// значение по умолчанию для параметра не может переопределяться
			if( dFpl[i]->IsHaveDefaultValue() )
			{
				haveDP = true;
				if( inFpl[i]->IsHaveDefaultValue() )
				{
					theApp.Error(errPos,
						"'%s' - значение по умолчанию переопределяется",
						dFpl[i]->GetName().c_str());
					break;
				}

				// иначе задаем значение по умолчанию для функции в таблице
				else
					const_cast<Parametr&>(*inFpl[i]).
							SetDefaultValue( dFpl[i]->GetDefaultValue() );
			}

			// иначе если значение по умолчанию уже было, а в функции,
			// которая в таблице оно пропущено, вывести ошибку
			else if( haveDP && !inFpl[i]->IsHaveDefaultValue() )
			{
				theApp.Error(errPos,
					"'%s' - пропущено значение по умолчанию",
					dFpl[i]->GetName().c_str());							
				break;
			}
		}
	}
}


// вывести ошибку, установить флаг
void GlobalDeclarationChecker::Error( PCSTR msg, PCSTR arg ) 
{
	theApp.Error(object.errPos, msg, arg);
	incorrect = true;
}


// скрытая функция проверки
void GlobalDeclarationChecker::Check()
{
	// проверяем спецификаторы хранения которые могут использоваться
	// в глобальной области видимости
	if( object.ssCode != -1		  &&
		object.ssCode != KWSTATIC &&
		object.ssCode != KWEXTERN &&
		object.ssCode != KWTYPEDEF )
	{
		if( localDeclaration			&&
			object.ssCode != KWAUTO		&&
			object.ssCode != KWREGISTER )
		{
			Error("'спецификатор хранения %s' некорректен в данном контексте",
				GetKeywordName(object.ssCode));
			object.ssCode = -1;
		}
	}

	// проверяем спецификатор friend
	if( object.friendSpec )
		Error("'%s' - 'спецификатор дружбы friend' некорректен в данном контексте",
			object.name.c_str());

	// проверяем если задан спецификатор функции, а объект не является ф-цией
	if( object.fnSpecCode != -1 )
	{
		if( !object.dtl.IsFunction() )
		{
			theApp.Error( object.errPos,
				"'%s' - использование спецификатора функции '%s', в объявлении не-функции",
				object.name.c_str(), GetKeywordName(object.fnSpecCode) );
			incorrect = true;
		}

		// в глобальной декларации может использоваться только спецификатор inline
		if( object.fnSpecCode != KWINLINE )
		{
			theApp.Error( object.errPos,
				"'%s' - использование спецификатора функции '%s' некорректно в данном контексте",
				object.name.c_str(), GetKeywordName(object.fnSpecCode) );
			incorrect = true;
		}
	}

	// стандартная проверка списка производных типов	
	if( !CheckDerivedTypeList(object) )
		incorrect = true;	

	// проверка совместимости базового типа и производных
	if( !CheckRelationOfBaseTypeToDerived(object, object.ssCode == KWEXTERN ||
									object.ssCode == KWTYPEDEF ) )
		incorrect = true;	

	// проверим, если декларация является функцией
	if( object.dtl.IsFunction() )
	{
		const FunctionPrototype &fnp =	
			static_cast<const FunctionPrototype &>(*object.dtl.GetHeadDerivedType());

		// cv-квалификаторы функции могут присутствовать только при
		// наличии typedef
		if( fnp.CV_Qualified() != 0 && object.ssCode != KWTYPEDEF )
			Error( "'%s' - некорректное использование cv-квалификаторов функции",
					object.name.c_str());
	}

	// проверим если декларация является массив, его размер должен быть известен
/*	else if( object.dtl.IsArray() )
	{
		if( object.dtl[0]->GetDerivedTypeSize() < 0 &&
			object.ssCode != KWTYPEDEF && object.ssCode != KWEXTERN )			
			Error( "'%s' - неизвестный размер массива", object.name.c_str());

		if( object.dtl[0]->GetDerivedTypeSize() == 0 )
			Error( "'%s' - нулевой размер массива", object.name.c_str());
	} */

	// проверяем, если базовый тип классовый и он локальный и
	// спецификатор доступа extern или static, ошибка
	if( object.finalType->IsClassType() &&
		static_cast<const ClassType*>(object.finalType)->IsLocal() &&
		(object.ssCode == KWSTATIC || object.ssCode == KWEXTERN) )	
		Error( "'%s' - базовый тип не имеет связывания", object.name.c_str());	
}


// скрытая функция проверки параметра, выполняет основную работу
// объекта 
void ParametrChecker::Check()
{	
	// параметр может иметь только спец. хранения регистр или вообще не иметь
	if( parametr.ssCode != -1		  &&
		parametr.ssCode != KWREGISTER )
		theApp.Error( parametr.errPos,
		 "'%s' - 'спецификатор хранения %s' некорректен для параметра",
			parametr.name.c_str(), GetKeywordName(parametr.ssCode)),
			incorrect = true;

	// проверяем спецификатор friend
	if( parametr.friendSpec )
		theApp.Error( parametr.errPos,
			"'%s' - 'спецификатор дружбы friend' некорректен для параметра",
			parametr.name.c_str()), incorrect = true;

	// проверяем если задан спецификатор функции в параметре это считается ошибкой
	if( parametr.fnSpecCode != -1 )
	{
		theApp.Error( parametr.errPos,
			"'%s' - использование спецификатора функции '%s', в объявлении параметра",
			parametr.name.c_str(), GetKeywordName(parametr.fnSpecCode) );
		incorrect = true;
	}

	// стандартная проверка списка производных типов	
	if( !CheckDerivedTypeList(parametr) )
		incorrect = true;

	// проверка совместимости базового типа и производных
	if( !CheckRelationOfBaseTypeToDerived(parametr, false) )
		incorrect = true;	

	// далее если параметр является функции - преобразуем его в указатель
	// на функцию и проверяем cv-квалификаторы
	if( parametr.dtl.IsFunction() )
	{
		const FunctionPrototype &fp = 
			static_cast<const FunctionPrototype &>(*parametr.dtl[0]);		

		if( fp.CV_Qualified() != 0 )
			theApp.Error( parametr.errPos,
			"'%s' - некорректное использование cv-квалификаторов функции",
				parametr.name.c_str()), 
				incorrect = true;

		parametr.dtl.PushHeadDerivedType( new Pointer(false, false) );
	}

	// если массив, преобразуем его в указатель
	else if( parametr.dtl.IsArray() )
	{
		parametr.dtl.PopHeadDerivedType();
		parametr.dtl.PushHeadDerivedType( new Pointer(false, false) );
	}


	// проверка переопределения параметра
	if( fnParamList.HasParametr(parametr.name) >= 0 )
	{
		theApp.Error( parametr.errPos, "'%s' - параметр переопределен", parametr.name.c_str());
		parametr.name = (string("<без имени ") + 
			CharString(fnParamList.GetFunctionParametrCount()).c_str() + ">").c_str();
	}

}


// проверка типа throw-спецификации
void ThrowTypeChecker::Check()
{
	register TempObjectContainer &toc = throwType;

	// не может иметь спецификатор хранения
	if( toc.ssCode != -1 )		
		theApp.Error( toc.errPos,
		 "'спецификатор хранения %s' некорректен для %s",
			GetKeywordName(toc.ssCode), toc.name.c_str());			

	// проверяем спецификатор friend
	if( toc.friendSpec )
		theApp.Error( toc.errPos,
		 "'спецификатор дружбы friend' некорректен для %s", toc.name.c_str());
			

	// проверяем если задан спецификатор функции в параметре это считается ошибкой
	if( toc.fnSpecCode != -1 )	
		theApp.Error( toc.errPos,
			"'%s' - использование спецификатора функции '%s' некорректно",
			toc.name.c_str(), GetKeywordName(toc.fnSpecCode) );			

	// стандартная проверка списка производных типов	
	CheckDerivedTypeList(toc);		

	// проверка совместимости базового типа и производных
	CheckRelationOfBaseTypeToDerived(toc, false);	
}


// скрытая функция проверки типа, выполняет основную работуо бъекта 
void CatchDeclarationChecker::Check()
{
	// не может иметь спецификатор хранения
	if( toc.ssCode != -1 )		
		theApp.Error( toc.errPos,
		 "'спецификатор хранения %s' некорректен для catch-декларации",
			GetKeywordName(toc.ssCode));			

	// проверяем спецификатор friend
	if( toc.friendSpec )
		theApp.Error( toc.errPos,
		 "'спецификатор дружбы friend' некорректен для для catch-декларации");
			

	// проверяем если задан спецификатор функции в параметре это считается ошибкой
	if( toc.fnSpecCode != -1 )	
		theApp.Error( toc.errPos,
			"'%s' - использование спецификатора функции '%s' некорректно",
			toc.name.c_str(), GetKeywordName(toc.fnSpecCode) );			

	// стандартная проверка списка производных типов	
	CheckDerivedTypeList(toc);		

	// проверка совместимости базового типа и производных
	CheckRelationOfBaseTypeToDerived(toc, false);

	// преобразуем функция типа T - в указатель на функцию типа T
	if( toc.dtl.IsFunction() )
	{
		if( static_cast<const FunctionPrototype &>(*toc.dtl[0]).CV_Qualified() != 0 )
			theApp.Error( toc.errPos,
			"'%s' - некорректное использование cv-квалификаторов функции",
				toc.name.c_str());				

		toc.dtl.PushHeadDerivedType( new Pointer(false, false) );
	}

	// массив типа T - в указатель на T
	else if( toc.dtl.IsArray() )
	{
		toc.dtl.PopHeadDerivedType();
		toc.dtl.PushHeadDerivedType( new Pointer(false, false) );
	}

	// далее проверяем, у catch-декларации не может быть необъявленного типа 
	// или указателя на него
	if( (toc.finalType->IsClassType() && 
		 static_cast<const ClassType *>(toc.finalType)->IsUncomplete()) ||
		(toc.finalType->GetBaseTypeCode() == BaseType::BT_VOID &&
		 toc.dtl.IsEmpty()) )
		theApp.Error( toc.errPos, 
			"не полный тип '%s' является некорректными для catch-декларации",
			toc.finalType->IsClassType() ? 
			static_cast<const ClassType *>(toc.finalType)->GetQualifiedName().c_str() :
			"void" );
}


// возвращает true, если объект является константым
bool DataMemberChecker::ConstantMember()
{
	const DerivedType *dt = NULL;
	for( int i = 0; i<dm.dtl.GetDerivedTypeCount(); i++ )
		if( dm.dtl.GetDerivedType(i)->GetDerivedTypeCode() == DerivedType::DT_ARRAY )
			continue;
		else
		{
			dt = &*dm.dtl.GetDerivedType(i);
			break;
		}

	if( !dt )
		return dm.constQual;

	if( dt->GetDerivedTypeCode() == DerivedType::DT_POINTER &&
		((Pointer *)dt)->IsConst() )
		return true;

	if( dt->GetDerivedTypeCode() == DerivedType::DT_POINTER_TO_MEMBER && 
		((PointerToMember *)dt)->IsConst() )
		return true;

	return false;
}


// возвращает true, если класс содержит тривиальные к-тор,
// к-тор копирования, деструктор, оператор копирования
PCSTR DataMemberChecker::HasNonTrivialSMF( const ClassType &cls )
{
	SMFManager smfm(cls);
	if( smfm.GetDefaultConstructor().first &&
		!smfm.GetDefaultConstructor().first->IsTrivial() )
		return "конструктор по умолчанию";

	if( smfm.GetCopyConstructor().first &&
		!smfm.GetCopyConstructor().first->IsTrivial() )
		return "конструктор копирования" ;
	
	if( smfm.GetCopyOperator().first &&
		!smfm.GetCopyOperator().first->IsTrivial() )
		return "оператор копирования";

	if( smfm.GetDestructor().first && 
		!smfm.GetDestructor().first->IsTrivial() )
		return "деструктор";

	return NULL;
}


// метод проверки члена перед вставкой его в таблицу
void DataMemberChecker::Check()
{
	// проверяем спецификаторы хранения данного-члена
	if( dm.ssCode != -1		  &&
		dm.ssCode != KWSTATIC &&
		dm.ssCode != KWMUTABLE &&
		dm.ssCode != KWTYPEDEF )
		theApp.Error(dm.errPos,
			"'спецификатор хранения %s' некорректен для данного-члена",
			GetKeywordName(dm.ssCode)), incorrect = true;

	// проверяем спецификатор friend
	if( dm.friendSpec )
		theApp.Error(dm.errPos,
			"'%s' - 'спецификатор дружбы friend' некорректен для данного-члена",
			dm.name.c_str()), incorrect = true;

	// проверяем если задан спецификатор функции, а объект не является ф-цией
	if( dm.fnSpecCode != -1 )	
		theApp.Error( dm.errPos,
				"'%s' - использование спецификатора функции '%s', в объявлении не-функции",
				dm.name.c_str(), GetKeywordName(dm.fnSpecCode) ), incorrect = true;
			

	// стандартная проверка списка производных типов	
	if( !CheckDerivedTypeList(dm) )
		incorrect = true;	

	// проверка совместимости базового типа и производных
	if( !CheckRelationOfBaseTypeToDerived(dm, 
					dm.ssCode == KWTYPEDEF || dm.ssCode == KWSTATIC ) )
		incorrect = true;

	// имя члена должно отличаться от имени класса в котором оно определяется
	ClassType *cls = dynamic_cast<ClassType *>(&GetCurrentSymbolTable());
	INTERNAL_IF( cls == NULL );
	INTERNAL_IF( dm.name.empty() );
	
	if( cls->GetName() == dm.name )
		theApp.Error( dm.errPos,
			"'%s' - данное-член не может иметь имя класса в котором объявляется",
			dm.name.c_str() ), redeclared = true;

	// локальный класс не может иметь статических данных-членов
	if( cls->IsLocal() &&  dm.ssCode == KWSTATIC )
		theApp.Error( dm.errPos,
			"'%s' - локальный класс '%s', не может иметь статических данных-членов",
			dm.name.c_str(), cls->GetName().c_str() ), incorrect = true;


	// в случае если член объявляется внутри объединения, то он не должен
	// быть статическим или ссылкой
	if( cls->GetBaseTypeCode() == BaseType::BT_UNION )
	{
		// ссылкой не может быть потому, что ссылка потеряет свое свойство
		// неизменяемости, если объявить объединение с ссылкой и указателем
		// на один объект
		if( dm.dtl.IsReference() )
			theApp.Error( dm.errPos,
				"'%s' - ссылка не может быть членом объединения",
				dm.name.c_str() ), incorrect = true;

		if( dm.ssCode == KWSTATIC )
			theApp.Error( dm.errPos,
				"'%s' - статический данное-член не может быть членом объединения",
				dm.name.c_str() ), incorrect = true;

		// также следует проверить чтобы у объекта был тривиальный конструктор,
		// деструктор, оператор копирования и к-тор копирования
		if( dm.finalType->IsClassType() )
		{
			bool chk = true;
			for( int i = 0; i<dm.dtl.GetDerivedTypeCount(); i++ )
				if( dm.dtl.GetDerivedType(i)->GetDerivedTypeCode() != DerivedType::DT_ARRAY )
				{
					chk = false;
					break;
				}

			if( chk )
			{
				const ClassType &dmCls = static_cast<const ClassType &>(*dm.finalType);
				if( PCSTR smfName = HasNonTrivialSMF(dmCls) )
					theApp.Error(dm.errPos,
						"'%s' - класс '%s' содержит нетривиальный %s",
						dm.name.c_str(), dmCls.GetQualifiedName().c_str(), smfName);				
			}
		}
	}
	

	// в случае если член объявлен как mutable, он должен быть не константным
	// и не ссылкой
	if( dm.ssCode == KWMUTABLE )
	{
		if( dm.dtl.IsReference() )
		{
			theApp.Error( dm.errPos,
				"'%s' - ссылка не может иметь спецификатор хранения 'mutable'",
				dm.name.c_str() ), incorrect = true;
		}

		else if( ConstantMember() )
		{
			theApp.Error( dm.errPos,
				"'%s' - константный данное-член не может иметь спецификатор хранения 'mutable'",
				dm.name.c_str() ), incorrect = true;	
		}
	}

	// проверим если декларация является массив, его размер должен быть известен
	if( dm.dtl.IsArray() )
	{
		if( dm.dtl[0]->GetDerivedTypeSize() < 0 && 
			!(dm.ssCode == KWSTATIC || dm.ssCode == KWTYPEDEF) )			
			theApp.Error( dm.errPos, "'%s' - неизвестный размер массива", dm.name.c_str());

		if( dm.dtl[0]->GetDerivedTypeSize() == 0 )
			theApp.Error( dm.errPos, "'%s' - нулевой размер массива", dm.name.c_str());
	}
}


// метод проверки метода перед вставкой его в таблицу
void MethodChecker::Check()
{
	// проверяем спецификаторы хранения данного-члена
	if( method.ssCode != -1		  &&
		method.ssCode != KWSTATIC )
		theApp.Error(method.errPos,
			"'спецификатор хранения %s' некорректен для метода класса",
			GetKeywordName(method.ssCode)), incorrect = true;

	// проверяем спецификатор friend
	INTERNAL_IF( method.friendSpec );
		
	// спецификатор функции не может быть explicit
	if( method.fnSpecCode == KWEXPLICIT )	
		theApp.Error( method.errPos,
				"'%s' - 'explicit' может использоваться только с конструкторами",
				method.name.c_str() ), incorrect = true;
			
	// стандартная проверка списка производных типов	
	if( !CheckDerivedTypeList(method) )
		incorrect = true;	

	// проверка совместимости базового типа и производных
	if( !CheckRelationOfBaseTypeToDerived(method, false) )					
		incorrect = true;

	// статическая функиця не может быть виртуальным и объявляться
	// с const,volatile
	if( method.ssCode == KWSTATIC )
	{
		if( method.fnSpecCode == KWVIRTUAL )
			theApp.Error( method.errPos,
					"'%s' - статический метод не может быть виртуальным",
					method.name.c_str() ), incorrect = true;		

		INTERNAL_IF( !method.dtl.IsFunction() );
		FunctionPrototype &fp = ((FunctionPrototype &)*method.dtl.GetHeadDerivedType());
		if( fp.IsConst() || fp.IsVolatile() )
			theApp.Error( method.errPos,
					"'%s' - статический метод не может объявляться с cv-квалификаторами",
					method.name.c_str() ), incorrect = true;		
	}

	
	// если метод имеет имя своего класса, значит это конструткор
	// и он должен проверяться в ConstructorChecker'e
	INTERNAL_IF( method.name.empty() );
	const ClassType &cls = static_cast<const ClassType&>(GetCurrentSymbolTable());
	if( method.name == cls.GetName() )
	{
		theApp.Error(method.errPos,
			"'%s' - метод имеет имя класса в котором объявляется "
			"(возможно это должен быть конструктор)",
			method.name.c_str() ), incorrect = true;
		redeclared = true;
	}

	// объединение не может иметь виртуальных методов
	if( cls.GetBaseTypeCode() == BaseType::BT_UNION && method.fnSpecCode == KWVIRTUAL )
		theApp.Error(method.errPos,
			"'%s' - объединение не может иметь виртуальные методы",
			method.name.c_str() ), incorrect = true;
}

	
// метод возвращающий true, если параметр является целым
bool ClassOperatorChecker::IsInteger( const Parametr &prm ) const
{
	return prm.GetBaseType().GetBaseTypeCode() == BaseType::BT_INT &&
		   prm.GetDerivedTypeList().IsEmpty();
}


// метод проверки перегруженного оператора класса
void ClassOperatorChecker::Check()
{
	// проверяем спецификаторы хранения перегруженного оператора
	if( op.ssCode == KWSTATIC )
	{
		if( tooc.opCode != KWNEW && tooc.opCode != KWDELETE &&
			tooc.opCode != OC_NEW_ARRAY && tooc.opCode != OC_DELETE_ARRAY )
			theApp.Error(op.errPos,
				"'%s' - перегруженный оператор класса не может быть статическим",
				op.name.c_str());
	}

	else if( op.ssCode != -1 )		
		theApp.Error(op.errPos,
			"'спецификатор хранения %s' некорректен для перегруженного оператора класса",
			GetKeywordName(op.ssCode));

	// проверяем спецификатор friend
	INTERNAL_IF( op.friendSpec );
		
	// спецификатор функции не может быть explicit
	if( op.fnSpecCode == KWEXPLICIT )	
		theApp.Error( op.errPos,
				"'%s' - 'explicit' может использоваться только с конструкторами",
				op.name.c_str() );
			
	// стандартная проверка списка производных типов	
	CheckDerivedTypeList(op);		

	// проверка совместимости базового типа и производных
	CheckRelationOfBaseTypeToDerived(op, false);


	// перегруженный оператор обязательно должен быть функцией
	if( !op.dtl.IsFunction() )
		theApp.Error( op.errPos,
				"'%s' - перегруженный оператор должен быть функцией",
				op.name.c_str() );


	else
	{
	// теперь проверяем, соотв. параметров и оператора. Унарные операторы
	// не должны иметь параметров, бинарные должны иметь 1 параметр,
	// "+, -, ++, --, *, &" могут иметь как 0 так и 1 параметр, оператор ()
	// может иметь несколько параметров и '...'.
	int code = tooc.opCode; 
	const FunctionPrototype &fp = 
			static_cast<const FunctionPrototype&>(*op.dtl.GetHeadDerivedType());
	int pcount = fp.GetParametrList().GetFunctionParametrCount();

	if( code == '+' || code == '-' || code == '*' || code == '&' )
	{
		if( pcount > 1 ) 
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с 0 или 1 параметром",
				op.name.c_str() );
	}

	else if( code == INCREMENT || code == DECREMENT )
	{
		// может объявляться с одним параметром, тогда это постфиксный кремент,
		// но необъодимо чтобы параметр имел тип int
		if( pcount == 1 )
		{
			if( !IsInteger( *fp.GetParametrList().GetFunctionParametr(0) ) )
				theApp.Error( op.errPos,
					"'%s' - оператор постфиксного %s должен иметь параметр типа 'int'",
					op.name.c_str(), code == INCREMENT ? "инкремента" : "декремента" );
		}

		else if( pcount != 0 )
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с 0 или 1 параметром",
				op.name.c_str() );
	}

	// если оператор унарный
	else if( code == '!' || code == '~' || code == ARROW )
	{
		if( pcount != 0 )
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться без параметров",
				op.name.c_str() );
	}

	// если оператор является оператором выделения или освобождения памяти
	else if( code == KWNEW || code == OC_NEW_ARRAY )
	{
		op.ssCode = KWSTATIC;
		if( op.fnSpecCode == KWVIRTUAL )	
			theApp.Error( op.errPos,
				"'%s' - оператор является статическим и не может быть виртуальным",
				op.name.c_str() );
				
		// У оператора new и new[] первый параметр
		// должен быть целым и возвращаемое значение должно быть void *. 
		if( !IsInteger(*fp.GetParametrList().GetFunctionParametr(0)) )
			theApp.Error( op.errPos,
					"'%s' - первый параметр должен быть типа 'size_t'",
					op.name.c_str());

		// возвращаемое значение должно быть типа void*
		if( op.finalType->GetBaseTypeCode() != BaseType::BT_VOID ||
			op.dtl.GetDerivedTypeCount() != 2					 ||
			op.dtl.GetDerivedType(1)->GetDerivedTypeCode() != DerivedType::DT_POINTER )
			theApp.Error( op.errPos,
					"'%s' - возвращаемое значение должно быть типа 'void *'",
					op.name.c_str());

	}
	
	// если оператор освобождения-выделения для массивов
	else if( code == KWDELETE || code == OC_DELETE_ARRAY )
	{
		op.ssCode = KWSTATIC;
		if( op.fnSpecCode == KWVIRTUAL )	
			theApp.Error( op.errPos,
				"'%s' - оператор является статическим и не может быть виртуальным",
				op.name.c_str() );

		// декларация оператора деалокации может иметь две формы:
		// 'void operator delete( void *, size_t)' или 'void operator delete(void*)'.	
		bool correct = op.finalType->GetBaseTypeCode() == BaseType::BT_VOID && 
			op.dtl.GetDerivedTypeCount() == 1;
		
		// проверяем первый параметр, он должен быть типа void *
		if( pcount >= 1 )
		{
			const Parametr &prm = *fp.GetParametrList().GetFunctionParametr(0);
			if( prm.GetBaseType().GetBaseTypeCode() != BaseType::BT_VOID	||
				prm.GetDerivedTypeList().GetDerivedTypeCount() != 1			||
				prm.GetDerivedTypeList().GetDerivedType(0)->GetDerivedTypeCode() != 
					DerivedType::DT_POINTER )
				correct = false;
		}
		
		// проверяем второй параметр
		if( pcount == 2 )
		{			
			if( !IsInteger(*fp.GetParametrList().GetFunctionParametr(1)) )
				correct = false;
		}

		else if( pcount != 1 )
			correct = false;

		// теперь, если оператор объявлен не корректно, выведем ошибку
		if( !correct )
			theApp.Error( op.errPos,
				"декларация оператора деалокации может иметь две формы: "
				"'void operator %s(void *)' или 'void operator %s(void *, size_t)'",
				tooc.opString.c_str(), tooc.opString.c_str());
	}

	// иначе если оператор не функция, он является бинарным и должен объявляться с
	// одним параметром
	else if( code != OC_FUNCTION )
	{
		if( pcount != 1 )
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с 1 параметром",
				op.name.c_str() );				
	}

	// проверяем, если оператор статический, он не может объявляться 
	// с квалификаторами
	if( op.ssCode == KWSTATIC && (fp.IsConst() || fp.IsVolatile()) )
		theApp.Error( op.errPos,
			"'%s' - статический метод не может объявляться с cv-квалификаторами",
				op.name.c_str() ) ;

	// проверить наличие параметров по умолчанию, 
	if( code != OC_FUNCTION )
	for( int i = 0; i<pcount; i++ )
		if( fp.GetParametrList().GetFunctionParametr(i)->IsHaveDefaultValue() )
		{
			theApp.Error( op.errPos,
				"'%s' - оператор не может иметь параметров по умолчанию",
				op.name.c_str() );			
			break;
		}
	}	
}


// проверка корректности объявления оператора приведения
void CastOperatorChecker::Check()
{
	if( cop.ssCode != -1 )		
		theApp.Error(cop.errPos,
			"'спецификатор хранения %s' некорректен для оператора приведения типа",
			GetKeywordName(cop.ssCode));
	
	// спецификатор функции не может быть explicit
	if( cop.fnSpecCode == KWEXPLICIT )	
		theApp.Error( cop.errPos,
				"'%s' - 'explicit' может использоваться только с конструкторами",
				cop.name.c_str() );

	// проверяем спецификатор friend
	if( cop.friendSpec )
		theApp.Error( cop.errPos,
				"оператор приведения не может быть дружеским",
				cop.name.c_str() );

	// у оператора не может быть cv-квалификаторов и базовых типов
	if( cop.constQual || cop.volatileQual || cop.finalType != NULL )
		theApp.Error( cop.errPos,
				"оператор приведения не может содержать базовый тип",
				cop.name.c_str() );

	// оператор приведения должен быть функцией и не содержать других производных
	// типов
	if( !cop.dtl.IsFunction() || cop.dtl.GetDerivedTypeCount() > 1 )
	{
		theApp.Error( cop.errPos,
			"'%s' - оператор приведения должен быть функцией",
			cop.name.c_str() );

		// выходим, т.к. не функцию проверять не нужно
		incorrect = true;
		return ;
	}

	// теперь присоединяем к временной структуре данные из типа приведения
	// и выполняем стандартные проверки
	{
		cop.finalType = const_cast<BaseType*>(&tcoc.castType->GetBaseType());
		cop.constQual = tcoc.castType->IsConst();
		cop.volatileQual = tcoc.castType->IsVolatile();
		PDerivedType fn = cop.dtl.GetHeadDerivedType();
		cop.dtl = tcoc.castType->GetDerivedTypeList();
		cop.dtl.PushHeadDerivedType(fn);
	}

	
	// стандартная проверка списка производных типов	
	CheckDerivedTypeList(cop);		

	// проверка совместимости базового типа и производных
	CheckRelationOfBaseTypeToDerived(cop, true);


	// теперь проверяем, чтобы оператор приведения не содержал параметров
	if( static_cast<const FunctionPrototype &>(*cop.dtl.GetHeadDerivedType()).
			GetParametrList().GetFunctionParametrCount() != 0 )
		theApp.Error( cop.errPos,
			"'%s' - оператор приведения должен объявляться без параметров",
			cop.name.c_str() );
}


// инициализация временной структуры
ConstructorChecker::ConstructorChecker( TempObjectContainer &c, const ClassType &cl )
		: ctor(c), cls(cl), incorrect(false) 
{
	INTERNAL_IF( ctor.name.empty() );
	Check();
}


// инициализация временной структуры
void ConstructorChecker::Check()
{
	// если базового типа нет, присваиваем тип текущего класса
	if( ctor.finalType == NULL )
		ctor.finalType = const_cast<ClassType *>(&cls);

	// 1. Если базовый тип не совпадает с классом, дальнейшие проверки не имеют смысла. 
	//    Выводим ошибку выходим
	if( static_cast<ClassType *>(ctor.finalType) != &cls )
	{
		theApp.Error( ctor.errPos, "в декларации члена пропущено имя");
		incorrect = true;
		return;
	}
	
	// 2. Конструктор должен быть функцией и возвращать ссылку
	if( !ctor.dtl.IsFunction() || ctor.dtl.GetDerivedTypeCount() != 1 )
	{
		theApp.Error( ctor.errPos, 
			"'%s' - конструктор должен быть функцией", ctor.name.c_str());
		incorrect = true;
		return;
	}

	// 3. Конструктор не может иметь спецификаторы хранения, cv-квалификаторы, friend, virtual.
	if( ctor.ssCode != -1 )
		theApp.Error(ctor.errPos,
			"'спецификатор хранения %s' некорректен для конструктора",
			GetKeywordName(ctor.ssCode));

	if( ctor.constQual || ctor.volatileQual || ctor.friendSpec )
		theApp.Error(ctor.errPos,
			"'%s' - %s некорректен в объявлении конструктора",
			ctor.name.c_str(), ctor.friendSpec ? "friend" : "cv-квалификатор");

	if( ctor.fnSpecCode == KWVIRTUAL )
		theApp.Error(ctor.errPos,
			"'%s' - конструктор не может быть виртуальным",
			ctor.name.c_str());

	
	// 4. Конструктор не может содержать cv-квалификаторы функции
	if( static_cast<const FunctionPrototype&>(
			*ctor.dtl.GetHeadDerivedType()).CV_Qualified() != 0 )
		theApp.Error( ctor.errPos, 
			"'%s' - конструктор не может содержать cv-квалификаторы функции", ctor.name.c_str());
}


// метод возвращающий true, если параметр является целым
bool GlobalOperatorChecker::IsInteger( const Parametr &prm ) const
{
	return prm.GetBaseType().GetBaseTypeCode() == BaseType::BT_INT &&
		   prm.GetDerivedTypeList().IsEmpty();
}


// проверить, чтобы параметр был классом, ссылкой на класс, перечислением,
// ссылкой на перечисление
bool GlobalOperatorChecker::IsCompoundType( const Parametr &prm ) const
{
	if( prm.GetBaseType().IsClassType() || prm.GetBaseType().IsEnumType() )
	{
		int cnt = prm.GetDerivedTypeList().GetDerivedTypeCount() ;
		if( cnt == 0 || 
			(cnt == 1 && prm.GetDerivedTypeList().IsReference()) )
			return true;
		else 
			return false;
	}

	else
		return false;
}


// функция проверки глобального перегруженного оператора
void GlobalOperatorChecker::Check()
{
	// сохраняем имя
	op.name = tooc.opFullName;

	if( op.ssCode != -1 && op.ssCode != KWSTATIC && op.ssCode != KWEXTERN )		
		theApp.Error(op.errPos,
			"'спецификатор хранения %s' некорректен для перегруженного оператора",
			GetKeywordName(op.ssCode));

	// проверяем спецификатор friend
	if( op.friendSpec )
		theApp.Error(op.errPos,
			"'спецификатор дружбы friend' некорректен для перегруженного оператора");			
		
	// спецификатор функции не может быть explicit
	if( op.fnSpecCode != -1 && op.fnSpecCode != KWINLINE )	
		theApp.Error( op.errPos,
				"'спецификатор функции %s' некорректен для перегруженного оператора",
				GetKeywordName(op.fnSpecCode) );
					
	// стандартная проверка списка производных типов	
	CheckDerivedTypeList(op);		

	// проверка совместимости базового типа и производных
	CheckRelationOfBaseTypeToDerived(op, false);


	// перегруженный оператор обязательно должен быть функцией
	if( !op.dtl.IsFunction() )
	{
		theApp.Error( op.errPos,
				"'%s' - перегруженный оператор должен быть функцией",
				op.name.c_str() );
		return;
	}

	const FunctionPrototype &fp = 
		static_cast<const FunctionPrototype&>(*op.dtl.GetHeadDerivedType());
	int code = tooc.opCode,
		pcount = fp.GetParametrList().GetFunctionParametrCount();

	// Оператор присваивания, вызова функции, индексации, селектор члена
	// не могут объявляться глобально. 
	if( code == '=' || code == OC_FUNCTION || code == ARROW || 
		code == OC_ARRAY )
	{
		theApp.Error( op.errPos,
				"'%s' - перегруженный оператор может быть только членом класса",
				op.name.c_str() );
		return;
	}


	// перегруженный оператор не может иметь cv-квалификаторов 
	if( fp.CV_Qualified() != 0 )
		theApp.Error( op.errPos,
				"'%s' - перегруженный оператор не может иметь cv-квалификаторов функции",
				op.name.c_str() );
	

	// 1. Перегруженный оператор должен содержать как минимум 
	// один параметр, который является классом, ссылкой на класс, перечислением, 
	// ссылкой на перечисление.

	// если оператор унарный или бинарный	
	if( code == '+' || code == '-' || code == '*' || code == '&' ||
		code == INCREMENT || code == DECREMENT )
	{
		if( pcount == 1 )
		{
			if( !IsCompoundType( *fp.GetParametrList().GetFunctionParametr(0) ) )
				theApp.Error( op.errPos,
					"'%s' - параметр должен иметь тип класса или перечисления"
					"(или ссылки на них)",
					op.name.c_str() );
		}

		else if( pcount == 2 )
		{
			if( code == INCREMENT || code == DECREMENT )
			{
				if( !(IsCompoundType(*fp.GetParametrList().GetFunctionParametr(0)) &&
					  IsInteger(*fp.GetParametrList().GetFunctionParametr(1)) ) )
  				theApp.Error( op.errPos,
					"'%s' - первый параметр должен иметь тип класса или перечисления"
					"(или ссылки на них). Второй параметр должен иметь тип int",
					op.name.c_str() );

			}

			else
			if( !(IsCompoundType( *fp.GetParametrList().GetFunctionParametr(0) ) ||
				  IsCompoundType( *fp.GetParametrList().GetFunctionParametr(1) )) )
				theApp.Error( op.errPos,
					"'%s' - один из параметров должен иметь тип класса или перечисления"
					"(или ссылки на них)",
					op.name.c_str() );
		}

		else
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с 1 или 2 параметрами",
				op.name.c_str() );

	}

	// если оператор унарный
	else if( code == '!' || code == '~' )
	{
		if( pcount == 1 )
		{
			if( !IsCompoundType( *fp.GetParametrList().GetFunctionParametr(0) ) )
				theApp.Error( op.errPos,
					"'%s' - параметр должен иметь тип класса или перечисления"
					"(или ссылки на них)",
					op.name.c_str() );
		}

		else
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с одним параметром",
				op.name.c_str() );
	}

	// если оператор является оператором выделения или освобождения памяти
	else if( code == KWNEW || code == OC_NEW_ARRAY )
	{							 	
		// У оператора new и new[] первый параметр
		// должен быть целым и возвращаемое значение должно быть void *. 
		if( !IsInteger(*fp.GetParametrList().GetFunctionParametr(0)) )
			theApp.Error( op.errPos,
					"'%s' - первый параметр должен быть типа 'size_t'",
					op.name.c_str());

		// возвращаемое значение должно быть типа void*
		if( op.finalType->GetBaseTypeCode() != BaseType::BT_VOID ||
			op.dtl.GetDerivedTypeCount() != 2					 ||
			op.dtl.GetDerivedType(1)->GetDerivedTypeCode() != DerivedType::DT_POINTER )
			theApp.Error( op.errPos,
					"'%s' - возвращаемое значение должно быть типа 'void *'",
					op.name.c_str());

	}
	
	// если оператор освобождения-выделения для массивов
	else if( code == KWDELETE || code == OC_DELETE_ARRAY )
	{
		// декларация оператора деалокации может иметь две формы:
		// 'void operator delete( void *, size_t)' или 'void operator delete(void*)'.	
		bool correct = op.finalType->GetBaseTypeCode() == BaseType::BT_VOID && 
			op.dtl.GetDerivedTypeCount() == 1;
		
		// проверяем первый параметр, он должен быть типа void *
		if( pcount >= 1 )
		{
			const Parametr &prm = *fp.GetParametrList().GetFunctionParametr(0);
			if( prm.GetBaseType().GetBaseTypeCode() != BaseType::BT_VOID	||
				prm.GetDerivedTypeList().GetDerivedTypeCount() != 1			||
				prm.GetDerivedTypeList().GetDerivedType(0)->GetDerivedTypeCode() != 
					DerivedType::DT_POINTER )
				correct = false;
		}
		
		// проверяем второй параметр
		if( pcount == 2 )
		{			
			if( !IsInteger(*fp.GetParametrList().GetFunctionParametr(1)) )
				correct = false;
		}

		else if( pcount != 1 )
			correct = false;

		// теперь, если оператор объявлен не корректно, выведем ошибку
		if( !correct )
			theApp.Error( op.errPos,
				"декларация оператора деалокации может иметь две формы: "
				"'void operator %s(void *)' или 'void operator %s(void *, size_t)'",
				tooc.opString.c_str(), tooc.opString.c_str());
	}
	
	// иначе оператор является бинарным
	else
	{
		if( pcount == 2 )
		{
			if( !(IsCompoundType( *fp.GetParametrList().GetFunctionParametr(0) ) ||
				  IsCompoundType( *fp.GetParametrList().GetFunctionParametr(1) )) )
				theApp.Error( op.errPos,
					"'%s' - один из параметров должен иметь тип класса или перечисления"
					"(или ссылки на них)",
					op.name.c_str() );
		}

		else
			theApp.Error( op.errPos,
				"'%s' - оператор должен объявляться с двумя параметрами",
				op.name.c_str() );
	}


	// проверить наличие параметров по умолчанию, 
	for( int i = 0; i<pcount; i++ )
		if( fp.GetParametrList().GetFunctionParametr(i)->IsHaveDefaultValue() )
		{
			theApp.Error( op.errPos,
				"'%s' - оператор не может иметь параметров по умолчанию",
				op.name.c_str() );			
			break;
		}		
}


// рекурсивная функция, проходит по всему дереву базовых классов и 
// изменяет спецификатор доступа члена в зависимости от спецификатора
// наследования по правилу:  public-(public, protected, no_access), 
// protected-(protected, protected, no_access), private-(no_access, no_access, no_access).
// Функция анализирует всю иерархию так как возможна ситуация когда член
// достижим по нескольким путям, в этом случае выбирается наиболее доступный член
void AccessControlChecker::AnalyzeClassHierarhy( RealAccessSpecifier &ras, 
							ClassMember::AS curAS, const ClassType &curCls, int level )
{
	INTERNAL_IF( level > 1000 );

	// если член принадлжит текущему классу, задаем класс и спец. доступа
	if( ras.pClass == &curCls )
	{
		// если класс уже задан, то задаем его только в случае если
		// текущий спецификатор доступней предыдущего
		if( ras.isClassFound )
		{
			// спецификаторы идут в таком порядке: NOT_CLASS_MEMBER, AS_PRIVATE,
			// AS_PROTECTED, AS_PUBLIC
			if( curAS > ras.realAs )
				ras.realAs = curAS;			
		}

		// иначе задаем как есть и выходим
		else 			
		{
			ras.realAs = curAS;
			ras.isClassFound = true;
		}
		
		return;
	}

	// для каждого базового класса
	register const BaseClassList &bcl = curCls.GetBaseClassList();
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		const BaseClassCharacteristic &clh = *bcl.GetBaseClassCharacteristic(i);

		// вычисляем изменение текущего спецификатора доступа на основании
		// спецификатора доступа в наследовании
		ClassMember::AS nxtAS, bcAS = clh.GetAccessSpecifier();
		
		// наследования по правилу:  public-(public, protected, no_access), 
		// protected-(protected, protected, no_access), 
		// private-(private, private, no_access).
		if( bcAS == ClassMember::AS_PUBLIC )
			nxtAS = curAS == ClassMember::AS_PUBLIC || curAS == ClassMember::AS_PROTECTED ?
				curAS : ClassMember::NOT_CLASS_MEMBER;

		else if( bcAS == ClassMember::AS_PROTECTED )
			nxtAS = curAS == ClassMember::AS_PUBLIC || curAS == ClassMember::AS_PROTECTED  ?
				ClassMember::AS_PROTECTED :	ClassMember::NOT_CLASS_MEMBER;

		else if( bcAS == ClassMember::AS_PRIVATE )
			nxtAS = curAS == ClassMember::AS_PUBLIC || curAS == ClassMember::AS_PROTECTED ?
					ClassMember::AS_PRIVATE : ClassMember::NOT_CLASS_MEMBER;

		else
			INTERNAL( "'AccessControlChecker::AnalyzeClassHierarhy' некорректный "
					  "спецификатор доступа базового класса" );

		// вызываем рекурсию
		AnalyzeClassHierarhy( ras, nxtAS, clh.GetPointerToClass(), level+1);
	}
}


// функция возвращает true, если d является производным классом b
bool AccessControlChecker::DerivedFrom( const ClassType &d, const ClassType &b )
{
	if( &d == &b )
		return true;

	register const BaseClassList &dbcl = d.GetBaseClassList();
	for( int i = 0; i<dbcl.GetBaseClassCount(); i++ )
		if( DerivedFrom( dbcl.GetBaseClassCharacteristic(i)->GetPointerToClass(), b ) )
			return true;	

	return false;
}


// закрытая функция, которая выполняет основную работу класса
void AccessControlChecker::Check()
{
	INTERNAL_IF( member.GetAccessSpecifier() == ClassMember::NOT_CLASS_MEMBER );

	// текущая таблица символов не должна быть локальная
	INTERNAL_IF( curST.IsLocalSymbolTable() );

	// выявляем настоящий спецификатор доступа члена поднимаясь вверх по иерархии,
	// либо сразу получая его если member принадлежит memberCls
	RealAccessSpecifier ras(member.GetAccessSpecifier(), &member) ;
		
	// выявляем спецификатор доступа проходя по иерархии,
	// от класса, через который обращаемся к члену до класса в котором находится член, 
	// изменяя при этом спецификатор доступа. Если член не принадлежит базовому классу,
	// isClassFound будет равен 0
	AnalyzeClassHierarhy( ras, member.GetAccessSpecifier(), memberCls, 0);	

	// далее следует проверка относительно текущей области видимости.
	// Если текущая область видимости глобальная или именованная, значит
	// спецификатор доступа должен быть public
	if( curST.IsGlobalSymbolTable() || curST.IsNamespaceSymbolTable() )
		accessible = ras.realAs == ClassMember::AS_PUBLIC;


	// если текущая область видимости является функцией
	else if( curST.IsFunctionSymbolTable() )
	{
		const Function &fn = static_cast<const FunctionSymbolTable &>(curST).GetFunction();

		// если функция член
		if( fn.IsClassMember() )
		{			
			const ClassType &fnCls = static_cast<const ClassType&>(fn.GetSymbolTableEntry());

			// если спецификатор не выявлен, член не является членом базового класса,
			// значит если он private или protected, заменяем их на no_access
			if( !ras.isClassFound )	
				ras.realAs = member.GetAccessSpecifier() == ClassMember::AS_PUBLIC ?
					ClassMember::AS_PUBLIC : ClassMember::NOT_CLASS_MEMBER;

			// если это функция член класса ras.pClass, значит доступ 
			// разрешен к члену с любым спецификатором доступа, кроме 
			// закрытых членов базовых классов
			if( &fnCls == ras.pClass )
				accessible = ras.realAs != ClassMember::NOT_CLASS_MEMBER;

			// иначе класс к которому принадлежит функция
			// может быть дружественным для класса ras.pClass
			else if( ras.pClass->GetFriendList().FindClassFriend( &fnCls ) >= 0 )
				accessible = true;
						
			// иначе если это функция член класса производного от ras.pClass,
			// значит доступ разрешен для открытых и защищенных членов
			else if( DerivedFrom( fnCls, *ras.pClass ) )				
			{
				// если закрытый член базового класса, он недоступен
				if( ras.realAs == ClassMember::NOT_CLASS_MEMBER )				
				{
					accessible = false;
					return;
				}

				// здесь есть одно исключение. Если доступ к члену произведен
				// не напряму через this, тогда член не может быть доступным,
				// потому что изменяется не текущий объект, а другой. Но только
				// если этот член не является статическим				
				if( &memberCls != &fnCls )
				{
					if( const ::Object *ob = dynamic_cast<const ::Object *>(&member) )
						accessible = ob->GetStorageSpecifier() == ::Object::SS_STATIC ?
							true : ras.realAs == ClassMember::AS_PUBLIC;
					
					else if( const Function *f = dynamic_cast<const Function *>(&member) )
						accessible = f->GetStorageSpecifier() == Function::SS_STATIC ?
							true : ras.realAs == ClassMember::AS_PUBLIC;
					else
						accessible = false;
				}

				// иначе доступны все члены внутри функции-члена производного класса,
				// т.к. закрытые члены базового, отсеились на стадии анализа иерархии
				else
					accessible = true;
			}

			// иначе доступ возможен только к открытым членам
			else
				accessible = ras.realAs == ClassMember::AS_PUBLIC;
		}

		// иначе имеем функцию не член
		else
		{
			// если функция дружественная значит доступны все члены,
			// иначе только открытые
			if( ras.pClass->GetFriendList().FindClassFriend( &fn ) >= 0 )
				accessible = true;
		
			else
				accessible = ras.realAs == ClassMember::AS_PUBLIC;
		}
	}

	// иначе если внутри класса
	else if( curST.IsClassSymbolTable() )
	{
		const ClassType &curCls = static_cast<const ClassType &>(curST);

		// если спецификатор не выявлен, член не является членом базового класса,
		// значит если он private или protected, заменяем их на no_access
		if( !ras.isClassFound )	
			ras.realAs = member.GetAccessSpecifier() == ClassMember::AS_PUBLIC ?
					ClassMember::AS_PUBLIC : ClassMember::NOT_CLASS_MEMBER;

		// если класс является другом, то спцификатор подойдет любой,
		// иначе только public
		if( ras.pClass->GetFriendList().FindClassFriend( &curCls ) >= 0 )
			accessible = true;

		// при наследовании спецификатор должен быть protected или public
		else if( DerivedFrom(curCls, *ras.pClass ) )
			accessible = ras.realAs == ClassMember::AS_PUBLIC ||
				ras.realAs == ClassMember::AS_PROTECTED ;

		else
			accessible = ras.realAs == ClassMember::AS_PUBLIC;
	}

	// иначе ошибка
	else
		INTERNAL( "'AccessControlChecker::Check' передана неизвестная область видимости" );
}


// если сигнатуры у метода 'vm', такая же как у 'method',
// вернуть true. При этом 'vm' должен быть виртуальным
bool VirtualMethodChecker::EqualSignature( const Method *vm )
{
	// vm - должен быть виртуальной функцией
	if( !vm->IsVirtual() )
		return false;

	// проверяем чтобы прототипы функций совпадали
	const FunctionPrototype &fp1 = method.GetFunctionPrototype(), 
							&fp2 = vm->GetFunctionPrototype();	

	// количество параметров должно совпадать
	const FunctionParametrList &fpl1 = fp1.GetParametrList(),
							   &fpl2 = fp2.GetParametrList();

	if( fpl1.GetFunctionParametrCount() != fpl2.GetFunctionParametrCount() )
		return false;

	// cv-квалификаторы также должны совпадать
	if( fp1.IsConst() != fp2.IsConst() || fp1.IsVolatile() != fp2.IsVolatile() )
		return false;

	// проверяем каждый параметр в списке на соответствие	
	for( int i = 0; i<fpl1.GetFunctionParametrCount(); i++ )
		if( !RedeclaredChecker::DeclEqual(*fpl1[i], *fpl2[i]) )
			return false;
		
	// если списки параметров совпали следует полностью проверить
	// типы функций
	if( !RedeclaredChecker::DeclEqual( method, *vm ) )
	{
		theApp.Error( errPos,
			"'%s' - виртуальная функция отлична только типом возвращаемого значения от '%s'",
			method.GetQualifiedName().c_str(), vm->GetQualifiedName().c_str());		
		return false;
	}

	return true;
}


// закрытый метод, выполняет наполнение списка, методами,
// которые совпадают по сигнатуре с имеющимся методом
void VirtualMethodChecker::FillVML( const ClassType &curCls )
{
	// проходим по списку базовых классов, в поисках метода,
	// если имя найдено, поиск прекращается и происходит сравнение
	// сигнатур, если имя - метод
	register const BaseClassList &bcl = curCls.GetBaseClassList();
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		// получаем класс напрямую
		const ClassType &baseCls = bcl.GetBaseClassCharacteristic(i)->GetPointerToClass();
		const VirtualFunctionList &vfl = baseCls.GetVirtualFunctionList();
		VirtualFunctionList::const_iterator pvf = 
			find_if(vfl.begin(), vfl.end(), VMFunctor(method.GetName().c_str()) );

		// если ничего не найдено, вызываем рекурсию
		if( pvf == vfl.end() )
		{
			FillVML( baseCls );
			continue;
		}

		// иначе из имеющегося списка выявляем виртуальные функции,
		// которые имеют такую же сигнатуру, что и текущий метод.
		// Если сигнатуры совпадают, добавить метод в список. Метод
		// должен быть единственным в базовом классе.
		bool haveVm = false;
		for( VirtualFunctionList::const_iterator p = pvf; p != vfl.end(); p++ )
		{			
			if( (*p)->GetName() == method.GetName() && EqualSignature(*p) )
			{
				INTERNAL_IF( haveVm );
				vml.push_back( *p );
				haveVm = true;
			}
		}
	}
}


// выполнить проверку деструткоров ближайших
// базовых классов
void VirtualMethodChecker::CheckDestructor( const ClassType &curCls )
{
	INTERNAL_IF( !curCls.IsDerived() );

	// нам следует проверять только ближайшие базовые классы,
	// т.к. деструкторы генерируются автоматически компилятором,
	// если он не задан явно.
	register const BaseClassList &bcl = curCls.GetBaseClassList();
	for( int i = 0; i<bcl.GetBaseClassCount(); i++ )
	{
		// получаем класс напрямую
		const ClassType &baseCls = bcl.GetBaseClassCharacteristic(i)->GetPointerToClass();
		const VirtualFunctionList &vfl = baseCls.GetVirtualFunctionList();
		VirtualFunctionList::const_iterator pvf = 
			find_if(vfl.begin(), vfl.end(),
				VMFunctor(string("~") + baseCls.GetName().c_str()) );

		if( pvf != vfl.end() )
		{			
			const Method *dtor = *pvf;
			INTERNAL_IF( dtor == NULL );
			if( dtor->IsVirtual() )
			{
				method.SetVirtual(dtor);

				// записываем деструктор в список соответствий
				vml.push_back(dtor);

				// если деструктор является абстрактным, а наш деструктор
				// не является, уменьшить кол-во
				if( method.IsAbstract() )
					break;

				// уменьшаем кол-во абстрактных методов текущего класса
				if( dtor->IsAbstract() )
					const_cast<ClassType &>(curCls).DecreaseAbstractMethods();
			}
		}
	}
}


// закрытая ф-ция выполняет проверку
void VirtualMethodChecker::Check()
{
	// если метод статический или конструктор, не выполнять проверок 
	if( method.GetStorageSpecifier() == Function::SS_STATIC ||
		method.IsConstructor() )
		return;

	const ClassType *cls = dynamic_cast<const ClassType *>(&method.GetSymbolTableEntry());
	INTERNAL_IF( cls == NULL );

	// класс не является производным, проверка виртуальной функции не имеет
	// смысла
	if( !cls->IsDerived() )
		return;

	// если метод является деструктором, выполнить для него обход только
	// ближайших базовых классов в поисках виртуальных деструкторов,
	// если  хотя-бы один класс имеет вирт. деструктор, значит присваиваем
	// виртуальность нашему
	if( method.IsDestructor() )
	{
		CheckDestructor( *cls );
		return ;
	}

	// выявляем роль метода
	destRole = NameManager::GetIdentifierRole(&method);

	// иначе заполняем список кандидатов
	FillVML( *cls );

	// если список не пустой, присваиваем виртуальность текущей функции,
	// и проверяем
	if( !vml.empty() )
	{
		// задаем корневой метод - первый элемент в списке соотв.
		// Для нас не имеет значения какой метод из всего списка будет
		// корневым для декларируемого, т.к. указатель на него в V-таблице
		// все равно задается во все места
		method.SetVirtual( vml.front() );

		// если метод не является абстрактным, уменьшить кол-во абстрактных
		// методов, на кол-во абстрактных методов в списке
		if( !method.IsAbstract() )
			for( VML::iterator p = vml.begin(); p != vml.end(); p++ )
				if( (*p)->IsAbstract() )
					const_cast<ClassType *>(cls)->DecreaseAbstractMethods();
	}
}
