// реализация вспомогательных классов парсера - Reader.cpp


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
#include "Parser.h"
#include "Body.h"
#include "Reader.h"
#include "ExpressionMaker.h"
#include "Maker.h"
#include "MemberMaker.h"
#include "Coordinator.h"


// счетчик стека
int ExpressionReader::StackOverflowCatcher::stackDeep = 0;


// получить следующую лексему, сохранить ее в контейнере
const Lexem &DeclaratorReader::NextLexem()
{
	// если контейнер отката не пуст, вернуть лексему ИЗ него
	if( canUndo )
	{
		if( !undoContainer.empty() )
		{
			lastLxm = undoContainer.front();
			lexemContainer.push_back(lastLxm);
			undoContainer.pop_front();						
			return lastLxm;
		}

		else
			canUndo = false;
	}

	lexemContainer.push_back(lexicalAnalyzer.NextLexem());
	return lastLxm = lexemContainer.back();
}


// вернуть предыдущую лексему в поток
void DeclaratorReader::BackLexem()
{
	// если контейнер отката не пуст, вернуть лексему В него
	if( canUndo )	
	{
		lexemContainer.pop_back();
		undoContainer.push_front(lastLxm);
	}

	else
	{
		lexicalAnalyzer.BackLexem();
		lexemContainer.pop_back();
	}
}


// получить следующую лексему, сохранить ее в контейнере
const Lexem &QualifiedConstructionReader::NextLexem()
{
	// если контейнер отката не пуст, вернуть лексему ИЗ него
	if( canUndo )
	{
		if( !undoContainer->empty() )
		{
			lastLxm = undoContainer->front();
			lexemContainer.push_back(lastLxm);
			undoContainer->pop_front();						
			return lastLxm;
		}

		else
			canUndo = false;
	}

	lexemContainer.push_back(lexicalAnalyzer.NextLexem());
	return lastLxm = lexemContainer.back();
}


// вернуть предыдущую лексему в поток
void QualifiedConstructionReader::BackLexem()
{
	// если контейнер отката не пуст, вернуть лексему В него
	if( canUndo )	
	{
		lexemContainer.pop_back();
		undoContainer->push_front(lastLxm);
	}

	else
	{
		lexicalAnalyzer.BackLexem();
		lexemContainer.pop_back();
	}
}


// считать спецификаторы типа
void DeclaratorReader::ReadTypeSpecifierList()
{
	bool hasBaseType = false;
	Lexem lxm;

	for( ;; )
	{
		lxm = NextLexem();		

		// любой из спецификаторов: базовый тип, cv-квалификатор, 
		// спецификатор хранения, модификатор размера, модификатор знака,
		// спецификаторы функций (inline, virtual, explicit), 
		// спецификатор friend
		if( lxm == KWAUTO	  || lxm == KWBOOL		|| lxm == KWCHAR		||
			lxm == KWCONST	  || lxm == KWDOUBLE	|| lxm == KWEXPLICIT	||
			lxm == KWEXTERN   || lxm == KWFLOAT		|| lxm == KWFRIEND		||
			lxm == KWINLINE   || lxm == KWINT		|| lxm == KWLONG		||
			lxm == KWMUTABLE  || lxm == KWREGISTER 	|| lxm == KWSHORT		||
			lxm == KWSIGNED   || lxm == KWSTATIC	|| lxm == KWTYPEDEF		||
			lxm == KWUNSIGNED || lxm == KWVIRTUAL	||
			lxm == KWVOID	  || lxm == KWVOLATILE	|| lxm == KWWCHAR_T	 )		
		{
			tslPkg->AddChildPackage( new LexemPackage(lxm) );
			if(lxm == KWBOOL || lxm == KWCHAR || lxm == KWDOUBLE ||
				lxm == KWFLOAT || lxm == KWINT || lxm == KWVOID || lxm == KWWCHAR_T ||
				lxm == KWSHORT || lxm == KWLONG || lxm == KWUNSIGNED || lxm == KWSIGNED ) 
				hasBaseType = true;
		}
	 
		// спецификаторы класса, могут содержать имя после себя
		else if( lxm == KWCLASS || lxm == KWSTRUCT ||
				 lxm == KWENUM  || lxm == KWUNION )
		{
			if( hasBaseType )
				break;

			tslPkg->AddChildPackage( new LexemPackage(lxm) );

			lxm = NextLexem();
			BackLexem();
			if( lxm == '{' || lxm == ':' )		// имеем безимянный класс
				return;
			
			QualifiedConstructionReader qcr(lexicalAnalyzer, false, true);
			PNodePackage np = qcr.ReadQualifiedConstruction();	// считываем имя класса

			if( qcr.IsPointerToMember() )
				throw qcr.GetLexemContainer().back();
			tslPkg->AddChildPackage( np.Release() );
			hasBaseType = true;
		}	

		// иначе если имя, идентификатор может быть типом
		else if( lxm == NAME || lxm == COLON_COLON )
		{
			// если базовый тип уже считан - значит имеем дело с идентификатором,
			// и следует вернуть лексему и выйти
			if( hasBaseType )			
				break;			

			QualifiedConstructionReader qcr(lexicalAnalyzer,
				false, false, PLexemContainer(NULL), canBeExpression);
			if( canUndo )
			{
				BackLexem();
				qcr.LoadToUndoContainer(undoContainer);
				undoContainer.clear();
				canUndo = false;
			}

			else
				BackLexem();
			PNodePackage np = qcr.ReadQualifiedConstruction();

			// если было считано выражение, сохраняем считанные лексемы
			// в контейнере, и генерируем синтаксическую ошибку
			if( qcr.IsExpression() )
			{
				INTERNAL_IF( !canBeExpression );				
				LoadToLexemContainer(qcr.GetLexemContainer());
				throw qcr.GetLexemContainer().back();
			}

			// был считан не тип, сохраняем все считанные лексемы 
			// в контейнере отката, для считывания идентификатора
			// методом ReadDeclarator
			if( qcr.IsPointerToMember() || !qcr.IsTypeName() )
			{
				LoadToUndoContainer(qcr.GetLexemContainer());						
				return;
			}

			// сохраним считанные лексемы в контейнере			
			LoadToLexemContainer(qcr.GetLexemContainer());
			LoadToUndoContainer(qcr.GetUndoContainer());
			np->SetPackageID(PC_QUALIFIED_TYPENAME);
			tslPkg->AddChildPackage( np.Release() );
			hasBaseType = true;
		}

		// может быть строковый литерал, в этом случае можно проверить,
		// была ли предыдущая лексема extern, и если да выполнить соотв. действия
		else if( lxm == STRING && lexicalAnalyzer.PrevLexem() == KWEXTERN )
		{
			if( !(::GetCurrentSymbolTable().IsGlobalSymbolTable() ||
				  ::GetCurrentSymbolTable().IsNamespaceSymbolTable()) )
			{
				theApp.Error(lxm.GetPos(), 
					"спецификация связывания может задаваться только глобально");
				continue;
			}

			tslPkg->AddChildPackage( new LexemPackage(lxm) );
			if( NextLexem() == '{' && tslPkg->GetChildPackageCount() == 2 )
			{
				const_cast<Parser&>(theApp.GetTranslationUnit().GetParser()).
					SetLinkSpecification(Parser::LS_C);
				return;
			}

			else
				BackLexem();
		}

		else
			break;
	}

	// вернуть последнюю считанную лексему
	BackLexem();	
}


// считать следующий декларатор, используется в случае, когда в
// декларации может быть несколько деклараторов. Также возбуждает
// исключительную ситуацию если была синтаксическая ошибка
PNodePackage DeclaratorReader::ReadNextDeclarator()
{
	declPkg = new NodePackage(PC_DECLARATOR);
	crampLevel = 0;
	nameRead = false;
	initializatorList = NULL;		// очищаем список инициализаторов
	ReadDeclarator();
	return declPkg;
}


