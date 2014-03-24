// реализация интерфеса работающего с перегрузкой и перекрытием имен - Overload.cpp

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



// сравнивает destID с типизированной сущностью и если их типы не эквивалентны,
// вернуть false. Метод учитывает тип входного параметра. Например у 
// параметра функции не проверяется константность и throw-спецификация.
// Следует учесть, что метод не проверяет спецификаторы хранения
bool RedeclaredChecker::DeclEqual( const TypyziedEntity &ob1, const TypyziedEntity &ob2 )
{
	// 1. проверяем базовые типы
	const BaseType &bt1 = ob1.GetBaseType(),
				   &bt2 = ob2.GetBaseType();

	// проверяем сначала коды, все коды должны совпадать
	if( bt1.GetBaseTypeCode() != bt2.GetBaseTypeCode() ||
		bt1.GetSignModifier() != bt2.GetSignModifier() ||
		bt1.GetSizeModifier() != bt2.GetSizeModifier() )
		return false;

	// если типы относятся к составным типам таким как класс или перечисление
	// их указатели должны совпадать
	BaseType::BT bc = bt1.GetBaseTypeCode();
	if( bc == BaseType::BT_CLASS || bc == BaseType::BT_STRUCT || 
		bc == BaseType::BT_UNION || bc == BaseType::BT_ENUM )
		if( &bt1 != &bt2 )
			return false;

	// 2. проверяем cv-квалификаторы	
	if( ob1.IsConst() != ob2.IsConst() ||
		ob1.IsVolatile() != ob2.IsVolatile() )
	{
		// у параметров, можно проигнорировать, если 
		// квалификатор относится к базовому типу, не к производному
		if( ob1.IsParametr() && ob1.GetDerivedTypeList().IsEmpty() &&
			ob2.IsParametr() && ob2.GetDerivedTypeList().IsEmpty() )
			;
		else
			return false;
	}


	// 3. проверяем список производных типов
	const DerivedTypeList &dtl1 = ob1.GetDerivedTypeList(),
						  &dtl2 = ob2.GetDerivedTypeList();

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

		// если ссылка, то проверять нечего
		if( dc1 == DerivedType::DT_REFERENCE )
			;

		// если указатель, проверим квалификаторы
		else if( dc1 == DerivedType::DT_POINTER )
		{
			if( i == 0 && ob1.IsParametr() && ob2.IsParametr() )
				continue;

			if( ((const Pointer &)dt1).IsConst() != ((const Pointer &)dt2).IsConst() ||
				((const Pointer &)dt1).IsVolatile() != ((const Pointer &)dt2).IsVolatile() )
				return false;
		}

		// если указатель на член проверим квалификаторы и класс к которому
		// принадлежит указатель
		else if( dc1 == DerivedType::DT_POINTER_TO_MEMBER )
		{
			if( i == 0 && ob1.IsParametr() && ob2.IsParametr() )
				continue;

			const PointerToMember &ptm1 = static_cast<const PointerToMember &>(dt1),
								  &ptm2 = static_cast<const PointerToMember &>(dt2);

			if( ptm1.IsConst() != ptm2.IsConst()			||
				ptm1.IsVolatile() != ptm2.IsVolatile()		||
				&ptm1.GetMemberClassType() != &ptm2.GetMemberClassType() )
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
				if( !DeclEqual(*fpl1[i], *fpl2[i]) )
					return false;

			// проверяем cv-квалификаторы
			if( fp1.IsConst() != fp2.IsConst() ||
				fp1.IsVolatile() != fp2.IsVolatile() )
				return false;

			// проверяем throw-спецификацию
			if( !ob1.IsParametr() )
			{
			if( fp1.CanThrowExceptions() != fp2.CanThrowExceptions() )
				return false;

			// проверяем список throw-типов
			const FunctionThrowTypeList &ftt1 = fp1.GetThrowTypeList(),
									    &ftt2 = fp2.GetThrowTypeList();
				
			// количество должно быть равно
			if( ftt1.GetThrowTypeCount() != ftt2.GetThrowTypeCount() )
				return false;

			// проверяем каждый тип в спецификации
			for( i = 0; i<ftt1.GetThrowTypeCount(); i++ )
				if( !DeclEqual(*ftt1[i], *ftt2[i]) )
					return false;
			}
		}

		// иначе неизвестный код
		else
			INTERNAL("'RedeclaredChecker::DeclEqual' неизвестный код производного типа");
	}

	return true;
}


