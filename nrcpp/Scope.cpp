// реализация методов таблицы символов - SymbolTable.cpp

#pragma warning(disable: 4786)
#include <nrc.h>
using namespace NRC;

#include "Limits.h"
#include "Application.h"
#include "Object.h"
#include "Scope.h"



// функтор
bool IdentifierListFunctor::operator() ( const IdentifierList &il ) const
{
	return il.front()->GetName() == name;
}


// создадим таблицу
HashTab::HashTab( unsigned htsz )
	: size(htsz)
{
	table = new ListOfIdentifierList[size];
}


// деструктор уничтожает таблицу
HashTab::~HashTab()
{
	delete [] table;
}


// функция хеширования, возвращает индекс по имени
unsigned HashTab::Hash( const CharString &key ) const
{
	register const char *p;
	register unsigned int h = 0, g;
		
	for(p = key.c_str(); *p != '\0'; p++)
		if(g = (h = (h << 4) + *p) & 0xF0000000)
			h ^= g >> 24 ^ g;
		
	return h % size;
}


// найти элемент
IdentifierList *HashTab::Find( const CharString &key ) const
{
	ListOfIdentifierList &lst = table[Hash(key)];
	ListOfIdentifierList ::iterator p = 
		find_if( lst.begin(), lst.end(), IdentifierListFunctor(key) );

	return p == lst.end() ? NULL : &*p;
}


// вставить элемент в таблицу
unsigned HashTab::Insert( const Identifier *id )
{
	ListOfIdentifierList &lst = table[ Hash(id->GetName()) ];
	ListOfIdentifierList::iterator p = find_if(lst.begin(), lst.end(), 
		IdentifierListFunctor(id->GetName()) );

	// если такой элемент не найден, создаем новый список идентификатор, 
	// вставляем его в список списков
	if( p == lst.end() )
	{
		IdentifierList il;
		il.push_back(id);
		lst.push_back( il );		
		return 1;
	}
	
	// иначе вставляем идентификатор в имеющийся список
	else	
	{
		(*p).push_back(id);	
		return (*p).size();
	}	
}


// добавить using-область видимости, которая будет использоваться
// исключительно при поиске
void GeneralSymbolTable::AddUsingNamespace( NameSpace *ns ) 
{
	if( usingList.HasSymbolTable(ns) < 0 )
		usingList.AddSymbolTable(ns);
}


// добавить using-область видимости, которая будет использоваться
// исключительно при поиске
void FunctionSymbolTable::AddUsingNamespace( NameSpace *ns ) 
{
	if( usingList.HasSymbolTable(ns) < 0 )
		usingList.AddSymbolTable(ns);
}


// функция поиска, специально вызываемая при поиске с учетом используемых
// областей видимости. Должна переопределеяться в NameSpace
bool GeneralSymbolTable::FindSymbolWithUsing( const CharString &name,
					SymbolTableList &tested, IdentifierList &out ) const
{	
	if( IdentifierList *il = hashTab->Find(name) )
		out.insert( out.end(), il->begin(), il->end() );
	
	// далее для каждой используемой ОВ, выполняем операцию поиска
	for( int i = 0; i<usingList.GetSymbolTableCount(); i++ )
	{
		// если эта область видимости уже участвовала в поиске,
		// предупреждаем зацикливание, когда две ОВ используют друг друга
		if( tested.HasSymbolTable( usingList[i] ) >= 0 )
			continue;

		tested.AddSymbolTable( usingList[i] );
		const GeneralSymbolTable *ns = dynamic_cast<const NameSpace *>(usingList[i]);
		INTERNAL_IF( ns == NULL );

		// все что найдено, записывается в out
		ns->FindSymbolWithUsing(name, tested, out);			
	}

	// если список не пустой
	return !out.empty();
}


// поиск символа в локальной или глобальной области видимости,
// ищет в своей области видимости и потом к найденному результату
// прибавляет поиск в дружеских областях видимости (using). При этом
// следует контролировать, чтобы процесс не зацикливался т.к. 2 области
// могут быть дружескими по отношению друг к другу. Если не одно из
// имен не найдено - возвращается false
bool GeneralSymbolTable::FindSymbol( const NRC::CharString &name, 
			IdentifierList &out ) const 
{
	SymbolTableList tested;	
	return FindSymbolWithUsing(name, tested, out);
}


// производит поиск без учета using-областей, только глобальной (или локальной) ОВ
bool GeneralSymbolTable::FindInScope( const NRC::CharString &name, IdentifierList &out ) const 
{		
	if( IdentifierList *il = hashTab->Find(name) )
		out.insert( out.end(), il->begin(), il->end() );

	return !out.empty();	
}


// вставка символа таблицу
bool GeneralSymbolTable::InsertSymbol( Identifier *id ) 
{
	unsigned icnt = hashTab->Insert(id);

	// прибавляем к С-имени число, для разрешения конфликта имен,
	// если только у идентификатора есть С-имя
	if( icnt > 1 && !id->GetC_Name().empty() )
		const_cast<string &>(id->GetC_Name()) += CharString((int)icnt).c_str();
	return true;
}