// считать декларатор, закрытая функция которая выполняет всю работу
// по считке, ReadNextDeclarator - вызывает ее. Перед вызовом
// объект declPkg - должен создаваться заново
void DeclaratorReader::ReadDeclarator( )
{
	OverflowController oc(MAX_PARSER_STACK_DEEP);		// контроль переполнения	

	Lexem lxm = NextLexem();
	if( lxm == '*' )
	{	
		PNodePackage p = new NodePackage(0);		
		ReadCVQualifierSequence(p);

		ReadDeclarator();
		declPkg->AddChildPackage(new LexemPackage(lxm));

		if( !p->IsNoChildPackages()  )
		{
			Lexem cv1 = ((LexemPackage *)p->GetChildPackage(0))->GetLexem();

			if( p->GetChildPackageCount() > 1 )
			{
				Lexem cv2 = ((LexemPackage *)p->GetChildPackage(1))->GetLexem();
				declPkg->AddChildPackage( new LexemPackage(cv1) );
				declPkg->AddChildPackage( new LexemPackage(cv2) );
			}

			else
				declPkg->AddChildPackage( new LexemPackage(cv1) );									
		}
	}

	else if( lxm == '&' )
	{
		ReadDeclarator();
		declPkg->AddChildPackage(  new LexemPackage(lxm) );
	}

	// в этом месте расположена одна прикольная замануха. При считывании
	// производного типа, существует неоднозначность между 'выражение в скобках'
	// и 'прототипом функции'. Согласно стандарту, предпочтение отдается прототипу
	// функции, но только в случае если в скобках список параметров или пустой список.
	else if( lxm == '(' )			
	{				
		if( declType == DV_CAST_OPERATOR_DECLARATION )
		{
			BackLexem();
			return;
		}

		// контейнер должен быть пустой, чтобы мы могли с его помощью разрешить
		// неоднозначность
		INTERNAL_IF( !undoContainer.empty() );		
		
		DeclaratorReader dr(DV_PARAMETR, lexicalAnalyzer);
		dr.ReadTypeSpecifierList();
		PNodePackage np = dr.GetTypeSpecPackage(); 
				
		// был считан тип, значит загружаем его в контейнер отката
		// и считываем прототип функции
		if( !np->IsNoChildPackages() || dr.lastLxm == ELLIPSES )
		{
			// считываем последнюю лексему, чтобы предотвратить утечку
			dr.NextLexem();
			// объединяем два контейнера
			dr.lexemContainer.insert(dr.lexemContainer.end(), dr.undoContainer.begin(),
				dr.undoContainer.end());
			// из undo-контейнера, теперь будет считываться параметр
			LoadToUndoContainer(dr.GetLexemContainer());	
			lastLxm = lxm;
		}

		// иначе если был считан ')', значит имеем дело с пустым список параметров
		else if( dr.lastLxm == ')' )
		{
			lastLxm = lxm;
		}

		// если ничего не считано, считываем декларатор
		else
		{		
			// в этой точке , dr мог считать только имя параметра, либо
			// указатель на член, все эти лексемы хранятся в undo-контейнере		
			LoadToUndoContainer(dr.undoContainer);	
			lastLxm = lxm;

			crampLevel++;
			ReadDeclarator();
			crampLevel--;
			if( lastLxm != ')' )
				throw lastLxm;
			NextLexem();
		}
	}


	// это может быть: указатель на член, или идентификатор,
	// который мы и объявляем	
	else if( lxm == NAME || lxm == COLON_COLON )
	{
		bool nq = false, ns = false;
		if( declType == DV_PARAMETR || declType == DV_CAST_OPERATION )
			nq = ns = true;

		// только в локальной декларации. Для using-декларации внутри класса
		// требуется квалифицированное имя
		else if( declType == DV_LOCAL_DECLARATION )
			nq = true;
			
		BackLexem();
		QualifiedConstructionReader qcr(lexicalAnalyzer, nq, ns);
		qcr.LoadToUndoContainer( undoContainer );	// помещаем имя в контейнер
		undoContainer.clear();
		PNodePackage np = qcr.ReadQualifiedConstruction();
		LoadToLexemContainer(qcr.GetLexemContainer());


		// если указатель на член
		if( qcr.IsPointerToMember() )
		{			
			ReadCVQualifierSequence(np);		// считываем cv-квалификаторы
			ReadDeclarator();
		
			declPkg->AddChildPackage( np.Release() );		// добавляем указатель к списку произ. типов			
		}

		// иначе имеем дело с идентификатором, добавляем его к пакетам
		else 
		{
			// проверяем, можно ли использовать специальные функции
			// в заданном контексте
			if( declType != DV_GLOBAL_DECLARATION && declType != DV_LOCAL_DECLARATION && 
				declType != DV_CLASS_MEMBER && declType != DV_PARAMETR ) 
				throw lxm;

			declPkg->AddChildPackage( np.Release() );

			// считываем последнюю лексему, т.к. 
			// ReadQualifiedConstruction ее оставил в потоке
			NextLexem();	
			
			// устанавливаем, что имя считано
			nameRead = true;
		}
	}	

	// если имеем дело с глобальной декларацией, декларация члена или локальная
	// декларация, можно считывать специальные функции члены
	else if( lxm == KWOPERATOR || lxm == '~' ) 
	{
		// проверяем, можно ли использовать специальные функции
		// в заданном контексте
		if( declType != DV_GLOBAL_DECLARATION &&
			declType != DV_LOCAL_DECLARATION && 
			declType != DV_CLASS_MEMBER ) 
			throw lxm;

		BackLexem();
		QualifiedConstructionReader qcr(lexicalAnalyzer);
		PNodePackage np = qcr.ReadQualifiedConstruction();

		LoadToLexemContainer(qcr.GetLexemContainer());
		declPkg->AddChildPackage( np.Release() );	// добавляем указатель к списку произ. типов			
		lastLxm = NextLexem();
		nameRead = true;
	}	

	// в любом случае считысчитываем хвостовую часть декларатора [], ()
	ReadDeclaratorTailPart();	
}


// считываем хвостовую часть декларатора, если не приведение типа
void DeclaratorReader::ReadDeclaratorTailPart()
{
	Lexem lxm = lastLxm;

	// считываем постфиксные производные типы
	bool in;
	for( in = false; ;in++ )
	{	
		// требуется считать прототип функции
		if( lxm == '(' )
		{  			
			if( declType == DV_CAST_OPERATOR_DECLARATION )
				break;
			
			PNodePackage np = ReadFunctionPrototype();
			if( !np.IsNull() )
			{
				INTERNAL_IF( np->GetPackageID() != PC_FUNCTION_PROTOTYPE || 
							 np->IsNoChildPackages() );		
				declPkg->AddChildPackage(np.Release());
			}

			// иначе был считан список инициализаторов, и мы выходим
			else
			{
				INTERNAL_IF( initializatorList.IsNull() || initializatorList->empty() );
				break;
			}
		}

		else if( lxm == '[' )
		{
			Lexem tmp = NextLexem();
			PNodePackage ar = new NodePackage(PC_ARRAY) ;

			// считываем выражение
			if( tmp != ']' )
			{
				BackLexem();
				ExpressionReader expReader( lexicalAnalyzer, 
					undoContainer.empty() ? NULL : &undoContainer, true);
				expReader.Read();				
				INTERNAL_IF( expReader.GetResultOperand().IsNull() );

				// создаем пакет-выражение
				ar->AddChildPackage( new LexemPackage(lxm) );
				ar->AddChildPackage( new ExpressionPackage( expReader.GetResultOperand()) );
				tmp = lexicalAnalyzer.LastLexem();
				if( tmp != ']' )
					throw tmp;
				ar->AddChildPackage( new LexemPackage(tmp) );
				declPkg->AddChildPackage(ar.Release());
			}

			// иначе массив пустой, сохраняем его
			else
			{
				ar->AddChildPackage( new LexemPackage(lxm) );
				ar->AddChildPackage( new LexemPackage(tmp) );
				declPkg->AddChildPackage(ar.Release());
			}
		}

		else
			break;		

		lxm = NextLexem();
	}

	// если in установлен и 
	if( declType == DV_CAST_OPERATOR_DECLARATION && in )
		BackLexem();
		
}

