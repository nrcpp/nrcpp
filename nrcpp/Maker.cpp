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
#include "Overload.h"
#include "Body.h"
#include "ExpressionMaker.h"

using namespace MakerUtils;


// функция создающая указатель на член по пакету,
// является дружественной для класса NodePackage 
inline static PointerToMember *MakePointerToMember( NodePackage &ptm )
{
	INTERNAL_IF( ptm.GetPackageID() != PC_POINTER_TO_MEMBER );
	INTERNAL_IF( ptm.GetChildPackageCount() < 3 );

	int lpid = ptm.GetLastChildPackage()->GetPackageID();
	bool cq = false, vq = false;

	// если укзатель на член содержит cv-квалификаторы
	if( lpid == KWCONST || lpid == KWVOLATILE )
	{
		(lpid == KWCONST ? cq : vq) = true;
		delete ptm.childPackageList.back();
		ptm.childPackageList.pop_back();

		// может быть еще один квалификатор
		lpid = ptm.GetLastChildPackage()->GetPackageID();
		if( lpid == KWCONST || lpid == KWVOLATILE )
			(lpid == KWCONST ? cq : vq) = true,
			delete ptm.childPackageList.back(),
			ptm.childPackageList.pop_back();
	}

	INTERNAL_IF( ptm.GetLastChildPackage()->GetPackageID() != '*' );

	// отсоединяем последний две лексемы для получения класса
	delete ptm.childPackageList.back();
	ptm.childPackageList.pop_back();
	delete ptm.childPackageList.back();
	ptm.childPackageList.pop_back();

	// получим позицию имени
	Position epos = ParserUtils::GetPackagePosition(&ptm);
		
	// задаем код квалифицированного имени
	ptm.SetPackageID(PC_QUALIFIED_NAME);
	QualifiedNameManager qnm(&ptm);
	AmbiguityChecker achk(qnm.GetRoleList(), epos, true);
	
	const ClassType *cls = NULL;
		
	cls = achk.IsClassType(true);			
	if( cls == NULL )		// если имя не является классом, возможно это имя typedef	
		if( const ::Object *id = achk.IsTypedef() )
			cls = CheckerUtils::TypedefIsClass(*id);

	// создаем указатель на член, если класс найден
	if( cls != NULL )
	{
		// проверяем класс на доступность и возвращаем создаваемый указатель на член
		CheckerUtils::CheckAccess(qnm, *cls, epos);		
		return new PointerToMember(cls, cq, vq);
	}
			
	// иначе класс не найден
	else
	{
		theApp.Error( ParserUtils::GetPackagePosition(&ptm),
			"'%s' - не является классом", ParserUtils::PrintPackageTree(&ptm).c_str());
		return NULL;
	}
}


// функция сбора информации из пакета во временную структуру,
// используется при сборе информации при декларации объектов и функций
bool MakerUtils::AnalyzeTypeSpecifierPkg( const NodePackage *typeSpecList, 
			TempObjectContainer *tempObjectContainer, bool canDefineImplicity )
{		
	bool errorFlag = false;
	// cначала обрабатываем спецификаторы типа
	for( int i = 0; i < typeSpecList->GetChildPackageCount(); i++ )
	{
		// синоним типа, или перечисление, или класс, или шаблонный класс
		if( typeSpecList->GetChildPackage(i)->IsNodePackage() )
		{
			INTERNAL_IF( 
				typeSpecList->GetChildPackage(i)->GetPackageID() != PC_QUALIFIED_TYPENAME);

			QualifiedNameManager qnm( (NodePackage *)typeSpecList->GetChildPackage(i) );
			AmbiguityChecker achk(qnm.GetRoleList(),tempObjectContainer->errPos, true);
			const Identifier *chkId = NULL;

			if( const ::Object *td = achk.IsTypedef() )
				tempObjectContainer->baseType = 
					new TempObjectContainer::SynonymType(*td), chkId = td ; 

			else if( const ClassType *ct = achk.IsClassType(false) )
				tempObjectContainer->baseType = 
					new TempObjectContainer::CompoundType(*ct), chkId = ct ;

			else if( const EnumType *et = achk.IsEnumType(false) )
				tempObjectContainer->baseType = 
					new TempObjectContainer::CompoundType(*et), chkId = et;

			else
				INTERNAL("'AnanlyzeTypeSpecifierPkg' принимает узловой "
						 "пакет с неизвестным кодом");		
			
			// проверим тип на доступность
			CheckerUtils::CheckAccess(qnm, *chkId, tempObjectContainer->errPos);
			continue;
		}

		// иначе имеем лексему
		const Lexem &lxm = ((LexemPackage *)typeSpecList->GetChildPackage(i))->GetLexem();		
		TypeSpecifierManager ts(lxm);
		bool error = false;

		// встроенный базовый тип
		if( ts.IsBaseType() )
		{
			// если базовый тип уже задан
			if( tempObjectContainer->baseType != NULL )
			{
				BaseType::BT &bt = ((TempObjectContainer::ImplicitType *)tempObjectContainer->
						baseType)->baseTypeCode;

				if( tempObjectContainer->baseType->IsImplicit() &&
					bt == BaseType::BT_NONE )
					bt = ts.CodeToBaseType();
				else
					error = true;
			}
				
			else
				tempObjectContainer->baseType = 
					new TempObjectContainer::ImplicitType( ts.CodeToBaseType() );
		}

		// если спецификатор класса
		else if( ts.IsClassSpec() )
		{
			if( tempObjectContainer->baseType != NULL )
				error = true;

			// если класс безимянный, это считается ошибкой 
			else if( i+1 == typeSpecList->GetChildPackageCount() ||
					 typeSpecList->GetChildPackage(i+1)->GetPackageID() != PC_QUALIFIED_NAME )
				theApp.Error(lxm.GetPos(), "пропущено имя после '%s'", lxm.GetBuf().c_str());

			// определяем класс, только если это не запрещено
			else
			{
				i++;	// игнорируем имя
				NodePackage *tsl = const_cast<NodePackage *>(typeSpecList);
				BaseType *bt = NULL;

				// создаем динамически (или находим существующий) класс, если это
				// не запрещено
				if( canDefineImplicity )
					bt = lxm == KWENUM 
					? (BaseType *)EnumTypeMaker(tsl, tempObjectContainer->curAccessSpec).Make() 
					: (BaseType *)ClassTypeMaker(tsl, tempObjectContainer->curAccessSpec).Make();

				// иначе только находим
				else
				{
					// ищем
					QualifiedNameManager qnm((NodePackage *)tsl->GetChildPackage(i));
					AmbiguityChecker achk(qnm.GetRoleList());

					// проверяем, если тип не найден задаем как void и выводим ошибку
					if( (bt = (lxm == KWENUM ? (BaseType*)achk.IsEnumType(true) :
							(BaseType*)achk.IsClassType(true)) ) == NULL )
					{
						theApp.Error(tempObjectContainer->errPos,
							"'%s %s' - тип не объявлен; динамическое объявление также невозможно",
							lxm.GetBuf().c_str(), 
							ParserUtils::PrintPackageTree((NodePackage *)
								typeSpecList->GetChildPackage(i)).c_str());

						// задаем тип как void
						tempObjectContainer->baseType = 
							new TempObjectContainer::ImplicitType( BaseType::BT_VOID );
						continue;
					}
				}

				if( bt != NULL )
					tempObjectContainer->baseType = 
							new TempObjectContainer::CompoundType(*bt);					
				
			}
		}
		
		else if( ts.IsCVQualifier() )
		{
			(lxm == KWCONST ? 
				tempObjectContainer->constQual : tempObjectContainer->volatileQual) = true;
		}

		else if( ts.IsSignModifier() || ts.IsSizeModifier() )
		{
			if( tempObjectContainer->baseType == NULL )
			{
				TempObjectContainer::ImplicitType *bt = new TempObjectContainer::ImplicitType;
				if( ts.IsSignModifier() )
					bt->signMod = ts.CodeToSignModifier();
				else
					bt->sizeMod = ts.CodeToSizeModifier();
				tempObjectContainer->baseType = bt;
			}

			else if( tempObjectContainer->baseType->IsImplicit() )
			{
				TempObjectContainer::ImplicitType *bt = 
					 static_cast<TempObjectContainer::ImplicitType *>
						(tempObjectContainer->baseType);

				if( ts.IsSignModifier() )
				{
					if( bt->signMod != BaseType::MN_NONE )
						error = true;
					else
						bt->signMod = ts.CodeToSignModifier();
				}

				else
				{
					if( bt->sizeMod != BaseType::MZ_NONE )
						error = true;
					else
						bt->sizeMod = ts.CodeToSizeModifier();
				}
			}

			else
				error = true;			
		}
		

		else if( ts.IsStorageSpecifier() )
		{
			// -1, если код не задан
			if( tempObjectContainer->ssCode != -1  )
				error = true;
			else
			{
				tempObjectContainer->ssCode = lxm;
				// проверим, если следующая лексема строка и
				// текущая extern, значит задаем спецификацию
				if( lxm == KWEXTERN && 
					i != typeSpecList->GetChildPackageCount()-1 &&
					typeSpecList->GetChildPackage(i+1)->GetPackageID() == STRING )
				{
					string linkSpec = ((LexemPackage *)typeSpecList->GetChildPackage(i+1))->
							GetLexem().GetBuf().c_str();
					if( linkSpec == "\"C\"" )
						tempObjectContainer->clinkSpec = true;
					else if( linkSpec == "\"C++\"" )
						tempObjectContainer->clinkSpec = false;
					else
						theApp.Error(tempObjectContainer->errPos,
							"%s - неизвестная спецификация связывания",
							linkSpec.c_str());
					i++;
				} 
			}
		}

		// спецификатор функции
		else if( ts.IsFunctionSpecifier() )
		{
			if( tempObjectContainer->fnSpecCode != -1 )
				error = true;
			else
				tempObjectContainer->fnSpecCode = lxm;
		}

		// спецификатор дружбы
		else if( ts.IsFriend() )
		{
			if( tempObjectContainer->friendSpec )
				error = true;
			else
				tempObjectContainer->friendSpec = true;

		}

		else
		{
			if( ts.IsUncknown() )
				theApp.Error(lxm.GetPos(), "использование '%s' некорректно при объявлении объекта",
					lxm.GetBuf().c_str());
			else
				theApp.Error(lxm.GetPos(), 
					"использование '%s %s' некорректно при объявлении объекта",
						ts.GetGroupNameRU().c_str(), lxm.GetBuf().c_str());
		}

		// перехват ошибки
		if( error )
		{
			theApp.Error(lxm.GetPos(), "использование '%s' некорректно, '%s' уже задан",
					lxm.GetBuf().c_str(), ts.GetGroupNameRU().c_str());
			errorFlag = true;		// обозначаем, что объявление составлено некорректно
		}
	}

	return errorFlag;
}