// функция поиска, специально вызываемая при поиске с учетом используемых
// областей видимости. Должна переопределеяться в NameSpace
bool FunctionSymbolTable::FindSymbolWithUsing( const CharString &name,
							SymbolTableList &tested, IdentifierList &out ) const
{
	FindInScope(name, out);
	
	// далее для каждой используемой ОВ, выполняем операцию поиска
	for( int i = 0; i<usingList.GetSymbolTableCount(); i++ )
	{
		// если эта область видимости уже участвовала в поиске,
		// предупреждаем зацикливание, когда две ОВ используют друг друга
		if( tested.HasSymbolTable( usingList[i] ) >= 0 )
			continue;

		tested.AddSymbolTable( usingList[i] );
		const GeneralSymbolTable *ns = dynamic_cast<const NameSpace *>(usingList[i]);
		INTERNAL_IF( ns == NULL );
		ns->FindSymbolWithUsing(name, tested, out);		
	}

	// если что-то найдено - true
	return !out.empty();
}


// поиск символа в функциональной области видимости, потом в списке параметров функции
// ищет в своей области видимости и потом к найденному результату
// прибавляет поиск в дружеских областях видимости (using). 
bool FunctionSymbolTable::FindSymbol( const NRC::CharString &name, IdentifierList &out ) const
{
	SymbolTableList tested;	
	return FindSymbolWithUsing(name, tested, out);
}


// производит поиск без учета using-областей, только в функциональной области
// видимости и в спике параметров
bool FunctionSymbolTable::FindInScope( const NRC::CharString &name, IdentifierList &out ) const
{
	ListOfIdentifierList::const_iterator p = 
		find_if( localIdList.begin(), localIdList.end(), IdentifierListFunctor(name) );
	if( p != localIdList.end() )
		out.insert( out.end(), (*p).begin(), (*p).end() );
	
	// ищем также и в параметрах
	const FunctionParametrList &fpl = pFunction.GetFunctionPrototype().GetParametrList();
	int pix = fpl.HasParametr(name);

	// если найден параметр, добавляем его в рещультирующий список
	if( pix >= 0 )	
		out.push_back( &*fpl[pix] );
	return !out.empty();
}


// вставка символа таблицу
bool FunctionSymbolTable::InsertSymbol( Identifier *id )
{
	ListOfIdentifierList::iterator p = 
		find_if( localIdList.begin(), localIdList.end(), IdentifierListFunctor(id->GetName()) );

	// если список с таким именем создан, вставляем в него
	if( p != localIdList.end() )
		(*p).push_back(id);

	// иначе создаем новый список
	else
	{
		IdentifierList il;
		il.push_back(id);
		localIdList.push_back(il);
	}
	
	return true;	
}


// очищает всю таблицу
void FunctionSymbolTable::ClearTable()
{
}


// поиск символа	
bool LocalSymbolTable::FindSymbol( const NRC::CharString &name, 
					IdentifierList &out ) const 
{
	if( !table )
		return false;

	ListOfIdentifierList::const_iterator p = 
			find_if( table->begin(), table->end(), IdentifierListFunctor(name) );
	if( p != table->end() )
	{
		out.insert( out.end(), (*p).begin(), (*p).end() );
		return true;
	}

	return false;
}


// вставка символа таблицу
bool LocalSymbolTable::InsertSymbol( Identifier *id ) 
{
	// если таблица не создана, создать ее
	if( table == NULL )
		table = new ListOfIdentifierList;
	ListOfIdentifierList::iterator p = 
		find_if( table->begin(), table->end(), IdentifierListFunctor(id->GetName()) );

	// если список с таким именем создан, вставляем в него
	if( p != table->end() )
		(*p).push_back(id);

	// иначе создаем новый список
	else
	{
		IdentifierList il;
		il.push_back(id);
		table->push_back(il);
	}

	return true;
}


// глубокий поиск по всем областям видимости, 
// поиск начинается с конца, т.е. с текущей ОВ и возвращается
// первое соответствие - список идентификаторов имеющих
// заданное имя. Если соотв. нет - возвращает пустая строка
bool Scope::DeepSearch( const CharString &name, IdentifierList &out ) const
{	
	// проходим по всем областям видимости
	list<SymbolTable *>::const_iterator i = symbolTableStack.end();	
	for( i--; ; i-- )	
	{
		if( (*i)->FindSymbol(name, out) )
			return true;
	
		if( i == symbolTableStack.begin() )
			break;
	}

	return false;
}


// получить ближайщую глобальную область видимости
const SymbolTable &Scope::GetGlobalSymbolTable() const
{
	// проходим по всем областям видимости
	list<SymbolTable *>::const_iterator i = symbolTableStack.end();	
	for( i--; ; i-- )	
	{
		if( (*i)->IsGlobalSymbolTable() || (*i)->IsNamespaceSymbolTable() )
			return **i;

		if( i == symbolTableStack.begin() )
			break;
	}

	INTERNAL( "'Scope::GetGlobalSymbolTable' не возвратил глобальную ОВ" );
	return *(SymbolTable *)0;
}


// получить ближайшую функциональную область видимости. 
// Текущая область видимости обязательно должна быть локальной
const SymbolTable &Scope::GetFunctionalSymbolTable() const
{
	// если текущая область видимости функциональная, вернуть ее
	if( GetCurrentSymbolTable()->IsFunctionSymbolTable() )
		return *GetCurrentSymbolTable();

	INTERNAL_IF( !GetCurrentSymbolTable()->IsLocalSymbolTable() );
	list<SymbolTable *>::const_iterator i = symbolTableStack.end();	
	for( i--; ; i-- )	
		if( (*i)->IsFunctionSymbolTable() )
			return **i;

	INTERNAL( "'Scope::GetFunctionalSymbolTable' не возвратил функциональную ОВ" );
	return *(SymbolTable *)0;
}