// считать прототип функции
PNodePackage DeclaratorReader::ReadFunctionPrototype()
{
	PNodePackage np = new NodePackage(PC_FUNCTION_PROTOTYPE);			
	np->AddChildPackage( new LexemPackage( lastLxm ) );	// добавить '('
	Lexem lxm;
	
	// получаем следующую лексему для проверки
	lxm = NextLexem();

	// если уровень скобок нулевой, имеем локальную или глобальную декларацию,
	// пытаемся считать список инициализации
	if( crampLevel == 0 &&
		(declType == DV_GLOBAL_DECLARATION || declType == DV_LOCAL_DECLARATION) &&
		(lxm != ')' && lxm != ELLIPSES) &&
		undoContainer.empty() )
	{
		BackLexem();
		TypeExpressionReader ter(lexicalAnalyzer, true);
		ter.Read(true);
		INTERNAL_IF( ter.GetResultPackage() == NULL );

		// теперь проверяем, если имеем выражение, считываем все инициализаторы,
		if( ter.GetResultPackage()->IsExpressionPackage() )
		{
			initializatorList = new ExpressionList;
			initializatorList->push_back( 
				static_cast<const ExpressionPackage &>(*ter.GetResultPackage()).GetExpression());

			// считываем весь список
			for( ;; )
			{
				if( lexicalAnalyzer.LastLexem() == ')' )
					break;
				if( lexicalAnalyzer.LastLexem() != ',' )
					throw lexicalAnalyzer.LastLexem();

				// считываем очередное выражение
				ExpressionReader er(lexicalAnalyzer, NULL, true);
				er.Read();

				// получаем результат
				initializatorList->push_back( er.GetResultOperand() );
			}

			// считываем ';' для парсера и выходим
			lexicalAnalyzer.NextLexem();
			return NULL;
		}

		// иначе имеем декларатор, т.е. параметр. Создаем пакет
		// с параметром и продолжаем считывание в цикле
		else
		{
			INTERNAL_IF( !ter.GetResultPackage()->IsNodePackage() );

			// также возможно значение по умолчанию
			PNodePackage prm = new NodePackage(PC_PARAMETR);
			const NodePackage *dcl = static_cast<const NodePackage*>(ter.GetResultPackage());
			INTERNAL_IF( dcl->GetChildPackageCount() != 2 );

			prm->AddChildPackage( const_cast<Package*>(dcl->GetChildPackage(0)) );
			prm->AddChildPackage( const_cast<Package*>(dcl->GetChildPackage(1)) );
	
			// проверяем, если последняя считанная лексема '=', значит следует
			// также считать значение параметра по умолчанию
			if( lexicalAnalyzer.LastLexem() == '=' )
			{
				// считываем очередное выражение
				ExpressionReader er(lexicalAnalyzer, NULL, true);
				er.Read();
			
				// добавляем пакет с выражением к параметру
				prm->AddChildPackage( new ExpressionPackage(er.GetResultOperand()) );
			}

			// добавляем параметр к общему пакету
			np->AddChildPackage( prm.Release() );
			lxm = lexicalAnalyzer.LastLexem() == ',' ? 
				lexicalAnalyzer.NextLexem() : lexicalAnalyzer.LastLexem();
		}
	}

	// цикл считки параметров
	if( lxm != ')' )
	for( ;; )
	{			
		if( lxm == ELLIPSES )	// ... 
		{
			Lexem tmp = NextLexem();
			if( tmp != ')' )
				throw tmp;
			else
			{
				np->AddChildPackage( new LexemPackage(lxm) );
				break;
			}
		}

		else
			BackLexem();

		DeclaratorReader dr(DV_PARAMETR, lexicalAnalyzer);
		dr.LoadToUndoContainer( undoContainer );
		undoContainer.clear();
		canUndo = false;		

		dr.ReadTypeSpecifierList();						// считываем список спецификаторов типа
		PNodePackage tsl = dr.GetTypeSpecPackage();
		if( tsl->IsNoChildPackages() )
			throw lexicalAnalyzer.LastLexem();

		PNodePackage dcl = dr.ReadNextDeclarator(),			// считываем декларатор
					 prm = new NodePackage(PC_PARAMETR);
		
		prm->AddChildPackage(tsl.Release());
		prm->AddChildPackage(dcl.Release());
		
		LoadToLexemContainer(dr.GetLexemContainer());

		// имеется значение по умолчанию для параметра
		if( lexicalAnalyzer.LastLexem() == '=' )
		{
			// параметры по умолчанию считываем только у локальных,
			// глобальных и декларациях членов
			if( declType != DV_GLOBAL_DECLARATION &&
				declType != DV_LOCAL_DECLARATION  &&
				declType != DV_CLASS_MEMBER )
				throw lexicalAnalyzer.LastLexem();

			// считываем значение параметра по умолчанию
			ExpressionReader er(lexicalAnalyzer, NULL, true);
			er.Read();
			
			// добавляем пакет с выражением к параметру
			prm->AddChildPackage( new ExpressionPackage(er.GetResultOperand()) );
		}
		
		np->AddChildPackage( prm.Release() );			// наконец добавляем параметр к прототипу	
		if( lexicalAnalyzer.LastLexem() != ',' && lexicalAnalyzer.LastLexem() != ')' )
			throw lexicalAnalyzer.LastLexem();

		if( lexicalAnalyzer.LastLexem() == ')' )
			break;

		// считываем следующую
		lxm = NextLexem();
	}

	np->AddChildPackage( new LexemPackage(lexicalAnalyzer.LastLexem()) );

	// дальше может потребоваться считать список квалификаторов 
	Lexem c, v; 
	ReadCVQualifierSequence(np);	

	// далее может идти список возбуждаемых исключительных ситуаций
	lxm = NextLexem();

	// считываем список исключительных ситуаций
	if( lxm == KWTHROW )
	{
		PNodePackage throwSpec = new NodePackage(PC_THROW_TYPE_LIST);
		
		if( (lxm = NextLexem()) != '(' )
			throw lxm;

		throwSpec->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();

		// пустая throw-спецификация
		if( lxm == ')' )		
			throwSpec->AddChildPackage( new LexemPackage(lxm) );

		// иначе считываем список типов
		else
		{
			BackLexem();
			while(true)
			{
				throwSpec->AddChildPackage(ReadThrowSpecType().Release());
				lxm = lexicalAnalyzer.LastLexem();

				if( lxm == ')' )
				{
					throwSpec->AddChildPackage( new LexemPackage(lxm) );
					break;
				}

				else if( lxm != ',' )
					throw lxm;
			}
		}

		np->AddChildPackage(throwSpec.Release());
	}

	else
		BackLexem();
	return np;
}


// считать тип в throw-спецификации и вернуть его
PNodePackage DeclaratorReader::ReadThrowSpecType()
{
	DeclaratorReader dr(DV_CAST_OPERATION, lexicalAnalyzer);
	
	dr.ReadTypeSpecifierList();						// считываем список спецификаторов типа
	PNodePackage tsl = dr.GetTypeSpecPackage();
	if( tsl->IsNoChildPackages() )
		throw lexicalAnalyzer.LastLexem();

	PNodePackage dcl = dr.ReadNextDeclarator(),			// считываем декларатор
				 tht = new NodePackage(PC_THROW_TYPE);
		
	tht->AddChildPackage(tsl.Release());
	tht->AddChildPackage(dcl.Release());
		
	LoadToLexemContainer(dr.GetLexemContainer());
	return tht;
}


// считать последовательность из cv-квалификаторов, и сохраняет их 
// в пакете
void DeclaratorReader::ReadCVQualifierSequence( PNodePackage &np )
{
	Lexem cv;
	bool c = false, v = false;
	for( ;; )
	{
		cv = NextLexem();
		if( cv == KWCONST && c == false ) 
			np->AddChildPackage( new LexemPackage(cv) ), c = true;

		else if( cv == KWVOLATILE && v == false ) 
			np->AddChildPackage( new LexemPackage(cv) ), v = true;

		else 
			break;
	}

	BackLexem();
}


// если считан тип - класс, перечисление, typedef, специализированный шаблон
bool QualifiedConstructionReader::IsTypeName( ) const
{
	return QualifiedNameManager(&*resultPackage, NULL).IsTypeName(); 
}


// считать квалифицированную конструкцию
PNodePackage QualifiedConstructionReader::ReadQualifiedConstruction()
{
	bool wasQual = false;
	Lexem lxm = NextLexem();

	// если была считана '::', запишем ее и считаем имя
	if( lxm == COLON_COLON )		
	{
		resultPackage->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();		

		// если разрешено выражение и следующая лексема - new или delete,
		// синтаксической ошибки не возникает
		if( noErrorOnExp && (lxm == KWNEW || lxm == KWDELETE) )
			return readExpression = true, NULL;

		// если нет имени после '::' - это синтаксическая ошибка	
		if( lxm != NAME && 
			(!noQualified && !noSpecial && lxm != KWOPERATOR) )
			throw lxm;
		wasQual = true;
	}


	// здесь должно быть имя, либо сразу, либо после '::'
	if( lxm == NAME )
		resultPackage->AddChildPackage( new LexemPackage(lxm) );	

	// пробуем считать оператор, а потом уже проверим, нужно 
	else if( lxm == KWOPERATOR )
	{
read_operator:

		// если мы не можем считывать операторы
		if( noSpecial || (noQualified && wasQual) )
			throw lxm;

		// пытаемся считать оператор, в случае синтаксической ошибки,
		// весь пакет удаляется
		NodePackage *op = ReadOverloadOperator().Release();
		resultPackage->AddChildPackage( op );
		return resultPackage;		// выходим, имя считано
	}

	// возможно деструктор
	else if( lxm == '~' )
	{
read_destructor:

		// не можем считывать деструткоры
		if( noSpecial || (noQualified && wasQual) )
			throw lxm;

		PNodePackage dtor = new NodePackage( PC_DESTRUCTOR );
		dtor->AddChildPackage(new LexemPackage(lxm));
		
		lxm = NextLexem();
		if( lxm != NAME )
			throw lxm;
		
		dtor->AddChildPackage(new LexemPackage(lxm));
		resultPackage->AddChildPackage(dtor.Release());
		return resultPackage;		// выходим, деструктор считан
	}

	// иначе синтаксическая ошибка
	else		
		throw lxm;	

	// цикл считывания составной части, считываем имя шаблона,
	// только в случае если обнаружено ключевое слово template 
	// после '::'
	for( ;; )
	{
		lxm = NextLexem();
		if( lxm != COLON_COLON )
		{
			if( wasQual && noQualified )
				throw lxm;
			break;
		}
		else
			wasQual = true;

		resultPackage->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();

		// считываем имя шаблона
		if( lxm == KWTEMPLATE )
		{
		}

		// иначе должно попастся имя
		else
		{
			// имеем имя, добавляем его в пакет
			if( lxm == NAME   )
				resultPackage->AddChildPackage( new LexemPackage(lxm) );

			// если требуется считать указатель на член
			else if( lxm == '*' )
			{
				resultPackage->SetPackageID( PC_POINTER_TO_MEMBER );
				resultPackage->AddChildPackage( new LexemPackage(lxm) );
				return resultPackage;				
			}

			else if( lxm == KWOPERATOR && !noSpecial )
				goto read_operator;

			else if( lxm == '~' && !noSpecial )
				goto read_destructor;

			else
				throw lxm;				
		}
	}

	BackLexem();		// возвращаем послед. считанную лексему в поток	
	return resultPackage;
}


