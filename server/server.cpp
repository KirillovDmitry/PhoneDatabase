
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif //_CRT_SECURE_NO_WARNINGS
#endif

#include <iostream>
#include <string>
#include <utility>
#include <thread>
#include <future>
#include <memory>

#include "../lib/httplib.h"
#include "../lib/timer.h"
#include "data.h"
#include "hash.h"
#include "record.h"


// вспомогательный класс для увеличения счетчика активных потоков в конструкторе при 
// создании объекта и уменьшения в деструкторе при его уничтожении
class increment_number_threads {
	int increment;
	std::atomic<unsigned int>& current_count_threads;
public:
	explicit increment_number_threads(unsigned int inc, std::atomic<unsigned int>& threads) :
		increment(inc), current_count_threads(threads)
	{
		current_count_threads += increment;
	};

	~increment_number_threads() { current_count_threads -= increment; };
};


int main()
{

#ifdef _WIN32    
	SetConsoleCP(CP_UTF8);       // установка кодовой страницы CP_UTF8 в поток ввода
	SetConsoleOutputCP(CP_UTF8); // установка кодовой страницы CP_UTF8 в поток вывода
#endif       

	httplib::Server svr;   

	// установка увеличенных таймаутов
	svr.set_keep_alive_timeout(100);
	svr.set_read_timeout(100, 0);     
	svr.set_write_timeout(100, 0);    
	svr.set_idle_interval(0, 100000); // 100 milliseconds

	std::cout << std::endl << "The server is running...." << std::endl;
	std::cout << "Waiting for request......" << std::endl << std::endl;
	std::cout << "-----------------------------------------" << std::endl;

	Timer t;
	const unsigned int MaxThreads = 15;  // максимальное число потоков, одновременно обрабатывающих запросы к базе данных
	std::atomic<unsigned int> current_count_threads(0);
	std::string print_time;

	try {

		DataBase::data<std::string, DataBase::record > db(4, 6);
		
		auto init_time = t.elapsed();
		std::cout << "Database initialize time: " + std::to_string(init_time) + " mc." << std::endl;
		std::cout << "-----------------------------------------" << std::endl;

		// приветствие клиента
		svr.Get("/hi", [&](const httplib::Request& req, httplib::Response& res) {
			res.set_content("Hello!", "text/plain");
			std::cout << "Command 'hi' received." << std::endl;
			std::cout << "-----------------------------------------" << std::endl;
			});

		// запрос генерации базы данных
		svr.Post("/generate", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {
			std::cout << "Command 'generate' received." << std::endl;
			Timer t;

			unsigned int NumOfThreads = 1, NumOfRecords = 4;
		
			// получение из запроса числа потоков
			if (req.has_param("NumOfThreads")) {
				NumOfThreads = std::stoi(req.get_param_value("NumOfThreads"));
			}

			// получение из запроса числа генерируемых записей
			if (req.has_param("NumOfRecords")) {
				NumOfRecords = std::stoi(req.get_param_value("NumOfRecords"));
			}
		

			std::string answer, time;
			t.reset();
			try {

				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается
				if (current_count_threads + NumOfThreads > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'generate' request.");
			
				increment_number_threads inc(NumOfThreads, current_count_threads);

				auto count = db.Generate(NumOfRecords, NumOfThreads);		

				time   = std::to_string(t.elapsed());
				answer = "DataBase generated successfully. Count of records: " + std::to_string(count.first) 
					   + ". Count of bytes: " + std::to_string(count.second)  
					   + ". Duration of the generate operation (on the server): " + time + " mc.";
					   
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() <<  std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
		});

		// запрос сохранения базы данных на диск
		svr.Post("/save", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {
		
			std::cout << "Command 'save' received." << std::endl;
			Timer t;

			std::string file_name;

			unsigned int NumOfThreads = 1;
			// получение из запроса числа потоков
			if (req.has_param("NumOfThreads")) {
				NumOfThreads = std::stoi(req.get_param_value("NumOfThreads"));
			}

			// получение из запроса имени сохраняемого файла
			if (req.has_param("FileName")) {
				file_name = req.get_param_value("FileName");
			}


			std::string answer, time;
			t.reset();
			try {
				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается
				if (current_count_threads + NumOfThreads > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'save' request.");

				// увеличение счетчика количества потоков.
				increment_number_threads inc(NumOfThreads, current_count_threads);

				// выполнение запроса
				auto count = db.Save(NumOfThreads, file_name);

				// формирование ответа клиенту
				time = std::to_string(t.elapsed());
				answer = "DataBase saved successfully. Count of saved records: " + std::to_string(count)
					+ ". Duration of the save operation (on the server): " + time + " mc.";
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
		});

		// запрос загрузки базы данных с диска в память
		svr.Post("/load", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {

			std::cout << "Command 'load' received." << std::endl;
			Timer t;
			std::string file_name = "data.csv";

			unsigned int NumOfThreads = 1;
			// получение из запроса числа потоков
			if (req.has_param("NumOfThreads")) {
				NumOfThreads = std::stoi(req.get_param_value("NumOfThreads"));
			}

			// получение из запроса имени загружаемого файла
			if (req.has_param("FileName")) {
				file_name = req.get_param_value("FileName");
			}

			std::string answer, time;
			t.reset();
			try {
				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается
				if (current_count_threads + NumOfThreads > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'load' request.");

				increment_number_threads inc(NumOfThreads, current_count_threads);

				unsigned long count = db.Load(NumOfThreads, file_name);

				time = std::to_string(t.elapsed());
				answer = "DataBase loaded successfully. Count of load records: " + std::to_string(count)
					+ ". Duration of the load operation (on the server): " + time + " mc.";
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
		});

		// запрос очистки базы данных в памяти
		svr.Post("/clear", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {

			std::cout << "Command 'clear' received." << std::endl;
			Timer t;

			int NumOfThreads = 1;

			// получение из запроса числа потоков
			if (req.has_param("NumOfThreads")) {
				NumOfThreads = std::stoi(req.get_param_value("NumOfThreads"));
			}

			std::string answer, time;
			t.reset();
			try {
			
				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается
				if (current_count_threads + 1 > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'clear' request.");

				increment_number_threads inc(NumOfThreads, current_count_threads);

				db.Clear(NumOfThreads);

				time = std::to_string(t.elapsed());
				answer = "DataBase erased successfully. Duration of the 'clear' operation (on the server): " + time + " mc.";
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
		});
	
		// запрос добавления записи в базу данных
		svr.Post("/add", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {
			std::cout << "Command 'add' received." << std::endl;
			Timer t;

			std::string last_name(""), first_name(""), patronymic(""), number("");
			bool activity = false;

			// получение из запроса имени добавляемого абонента
			if (req.has_param("FIRST_NAME")) {
				first_name = req.get_param_value("FIRST_NAME");
			}

			// получение из запроса фамилии добавляемого абонента
			if (req.has_param("LAST_NAME")) {
				last_name = req.get_param_value("LAST_NAME");
			}

			// получение из запроса отчества добавляемого абонента
			if (req.has_param("PATRONYMIC")) {
				patronymic = req.get_param_value("PATRONYMIC");
			}

			// получение из запроса номера телефона добавляемого абонента
			if (req.has_param("NUMBER")) {
				number = req.get_param_value("NUMBER");
			}

			// получение из запроса признака активности добавляемого абонента
			if (req.has_param("ACTIVITY")) {
				activity = std::stoi(req.get_param_value("ACTIVITY"));
			}

			std::string answer, time;
			t.reset();
			try {

				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается.
				if (current_count_threads + 1 > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'add' request.");

				increment_number_threads inc(1, current_count_threads);

				// создание записи из принятых параметров и добавление ее в базу данных
				//DataBase::record rec(last_name, first_name, patronymic);
				DataBase::record rec(std::move(last_name), std::move(first_name), std::move(patronymic));

				bool success = db.AddRecord(number, activity, rec);
			
				time = std::to_string(t.elapsed());

				std::string str;
				if (success)
					str = "New record added successfully: " + number + ", " + rec.get_last_name() + ", " + rec.get_first_name() + ", " + rec.get_patronymic() + ". ";
				else
					str = "Record replaced successfully: " + number + ", " + rec.get_last_name() + ", " + rec.get_first_name() + ", " + rec.get_patronymic() + ". ";

				answer = str + " Duration of the 'add' operation (on the server): " + time + " mc.";
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
		});

		// запрос удаления записи из базы данных
		svr.Post("/delete", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {
		
			std::cout << "Command 'delete' received." << std::endl;
			Timer t;
			std::string number("");

			// получение из запроса номера телефона добавляемого абонента
			if (req.has_param("NUMBER")) {
				number = req.get_param_value("NUMBER");
			}

			std::string answer, time;
			t.reset();
			try {

				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается.
				if (current_count_threads + 1 > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'add' request.");

				increment_number_threads inc(1, current_count_threads);

				bool success = db.DeleteRecord(number);

				time = std::to_string(t.elapsed());

				std::string str;
				if (success)
					str = "Record with number " + number + " deleted successfully.";
				else
					str = "The record was not in the database.";

				answer = str + " Duration of the 'delete' operation (on the server): " + time + " mc.";
				std::cout << answer << std::endl;

				// сохранение результатов в http-заголовках и передача их клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}
			std::cout << "-----------------------------------------" << std::endl;
			});

		// запрос поиска в базе данных абонента по номеру
		svr.Post("/find", [&db, &current_count_threads, &MaxThreads](const httplib::Request& req, httplib::Response& res) {
			std::cout << "Command 'find' received." << std::endl;
			Timer t;
			std::string number;

			// получение из запроса клиента числа потоков
			if (req.has_param("NUMBER")) {
				number = req.get_param_value("NUMBER");
			}

			std::string answer, time;
			bool activity = false;
			DataBase::record rec;

			t.reset();
			try {
				// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
				// генерируется исключение и запрос не обрабатывается
				if (current_count_threads + 1 > MaxThreads)
					throw DataBase::MaxThreadError("Thread limit exceeded in 'find' request.");

				// увеличение счетчика текущих процессов
				increment_number_threads inc(1, current_count_threads);

				bool success = db.FindRecord(number, activity, rec);
				time = std::to_string(t.elapsed());

				if (success) {
					answer = "Record found successfully. The subscriber " + rec.get_last_name() + " " + rec.get_first_name() + " "
						+ rec.get_patronymic() + " has a number " + number + " and it is an " + (activity? "active":"inactive") + " subscriber."
						+ " Duration of the 'find' operation (on the server): " + time + " mc.";
				}
				else
					answer = "Record not found. The subscriber with number " + number + " is not in the phone base." 
					+ " Duration of the 'find' operation (on the server): " + time + " mc.";

				// сохранение результатов в http-заголовках и передача клиенту
				res.set_header("ANSWER", answer);
				res.set_header("TIME", time);
				std::cout << answer << std::endl;

			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc&) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}

			std::cout << "-----------------------------------------" << std::endl;
			});

		// запрос чтения базы данных
		svr.Get("/print", [&db, &current_count_threads, &MaxThreads, &print_time](const httplib::Request& req, httplib::Response& res) {

			std::cout << "Command 'print' received." << std::endl;

			res.set_content_provider(
				"text/plain", // Content type
				[&](size_t offset, httplib::DataSink& sink) {

					Timer t;
					std::string answer, time;
					t.reset();

					try {

						bool activity = false;
						int curent_thread = 0;
						int num_threads = 1;
						// получение из запроса клиента признака активности
						if (req.has_header("ACTIVITY")) {
							activity = (req.get_header_value("ACTIVITY") == "1") ? true : false;
						}

						// получение из запроса клиента общее число потоков
						if (req.has_header("NumOfThreads")) {
							num_threads = std::stoi(req.get_header_value("NumOfThreads"));
						}
						// получение из запроса клиента номера текущего потока
						if (req.has_header("CurentThread")) {
							curent_thread = std::stoi(req.get_header_value("CurentThread"));
						}

						// при превышении максимального числа потоков, обрабатывающих запросы к базе данных,
						// генерируется исключение и запрос не обрабатывается
						if (current_count_threads + num_threads > MaxThreads)
							throw DataBase::MaxThreadError("Thread limit exceeded in 'print' request.");

						increment_number_threads inc(1, current_count_threads);

						std::string answer, time;

						// захват блокировки внешним кодом для вывода записей базы данных в выходной поток
						boost::shared_lock<boost::shared_mutex> lock(db.GetLock());

						// начальные указатели устанавливаются на массив либо с активными абонентами
						// либо с неактивными абонентами.
						// auto ptr_begin = activity ? db.begin() : db.half();
						// auto ptr_end = activity ? db.half() : db.end();
						auto ptr_begin = db.begin();
						auto ptr_end = db.end();

						// в случае отсутствия записей для передачи их клиенту в переменную print_time
						// заносится сообщение об ошибке
						bool error = false;
						if (ptr_begin == ptr_end)
							error = true;

						// определение количества сегментов базы данных, выводимых данным потоком
						unsigned int block_size = static_cast<unsigned int>(pow(10, db.Get_first_length()) / num_threads);

						unsigned int block_begin = block_size * curent_thread;
						unsigned int block_end = block_begin + block_size;

						std::string number, buffer;
						// в цикле по указателю перебираются все активные, либо все неактивные абоненты
						while (ptr_begin != ptr_end) {
							if (ptr_begin.GetActiv() == activity) {
								if (ptr_begin.GetBucket() >= block_begin && ptr_begin.GetBucket() < block_end) {
									number = db.Hasher.unhash(ptr_begin.GetBucket(), ptr_begin->first);
									buffer = number + ", " +
										ptr_begin->second.get_last_name() + ", " + ptr_begin->second.get_first_name() + ", " +
										ptr_begin->second.get_patronymic() + "\n";
									sink.write(buffer.c_str(), buffer.size());
								}
							}
							++ptr_begin;
						}
						sink.done();

						print_time = std::to_string(t.elapsed());
						if (error)
							answer = "No records to print.";
						else
							answer = "Duration of the 'print' operation (on the server): " + print_time + " mc.";

						std::cout << answer << std::endl;
					}
					catch (std::runtime_error& e) {
						std::cout << e.what() << std::endl;
						res.set_header("ERROR", e.what());
					}
					catch (std::bad_alloc) {
						std::cout << "Memory allocation error." << std::endl;
						res.set_header("ERROR", "Memory allocation error.");
					}
					catch (std::invalid_argument& e) {
						std::cout << e.what() << std::endl;
						res.set_header("ERROR", e.what());
					}
					catch (...) {
						res.set_header("ERROR", "Unknown error.");
						std::cout << "Unknown error." << std::endl;
					}
					std::cout << "-----------------------------------------" << std::endl;
					return true;
				});
			});

		// вспомогательный запрос времени чтения базы данных
		svr.Get("/print_time", [&db, &current_count_threads, &MaxThreads, &print_time](const httplib::Request& req, httplib::Response& res) {

			res.set_content_provider(
				"text/plain", // Content type
				[&](size_t offset, httplib::DataSink& sink) {
						sink.write(print_time.c_str(), print_time.size());
						sink.done();
						return true;
				});
		});

		// запрос остановки сервера
		svr.Get("/stop", [&](const httplib::Request& req, httplib::Response& res) {
			std::cout << "Command 'stop' receive." << std::endl;
			Timer t;
			std::string answer, time;
			try {
				svr.stop();
				time = std::to_string(t.elapsed());
				answer = " The server stopped successfully.";
			}
			catch (std::runtime_error& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (std::bad_alloc&) {
				std::cout << "Memory allocation error." << std::endl;
				res.set_header("ERROR", "Memory allocation error.");
			}
			catch (std::invalid_argument& e) {
				std::cout << e.what() << std::endl;
				res.set_header("ERROR", e.what());
			}
			catch (...) {
				res.set_header("ERROR", "Unknown error.");
				std::cout << "Unknown error." << std::endl;
			}

			// сохранение результатов в http-заголовках и передача клиенту
			res.set_header("ANSWER", answer);
			res.set_header("TIME", time);
			std::cout << answer << std::endl;

			std::cout << "-----------------------------------------" << std::endl;
			});

		svr.listen("localhost", 8080);   // слушаем локальный порт
		//svr.listen("127.0.0.1", 8080);   // слушаем локальный порт
		//svr.listen("192.168.0.158", 8080); // слушаем порт в локальной сети

	}
	catch (...)
	{
		std::cout << "memory allocation error during database initialization." << std::endl;
	}
	return 0;
}