// метод сбора информации из пакета с декларатором
// во временную структуру tempObjectContainer
void MakerUtils::AnalyzeDeclaratorPkg( const NodePackage *declarator, 
					TempObjectContainer *tempObjectContainer )
{	
	for( int i = 0; i<declarator->GetChildPackageCount(); i++ )
	{		
		// имя пропускаем, т.к. в конструкторе TempObjectContainer
		// оно уже определялось
		if( declarator->GetChildPackage(i)->GetPackageID() == PC_QUALIFIED_NAME )
			continue;

		else if( declarator->GetChildPackage(i)->GetPackageID() == '*' )
		{
			bool c = false, v = false;

			// если не последний, проверим cv-квалификаторы
		check_cv:
			if( i != declarator->GetChildPackageCount()-1 )			
				if( declarator->GetChildPackage(i+1)->GetPackageID() == KWCONST )
				{
					c = true, i++;
					goto check_cv;
				}

				else if( declarator->GetChildPackage(i+1)->GetPackageID() == KWVOLATILE )
				{
					v = true, i++;
					goto check_cv;
				}				
			
			// создаем и добавляем указатель
			tempObjectContainer->dtl.AddDerivedType( new Pointer(c,v) );
		}

		else if( declarator->GetChildPackage(i)->GetPackageID() == '&' )
			tempObjectContainer->dtl.AddDerivedType( new Reference );

		else if( declarator->GetChildPackage(i)->GetPackageID() == PC_FUNCTION_PROTOTYPE )
		{
			FunctionPrototypeMaker fpm( *(NodePackage *)declarator->GetChildPackage(i));
			FunctionPrototype *fp = fpm.Make();
			INTERNAL_IF( fp == NULL );
			tempObjectContainer->dtl.AddDerivedType( fp );
		}

		else if( declarator->GetChildPackage(i)->GetPackageID() == PC_POINTER_TO_MEMBER )
		{
			if( PointerToMember *ptm = MakePointerToMember( 
					*(NodePackage *)declarator->GetChildPackage(i)) )
				tempObjectContainer->dtl.AddDerivedType( ptm );	
			
		}

		else if( declarator->GetChildPackage(i)->GetPackageID() == PC_ARRAY )
		{
			const NodePackage &ar = *static_cast<const NodePackage *>(
												declarator->GetChildPackage(i));

			INTERNAL_IF( ar.GetChildPackageCount() < 2 || ar.GetChildPackageCount() > 3 );

			// два дочерних пакета - '[' и ']'
			if( ar.GetChildPackageCount() == 2 )			
				tempObjectContainer->dtl.AddDerivedType( new Array );
			
			// иначе анализируем выражение
			else
			{
				INTERNAL_IF( ar.GetChildPackage(0)->GetPackageID() != '[' ||
					ar.GetChildPackage(2)->GetPackageID() != ']' );
				INTERNAL_IF( !ar.GetChildPackage(1)->IsExpressionPackage() );
				const POperand &exp = static_cast<const ExpressionPackage *>(
					ar.GetChildPackage(1))->GetExpression();

				// выражение должно быть целое и константное
				double arSize;
				if( ExpressionMakerUtils::IsInterpretable(exp, arSize) &&
					exp->GetType().GetBaseType().GetBaseTypeCode() != BaseType::BT_FLOAT &&
					exp->GetType().GetBaseType().GetBaseTypeCode() != BaseType::BT_DOUBLE )
					tempObjectContainer->dtl.AddDerivedType( new Array(arSize) );

				// иначе создаем без размера
				else
					tempObjectContainer->dtl.AddDerivedType( new Array );
			}
		}		

		else
			INTERNAL( "'AnalyzeDeclaratorPkg' получил некорректный входной пакет");
	}
}


// уточнить базовый тип временного объекта: 1. определить есть ли базовый тип,
// и если нет то добавить тип по умолчанию, 2. если базовый тип задан как
// синоним типа typedef, преобразовать 
void MakerUtils::SpecifyBaseType( TempObjectContainer *tempObjectContainer )
{
	// базовый тип не задан, создать встроенный базовый тип по умолчанию
	if( tempObjectContainer->baseType == NULL )
	{
		theApp.Warning(tempObjectContainer->errPos, 
			"'%s' - пропущен базовый тип",
			tempObjectContainer->name.c_str() );

		tempObjectContainer->finalType = (BaseType *)
			&ImplicitTypeManager(BaseType::BT_INT).GetImplicitType();
	}

	// если базовый тип создан, но не задан, задаем его как int
	else if( tempObjectContainer->baseType->IsImplicit() )
	{ 
		TempObjectContainer::ImplicitType *it = (TempObjectContainer::ImplicitType *)
				tempObjectContainer->baseType;

		if( it->baseTypeCode == BaseType::BT_NONE )
			it->baseTypeCode = BaseType::BT_INT;
		
		// следует проверка модификаторов
		// проверяем модификатор знака 
		if( it->signMod != BaseType::MN_NONE && it->baseTypeCode != BaseType::BT_CHAR &&
			it->baseTypeCode != BaseType::BT_INT )
		{
			theApp.Error(tempObjectContainer->errPos, 
				"'модификатор знака %s' может использоваться только с типом 'char', либо 'int'",
				it->signMod == BaseType::MN_SIGNED ? "signed" : "unsigned");
			it->signMod = BaseType::MN_NONE ;
		}

		// проверяем модификатор размера
		if( it->sizeMod == BaseType::MZ_SHORT && it->baseTypeCode != BaseType::BT_INT )
		{
			theApp.Error( tempObjectContainer->errPos,
				"'модификатор размера short' может использоваться только с типом 'int'"),
			it->sizeMod =  BaseType::MZ_NONE;
		}

		if( it->sizeMod == BaseType::MZ_LONG && 
			it->baseTypeCode !=  BaseType::BT_INT &&
			it->baseTypeCode !=  BaseType::BT_DOUBLE )
		{
			theApp.Error(tempObjectContainer->errPos, 
				"'модификатор размера long' может использоваться только "
				"с типом 'int', либо 'double'");
			it->sizeMod =  BaseType::MZ_NONE;
		}

		BaseType *tmp = (BaseType *)
			&ImplicitTypeManager(it->baseTypeCode, it->sizeMod, it->signMod).GetImplicitType();
		delete tempObjectContainer->baseType;
		tempObjectContainer->finalType = tmp;
	}

	// если базовый тип задан как typedef, значит требуется его развертка
	else if( tempObjectContainer->baseType->IsSynonym() )
	{
		const ::Object &tname = ((TempObjectContainer::SynonymType *)
			tempObjectContainer->baseType)->GetTypedefName();

		INTERNAL_IF( tname.GetStorageSpecifier() != ::Object::SS_TYPEDEF );
		
		// развертываем синоним типа и добавляем его к уже имеющемуся по след. алгоритму:
		// 1. Присоединяем список производных типов из obj, в конец результирующегося типа
		// 2. В первый производный тип obj, такой как '*', 'X::*', '()' добавляем
		// cv-квалификаторы, если они у нас есть. Например:  T* = U, тогда const U=T *const
		// Соотв. при добавлении квалификатор удаляется из декларации		
		// 3. cv-квалификаторы и базовый тип сохраняем в результ. типе
		
		// если имеются производные типы в синониме
		if( tname.GetDerivedTypeList().GetDerivedTypeCount() > 0 )
		{
			bool was_qual = false;
			was_qual = tempObjectContainer->dtl.AddDerivedTypeListCV(tname.GetDerivedTypeList(),
				tempObjectContainer->constQual, tempObjectContainer->volatileQual);
		
			if( was_qual )
				tempObjectContainer->constQual = tempObjectContainer->volatileQual = false;
		}

		// перемещаем базовый тип в контейнер, прежде удалив синоним
		delete tempObjectContainer->baseType;
		bool c = tempObjectContainer->constQual || tname.IsConst(),
			 v = tempObjectContainer->volatileQual || tname.IsVolatile();
		tempObjectContainer->constQual = c;
		tempObjectContainer->volatileQual = v;
		tempObjectContainer->finalType = (BaseType*)&tname.GetBaseType();
	}

	else if( tempObjectContainer->baseType->IsCompound() )
	{
		tempObjectContainer->finalType = (BaseType *)
			((TempObjectContainer::CompoundType *)tempObjectContainer->baseType)->GetBaseType();
		INTERNAL_IF( tempObjectContainer->finalType == NULL );
	}

	else
		INTERNAL( "неизвестная разновидность базового типа");

	// в последнюю очередь проверяем, если задан спецификатор свзяывания, но
	// extern не задан, задаем, предварительно проверив
	if( tempObjectContainer->clinkSpec )
	{
		if( tempObjectContainer->ssCode != -1 &&
			tempObjectContainer->ssCode != KWEXTERN )
			theApp.Error(tempObjectContainer->errPos,
				"использование '%s' некорректно, спецификатор хранения уже задан",
				GetKeywordName(tempObjectContainer->ssCode) );
	}
}