// считать перегруженный оператор
PNodePackage QualifiedConstructionReader::ReadOverloadOperator( )
{
	PNodePackage op( new NodePackage( PC_OVERLOAD_OPERATOR ) );
	
	// записываем 'operator'	
	op->AddChildPackage( new LexemPackage(
		lastLxm == KWOPERATOR ? lastLxm : lexicalAnalyzer.LastLexem()) );
	Lexem lxm = NextLexem();

	// если оператор требует считывания []
	if( lxm == KWNEW || lxm == KWDELETE )
	{
		op->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();
		if( lxm != '[' )
		{
			BackLexem();
			return op;
		}
				
		op->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();
		if( lxm != ']' )
			throw lxm;
		op->AddChildPackage( new LexemPackage(lxm) );

		return op;
	}

	// если оператор '()' или '[]'
	else if( lxm == '(' || lxm == '[' )
	{
		int nop = lxm == '(' ? ')' : ']';
		op->AddChildPackage( new LexemPackage(lxm) );
		lxm = NextLexem();
		if( lxm != nop )
			throw lxm;
				
		op->AddChildPackage( new LexemPackage(lxm) );
		return op;
	}


	// иначе оператор состоит из одной лексемы
	else if( lxm == '+' || lxm == '-'  || lxm == '*'				||
			 lxm == '/' || lxm == '%'  || lxm == '^'				||
			 lxm == '&' || lxm == '|'  || lxm == '~'				||
			 lxm == '!' || lxm == '='  || lxm == '<'				||
			 lxm == '>'												|| 
			 lxm == PLUS_ASSIGN		   || lxm == MINUS_ASSIGN		||
			 lxm == MUL_ASSIGN		   || lxm == DIV_ASSIGN			|| 
			 lxm == PERCENT_ASSIGN     || lxm == LEFT_SHIFT_ASSIGN  ||
			 lxm == RIGHT_SHIFT_ASSIGN || lxm == AND_ASSIGN			|| 
			 lxm == XOR_ASSIGN		   || lxm == OR_ASSIGN			|| 
			 lxm == LEFT_SHIFT		   || lxm == RIGHT_SHIFT		|| 
			 lxm == LESS_EQU		   || lxm == GREATER_EQU		|| 
			 lxm == EQUAL			   || lxm == NOT_EQUAL			||
			 lxm == LOGIC_AND		   || lxm == LOGIC_OR			|| 
			 lxm == INCREMENT		   || lxm == DECREMENT		    || 
			 lxm == ARROW			   || lxm == ARROW_POINT		||
			 lxm == ',' )
	{
		op->AddChildPackage( new LexemPackage(lxm) );
		return op;
	}

	// иначе пытаемся считать тип, имеем оператор приведения типа
	else
	{
		DeclaratorReader dr(DV_CAST_OPERATOR_DECLARATION, lexicalAnalyzer);

		// считываем декларацию, как будто она является операцией приведения типа
		PNodePackage ct = new NodePackage(PC_CAST_TYPE);		
		
		BackLexem();				
		dr.LoadToUndoContainer( *undoContainer ); 
		undoContainer->clear();
		canUndo = false;		

  		dr.ReadTypeSpecifierList();
		if( dr.GetTypeSpecPackage()->IsNoChildPackages() )
			throw lastLxm;
		ct->AddChildPackage( dr.GetTypeSpecPackage().Release() );	
		ct->AddChildPackage( dr.ReadNextDeclarator().Release() );						
		op->AddChildPackage(ct.Release());

		lexemContainer.insert(lexemContainer.end(), 
			dr.GetLexemContainer().begin(), dr.GetLexemContainer().end());

		op->SetPackageID( PC_CAST_OPERATOR );
		return op;
	}
		
}


// функция считывания конструкции от { до соотв. ей }
void CompoundStatementReader::Read()
{
	int sc = 0;
	for( ;; )
	{
		Lexem lxm = lexicalAnalyzer.NextLexem();
		
		// если не игнорируем поток, значит сохраняем лексему в контейнере
		if( !ignoreStream )
			lexemContainer.push_back(lxm);

		if( lxm == '{' )
			sc ++;

		else if( lxm == '}' )
		{
			sc--;
			INTERNAL_IF( sc < 0 );
			if( sc == 0 )
				break;
		}

		else if( lxm == EOF )
			theApp.Fatal(lxm.GetPos(), "неожиданный конец файла");
	}
}


// закрытая функция, которая считывает try-блок
void FunctionBodyReader::ReadTryBlock()
{
	INTERNAL_IF( lexicalAnalyzer.LastLexem() != KWTRY );
	if( !ignoreStream )
		lexemContainer->push_back(lexicalAnalyzer.LastLexem());

	// считываем следующую лексему, она может быть ':', тогда
	// следует считать список инициализации. Иначе это должно быть '{'
	register Lexem lxm = lexicalAnalyzer.NextLexem();
	if( lxm == ':' )
	{
		lexicalAnalyzer.BackLexem();
		ReadInitList();
		lxm = lexicalAnalyzer.LastLexem();
	}

	INTERNAL_IF( lxm != '{' );
	if( !ignoreStream )
		lexemContainer->push_back(lxm);
	

	int sc = 1;
	bool tryEnd = false;
	for( ;; )
	{
		lxm = lexicalAnalyzer.NextLexem();
		if( !ignoreStream )
			lexemContainer->push_back(lxm);

		if( lxm == '{' )
			sc++;

		else if( lxm == '}' )
		{
			sc--;
			if( sc )		// если это вложенная скобка, ничего не проверяем
				continue;

			lxm = lexicalAnalyzer.NextLexem();

			// следующая лексема должна быть 'catch', если catch-блока
			// еще не было
			if( lxm == KWCATCH )
			{
				tryEnd = true;
				if( !ignoreStream )
					lexemContainer->push_back(lxm);
			}

			// иначе catch-блок уже должен быть
			else 
			{
				if( !tryEnd )
					theApp.Fatal(lxm.GetPos(), "try-блок конструктора не содержит обработчика");

				// иначе возвращаем лексему в поток и выходим
				lexicalAnalyzer.BackLexem();
				break;
			}
		}
		
		else if( lxm == EOF )
			theApp.Fatal(lxm.GetPos(), "неожиданный конец файла");
	}
}


// закрытая функция считывает список инициализации
void FunctionBodyReader::ReadInitList()
{
	INTERNAL_IF( lexicalAnalyzer.LastLexem() != ':' );
	if( !ignoreStream )
		lexemContainer->push_back(lexicalAnalyzer.LastLexem());

	for( ;; )
	{
		register Lexem lxm = lexicalAnalyzer.NextLexem();
		if( lxm == '{' )
			break;		

		else if( lxm == EOF )
			theApp.Fatal(lxm.GetPos(), "неожиданный конец файла");

		else 
			if( !ignoreStream )
				lexemContainer->push_back(lxm);

	}
}


// метод считывания тела
void FunctionBodyReader::Read()
{
	// требуется считывание try-блока
	if( lexicalAnalyzer.NextLexem() == KWTRY )
	{
		ReadTryBlock();	
		return;
	}

	// если требуется считать список инициализации
	else if( lexicalAnalyzer.LastLexem() == ':' )
		ReadInitList();

	int sc = 1;
	register Lexem lxm = lexicalAnalyzer.LastLexem();

	INTERNAL_IF( lxm != '{' );
	if( !ignoreStream )
		lexemContainer->push_back(lxm);

	for( ;; )
	{
		lxm = lexicalAnalyzer.NextLexem();
		
		// если не игнорируем поток, значит сохраняем лексему в контейнере
		if( !ignoreStream )
			lexemContainer->push_back(lxm);

		if( lxm == '{' )
			sc ++;

		else if( lxm == '}' )
		{
			sc--;
			INTERNAL_IF( sc < 0 );
			if( sc == 0 )
				break;
		}

		else if( lxm == EOF )
			theApp.Fatal(lxm.GetPos(), "неожиданный конец файла");
	}
}


