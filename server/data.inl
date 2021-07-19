#ifndef data_INL
#define data_INL

#include "data.h"

namespace DataBase {

	template<typename Key, typename T>
	data<Key, T>::data(int L_ex, int L_in)
		try :
		number_of_first_digits(L_ex), number_of_second_digits(L_in), number_of_records(0), number_of_bytes(0),
		activ_users(int(pow(10, L_ex))), inactiv_users(int(pow(10, L_ex))),
		count_of_read_operations(0), count_of_write_operations(0),
		Hasher(L_ex, L_in)
	{
		if (L_ex + L_in != 10 || L_ex < 1 || L_ex > 9) // размеры вектора и ассоциативного массива должны быть согласованы
			throw std::invalid_argument("DataBase: the sum of the parameters L_ex and L_in should be equal to 10");
	}
	catch (std::bad_alloc) {
		// Ошибка выделения памяти. Исключение должно быть обработано кодом более высокого уровня.
		throw;
	}

	// Генерация базы данных.
	template<typename Key, typename T>
	std::pair<unsigned long, unsigned long long> data<Key, T>::Generate(int num_records, int num_threads, int wait_time,
	 	const std::string& last_name_male_file,  const std::string& last_name_female_file,
		const std::string& first_name_male_file, const std::string& first_name_female_file,
		const std::string& patronymic_male_file, const std::string& patronymic_female_file)
	{
		// База данных отсутствует в памяти. Любые операции чтения, сохранения и модификации
		// во время создания записей не имеют смысла.
		// База данных блокируется для доступа со стороны других потоков до полного завершения операции генерирования.
		std::unique_lock<boost::shared_mutex> lock(mutex);
		
		// Проверка на наличие базы данных в памяти. Генерируется исключение, если база данных ранее была сгенерирована
		// и не произвелась ее очистка в течении времени ожидания
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return Empty(); }) == false)
			throw SequenceError("The database has already been generated.");
				
		// Чтение имен, фамилий и отчеств из баз данных "first_name.csv", "last_name.csv", "patronymic.csv".
		// Объем этих файлов на диске имеет относительно небольшой размер, 
		// поэтому считываем эти данные в память. Так как к данным потребуется большое
		// количество обращений на чтение произвольного элемента, то данные помещаем в вектор.
		names_vector v_last_name_male;
		names_vector v_last_name_female;
		names_vector v_first_name_male;
		names_vector v_first_name_female;
		names_vector v_patronymic_male;
		names_vector v_patronymic_female;

		v_last_name_male   = read_name_file(last_name_male_file);
		v_last_name_female = read_name_file(last_name_female_file);

		v_first_name_male   = read_name_file(first_name_male_file);
		v_first_name_female = read_name_file(first_name_female_file);
		
		v_patronymic_male   = read_name_file(patronymic_male_file);
		v_patronymic_female = read_name_file(patronymic_female_file);

		// Массив будущих результатов используется для фиксации исключений.
		std::vector<std::future<void> > futures(num_threads - 1);

		unsigned int block_size = static_cast<unsigned int>(pow(10, number_of_first_digits) / num_threads); // определение количества блоков, обрабатываемого в одном потоке.
		unsigned long count = num_records / num_threads; // число записей, генерируемых в одном потоке.
		if (block_size == 0) // если количество блоков меньше числа потоков, избыточные потоки не запускаются
			block_size = num_threads - 1;

		// начальный и конечный индексы массивов, в которых осуществляется однопоточная генерация
		unsigned int block_begin = 0;
		unsigned int block_end   = 0;

		// генерация num_records записей типа record в num_threads потоках
		// и размещение их в памяти в массивах activ_users и inactiv_users.
		for (int i = 0; i < (num_threads - 1); ++i) {
			block_end += block_size;
			futures[i] = std::async(std::launch::async, &data::GenerateOneThread, this, 
						count, block_begin, block_end,
						std::ref(v_last_name_male),  std::ref(v_last_name_female),
						std::ref(v_first_name_male), std::ref(v_first_name_female),
						std::ref(v_patronymic_male), std::ref(v_patronymic_female));
			block_begin = block_end;
		}

		unsigned int d = (num_records - static_cast<unsigned int>(num_records/num_threads)*num_threads);

		// генерация оставшихся записей в главном потоке
		data::GenerateOneThread(count + d, block_begin, (unsigned int)pow(10, number_of_first_digits),
			std::ref(v_last_name_male),
			std::ref(v_last_name_female),
			std::ref(v_first_name_male),
			std::ref(v_first_name_female),
			std::ref(v_patronymic_male),
			std::ref(v_patronymic_female));

		// ожидание завершения работы потоков.
		// возникшие исключения сохраняются в массиве futures[i].
		for (int i = 0; i < (num_threads - 1); ++i)
		{
			futures[i].get();
		}

		// генерация базы данных завершена

		unsigned long count_records    = number_of_records.load();
		unsigned long long count_bytes = number_of_bytes.load();

		// уведомление ожидающим потокам
		lock.unlock();
		data_cond.notify_one();
		
		return std::make_pair(count_records, count_bytes);
	}

	// Сохранение базы данных в файл
	template<typename Key, typename T>
	unsigned long data<Key, T>::Save(const unsigned int num_threads, const std::string file_name, int wait_time)
	{
		// При сохранении базы данных доступ к ней осуществляется только на чтение, поэтому
		// имеется возможность выполнить данную операцию в несколько потоков. Однако при записи 
		// в один файл (буфер) данную операцию различным потокам необходимо выполнять синхронно.
		// Поэтому операция сохранения базы данных исполняется одним потоком.

		// Ожидание завершения блокирующих операций с базой данных (например, ее генерация)
		// и осуществление вывода ее записей в файл. Используется boost::shared_lock<boost::shared_mutex>,
		// поэтому доступ на чтение со стороны других запросов не блокируется.
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		
		// Ожидание готовности записей в базы данных в течении wait_time мс. 
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return !( Empty()); }) == false) 
			throw SequenceError("The database is not in memory.");
		
		// увеличение счетчика операций чтения - блокируется запуск новых операций на запись (например AddRecord).
		increase_count_of_operation inc(count_of_read_operations);

		// Ожидание завершения операций записи в базу данных (например, AddRecord) в течении wait_time мс.
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_write_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		// Массив будущих результатов используется для передачи количества сохраненых элементов в основной поток и фиксации исключений.
		std::vector<std::future<unsigned long> > futures(num_threads - 1);

		unsigned int block_size = static_cast<unsigned int>(pow(10, number_of_first_digits) / num_threads); // определение количества блоков, обрабатываемого в одном потоке.
		
		if (block_size == 0) // если количество блоков меньше числа потоков, избыточные потоки не запускаются
			block_size = num_threads - 1;

		// на каждом блоке ассоциативных массивов запускается однопоточная задача сохранения базы данных
		unsigned int block_begin = 0;
		unsigned int block_end = 0;
		std::string file = file_name;
		unsigned int i = 0;
		for (; i < (num_threads - 1); ++i) {
			block_end += block_size;
			file.insert(file.size() - 4, std::to_string(i)); // добавление префикса к имени сохраняемого файла
			futures[i] = std::async(std::launch::async, &data::SaveOneThread, this,
				block_begin, block_end, file);
			block_begin = block_end;
			file = file_name;
		}

		file.insert(file.size() - 4, std::to_string(i)); // добавление префикса к имени сохраняемого файла
		unsigned long count = data::SaveOneThread(block_begin, (unsigned int)pow(10, number_of_first_digits), file);

		// ожидание завершения работы потоков.
		// возникшие исключения сохранены в массиве futures[i].
		for (unsigned int i = 0; i < (num_threads - 1); ++i)
		{
			count += futures[i].get();
		}

		// сохранение базы данных завершено;
		// уведомление ожидающим потокам
		lock.unlock();
		data_cond.notify_one();

		return count;
	}

	// Загрузка базы данных из файла
	template<typename Key, typename T>
	unsigned long data<Key, T>::Load(const unsigned int num_threads, const std::string file_name, int wait_time)
	{
		// Ожидание завершения блокирующих операций с базой данных (например, удаление записей базы)
		// и осуществление чтения записей из файла. Используется std::unique_lock<boost::shared_mutex>,
		// поэтому доступ к базе данных на время ее загрузки из файла блокируется со стороны других запросов.
		std::unique_lock<boost::shared_mutex> lock(mutex);

		// ожидание данных в течении wait_time мс с проверкой отсутсвия базы данных в памяти. 
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return Empty(); }) == false)
			throw SequenceError("The database is already in memory. The database must be out of memory before loading.");

		// увеличение счетчика операций чтения - блокируется запуск новых операций на запись (например AddRecord).
		increase_count_of_operation inc(count_of_write_operations);

		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_read_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		// Массив будущих результатов используется для фиксации исключений.
		std::vector<std::future<unsigned long> > futures(num_threads - 1);

		std::string file = file_name;
		unsigned int i = 0;
		for (; i < (num_threads - 1); ++i) {
			file.insert(file.size() - 4, std::to_string(i)); // добавление префикса к имени сохраняемого файла
			futures[i] = std::async(std::launch::async, &data::LoadOneThread, this, file);
			file = file_name;
		}

		file.insert(file.size() - 4, std::to_string(i)); // добавление префикса к имени сохраняемого файла
		unsigned long count = data::LoadOneThread(file);

		// ожидание завершения работы потоков.
		// возникшие исключения сохраняются в массиве futures[i].
		for (unsigned int i = 0; i < (num_threads - 1); ++i)
		{
			count += futures[i].get();
		}

		// Загрузка базы данных из файла завершена, посылается уведомление ожидающим потокам.
		lock.unlock();
		data_cond.notify_one();
		
		return count;
	}
		
	// Очистка базы данных
	template<typename Key, typename T>
	void data<Key, T>::Clear(unsigned int num_threads, int wait_time)
	{
		// Ожидание завершения блокирующих операций с базой данных (например, загрузка записей базы данных из файла)
		// и осуществление очистка записей базы данных. Используется std::unique_lock<boost::shared_mutex>,
		// поэтому доступ к базе данных на время очистки блокируется со стороны других потоков.
		std::unique_lock<boost::shared_mutex> lock(mutex);

		// ожидание данных в течении wait_time мс. при отсутствии данных в течении времени ожидания возвращение управления.
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return !(Empty()); }) == false)
			throw SequenceError("The database is not in memory.");
		
		// Массив будущих результатов используется для фиксации исключений.
		std::vector<std::future<void> > futures(num_threads - 1);

		unsigned int block_size = static_cast<unsigned int>( pow(10, number_of_first_digits) / num_threads); // определение количества блоков, обрабатываемого в одном потоке.

		unsigned int block_begin = 0;
		unsigned int block_end = 0;

		for (unsigned int i = 0; i < (num_threads - 1); ++i) {
			block_end += block_size;

			futures[i] = std::async(std::launch::async, &data::ClearOneThread, this,
				block_begin, block_end);

			block_begin = block_end;
		}

		// очищение оставшихся массивов
		while (block_begin != pow(10, number_of_first_digits)) {
			activ_users[block_begin].clear();
			inactiv_users[block_begin].clear();
			++block_begin;
		}

		// ожидание завершения работы потоков.
		// возникшие исключения сохраняются в массиве futures[i].
		for (unsigned int i = 0; i < (num_threads - 1); ++i)
		{
			futures[i].get();
		}

		Set_number_of_records(0);
		Set_number_of_bytes(0);

		// Очистка базы данных завершена, посылается уведомление ожидающим потокам.
		lock.unlock();
		data_cond.notify_one();
	}

	// Добавление записи в базу данных
	template<typename Key, typename T>
	bool data<Key, T>::AddRecord(key_type& number, bool activity, T& rec, int wait_time) {

		// Предполагается, что одному индексу соответствует только один абонент.
		// В противном случае вместо контейнера map следовало бы выбрать std::multimap (или в map<> помещать list<record>).
		// Также предполагается наличие абонента с одним номером только в одном из ассоциативном массиве: либо в activ_users, либо в inactiv_users.

		// Запись добавляется в потокобезопасный массив, поэтому ее осуществление возможно из нескольких потоков без
		// захвата мьютекса. Для синхронизации с блокирующими операциями доступа к базе данных (например, при ее генерации)
		// используется блокировка boost::shared_lock<>. При этом возможны многопоточные операции добавления записи в базу данных AddRecord.
		// Одновременный доступ со стороны читающих операций (таких как Save), которые также защищены блокировкой boost::shared_lock<> необходимо
		// запретить, так как в процессе исполнения операции AddRecord могут нарушаться инварианты базы данных. Для этого 
		// используются два счетчика операций записи/чтения count_of_write_operations и count_of_read_operations. При этом
		// все операции записи (защищенные boost::shared_lock<>) не начинают свое выполнение до завершения всех
		// операций чтения (также защищенных boost::shared_lock<>), и наоборот.

		boost::shared_lock<boost::shared_mutex> lock(mutex);

		// увеличение счетчика операций записи - блокируется запуск новых операций на чтение, защищенных
		// блокировкой boost::shared_lock<> (например Save).
		increase_count_of_operation inc(count_of_write_operations);

		// Ожидание завершения операций чтения в базу данных (Save) в течении wait_time мс. 
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_read_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		std::pair<int, int> P = Hasher.hash(number);

		// произведен захват внешней блокировки - возможно применении асинхронной функции AddRecord_no_block
		bool success = AddRecord_no_block(P.first, P.second, activity, rec);

		// операция добавления завершена, уменьшении счетчика записывающих операций
		// и уведомление ожидающих потоков.
		lock.unlock();
		data_cond.notify_one();
		return success;
	}

	// Удаление записи из базы данных
	template<typename Key, typename T>
	bool data<Key, T>::DeleteRecord(key_type& number, int wait_time) {
				
		boost::shared_lock<boost::shared_mutex> lock(mutex);

		// увеличение счетчика операций записи - блокируется запуск новых операций на чтение, защищенных
		// блокировкой boost::shared_lock<> (например Save).
		increase_count_of_operation inc(count_of_write_operations);

		// Ожидание завершения операций чтения в базу данных (например Save) в течении wait_time мс. 
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_read_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		// преобразование строкового представления номера в два целых числа
		std::pair<int, int> P = Hasher.hash(number);

		// произведен захват внешней блокировки - возможно применении асинхронной функции AddRecord_no_block
		bool success = DeleteRecord_no_block(P.first, P.second);

		// операция добавления завершена, уменьшении счетчика записывающих операций
		// и уведомление ожидающих потоков.
		lock.unlock();
		data_cond.notify_one();

		return success;
	}

	// Поиск записи в базе данных по телефонному номеру
	template<typename Key, typename T>
	bool data<Key, T>::FindRecord(const key_type& number, bool& activity, T& rec, const unsigned int wait_time) {
				
		boost::shared_lock<boost::shared_mutex> lock(mutex);

		// Ожидание появления в течении wait_time мс записей в базе данных.
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return !(Empty()); }) == false)
			throw SequenceError("The database is empty.");

		// увеличение счетчика операций чтения - блокируется запуск новых операций на запись (например AddRecord).
		increase_count_of_operation inc(count_of_read_operations);

		// ожидание завершения операций записи, защищенных блокировкой boost::shared_lock<>.
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_write_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		//преобразование номер абонента в два индекса first_number и second_number
		std::pair<int, int> P = Hasher.hash(number);
		int first_number = P.first;
		int second_number = P.second;

		// значение по умолчанию для возврата из функции поиска в случае отсутствия найденного значения
		record default_value = { "", "", "" };
		
		// поиск элемента базы данных сначала в массиве с активными абонентами, затем в массиве с неактивными абонентами
		if ((rec = (activ_users[first_number]).find(second_number, default_value)) != default_value) {
			activity = true; // установка признака активности найденного абонента
			return true;
		}
		else if ((rec = (inactiv_users[first_number]).find(second_number, default_value)) != default_value) {
			activity = false; // установка признака активности найденного абонента
			return true;
		}
		
		// операция добавления завершена, уменьшении счетчика записывающих операций
		// и уведомление ожидающих потоков.
		lock.unlock();
		data_cond.notify_one();

		return false; // запись не найдена
	}

	// Вывод N первых записей базы данных в консоль. (Вспомогательная отладочная функция)
	template<typename Key, typename T>
	int data<Key, T>::Print(int N, int wait_time)
	{
		// Ожидание завершения блокирующих операций с базой данных (например, ее генерация)
		// и осуществление вывода ее записей в консоль. Используется boost::shared_lock<boost::shared_mutex>,
		// поэтому доступ на чтение со стороны других потоков не блокируется.
		boost::shared_lock<boost::shared_mutex> lock(mutex);

		// Ожидание появления записей в базе данных в течении wait_time мс.
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return !(Empty()); }) == false)
			throw SequenceError("The database has no records.");

		// увеличение счетчика операций чтения - блокируется запуск новых операций на запись (например AddRecord).
		increase_count_of_operation inc(count_of_read_operations);

		// ожидание завершения операций записи
		if (data_cond.wait_for(lock, std::chrono::milliseconds(wait_time), [&] {return count_of_write_operations == 0; }) == false)
			throw WaitTimeError("Timeout exceeded. Write operations in progress.");

		int count = 0;
		bool a = true; // признак активности абонента

		auto it_ex_end = activ_users.end();
		auto it_ex_beg = activ_users.begin();
		auto it_ex = activ_users.begin();
		auto it_in_end = activ_users.begin()->end();
		auto it_in = activ_users.begin()->begin();

		for (int n = 0; n < 2; ++n) {  // всего две итерации: выбираем вектор, содержащий либо активных, либо неактивных абонентов
			int num = 0;
			while (it_ex != it_ex_end && num < N) {  // цикл по вектору, содержащих активных или неактивных абонентов

				// Для каждого элемента вектора получили итератор на 
				// соответствующий ассоциативный массив и выводим все его элементы в файл.
				it_in = it_ex->begin();
				it_in_end = it_ex->end();

				while (it_in != it_in_end && num < N) {   // цикл по каждому ассоциативному массиву 
					auto ind = std::distance(it_ex_beg, it_ex);       // расстояние между итераторами it_ex_beg и it_ex соответствует индексу текущего элемента, т.е. первой части телефонного номера
					std::cout << "8" << int_to_string(ind, number_of_first_digits) <<
						int_to_string(it_in->first, number_of_second_digits) << ", ";  // вывод номера телефона
						std::cout << it_in->second.last_name << ", ";				   // вывод фамилии
						std::cout << it_in->second.first_name << ", ";				   // вывод имени
						std::cout << it_in->second.patronymic << ", ";				   // вывод отчества
						std::cout << a << std::endl;								   // вывод признака активности
					++it_in;
					++num;
					++count;
				}
				// После завершения вывода в файл всех записей, содержащихся в одном ассоциативном массиве,
				// переходим к следующему ассоциативному массиву.
				++it_ex;
			}

			// второй цикл по неактивным абонентам: устанавливаем итераторы на вектор с неактивными абонентами
			a = false;
			it_ex_end = inactiv_users.end();
			it_ex_beg = inactiv_users.begin();
			it_ex = inactiv_users.begin();
			it_in_end = inactiv_users.begin()->end();
			it_in = inactiv_users.begin()->begin();
		}

		// Вывод базы данных завершен, уведомляются ожидающие потоки.
		lock.unlock();
		data_cond.notify_one();

		return count;
	}

	// Вспомогательная функция добавления записи в базу данных без захвата блокировки.
	template<typename Key, typename T>
	bool data<Key, T>::AddRecord_no_block(const unsigned int& first_number, const unsigned int& second_number, const bool& activity, mapped_type& rec) {

		unsigned int old_size = 0;  // размер старой записи, которая заменяется при добавлении новой записи
		bool succes;
		// Проверка на наличие абонента в обоих массивах.
		// В случае отсутствия записи, соответствующей добавляемому номеру,
		// добавляем запись в соответствующий ассоциативный массив.
		// В противном случае производится замена старой записи новой и коррекция размера база данных.
		if (activity) {
			if (!(activ_users[first_number]).add_or_update(second_number, rec, old_size)) {
				// запись уже присутствовала в ассоциативном массиве; после ее замены корректируем размер базы данных.
				number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size() - old_size);
				succes = false;
			}
			else {
				// при добавлении записи в массив с активными абонентами необходимо удалить запись,
				// соответствующую заданному индексу, из массива с неактивными абонентами.
				if ((inactiv_users[first_number]).erase(second_number, old_size)) {
					// добавляемая запись уже присутствовала в массиве с неактивными абонентами.
					number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size() - old_size);
					succes = false;
				}
				else // вновь добавленной записи не было ни в массиве с активными абонентами, ни в массиве с неактивными абонентами
				{
					number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size());
					++number_of_records;
					succes = true;
				}
			}
		}
		else { // аналогично ветке для активных абонентов
			if (!(inactiv_users[first_number]).add_or_update(second_number, rec, old_size)) {
				number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size() - old_size);
				succes = false;
			}
			else {
				if ((activ_users[first_number]).erase(second_number, old_size)) {
					number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size() - old_size);
					succes = false;
				}
				else
				{
					number_of_bytes += (rec.last_name_size() + rec.first_name_size() + rec.patronymic_size());
					++number_of_records;
					succes = true;
				}
			}
		}
		return succes;
	}

	// Вспомогательная функция удаления записи из базы данных без захвата блокировки.
	template<typename Key, typename T>
	bool data<Key, T>::DeleteRecord_no_block(int first_number, int second_number) {

		unsigned int old_size = 0;  // размер удаляемой записи (если таковая существует)
	
		// Удаление абонента из обоих массивов.
		if ( (activ_users[first_number]).erase(second_number, old_size) ){
			// запись успешно удалена; корректируем размер базы данных
			number_of_bytes -= old_size;
			--number_of_records;
			return true;
		}
		if ((inactiv_users[first_number]).erase(second_number, old_size)) {
			// запись успешно удалена; корректируем размер базы данных
			number_of_bytes -= old_size;
			--number_of_records;
			return true;
		}
		
		return false; // записи не было в массиве, удаление не произошло.
	}

	// захват блокировки внешним кодом
	template<typename Key, typename T>
	boost::shared_lock<boost::shared_mutex> data<Key, T>::GetLock(void) {
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		return lock;
	}

	// проверка базы данных на пустоту
	template<typename Key, typename T>
	bool data<Key, T>::Empty() {
		if (number_of_records.load())
			return false;
		else
			return true;
	}

	template<typename Key, typename T>
	void data<Key, T>::GenerateOneThread(unsigned int count_of_records,
		unsigned int block_begin, unsigned int block_end,
		const names_vector& v_last_name_male,
		const names_vector& v_last_name_female,
		const names_vector& v_first_name_male,
		const names_vector& v_first_name_female,
		const names_vector& v_patronymic_male,
		const names_vector& v_patronymic_female)
	{
		// проверка на непустоту генерируемого диапазона
		unsigned int count_of_map = block_end - block_begin; // количество map, в которые генерируются записи
		if (count_of_map == 0)
			return;

		double count_record_per_map = count_of_records / static_cast<double>(count_of_map);
		int count_integer_part = static_cast<int>(count_record_per_map);
		double count_fractional_part = count_record_per_map - count_integer_part;
		int count_in_map = count_integer_part;
		
		std::vector<int> indexes_vec; // вектор для хранения второй половины генерируемого номера
		std::set<int>    indexes_set; // набор для хранения второй половины генерируемого номера

		// размер вектора известен заранее - резервируем его.
		indexes_vec.reserve(count_integer_part);
				
		std::random_device rd;
		std::default_random_engine generator(rd());
		//std::mt19937 generator(rd());
		std::bernoulli_distribution bernoulli(0.5); // генератор числа 0 либо 1 с равной вероятностью
		std::bernoulli_distribution small_bernoulli(count_fractional_part); // генератор числа 0 либо 1 с вероятностью count_fractional_part
		std::uniform_int_distribution<unsigned int> distr_second_part(0, (unsigned int)pow(10, number_of_second_digits) - 1);     // генератор случайного целого числа от 0 до 10^number_of_second_digits - случайная вторая половина телефонного номера

		// генераторы индексов массивов с соответсвующими именами
		std::uniform_int_distribution<unsigned int> distr_last_name_male(0,    (unsigned int) v_last_name_male.size() - 1);
		std::uniform_int_distribution<unsigned int> distr_last_name_female(0,  (unsigned int) v_last_name_female.size() - 1);
		std::uniform_int_distribution<unsigned int> distr_first_name_male(0,   (unsigned int) v_first_name_male.size() - 1);
		std::uniform_int_distribution<unsigned int> distr_first_name_female(0, (unsigned int) v_first_name_female.size() - 1);
		std::uniform_int_distribution<unsigned int> distr_patronymic_male(0,   (unsigned int) v_patronymic_male.size() - 1);
		std::uniform_int_distribution<unsigned int> distr_patronymic_female(0, (unsigned int) v_patronymic_female.size() - 1);

		int count = 0;
		int small_random = 0;
		std::pair<const Key, T> P;
		unsigned int second_part;
		unsigned int random_last_name_index;
		unsigned int random_first_name_index;
		unsigned int random_patronymic_index;

		unsigned int first_index = block_begin;
		while (first_index != block_end)	//  цикл по map
		{
			// два цикла: по целой (k == 0) и дробной частям (k == 1)
			// отдельная генерация нецелого числа записей: если генератор случайного числа 0/1 с вероятностью, 
			// равной дробной части количества генерируемых записей в каждом массиве, выдает значение 1 - производится
			// генерация новой записи и ее добавление в массив.
			for (int k = 0; k < 2; ++k) {
				if (count_integer_part > 0 && k == 0) {

					// генерация count_integer_part неповторяющихся случайных чисел, соответствующим второй половине номера абонента
					while (indexes_vec.size() != count_integer_part) {
						second_part = distr_second_part(generator);
						indexes_vec.push_back(second_part);
						indexes_set.insert(second_part);
						if (indexes_vec.size() != indexes_set.size())
							indexes_vec.pop_back();
					}
				}
				if (count_fractional_part == 0 && k == 1)	continue;

				count = 0;

				// генерация count_integer_part записей в каждом ассоциативном массиве
				// для дробной части производится только один цикл в случае выпадения 1 (с вероятностью count_fractional_part)
				count_in_map = (k == 0)? count_integer_part : small_bernoulli(generator);
								
				while (count < count_in_map) {

					// генерация случайных индексов массивов с фамилиями, именами и отчествами
					std::string sex = bernoulli(generator) ? "f" : "m"; // равновероятный пол
					if (sex == "m") {
						random_last_name_index  = distr_last_name_male(generator);
						random_first_name_index = distr_first_name_male(generator);
						random_patronymic_index = distr_patronymic_male(generator);
					}
					else
					{
						random_last_name_index  = distr_last_name_female(generator);
						random_first_name_index = distr_first_name_female(generator);
						random_patronymic_index = distr_patronymic_female(generator);
					}

					// считывание фамилии, имени и отчества из соответствующих векторов по случайному индексу
					std::string last_name  = (sex == "m") ? v_last_name_male[random_last_name_index] : v_last_name_female[random_last_name_index];
					std::string first_name = (sex == "m") ? v_first_name_male[random_first_name_index] : v_first_name_female[random_first_name_index];
					std::string patronymic = (sex == "m") ? v_patronymic_male[random_patronymic_index] : v_patronymic_female[random_patronymic_index];
					
					// генерация второй части номера
					second_part = distr_second_part(generator);

					// генерация признака активности
					bool activity = bernoulli(generator) ? true : false;

					// создание новой записи
					T rec(std::move(last_name), std::move(first_name), std::move(patronymic));

					if (k == 0) {
						// добавление записи в один из массивов. на каждой итерации цикла по уникальному ключу добавляется
						// новая запись, поэтому отсутствует необходимость проверки на ее наличие в обоих массивах.						//activ_users[first_index].add(indexes[count], rec); // значение второй половины записи находится в векторе indexes 
						if(activity)
							activ_users[first_index].add(indexes_vec[count], rec);
						else 
							inactiv_users[first_index].add(indexes_vec[count], rec);
					}
					else {// ветвь для дробной части
						unsigned int old_size = 0; 
						// добавление записи с проверкой на ее наличие в обоих массивах
						if (activity){
							while( !(activ_users[first_index].add_or_update(second_part, rec, old_size)) )
								second_part = distr_second_part(generator); // повторная генерация второй части номера
							number_of_bytes += rec.size() - old_size;   // коррекция размера базы данных в случае обновления элемента
						}
						else {
							while (!(inactiv_users[first_index].add_or_update(second_part, rec, old_size)))
								second_part = distr_second_part(generator); // повторная генерация второй части номера
							number_of_bytes += rec.size() - old_size;   // коррекция размера базы данных в случае обновления элемента
						}
					}

					// увеличение счетчиков числа записей и размера базы данных
					++number_of_records;
					number_of_bytes += rec.size();
					++count;
				}
			}			
			++first_index;
		}
	}
			
	// Сохранение базы данных в файл в один поток
	template<typename Key, typename T>
	unsigned long data<Key, T>::SaveOneThread(unsigned int block_begin, unsigned int block_end, const std::string file_name)
	{
		// открытие файла на запись
		std::ofstream file(file_name);
		if (!file) {
			file.close(); // явно закрываем файл при наличии исключения
			throw(FileOpenError("Can not open " + file_name + "."));
		}

		unsigned long count = 0;

		// вывод активных абонентов
		unsigned int index = block_begin;
		while (index != block_end) {
			count += activ_users[index].print(file, index, true, number_of_first_digits, number_of_second_digits);
			++index;
		}

		// вывод неактивных абонентов
		index = block_begin;
		while (index != block_end) {
			count += inactiv_users[index].print(file, index, false, number_of_first_digits, number_of_second_digits);
			++index;
		}

		// Сохранение базы данных в файл завершено, посылается уведомление ожидающим потокам.
		file.close(); // Когда file выйдет из области видимости, то деструктор класса ofstream автоматически закроет файл - поэтому нет необходимости в вызове .close().
		data_cond.notify_one();

		return count;
	}

	// Загрузка базы данных из файла в один поток
	template<typename Key, typename T>
	unsigned long data<Key, T>::LoadOneThread(const std::string file_name)
	{
		unsigned long count = 0;

		// открытие файла базы данных на чтение с помощью стороннего ридера (https://github.com/ben-strasser/fast-cpp-csv-parser).
		try {
			io::CSVReader<5> in(file_name); // инициализация ридера

			std::pair<int, int> pair;
			// построчное чтение csv-файла в переменные number, last_name, first_name, patronymic, activity
			std::string number; std::string last_name; std::string first_name; std::string patronymic; int activity;
			while (in.read_row(number, last_name, first_name, patronymic, activity)) {
				// создание новой записи в памяти
				T rec(std::move(last_name), std::move(first_name), std::move(patronymic));
				//T rec(last_name, first_name, patronymic);

				// нахождение индексов, соответсвующих телефонному номеру
				std::pair<int, int> pair = Hasher.hash(number);

				// Добавление записи в базу данных без защиты блокировкой, так как операцией Load уже захвачена блокировка с монопольным доступом.
				// Теоретически в базе данных могут находиться неуникальные записи, поэтому при каждом добавлении записи в базу данных
				// осуществляется ее предварительный поиск в ней, что существенно снижает скорость операции загрузки базы данных отностильно ее выгрузки на диск
				AddRecord_no_block(pair.first, pair.second, activity, rec);
				++count;
			}

			// Когда file выйдет из области видимости, то деструктор класса io::CSVReader<5> автоматически закроет файл - поэтому нет необходимости в вызове .close().

		}
		catch (std::bad_alloc) {
			throw FileError("Ошибка выделения памяти при загрузки данных из файла: " + file_name);
		}
		catch (...) {
			throw FileError("Ошибка открытия/чтения файла: " + file_name);
		}
		// Сохранение базы данных в файл завершено, посылается уведомление ожидающим потокам.
		data_cond.notify_one();

		return count;
	}

	template<typename Key, typename T>
	void data<Key, T>::ClearOneThread(unsigned int block_begin, unsigned int block_end) {
		while (block_begin != block_end) {
			// очистка каждого ассоциативного массива
			activ_users[block_begin].clear();
			inactiv_users[block_begin].clear();
			++block_begin;
		}
	}

	template<typename Key, typename T> unsigned long data<Key, T>::Get_number_of_records(void) const {
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		return number_of_records.load();
	}

	template<typename Key, typename T>
	int data<Key, T>::Get_first_length(void)  const {
		return number_of_first_digits;
	}

	template<typename Key, typename T> unsigned long long data<Key, T>::Get_number_of_bytes(void)   const {
		boost::shared_lock<boost::shared_mutex> lock(mutex);
		return number_of_bytes.load();
	}

	template<typename Key, typename T> void data<Key, T>::Set_number_of_records(unsigned long  N) {
		number_of_records = N;
	}

	template<typename Key, typename T> void data<Key, T>::Set_number_of_bytes(unsigned long long N) {
		number_of_bytes = N;
	}


	// ------------------------------------------------------------ //
	// ----------------- Функции итератора ------------------------ //

	template <typename Key, typename T>
	typename data<Key, T>::const_iterator
		data<Key, T>::begin() const
	{

		if (number_of_records.load() == 0) {
			// Элементы отсутствуют - возвращается конечный итератор.
			return (data_iterator<Key, T>(static_cast<unsigned int>(pow(10, number_of_first_digits)) - 1, false, (inactiv_users[static_cast<unsigned int>(pow(10, number_of_first_digits)) - 1]).end(), this));
		}

		// Здесь существует по крайней мере один элемент. Находим первый элемент и возвращаем итератор на него
		for (unsigned int i = 0; i < activ_users.size(); ++i) { // цикл по активным абонентам
			if (!(activ_users[i].empty()))
				return (data_iterator<Key, T>(i, true, activ_users[i].begin(), this));
		}

		for (unsigned int i = 0; i < inactiv_users.size(); ++i) { // цикл по неактивным абонентам
		    if (!(inactiv_users[i].empty()))
				return (data_iterator<Key, T>(i, false, inactiv_users[i].begin(), this));
		}

		// Теоретически мы не должны попасть сюда, но в этом случае возвращаем конечный итератор. 
		return (data_iterator<Key, T>( (unsigned int)pow(10, number_of_first_digits) - 1, false, (inactiv_users[inactiv_users.size() - 1]).end(), this));

	}

	template <typename Key, typename T>
	typename data<Key, T>::const_iterator
		data<Key, T>::half() const
	{

		// нахождение первого непустого элемента вектора с неактивными абонентами
		for (unsigned int i = 0; i < inactiv_users.size(); ++i) {
			if (!(inactiv_users[i].empty())) 
				return (data_iterator<Key, T>(i, false, inactiv_users[i].begin(), this));
		}

		// Теоретически мы не должны попасть сюда, но в этом случае возвращаем конечный итератор. 
		return (data_iterator<Key, T>((unsigned int)pow(10, number_of_first_digits) - 1, false, (inactiv_users[inactiv_users.size() - 1]).end(), this));
	}

	template <typename Key, typename T>
	typename data<Key, T>::const_iterator
		data<Key, T>::end() const
	{
		// Конечный итератор базы данных - это конечный итератор ассоциативного массива в последнем сегменте. 
		return (data_iterator<Key, T>((unsigned int)pow(10, number_of_first_digits) - 1, false, (inactiv_users[inactiv_users.size() - 1]).end(), this));
	}

	// итератор по-умолчанию. так как операция разыменования данного итератора не имеет смысла, 
	// то инициализируем его произвольными значениями.
	template<typename Key, typename T>
	data_iterator<Key, T>::data_iterator()
	{
		mBucket = -1;
		activ = true;
		it = thread_safe_map<int, T>::const_iterator();
		ptr_vector = NULL;
	}


	template<typename Key, typename T>
	data_iterator<Key, T>::data_iterator(unsigned int Bucket, bool Activ, typename thread_safe_map<int, T>::const_iterator in_iterator,
		const data<Key, T>* in_ptr_vector) :
		mBucket(Bucket), activ(Activ), it(in_iterator), ptr_vector(in_ptr_vector)
	{
	}

	template<typename Key, typename T>
	const unsigned int data_iterator<Key, T>::GetBucket() const
	{
		return mBucket;
	}

	template<typename Key, typename T>
	const bool data_iterator<Key, T>::GetActiv() const
	{
		return activ;
	}

	template<typename Key, typename T>
	const std::pair<const int, T>&
		data_iterator<Key, T>::operator*() const
	{
		return (*it);
	}

	template<typename Key, typename T>
	const std::pair<const int, T>*
		data_iterator<Key, T>::operator->() const
	{
		return (&(*it));
	}


	template<typename Key, typename T>
	data_iterator<Key, T>&
		data_iterator<Key, T>::operator++()
	{
		// Инкрементируем итератор. Если есть в текущем сегменте запись, расположенная
		// за текущей позицией итератора, то итератор укажет на нее. В противном случае
		// итератор укажет на элемент, следующим за последним элементом текущего сегмента.
		++it;

		// Если мы находимся в конце текущего сегмента, находим переходим к последующему сегменту с элементами. 
		if (activ) // если находимся в векторе с активными абонентами, то осуществляем поиск в нем
		{
			if (it == (ptr_vector->activ_users)[mBucket].end()) { // если в текущем ассоциативном массиве нет больше элементов, то осуществляем поиск элемента в следующих массивах.
				for (int i = mBucket + 1; i < (ptr_vector->activ_users).size(); i++) {
					if (!((ptr_vector->activ_users)[i].empty())) {// Нашли непустой сегмент и ссылаемся итератором it на первый элемент в нем.
						it = (ptr_vector->activ_users)[i].begin();
						mBucket = i;
						return (*this);
					}
				}
			}
			else { // если следующий элемент существует в текущем ассоциативном массиве
				// итератор уже инкрементирован и указывает на следующий элемент; значение
				// текущего индекса вектора (номера текущего сегмента) изменять не требуется.
				return (*this);
			}

			// если следующий элемент не обнаружен в векторе с активными абонентами, то переходим в вектор с неактивными абонентами,
			// процедуру поиска последующей записи повторяем в векторе с неактивными абонентами.
			activ = false;

			// в векторе с неактивными абонентами находим номер сегмента, содержащего первую запись.
			for (int i = 0; i < (ptr_vector->inactiv_users).size(); ++i) { // цикл по неактивным абонентам
				if (!(ptr_vector->inactiv_users[i].empty())) {
					// в непустом сегменте указываем указателем на первую запись
					mBucket = i;
					it = (ptr_vector->inactiv_users)[i].begin();
					return (*this);
				}
			}

		}
		else // поиск в векторе с неактивными абонентами
		{    //производим идентично поиску последующего элемента по вектору с активными абонентами
			if (it == (ptr_vector->inactiv_users)[mBucket].end()) {
				for (int i = mBucket + 1; i < (ptr_vector->inactiv_users).size(); i++) {
					if (!((ptr_vector->inactiv_users)[i].empty())) {
						it = (ptr_vector->inactiv_users)[i].begin();
						mBucket = i;
						return (*this);
					}
				}
			}
			else { // если следующий элемент существует в текущем ассоциативном массиве
				   // итератор уже инкрементирован и указывает на следующий элемент; значение
				   // текущего индекса вектора (номера текущего сегмента) изменять не требуется.
				return (*this);
			}
		}

		// В базе данных больше нет непустых сегментов. Присваиваем итератору 
		// it значение, соответствующее конечному итератору 
		// последнего ассоциативного массива. 
		mBucket = (unsigned int)(ptr_vector->inactiv_users).size() - 1;
		it = (ptr_vector->inactiv_users)[mBucket].end();

		return (*this);
	}

	template<typename Key, typename T>
	bool data_iterator<Key, T>::operator==(
		const data_iterator& rhs) const
	{
		// Все поля, на которые ссылаются итераторы, должны быть равны. 
		return (mBucket == rhs.mBucket && activ == rhs.activ && ptr_vector == rhs.ptr_vector && it == rhs.it);
	}
	template<typename Key, typename T>
	bool data_iterator<Key, T>::operator!=(const data_iterator& rhs) const
	{
		return (!operator== (rhs));
	}

	// вспомогательные функции

		// чтение базы данных имен, фамилий и отчеств из файла f в вектор v.
	names_vector read_name_file(const std::string& file_name)
	{
		names_vector vec;

		// открытие базы имен, фамилий или отчеств,
		// в случае неуспеха генерируется исключение.
		try{
		io::CSVReader<1> in(file_name); // инициализация ридера

		// построчное чтение csv-файла в переменные name, sex
		std::string name; 
		while (in.read_row(name)) 
			vec.push_back(name);
			
		}
		catch(...)
		{
			// ошибка чтения файла, генерируется исключение
			throw FileReadError(file_name);
		}

		return vec;
	}

	// функция из вектора случайным образом выбирает имя с заданным полом.
	const std::string& generate_random_name(const names_vector& v, const std::string& sex) {

		std::random_device rd;
		std::default_random_engine generator(rd());
		std::uniform_int_distribution<> distribution(0, int(v.size() - 1));

		int index = distribution(generator); // индекс случайного элемента вектора

		return v[index]; // возвращение случайного имени с заданным полом.
	}
	
} // namespace DataBase

#endif // data_INL