// пробегает по списку ролей и пытается найти функцию
// совпадающую по прототипу с имеющийся. Если функция найдена, возвращается
// указатель на нее, иначе NULL
const Function *RedeclaredChecker::FnMatch( ) const
{
	INTERNAL_IF( !(destIDRole >= R_FUNCTION && destIDRole <= R_CONSTRUCTOR) );

	const Function *rval = NULL;
	const FunctionPrototype &fp1 = dynamic_cast<const FunctionPrototype &>(
		*destID.GetDerivedTypeList().GetHeadDerivedType());

	for( RoleList::const_iterator p = roleList.begin(); p != roleList.end(); p++ )
	{
		if( (*p).second != destIDRole )
			continue;

		// проверяем чтобы прототипы функций совпадали
		const Function *cand = dynamic_cast<const Function *>((*p).first);
		const FunctionPrototype &fp2 = cand->GetFunctionPrototype();
		INTERNAL_IF( cand == NULL );

		// объявление перегруженных функций в локальной области видимости,
		// следует запретить из-за неверной генерации кода
		if( (destIDRole == R_FUNCTION || destIDRole == R_OVERLOAD_OPERATOR) &&
			(GetCurrentSymbolTable().IsFunctionSymbolTable() || 
			 GetCurrentSymbolTable().IsLocalSymbolTable()) )
			theApp.Error(errPos, 
				"'%s' - объявление локальной перегруженной функции невозможно",
				cand->GetName().c_str());

		// количество параметров должно совпадать
		const FunctionParametrList &fpl1 = fp1.GetParametrList(),
								   &fpl2 = fp2.GetParametrList();

		if( fpl1.GetFunctionParametrCount() != fpl2.GetFunctionParametrCount() )
			continue;

		// cv-квалификаторы также должны совпадать
		if( fp1.IsConst() != fp2.IsConst() || fp1.IsVolatile() != fp2.IsVolatile() )
			continue;

		// проверяем каждый параметр в списке на соответствие
		bool eq = true;
		for( int i = 0; i<fpl1.GetFunctionParametrCount(); i++ )
			if( !DeclEqual(*fpl1[i], *fpl2[i]) )
			{
				eq = false;
				break;
			}

		// если списки параметров совпали следует полностью проверить
		// типы функций и спецификаторы хранения
		if( eq )
		{
			// может быть только одно соотв. перегруженной функции
			if( rval != NULL )
				INTERNAL( "несколько соответствий перегруженной функции" );

			rval = cand;
			if( !DeclEqual( destID, *cand ) || 
				(destID.IsFunction() && 
				  (cand->GetStorageSpecifier() != 
				    static_cast<const Function&>(destID).GetStorageSpecifier() ||
				   cand->GetCallingConvention() != 
				    static_cast<const Function&>(destID).GetCallingConvention()) ) )
				theApp.Error( errPos,
					"'%s' - несоответствие типа с предыдущей декларацией",
					cand->GetName().c_str());
		}			
	}

	return rval;
}