// считывает список, возвращает указатель на сформированный.
// Рекурсивная ф-ция
void InitializationListReader::ReadList( ListInitComponent *lic )
{
	// '{' - считано, считываем до '}'. Если появляется '{',
	// тогда создаем новый список и считываем в него, затем сохраняем
	// его в текущем
	INTERNAL_IF( lic == NULL );
	for( ;; )
	{
		int lc = lexicalAnalyzer.NextLexem();
		if( lc == '}' )
			break;
		else if( lc == '{' )
		{
			ListInitComponent *subLic = 
				new ListInitComponent(lexicalAnalyzer.LastLexem().GetPos());
			ReadList(subLic);
			lic->AddInitComponent(subLic);
		}

		// иначе считываем выражение
		else
		{
			lexicalAnalyzer.BackLexem();
			ExpressionReader er(lexicalAnalyzer, NULL, true);
			er.Read();

			// сохраняем выражение			
			lic->AddInitComponent( new AtomInitComponent(
				er.GetResultOperand(), lexicalAnalyzer.LastLexem().GetPos()) );
		}

		// если '}', выходим
		if( lexicalAnalyzer.LastLexem() == '}' )
			break;

		// проверяем, если следующая лексема не ',', значит ошибка
		if( lexicalAnalyzer.LastLexem() != ',' )
			throw lexicalAnalyzer.LastLexem();
	}

	// завершаем формирование списка
	lic->Done();

	// считыванием следующую лексему для вызывающей функции
	lexicalAnalyzer.NextLexem();
}


// считать список инициализации
void InitializationListReader::Read()
{		
	listInitComponent = new ListInitComponent(lexicalAnalyzer.LastLexem().GetPos());
	ReadList( listInitComponent );	
	listInitComponent->Done();
}


// считать декларацию, если декларация некорректно сформирована,
// считать выражение
void TypeExpressionReader::Read( bool fnParam, bool readTypeDecl )
{
	bool canReadExpr = true;
	DeclaratorReader dr( DV_LOCAL_DECLARATION, lexicalAnalyzer, true );

	try {
		dr.ReadTypeSpecifierList();		
		PNodePackage typeLst = dr.GetTypeSpecPackage();
		int llc = typeLst->IsNoChildPackages() ? 0 : 
			typeLst->GetLastChildPackage()->GetPackageID();
			
		// если не было считано типа, считываем выражение
		if( typeLst->IsNoChildPackages() )
		{
			// проверим, если было считано две лексемы - имя и ':',
			// значит имеем метку, генерируем синтаксическую ошибку сразу
			if( lexicalAnalyzer.LastLexem() == ':' && lexicalAnalyzer.PrevLexem() == NAME )
			{
				Lexem prv = lexicalAnalyzer.PrevLexem();
				lexicalAnalyzer.NextLexem();
				throw LabelLexem( prv );
			}

			// иначе переходим к считке выражения
			goto read_expression;
		}

		// считано больше одного пакета, либо считаны ключи составных
		// типов выражение не получится
		else if( typeLst->GetChildPackageCount() > 1 ||
			(llc == KWCLASS || llc == KWSTRUCT || llc == KWUNION || llc == KWENUM) )
		{
			// базовый тип, который возможно был считан
			const BaseType *rbt = NULL;

			// возвращает true, если дальнейшие считывания деклараций не нужны,
			// т.к. идет объявление типа заканчивающееся ';'
			if( !fnParam && readTypeDecl && 
				ParserUtils::LocalTypeDeclaration( lexicalAnalyzer, &*typeLst, rbt) )
			{
				INTERNAL_IF( lexicalAnalyzer.LastLexem() != ';' );
				NodePackage *np = new NodePackage(PC_CLASS_DECLARATION);
				np->AddChildPackage( typeLst.Release() );
				resultPkg = np;

				// если был считан класс, сохраняем его, перечисление не сохраняем
				if( rbt->IsClassType() )
					redClass = static_cast<const ClassType *>(rbt);
				return ;
			}

			canReadExpr = false;
		}
	

		// считываем декларатор
		PNodePackage decl = dr.ReadNextDeclarator();

		// если имя отстутствует, это считается синтаксической ошибкой,
		// но только если не считываем параметр функции
		if( decl->FindPackage(PC_QUALIFIED_NAME) < 0 && !fnParam)
		{
			// если количество производных типов больше 1,
			// считывать выражение не имеет смысла
			if( decl->GetChildPackageCount() > 1 )
				canReadExpr = false;			
			throw lexicalAnalyzer.LastLexem();
		}

		// в этой точке может быть только декларация,
		// поэтому сохраняем пакеты и выходим
		NodePackage *np= new NodePackage(PC_DECLARATION);
		np->AddChildPackage( typeLst.Release() );
		np->AddChildPackage( decl.Release() );

		resultPkg = np;
		if( !ignore )
			lexemContainer = new LexemContainer(dr.GetLexemContainer());

		// сохраняем список инициализаторов, в случае успешной считки
		initializatorList = dr.GetInitializatorList();
		return;
	
	} catch( const LabelLexem & ) {
		// перехватываем считанную лексему, передаем дальше
		throw;	
	} catch( const Lexem &lxm ) {

		// произошла синтаксическая ошибка, например 'int (3)'
		// вызовет такую ошибку. Тогда мы загружаем контейнер в 
		// ридер выражения и считываем выражение
		if( canReadExpr )
			goto read_expression;
		else
			throw lxm;
	}

read_expression: 
	LexemContainer *lc = const_cast<LexemContainer*>(&dr.GetLexemContainer());
	lc->insert(lc->end(), dr.GetUndoContainer().begin(), dr.GetUndoContainer().end());	
	ExpressionReader er(lexicalAnalyzer, lc, noComa, noGT, ignore);	
	er.Read();
	
	// создаем пакет с выражением в качестве результирующего	
	resultPkg = new ExpressionPackage(er.GetResultOperand());
	if( !ignore )
		lexemContainer = er.GetLexemContainer();
}


// возвращает следующую лексему из потока, либо из контейнера, если тот не пуст
Lexem ExpressionReader::NextLexem()
{
	// если контейнер отката не пуст, вернуть лексему ИЗ него
	if( canUndo )
	{
		INTERNAL_IF( undoContainer == NULL );
		if( !undoContainer->empty() )
		{
			lastLxm = undoContainer->front();			
			undoContainer->pop_front();						

			// сохраняем лексему в контейнере при необходимости
			if( !ignore)
				lexemContainer->push_back(lastLxm);
			return lastLxm;
		}

		else
			canUndo = false;
	}

	lastLxm = lexicalAnalyzer.NextLexem();
	if( !ignore )
		lexemContainer->push_back(lastLxm);	
	return lastLxm;
}


// возвращает в поток или в контейнер последнюю считанную лексему
void ExpressionReader::BackLexem()
{
	// нельзя возвратить в поток пустую лексему
	INTERNAL_IF( lastLxm.GetCode() == 0 );

	// если контейнер отката не пуст, вернуть лексему В него
	if( canUndo )	
	{
		INTERNAL_IF( undoContainer == NULL );

		if( !ignore )
			lexemContainer->pop_back();
		undoContainer->push_front(lastLxm);
	}

	else
	{
		lexicalAnalyzer.BackLexem();

		if( !ignore )
			lexemContainer->pop_back();
	}

	lastLxm = Lexem();		// очищаем лексему для того чтобы легче определять ошибки
}


// вернуть в undoContainer лексему. При этом если режим восстановления
// не включен, он включается
void ExpressionReader::UndoLexem( const Lexem &lxm ) 
{
	if( !ignore )
		lexemContainer->pop_back();

	undoContainer->push_front(lxm);
	canUndo = true;
}


// проверить результирующий операнд, он не должен быть типом, 
// перегруженной функцией, а также основным операндом, который
// является нестатическим членом класса
void ExpressionReader::CheckResultOperand( POperand &pop )
{
	const Position &errPos = lexicalAnalyzer.LastLexem().GetPos();

	// если тип или перегруженная ф-ция
	if( pop->IsTypeOperand() || pop->IsOverloadOperand() )	
	{	
		theApp.Error(errPos, "%s не может быть выражением",
			pop->IsTypeOperand() ? "тип" : "перегруженная функция");					
		pop = ErrorOperand::GetInstance();
	}

	// если основной операнд, проверим доступен ли 'this' в данном месте
	else if( pop->IsPrimaryOperand() )
	{
		if( ExpressionMakerUtils::CheckMemberThisVisibility(pop, errPos) < 0 )
			pop = ErrorOperand::GetInstance();
	}
}


// считать выражение
void ExpressionReader::Read()
{
	// внимание! метод может генерировать исключительные ситуации,
	// вызывающая функция должна их перехватывать
	NextLexem();
	EvalExpr1(resultOperand);

	// проверим, чтобы результирующий операнд не был типом, 
	// перегруженной функцией, а также основным операндом, который
	// является нестатическим членом класса
	CheckResultOperand(resultOperand);
}


