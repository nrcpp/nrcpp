// реализация синтаксического анализатора - Parser.cpp

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
#include "Maker.h"
#include "MemberMaker.h"
#include "Parser.h"
#include "Body.h"
#include "Coordinator.h"
#include "Reader.h"
#include "Checker.h"
#include "ExpressionMaker.h"
#include "BodyMaker.h"
#include "Translator.h"

using namespace ParserUtils;

 
// счетчик переполнения
int OverflowController::counter = 0;

// счетчик переполнения для конструкций
int StatementParserImpl::OverflowStackController::deep = 0;


// проверяет по пакету, и последней считанной лексеме, возможно
// ли определение класса
inline static bool NeedClassParserImpl( const Lexem &lastLxm, const NodePackage *np ) ;


// проверяет по идентификатору и последней считанной лексеме, необходим
// ли функции разбор тела
inline static bool NeedFunctionParserImpl( LexicalAnalyzer &la, const Identifier *id );


// проверяет по пакету, и последней считанной лексеме, требуется ли 
// определение перечисления
inline static bool NeedEnumReading( const Lexem &lastLxm, const NodePackage *np ) ;


// считать список инициализации и выполнить проверку
inline static ListInitComponent *ReadInitList( LexicalAnalyzer &la, 
			PDeclarationMaker &pdm, const Position &errPos );


// метод запуска синтаксического анализатора
void Parser::Run()
{
	Lexem lxm;
	for( ;; )
	{
		lxm = lexicalAnalyzer.NextLexem();
		if( lxm == EOF )
		{
			// все области видимости должны быть закрыты
			if( !GetCurrentSymbolTable().IsGlobalSymbolTable() )
				theApp.Fatal( lxm.GetPos(), "неожиданный конец файла" );
			break;
		}

		// либо объявление области видимости, либо объявление синонима ОВ
		if( lxm == KWNAMESPACE )
		{
			lxm = lexicalAnalyzer.NextLexem();

			// безимянная область видимости
			if( lxm == '{' )
			{
				if( MakerUtils::MakeNamepsaceDeclRegion(NULL) )
					crampControl.push_back(0);
			}

			// иначе считываем имя области видимости
			else
			{
				lexicalAnalyzer.BackLexem();
				try {
					// считываем имя области видимости и передаем его в строитель
					// области видимости
					PNodePackage nspkg = QualifiedConstructionReader(lexicalAnalyzer, 
						false, true).ReadQualifiedConstruction();
					
					Lexem nxt = lexicalAnalyzer.NextLexem() ;
						
					// возможно идет объявление синонима области видимости
					if( nxt == '=' )
					{
						MakerUtils::MakeNamespaceAlias( &*nspkg, &*QualifiedConstructionReader(
							lexicalAnalyzer, false, true).ReadQualifiedConstruction());

						if( lexicalAnalyzer.NextLexem() != ';' )
							throw lexicalAnalyzer.LastLexem();
					}

					else
					{
						if( MakerUtils::MakeNamepsaceDeclRegion(&*nspkg) )
							crampControl.push_back(0);

						if( nxt != '{' )
							SyntaxError(nxt);						
					}
			
				} catch( const Lexem &ep ) {
					SyntaxError(ep);
					IgnoreStreamWhileNotDelim(lexicalAnalyzer);
				}

			}
		}

		// using namespace 'name' или
		// using 'name'
		else if( lxm == KWUSING )
		{
			// использование области видимости
			if( lexicalAnalyzer.NextLexem() == KWNAMESPACE )
			{
				try {
					// считываем имя области видимости и передаем его в строитель
					// области видимости
					PNodePackage nspkg = QualifiedConstructionReader(lexicalAnalyzer, 
						false, true).ReadQualifiedConstruction();
					
					MakerUtils::MakeUsingNamespace( &*nspkg );
					if( lexicalAnalyzer.NextLexem() != ';' )
						throw lexicalAnalyzer.LastLexem();

				} catch( const Lexem &ep ) {
					SyntaxError(ep);
					IgnoreStreamWhileNotDelim(lexicalAnalyzer);				
				}
				
			}

			// иначе using-декларация
			else
			{
				lexicalAnalyzer.BackLexem();
				try {
					// считываем имя области видимости и передаем его в строитель
					// области видимости
					PNodePackage nspkg = QualifiedConstructionReader(
						lexicalAnalyzer).ReadQualifiedConstruction();
					
					MakerUtils::MakeUsingNotMember( &*nspkg );
					if( lexicalAnalyzer.NextLexem() != ';' )
						throw lexicalAnalyzer.LastLexem();

				} catch( const Lexem &ep ) {
					SyntaxError(ep);
					IgnoreStreamWhileNotDelim(lexicalAnalyzer);				
				}
				
			}
		}

		else if( lxm == '}' )
		{
			// проверяем, что находится в crampControl. Если 0,
			// достаем из стека область видимости, иначе задаем новую
			// спецификацию связывания
			if( crampControl.empty() )
				SyntaxError(lxm);
			else if( crampControl.back() != 0 )
			{
				crampControl.pop_back();
				linkSpec = crampControl.empty() || crampControl.back() != 1 ? LS_CPP : LS_C;
			}

			// иначе именованная область видимости
			else
			{
				INTERNAL_IF( !GetCurrentSymbolTable().IsNamespaceSymbolTable() );
				GetScopeSystem().DestroySymbolTable();
				crampControl.pop_back();
			}			
		}

		else
		{
			lexicalAnalyzer.BackLexem();
			DeclareParserImpl(lexicalAnalyzer).Parse();		
		}
	}
}


// метод разбора деклараций, возвращает пакет, по заголовку которго 
// можно определить что делать дальше: создавать парсер класс, 
// создавать парсер функции, либо прекратить, т.к. декларация проанализирована. 
// В случае если прекратить, интеллектуальный указатель сам освободит память
void DeclareParserImpl::Parse()
{	
	NodePackage *tsl = NULL, *dcl = NULL;
	DeclaratorReader dr(DV_GLOBAL_DECLARATION, lexicalAnalyzer);

	try {		
		dr.ReadTypeSpecifierList();
		tsl = dr.GetTypeSpecPackage().Release();
				
		// если последняя считанная лексема '{', а предпоследняя "строка",
		// значит имеем спецификацию связывания, и выходим
		if( lexicalAnalyzer.LastLexem() == '{' && tsl->GetLastChildPackage() && 
			tsl->GetLastChildPackage()->GetPackageID() == STRING )
			return;

		// в этом месте может быть определение класса, если последние
		// два пакета были ключ класса и PC_QUALIFIED_NAME, а последняя
		// считанная лексема - '{' или ':'
		if( NeedClassParserImpl(lexicalAnalyzer.LastLexem(), tsl) )
		{
			ClassParserImpl cpi(lexicalAnalyzer, tsl, ClassMember::NOT_CLASS_MEMBER);
			cpi.Parse();

			// генерируем определение класса, если класс был построен
			if( cpi.IsBuilt() )
				TranslatorUtils::TranslateClass(cpi.GetClassType());

			// завершаем определение
			if( lexicalAnalyzer.NextLexem() == ';' )
			{
				delete tsl;
				return;
			}
			else
				lexicalAnalyzer.BackLexem();

		}

		// иначе если требуется считать перечисление, считываем
		else if( NeedEnumReading(lexicalAnalyzer.LastLexem(), tsl) )
		{
			EnumParserImpl epi(lexicalAnalyzer, tsl, ClassMember::NOT_CLASS_MEMBER);
			epi.Parse();
			if( lexicalAnalyzer.NextLexem() == ';' )
			{
				delete tsl;
				return;
			}
			else
				lexicalAnalyzer.BackLexem();

		}

		// иначе если объявление класса или перечисления
		else if( tsl->GetChildPackageCount() == 2 && 
				 tsl->GetChildPackage(1)->GetPackageID() == PC_QUALIFIED_NAME && 
				 lexicalAnalyzer.LastLexem() == ';' )
		{
			register int pc = tsl->GetChildPackage(0)->GetPackageID() ;
			lexicalAnalyzer.NextLexem();

			if( pc == KWCLASS || pc == KWSTRUCT || pc == KWUNION )
			{
				ClassTypeMaker ctm( tsl, ClassMember::NOT_CLASS_MEMBER );
				ctm.Make();
				return;
			}

			else if( pc == KWENUM )
			{
				EnumTypeMaker etm( tsl,  ClassMember::NOT_CLASS_MEMBER );
				etm.Make();			
				return;
			}
		}


		// считываем список деклараций
		bool firstDecl = true;
		for( ;; )
		{
			dcl = dr.ReadNextDeclarator().Release();

			// в декларации должно присутствовать имя
			if( dcl->FindPackage(PC_QUALIFIED_NAME) < 0 )
				throw lexicalAnalyzer.LastLexem() ;

			// далее создаем декларацию из пакета и проверяем ее
			DeclarationCoordinator dcoord(tsl, dcl);
			PDeclarationMaker dmak = dcoord.Coordinate();
			if( !dmak.IsNull() )
			{
				dmak->Make();

				// далее идет проверка на тело функции, должны быть след. условия
				// 1. это первая декларация
				// 2. идентификатор является функцией
				// 3. последняя считанная лексема - '{', или ':', try - если функция конструктор				
				if( firstDecl )
				{	
					if( NeedFunctionParserImpl(lexicalAnalyzer, dmak->GetIdentifier()) )
					{
						Function &fn = *const_cast<Function *>(
							static_cast<const Function *>(dmak->GetIdentifier()) );

						// если у функции уже есть тело, игнорируем его и выходим
						if( fn.IsHaveBody() )
						{
							theApp.Error(lexicalAnalyzer.LastLexem().GetPos(), 
								"'%s' - у функции уже есть тело",
								fn.GetQualifiedName().c_str());

							// игнорируем его
							lexicalAnalyzer.BackLexem();
							FunctionBodyReader(lexicalAnalyzer, true).Read();						
						}

						else
						{
							lexicalAnalyzer.BackLexem();
							FunctionParserImpl fnpi(lexicalAnalyzer, fn);
							fnpi.Parse();							
						}

						dcoord.RestoreScopeSystem();
						return;
					}

					firstDecl = false;
				}
			}

			// инициализатор объекта, задаем для генерации
			PObjectInitializator objIator = NULL;

			// считываем инициализатор
			PExpressionList initList = dr.GetInitializatorList();
			if( lexicalAnalyzer.LastLexem() == '=' )				
			{
				// если список инициализаторов уже считан, то это синтаксическая
				// ошибка
				if( !initList.IsNull() )
					throw lexicalAnalyzer.LastLexem();

				// проверяем, если следующая лексема '{',
				// значит считываем список инициализации
				else if( lexicalAnalyzer.NextLexem() == '{' )
				{
					objIator = BodyMakerUtils::MakeObjectInitializator(
					  ReadInitList(lexicalAnalyzer, dmak, lexicalAnalyzer.LastLexem().GetPos()) );
					INTERNAL_IF( dmak.IsNull() );
					
					// генерируем декларацию
					TranslatorUtils::TranslateDeclaration( 
						*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()),
						objIator, true);

					// предотвращаем проверку инициализации
					dmak = NULL;
				}

				// иначе считываем обычное выражение
				else
				{
					lexicalAnalyzer.BackLexem();
					ExpressionReader er( lexicalAnalyzer, NULL, true );
					er.Read();
					initList = new ExpressionList;
					initList->push_back(er.GetResultOperand());					
				}
			}

			// инициализируем объект списком значений. Список значений
			// может отсутствовать, в этом случае происходит инициализация по умолчанию
			if( !dmak.IsNull() && dmak->GetIdentifier() != NULL )			
			{
				dmak->Initialize( *initList );
				// строитель может вернуть NULL, если идентификатор не является объектом			
				objIator = BodyMakerUtils::MakeObjectInitializator(initList, *dmak);
				// генерируем декларацию
				TranslatorUtils::TranslateDeclaration( 
					*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()),
					objIator, true);
			}
						
			// возможно было объявление члена, в этом случае требуется
			// восстановить систему ОВ, т.к. в нее были помещены области 
			// видимости члена
			dcoord.RestoreScopeSystem();
				
			if( lexicalAnalyzer.LastLexem() != ';' &&  
				lexicalAnalyzer.LastLexem() != ',' )
				throw lexicalAnalyzer.LastLexem() ;

			if( lexicalAnalyzer.LastLexem() == ';' )
				break;

			delete dcl;
			dcl = NULL;
		}

	} catch( Lexem &lxm ) {				
		SyntaxError(lxm);
		IgnoreStreamWhileNotDelim(lexicalAnalyzer);
	}

	delete dcl;
	delete tsl;
}