// проверить и создать характеристику базового класса, если
// базовый класс не найден, либо он не доступен, возвращается 0
PBaseClassCharacteristic MakerUtils::MakeBaseClass( const NodePackage *bc, bool defaultIsPrivate)
{
	// пакет должен иметь заголовок PC_BASE_CLASS и содержать от 1
	// до 3 дочерних пакетов. Первые два - спецификатор доступа и virtual.
	// Последний всегда имя класса.
	INTERNAL_IF( bc->GetPackageID() != PC_BASE_CLASS || bc->GetChildPackageCount() < 1 ||
		bc->GetChildPackageCount() > 3 );

	NodePackage *cnam = (NodePackage *)bc->GetLastChildPackage();
	INTERNAL_IF( cnam->GetPackageID() != PC_QUALIFIED_NAME );

	ClassMember::AS as = defaultIsPrivate ? ClassMember::AS_PRIVATE : ClassMember::AS_PUBLIC;
	bool vd = false;

	// если имеется спецификатор доступа или virtual
	if( bc->GetChildPackageCount() > 1 )
	{
	for( int i = 0; i < bc->GetChildPackageCount()-1; i++ )
	{
		const Lexem &lxm = ((LexemPackage *)bc->GetChildPackage(i))->GetLexem();
		if( lxm == KWVIRTUAL )
			vd = true;

		else if( lxm == KWPUBLIC )
			as = ClassMember::AS_PUBLIC;

		else if( lxm == KWPROTECTED )
			as = ClassMember::AS_PROTECTED;

		else if( lxm == KWPRIVATE )
			as = ClassMember::AS_PRIVATE;
		
		else
			INTERNAL( "'MakerUtils::MakeBaseClass' получил некорректный пакет" );
	}
	}

	QualifiedNameManager qnm(cnam);
	AmbiguityChecker achk(qnm.GetRoleList(), ParserUtils::GetPackagePosition(cnam), true);
	const ClassType *cls = NULL;
		
	cls = achk.IsClassType(true);			
	if( cls == NULL )		// если имя не является классом, возможно это имя typedef	
		if( const ::Object *id = achk.IsTypedef() )
			cls = CheckerUtils::TypedefIsClass(*id);

	if( cls != NULL )
	{
		// проверяем, можно ли класс использовать как базовый и
		// если можно - возвращаем
		Position epos = ParserUtils::GetPackagePosition(cnam);
		if( CheckerUtils::BaseClassChecker(*cls, qnm.GetQualifierList(), epos,
				ParserUtils::PrintPackageTree(cnam).c_str()) )
		{
			// проверяем на доступность и возвращаем характеристику базового класса
			CheckerUtils::CheckAccess(qnm, *cls, epos);
			return new BaseClassCharacteristic(vd, as, *cls);
		}

		// иначе класс не может быть базовым классом и мы возвращаем 0
		else
			return NULL;
	}

	// иначе класс не найден
	else
	{
		theApp.Error( ParserUtils::GetPackagePosition(cnam),
			"'%s' - базовый класс не найден", ParserUtils::PrintPackageTree(cnam).c_str());
		return NULL;
	}
}


// анализировать пакет с перегруженным оператором и поместить
//  его во временную структуру
void MakerUtils::AnalyzeOverloadOperatorPkg( const NodePackage &op, 
											TempOverloadOperatorContainer &tooc )
{
	INTERNAL_IF( op.GetPackageID() != PC_OVERLOAD_OPERATOR );

	// формат пакета: KWOPERATOR opLxm1 [opLxm2]
	INTERNAL_IF( op.GetChildPackageCount() < 2 );
	INTERNAL_IF( op.GetChildPackage(0)->GetPackageID() != KWOPERATOR );
	INTERNAL_IF( !op.GetChildPackage(1)->IsLexemPackage() );

	// если оператор состоит из одной лексемы
	if( op.GetChildPackageCount() == 2 )
	{			
		// записываем результат в пакет		
		Lexem lxm = static_cast<const LexemPackage *>(op.GetChildPackage(1))->GetLexem();
		tooc.opCode = lxm.GetCode();
		tooc.opString = lxm.GetBuf();
		tooc.opFullName = (string("operator ") + lxm.GetBuf().c_str()).c_str();
	}

	// иначе если оператор состоит из двух лексем (только '[]' и '()')
	else if( op.GetChildPackageCount() == 3 )
	{		
		Lexem lxm = static_cast<const LexemPackage *>(op.GetChildPackage(1))->GetLexem();
		INTERNAL_IF( lxm != '(' && lxm != '[' );

		tooc.opCode = lxm == '(' ? OC_FUNCTION : OC_ARRAY;
		tooc.opString = lxm == '(' ? "()" : "[]";
		tooc.opFullName = (string("operator ") + tooc.opString.c_str()).c_str();
	}

	// иначе если оператор из 3 лексем (только 'new[]' и 'delete[]')
	else if( op.GetChildPackageCount() == 4 )
	{
		Lexem kwlxm = static_cast<const LexemPackage *>(op.GetChildPackage(1))->GetLexem();
		

		INTERNAL_IF( kwlxm  != KWNEW && kwlxm != KWDELETE );
		INTERNAL_IF( op.GetChildPackage(2)->GetPackageID() != '[' ||
					 op.GetChildPackage(3)->GetPackageID() != ']' );

		tooc.opCode = kwlxm == KWNEW ? OC_NEW_ARRAY : OC_DELETE_ARRAY;
		tooc.opString = kwlxm == KWNEW ? "new[]" : "delete[]";
		tooc.opFullName = (string("operator ") + tooc.opString.c_str()).c_str();

	}	

	// иначе внутренняя ошибка
	else
		INTERNAL( "'MakerUtils::AnalyzeOverloadOperatorPkg' принимает некорректный пакет");
}


// анализировать пакет с оператором приведения и поместить
// его во временную структуру
void MakerUtils::AnalyzeCastOperatorPkg( const NodePackage &op, 
							TempCastOperatorContainer &tcoc  )
{
	INTERNAL_IF( op.GetPackageID() != PC_CAST_OPERATOR );

	// формат пакета: KWOPERATOR PC_CAST_TYPE
	INTERNAL_IF( op.GetChildPackageCount() != 2 );
	INTERNAL_IF( op.GetChildPackage(0)->GetPackageID() != KWOPERATOR || 
				 op.GetChildPackage(1)->GetPackageID() != PC_CAST_TYPE );

	const NodePackage &ct = *static_cast<const NodePackage *>(op.GetChildPackage(1));
	INTERNAL_IF( ct.IsNoChildPackages() );

	// создаем временную структуру для типа приведения
	TempObjectContainer toc( ParserUtils::GetPackagePosition(&ct), 
		CharString("<оператор приведения>") );

	// начинаем анализ спецификаторов типа
	AnalyzeTypeSpecifierPkg( ((NodePackage *)ct.GetChildPackage(0)), &toc );

	// далее, анализируем декларатор
	AnalyzeDeclaratorPkg( ((NodePackage *)ct.GetChildPackage(1)), &toc );
	
	// уточняем базовый тип
	SpecifyBaseType( &toc );

	// проверяем, чтобы не использовались не нужные спецификаторы
	if( toc.ssCode != -1 || toc.fnSpecCode != -1 || toc.friendSpec )
		theApp.Error(toc.errPos, "'%s' некорректен в контексте объявления оператора приведения",
			toc.friendSpec ? "friend" :	
				GetKeywordName(toc.ssCode < 0 ? toc.fnSpecCode : toc.ssCode));

	// после всех операций, тип готов. Заполняем временную структуру
	tcoc.opCode = OC_CAST;
	tcoc.castType = new TypyziedEntity(toc.finalType, toc.constQual, toc.volatileQual, toc.dtl); 
	tcoc.opString = tcoc.castType->GetTypyziedEntityName();
	tcoc.opFullName = (string("operator ") + tcoc.opString.c_str()).c_str();
}