// проверить, возможно ли объявление функции или объекта в текущей
// области видимости. Если невозможно устанавливает redeclared в false
void RedeclaredChecker::Check() 
{
	INTERNAL_IF( !(destIDRole >= R_OBJECT && destIDRole <= R_CONSTRUCTOR) );
	bool isFn = destIDRole >= R_FUNCTION && destIDRole <= R_CONSTRUCTOR;

	// в первую очередь, если список ролей пустой, значит ничего проверять
	// не нужно
	if( roleList.empty() )
		return;

	// далее следует проверить список ролей, мы игнорируем те имена,
	// которые являются классом или перечислением, а также те которые 
	// имеют код нашего идентификатора
	for( RoleList::const_iterator p = roleList.begin(); p != roleList.end(); p++ )
	{
		if( (*p).second == R_CLASS_TYPE || (*p).second == R_UNION_CLASS_TYPE ||
			(*p).second == R_ENUM_TYPE )
		{
			// проверяем, если объявляется typedef, то это ошибка
			if( destIDRole == R_OBJECT )
			{
				const ::Object *ob = dynamic_cast<const ::Object *>(&destID);
				INTERNAL_IF( ob == NULL );
				if( ob->GetStorageSpecifier() == ::Object::SS_TYPEDEF )
				{
					redeclared = true;
					theApp.Error(errPos,
						"'%s' - тип уже объявлен", ob->GetQualifiedName().c_str());
					return;
				}
			}

			// иначе игнорируем тип
			continue;
		}

		// если коды совпали, проверяем возможность переопределения
		if( (*p).second == destIDRole )
		{
			// соответствие перегруженных функций проверим позже
			if( isFn )
				;

			// иначе если объект, необходимо чтобы он был 
			// typedef или extern и типы совпадали
			else
			{
				INTERNAL_IF( prevID != NULL );
				prevID = (*p).first;
				redeclared = true;
				
				// если получили действительный объект, проверим вместе со спецификаторами,
				// иначе сравним только типы
				if( const ::Object *ob1 = dynamic_cast<const ::Object *>(&destID) )
				{
					const ::Object &ob2 = static_cast<const ::Object&>(*prevID);								
					::Object::SS ss1 = ob1 ? ob1->GetStorageSpecifier() : ::Object::SS_NONE,
								 ss2 = ob2.GetStorageSpecifier();

					if( ob1->IsCLinkSpecification() != ob2.IsCLinkSpecification() )
						goto err;
					if( ss1 != ss2 )
					{
						// проверяем, если статический член определяется
						// в глобальной области видимости
						if( ss2 == ::Object::SS_STATIC && ob2.IsClassMember() &&
							ss1 == ::Object::SS_NONE && 
							!GetCurrentSymbolTable().IsClassSymbolTable() )
							;

						else if( ss2 != ::Object::SS_EXTERN && ss1 != ::Object::SS_NONE )
							goto err;

						if( !DeclEqual(destID, ob2) )
							goto err;
					}
					
					else if( (ss1 != ::Object::SS_TYPEDEF && ss1 != ::Object::SS_EXTERN ) ||
						!DeclEqual(destID, ob2) )
					err:
						theApp.Error(errPos, 
							"'%s' - уже объявлен", 
							ob2.GetQualifiedName().c_str());
				}

				// иначе сравним только типы
				else if( !DeclEqual(destID, static_cast<const ::Object&>(*prevID)) ) 	
					theApp.Error(errPos, 
						"'%s' - не совпадение типов с предыдущей декларацией", 
						prevID->GetQualifiedName().c_str());
			}
		}

		// в противном случае имя уже используется и это считается ошибкой
		else 
		{
			theApp.Error(errPos,
				"'%s' - имя переопределено", (*p).first->GetName().c_str());
			prevID = (*p).first;
			redeclared = true;
			return ;
		}
	}

	// если функция, следует проверить возможность ее определения среди перегруженных
	// функций
	if( isFn )			
		if( const Function *fn = FnMatch() )
		{
			prevID = fn;
			redeclared = true;
		}	
}


// функция, используется для определения семантической группы имени. 
// Во-первых, все коды в списке, должны быть равны r, во вторых,
// если правило один верно, значит каждый указатель в списке, должен
// быть равен первому указателю в списке. В случае если оба правила
// равны, возвращается идентификатор, иначе - NULL
const Identifier *AmbiguityChecker::IsDestRole( Role r ) const
{
	Identifier *rval = NULL;

	if( rlist.empty() || rlist.front().second != r )
		return NULL;

	rval = rlist.front().first;
	for( RoleList::const_iterator p = rlist.begin(); p != rlist.end(); p++ )
	{
		// если роли не совпадают либо указатели
		if( (*p).second != r || (*p).first != rval )
		{
			// выводим сообщ. только если идентификаторы из разных ОВ
			if( diagnostic )
			{
				if( &(*p).first->GetSymbolTableEntry() != &rval->GetSymbolTableEntry() )				
				theApp.Error(errPos, "неоднозначность между '%s' и '%s'", 
					rval->GetQualifiedName().c_str(), (*p).first->GetQualifiedName().c_str());

				// все равно возвращаем rval, чтобы вызывающая функция могла его
				//	проанализировать
				return rval;		
			}

			// иначе 0
			else
				return NULL;
		}
				
	}
	
	return rval;
}


// если имя является синонимом типа typedef, вернуть указатель на него
const ::Object *AmbiguityChecker::IsTypedef( ) const
{	
	if( rlist.empty() )
		return NULL;
	
	Role r = (*rlist.begin()).second;
	if( r != R_OBJECT  && r != R_DATAMEMBER )
		return NULL;
	
	const ::Object *rval = static_cast<const ::Object *>(IsDestRole(r));
	if( !rval || rval->GetStorageSpecifier() != ::Object::SS_TYPEDEF )
		return NULL;
	return rval;
}