// метод разбора деклараций. Возвращает true, если декларация была считана,
// иначе возвращает false
void InstructionParserImpl::Parse()
{
	// считывание, либо выражения, либо декларации
	TypeExpressionReader ter( lexicalAnalyzer );	
	const NodePackage *tsl = NULL;

	// инструкции могут возбуждать исключительные ситуации
	try
	{	
		ter.Read();
		INTERNAL_IF( ter.GetResultPackage() == NULL );		

		// если имеем выражение, должно быть ';', и на этом разбор интсрукций
		// заканчивается
		if( ter.GetResultPackage()->IsExpressionPackage() )
		{		
			const ExpressionPackage &epkg = 
				static_cast<const ExpressionPackage &>(*ter.GetResultPackage());
		#if  _DEBUG
		//	cout << ExpressionPrinter(epkg.GetExpression()).GetExpressionString() << endl; 
			{
				const ClassType *thisCls = NULL;
				if( const FunctionSymbolTable *fst = GetScopeSystem().GetFunctionSymbolTable() )
					if( fst->GetParentSymbolTable().IsClassSymbolTable() )
						thisCls = &static_cast<const ClassType &>(fst->GetParentSymbolTable());
				ExpressionGenerator eg(epkg.GetExpression(), thisCls);
				eg.Generate();
				cout << eg.GetOutBuffer() << endl;
			} //*/
		#endif

			if( lexicalAnalyzer.LastLexem() != ';' )
				throw lexicalAnalyzer.LastLexem() ;

			// строим инструкцию и выходим
			insList.push_back( BodyMakerUtils::ExpressionInstructionMaker(epkg.GetExpression(), 
				lexicalAnalyzer.LastLexem().GetPos()) );
			delete &epkg;
			return;
		}

		// если есть декларация класса, сохраняем его в списке
		if( ter.GetRedClass() )
			insList.push_back( BodyMakerUtils::ClassInstructionMaker(*ter.GetRedClass(),
				lexicalAnalyzer.LastLexem().GetPos()) );

		// иначе имеем декларацию
		SmartPtr<const NodePackage> rpkg = 
			static_cast<const NodePackage *>(ter.GetResultPackage());		

		// проверяем, если имеем декларацию типа, значит последняя считанная лексема ';'
		// и необходимо выйти, декларация уже построения
		if( rpkg->GetPackageID() == PC_CLASS_DECLARATION )
			return ;
		
		// иначе проверяем, считана ли декларация
		const NodePackage *tsl = static_cast<const NodePackage*>(rpkg->GetChildPackage(0));
		INTERNAL_IF( rpkg->GetPackageID() != PC_DECLARATION || 
				rpkg->GetChildPackageCount() != 2 );
		AutoDeclarationCoordinator dcoord(tsl, (NodePackage*)rpkg->GetChildPackage(1));
		PDeclarationMaker dmak = dcoord.Coordinate();
		// строим декларацию
		if( !dmak.IsNull() )				
			dmak->Make();

		// инициализатор объекта, задаем для генерации
		PObjectInitializator objIator = NULL;

		// считываем инициализатор
		PExpressionList initList = ter.GetInitializatorList();
		if( lexicalAnalyzer.LastLexem() == '=' )				
		{
			// если список инициализаторов уже считан, то это синтаксическая
			// ошибка
			if( !initList.IsNull() )
				throw lexicalAnalyzer.LastLexem();

			// проверяем, если следующая лексема '{',
			// значит считываем список инициализации
			else if( lexicalAnalyzer.NextLexem() == '{' )
			{
				objIator = BodyMakerUtils::MakeObjectInitializator(
					ReadInitList(lexicalAnalyzer, dmak, lexicalAnalyzer.LastLexem().GetPos()) );

				// после того, как инициализатор и идентификатор известны, строим декларацию
				insList.push_back( BodyMakerUtils::DeclarationInstructionMaker( 
					*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()), objIator,
					lexicalAnalyzer.LastLexem().GetPos() )  );
	
				// предотвращаем проверку инициализации
				dmak = NULL;
			}

			// иначе считываем обычное выражение
			else
			{
				lexicalAnalyzer.BackLexem();
				ExpressionReader er( lexicalAnalyzer, NULL, true );
				er.Read();
				initList = new ExpressionList;
				initList->push_back(er.GetResultOperand());					
			}
		}

		// инициализируем объект списком значений. Список значений
		// может отсутствовать, в этом случае происходит инициализация по умолчанию
		if( !dmak.IsNull() && dmak->GetIdentifier() != NULL )		
		{
			dmak->Initialize( *initList ),

			// строитель может вернуть NULL, если идентификатор не является объектом			
			objIator = BodyMakerUtils::MakeObjectInitializator(initList, *dmak);
		
			// после того, как инициализатор и идентификатор известны, строим декларацию
			insList.push_back( BodyMakerUtils::DeclarationInstructionMaker( 
				*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()), objIator,
				lexicalAnalyzer.LastLexem().GetPos() ) );
		}

		if( lexicalAnalyzer.LastLexem() == ';' )
			;

		// иначе, если не ',', выводим синтаксическую ошибку
		else if( lexicalAnalyzer.LastLexem() != ',' )
			throw lexicalAnalyzer.LastLexem() ;

		// иначе считываем список деклараций
		else
		for( ;; )
		{
			DeclaratorReader dr(DV_LOCAL_DECLARATION, lexicalAnalyzer);
			PNodePackage dcl = dr.ReadNextDeclarator();

			// в декларации должно присутствовать имя
			if( dcl->FindPackage(PC_QUALIFIED_NAME) < 0 )
				throw lexicalAnalyzer.LastLexem() ;

			// далее создаем декларацию из пакета и проверяем ее
			AutoDeclarationCoordinator dcoord(tsl, &*dcl);
			PDeclarationMaker dmak = dcoord.Coordinate();
			if( !dmak.IsNull() )			
				dmak->Make();
			
			// считываем инициализатор
			PExpressionList initList = dr.GetInitializatorList();
			if( lexicalAnalyzer.LastLexem() == '=' )				
			{
				// если список инициализаторов уже считан, то это синтаксическая
				// ошибка
				if( !initList.IsNull() )
					throw lexicalAnalyzer.LastLexem();
							
				// считываем список инициализации
				else if( lexicalAnalyzer.NextLexem() == '{' )
				{
					objIator = BodyMakerUtils::MakeObjectInitializator(
					  ReadInitList(lexicalAnalyzer, dmak, lexicalAnalyzer.LastLexem().GetPos()) );			
		
					// после того, как инициализатор и идентификатор известны, строим декларацию
					insList.push_back( BodyMakerUtils::DeclarationInstructionMaker( 
						*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()), objIator,
						lexicalAnalyzer.LastLexem().GetPos() ) );

					// предотвращаем проверку инициализации
					dmak = NULL;
				}

				// иначе считываем обычное выражение			
				else
				{
					lexicalAnalyzer.BackLexem();
					ExpressionReader er( lexicalAnalyzer, NULL, true );
					er.Read();
					initList = new ExpressionList;
					initList->push_back(er.GetResultOperand());					
				}
			}

			// инициализируем объект списком значений. Список значений
			// может отсутствовать, в этом случае происходит инициализация по умолчанию
			if( !dmak.IsNull() && dmak->GetIdentifier() != NULL )
			{
				dmak->Initialize( *initList ),
				objIator = BodyMakerUtils::MakeObjectInitializator(initList, *dmak);
				insList.push_back( BodyMakerUtils::DeclarationInstructionMaker( 
					*dynamic_cast<const TypyziedEntity *>(dmak->GetIdentifier()), objIator,
					lexicalAnalyzer.LastLexem().GetPos() )  );
			}
			
			// проверяем, если есть
			if( lexicalAnalyzer.LastLexem() == ';' )
				break;

			// иначе, если не ',', выводим синтаксическую ошибку
			else if( lexicalAnalyzer.LastLexem() != ',' )
				throw lexicalAnalyzer.LastLexem();
		}		

	// перехватываем метку
	} catch( const LabelLexem &labLxm ) {
		// если мы в блоке и имеем считанную метку, передаем ее дальше
		// StatementParserImpl
		if( inBlock )
			throw;

		// иначе обрабатываем как синтаксическую ошибку
		SyntaxError(labLxm);
		IgnoreStreamWhileNotDelim(lexicalAnalyzer);
		insList.clear();
	
	// перехватываем в остальных случаях
	} catch( const Lexem &lxm ) {
		SyntaxError(lxm);
		IgnoreStreamWhileNotDelim(lexicalAnalyzer);
		insList.clear();		// очищаем список в случае синтаксической ошибки
	}
}