// создает новую область видимости, если она не создана, вставляет
// ее в стек областей видимости, и делаем необходимые проверки.
// В параметре передается пакет с именем, который может быть NULL,
// если создается безимянная ОВ
bool MakerUtils::MakeNamepsaceDeclRegion( const NodePackage *nn )
{	
	// если нет имени, работаем с безымянной областью видимости
	if( nn == NULL )
	{			
		CharString tun = theApp.GetTranslationUnit().GetShortFileName();
		if( unsigned p = tun.find(".") )
			tun.erase(p, tun.size());
		CharString tmpName = ( ("__" + CharString(tun) + "_namespace").c_str() );		

		// сначала пытаемся найти эту область видимости, возможно она уже
		// существует
		NameManager nm(tmpName, &GetCurrentSymbolTable(), false);
		INTERNAL_IF( nm.GetRoleCount() > 1 );

		// область видимости найдена
		if( nm.GetRoleCount() == 1 )
		{
			INTERNAL_IF( nm.GetRoleList().front().second != R_NAMESPACE );
				// делаем ее текущей
			GetScopeSystem().MakeNewSymbolTable((NameSpace*)nm.GetRoleList().front().first);
			return true;
		}

		// далее создаем
		NameSpace *ns = new NameSpace(tmpName, &GetCurrentSymbolTable(), true);

		// вставляем
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(ns) );

		// делаем дружеской по отношению к текущей, чтобы она имела 
		// доступ к именам из безимянной
		GeneralSymbolTable *gst = dynamic_cast<GeneralSymbolTable *>(&GetCurrentSymbolTable());
		INTERNAL_IF( gst == NULL );
		gst->AddUsingNamespace(ns);

		// сохраняем в стеке, делая ее текущей
		GetScopeSystem().MakeNewSymbolTable(ns);
		return true;		// выходим
	}

	INTERNAL_IF( nn->GetPackageID() != PC_QUALIFIED_NAME );

	// в противном случае, создаем именованную область видимости
	Position ep = ParserUtils::GetPackagePosition(nn);
	CharString nam = ParserUtils::PrintPackageTree(nn);
	QualifiedNameManager qnm(nn, &GetCurrentSymbolTable());

	// если имеется несолько имен
	if( qnm.GetRoleCount() > 1 )
	{
		theApp.Error(ep, "'%s' - имя области видимости должно быть уникальным",
			nam.c_str());
		return false;
	}

	// если имеется одна роль, она должна быть обязательно именем области видимости
	else if( qnm.GetRoleCount() == 1 )
	{
		RolePair r = qnm.GetRoleList().front();
		if( r.second != R_NAMESPACE )
		{
			theApp.Error(ep, "'%s' - идентификатор не является именованной областью видимости",
				nam.c_str());
			return false;
		}

		// иначе, имя является областью видимости и нам следует сохранить его в стеке
		else
		{
			NameSpace *ns = dynamic_cast<NameSpace *>(r.first);
			GetScopeSystem().MakeNewSymbolTable(ns);
			return true;
		}	
	}

	// в противном случае, у имени нет ролей. Нам следует проверить, 
	// если имя является составным, то это ошибка, иначе создаем область видимости
	else
	{
		if( nn->GetChildPackageCount() > 1 )
		{
			theApp.Error(ep, "'%s' - идентификатор не является именованной областью видимости",
				nam.c_str());
			return false;
		}

		// в противном случае, создаем область видимости
		INTERNAL_IF( nn->GetChildPackage(0)->GetPackageID() != NAME );
		NameSpace *ns = new NameSpace(nam, &GetCurrentSymbolTable(), false);

		// вставляем
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(ns) );

		// сохраняем в стеке, делая ее текущей
		GetScopeSystem().MakeNewSymbolTable(ns);
		return true;
	}
}


// проверить, создать и вставить в таблицу синоном области видимости
// в параметрах принимаются имя синонима и имя области видимости для
// которов синоним создается
void MakerUtils::MakeNamespaceAlias( const NodePackage *al, const NodePackage *ns )
{
	// 1. al - должно быть неквалифицированным именем
	// 2. ns и al должны иметь код PC_QUALIFIED_NAME
	// 3. ns - должно быть именованной областью видимости
	// 4. al - должно быть уникальным именем в своей области видимости.
	//	  если оно уже создано, проверить чтобы оно было областью видимости 
	//	  и указатели равнялись области видимости ns
	INTERNAL_IF( al == NULL || ns == NULL || al->GetPackageID() != PC_QUALIFIED_NAME ||
		ns->GetPackageID() != PC_QUALIFIED_NAME );

	Position alPos = ParserUtils::GetPackagePosition(al), 
			 nsPos = ParserUtils::GetPackagePosition(ns);

	INTERNAL_IF( al->GetChildPackageCount() != 1 || 
				 al->GetChildPackage(0)->GetPackageID() != NAME );
	
	QualifiedNameManager qnm(ns);
	AmbiguityChecker achk(qnm.GetRoleList(), alPos, true);

	// если это именованая область видимости, то продолжаем проверку
	if( const NameSpace *tns = achk.IsNameSpace() )
	{
		CharString name = ParserUtils::PrintPackageTree(al);
		NameManager nm(name, &GetCurrentSymbolTable(), false); 
		if( nm.GetRoleCount() > 1 || 
			(nm.GetRoleCount() == 1 && nm.GetRoleList().front().second != R_NAMESPACE) )
		{
			theApp.Error(alPos, 
				"'%s' - идентификатор не является именованной областью видимости",
				name.c_str());
			return;
		}

		// иначе если имя еще не создано, создаем и вставляем в таблицу
		if( nm.GetRoleCount() == 0 )
		{
			NameSpaceAlias *nsa = new NameSpaceAlias(name, &GetCurrentSymbolTable(), *tns);
			INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(nsa) );
		}

		// иначе имя уже является областью видимости, проверим, чтобы оно
		// соотв. предыдущ. объявлению
		else
		{
			if( static_cast<NameSpace *>(nm.GetRoleList().front().first) != tns )
				theApp.Error(alPos, 
				"'%s' - декларация синонима области видимости не совпадает с предыдущей", 
					name.c_str());
		}
	}

	// иначе выводим ошибку и выходим
	else
	{
		theApp.Error(alPos, "'%s' - область видимости не найдена", 
			ParserUtils::PrintPackageTree(ns).c_str());
		return;
	}
}


// создать и проверить декларацию дружеской области видимости
void MakerUtils::MakeUsingNamespace( const NodePackage *ns )
{
	INTERNAL_IF( ns == NULL || ns->GetPackageID() != PC_QUALIFIED_NAME ||
		ns->IsNoChildPackages() );

	INTERNAL_IF( GetCurrentSymbolTable().IsClassSymbolTable() );

	QualifiedNameManager qnm(ns); 
	CharString name = ParserUtils::PrintPackageTree(ns);
	Position pos = ParserUtils::GetPackagePosition(ns);

	if( qnm.GetRoleCount() > 1 ||
		(qnm.GetRoleCount() == 1 && qnm.GetRoleList().front().second != R_NAMESPACE) )
		{
			theApp.Error(pos, 
				"'%s' - идентификатор не является именованной областью видимости",
				name.c_str());
			return;
		}

	// иначе если имени вообще нет
	if( qnm.GetRoleCount() == 0 )
	{
		theApp.Error(pos, 
				"'%s' - область видимости не найдена",
				name.c_str());
		return;		
	}

	//в этом месте имеем единственную область видимости
	NameSpace *tns = dynamic_cast< NameSpace *>(qnm.GetRoleList().front().first);
	INTERNAL_IF( tns == NULL );

	// если текущая область видимости функциональная, добавляем в нее,	
	if( GetCurrentSymbolTable().IsFunctionSymbolTable() )
	{
		FunctionSymbolTable &fst = static_cast<FunctionSymbolTable &>(GetCurrentSymbolTable());
		fst.AddUsingNamespace(tns);
	}

	// иначе в генеральную
	else
	{
		GeneralSymbolTable *gst = dynamic_cast<GeneralSymbolTable *>(&GetCurrentSymbolTable());
		INTERNAL_IF( gst == NULL );
		gst->AddUsingNamespace(tns);
	}
}


// создать дружественный класс
void MakerUtils::MakeFriendClass( const NodePackage *tsl )
{
	INTERNAL_IF( tsl == NULL || tsl->GetChildPackageCount() != 3 );
	INTERNAL_IF( !GetCurrentSymbolTable().IsClassSymbolTable() );

	ClassTypeMaker ctm((NodePackage *)tsl, ClassMember::NOT_CLASS_MEMBER, 
		(SymbolTable&)GetScopeSystem().GetGlobalSymbolTable());
	
	ClassType *frnd = ctm.Make(),
			  &curCls = static_cast<ClassType &>(GetCurrentSymbolTable());
	if( !frnd )
		return;

	// если такого друга еще нет, вставляем его в список друзей текущего класса
	if( curCls.GetFriendList().FindClassFriend(frnd) < 0 )
		const_cast<ClassFriendList &>(
			curCls.GetFriendList()).AddClassFriend(ClassFriend(frnd));
}


// создать using-декларацию член
void MakerUtils::MakeUsingMember( const NodePackage *npkg, ClassMember::AS as )
{
	INTERNAL_IF( npkg == NULL || npkg->GetPackageID() != PC_QUALIFIED_NAME ||
		npkg->IsNoChildPackages() );
	INTERNAL_IF( !GetCurrentSymbolTable().IsClassSymbolTable() ||
		as == ClassMember::NOT_CLASS_MEMBER );

	Position ep = ParserUtils::GetPackagePosition(npkg);
	CharString name = ParserUtils::PrintPackageTree(npkg);
	ClassType &cls = static_cast<ClassType &>(GetCurrentSymbolTable());

	// не квалифицированное имя
	if( npkg->GetChildPackageCount() == 1 )
	{
		theApp.Error(ep, 
			"в using-декларации должны использоваться только квалифицированные имена");
		return;
	}

	// если у класса нет базовых классов, то проверять ничего не нужно
	if( !cls.IsDerived() )
	{
		theApp.Error(ep, 
			"using-декларация может быть только в производном классе");
		return;
	}

	// ищем имя заданное квалифицированным именем 
	QualifiedNameManager qnm(npkg);
	
	// если имен нет, выйти
	if( qnm.GetRoleCount() == 0 )
		return;


	// получим список ролей данного имени в текущей области видимости,
	// для проверки переопределения
	NameManager cnm(qnm.GetRoleList().front().first->GetName(), &GetCurrentSymbolTable(), false);

	// далее следует проверить, чтобы каждое имя было членом базового
	// класса данного класса и было членом класса вообще	
	for( RoleList::const_iterator p = qnm.GetRoleList().begin(); 
		p != qnm.GetRoleList().end(); p++)
	{
		Identifier *id = (*p).first;
		ClassMember *cm = dynamic_cast<ClassMember *>(id);
		if( !cm || !id->GetSymbolTableEntry().IsClassSymbolTable() ||
			cls.GetBaseClassList().HasBaseClass(
				static_cast<const ClassType *>(&id->GetSymbolTableEntry())) < 0 )
		{
			theApp.Error(ep, "'%s' - не является членом базового класса", name.c_str());
			continue;
		}

		// проверяем член на доступность
		CheckerUtils::CheckAccess(qnm, *id, ep);

		// если идентификатор сам является using-идентификатором, преобразуем
		// его в обычный вид
		if( UsingIdentifier *ui = dynamic_cast<UsingIdentifier *>(id) )
			id = const_cast<Identifier *>(&ui->GetUsingIdentifier());

		Role idr = NameManager::GetIdentifierRole(id);
		if( idr == R_CONSTRUCTOR )
		{
			theApp.Error(ep, 
				"'%s' - конструктор не может объявляться в using-декларации", name.c_str());
			break;
		}
	
		// проверяем, переопределяет ли идентификатор какой-либо член
		// уже объявленный в классе
		if( TypyziedEntity *te = dynamic_cast<TypyziedEntity *>(id) )
		{
			RedeclaredChecker rchk(*te, cnm.GetRoleList(), ep, idr);				

			if( !rchk.IsRedeclared() )			
				GetCurrentSymbolTable().InsertSymbol( new UsingIdentifier(id->GetName(),
					&GetCurrentSymbolTable(), id, as) );
		}

		// иначе это может быть класс или перечисление 
		else
		{
			if( cnm.GetRoleCount() != 0 )
				theApp.Error(ep, "'%s' - переопределен", id->GetName().c_str());
		}
	}
}


