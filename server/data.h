#ifndef data_H
#define data_H

#include <vector>
#include <set>
#include <random>
#include "record.h"
#include "error.h"
#include "thread_safe_map.h"
#include "hash.h"
#include "../lib/csv.h"


namespace DataBase {

	typedef std::vector<std::string> names_vector;
	template<typename Key, typename T> class data_iterator;

	// база данных параметризуется типом ключа Key и типом хранящегося элемента T
	template<typename Key, typename T>
	class data {

	public:
		typedef Key key_type;
		typedef T mapped_type;
		typedef std::pair<const Key, T> value_type;
		typedef size_t size_type;
		typedef DefaultHash<std::string> Hash;
		typedef std::vector<thread_safe_map<int, T>>   data_vector;

		// итератор контейнера.
		typedef data_iterator<key_type, mapped_type> const_iterator;
		const_iterator begin() const;
		const_iterator half()  const;
		const_iterator end()   const;

		// Класс итератора должен иметь доступ к защищенным 
		// членам класса data. 
		friend class data_iterator<key_type, mapped_type>;
				
		// конструктор базы данных, int 2^number_of_first_digits - размер внешнего вектора; int 2^number_of_second_digits - размер (максимальный) внутреннего ассоциативного массива
		explicit  data(int number_of_first_digits = 4, int number_of_second_digits = 6);

		~data() { }

		// генерация базы данных
		std::pair<unsigned long, unsigned long long> Generate(int num_records = 10, int num_threads = 4, int wait_time = 1000,
			const std::string& last_name_male_file    = "last_name_male.csv",
			const std::string& last_name_female_file  = "last_name_female.csv",
			const std::string& first_name_male_file   = "first_name_male.csv",
			const std::string& first_name_female_file = "first_name_female.csv",
			const std::string& patronymic_male_file   = "patronymic_male.csv",
			const std::string& patronymic_female_file = "patronymic_female.csv"
		);

		// сохранение базы данных на диск
		unsigned long Save(const unsigned int num_threads = 1, const std::string file_name = "data.csv", int wait_time = 1000);

		// загрузка базы данных с диска в память
		unsigned long Load(const unsigned int num_threads = 1, const std::string file_name = "data.csv", int wait_time = 1000);

		// очистка базы данных
		void Clear(unsigned  int num_threads = 1, int wait_time = 1000);

		// добавление записи в базу данных
		bool AddRecord(key_type& number, bool activity, mapped_type& rec, int wait_time = 1000);

		// удаление записи из базы данных
		bool DeleteRecord(key_type& number, int wait_time = 1000);

		// поиск записи в базе данных по номеру телефона
		bool FindRecord(const key_type& number, bool& activity, mapped_type& rec, const unsigned int wait_time = 1000);

		// чтение размера базы данных
		unsigned long Get_number_of_records(void)  const;
		unsigned long long Get_number_of_bytes(void) const;
		int Get_first_length(void)  const;

		// захват блокировки внешним кодом
		boost::shared_lock<boost::shared_mutex> GetLock(void);

		// отладочная функция: печать первых N записей базы данных активных и неактивных абонентов
		int Print(int N, int wait_time = 1000);
		
	private:
		// количество разрядов телефонного номера для внешнего вектора и внутреннего ассоциативного массива
		int number_of_first_digits, number_of_second_digits;

		std::atomic<unsigned long>      number_of_records; // количество записей в базе данных 
		std::atomic<unsigned long long> number_of_bytes;   // количество байт в базе данных 

		// векторы activ_users и inactiv_users содержат ассоциативные массивы для активных и неактивных абонентов
		data_vector activ_users;
		data_vector inactiv_users;
				
		// блокировка
		mutable boost::shared_mutex mutex;

		// условная переменная для синхронизации операций, требующих наличия записей в базе данных,
		// и операций, добавляющих записи в базу данных.
		std::condition_variable_any data_cond;