// в конструктор получаем пакет со списком типов, последним
// в котором должен быть класс, а также лексический анализатор
ClassParserImpl::ClassParserImpl( LexicalAnalyzer &la, NodePackage *np, ClassMember::AS as ) 
	: lexicalAnalyzer(la), typePkg(*np)

{
	// 1. создаем класс, если он еще не создан, а также проверяем
	//    его корректность
	// 2. считываем базовые типы, если последней считанной лексемой 
	//    была ':'
	// 3. проверяем возможность определения класса в текущей области видимости
	// 4. вставляем области видимости класса, которыми он квалифицирован
	//    в объявлении и сам класс в стек областей видимости	
	//
	// Если на этапах 1 и 3 были зафиксированы ошибки, тело класса,
	// игнорируется от '{' до соответвующей ей '}'
	ClassTypeMaker ctm(np, as, GetCurrentSymbolTable(), true);
	clsType = ctm.Make();			// класс может не "построится", тогда clsType == 0
	qualifierList = ctm.GetQualifierList();
	isUnion = clsType && clsType->GetBaseTypeCode() == BaseType::BT_UNION;	

	// требуется считка списка базовых классов
	if( lexicalAnalyzer.LastLexem() == ':' )
		ReadBaseClassList();

	// синтаксическая ошибка
	if( lexicalAnalyzer.LastLexem() != '{' )
		throw lexicalAnalyzer.LastLexem();

	// если класс не был найден, либо его определение невозможно
	// просто игнорируем все тело класса
	if( clsType == NULL || !CheckerUtils::ClassDefineChecker(
				*clsType, qualifierList, GetPackagePosition(np->GetLastChildPackage()) ) 
	  )
	{
		// игнорируем тело класса 
		CompoundStatementReader(lexicalAnalyzer, true).Read();
		wasRead = true;
	}

	// иначе вставляем области видимости класса в общий стек,
	// причем глобальная область видимости не должна вставляться если она есть
	else
	{
		wasRead = false;
		curAccessSpec = clsType->GetBaseTypeCode() == BaseType::BT_CLASS ?
			ClassMember::AS_PRIVATE : ClassMember::AS_PUBLIC; 

		// если первая область видимости глобальная, вынуть из списка
		if( !qualifierList.IsEmpty() && qualifierList[0] == 
			::GetScopeSystem().GetFirstSymbolTable() )
			qualifierList.PopFront();

		// загружаем области видимости
		::GetScopeSystem().PushSymbolTableList(qualifierList);

		// загружаем сам класс
		::GetScopeSystem().MakeNewSymbolTable(clsType);
	}
}


// считать члены класса
void ClassParserImpl::Parse()
{
	// если не было ошибки при объявлении имени класса, считываем тело класса,
	// в противном случае оно уже считано
	if( wasRead )		
		return;
	
	// считываем '{'
	INTERNAL_IF( lexicalAnalyzer.NextLexem() != '{' );			
	for( ;; )
	{
		Lexem lxm = lexicalAnalyzer.NextLexem();
		if( lxm == '}' )
			break;

		else if( lxm == KWPUBLIC || lxm == KWPROTECTED || lxm == KWPRIVATE )
		{
			if( lexicalAnalyzer.NextLexem() != ':' )
			{
				SyntaxError(lexicalAnalyzer.LastLexem());
				lexicalAnalyzer.BackLexem();
			}
							
			curAccessSpec = lxm == KWPUBLIC ? ClassMember::AS_PUBLIC
				: (lxm == KWPRIVATE ? ClassMember::AS_PRIVATE : ClassMember::AS_PROTECTED);
		}
		
		// если using-декларация
		else if( lxm == KWUSING )
		{
			try {
				// считываем имя области видимости и передаем его в строитель
				// области видимости
				PNodePackage nspkg = QualifiedConstructionReader(
					lexicalAnalyzer).ReadQualifiedConstruction();
				
				MakerUtils::MakeUsingMember( &*nspkg, curAccessSpec );

				// проверяем, если считали оператор приведения, значит
				// проверяем последнюю лексему
				if( nspkg->FindPackage(PC_CAST_OPERATOR) >= 0 )
				{	
					if( lexicalAnalyzer.LastLexem() != ';' )
						throw lexicalAnalyzer.LastLexem();
				}

				else if( lexicalAnalyzer.NextLexem() != ';' )
					throw lexicalAnalyzer.LastLexem();

			} catch( const Lexem &ep ) {
				SyntaxError(ep);
				IgnoreStreamWhileNotDelim(lexicalAnalyzer);				
			}
		}

		// если декларация шаблона 
		else if( lxm == KWTEMPLATE )
		{
		}

		// иначе считываем член класса
		else
		{
			lexicalAnalyzer.BackLexem();
			ParseMember();			
		}
	}

	// задаем классу флаг, что он полностью объявлен, т.е. не является
	// не полным
	clsType->uncomplete = false;

	// генерируем специальные функции-члены, которые не заданы явно,
	// к-ор по умолчанию, к-ор копирования, д-ор, оператор копирования
	GenerateSMF();

	// обработать считанные inline-функции
	LoadInlineFunctions();

	// вытаскиваем все области, которые были положены в стек в конструкторе
	// и задаем классу что он сформирован
	for( int i = 0; i<qualifierList.GetSymbolTableCount(); i++ )
		::GetScopeSystem().DestroySymbolTable();

	// удаляем сам класс из стека областей видимости
	::GetScopeSystem().DestroySymbolTable();

	// загружаем friend-функции, после того как классовая область видимости
	// удалена.
	LoadFriendFunctions();
}