// создать using-декларацию не член
void MakerUtils::MakeUsingNotMember( const NodePackage *npkg )
{
	INTERNAL_IF( npkg == NULL || npkg->GetPackageID() != PC_QUALIFIED_NAME ||
		npkg->IsNoChildPackages() );
	INTERNAL_IF( GetCurrentSymbolTable().IsClassSymbolTable() );

	Position ep = ParserUtils::GetPackagePosition(npkg);
	CharString name = ParserUtils::PrintPackageTree(npkg);

	// не квалифицированное имя
	if( npkg->GetChildPackageCount() == 1 )
	{
		theApp.Error(ep, 
			"в using-декларации должны использоваться только квалифицированные имена");
		return;
	}

		// ищем имя заданное квалифицированным именем 
	QualifiedNameManager qnm(npkg);
	
	// если имен нет, выйти
	if( qnm.GetRoleCount() == 0 )
		return;

	// далее следует проверить, чтобы каждое имя было членом базового
	// класса данного класса и было членом класса вообще	
	NameManager cnm( qnm.GetRoleList().front().first->GetName(), &GetCurrentSymbolTable(),false);
	for( RoleList::const_iterator p = qnm.GetRoleList().begin(); 
		p != qnm.GetRoleList().end(); p++)
	{
		Identifier *id = (*p).first;
		ClassMember *cm = dynamic_cast<ClassMember *>(id);
		if( !cm || cm->GetAccessSpecifier() != ClassMember::NOT_CLASS_MEMBER )
		{
			theApp.Error(ep, "'%s' - не является членом глобальной области видимости", 
				name.c_str());
			continue;
		}

		// если идентификатор сам является using-идентификатором, преобразуем
		// его в обычный вид
		if( UsingIdentifier *ui = dynamic_cast<UsingIdentifier *>(id) )
			id = const_cast<Identifier *>(&ui->GetUsingIdentifier());

		Role idr = NameManager::GetIdentifierRole(id);
		if( idr == R_CONSTRUCTOR )
		{
			theApp.Error(ep, 
				"'%s' - конструктор не может объявляться в using-декларации", name.c_str());
			break;
		}
	
		// проверяем, переопределяет ли идентификатор какой-либо член
		// уже объявленный в классе
		if( TypyziedEntity *te = dynamic_cast<TypyziedEntity *>(id) )
		{
			RedeclaredChecker rchk(*te, cnm.GetRoleList(), ep, idr);				

			if( !rchk.IsRedeclared() )
				GetCurrentSymbolTable().InsertSymbol( new UsingIdentifier(id->GetName(),
					&GetCurrentSymbolTable(), id, ClassMember::NOT_CLASS_MEMBER) );
		}

		// иначе это может быть класс или перечисление 
		else
		{
			if( cnm.GetRoleCount() != 0 )
				theApp.Error(ep, "'%s' - переопределен", id->GetName().c_str());
		}
	}	
}

// создать константу перечисления и вставить ее в текущую область видимости
EnumConstant *MakerUtils::MakeEnumConstant(
		const CharString &name, ClassMember::AS curAccessSpec,
		int lastVal, const Position &errPos, EnumType *enumType )
{

	// проверяем переопределение
	NameManager nm(name, &GetCurrentSymbolTable(), false);
	if( nm.GetRoleCount() > 0 )
	{
		bool type = false;
		if( nm.GetRoleCount() == 1 )
		{
			Role r = nm.GetRoleList().front().second;
			type = r == R_CLASS_TYPE || r == R_ENUM_TYPE			||
				   r == R_UNION_CLASS_TYPE || r == R_TEMPLATE_CLASS	||
				   r == R_TEMPLATE_CLASS_SPECIALIZATION;
		}

		if( !type )
		{
			theApp.Error(errPos, "'%s' - переопределен", name.c_str());
			return NULL;
		}
	}

	// проверяем, также чтобы не имел имя класса
	if( GetCurrentSymbolTable().IsClassSymbolTable() &&
		static_cast<const ClassType &>(GetCurrentSymbolTable()).GetName() == name )
	{
		theApp.Error(errPos,
			"'%s' - не может иметь имя класса в котором объявляется", name.c_str());
		return NULL;
	}

	// теперь создаем. Если спецификатор доступа задан, создаем классовую
	// константу, иначе обычную
	EnumConstant *ec = curAccessSpec == ClassMember::NOT_CLASS_MEMBER ?
		new EnumConstant( name, &GetCurrentSymbolTable(), lastVal, enumType) :
	    new ClassEnumConstant( name, &GetCurrentSymbolTable(), lastVal, enumType, curAccessSpec);

	// вставляем константу в таблицу
	GetCurrentSymbolTable().InsertSymbol( ec );
	return ec;
}


// конструктор принимает пакет, для нахождения в нем имени и 
// инициализации nameManager
TempObjectContainer::TempObjectContainer( const NodePackage *qualName, ClassMember::AS cas ) 
{
	INTERNAL_IF( qualName == NULL || qualName->GetPackageID() != PC_QUALIFIED_NAME );
	nameManager = new QualifiedNameManager(qualName);
	const Lexem &namLxm = 
		((LexemPackage *)qualName->GetChildPackage( qualName->GetChildPackageCount()-1 ))
			->GetLexem();

	name = namLxm.GetBuf();
	errPos = ((LexemPackage *)qualName->GetChildPackage(0))->GetLexem().GetPos();
	baseType = NULL;
	constQual = volatileQual = false;
	ssCode = -1;
	fnSpecCode = -1;
	friendSpec = false;
	curAccessSpec = cas;
	clinkSpec = theApp.GetTranslationUnit().GetParser().GetLinkSpecification() == Parser::LS_C;		
}


// конструктор для объектов, которые не могут содержать составные
// имена, либо могут не иметь имени вовсе
TempObjectContainer::TempObjectContainer( const Position &ep, 
			const CharString &n, ClassMember::AS cas )
{
	nameManager = NULL;
	name = n;
	errPos = ep;
	baseType = NULL;
	constQual = volatileQual = false;
	ssCode = -1;
	fnSpecCode = -1;
	friendSpec = false;
	curAccessSpec = cas;
	clinkSpec = theApp.GetTranslationUnit().GetParser().GetLinkSpecification() == Parser::LS_C;		
}


// очищает список производных типов, удаляем занятую память
TempObjectContainer::~TempObjectContainer() 
{
	dtl.ClearDerivedTypeList();
	delete nameManager;
}


// в конструкторе задаем временную структуру,которая будет
// использоваться при построении объекта
GlobalObjectMaker::GlobalObjectMaker( const PTempObjectContainer &toc, bool ld ) 
	: tempObjectContainer(toc), localDeclaration(ld), ictor(NULL)
{	
	targetObject = NULL;
	redeclared = false;
}
					

// проверить корректность входного пакета, собрать информацию во временную структуру,
// создать объект-контейнер, произвести строгую проверку объекта-контейнра,
// вставить контейнер в таблицу и возвратить его как результат работы
bool GlobalObjectMaker::Make( )
{
	// проверяем корректность сформированного объекта
	GlobalDeclarationChecker goc(*tempObjectContainer, localDeclaration);

	
	// проверка является ли имя уникальным в своей области видимости
	NameManager nm(tempObjectContainer->name, &GetCurrentSymbolTable(), false);

	// создаем объект для проверки
	register TempObjectContainer *toc = &*tempObjectContainer;
	targetObject = new ::Object(toc->name, &GetCurrentSymbolTable(), toc->finalType,
		toc->constQual, toc->volatileQual, toc->dtl, toc->ssCode < 0 ? ::Object::SS_NONE : 
			TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierObj(), toc->clinkSpec );	

	// проверяем на переопределение
	RedeclaredChecker rchk(*targetObject, nm.GetRoleList(), toc->errPos, R_OBJECT);

	// объект переопределен
	if( rchk.IsRedeclared() )
	{		
		delete targetObject;
		redeclared = true;
		targetObject = const_cast<::Object *>(
			dynamic_cast<const ::Object *>(rchk.GetPrevDeclaration()) );
	}

	// если объект уникален в своей области видимости, создаем и
	// вставляем объект в таблицу
	else
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(targetObject) );
	return redeclared;
}