// считать абстрактный декларатор, вернуть пакет с типом. Если 
// noError - true, значит считанные лексемы загружаются в undoContainer
PNodePackage ExpressionReader::ReadAbstractDeclarator( bool noError, DeclarationVariant dv )
{
	bool canReadExpr = true;
	DeclaratorReader dr( dv, lexicalAnalyzer, true );

	try {
		dr.ReadTypeSpecifierList();
		PNodePackage typeLst = dr.GetTypeSpecPackage();

		// если не было считано типа, загружаем считанные лексемы в undoContainer
		// и выходим
		if( typeLst->IsNoChildPackages() )
		{		
			UndoLexemS(dr.GetLexemContainer());
			UndoLexemS(dr.GetUndoContainer());
			if( noError )
				return NULL;
			else
				throw lexicalAnalyzer.LastLexem();
		}

		// считано больше одного пакета, выражение не получится
		else if( typeLst->GetChildPackageCount() > 1 )
			canReadExpr = false;

		// считываем декларатор	
		PNodePackage decl = dr.ReadNextDeclarator();

		// в этой точке может быть только декларация,
		// поэтому сохраняем пакеты и выходим
		NodePackage *np= new NodePackage(PC_DECLARATION);
		np->AddChildPackage( typeLst.Release() );
		np->AddChildPackage( decl.Release() );
		
		// если необходимо сохранить считанные лексемы в контейнере,
		// сохраняем их в конец
		if( !ignore )
			lexemContainer->insert(lexemContainer->end(),
				dr.GetLexemContainer().begin(), dr.GetLexemContainer().end());		
		return np;

	} catch( const Lexem & ) {
		if( noError && canReadExpr )
			UndoLexemS( dr.GetLexemContainer() );
		else
			throw;
	}

	return PNodePackage(NULL);
}


// считать список выражений, вернуть список в упакованном виде,
// Условием завершения считывания является лексема ')'. 
// Результирующий список может быть пустым
PExpressionList ExpressionReader::ReadExpressionList( )
{
	PExpressionList expList = new ExpressionList ;	
	if( lastLxm == ')' )
		return  expList;

	for( ;; )
	{
		POperand exp = NULL;
		EvalExpr2( exp );

		// проверяем считанное выражение на корректность
		CheckResultOperand( exp );

		// сохраняем считанное выражение в списке
		expList->push_back( exp );
		
		// если последняя лексема - ')', завершаем считывание
		if( lastLxm == ')' )
			break;

		// значит следующая лексема должна быть ','
		if( lastLxm != ',' )
			throw lastLxm;

		INTERNAL_IF( exp.IsNull() );
		NextLexem();
	}

	return expList;
}


// далее идут методы разбора выражений.
//
// оператор ','
void ExpressionReader::EvalExpr1( POperand &result )
{
	POperand temp(NULL);

	// при создании объекта контролируется переполнение стека
	StackOverflowCatcher soc;	

	EvalExpr2( result );
	while( lastLxm == ',' && (!noComa || level) )
	{
		NextLexem();
		EvalExpr2(temp);

		// строим выражение из двух запятых
		result = BinaryExpressionCoordinator<ComaBinaryMaker>(result, temp, 
			',', lastLxm.GetPos()).Coordinate();
	}
}


// операторы присвоения '=', '+=','-=','*=','/=','%=','>>=','<<=','|=','&=','^=',
// throw
void ExpressionReader::EvalExpr2( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	
	int op;	

	if( lastLxm == KWTHROW )
	{
		NextLexem();

		// проверяем, если знак пунктуации, значит throw пустой,
		// и выражение считывать не нужно
		if( lastLxm != ';' && lastLxm != ',' && lastLxm != ':' )
			EvalExpr2(temp);	

		// иначе создаем операнд с типом void, как знак того,
		// что throw вызывается без параметра
		else		
			temp = new PrimaryOperand( false, *new TypyziedEntity(
				(BaseType*)&ImplicitTypeManager(KWVOID).GetImplicitType(), 
				false, false, DerivedTypeList()) );
				
		// постройка выражения и выход
		result = UnaryExpressionCoordinator(temp, KWTHROW, lastLxm.GetPos()).Coordinate();	
	}

	else
	{

	EvalExpr3( result );
	if( (op = lastLxm) == '=' || 
		 op == PLUS_ASSIGN  || op == MINUS_ASSIGN ||
		 op == MUL_ASSIGN   || op == DIV_ASSIGN   || op == PERCENT_ASSIGN || 
		 op == LEFT_SHIFT_ASSIGN || op == RIGHT_SHIFT_ASSIGN ||
		 op == AND_ASSIGN || op == XOR_ASSIGN || op == OR_ASSIGN )
	
	{
		NextLexem();
		EvalExpr2(temp);		

		result = BinaryExpressionCoordinator<AssignBinaryMaker>(result, temp, 
			op, lastLxm.GetPos()).Coordinate();
	}

	}	// else

}


// оператор '?:'
void ExpressionReader::EvalExpr3( POperand &result )
{
	POperand temp1(NULL), temp2(NULL);
	StackOverflowCatcher soc;

	EvalExpr4( result );
	if( lastLxm == '?' )
	{
		NextLexem();
		
		level++;
		EvalExpr1(temp1);			
		level--;
		
		if( lastLxm != ':' )	
			throw lastLxm;

		NextLexem();		
		EvalExpr4(temp2);
		
		// строим тернарное выражение
		result = TernaryExpressionCoordinator(result, temp1, temp2,
			'?', lastLxm.GetPos()).Coordinate();
	}
}


// оператор ||
void ExpressionReader::EvalExpr4( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	

	EvalExpr5( result );
	while( lastLxm == LOGIC_OR )
	{
		NextLexem();
		EvalExpr5( temp );
		
		result = BinaryExpressionCoordinator<LogicalBinaryMaker>(result, temp, 
			LOGIC_OR, lastLxm.GetPos()).Coordinate();
	}
}


// оператор &&
void ExpressionReader::EvalExpr5( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	

	EvalExpr6( result );
	while( lastLxm == LOGIC_AND )
	{
		NextLexem();
		EvalExpr6( temp );
		
		result = BinaryExpressionCoordinator<LogicalBinaryMaker>(result, temp, 
			LOGIC_AND, lastLxm.GetPos()).Coordinate();
	}
}


// оператор |
void ExpressionReader::EvalExpr6( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	

	EvalExpr7( result );
	while( lastLxm == '|' )
	{
		NextLexem();
		EvalExpr7( temp );

		result = BinaryExpressionCoordinator<IntegralBinaryMaker>(result, temp, 
			'|', lastLxm.GetPos()).Coordinate();
	}
}


// оператор '^'
void ExpressionReader::EvalExpr7( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	

	EvalExpr8( result );
	while( lastLxm == '^' )
	{
		NextLexem();
		EvalExpr8( temp );		

		result = BinaryExpressionCoordinator<IntegralBinaryMaker>(result, temp, 
				'^', lastLxm.GetPos()).Coordinate();
	}
}


// оператор '&'
void ExpressionReader::EvalExpr8( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;	

	EvalExpr9( result );
	while( lastLxm == '&' )
	{
		NextLexem();
		EvalExpr9( temp );		

		result = BinaryExpressionCoordinator<IntegralBinaryMaker>(result, temp, 
					'&', lastLxm.GetPos()).Coordinate();
	}
}

	
// операторы ==, !=
void ExpressionReader::EvalExpr9( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;		

	EvalExpr10( result );
	while( lastLxm == EQUAL || lastLxm == NOT_EQUAL )
	{
		register int op = lastLxm;
		NextLexem();
		EvalExpr10( temp );		
			
		// строим операции сравнения
		result = BinaryExpressionCoordinator<ConditionBinaryMaker>(result, temp, 
				op, lastLxm.GetPos()).Coordinate();
	}	
}


// операторы <=, <, >, >=
void ExpressionReader::EvalExpr10( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;			
	register int op;

	EvalExpr11( result );
	while( (op = lastLxm) == LESS_EQU || op == GREATER_EQU || 
			op == '<' || op == '>' )
	{
		NextLexem();
		EvalExpr11( temp );		

		// строим операции сравнения
		result = BinaryExpressionCoordinator<ConditionBinaryMaker>(result, temp, 
				op, lastLxm.GetPos()).Coordinate();
	}			
}


// операторы <<, >>
void ExpressionReader::EvalExpr11( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;			

	EvalExpr12( result );
	while( lastLxm == RIGHT_SHIFT || lastLxm == LEFT_SHIFT )
	{
		register int op = lastLxm;
		NextLexem();
		EvalExpr12( temp );		

		// бинарные операции сдвига
		result = BinaryExpressionCoordinator<IntegralBinaryMaker>(result, temp, 
					op, lastLxm.GetPos()).Coordinate();
	}
}


// операторы +, -
void ExpressionReader::EvalExpr12( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;			

	EvalExpr13( result );
	while( lastLxm == '+' || lastLxm == '-' )
	{
		register int op = lastLxm;
		NextLexem();
		EvalExpr13( temp );	
		
		// строим бинарные операции сложения или вычитания
		result = op == '+' 
			? 	BinaryExpressionCoordinator<PlusBinaryMaker>(result, temp, 
					op, lastLxm.GetPos()).Coordinate()
			:	BinaryExpressionCoordinator<MinusBinaryMaker>(result, temp, 
					op, lastLxm.GetPos()).Coordinate();
	}
}