// считать список базовых типов и сохранить их в классе,
// также проверить их корректность
void ClassParserImpl::ReadBaseClassList()
{
	// считываем ':'
	INTERNAL_IF( lexicalAnalyzer.NextLexem() != ':' );

	// далее считываем список базовых классов, пока не появится '{',
	// либо не будет синтаксической ошибки
	for( ;; )
	{
		PNodePackage bcp = new NodePackage( PC_BASE_CLASS );
		Lexem lxm = lexicalAnalyzer.NextLexem();

		if( lxm == KWVIRTUAL )
		{
			bcp->AddChildPackage( new LexemPackage(lxm) );

			lxm = lexicalAnalyzer.NextLexem();
			if( lxm == KWPUBLIC || lxm == KWPROTECTED || lxm == KWPRIVATE )
				bcp->AddChildPackage( new LexemPackage(lxm) );
			else
				lexicalAnalyzer.BackLexem();
		}

		else if( lxm == KWPUBLIC || lxm == KWPROTECTED || lxm == KWPRIVATE )
		{
			bcp->AddChildPackage( new LexemPackage(lxm) );

			lxm = lexicalAnalyzer.NextLexem();
			if( lxm == KWVIRTUAL )
				bcp->AddChildPackage( new LexemPackage(lxm) );
			else
				lexicalAnalyzer.BackLexem();
		}

		else
			lexicalAnalyzer.BackLexem();

		// считываем имя класса
		QualifiedConstructionReader qcr(lexicalAnalyzer, false, true);
		bcp->AddChildPackage( qcr.ReadQualifiedConstruction().Release() );

		// если класс не был найден, то создавать базовый класс для него
		// не нужно, просто продолжаем считывание 
		if( clsType != NULL )
		{
		
		// создаем базовый класс
		PBaseClassCharacteristic bchar = MakerUtils::MakeBaseClass( &*bcp, 
			clsType->GetBaseTypeCode() == BaseType::BT_CLASS );

		// может быть NULL, если при создании базового класса была ошибка
		if( bchar.IsNull() )
			;

		// проверяем, имеется ли класс в списке
		else if( clsType->baseClassList.HasBaseClass( &bchar->GetPointerToClass() ) >= 0 )
			theApp.Error( GetPackagePosition(bcp->GetLastChildPackage()),
				"'%s' - класс уже задан как базовый", 
					bchar->GetPointerToClass().GetName().c_str());

		// иначе добавляем, при этом если класс полиморфный, то и формируемый
		// класс полиморфный, также добавляем кол-во абстрактных методов
		else
		{
			clsType->AddBaseClass(bchar);
			const ClassType &bcls = bchar->GetPointerToClass();
			if( bcls.IsPolymorphic() )
				clsType->polymorphic = true;

			clsType->abstractMethodCount += bcls.abstractMethodCount;			
		}
		
		} // clsType != NULL

		// следующая лексема должна быть либо ',' либо '{',	
		lxm = lexicalAnalyzer.NextLexem();
		if( lxm != ',' && lxm != '{' )
		{
			clsType->baseClassList.ClearBaseClassList();	// очищаем список
			throw lxm;										// синтаксическая ошибка
		}

		if( lxm == '{' )
		{
			// проверяем, если класс является объединением,
			// он не может иметь списка базовых классов
			if( clsType && clsType->GetBaseTypeCode() == BaseType::BT_UNION &&
				!clsType->baseClassList.IsEmpty() )
			{
				theApp.Error(lxm.GetPos(), 
					"'%s' - объединение не может иметь базовых классов",
					clsType->GetName().c_str());
				 clsType->baseClassList.ClearBaseClassList();
			}

			lexicalAnalyzer.BackLexem();
			break;
		}
	}
}


// разобрать член
void ClassParserImpl::ParseMember( )
{
	NodePackage *tsl = NULL, *dcl = NULL;
	DeclaratorReader dr(DV_CLASS_MEMBER, lexicalAnalyzer);

	try {		
		dr.ReadTypeSpecifierList();
		tsl = dr.GetTypeSpecPackage().Release();	
				
		// в этом месте может быть определение класса, если последние
		// два пакета были ключ класса и PC_QUALIFIED_NAME, а последняя
		// считанная лексема - '{' или ':'
		if( NeedClassParserImpl(lexicalAnalyzer.LastLexem(), tsl) )
		{
			ClassParserImpl cpi(lexicalAnalyzer, tsl, curAccessSpec);
			cpi.Parse();

			if( lexicalAnalyzer.NextLexem() == ';' )
			{
				delete tsl;
				return;
			}
			else
				lexicalAnalyzer.BackLexem();
		}

		// иначе если требуется считать перечисление, считываем
		else if( NeedEnumReading(lexicalAnalyzer.LastLexem(), tsl) )
		{
			EnumParserImpl epi(lexicalAnalyzer, tsl, curAccessSpec);
			epi.Parse();
			if( lexicalAnalyzer.NextLexem() == ';' )
			{
				delete tsl;
				return;
			}
			else
				lexicalAnalyzer.BackLexem();

		}

		// иначе если объявление класса или перечисления
		else if( tsl->GetChildPackageCount() == 2 && 
				 tsl->GetChildPackage(1)->GetPackageID() == PC_QUALIFIED_NAME && 
				 lexicalAnalyzer.LastLexem() == ';' )
		{
			register int pc = tsl->GetChildPackage(0)->GetPackageID() ;
			lexicalAnalyzer.NextLexem();

			if( pc == KWCLASS || pc == KWSTRUCT || pc == KWUNION )
			{
				ClassTypeMaker ctm( tsl, curAccessSpec );
				ctm.Make(); 
				return;
			}

			else if( pc == KWENUM )
			{
				EnumTypeMaker etm( tsl, curAccessSpec );
				etm.Make();
				return;
			}
		}

		// иначе если объявление дружеского класса
		else if( tsl->GetChildPackageCount() == 3 &&
			tsl->GetChildPackage(0)->GetPackageID() == KWFRIEND &&
			(tsl->GetChildPackage(1)->GetPackageID() == KWCLASS  ||
			 tsl->GetChildPackage(1)->GetPackageID() == KWSTRUCT ||
			 tsl->GetChildPackage(1)->GetPackageID() == KWUNION) &&
			tsl->GetChildPackage(2)->GetPackageID() == PC_QUALIFIED_NAME &&
			lexicalAnalyzer.LastLexem() == ';' )
		{
			lexicalAnalyzer.NextLexem();
			MakerUtils::MakeFriendClass(tsl);
			return;
		}


		// считываем список деклараций
		bool firstDecl = true;
		for( ;; )
		{
			dcl = dr.ReadNextDeclarator().Release();

			// здесь проверяем, если дочерних пакетов не у типа не
			// у декларатора, значит выходим
			if( dcl->IsNoChildPackages() && tsl->IsNoChildPackages() )
				throw lexicalAnalyzer.LastLexem();

			// далее создаем декларацию из пакета и проверяем ее
			MemberDeclarationCoordinator dcoord(tsl, dcl, *clsType, curAccessSpec);
			PMemberDeclarationMaker dmak = dcoord.Coordinate();

			// строитель должен быть определен
			if( !dmak.IsNull() )
			{
				dmak->Make();

				// далее идет проверка на тело функции, должны быть след. условия
				// 1. это первая декларация
				// 2. идентификатор является функцией
				// 3. последняя считанная лексема - '{', или ':', try - если функция конструктор				
				if( firstDecl )
				{	
					if( NeedFunctionParserImpl(lexicalAnalyzer, dmak->GetIdentifier()) )
					{
						Function &fn = *const_cast<Function *>(
							static_cast<const Function *>(dmak->GetIdentifier()) );

						// считываем тело в контейнер и сохраняем внутри класса,
						// чтобы по окончании определения класса, разобрать тело метода
						lexicalAnalyzer.BackLexem();
						FunctionBodyReader fbr(lexicalAnalyzer, false);
						fbr.Read();

						// после тела может идти ';'
						if( lexicalAnalyzer.NextLexem() != ';' )
							lexicalAnalyzer.BackLexem();

						// сохраняем
						methodBodyList.push_back( FnContainerPair(&fn, fbr.GetLexemContainer()) );
						return;
					}

					firstDecl = false;
				}


				// если имеем функцию, то это может быть только чистый
				// спецификатор, либо без инициализации, иначе полный инициализатор
				if( const Method *meth = dynamic_cast<const Method *>(dmak->GetIdentifier()) )
				{				
					// далее может идти задание чистого спецификатора для метода					
					if( lexicalAnalyzer.LastLexem() == '=' )
					{					
						if( lexicalAnalyzer.NextLexem().GetBuf() != "0" )
							throw lexicalAnalyzer.LastLexem();

						// проверяем инициализацию
						dmak->Initialize( MIT_PURE_VIRTUAL, *ErrorOperand::GetInstance() );

						// задаем. соотв. параметры класса
						if( meth->IsAbstract() )
							clsType->abstractMethodCount += 1;

						lexicalAnalyzer.NextLexem();
					}

					// иначе инициализации без ничего, в этом случае выявляем
					// виртуальность функции в иерархии
					else
					{						
						dmak->Initialize( MIT_NONE, *ErrorOperand::GetInstance() );
					}
				}

				// иначе инициализация данного-члена или дружеской функции,
				// в последнем случае всегда ошибка
				else if( lexicalAnalyzer.LastLexem() == '=' )
				{
					ExpressionReader er(lexicalAnalyzer, NULL, true);
					er.Read();	

					// проверяем инициализацию данного члена
					if( dmak->GetIdentifier() != NULL )
						dmak->Initialize( MIT_DATA_MEMBER, *er.GetResultOperand() );
				}

				// если идет ':', видимо имеем битовое поле
				else if( lexicalAnalyzer.LastLexem() == ':' )
				{
					ExpressionReader er(lexicalAnalyzer, NULL, true);
					er.Read();
					
					// проверяем создание битового поля
					if( dmak->GetIdentifier() != NULL )
						dmak->Initialize( MIT_BITFIELD, *er.GetResultOperand() );
				}
			}

			if( lexicalAnalyzer.LastLexem() != ';' &&  
				lexicalAnalyzer.LastLexem() != ',' )
				throw lexicalAnalyzer.LastLexem() ;

			if( lexicalAnalyzer.LastLexem() == ';' )
				break;

			delete dcl;
			dcl = NULL;
		}

	} catch( Lexem &lxm ) {				
		SyntaxError(lxm);
		IgnoreStreamWhileNotDelim(lexicalAnalyzer);
	}

	delete dcl;
	delete tsl;
}


// по завершении определения класса, разбираем inline-функции, которые
// находятся в контейнере
void ClassParserImpl::LoadInlineFunctions()
{
	// если список пустой, выйти
	if( methodBodyList.empty() )
		return;

	list<FnContainerPair>::iterator p = methodBodyList.begin();
	while( p != methodBodyList.end() )
	{
		Function &fn = *(*p).first;
		LexemContainer *lc = &*(*p).second;

		// пропускаем постройку, если ф-ция дружественная. Этим
		// будет заниматься ф-ция LoadFriendFunctions
		if( clsType->GetFriendList().FindClassFriend(&fn) >= 0 )
		{
			p++;
			continue;
		}

		if( fn.IsHaveBody() )
		{
			theApp.Error( lc->front().GetPos(),
				"'%s' - у метода уже есть тело", fn.GetQualifiedName().c_str());
			p = methodBodyList.erase(p);
			continue;
		}

		lexicalAnalyzer.LoadContainer( lc );	// загружаем контейнер
		FunctionParserImpl fpi( lexicalAnalyzer, fn );
		fpi.Parse();

		// удаляем тело, чтобы по второму заходу обработать friend-функции
		p = methodBodyList.erase(p);	
	}
}


