// реализация методов классов представляющих сущности класс, базовый тип, 
// шаблон, шаблонный параметр - BaseType.cpp

#pragma warning(disable: 4786)
#include <nrc.h>

using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "LexicalAnalyzer.h"
#include "Object.h"
#include "Scope.h"
#include "Class.h"


// утилиты транслятора
namespace TranslatorUtils
{
	// генерирует имя области видимости без добавочных символов. Учитывает
	// след. области видимости: глобальная, именованная, классовая, функциональная (локальная).
	// Определен в Translator.h
	const string &GenerateScopeName( const SymbolTable &scope );

	// сгенерировать имя для безимянного идентификатора
	string GenerateUnnamed( );
}


// возвращает индекс друга по указателю, если друга нет вернуть -1
int ClassFriendList::FindClassFriend( const Identifier *p ) const
{
	for( int i = 0; i<friendList.size(); i++ )
		if( friendList[i].IsClass() ) 
		{
			if( &friendList[i].GetClass() == p )
				return i;
		}
		else
			if( &friendList[i].GetFunction() == p )
				return i;

	return -1;
}


// получить все члены с именем name в виде списка строк,
// если такого члена нет, вернуть пустой список
IdentifierList *ClassMemberList::FindMember( const CharString &name ) const
{
	ListOfIdentifierList::iterator p = 
		find_if( memberList.begin(), memberList.end(), IdentifierListFunctor(name) );
	if( p != memberList.end() )
		return &*p;
	return NULL;
}


// вставить член в список. При этом если список перегруженных
// членов не создан, он создается, а если создан, член добавляется
// в конец этого списка
void ClassMemberList::AddClassMember( PClassMember cm ) 
{	
	Identifier *id;
	// член должен преобразовываться к идентификатору
	INTERNAL_IF( (id = dynamic_cast<Identifier *>(&*cm)) == NULL );

	if( IdentifierList *il = FindMember( id->GetName() ) )
	{
		il->push_back(id);

		// прибавляем к С-имени число, для разрешения конфликта имен,
		// если только у идентификатора есть С-имя
		if( !id->GetC_Name().empty() )
			const_cast<string &>(id->GetC_Name()) += CharString((int)il->size()).c_str();
	}

	else
	{
		// иначе создаем новый список идентификаторов
		IdentifierList newil;
		newil.push_back(id);
		memberList.push_back(newil);
	}
	
	// добавляем указатель на член в список для хранения
	// порядка следования членов при объявлении
	order.push_back(cm);
}


// очистить список членов одновременным освобождением памяти
void ClassMemberList::ClearMemberList()
{
	// сначала освобождаем память отведенную для членов
	order.clear();
	memberList.clear();
}


// конструктор с заданием первоначальных параметров, т.е. объект
// класса создается при его объявлении, а не определении
ClassType::ClassType( const NRC::CharString &name, SymbolTable *entry, BT bt, AS as ) :
	Identifier( (name[0] == '<' ? TranslatorUtils::GenerateUnnamed().c_str() : name), entry), 
	BaseType(bt), accessSpecifier(as), uncomplete(true), polymorphic(false), madeVmTable(false),
	abstractMethodCount(0), virtualMethodCount(0), castOperatorList(NULL),
	destructor(NULL), virtualFunctionList(NULL)
{
	c_name = ( "__" + TranslatorUtils::GenerateScopeName(*entry) + GetName().c_str() );
}


// добавить базовый класс
void ClassType::AddBaseClass( const PBaseClassCharacteristic &bcc ) 
{
	baseClassList.AddBaseClassCharacteristic(bcc);
	virtualMethodCount += bcc->GetPointerToClass().virtualMethodCount;

	// следует добавить операторы приведения
	if( bcc->GetPointerToClass().GetCastOperatorList() != NULL )
	{
		const ClassType &cls = bcc->GetPointerToClass();
		if( castOperatorList == NULL )
			castOperatorList = new CastOperatorList;
		castOperatorList->insert(castOperatorList->end(), 
			cls.GetCastOperatorList()->begin(), cls.GetCastOperatorList()->end());
	}
}


// поиск члена в классе, а также в базовых классах, в случае успешного поиска,
// возвращает список членов, в противном случае, список пуст. 
// friend-декларации членами не являются и поэтому в поиске не учавствуют
bool ClassType::FindSymbol( const NRC::CharString &name, IdentifierList &out ) const
{	
	// если найдено имя в этом классе, значит оно перекрывает все
	// имена из базовых классов и дальнейший поиск не имеет смысла
	if( IdentifierList *il = (memberList).FindMember(name) )
	{
		out.insert( out.end(), il->begin(), il->end() );
		return true;
	}
	
	// иначе ничего не найдено, продолжаем поиск по базовым классам
	// с добавлением членов в результирующий список
	StringList tempList;
	for( int i = 0; i<baseClassList.GetBaseClassCount(); i++ )	
		baseClassList[i]->GetPointerToClass().FindSymbol(name, out);
	
	return !out.empty();	
}


// поиск только внутри класса, без учета базовых классов
bool ClassType::FindInScope( const NRC::CharString &name, IdentifierList &out ) const
{
	if( IdentifierList *il = (memberList).FindMember(name) )
	{
		out.insert( out.end(), il->begin(), il->end() );
		return true;
	}
	else
		return false;	
}


// вставка члена в таблицу
bool ClassType::InsertSymbol( Identifier *id )
{
	ClassMember *cm = dynamic_cast<ClassMember *>(id);
	INTERNAL_IF( cm == NULL );
	memberList.AddClassMember( cm );
	
	// если член является методом, проверим, возможно его стоит
	// сохранить в отдельной структуре
	if( Method *meth = dynamic_cast<Method *>(&*cm) )
	{		
		if( meth->IsConstructor() )
			constructorList.push_back(static_cast<ConstructorMethod *>(&*cm));
		else if( meth->IsOverloadOperator() &&
			static_cast<const ClassOverloadOperator*>(meth)->IsCastOperator() )
		{
			if( castOperatorList == NULL )
				castOperatorList = new CastOperatorList;
			castOperatorList->push_back(static_cast<ClassCastOverloadOperator *>(&*cm));
		}

		else if( meth->IsDestructor() )
		{
			INTERNAL_IF( destructor != NULL );
			destructor = meth;
		}
	}

	return true;
}


// очищает всю таблицу
void ClassType::ClearTable()
{
	memberList.ClearMemberList();
}