// проверить является ли тип перечислением, если параметр withOverload == true,
// тогда достаточно, чтобы роль присутствовала в списке, в противном случае,
// роль должна быть одна
const EnumType *AmbiguityChecker::IsEnumType( bool withOverload ) const
{
	if( withOverload )
	{
		// следует проверить весь список, т.к. в списке может быть несколько
		// типов, в этом случае будет неоднозначность и следует вернуть NULL
		EnumType *rval = NULL;
		for( RoleList::const_iterator p = rlist.begin(); p != rlist.end(); p++ )
		{
			// любое имя кроме объекта, функции, константы перечисления,
			// а также производных от них, не может сосуществовать с перечислимым типом.
			// Причем даже если объект объявлен как typedef, это ничего не меняет,
			// т.к. ключ перечисления задается явно (флаг withOverload == true)
			Role r = (*p).second; 
			if( r == R_OBJECT	|| r == R_DATAMEMBER || r == R_PARAMETR  ||
				r == R_ENUM_CONSTANT ||  r == R_CLASS_ENUM_CONSTANT      ||
				r == R_FUNCTION		 ||  r == R_METHOD )
				continue;
	
			else if( (*p).second == R_ENUM_TYPE )
			{
				if( rval == NULL )	
					rval = static_cast<EnumType *>( (*p).first );

				// перечисление уже задано. Если их указатели равны,
				// такое может случится при наследовании или using,
				// тогда это не считается неоднозначностью
				else if( rval == (*p).first ) 
					continue;

				// иначе неоднозначность
				else
				{
					if( diagnostic )
					theApp.Error(errPos, "неоднозначность между '%s' и '%s'", 
						rval->GetQualifiedName().c_str(), 
						(*p).first->GetQualifiedName().c_str());

					// возвращаем rval, чтобы вызывающая функция
					// могла его проанализировать
					return rval;	
				}
			}

			// в противном случае имя не может сосуществовать с перечислимым
			// типом и не является им
			else
				return NULL;
		}

		return rval;
	}

	else
	{
		return static_cast<const EnumType *>( IsDestRole( R_ENUM_TYPE) );
	}
}


// проверить является ли тип классом, если параметр withOverload == true,
// тогда достаточно, чтобы роль присутствовала в списке, в противном случае,
// роль должна быть одна
const ClassType *AmbiguityChecker::IsClassType( bool withOverload ) const
{
	if( withOverload )
	{
		// следует проверить весь список, т.к. в списке может быть несколько
		// типов, в этом случае будет неоднозначность и следует вернуть NULL
		ClassType *rval = NULL;
		for( RoleList::const_iterator p = rlist.begin(); p != rlist.end(); p++ )
		{
			Role r = (*p).second; 
			if( r == R_OBJECT		 || r == R_DATAMEMBER || r == R_PARAMETR  ||
				r == R_ENUM_CONSTANT || r == R_CLASS_ENUM_CONSTANT      ||
				r == R_FUNCTION		 || r == R_METHOD )
				continue;
	
			else if( (*p).second == R_CLASS_TYPE || 
					 (*p).second == R_UNION_CLASS_TYPE )
			{
				if( rval == NULL )	
					rval = static_cast<ClassType *>( (*p).first );

				else if( rval == (*p).first ) 
					continue;

				// иначе неоднозначность
				else
				{
					if( diagnostic )
						theApp.Error(errPos, "неоднозначность между '%s' и '%s'", 
						rval->GetQualifiedName().c_str(), 
						(*p).first->GetQualifiedName().c_str() );
					return rval;
				}
			}

			// в противном случае имя не может сосуществовать с перечислимым
			// типом и не является им
			else
				return NULL;
		}
		
		return rval;
	}

	else
	{
		if( rlist.empty() || 
			(rlist.front().second != R_CLASS_TYPE && 
			 rlist.front().second != R_UNION_CLASS_TYPE) )
			return NULL;

		return static_cast<const ClassType *>( IsDestRole(rlist.front().second) );
	}
}


// проинспектировать имя - является ли оно именем типа,
// т.е. классом, перечислением, типом typedef, шаблонным параметром типа.
// Если у имени > 1 роль, вернуть false, т.к. имя типа должно быть уникальным
// в своей области видимости, в противном случае оно перекрывается 
const Identifier *AmbiguityChecker::IsTypeName( bool withOverload  )  const
{
	const Identifier *id = NULL;

	// если имеем дело с шаблонным параметром типа
	if( IsDestRole(R_TEMPLATE_TYPE_PARAMETR) != NULL )
		return (*rlist.begin()).first;
	
	else if( (id = IsTypedef()) != NULL )
		return id;

	else if( (id = IsClassType( withOverload)) != NULL )
		return id;

	else if( (id = IsEnumType( withOverload)) != NULL )
		return id;

	// иначе не имя типа
	return NULL;
}


// если имя является шаблонным классом
const TemplateClassType *AmbiguityChecker::IsTemplateClass( ) const
{
	return static_cast<const TemplateClassType *>( IsDestRole( R_TEMPLATE_CLASS) );		
}


// если имя является именованной областью видимости
const NameSpace *AmbiguityChecker::IsNameSpace( ) const
{
	return static_cast<const NameSpace *>( IsDestRole(R_NAMESPACE) );
}