// загружаем дружественные функции, считанные в процессе постройки класса
void ClassParserImpl::LoadFriendFunctions()
{
	// если список пустой, выйти
	if( methodBodyList.empty() )
		return;

	// проходим по списку загружая функции
	for( list<FnContainerPair>::iterator p = methodBodyList.begin(); 
		p != methodBodyList.end(); p++ )
	{
		Function &fn = *(*p).first;
		LexemContainer *lc = &*(*p).second;

		// область видимости должна быть та же
		INTERNAL_IF( &fn.GetSymbolTableEntry() != &::GetCurrentSymbolTable() );
	
		// у дружественной ф-ции может быть тело
		if( fn.IsHaveBody() )
		{
			theApp.Error( lc->front().GetPos(),
				"'%s' - у метода уже есть тело", fn.GetQualifiedName().c_str());
			continue;
		}

		lexicalAnalyzer.LoadContainer( lc );	// загружаем контейнер
		FunctionParserImpl fpi( lexicalAnalyzer, fn );
		fpi.Parse();

	}
}


// анализатор, пакет с перечислением, спецификатор доступа если находимся внутри класса
void EnumParserImpl::Parse( )
{
	EnumTypeMaker ctm(&typePkg, curAccessSpec,  true);
	enumType = ctm.Make();			// перечисление может не "построится", тогда NULL
	
	// если перечисление не создано, либо оно объявляется дважды, либо
	// оно объявляется не в глобальной области видимости
	if( enumType == NULL ||
		!enumType->IsUncomplete() || 
		(!ctm.GetQualifierList().IsEmpty() &&
		 !(GetCurrentSymbolTable().IsGlobalSymbolTable() || 
		   GetCurrentSymbolTable().IsNamespaceSymbolTable()) ) )
	{
		// выводим ошибку
		if( enumType != NULL )
			theApp.Error(errPos, 
				"'%s' - %s",
				enumType->GetQualifiedName().c_str(),
				!enumType->IsUncomplete() ?
				"перечисление уже определено" : 
				"перечисление должно определяться в глобальной области видимости");
		FunctionBodyReader(lexicalAnalyzer, true).Read();
		return;
	}

	// иначе считываем все константы перечисления
	Lexem lxm ;
	if( lexicalAnalyzer.NextLexem() != '{' )
		throw lexicalAnalyzer.LastLexem();

	// список констант
	EnumConstantList ecl;
	int lastVal = -1;
	for( ;; )
	{		
		lxm = lexicalAnalyzer.NextLexem();

		// следующая константа
		if( lxm == NAME )
		{
			CharString name = lxm.GetBuf();
			lxm = lexicalAnalyzer.NextLexem();

			// считываем инициализатор
			PEnumConstant pec = NULL;			
			if( lxm == '=' )
			{
				ExpressionReader er( lexicalAnalyzer, NULL, true);
				er.Read();
				POperand ival = er.GetResultOperand();				
				double v;
				if( ExpressionMakerUtils::IsInterpretable(ival, v) &&
					ExpressionMakerUtils::IsIntegral(ival->GetType()) )
					lastVal = v;
				else
					theApp.Error(errPos,
						"'%s' - инициализируемое значение должно быть целой константой",
						name.c_str());
				lxm = lexicalAnalyzer.LastLexem();
			}

			else
				lastVal++;

			pec = MakerUtils::MakeEnumConstant(name, curAccessSpec, lastVal, errPos,enumType);
			if( !pec.IsNull() )
			{
				ecl.AddEnumConstant(pec);
				lastVal = pec->GetConstantValue();
			}


			// если '{', выходим
			if( lxm == '}' )
				break;

			// иначе должен быть ','
			if( lxm != ',' )
				throw lxm;						
		}

		// если конец, выходим
		else if( lxm == '}' )
			break;

		// иначе синтаксическая ошибка
		else
			throw lxm;
	}

	enumType->CompleteCreation(ecl);
}


// конструктор принимает функцию и лексический анализатор и 
// список областей видимости для восстановления. Последий параметр может
// быть равен 0
FunctionParserImpl::FunctionParserImpl( LexicalAnalyzer &la, Function &fn )
		:  lexicalAnalyzer(la) 
{	
	fnBody = fn.IsClassMember() && static_cast<Method &>(fn).IsConstructor() ?
		new ConstructorFunctionBody(fn, la.LastLexem().GetPos()) : 
		new FunctionBody(fn, la.LastLexem().GetPos());

	// проверяем, если у функции уже есть тело, вывести ошибку
	if( fn.IsHaveBody() )
		theApp.Error(la.LastLexem().GetPos(),
			"'%s' - у функции уже есть тело", fn.GetQualifiedName().c_str());
	// задаем тело функции. Тело должно задаваться один раз
	else
		fn.SetFunctionBody();	
	
	// задаем функциональную область видимости
	GetScopeSystem().MakeNewSymbolTable( new FunctionSymbolTable(fn, fn.GetSymbolTableEntry()) );

}


// считать список инициализации конструктора и выполнить необх. проверки
void FunctionParserImpl::ReadContructorInitList( CtorInitListValidator &cilv )
{			
	for( unsigned orderNum = 1;; )
	{
		QualifiedConstructionReader qcr(lexicalAnalyzer, false, true);
		PNodePackage id = qcr.ReadQualifiedConstruction();
		
		// не может быть указателя на член
		if( qcr.IsPointerToMember() )
			throw lexicalAnalyzer.LastLexem();
			
		// создаем идентификатор, прежде достанем функциональную область
		// видимости, т.к. этого требует стандарт		
		SymbolTable *st = &::GetCurrentSymbolTable();
		GetScopeSystem().DestroySymbolTable();
		POperand result = IdentifierOperandMaker(*id).Make();
		GetScopeSystem().MakeNewSymbolTable(st);

		// следующая лексема должна быть '('
		Lexem lxm = lexicalAnalyzer.NextLexem();
		if( lxm != '(' )
			throw lxm;
		
		// считываем список выражений
		PExpressionList initializatorList = new ExpressionList;
		if( lexicalAnalyzer.NextLexem() != ')' )			
		{
		lexicalAnalyzer.BackLexem();
		for( ;; )
		{			
			// считываем очередное выражение
			ExpressionReader er(lexicalAnalyzer, NULL, true);
			er.Read();

			// сохраняем выражение
			initializatorList->push_back( er.GetResultOperand() );

			if( lexicalAnalyzer.LastLexem() == ')' )
				break;

			// проверяем чтобы конструкция была корректной
			if( lexicalAnalyzer.LastLexem() != ',' )				
				throw lexicalAnalyzer.LastLexem();
		}
		}

		// добавляем явный инициализатор 
		cilv.AddInitElement(result, initializatorList,
			lexicalAnalyzer.LastLexem().GetPos(), orderNum);

		// считываем до '{'
		lxm = lexicalAnalyzer.NextLexem();
		if( lxm == '{' )
			break;
		
		else if( lxm != ',' )
			throw lxm;		
	}
}


// разбор тела функции
void FunctionParserImpl::Parse()
{
	register Lexem lxm = lexicalAnalyzer.NextLexem();
	INTERNAL_IF( lxm != '{' && lxm != ':' && lxm != KWTRY );
	bool isTryRootBlock = false;
	// если try-блок, после него может идти список инициализации
	if( lxm == KWTRY )
	{
		lxm = lexicalAnalyzer.NextLexem();
		isTryRootBlock = true;				// устанавливаем флаг, что корневой - try-блок
	}	

	// если имеем тело конструктора, проверяем инициализацию членов
	if( fnBody->IsConstructorBody() )
	{
		CtorInitListValidator cilv( 
			static_cast<const ConstructorMethod &>(fnBody->GetFunction()), 
			lexicalAnalyzer.LastLexem().GetPos() );	

		// считываем список инициализации конструктора
		if( lxm == ':' )			
			ReadContructorInitList(cilv);		// последняя считанная лексема должна быть '{'

		// проверяем инициализацию членов
		cilv.Validate();

		// задаем список инициализации
		static_cast<ConstructorFunctionBody &>(*fnBody).
			SetConstructorInitList(cilv.GetInitElementList());
	}

	if( lexicalAnalyzer.LastLexem() != '{' )
		throw lexicalAnalyzer.LastLexem();

	// задаем корневую конструкцию
	if( isTryRootBlock )
		fnBody->SetBodyConstruction( 
			new TryCatchConstruction(NULL, lexicalAnalyzer.PrevLexem().GetPos()) );
	// иначе составную
	else
		fnBody->SetBodyConstruction( 
			new CompoundConstruction(NULL, lexicalAnalyzer.PrevLexem().GetPos()) );

	// создаем контроллер конструкций
	ConstructionController constructionController(fnBody->GetBodyConstruction());

	// выполняем считывание блока
	StatementParserImpl spi(lexicalAnalyzer, constructionController, *fnBody);
	spi.Parse();

	// после того как тело сфомрировано, выполняем заключительные проверки
	PostBuildingChecks(*fnBody).DoChecks();
}


