#ifndef THREAD_SAFE_MAP_H
#define THREAD_SAFE_MAP_H

#include <map>
#include <mutex>
#include <boost/thread.hpp>

namespace DataBase {

	// Потокобезопасная обертка для std::map (с крупногранулярными блокировками).
	// Данная реализация увеличивает уровень параллелизма за счет того,
	// что используется boost::shared_mutex, который позволяет нескольким потокам
	// одновременно читать ассоциативный массив.
	/*
	*  Реализованы следующие операции:
	*  - поиск элемента в ассоциативном массиве;
	*  - добавление и изменение элемента в ассоциативный массив;
	*  - удаление элемента из ассоциативного массива;
	*  - удаление всех элементов из ассоциативного массива;
	*  - получение размера ассоциативного массива;
	*  - вывод элементов ассоциативного массива в поток.
	*/

	// итератор ассоциативного массива (непотокобезопасный)
	template<typename Key, typename T> class map_iterator;

	// потокобезопасный массив по аналогии с std::map параметризован типами ключа и хранящегося элемента
	template<typename Key, typename T>
	class thread_safe_map
	{
	public:
		typedef Key key_type;
		typedef T mapped_type;
		typedef std::pair<const Key, T> value_type;
		typedef size_t size_type;

		// Итератор данного контейнера используется только для чтения его элементов,
		// поэтому реализован однонаправленный константный итератор.
		// Итератор не потокобезопасный, поэтому для его корректного использования необходимо
		// осуществлять синхронизацию операций кодом верхнего уровня.
		// (Уильямс, Параллельное программирование на C++ в действии, стр. 246)
		typedef map_iterator<Key, T> const_iterator;
		const_iterator begin() const;
		const_iterator end() const;

		// Класс итератора должен иметь доступ к защищенным 
		// членам класса thread_safe_map. 
		friend class map_iterator<Key, T>;

		// конструктор копирования и оператор присваивания не используется
		thread_safe_map&
			operator=(const thread_safe_map&) = delete;
		thread_safe_map(const thread_safe_map& other) = delete;

		thread_safe_map() {}
		~thread_safe_map() {}

		// Поиск элемента в ассоциативном массиве.
		// Метод возвращает значение, соответствующее значению ключа key; в противном случае
		// при отсутствии элемента возвращает значение по умолчанию default_value.
		const mapped_type& find(key_type const& key, mapped_type const& default_value);

		// Метод добавления элементов в ассоциативный массив. В случае наличия элемента в 
		// массиве происходт обновление элемента, при этом размер старого элемента возвращается
		// в переменной old_size.
		bool add_or_update(key_type const& key, mapped_type const& value, unsigned int& old_size);

		// Метод добавления элементов в массив без проверки на наличие в массиве, соответсвующего ключу.
		void add(key_type const& key, mapped_type const& value);

		// Вывод элемента массива в поток
		unsigned long print(std::ostream& stream, int first_number, bool activ, int number_of_first_digits, int number_of_second_digits);

		// Метод удаляет элемент с ключом key, если таковой существует. В случае успешного удаления элемента возвращает true.
		bool erase(key_type const& key, unsigned int& old_size);

		// Метод удаления всех элементов массива. 
		void clear();

		// Проверка на наличие элементов в массиве.
		bool empty() const { boost::shared_lock<boost::shared_mutex> lock(mutex); return data.empty(); }

	private:

		// оборачиваемый ассоциативный массив std::map<>
		std::map<key_type, mapped_type> data;

		// доступ к массиву под защитой мьютекса
		mutable boost::shared_mutex mutex;

		// вспомогательная функция для поиска элемента
		typename std::map<key_type, mapped_type>::iterator find(Key const& key)
		{
			return std::find_if(data.begin(), data.end(),
				[&](value_type const& item) {return item.first == key; });
		}

	};


	// Класс map_iterator. Итератор не являтся потокобезопасным
	template<typename Key, typename T>
	class map_iterator :
		public std::iterator<std::forward_iterator_tag, std::pair<const Key, T> > // наследуем итератор от итератора из шаблонного класса iterator_traits
	{
	public:
		map_iterator(); //конструктор по умолчанию
		map_iterator(typename std::map<Key, T>::const_iterator,
			const thread_safe_map<Key, T>*);

		// Стандартное поведение, заданное базовым классом итератора, полностью соответствует
		// поведению итератора map_iterator, поэтому необязательно переопределять 
		// конструктор копии, operator=(), а также и деструктор.

		// операторы разыменования итератора
		const std::pair<const Key, T>& operator*()  const;
		const std::pair<const Key, T>* operator->() const;

		// оператор инкрементирования
		map_iterator<Key, T>& operator++(); // префиксный инкремент

		// операторы сравнения
		bool operator==(const map_iterator& rhs) const;
		bool operator!=(const map_iterator& rhs) const;

	private:
		typename std::map<Key, T>::const_iterator it;
		const    thread_safe_map<Key, T>* ptr_map;
	};

}

#include "thread_safe_map.inl" // "C++ Cookbook", D.Ryan Stephens в главе 2.5 Including an inline File.

#endif