// операторы *, /, %
void ExpressionReader::EvalExpr13( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;		

	EvalExpr14( result );
	while( lastLxm == '*' || lastLxm == '/' || lastLxm == '%' )
	{
		register int op = lastLxm;

		NextLexem();
		EvalExpr14( temp );	
		
		// конструируем либо интегральное выражение с оператором '%',
		// либо арифметическое с '*' или '/'
		result = op == '%' 
			?	BinaryExpressionCoordinator<IntegralBinaryMaker>(result, temp, 
					op, lastLxm.GetPos()).Coordinate()
			:	BinaryExpressionCoordinator<MulDivBinaryMaker>(result, temp, 
					op, lastLxm.GetPos()).Coordinate();		
	}
}


// операторы .*, ->*
void ExpressionReader::EvalExpr14( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;			

	EvalExpr15( result );
	while( lastLxm == DOT_POINT || lastLxm == ARROW_POINT )
	{
		register int op = lastLxm;
		NextLexem();
		EvalExpr15( temp );	

		// конструируем обращение к указателю на член		
		result = BinaryExpressionCoordinator<PointerToMemberBinaryMaker>(result, temp, 
			op, lastLxm.GetPos()).Coordinate();
	}
}


// операторы приведения типа '(тип)'
void ExpressionReader::EvalExpr15( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;

	// возможно имеем приведение типа
	if( lastLxm == '(' )
	{
		// если имеем тип, считываем его. Иначе если имеем выражение
  		PNodePackage np = ReadAbstractDeclarator( true );

		// если тип не считан, значит имеем выражение в скобках,
		// и следует считатывать далее. lastLxm при этом не изменилась
		if( np.IsNull() )
			EvalExpr16(result);

		// иначе следующая лексема должна быть ')' и опять вызываем рекурсию
		else
		{
			lastLxm = !ignore ? lexemContainer->back() : lexicalAnalyzer.LastLexem();
			if( lastLxm != ')' )
				throw lastLxm;

			NextLexem();

			// строим операнд
			result = ExpressionMakerUtils::MakeTypeOperand(*np);
			EvalExpr15( temp );

			// конструируем явное приведение типа
			result = BinaryExpressionCoordinator<TypeCastBinaryMaker>(result, temp, 
				OC_CAST, lastLxm.GetPos()).Coordinate();
		}
	}

	else
		EvalExpr16(result);
}


// унарные операторы !, ~, +, -, *, &. size, ++, --, new, delete
void ExpressionReader::EvalExpr16( POperand &result )
{
	POperand temp(NULL);
	StackOverflowCatcher soc;
	register int op;	

	if( (op = lastLxm) == '!' || op == '~' ||
		 op == '+'			  || op == '-' || 
		 op == '*'			  || op == '&' ||
		 op == INCREMENT	  || op == DECREMENT )
		 
	{
		NextLexem();
		EvalExpr15( result );
		result = UnaryExpressionCoordinator(result, 
			op == INCREMENT || op == DECREMENT ? -op : op, lastLxm.GetPos()).Coordinate();
	}

	else if( op == KWSIZEOF )
	{
		// здесь возникает неоднозначность, считывать тип, или
		// выражение.
		NextLexem();

		// если имеем '(', вероятно это может быть тип
		if( lastLxm == '(' )
		{
			// если имеем тип, считываем его. Иначе если имеем выражение
			PNodePackage np = ReadAbstractDeclarator( true );

			// если тип не считан, значит имеем выражение в скобках,
			// и следует считатывать далее. lastLxm при этом не изменилась
			if( np.IsNull() )
				level--,				// уменьшаем уровень, т.к. далее его увеличат
				EvalExpr17(result);

			// иначе следующая лексема должна быть ')' и строим типовой операнд
			else
			{				
				lastLxm = !ignore ? lexemContainer->back() : lexicalAnalyzer.LastLexem();
				if( lastLxm != ')' )
					throw lastLxm;
				
				NextLexem();								
				// строим операнд
				result = ExpressionMakerUtils::MakeTypeOperand(*np);
			}			
		}
		
		// иначе неоднозначности нет и мы считываем унарное выражение
		else
			EvalExpr15( result );

		// конструируем выражение sizeof
		result = UnaryExpressionCoordinator(result, KWSIZEOF, lastLxm.GetPos()).Coordinate();
	}

	// операторы выделения памяти и освобождения. Также проверяется конструкция
	// которая начинается с '::', т.к. это может быть '::new' или '::delete'
	else if( op == KWNEW || op == KWDELETE || op == COLON_COLON )
	{
		// проверяем следующую лексему
		bool globalOp = false;
		if( op == COLON_COLON )
		{
			globalOp = true;

			Lexem temp = lastLxm;
			NextLexem();
			if( lastLxm != KWNEW && lastLxm != KWDELETE )
			{				
				UndoLexem(lastLxm);
				UndoLexem(temp);				
				NextLexem();
				EvalExpr17( result );
				return;
			}

			else
				op = lastLxm;
		}

		// считываем полную конструкцию 'new'
		bool arrayOp = false;
		if( op == KWNEW )
		{
			// в самом развернутом виде, выражение 'new', содержит:
			// список выражений-размещений, тип выделяемой памяти,
			// список инициализаторов. В первом случае, возникает
			// неоднозначность, т.к. и размещение и тип, могут быть в 
			// скобках.
			NextLexem();

			// параметры для new-с размещением
			PExpressionList placementList = new ExpressionList;

			// тип выделяемой памяти			
			PNodePackage allocType = NULL;

			// список инициализаторов 
			PExpressionList initList = new ExpressionList;

			// пробуем считать сначала декларатор, а уже потом выражение
			if( lastLxm == '(' )
			{				
				// пытаемся считать декларатор				
  				allocType = ReadAbstractDeclarator( true );				

				// если тип не считан, значит имеем, список выражений в скобках,
				// которые представляют параметры для new-с размещением
				if( allocType.IsNull() )
				{
					NextLexem();					
					placementList = ReadExpressionList();						
					NextLexem();
		
					// список выражений не может быть пустой
					if( placementList->empty() )
						throw lastLxm;
				}
				
				// иначе декларатор должен оканчиваться ')'
				else
				{					
					if( lexicalAnalyzer.LastLexem() != ')' )
						throw lastLxm;
					NextLexem();
					goto read_init_list;
				}
			}

			// тип может быть в скобках, только после списка выражений с размещением,
			// поэтому лексему возвращать не следует
			if( lastLxm == '(' )
			{
				allocType = ReadAbstractDeclarator( false );	
				if( lexicalAnalyzer.LastLexem() != ')' )
					throw lastLxm;
				NextLexem();
			}

			else
			{
				BackLexem();
				allocType = ReadAbstractDeclarator( false, DV_NEW_EXPRESSION );	

				// получаем следующую лексему. Если лексема является оператором
				// и не является производным типом или '(', 
				Lexem temp = lexicalAnalyzer.LastLexem();
				if( NextLexem() != '(' )
				{
					// если последняя считанная лексема была лексема после ']',
					// она остается в буфере и равна lastLxm, поэтому не следует
					// ее возвращать анализатору
					if( !(temp.GetPos().col == lastLxm.GetPos().col &&
						  temp.GetPos().line == lastLxm.GetPos().line) )
						BackLexem() ;
					lastLxm = temp,
					lexicalAnalyzer.SetLastLexem( temp );
				}
			}
			
		read_init_list:
			
			// считываем список инициализаторов если требуется
			if( lastLxm == '(' )
			{
				NextLexem();
				initList = ReadExpressionList();	
				NextLexem();
			}

			// в самом конце строим выражение. Следует учесть, что
			// строитель new, также как и строитель вызова функции, должен
			// сам позаботиться о входных параметрах, а также о поиске
			// перегруженного оператора			
			result = NewTernaryMaker( 
				(*allocType), placementList, initList,
				globalOp, lastLxm.GetPos()).Make();
		}

		// иначе считываем 'delete'
		else
		{
			NextLexem();

			// может быть освобождение массива			
			if( lastLxm == '[' )
				if( NextLexem() != ']' )
					throw lastLxm;
				else
				{
					arrayOp = true;
					NextLexem();
				}

			// считываем выражение приведения
			EvalExpr15(result);

			// конструируем операцию освобождения памяти. Если вызывается глобальный
			// оператор, коды операторов отрицательны
			int opc = (arrayOp ? OC_DELETE_ARRAY : KWDELETE);
			result = UnaryExpressionCoordinator(result, globalOp ? -opc : opc,
				lastLxm.GetPos()).Coordinate();
		}
	}
	
	else	
		EvalExpr17( result );		
}