// считать и создать using конструкцию
void StatementParserImpl::ReadUsingStatement( )
{
	// using namespace 'name' или
	// using 'name'
	try {		
		// использование области видимости
		if( la.NextLexem() == KWNAMESPACE )
		{
			// считываем имя области видимости и передаем его в строитель
			// области видимости
			PNodePackage nspkg = QualifiedConstructionReader(la, 
					false, true).ReadQualifiedConstruction();
					
			MakerUtils::MakeUsingNamespace( &*nspkg );
			if( la.NextLexem() != ';' )
				throw la.LastLexem();		
		}

		// иначе using-декларация
		else
		{
			la.BackLexem();
		
			// считываем имя области видимости и передаем его в строитель
			// области видимости
			PNodePackage nspkg = QualifiedConstructionReader(la).ReadQualifiedConstruction();					
			MakerUtils::MakeUsingNotMember( &*nspkg );
			if( la.NextLexem() != ';' )
				throw la.LastLexem();
		}

	} catch( const Lexem &ep ) {
		SyntaxError(ep);
		IgnoreStreamWhileNotDelim(la);				
	}	
}


// считать for конструкцию
ForConstruction *StatementParserImpl::ReadForConstruction( ConstructionController &cntCtrl )
{
	Position fpos = la.LastLexem().GetPos();
	if( la.NextLexem() != '(' ) 
		throw la.LastLexem();
		
	InstructionList init;
	PInstruction cond = NULL;
	POperand iter = NULL;
	if( la.NextLexem() != ';' )
	{
		// считываем секцию инициализации
		la.BackLexem();
		InstructionParserImpl ipl(la, init);
		ipl.Parse();
		init = ipl.GetInstructionList();
	}

	if( la.NextLexem() != ';' )
	{
		la.BackLexem();
		cond = ReadCondition();
		if( la.LastLexem() != ';' )
			throw la.LastLexem();
	}

	if( la.NextLexem() != ')' )
	{
		la.BackLexem();
		iter = ReadExpression();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();
	}


	return BodyMakerUtils::ForConstructionMaker( ForInitSection(init), cond, iter,  
		controller.GetCurrentConstruction(), fpos) ;
}


// считать выражение для конструкций, вернуть результат
POperand StatementParserImpl::ReadExpression( bool noComa )
{
	ExpressionReader er(la, NULL, noComa);
	er.Read();
	return er.GetResultOperand();		
}

	
// считать инструкцию. Инструкцией является декларация с инициализацией
// или выражение. Используется в if, switch, while, for
PInstruction StatementParserImpl::ReadCondition( )
{
	TypeExpressionReader ter(la);
	ter.Read( false, false );
	Position insPos = la.LastLexem().GetPos();

	// проверяем пакет,
	INTERNAL_IF( ter.GetResultPackage() == NULL );	

	// если имеем выражение, значит строим выражение
	if( ter.GetResultPackage()->IsExpressionPackage() )			
	{
		POperand exp = static_cast<const ExpressionPackage &>(
			*ter.GetResultPackage()).GetExpression();
		delete ter.GetResultPackage();
		return new ExpressionInstruction(exp, insPos);
	}

	// иначе имеем декларацию, следует считать инициализатор
	else
	{
		if( la.LastLexem() != '=' )
			throw la.LastLexem();

		// если инициализатор задан в скобках, это ошибка
		if( !ter.GetInitializatorList().IsNull() )
			theApp.Error(la.LastLexem().GetPos(), "инициализатор уже задан");
		POperand iator = ReadExpression();

		// строим инструкцию
		PInstruction cond = BodyMakerUtils::MakeCondition( 
			static_cast<const NodePackage &>(*ter.GetResultPackage()), iator, insPos);
		delete ter.GetResultPackage();
		return cond;
	}
}


// считать catch-декларацию
PTypyziedEntity StatementParserImpl::ReadCatchDeclaration( )
{
	if( la.NextLexem() != '(' )
		throw la.LastLexem();
	
	// считываем тип, если не считано '...'
	if( la.NextLexem() == ELLIPSES )
	{
		// след. лексема должна быть ')'
		if( la.NextLexem() != ')' )
			throw la.LastLexem();
		return NULL;
	}

	else
	{
		Position dpos = la.LastLexem().GetPos();

		// считываем декларацию
		la.BackLexem();
		DeclaratorReader dr( DV_CATCH_DECLARATION, la, true );
		dr.ReadTypeSpecifierList();		
		PNodePackage typeLst = dr.GetTypeSpecPackage();
	
		// если не было считано типа, ошибка
		if( typeLst->IsNoChildPackages() )
			throw la.LastLexem();
		
		// считываем декларатор
		PNodePackage decl = dr.ReadNextDeclarator();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();

		// строим декларацию, возвращаем
		return CatchDeclarationMaker(*typeLst, *decl, dpos).Make();
	}	
}


// считать список catch-обработчиков
void StatementParserImpl::ReadCatchList()
{	
	for( ;; )
	{
		if( la.NextLexem() != KWCATCH )
		{
			la.BackLexem();
			break;
		}
		
		Position cpos = la.LastLexem().GetPos();
		// создаем область видимости для catch-блока		
		MakeLST();

		// считываем catch-декларацию
		PTypyziedEntity catchObj = ReadCatchDeclaration();
		if( la.NextLexem() != '{' )
			throw la.LastLexem();

		// строим catch-конструкцию
		CatchConstruction *cc = CatchConstructionMaker(
			catchObj, controller.GetCurrentConstruction(), cpos).Make();
		PBodyComponent pCatch = cc;

		// считываем catch-блок, сохраняем его в catch-конструкции
		cc->AddChildComponent( ParseBlock() );

		// восстанавливаем область видимости и текущую конструкцию
		GetScopeSystem().DestroySymbolTable();
		controller.SetCurrentConstruction( cc->GetParentConstruction() );
		pCatch.Release();
	}
}


// считать блок 
CompoundConstruction *StatementParserImpl::ParseBlock( )
{
	INTERNAL_IF( la.LastLexem() != '{' );

	// сохраняем текущую ОВ и текущую конструкицю, 
	// для того чтобы в случае ошибки восстановить их
	const SymbolTable &cur = GetCurrentSymbolTable();
	Construction &parent = controller.GetCurrentConstruction();

	// создаем составну. конструкцию, делаем ее текущей
	CompoundConstruction *compound = 
		BodyMakerUtils::SimpleConstructionMaker<CompoundConstruction>(
			parent, la.LastLexem().GetPos() );
	controller.SetCurrentConstruction(compound);
	while( la.NextLexem() != '}' )
	{
		la.BackLexem();

		// при считывании компонента могут возникать 
		// синтаксические ошибки
		try {
			compound->AddChildComponent( ParseComponent() );
		} catch( const Lexem &lxm ) {
			// восстановим текущую конструкцию, восстановим область видимости
			controller.SetCurrentConstruction(compound);
			while( &GetCurrentSymbolTable() != &cur )
			{
				GetScopeSystem().DestroySymbolTable();
				INTERNAL_IF( GetCurrentSymbolTable().IsGlobalSymbolTable() );
			}

			SyntaxError(lxm);
			IgnoreStreamWhileNotDelim(la);	
		}
	}
	
	// восстанавливаем родительскую конструкцию
	controller.SetCurrentConstruction(&parent);
	return compound;
}


