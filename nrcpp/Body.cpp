// реализаци€ поведени€ сущностей относ€щихс€ к телу функции - Body.cpp

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
#include "Body.h"
#include "ExpressionMaker.h"


// единственный экземпл€р ошибочного операнда
POperand ErrorOperand::errorOperand = NULL;


// €вно определ€ем абстрактные деструкторы
ObjectInitializator::~ObjectInitializator() { }
BodyComponent::~BodyComponent() { }
Instruction::~Instruction() { }	
AdditionalOperation::~AdditionalOperation() { }
LabelBodyComponent::~LabelBodyComponent() { }


// в деструкторе уничтожаетс€ значение по умолчанию
Parametr::~Parametr() 
{
	if( defaultValue && !defaultValue->IsErrorOperand() )
		delete defaultValue;
}


// задаем функцию, создаем список инициализации
ConstructorFunctionBody::ConstructorFunctionBody( const Function &pFn, const Position &ccPos )
	: FunctionBody(pFn, ccPos), oieList(new ObjectInitElementList)
{
}


// удалить список инициализации конструктора
ConstructorFunctionBody::~ConstructorFunctionBody()
{
	delete oieList;
}


// задать список инициализации конструктора
void ConstructorFunctionBody::SetConstructorInitList( const ObjectInitElementList &ol )
{
	oieList->assign(ol.begin(), ol.end());
}