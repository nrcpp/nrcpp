// NoRules Compiler Library 
// заголовочный файл, декларирует возможности библиотеки - NRC.H


#ifndef _NRC_H
#define _NRC_H

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;


// все функциональные возможности библиотеки находятся в прострастве имен NRC
namespace NRC
{
	// объявляется далее
	class CharString;
	class StringList;


	// базовый класс для всех классов библиотеки
	class SObject
	{
	public:
		// единственный метод класса - возвращает имя самого класса
		virtual CharString Name();

		// виртуальный деструктор для уничтожения объектов внизу иерархии
		virtual ~SObject( ) {	
		}
	};


	// шаблонный класс, для преобразования любого типа в тип,
	// производный от SObject
	template <class T>
	class TypeToSObject : public SObject
	{
		// значение ассоциируемое с объектом
		T val;

	public:
		// конструктор с заданием значения
		TypeToSObject( T v = T() ) {
			val = v;
		}

		// автоматическое приведение к типу объекта
		operator T&() { return val; }

		// единственный метод класса - возвращает имя самого класса
		virtual CharString Name() {
			return CharString("NRC::TypeToSObject<T>");
		}
	};

	
	// объявляем список базовых типов производных от SObject
	typedef TypeToSObject<char> SObjectCHAR;
	typedef TypeToSObject<int> SObjectINT;
	typedef TypeToSObject<float> SObjectFLOAT;
	typedef TypeToSObject<double> SObjectDOUBLE;


	// встроенная функция для получения значения от объекта типа TypeToSObject
	template <class T>
	inline T &GetValueFromSObject( SObject *p ) {
		return (T&)*(TypeToSObject<T> *)p;
	}


	// класс символьных строк, расширяет класс строк из стандартной библиотеки
	class CharString : public SObject, public string
	{
		// можно задать объект, ассоциирующийся со строкой
		SObject *assocObj;

	public:
		// конструктор по умолчанию
		CharString( SObject *aobj = NULL ) { assocObj = aobj; }

		// конструктор принимает в параметре указатель на строку
		CharString( const char *s, SObject *aobj = NULL ) : string(s) {
			assocObj = aobj;
		}

		// конструктор преобразует число в строку
		CharString( int d, SObject *aobj = NULL ) {
			assocObj = aobj;
			*this = d;			
		}

		// конструктор преобразует вещественное число в строку
		CharString( double f, SObject *aobj = NULL ) {
			assocObj = aobj;
			*this = f;
		}

		// возвращает имя самого класса
		virtual CharString Name();

		// получить объект ассоциируемый со строкой
		NRC::SObject *GetStringObject() { return assocObj; }

		// удаляет передние и задние пробелы
		void Trim();

		// удаляет передние пробелы
		void TrimLeft();

		// удаляет задние пробелы
		void TrimRight();

		// форматирует строку и записывает ее в буфер
		void Format( const char *s, ... );

		// перевести строку в нижний регистр 
		void LowerCase();

		// перевести строку в верхний регистр
		void UpperCase();

		// перевернуть строку
		void Reverse();

		// сравнить две строки без учета регистра, но с учетом локальных установок
		bool CompareNoCase( const char *s );

		// удалить из строки все символы находящиеся в наборе 'set'
		void DeleteInSet( const char *set );

		// удалить символы с начала до того как появится символ из набора
		// вернуть удаленную строку
		CharString DeleteLeftWhileNot( const char *set );

		// удалить символы с конца до того как появится символ из набора
		// вернуть удаленную строку
		CharString DeleteRightWhileNot( const char *set );		

		// заменить символы из набора inset, символами из набора outset
		// причем каждому символу из inset должен соответствовать символ
		// из outset, если длинна inset меньше, то символы заменяются
		// defchar
		void ChangeInSet( const char *inset, const char *outset, char defchar = ' ' );

		// оставить в строке только символы из набора
		void KeepInSet( const char *set );

		// перегруженный метод, оставляет в строке символы, для которых
		// chkfunc возвращает ненулевое значение
		void KeepInSet( int (*chkfunc)(int c) );

		// вернуть указатель на буфер
		operator const char *();

		// преобразовать целое число в строку и записать
		CharString &operator=( int d );

		// преобразовать вещественное число в строку и заменить
		CharString &operator=( double f );
	};


	// список строк
	class StringList : protected vector<CharString *>, public SObject
	{
		// функция сравнения для строк по умолчанию, для поиска
		static bool EqualCmpF(const CharString &s1, const CharString &s2) {			
			return s1 == s2;
		}