		// переменные для синхронизации потоков, читающих базу данных, и обращающихся к ней на запись
		std::atomic<unsigned int> count_of_read_operations;
		std::atomic<unsigned int> count_of_write_operations;

		names_vector v_last_name;
		names_vector v_name;
		names_vector v_patronymic;

		// установка размера базы данных
		void Set_number_of_records(unsigned long N);
		void Set_number_of_bytes(unsigned long long N);
		
		bool Empty();

		// Вспомогательный метод добавления записи в базу данных без захвата блокировки.
		bool AddRecord_no_block(const unsigned int& first_number, const unsigned int& second_number, const bool& activity, mapped_type& rec);

		// Вспомогательный метод удаления записи из базы данных без захвата блокировки.
		bool DeleteRecord_no_block(int first_number, int second_number);

		// однопоточный метод генерации базы данных.
		void GenerateOneThread(unsigned int count,
			unsigned int block_begin,
			unsigned int block_end,
			const names_vector& v_last_name_male,
			const names_vector& v_last_name_female,
			const names_vector& v_first_name_male,
			const names_vector& v_first_name_female,
			const names_vector& v_patronymic_male,
			const names_vector& v_patronymic_female);

		// однопоточный метод очистки базы данных.
		void ClearOneThread(unsigned int block_begin, unsigned int block_end);

		// однопоточный метод сохранения базы данных.
		unsigned long SaveOneThread(unsigned int block_begin, unsigned int block_end, const std::string file_name);

		// однопоточный метод загрузки базы данных.
		unsigned long LoadOneThread(const std::string file_name);
	
		// вспомогательный класс, изменяющий счетчик операций записи/чтения в конструкторе при создании объекта
		// и отменяющей данное изменении в деструкторе при уничтожении объекта
		class increase_count_of_operation {
			std::atomic<unsigned int>& count_of_operations;
		public:
			increase_count_of_operation(std::atomic<unsigned int>& count) :
				count_of_operations(count)
			{
				++count_of_operations;
			}

			~increase_count_of_operation()
			{
				--count_of_operations;
			}

		};

		public:
			Hash Hasher;   // объект хэш-функции

	};

	// ------------------------------------------------------------ //


	// Итератор базы данных data_iterator.
	// Предназначен для последовательного вывода элементов базы данных, в силу чего
	// базовым классом выбран однонаправленный итератор forward_iterator_tag.
	template<typename Key, typename T>
	class data_iterator :
		public std::iterator<std::forward_iterator_tag, std::pair<const int, T> > // наследуем итератор от итератора из шаблонного класса iterator_traits
	{
	public:
		data_iterator(); //конструктор по умолчанию
		data_iterator(unsigned int, bool, typename thread_safe_map<int, T>::const_iterator,
			const data<Key, T>*);

		// операторы разыменования итератора
		const std::pair<const int, T>& operator*()  const;
		const std::pair<const int, T>* operator->() const;

		// оператор инкрементирования
		data_iterator<Key, T>& operator++(); // префиксный инкремент

		// операторы сравнения
		bool operator==(const data_iterator& rhs) const;
		bool operator!=(const data_iterator& rhs) const;

		// возврат индекса массива
		const unsigned int  GetBucket() const;

		// возврат признака активности
		const bool GetActiv()  const;

	private:
		unsigned int mBucket;
		bool activ;
		typename thread_safe_map<int, T>::const_iterator it;
		const    data<Key, T>* ptr_vector;
	};

	// Вспомогательные функции:
	// Чтение базы данных имен, фамилий и отчеств из файла f в вектор v
	names_vector read_name_file(const std::string& file_name);

	// Функция из вектора случайным образом выбирает имя с заданным полом
	const std::string& generate_random_name(const names_vector& v, const std::string& sex);

} // namespace DataBase

#include "data.inl" // "C++ Cookbook", D.Ryan Stephens в главе 2.5 Including an inline File.


#endif