// операторы '()', '[]', '->', '.', постфиксные ++, --,
// dynamic_cast, static_cast, const_cast, reinterpret_cast, typeid
void ExpressionReader::EvalExpr17( POperand &result )
{		
	POperand temp(NULL);
	StackOverflowCatcher soc;
	int op;

	// явное приведение типа
	if( lastLxm == KWDYNAMIC_CAST || lastLxm == KWSTATIC_CAST ||
		lastLxm == KWREINTERPRET_CAST || lastLxm == KWCONST_CAST )
	{
		op = lastLxm;

		// далее должно идти '<' тип '>'
		if( NextLexem() != '<' )
			throw lastLxm;

		// считываем тип		
		PNodePackage np = ReadAbstractDeclarator( false );
		lastLxm = !ignore ? lexemContainer->back() : lexicalAnalyzer.LastLexem();
		
		// тип должен заканчиваться на '>', следом должно идти 
		// выражение в скобках
		if( lastLxm != '>' )
			throw lastLxm;

		if( NextLexem() != '(' )
			throw lastLxm;

		// считываем выражение
		level++;
		NextLexem();
		EvalExpr1( temp );	
		level--;

		// должна скобка
		if( lastLxm != ')' )
			throw lastLxm;

		// создаем типовой операнд, на основе считанного пакета
		INTERNAL_IF( np.IsNull());
		result = ExpressionMakerUtils::MakeTypeOperand(*np);

		// конструируем приведение типа
		switch(op)
		{
		case KWDYNAMIC_CAST: 
			result = BinaryExpressionCoordinator<DynamicCastBinaryMaker>(
								result, temp, op, lastLxm.GetPos()).Coordinate();
			break;

		case KWSTATIC_CAST:
			result = BinaryExpressionCoordinator<StaticCastBinaryMaker>(
								result, temp, op, lastLxm.GetPos()).Coordinate();
			break;

		case KWREINTERPRET_CAST:
			result = BinaryExpressionCoordinator<ReinterpretCastBinaryMaker>(
								result, temp, op, lastLxm.GetPos()).Coordinate();
			break;

		case KWCONST_CAST:
			result = BinaryExpressionCoordinator<ConstCastBinaryMaker>(
								result, temp, op, lastLxm.GetPos()).Coordinate();		
			break;
		default:
			INTERNAL( "неизвестный код оператора" );
		}

		NextLexem();
	}

	// операция typeid
	else if( lastLxm == KWTYPEID )
	{
		// typeid ( <тип или выражение> )
		if( NextLexem() != '(' )
			throw lastLxm;

		// пытаемся сначала считать тип
  		PNodePackage np = ReadAbstractDeclarator( true );

		// если тип не считан, значит имеем выражение в скобках,
		// и следует считатывать далее. lastLxm при этом не изменилась
		if( np.IsNull() )					
			EvalExpr18(result);		

		// иначе следующая лексема должна быть ')' и опять вызываем рекурсию
		else
		{
			lastLxm = !ignore ? lexemContainer->back() : lexicalAnalyzer.LastLexem();
			if( lastLxm != ')' )
				throw lastLxm;
			NextLexem();
			// строим типовой операнд
			result = ExpressionMakerUtils::MakeTypeOperand(*np);		
		}
		
		// конструируем операцию typeid
		result = UnaryExpressionCoordinator(result, KWTYPEID, lastLxm.GetPos()).Coordinate();		
	}

	else
		EvalExpr18(result);

	for(;;)
	{
		if( lastLxm == '(' || lastLxm == '[' )
		{
			int b = lastLxm == '(' ? ')' : ']';
			op = lastLxm;

			NextLexem();

			if( op == '(' )
			{
				// Считыаваем список параметров функции, который может быть
				// пустой, сохраняем его 
				PExpressionList parametrList = ReadExpressionList();
				INTERNAL_IF( lastLxm != ')' );

				// конструируем вызов функции
				result = FunctionCallBinaryMaker(result, 
					parametrList, OC_FUNCTION, lastLxm.GetPos()).Make(); 
			}

			else 
			{
				if( lastLxm == ']' )
					throw lastLxm;
				else
				{
					level++;
					EvalExpr1( temp );		// вычисляем выражение-индекс
					level--;

					if( lastLxm != ']' )
						throw lastLxm;					
				}

				// конструируем обращение к элементу массива
				result = BinaryExpressionCoordinator<ArrayBinaryMaker>(
					result, temp, OC_ARRAY, lastLxm.GetPos()).Coordinate();
			}
		}

		// селектор члена класса
		else if( lastLxm == '.' || lastLxm == ARROW )
		{
			op = lastLxm;

			// считываем поле
			INTERNAL_IF( !undoContainer->empty() );
			QualifiedConstructionReader qcr(lexicalAnalyzer, false, false);
			PNodePackage id = qcr.ReadQualifiedConstruction();
		
			// не может быть указателя на член
			if( qcr.IsPointerToMember() )
				throw qcr.GetLexemContainer().back();
			
			// сохраняем считанные лексемы в контейнере, если требуется
			if( !ignore )
				lexemContainer->insert(lexemContainer->end(), 
					qcr.GetLexemContainer().begin(), qcr.GetLexemContainer().end());

			// строим результат
			result = SelectorBinaryMaker(result, *id, op, lastLxm.GetPos()).Make();
			
			// если считан оператор приведения, и он взят в скобки, скобка
			// теряется, поэтому следует ее восстановить
			if( id->GetLastChildPackage()->GetPackageID() == PC_CAST_OPERATOR && 
				qcr.GetLexemContainer().back() == ')' )
			{
				lastLxm = qcr.GetLexemContainer().back();
				continue;
			}			
		}

		// постфиксный инкремент, декремент
		else if( lastLxm == INCREMENT || lastLxm == DECREMENT )
		{
			// конструируем постфиксный кремент
			result = UnaryExpressionCoordinator(result, lastLxm, lastLxm.GetPos()).Coordinate();
		}

		else
			break;
		NextLexem();
	}
}


// литерал, true, false, this,
// идентификатор (перегруженный оператор, деструктор, оператор приведения)
// или выражение в скобках
void ExpressionReader::EvalExpr18( POperand &result )
{
	register int tokcode = lastLxm;

	// если выражение в скобках
	if( tokcode == '(' )
	{
		NextLexem();
		level++;
		EvalExpr1(result);
		level--;
		if( lastLxm != ')' )		
			throw lastLxm;				

		// задаем, что выражение в скобках
		if( !result.IsNull() && result->IsExpressionOperand() )
			static_cast<Expression &>(*result).SetCramps();
		NextLexem();
	}

	// если идентификатор
	else if( tokcode == NAME || tokcode == COLON_COLON || tokcode == KWOPERATOR )
	{		
		BackLexem();		// возвращаем лексему в поток

		// undoContainer может быть NULL, нас это устраивает
		PLexemContainer uc( undoContainer );	
		QualifiedConstructionReader qcr(lexicalAnalyzer, false, false, uc);
		PNodePackage id = qcr.ReadQualifiedConstruction();
		uc.Release();	

		// не может быть указателя на член
		if( qcr.IsPointerToMember() )
			throw qcr.GetLexemContainer().back();
			
		// сохраняем считанные лексемы в контейнере, если требуется
		if( !ignore )
			lexemContainer->insert(lexemContainer->end(), qcr.GetLexemContainer().begin(),
				qcr.GetLexemContainer().end());

		// если считан оператор приведения, и он взят в скобки, скобка
		// теряется, поэтому следует ее восстановить
		if( id->GetLastChildPackage()->GetPackageID() == PC_CAST_OPERATOR &&
			lexicalAnalyzer.LastLexem() != '(' )
			lastLxm = qcr.GetLexemContainer().back();

		else
			NextLexem();

		// создаем идентификатор
		result = IdentifierOperandMaker(*id).Make();
	}


	// если литерал
	else if( IS_LITERAL(tokcode) || tokcode == KWTRUE || tokcode == KWFALSE )
	{
		// создаем литерал
		result = LiteralMaker(lastLxm).Make();
		NextLexem();
	}
	
	// если указатель this
	else if( tokcode == KWTHIS )
	{
		// создаем указатель this с проверками
		result = ThisMaker( lastLxm.GetPos() ).Make();
		NextLexem();
	}

	// иначе если 'простой спецификатор типа', необходимо считать явный вызов
	// конструктора, который эквивалентен приведению типа
	else if( IS_SIMPLE_TYPE_SPEC(tokcode) )
	{
		// следующая лексема должна быть '(', т.к. должен идти вызов конструктора
		if( NextLexem() != '(' )
			throw lastLxm;

		// считываем список параметров в приведении типа и сохраняем его
		NextLexem();
		PExpressionList parametrList = ReadExpressionList();
		INTERNAL_IF( lastLxm != ')' );

		// создаем вызов функции через тип
		result = FunctionCallBinaryMaker(ExpressionMakerUtils::MakeSimpleTypeOperand(tokcode),
			parametrList, OC_FUNCTION, lastLxm.GetPos()).Make();
		NextLexem();
	}

	else
		throw lastLxm;	// обычная синтаксическая ошибка
}