		// функция сравнения для строк по умолчанию, для сортировки
		static bool EqualCmpS(const CharString *s1, const CharString *s2) {			
			return strcmp(s1->c_str(), s2->c_str()) < 0;
		}


	public:
		// конструктор по умолчанию
		StringList() { 
		}

		// конструктор копирования
		StringList( const StringList &sl ) {
			this->operator=( sl );
		}

		// деструктор освобождает отведенную под строки память
		~StringList() { Clear(); }

		// возвращает имя самого класса
		virtual CharString Name();

		// добавить строку в конец списка
		void Append( CharString &s );

		// удалить строку в позиции N
		void Delete( int n );

		// очистить весь список
		void Clear();

		// загрузить строки из файла
		// если загрузка не удалась - вернуть false
		bool LoadFromFile( const char *fname );

		// сохранить строки в файле
		// если сохранение не удалось - вернуть false
		bool SaveToFile( const char *fname );

		// найти строку, которая бы удовлетворяла функции сравнения 'Cmp'
		int Find( const CharString &s, 
			bool (*fcmp)(const CharString &, const CharString&) = StringList::EqualCmpF,
			int startPos = 0 ) const;
		
		// сортировка строк
		void Sort( 
			bool (*fcmp)(const CharString *, const CharString*) = StringList::EqualCmpS );
		

		// вернуть количество строк
		int Count() const { return size(); }

		// вернуть список строк в виде буфера
		CharString Text();

		// разделить строку на части, через символ-разделитель
		// и загрузить ее в список строк. При этом предыдущее состояние
		// списка стирается. Строка заключенная в двойные или одинарные кавычки
		// считается одним словом, независимо от разделителя
		void DelimText( const CharString &s, char delim = ';' );

		// оператор доступа к строкам
		CharString &operator[]( int ix );

		// const
		const CharString &operator[]( int ix ) const;		

		// скопировать список строк
		const StringList &operator=(  const StringList &sl );  
	};

	// интеллектуальные указатели
	template <class T>
	class SmartPtr
	{
		// счетчик ссылок на указатель
		int *refCount;

		// сам указатель
		T *ptr;

	public:  

		// конструктор должен принимать указатель на объект
		SmartPtr( T *p ) : ptr(p) {
			refCount = new int(1);
		}

		// конструктор копирования, увеличивает счетчик ссылок
		// на указатель
		SmartPtr( const SmartPtr &sptr ) {
			ptr = sptr.ptr;
			refCount = sptr.refCount;
			*refCount += 1;
		}

		// уничтожение объекта
		~SmartPtr() {
			// если указатель освобожден
			if( *refCount < 0 )	
			{
				if( !++*refCount )
					delete refCount ;
				return;
			}

			*refCount -= 1;
			if( *refCount == 0 )
			{
				delete refCount;
				delete ptr;
			}			
		}

		// если указатель - нулевой
		bool IsNull() const {
			return ptr == NULL;
		}

		// вернуть количество ссылок на этот указатель
		int GetReferenceCount() const {
			return *refCount;
		}

		// освободить указатель
		T *Release() {
			*refCount = -*refCount;		// признак того, что указатель освобожден
			T *t = ptr;
			ptr = NULL;
			return t;
		}

		// операция копирования, увеличивает счетчик указателей
		SmartPtr &operator=( const SmartPtr &sptr ) {			
			if( &sptr == this )
				return *this;

			// если присваивается особожденный указатель,
			// это считается ошибкой
			if( *sptr.refCount < 0 )
				throw "'SmartPtr<T>::operator=' присвоение освобожденного указателя";

			// если указатель освобожден
			if( *refCount < 0 )	
			{
				if( !++*refCount )
					delete refCount ;				
			}

			// иначе указатель владеет объектом
			else
			{
				*refCount -= 1;
				if( *refCount == 0 )
				{
					delete refCount;
					delete ptr;
				}
			}

			ptr = sptr.ptr;
			refCount = sptr.refCount;
			*refCount += 1;
			return *this;
		}


		// разименование указателя	(если указатель нулевой, этот метод вызывать нельзя)
		T &operator * () {
			return *ptr;
		}

		// разименование указателя (константный метод) 
		const T &operator * () const {
			return *ptr;
		}


		// доступ к членам указателя (если указатель нулевой, этот метод вызывать нельзя)
		T *operator ->() {
			return ptr;
		}		

		// доступ к членам указателя (константный метод)
		const T *operator ->() const {
			return ptr;
		}				
	};
}


#endif			// _NRC_H_