// в конструкторе задаем временную структуру,которая будет
// использоваться при построении функции
GlobalFunctionMaker::GlobalFunctionMaker( const PTempObjectContainer &toc )
	: tempObjectContainer(toc)
{	
	targetFn = NULL;
	errorFlag = false;
}


// создает функцию из временной структуры
bool GlobalFunctionMaker::Make()
{
	// проверяем корректность сформированной функции
	GlobalDeclarationChecker goc(*tempObjectContainer);

		
	// проверка является ли имя уникальным в своей области видимости
	NameManager nm(tempObjectContainer->name, &GetCurrentSymbolTable(), false);

	// создаем объект для проверки
	register TempObjectContainer *toc = &*tempObjectContainer;
	targetFn = new Function(toc->name, &GetCurrentSymbolTable(), toc->finalType,
			toc->constQual, toc->volatileQual, toc->dtl, toc->fnSpecCode == KWINLINE,
			toc->ssCode < 0 ? Function::SS_NONE : 
			TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(), 
			toc->clinkSpec ? Function::CC_CDECL : Function::CC_NON);

		// проверяем на переопределение
	RedeclaredChecker rchk(*targetFn, nm.GetRoleList(), toc->errPos, R_FUNCTION);

	// объект переопределен
	if( rchk.IsRedeclared() )
	{		
		Function *prevFn = const_cast<Function *>(
			dynamic_cast<const Function *>(rchk.GetPrevDeclaration()) );
		if( prevFn )
			CheckerUtils::DefaultArgumentCheck( targetFn->GetFunctionPrototype(), 
				&prevFn->GetFunctionPrototype(), toc->errPos);

		delete targetFn;
		targetFn = prevFn;
	}

	// если объект уникален в своей области видимости, создаем и
	// вставляем объект в таблицу
	else
	{
		CheckerUtils::DefaultArgumentCheck( targetFn->GetFunctionPrototype(), NULL, toc->errPos);
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(targetFn) );
	}

	return errorFlag;
}


// в конструкторе задаем временную структуру,которая будет
// использоваться при построении оператора
GlobalOperatorMaker::GlobalOperatorMaker( const PTempObjectContainer &toc,
										 const TempOverloadOperatorContainer &tc )
	: tempObjectContainer(toc), tooc(tc), targetOP(NULL)
{

}


// создать перегруженный оператор
bool GlobalOperatorMaker::Make()
{
	GlobalOperatorChecker goc(*tempObjectContainer, tooc);

	// проверяем переопределение
	register TempObjectContainer *toc = &*tempObjectContainer;
	NameManager nm(toc->name, &GetCurrentSymbolTable(), true);

	// создаем объекь для проверки на переопределяемость
	targetOP = new OverloadOperator(toc->name, &GetCurrentSymbolTable(),
		toc->finalType,	toc->constQual, toc->volatileQual, 
		toc->dtl, toc->fnSpecCode == KWINLINE,
		toc->ssCode < 0 ? Function::SS_NONE : 
		TypeSpecifierManager(toc->ssCode).CodeToStorageSpecifierFn(),
			Function::CC_NON, tooc.opCode, tooc.opString );
						
	// проверяем на переопределение
	RedeclaredChecker rchk(*targetOP, nm.GetRoleList(), toc->errPos, R_OVERLOAD_OPERATOR );

	// если переопределен
	if( rchk.IsRedeclared() )	
		delete targetOP, targetOP = 0 ;

	// иначе вставляем его в таблицу
	else
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(targetOP) );

	return true;
}


// конструктор принимает пакет и флаг имен
FunctionPrototypeMaker::FunctionPrototypeMaker( const NodePackage &pp, bool nn ) 
	: protoPkg(pp), noNames(nn) 
{
	INTERNAL_IF( protoPkg.GetPackageID() != PC_FUNCTION_PROTOTYPE );
	constQual = volatileQual = false;
	ellipse = false;
	canThrow = true;
}


// создать параметр и добавить его в список из пакета
void FunctionPrototypeMaker::MakeParametr( const NodePackage &pp, int pnum )
{
	INTERNAL_IF( pp.GetPackageID() != PC_PARAMETR					   || 
		pp.GetChildPackageCount() < 1 || pp.GetChildPackageCount() > 3 || 
		pp.IsErrorChildPackages()	  || 
		pp.GetChildPackage(0)->GetPackageID() != PC_TYPE_SPECIFIER_LIST );

	// параметр, как и все декларации состоит из трех частей:
	// список спецификаторов типа, декларатор и значение по умолчанию,
	// причем 2 последние могут отсутствовать. Это следует учитывать 
	// при формировании параметра

	Position ep;			// позиция для вывода ошибок
	CharString pname;

	const Package *pk = ((NodePackage *)pp.GetChildPackage(0))->GetChildPackage(0);
	if( pk->IsNodePackage() )	
		pk = ((NodePackage *)pk)->GetChildPackage(0);

	INTERNAL_IF( !pk->IsLexemPackage() );			
	ep = ((LexemPackage *)pk)->GetLexem().GetPos();

	if( pp.GetChildPackageCount() == 1 ||
		pp.GetChildPackage(1)->GetPackageID() != PC_DECLARATOR )
		pname = ("<параметр " + CharString(pnum) + ">").c_str();
	else
	{
		int ix = ((NodePackage *)pp.GetChildPackage(1))->FindPackage(PC_QUALIFIED_NAME);
		if( ix < 0 )
			pname = ("<параметр " + CharString(pnum) + ">").c_str();
		else
		{
			NodePackage *pn = (NodePackage *)
				((NodePackage *)pp.GetChildPackage(1))->GetChildPackage(ix);
			INTERNAL_IF( pn->GetChildPackageCount() != 1 || 
				pn->GetChildPackage(0)->GetPackageID() != NAME );

			const Lexem &lx = ((LexemPackage *)pn->GetChildPackage(0))->GetLexem();
			pname = lx.GetBuf();
			ep = lx.GetPos();
		}
	}

	// наконец создаем временную структуру
	TempObjectContainer toc( ep, pname );

	// начинаем анализ спецификаторов типа
	AnalyzeTypeSpecifierPkg( ((NodePackage *)pp.GetChildPackage(0)), &toc );

	// далее, если есть декларатор, анализируем и его
	if( pp.GetChildPackageCount() > 1 && 
		pp.GetChildPackage(1)->GetPackageID() == PC_DECLARATOR )
		AnalyzeDeclaratorPkg( ((NodePackage *)pp.GetChildPackage(1)), &toc );
	
	// уточняем базовый тип
	SpecifyBaseType( &toc );

	// проверяем, если пакета 3, то третий должен быть выражением 
	// (значение по умолчанию)
	POperand defaultArg = ( pp.GetChildPackageCount() == 3 && 
		pp.GetChildPackage(2)->IsExpressionPackage()) ? const_cast<POperand&>(
		static_cast<const ExpressionPackage*>(pp.GetChildPackage(2))->GetExpression()) : NULL;

	// проверяем параметр на корректность
	ParametrChecker pchk( toc, parametrList );

	// создадим для параметра уникальную область видимости
	static LocalSymbolTable *uniqueSt = new LocalSymbolTable(GetCurrentSymbolTable());
	
	// создаем параметр и добавляем его в список
	Parametr *param = new Parametr(
		toc.finalType, toc.constQual, toc.volatileQual, toc.dtl, toc.name,
		uniqueSt, NULL, toc.ssCode == KWREGISTER );

	// задаем значение по умолчанию, только после проверки этого значения
	if( !defaultArg.IsNull() )
	{
		const POperand &rda = DefaultArgumentChecker(*param, defaultArg, ep).Check();

		// задаем значение по умолчанию, в случае если имеем error operand,
		// следует быть очень осторожным, чтобы не удалить его
		param->SetDefaultValue( rda->IsErrorOperand() ? &const_cast<Operand&>(*rda) : 
			const_cast<POperand&>(rda).Release() );
	}

	parametrList.AddFunctionParametr( param );
}


// создать тип throw-спецификации
void FunctionPrototypeMaker::MakeThrowType( const NodePackage &tt )
{
	INTERNAL_IF( tt.GetPackageID() != PC_THROW_TYPE || 
		tt.GetChildPackageCount() != 2				||
		tt.GetChildPackage(0)->GetPackageID() != PC_TYPE_SPECIFIER_LIST ||
		tt.GetChildPackage(1)->GetPackageID() != PC_DECLARATOR );

	Position ep = ParserUtils::GetPackagePosition(&tt);
	CharString tname = "<тип throw-спецификации>";


	// наконец создаем временную структуру
	TempObjectContainer toc( ep, tname );

	// начинаем анализ спецификаторов типа
	AnalyzeTypeSpecifierPkg( ((NodePackage *)tt.GetChildPackage(0)), &toc );

	// далее, анализируем декларатор
	AnalyzeDeclaratorPkg( ((NodePackage *)tt.GetChildPackage(1)), &toc );
	
	// уточняем базовый тип
	SpecifyBaseType( &toc );


	// теперь проверяем тип
	ThrowTypeChecker ttc(toc);

	// добавляем тип в список
	throwTypeList.AddThrowType( PTypyziedEntity(
		new TypyziedEntity(toc.finalType, toc.constQual,toc.volatileQual, toc.dtl)) );
}