// считать компонент функции
BodyComponent *StatementParserImpl::ParseComponent( )
{	
	using namespace BodyMakerUtils;

	// контролируем переполнение стека
	OverflowStackController osc(la);
	PBodyComponent pComp = NULL;		// компонент, который считывается

	// считываем следующую лексему
	register int lcode = la.NextLexem();
	Position compPos = la.LastLexem().GetPos();

	// пустая инструкция
	if( lcode == ';' )
		return SimpleComponentMaker<EmptyInstruction>(compPos);	

	// using-декларация или using-namespace
	else if( lcode == KWUSING )
	{
		ReadUsingStatement();

		// создаем пустую инструкцию в качестве замены using-декларации		
		return SimpleComponentMaker<EmptyInstruction>(compPos);
	}

	// namespace-алиас
	else if( lcode == KWNAMESPACE )	
	{
		// считываем имя области видимости и передаем его в строитель
		// области видимости
		PNodePackage nspkg = QualifiedConstructionReader(
			la, true, true).ReadQualifiedConstruction();
					
		if( la.NextLexem() != '=' )
			throw la.LastLexem();
					
		// иначе считываем синоним области видимости		
		MakerUtils::MakeNamespaceAlias( &*nspkg, &*QualifiedConstructionReader(
				la, false, true).ReadQualifiedConstruction());

		if( la.NextLexem() != ';' )
			throw la.LastLexem();	

		// создаем пустую инструкцию в качестве замены namespace-декларации		
		return SimpleComponentMaker<EmptyInstruction>(compPos);
	}
	
	// case выражение:	компонент
	else if( lcode == KWCASE )
	{
		POperand exp = ReadExpression( true );
		if( la.LastLexem() != ':' )
			throw la.LastLexem();

		// считываем опять компонент
		BodyComponent &bc = *ParseComponent( );
		return CaseLabelMaker(exp, bc, controller.GetCurrentConstruction(), compPos);
	}

	// default: компонент
	else if( lcode == KWDEFAULT )
	{
		if( la.NextLexem() != ':' )
			throw la.LastLexem();
		BodyComponent &bc = *ParseComponent( );
		return DefaultLabelMaker(bc, controller.GetCurrentConstruction(), compPos);
	}

	// asm ( строка ) ;
	else if( lcode == KWASM )
	{
		if( la.NextLexem() != '(' ) 
			throw la.LastLexem();
		Lexem lxm = la.NextLexem();			// сохраняем строковый литерал
		if( lxm != STRING || la.NextLexem() != ')' )
			throw la.LastLexem();
		if( la.NextLexem() != ';' ) 
			throw la.LastLexem();
		
		return AsmOperationMaker(lxm.GetBuf(), compPos);
	}

	// return выражение? ;
	else if( lcode == KWRETURN )
	{
		POperand exp = NULL;
		if( la.NextLexem() == ';' )
			;
		else
		{
			la.BackLexem();
			exp = ReadExpression();
			if( la.LastLexem() != ';' )
				throw la.LastLexem() ;
		}

		return ReturnOperationMaker(exp, fnBody.GetFunction(), compPos);
	}
	
	// break;
	else if( lcode == KWBREAK )
	{
		if( la.NextLexem() != ';' )
			throw la.LastLexem();
		return BreakOperationMaker( controller.GetCurrentConstruction(), compPos);			
	}

	// continue;
	else if( lcode == KWCONTINUE )
	{
		if( la.NextLexem() != ';' )
			throw la.LastLexem();
		return ContinueOperationMaker( controller.GetCurrentConstruction(), compPos);
	}
	
	// goto метка;
	else if( lcode == KWGOTO )
	{
		Lexem lxm = la.NextLexem();
		if( lxm != NAME )
			throw la.LastLexem();
		if( la.NextLexem() != ';' )
			throw la.LastLexem();
		return GotoOperationMaker( lxm.GetBuf(), fnBody, compPos );
	}
	
	// do компонент while( выражение ) ;
	else if( lcode == KWDO )
	{
		// создаем область видимости для do
		MakeLST();

		// создаем do
		DoWhileConstruction *dwc = SimpleConstructionMaker<DoWhileConstruction>(
			controller.GetCurrentConstruction(), compPos);
		pComp = dwc;

		// задаем как текущую
		controller.SetCurrentConstruction(dwc);

		// считать компонент
		PBodyComponent doChild = ParseComponent();
		if( la.NextLexem() != KWWHILE )
			throw la.LastLexem();
		if( la.NextLexem() != '(' )
			throw la.LastLexem();
		POperand doExpr = ReadExpression();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();
		if( la.NextLexem() != ';' )
			throw la.LastLexem();

		// задаем выражение do-конструкции, проверяем выражение, 
		// присоединяем дочерний компонент, восстанавливаем родительскую конструкцию, 
		// восстанавливаем ОВ		
		dwc->SetCondition(doExpr);
		ValidCondition(doExpr, "do", compPos);
		dwc->AddChildComponent( doChild.Release() );
		controller.SetCurrentConstruction( dwc->GetParentConstruction() );
		GetScopeSystem().DestroySymbolTable();
		return pComp.Release();		// возвращаем do-конструкцию

	}

	// for( инициализация? ;  сравнение? ; выражение? ) компонент
	else if( lcode == KWFOR )
	{
		// создаем область видимости для конструкции
		MakeLST();

		// считываем и создаем саму конструкцию
		ForConstruction *fc = ReadForConstruction(controller);
		pComp = fc;
		// задаем как текущую
		controller.SetCurrentConstruction(fc);
		BodyComponent *forChild = ParseComponent();

		// присоединяем дочерний компонент, восстанавливаем родительскую конструкцию, 
		// восстанавливаем ОВ		
		fc->AddChildComponent( forChild );
		controller.SetCurrentConstruction( fc->GetParentConstruction() );
		GetScopeSystem().DestroySymbolTable();

		// вернуть for-конструкцию
		return pComp.Release();
	}

	// while( сравнение ) компонент
	else if( lcode == KWWHILE )
	{
		// создаем область видимости для конструкции
		MakeLST();	
		if( la.NextLexem() != '(' )
			throw la.LastLexem();
		PInstruction whileCond = ReadCondition();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();

		// создаем while-конструкцию, проверяем корректность выражения
		WhileConstruction *wc = ConditionConstructionMaker<WhileConstruction>(whileCond, 
			controller.GetCurrentConstruction(), compPos);
		ValidCondition(whileCond, "while");
		pComp = wc;
		// задаем как текущую
		controller.SetCurrentConstruction(wc);
		BodyComponent *whileChild = ParseComponent();

		// присоединяем дочерний компонент, восстанавливаем родительскую конструкцию, 
		// восстанавливаем ОВ		
		wc->AddChildComponent( whileChild );
		controller.SetCurrentConstruction( wc->GetParentConstruction() );
		GetScopeSystem().DestroySymbolTable();

		// вернуть while-конструкцию
		return pComp.Release();
	}

	// switch( сравнение ) компонент
	else if( lcode == KWSWITCH )
	{	
		// создаем область видимости для конструкции
		MakeLST();
		if( la.NextLexem() != '(' )
			throw la.LastLexem();
		PInstruction switchCond = ReadCondition();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();
		
		// создаем swicth-конструкцию, проверяем корректность выражения
		SwitchConstruction *sc = ConditionConstructionMaker<SwitchConstruction>(switchCond, 
			controller.GetCurrentConstruction(), compPos);
		ValidCondition(switchCond, "switch", true);
		pComp = sc;
		// задаем как текущую
		controller.SetCurrentConstruction(sc);
		BodyComponent *switchChild = ParseComponent();	

		// присоединяем дочерний компонент, восстанавливаем родительскую конструкцию, 
		// восстанавливаем ОВ		
		sc->AddChildComponent( switchChild );
		controller.SetCurrentConstruction( sc->GetParentConstruction() );
		GetScopeSystem().DestroySymbolTable();

		// вернуть swicth-конструкцию
		return pComp.Release();
	}

	// if( сравнение ) компонент [else компонент]?
	else if( lcode == KWIF )
	{
		// создаем область видимости для конструкции
		MakeLST();	
		if( la.NextLexem() != '(' )
			throw la.LastLexem();
		PInstruction ifCond = ReadCondition();
		if( la.LastLexem() != ')' )
			throw la.LastLexem();

		// создаем if-конструкцию, проверяем корректность выражения
		IfConstruction *ic = ConditionConstructionMaker<IfConstruction>(ifCond, 
			controller.GetCurrentConstruction(), compPos);
		ValidCondition(ifCond, "if");
		pComp = ic;
		// задаем как текущую
		controller.SetCurrentConstruction(ic);
		BodyComponent *ifChild = ParseComponent();			
	
		// присоединяем дочерний компонент
		ic->AddChildComponent( ifChild );
		
		// если есть else
		if( la.NextLexem() == KWELSE )
		{
			ElseConstruction *ec = SimpleConstructionMaker<ElseConstruction>(
					const_cast<Construction&>(*ic->GetParentConstruction()), compPos);
			PBodyComponent pElse = ec;
			// задаем как текущую
			controller.SetCurrentConstruction(ic);
			BodyComponent *elseChild = ParseComponent();

			// присоединяем дочерний компонент, присоединяем к if
			ec->AddChildComponent(elseChild);
			ic->SetElseConstruction( ec );

			// освобождаем интеллектуальный указатель который хранит else
			pElse.Release();				
		}
		
		else
			la.BackLexem();

		// восстанавливаем родительскую конструкцию, восстанавливаем ОВ				
		controller.SetCurrentConstruction( ic->GetParentConstruction() );
		GetScopeSystem().DestroySymbolTable();

		// вернуть if-конструкцию
		return pComp.Release();
	}

	// try блок список-обработчиков
	else if( lcode == KWTRY )
	{
		if( la.NextLexem() != '{' )
			throw la.LastLexem();

		// создаем try конструкцию
		TryCatchConstruction *tcc = SimpleConstructionMaker<TryCatchConstruction>(
					controller.GetCurrentConstruction(), compPos);
		pComp = tcc;

		// задаем как текущую, считываем блок, добавляем блок
		controller.SetCurrentConstruction(tcc);
		BodyComponent *block = ParseBlock();		
		tcc->AddChildComponent(block);

		if( la.NextLexem() != KWCATCH )		
			theApp.Error(la.LastLexem().GetPos(), "try без обработчиков");
		else	
		{
			la.BackLexem();
			ReadCatchList();
		}

		// восстанавливаем родительскую конструкцию, возвращаем try-блок
		controller.SetCurrentConstruction( tcc->GetParentConstruction() );
		return pComp.Release();
	}

	// блок
	else if( lcode == '{' )
	{
		bool isMade = BodyMakerUtils::MakeLocalSymbolTable( controller.GetCurrentConstruction() );
		CompoundConstruction *cc = ParseBlock();
		if( isMade )
			GetScopeSystem().DestroySymbolTable();
		return cc;
	}

	// если '}', аварийное завршение компиляции
	else if( lcode == '}' )
		theApp.Fatal( la.LastLexem().GetPos(), "аварийное завершение компиляции перед '}'" );

	// конец файла
	else if( lcode == EOF )
		theApp.Fatal( la.LastLexem().GetPos(), "неожиданный конец файла" );

	// инструкция: выражение, декларация, декларация класса, метка
	else
	{
		la.BackLexem();
		InstructionList insList;		// выходной список инструкций
		InstructionParserImpl ipl(la, insList, true);

		// можем перехватить метку
		try {
			ipl.Parse();
			return InstructionListMaker( ipl.GetInstructionList(), compPos );
		} catch( const LabelLexem &labLxm ) {
			// создаем метку
			Label label( labLxm.GetBuf(), 
				const_cast<FunctionSymbolTable *>(GetScopeSystem().GetFunctionSymbolTable()), 
				labLxm.GetPos() );

			// перехватили метку, считываем компонент
			return SimpleLabelMaker(label, *ParseComponent(), fnBody, compPos);
		}
	}

	// всегда ошибка, если доходим до сюда
	INTERNAL("'StatementParserImpl::ParseComponent' - ошибка в строительстве компонента");
	return NULL;
}

