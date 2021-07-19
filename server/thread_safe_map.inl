#ifndef THREAD_SAFE_MAP_INL
#define THREAD_SAFE_MAP_INL

#include "thread_safe_map.h"

namespace DataBase {

	// Поиск элемента в ассоциативном массиве;
	// при его отсутствии возвращается значение по умолчанию.
	// Операция защищена boost::shared_lock<boost::shared_mutex>, что позволяет
	// производить несколько операций поиска в массиве в многопоточном режиме.
	template<typename Key, typename T>
	const T&  thread_safe_map<Key, T>::find(key_type const& key, mapped_type const& default_value)
	{
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		typename std::map<key_type, mapped_type>::const_iterator found_entry = find(key);
		return (found_entry == data.end()) ?
			default_value : found_entry->second;
	}

	// Добавление элемента в ассоциативный массив под защитой std::lock_guard<>.
	// в случае его наличия в массиве - обновление значения.
	template<typename Key, typename T>
	bool thread_safe_map<Key, T>::add_or_update(key_type const& key, mapped_type const& value, unsigned int& old_size)
	{
		std::lock_guard<boost::shared_mutex> lock(mutex);

		// итератор указывает на искомый элемент, либо на элемент, следующий за конечным
		typename std::map<key_type, mapped_type>::iterator found_entry = find(key);

		if (found_entry == data.end()) // элемент не найден
		{
			data[key] = value;		   // помещение новых данных в ассоциативный массив
			old_size = 0;
			return true;
		}
		else						   // элемент найден
		{
			// вычисление размера старого элемента
			old_size = (unsigned int)(found_entry->second).last_name_size() + (unsigned int)(found_entry->second).first_name_size() + (unsigned int)(found_entry->second).patronymic_size();
			
			// замена старого элемента на новый
			found_entry->second = value; // предположительно при наличии действительного итератора так будет быстрее, чем через operator[]
			return false;
		}
	}

	// Добавление элемента в ассоциативный массив под защитой std::lock_guard<> без проверки на наличие элемента в нем.
	template<typename Key, typename T>
	void thread_safe_map<Key, T>::add(key_type const& key, mapped_type const& value)
	{
		std::lock_guard<boost::shared_mutex> lock(mutex);
		data[key] = value;		   // помещение новых данных в ассоциативный массив
	}

	// удаление элемента из ассоциативного массива под защитой std::lock_guard<>
	template<typename Key, typename T>
	bool thread_safe_map<Key, T>::erase(key_type const& key, unsigned int& old_size)
	{
		// монопольный захват мьютекса на запись
		std::lock_guard<boost::shared_mutex> lock(mutex);

		// получение итератора на удаляемый элемент
		typename std::map<key_type, mapped_type>::const_iterator found_entry = find(key);

		// если существует элемент, соответствующий заданному ключу, то осуществляется операция его удаления из массива
		if (found_entry != data.end())
		{
			// возврат размера удаляемого элемента
			old_size = (unsigned int)(found_entry->second).last_name_size() + (unsigned int)(found_entry->second).first_name_size() + (unsigned int)(found_entry->second).patronymic_size();
			
			// непосредсвенное удаление элемента
			data.erase(found_entry);
			return true;
		}
		else {
			old_size = 0;
			return false;
		}
	}

	// удаление всех элементов из ассоциативного массива под защитой std::lock_guard<>
	template<typename Key, typename T>
	void thread_safe_map<Key, T>::clear()
	{
		std::lock_guard<boost::shared_mutex> lock(mutex);
		data.clear();
	}

	// вывод элементов ассоциативного массива в поток
	template<typename Key, typename T>
	unsigned long thread_safe_map<Key, T>::print(std::ostream& stream, int first_number, bool activ, int number_of_first_digits, int number_of_second_digits) {
		
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		
		unsigned long count = 0; // счетчик выведенных в поток элементов
		auto it = data.begin();  // установка итератора на начало массива
		
		std::string s_first, s_second; // вспомогательные буферы для строкового представления первой и второй половины телефонного номера
		
		// цикл по всем элементам в массиве
		while (it != data.end()) {
			
			// первая и вторая половина телефонного номера	
			s_first  = std::to_string(first_number);
			s_second = std::to_string(it->first);
		
			// дополнение строк нулями до требуемого размера телефонного номера
			std::string add_first(number_of_first_digits - s_first.size(), '0');
			std::string add_second(number_of_second_digits - s_second.size(), '0');

			// результаты тестирования вывода в поток https://stackoverflow.com/questions/1924530/mixing-cout-and-printf-for-faster-output
			stream << "8" + add_first + s_first
				+ add_second + s_second + ", "		   // запись в поток номера телефона
				+ it->second.get_last_name() + ", "    // запись в поток фамилии
				+ it->second.get_first_name() + ", "   // запись в поток имени
				+ it->second.get_patronymic() + ", "   // запись в поток отчества
				+ ((activ) ? "1" : "0") + "\n";		   // запись в поток признака активности
			
			++it;
			++count;
		}
	
		return count;
	}
	
	// метод, устанавливающий итератор на начало ассоциативного массива
	template <typename Key, typename T>
	typename thread_safe_map<Key, T>::const_iterator
		thread_safe_map<Key, T>::begin() const
	{
		if (data.size() == 0) {
			// Специальный случай: элементы отсутствуют, поэтому 
			// возвращается конечный итератор. 
			return (map_iterator<Key, T>(data.cend(), this));
		}

		// Здесь существует по крайней мере один элемент.  
		return (map_iterator<Key, T>(data.cbegin(), this));
	}

	// метод, устанавливающий итератор на конец ассоциативного массива
	template <typename Key, typename T>
	typename thread_safe_map<Key, T>::const_iterator
		thread_safe_map<Key, T>::end() const
	{
		return (map_iterator<Key, T>(data.cend(), this));
	}

	// ----------------------------------------------------------------- //
	// Итераторные методы: создание, сравнение, разименование итераторов //

	template<typename Key, typename T>
	map_iterator<Key, T>::map_iterator()
	{
		it = std::map<Key, T>::const_iterator();
		ptr_map = NULL;
	}

	template<typename Key, typename T>
	map_iterator<Key, T>::map_iterator(typename std::map<Key, T>::const_iterator in_iterator,
		const thread_safe_map<Key, T>* in_ptr_map) :
		it(in_iterator), ptr_map(in_ptr_map)
	{

	}

	template<typename Key, typename T>
	const std::pair<const Key, T>&
		map_iterator<Key, T>::operator*() const
	{
		return (*it);
	}

	template<typename Key, typename T>
	const std::pair<const Key, T>*
		map_iterator<Key, T>::operator->() const
	{
		return (&(*it));
	}

	template<typename Key, typename T>
	map_iterator<Key, T>&
		map_iterator<Key, T>::operator++()
	{
		++it;
		return (*this);
	}

	template<typename Key, typename T>
	bool map_iterator<Key, T>::operator==(
		const map_iterator& rhs) const
	{
		// Все поля, на которые ссылаются итераторы, должны быть равны. 
		return (ptr_map == rhs.ptr_map && it == rhs.it);
	}

	template<typename Key, typename T>
	bool map_iterator<Key, T>::operator!=(const map_iterator& rhs) const
	{
		return (!operator== (rhs));
	}
}

#endif // THREAD_SAFE_MAP_INL