// создать полную throw-спецификацию
void FunctionPrototypeMaker::MakeThrowSpecification( const NodePackage &ts )
{
	// может быть пустой список. В любом случае список должен содержать 
	// ( )
	INTERNAL_IF( ts.GetPackageID() != PC_THROW_TYPE_LIST ||
		ts.GetChildPackageCount() < 2 || ts.GetChildPackage(0)->GetPackageID() != '(' );

	// проверяем, если второй пакет - это ')', значит у функции пустой список
	// спецификаций
	if( ts.GetChildPackageCount() == 2 && ts.GetChildPackage(1)->GetPackageID() == ')' )
	{
		canThrow = false;
		return;
	}

	// иначе обрабатываем список throw-типов
	for( int i = 1; ; i++ )
	{
		// если имеем последний пакет
		if( i+1 == ts.GetChildPackageCount() )
		{
			INTERNAL_IF( ts.GetChildPackage(i)->GetPackageID() != ')' );
			break;
		}

		// иначе должен быть создан throw-тип
		INTERNAL_IF( ts.GetChildPackage(i)->GetPackageID() != PC_THROW_TYPE );
		MakeThrowType( *static_cast<const NodePackage *>(ts.GetChildPackage(i)) );
	}
}


// метод создающий прототип функции из пакета и возвращающий его
// в качестве результата работы объекта
FunctionPrototype *FunctionPrototypeMaker::Make()
{
	// дочерних пакета должно быть как минимум два - '(' ')'
	INTERNAL_IF( protoPkg.GetChildPackageCount() < 2 || 
		protoPkg.GetChildPackage(0)->GetPackageID() != '(' ); 

	// далее проверяем, необходимо ли нам заполнять список параметров,
	// заполнять не нужно если: нет параметров между лексемами '(' и ')',
	// между '(' и ')', только лексема ELLIPSES, между '(' и ')' один
	// параметр, который содержит только список спецификаторов типа,
	// который содержит только void 

	int i = 1;
	if( protoPkg.GetChildPackage(1)->GetPackageID() == ')'	)
		goto skip_make_parametrs;

	else if( protoPkg.GetChildPackage(1)->GetPackageID() == ELLIPSES )
	{
		ellipse = true;
		i = 2;
		goto skip_make_parametrs;
	}

	// имеем 1 параметр, если это просто void, значит это эквиваленто
	// списку пустых параметров
	else if( protoPkg.GetChildPackage(1)->GetPackageID() == PC_PARAMETR &&
		 	 protoPkg.GetChildPackage(2)->GetPackageID() == ')' )		
	{
		NodePackage *np = ((NodePackage *)protoPkg.GetChildPackage(1));	// получить параметр
	
		INTERNAL_IF( np->GetChildPackage(0)->GetPackageID() != PC_TYPE_SPECIFIER_LIST );
		NodePackage *tsl = (NodePackage *)np->GetChildPackage(0),
				*decl = np->IsNoChildPackages() ? 0 : (NodePackage *)np->GetChildPackage(1);			

		// в списке спецификаторов типа должен быть только void,
		// а декларатор должен быть пустой
		if( tsl->GetChildPackageCount() == 1 && 
			tsl->GetChildPackage(0)->GetPackageID() == KWVOID &&
			(decl == NULL || decl->IsNoChildPackages()) )
			{
				i = 2;
				goto skip_make_parametrs;
			}
	}

	// в противном случае обрабатываем все параметры и размещаем их
	// в списке	
	for( i = 1; ; i++ )
	{
		const Package *p = protoPkg.GetChildPackage(i);
		if( p->GetPackageID() == ELLIPSES )
		{
			INTERNAL_IF( protoPkg.GetChildPackage(++i)->GetPackageID() != ')' );
			ellipse = true;
			break;
		}

		else if( p->GetPackageID() == ')' )
			break;

		else
		{
			INTERNAL_IF( p->GetPackageID() != PC_PARAMETR );
			MakeParametr( *(NodePackage *)p , i);
		}
	}		
	
skip_make_parametrs:

	// i, должен равнятся пакету с ')'
	INTERNAL_IF( protoPkg.GetChildPackage(i)->GetPackageID() != ')' );
		
	// осталось обработать cv-квалификаторы и throw-спецификацию
	i++;
	for( ; i<protoPkg.GetChildPackageCount(); i++ )
	{
		if( protoPkg.GetChildPackage(i)->GetPackageID() == KWCONST )
			constQual = true;

		else if( protoPkg.GetChildPackage(i)->GetPackageID() == KWVOLATILE )
			volatileQual = true;

		// обрабатываем throw-спецификацию, она должна быть последней в пакете
		else if( protoPkg.GetChildPackage(i)->GetPackageID() == PC_THROW_TYPE_LIST )
		{
			INTERNAL_IF( protoPkg.GetChildPackageCount() != i + 1 );
			MakeThrowSpecification(
				*static_cast<const NodePackage *>(protoPkg.GetChildPackage(i)) );
		}
	}

	return new FunctionPrototype( constQual, volatileQual, parametrList,
		throwTypeList, canThrow, ellipse);
}


// построить catch-декларацию, вернуть объект
::Object *CatchDeclarationMaker::Make()
{
	INTERNAL_IF( typeSpec.GetPackageID() != PC_TYPE_SPECIFIER_LIST ||
		declPkg.GetPackageID() != PC_DECLARATOR );

	static int noNameCnt = 1;
	int ix = declPkg.FindPackage(PC_QUALIFIED_NAME);
	CharString name;
	if( ix < 0 )	
		name = (string("<catch-декларация") + CharString(noNameCnt++).c_str() + ">").c_str();
	else
		name = ParserUtils::PrintPackageTree((NodePackage*)declPkg.GetChildPackage(ix) );
		
	TempObjectContainer toc( errPos, name );
			
	// начинаем анализ спецификаторов типа
	MakerUtils::AnalyzeTypeSpecifierPkg( &typeSpec, &toc );

	// далее анализируем декларатор
	MakerUtils::AnalyzeDeclaratorPkg( &declPkg, &toc );
	
	// уточняем базовый тип
	MakerUtils::SpecifyBaseType( &toc );

	// теперь проверяем тип
	CatchDeclarationChecker cdc(toc);

	// возвращаем объект
	::Object *targetObject = new ::Object(toc.name, &GetCurrentSymbolTable(), toc.finalType,
		toc.constQual, toc.volatileQual, toc.dtl, toc.ssCode < 0 ? ::Object::SS_NONE : 
		TypeSpecifierManager(toc.ssCode).CodeToStorageSpecifierObj(), toc.clinkSpec );

	// вставляем объект в таблицу только если у него есть имя
	if( ix >= 0 )
		GetCurrentSymbolTable().InsertSymbol(targetObject);
	return targetObject; 
}


//  к-тор принимает пакет и проверяет его правильность
ClassTypeMaker::ClassTypeMaker( 
	NodePackage *np, ClassMember::AS a, SymbolTable &d, bool def )
	: typePkg(np), as(a), resultCls(NULL), destST(d), defination(def)
{ 		
	INTERNAL_IF( np == NULL || np->GetPackageID() != PC_TYPE_SPECIFIER_LIST ||
				 np->IsNoChildPackages() );		
}


// создать класс, если он еще не создан, а также проверить 
// возможность его создания
ClassType *ClassTypeMaker::Make()
{
	const Package *pkg = typePkg->GetChildPackage(typePkg->GetChildPackageCount()-1);

	// имеем безимянный класс, создаем его, вставляем в таблицу и выходим
	if( pkg->GetPackageID() == KWCLASS || pkg->GetPackageID() == KWSTRUCT ||
		pkg->GetPackageID() == KWUNION )
	{
		MakeUnnamedClass();
		return resultCls;	
	}

	// в противном случае, класс именной и требуются проверки
	INTERNAL_IF( pkg->GetPackageID() != PC_QUALIFIED_NAME );
	NodePackage *namePkg = (NodePackage *)pkg;

	// получаем ключ
	INTERNAL_IF( typePkg->GetChildPackageCount() < 2 );
	pkg = typePkg->GetChildPackage(typePkg->GetChildPackageCount()-2);
	INTERNAL_IF( pkg->GetPackageID() != KWCLASS && pkg->GetPackageID() != KWSTRUCT &&
				 pkg->GetPackageID() != KWUNION ) ;
	const Lexem &key = ((LexemPackage *)pkg)->GetLexem();
	int code = key.GetCode();

	// если имя не является квалифицированным, значит вероятно его необходимо
	// создать
	if( namePkg->GetChildPackageCount() == 1 )
	{	
		// ищем исключительно в текущей области видимости
		const Lexem &name = ((LexemPackage *)namePkg->GetChildPackage(0))->GetLexem();

		// во первых, объявляемый класс не должен иметь имя внешнего класса,
		// если данный класс объявляется внутри другого класса
		if( GetCurrentSymbolTable().IsClassSymbolTable() &&
			dynamic_cast<Identifier &>(GetCurrentSymbolTable()).GetName() == name.GetBuf() )
		{
			theApp.Error(name.GetPos(), 
					"'%s' - класс не может иметь имя класса в котором объявляется",
					name.GetBuf().c_str() );
			return NULL;
		}

		NameManager nm( name.GetBuf(), &destST, false );
		AmbiguityChecker achk(nm.GetRoleList(), name.GetPos(), true);

		// если тип найден, ключи должны совпадать
		if( const Identifier *tnam = achk.IsTypeName(true) )
		{
			resultCls = dynamic_cast<ClassType *>(const_cast<Identifier *>(tnam));

			// если не класс
			if( !resultCls )		
				theApp.Error(name.GetPos(), "'%s' - тип уже объявлен", name.GetBuf().c_str());

			// если коды не совпадают
			else if( resultCls->GetBaseTypeCode() != 
							TypeSpecifierManager(code).CodeToClassSpec() )
			{
				theApp.Error(key.GetPos(), "'%s %s' - класс уже объявлен с другим ключом",
					GetKeywordName(code), name.GetBuf().c_str() );
				return NULL;
			}

			// если спецификаторы доступа не совпадают
			else if( GetCurrentSymbolTable().IsClassSymbolTable() &&
					 resultCls->GetAccessSpecifier() != as )
			{
				theApp.Error(key.GetPos(), 
					"'%s' - класс не может изменить спецификатор доступа",
					name.GetBuf().c_str() );			
			}

			// проверку доступа не осуществляем, т.к. это либо класс, который
			// является членом данного класса, либо класс, который объявляется

			// в противном случае класс переопределеяется и мы возвращаем уже существующий
			return resultCls;
		}

		// иначе тип не найден, но может быть ОВ или шаблон класса, имена
		// которых нельзя перекрывать
		else
		{
			if( achk.IsTemplateClass() != NULL )
			{
				theApp.Error(name.GetPos(), 
					"'%s' - объявлен как 'шаблонный класс'",
					name.GetBuf().c_str());
				return NULL;
			}

			if( achk.IsNameSpace() != NULL )
			{
				theApp.Error(name.GetPos(), 
					"'%s' - объявлен как 'именованная область видимости'",
					name.GetBuf().c_str());
				return NULL;
			}

			// иначе создаем класс 			
			MakeUncompleteClass(); 			
			return resultCls;			
		}
	}

	// иначе имя является квалифицированным и потребуется только
	// проверка существования класса с заданным кодом
	else
	{
		QualifiedNameManager qnm(namePkg);
		CharString cnam = ParserUtils::PrintPackageTree(namePkg);
		Position epos = ParserUtils::GetPackagePosition(namePkg);
		AmbiguityChecker achk(qnm.GetRoleList(), epos, true);

		if( (resultCls = const_cast<ClassType *>(achk.IsClassType(true)) ) != NULL )
		{
			// если коды не совпадают
			if( resultCls->GetBaseTypeCode() != TypeSpecifierManager(code).CodeToClassSpec() )
			{
				theApp.Error(key.GetPos(), "'%s %s' - класс уже объявлен с другим ключом",
					GetKeywordName(code), cnam.c_str() );
				return NULL;
			}

			// иначе, сохраняем список квалификаторов, проверяем и возвращаем класс
			stList = qnm.GetQualifierList();

			if( !defination )
				CheckerUtils::CheckAccess(qnm, *resultCls, epos);
			return resultCls;
		}

		// иначе класс не объявлен в области видимости
		else
			theApp.Error(key.GetPos(), "'%s %s' - класс не объявлен",
					GetKeywordName(code), cnam.c_str() );				
	}

	return NULL;	
}


