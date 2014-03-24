// заголовочный файл для всех функций и классов


// размер таблицы символов по умолчанию
#define DEFAULT_SIZE	531


// класс реализует хеш таблицу для переменных, функций, пространства имен
// для типа T
template <class T, int size>
class HashTab
{
	// сама таблица в которой храняться данные
	list<T> table[size];

protected:
	// хеш-функция, возвращает списков в котором находиться key
	list<T> &HashFunc( char *name ) {
		register char *p;
		unsigned int h = 0, g;

		for(p = name; *p != '\0'; p++)
			if(g = (h = (h << 4) + *p) & 0xF0000000)
				h ^= g >> 24 ^ g;
		
		return table[h % size];
	}

public:
	
	HashTab( ) { }
	~HashTab( ) {  }
};