// считать блок или try-блок, в зависимости от корневой конструкции
void StatementParserImpl::Parse() {
	// значит считываем блок, а за ним catch-обработчики
	if( controller.GetCurrentConstruction().GetConstructionID() == Construction::CC_TRY )
	{
		TryCatchConstruction &tcc = static_cast<TryCatchConstruction &>(
			controller.GetCurrentConstruction());

		// считываем блок
		BodyComponent *block = ParseBlock();		
		tcc.AddChildComponent(block);

		if( la.NextLexem() != KWCATCH )		
			theApp.Error(la.LastLexem().GetPos(), "try без обработчиков");
		else	
		{
			la.BackLexem();
			try {
				ReadCatchList();
			} catch( const Lexem &lxm ) {
				theApp.Fatal(lxm.GetPos(), "аварийное завершение компиляции перед '%s'",
					lxm.GetBuf().c_str());
			}
		}		
	}

	// иначе считываем просто блок
	else
		ParseBlock();
}


// вывести синтаксическую ошибку обнаруженную рядом с лексемой 'lxm'
void ParserUtils::SyntaxError( const Lexem &lxm )
{
	theApp.Error(
		lxm.GetPos(), "синтаксическая ошибка перед '%s'", lxm.GetBuf().c_str());
}


// функция считывает лексемы до тех пор пока не появится ';'
void ParserUtils::IgnoreStreamWhileNotDelim( LexicalAnalyzer &la )
{
	Lexem lxm;
	for( ;; )
	{
		lxm = la.NextLexem();
		if( lxm == EOF )
			theApp.Fatal( lxm.GetPos(), "неожиданный конец файла" );

		else if( lxm == '{' || lxm == '}' )
			theApp.Fatal( lxm.GetPos(), "аварийное завершение компиялции перед '%c'",
			(char)lxm);

		else if( lxm == ';' )
			break;
	}
}


// получить позицию пакета
Position ParserUtils::GetPackagePosition( const Package *pkg )
{
	INTERNAL_IF( pkg == NULL );
	for( ;; )
	{
		if( pkg->IsLexemPackage() )
			return ((LexemPackage *)pkg)->GetLexem().GetPos();

		else if( pkg->IsNodePackage() )
		{
			NodePackage *np = (NodePackage *)pkg;
			if( np->IsNoChildPackages() )
				INTERNAL( "'ParserUtils::GetPackagePosition' - нет дочерних пакетов" );
			else
				pkg = np->GetChildPackage(0);
		}

		else
			INTERNAL( "'ParserUtils::GetPackagePosition' - неизвестный тип пакета" );
	}

	return Position();
}


// распечатать все дерево пакетов, вернуть буфер
CharString ParserUtils::PrintPackageTree( const NodePackage *np )
{
	string outbuf;

	INTERNAL_IF( np == NULL );
	for( int i = 0; i<np->GetChildPackageCount(); i++ )
	{
		const Package *pkg = np->GetChildPackage(i);
		if( pkg->IsLexemPackage() )
			outbuf += ((LexemPackage *)pkg)->GetLexem().GetBuf().c_str();

		else if( pkg->IsNodePackage() )
			outbuf += PrintPackageTree( (NodePackage *)pkg ).c_str();

		else if( pkg->IsExpressionPackage() )
			;
		
		else
			INTERNAL( "'ParserUtils::GetPackagePosition' - неизвестный тип пакета" );
	}

	return outbuf.c_str();
}


// проверяет, если входной пакет и лекс. находится в состоянии
// декларации типа (класс, перечисление), считать тип, выполнить
// декларацию вернуть. Если не требуется считывать деклараторы, вернуть true, иначе false
bool ParserUtils::LocalTypeDeclaration( 
		LexicalAnalyzer &lexicalAnalyzer, NodePackage *pkg, const BaseType *&outbt )
{
	// в этом месте может быть определение класса, если последние
	// два пакета были ключ класса и PC_QUALIFIED_NAME, а последняя
	// считанная лексема - '{' или ':'
	if( NeedClassParserImpl(lexicalAnalyzer.LastLexem(), pkg) )
	{
		ClassParserImpl cpi(lexicalAnalyzer, pkg, ClassMember::NOT_CLASS_MEMBER);
		cpi.Parse();
		outbt = &cpi.GetClassType();

		if( lexicalAnalyzer.NextLexem() == ';' )
			return true;			
		else
			lexicalAnalyzer.BackLexem();

	}

	// иначе если требуется считать перечисление, считываем
	else if( NeedEnumReading(lexicalAnalyzer.LastLexem(), pkg) )
	{
		EnumParserImpl epi(lexicalAnalyzer, pkg, ClassMember::NOT_CLASS_MEMBER);
		epi.Parse();
		outbt = &epi.GetEnumType();

		if( lexicalAnalyzer.NextLexem() == ';' )
			return true;
		else
			lexicalAnalyzer.BackLexem();
	}

	// иначе если объявление класса или перечисления
	else if( pkg->GetChildPackageCount() == 2 && 
			 pkg->GetChildPackage(1)->GetPackageID() == PC_QUALIFIED_NAME && 
			 lexicalAnalyzer.LastLexem() == ';' )
	{
		register int pc = pkg->GetChildPackage(0)->GetPackageID() ;
		lexicalAnalyzer.NextLexem();

		if( pc == KWCLASS || pc == KWSTRUCT || pc == KWUNION )
		{
			ClassTypeMaker ctm( pkg, ClassMember::NOT_CLASS_MEMBER );
			outbt = ctm.Make();
		}	

		else if( pc == KWENUM )
		{
			EnumTypeMaker etm( pkg,  ClassMember::NOT_CLASS_MEMBER );
			outbt = etm.Make();				
		}

		else
			return false;
		return true;
	}
	
	return false;
}


// проверяет по идентификатору и последней считанной лексеме, необходим
// ли функции разбор тела
inline static bool NeedFunctionParserImpl( LexicalAnalyzer &la, const Identifier *id )
{
	const Function *fn = dynamic_cast<const Function *>(id);
	register Lexem lastLxm = la.LastLexem();
	
	if( (lastLxm == '{' ||
		(dynamic_cast<const ConstructorMethod *>(fn) &&
		 (lastLxm == ':' || lastLxm == KWTRY)) ) && fn != NULL )
		return true;	

	return false;		
}


// проверяет по пакету, и последней считанной лексеме, возможно
// ли определение класса
inline static bool NeedClassParserImpl( const Lexem &lastLxm, const NodePackage *np ) 
{
	if( np->IsNoChildPackages() )
		return false;

	int lid = np->GetLastChildPackage()->GetPackageID();
	if( lid == KWCLASS || lid == KWSTRUCT || lid == KWUNION )	// безимянное объявление
	{
		if( lastLxm == '{' || 
			lastLxm == ':' )
			return true;
		return false;
	}

	if( lid != PC_QUALIFIED_NAME || np->GetChildPackageCount() < 2 )
		return false;

	int preid = np->GetChildPackage(np->GetChildPackageCount()-2)->GetPackageID(),
		lcode = lastLxm;

	return (preid == KWCLASS || preid == KWSTRUCT || preid == KWUNION) &&
			(lcode == '{' || lcode == ':') ;
}


// проверяет по пакету, и последней считанной лексеме, требуется ли 
// определение перечисления
inline static bool NeedEnumReading( const Lexem &lastLxm, const NodePackage *np ) 
{
	if( np->IsNoChildPackages() )
		return false;

	int lid = np->GetLastChildPackage()->GetPackageID();
	if( lid == KWENUM )	// безимянное объявление
		return lastLxm == '{';

	if( lid != PC_QUALIFIED_NAME || np->GetChildPackageCount() < 2 )
		return false;

	int preid = np->GetChildPackage(np->GetChildPackageCount()-2)->GetPackageID(),
		lcode = lastLxm;

	return (preid == KWENUM && lcode == '{');
}


// считать список инициализации и выполнить проверку
inline static ListInitComponent *ReadInitList( LexicalAnalyzer &la, 
			PDeclarationMaker &pdm, const Position &errPos )
{
	// считываем
	InitializationListReader ilr(la);
	ilr.Read();
	INTERNAL_IF( ilr.GetListInitComponent() == NULL );

	// проверяем, чтобы инициализатор был объектом
	
	if( pdm.IsNull() || pdm->GetIdentifier() == NULL )
		return const_cast<ListInitComponent *>(ilr.GetListInitComponent());
	   
	else if( const Object *ob = dynamic_cast<const ::Object *>(pdm->GetIdentifier()) )
	{
		ListInitializationValidator( *ilr.GetListInitComponent(), 
			errPos, const_cast<::Object &>(*ob) ).Validate();

		// вернуть список для генерации
		return const_cast<ListInitComponent *>(ilr.GetListInitComponent());
	}

	// иначе проверяем инициализацию списком
	else
	{
		theApp.Error(errPos, "инициализация списком не-объекта");
		return const_cast<ListInitComponent *>(ilr.GetListInitComponent());
	}
}