// создать класс по ключу и по имени
void ClassTypeMaker::MakeUncompleteClass()
{	
	// получить лексему имени, подразумевается что это последний пакет в списке
	// с кодом PC_QUALIFIED_NAME и одним дочерним пакетом NAME. Правильность
	// этого утверждения проверяется в методе Make
	const CharString &name = ((LexemPackage *)((NodePackage *)typePkg->GetChildPackage(
		typePkg->GetChildPackageCount()-1))->GetChildPackage(0))->GetLexem().GetBuf();

	int code = ((LexemPackage *)typePkg->GetChildPackage(
		typePkg->GetChildPackageCount()-2))->GetLexem().GetCode();

	SymbolTable *cst = &destST;

	// вставляем класс
	resultCls = code == KWUNION
			? new UnionClassType(name, cst, as, false, ::Object::SS_NONE)
			: new ClassType(name, cst, 
				(code == KWCLASS ? BaseType::BT_CLASS : BaseType::BT_STRUCT), as);
	INTERNAL_IF( !cst->InsertSymbol(resultCls) );	
}


// создать безимянный класс
void ClassTypeMaker::MakeUnnamedClass()
{	
	static int clsCounter = 0;
	SymbolTable *cst = &destST;
	const Lexem &lxm = ((LexemPackage *)typePkg->GetChildPackage(
		typePkg->GetChildPackageCount()-1))->GetLexem();
	int key = lxm.GetCode();

	CharString knam = (string("<класс ") + 
		CharString(clsCounter).c_str() + ">").c_str();

	clsCounter++;
	
	resultCls = key == KWUNION
			? new UnionClassType(knam, cst, as, false, ::Object::SS_NONE)
			: new ClassType(knam, cst, 
				(key == KWCLASS ? BaseType::BT_CLASS : BaseType::BT_STRUCT), as);
	INTERNAL_IF( !cst->InsertSymbol(resultCls) );	
	

	// добавляем к списку пакетов сгенерированное имя
	NodePackage *np = new NodePackage(PC_QUALIFIED_NAME);	
	np->AddChildPackage( new LexemPackage( Lexem( knam, key, lxm.GetPos()) ) );
	typePkg->AddChildPackage(np);
}


//  к-тор принимает пакет и проверяет его правильность
EnumTypeMaker::EnumTypeMaker( NodePackage *np, ClassMember::AS a, bool def )
	: typePkg(np), as(a), resultEnum(NULL), defination(def)
{ 		
	INTERNAL_IF( np == NULL || np->GetPackageID() != PC_TYPE_SPECIFIER_LIST ||
				 np->IsNoChildPackages() );		
}


// создать класс, если он еще не создан, а также проверить 
// возможность его создания
EnumType *EnumTypeMaker::Make()
{
	const Package *pkg = typePkg->GetChildPackage(typePkg->GetChildPackageCount()-1);

	// имеем безимянное перечисление, создаем его, вставляем в таблицу и выходим
	if( pkg->GetPackageID() == KWENUM )
	{
		static int ecnt = 0;
		CharString nam = string(
				string("<перечисление ") + CharString(ecnt).c_str() + ">").c_str();

		ecnt++;

		resultEnum = new EnumType(nam, &GetCurrentSymbolTable(), as);
		INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(resultEnum) );		

		// добавляем к списку пакетов сгенерированное имя
		NodePackage *np = new NodePackage(PC_QUALIFIED_NAME);	
		const Position &pos = static_cast<const LexemPackage*>(pkg)->GetLexem().GetPos();
		np->AddChildPackage( new LexemPackage( Lexem( nam, NAME,  pos) ) );
		typePkg->AddChildPackage(np);

		return resultEnum;
	}

	// в противном случае, класс именной и требуются проверки
	INTERNAL_IF( pkg->GetPackageID() != PC_QUALIFIED_NAME );
	NodePackage *namePkg = (NodePackage *)pkg;

	// проверяем правильность ключа
	INTERNAL_IF( typePkg->GetChildPackageCount() < 2 );	
	INTERNAL_IF( typePkg->GetChildPackage(typePkg->GetChildPackageCount()-2)
						->GetPackageID() != KWENUM );

	pkg = typePkg->GetChildPackage(typePkg->GetChildPackageCount()-2);
	const Lexem &key = ((LexemPackage *)pkg)->GetLexem();	

	// если имя не является квалифицированным, значит вероятно его необходимо
	// создать
	if( namePkg->GetChildPackageCount() == 1 )
	{	
		// ищем исключительно в текущей области видимости
		const Lexem &name = ((LexemPackage *)namePkg->GetChildPackage(0))->GetLexem();

		// перечисление не может имя класса в котором объявляется		
		if( GetCurrentSymbolTable().IsClassSymbolTable() &&
			dynamic_cast<Identifier &>(GetCurrentSymbolTable()).GetName() == name.GetBuf() )
		{
			theApp.Error(name.GetPos(), 
					"'%s' - перечисление не может иметь имя класса в котором объявляется",
					name.GetBuf().c_str() );
			return NULL;
		}

		NameManager nm( name.GetBuf(), &GetCurrentSymbolTable(), false);
		AmbiguityChecker achk(nm.GetRoleList(), name.GetPos(), true);

		if( (resultEnum = (EnumType *)achk.IsEnumType(true)) != NULL )
		{
			// если спецификаторы доступа не совпадают
			if( resultEnum->GetAccessSpecifier() != as )
			{
				theApp.Error(name.GetPos(), 
					"'%s' - перечисление не может изменить спецификатор доступа",
					name.GetBuf().c_str() );
			
			}

			return resultEnum ;
		}
		
		else if( achk.IsTypeName(true) || achk.IsTemplateClass() || achk.IsNameSpace() )
		{
			theApp.Error( ParserUtils::GetPackagePosition(namePkg), 
				"'%s' - имя уже используется как тип, шаблонный класс или область видимости",
				name.GetBuf().c_str());
			return NULL;			
		}
		
		// иначе создаем перечисление и вставляем его в таблицу
		else
		{
			resultEnum = new EnumType(name.GetBuf(), &GetCurrentSymbolTable(), as);
			INTERNAL_IF( !GetCurrentSymbolTable().InsertSymbol(resultEnum) );
			return 	resultEnum ;		
		}			
	}

	// иначе имя квалифицированное и требуется только проверка его существования
	else
	{
		QualifiedNameManager qnm(namePkg);
		CharString cnam = ParserUtils::PrintPackageTree(namePkg);
		Position epos =  ParserUtils::GetPackagePosition(namePkg);
		AmbiguityChecker achk(qnm.GetRoleList(), epos, true);

		if( (resultEnum = const_cast<EnumType *>(achk.IsEnumType(true)) ) != NULL )
		{			
			// иначе, сохраняем список квалификаторов, проверяем на доступность
			// и возвращаем перечисление
			stList = qnm.GetQualifierList();
			
			if( !defination )
				CheckerUtils::CheckAccess(qnm, *resultEnum, epos);
			return resultEnum;
		}

		// иначе класс не объявлен в области видимости
		else
			theApp.Error(key.GetPos(), "'enum %s' - перечисление не объявлено",
							cnam.c_str() );				
	}

	return resultEnum;
}

	
