
#include <future>
#include <thread>

#include "../lib/httplib.h"
#include "../lib/timer.h"
#include "../lib/join_threads.h"

// пользовательский интерфейс
int displayMenu();

// функция выводит результаты запроса на экран и возвращает время работы процедуры на сервере
std::string GetAnswer(httplib::Result); 

int main()
{
#ifdef _WIN32    
	SetConsoleCP(CP_UTF8);       // установка кодовой страницы CP_UTF8 в поток ввода
	SetConsoleOutputCP(CP_UTF8); // установка кодовой страницы CP_UTF8 в поток вывода
#endif   

	bool done = false;

	// инициализация: установка IP-адреса и порта
	httplib::Client cli("localhost", 8080);
	//httplib::Client cli("127.0.0.1", 8080);    
	//httplib::Client cli("192.168.0.158", 8080);

	// установка увеличенных таймаутов
	cli.set_connection_timeout(0, 300000); // 300 milliseconds
	cli.set_read_timeout(200, 0);		   // 200 seconds
	cli.set_write_timeout(200, 0);		   // 200 seconds

	// проверка доступности сервера
	if (auto res = cli.Get("/hi")) {
		std::cout << "The server is available." << std::endl;
	}
	else {
		std::cout << "The server is not available." << std::endl;
		std::cout << "Error code: " << res.error() << std::endl;
		done = true;
	}

	Timer t;
	//std::string answer;
	
	while (!done) {

		int selection = displayMenu(); // получение от пользователя номера запроса 
									   // и его отправка в базу данных
									   
		switch (selection) {
			case 1: {  // запрос генерации базы данных
				
				httplib::Params params;
				params.emplace("NumOfThreads", "4");   // количество вычислительных потоков
				params.emplace("NumOfRecords", "18000000");  // количество генерируемых записей
				std::cout << "'generate' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();

				GetAnswer(cli.Post("/generate", params));
				std::cout << "Duration of the 'generate' request: " << t.elapsed() << " mc." << std::endl;

			}
			break;
		
			case 2: { // запрос сохранения базы данных на диск
				httplib::Params params;
				params.emplace("FileName", "data.csv");
				params.emplace("NumOfThreads", "4");   // количество вычислительных потоков
				std::cout << "'save' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Post("/save", params));
				std::cout << "Duration of the 'save' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 3: { // запрос загрузки базы данных в память
				httplib::Params params;
				params.emplace("FileName", "data.csv");
				params.emplace("NumOfThreads", "4");   // количество вычислительных потоков
				std::cout << "'load' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Post("/load", params));
				std::cout << "Duration of the 'load' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 4: { // запрос очистки базы данных
				httplib::Params params;
				std::cout << "'clear' request sent. waiting for an answer...." << std::endl << std::endl;
				params.emplace("NumOfThreads", "4");   // количество вычислительных потоков
				t.reset();
				GetAnswer(cli.Post("/clear", params));
				std::cout << "duration of the 'clear' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 5: { // запрос добавления абонента в базу данных
				httplib::Params params;
				params.emplace("FIRST_NAME", "Иван");	   // имя добавляемого абонента
				params.emplace("LAST_NAME", "Иванов");	   // фамилия добавляемого абонента
				params.emplace("PATRONYMIC", "Иванович");  // отчество добавляемого абонента
				params.emplace("NUMBER", "85556668215");   // номер телефона добавляемого абонента
				params.emplace("ACTIVITY", "1");		   // активность добавляемого абонента
				std::cout << "'add' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Post("/add", params));
				std::cout << "duration of the 'add' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 6: { // запрос удаления абонента из базы данных
				httplib::Params params;
				params.emplace("NUMBER", "85556668215");   // номер телефона удаляемого абонента
				std::cout << "'delete' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Post("/delete", params));
				std::cout << "duration of the 'delete' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 7: { // запрос поиска имени абонента по номеру телефона
				httplib::Params params;
				params.emplace("NUMBER", "85556668215");    // номер телефона искомого абонента
				std::cout << "'find' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Post("/find", params));
				std::cout << "duration of the 'find' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 8: {// запрос вывода активных либо неактивных абонентов	
				auto headers = httplib::Headers();
				httplib::Params params;
				int num_threads;
				
				struct process {
					void operator()(int curent_thread, std::string file_name, httplib::Client& cli, httplib::Headers headers) {
	
						headers.emplace("CurentThread", std::to_string(curent_thread));   // текущий поток
						//std::cout << "'print' request sent. waiting for an answer...." << std::endl << std::endl;

						std::ofstream file(file_name);
						std::string name_of_req = "'print" + std::to_string(curent_thread) + "'";
						std::cout << name_of_req + " request sent. waiting for an answer...." << std::endl << std::endl;
						cli.Get("/print", headers, //httplib::Headers(),
							[&](const httplib::Response& response) {
								return true;
							},
							[&](const char* data, size_t data_length) {
								file << std::string(data, data_length);
								return true;
							});
						file.close();

					}
				};

				t.reset();

				int i = 0;
				std::string answer;
				headers.emplace("ACTIVITY", "1");		    // активность выводимых абонентов
				headers.emplace("ANSWER", "1");
				headers.emplace("NumOfThreads", "4");       // количество потоков с запросами к базе данных
				num_threads = std::stoi(headers.find("NumOfThreads")->second);
				try{
					std::vector<std::thread> threads(num_threads);

					join_threads joiner(threads);
					std::string file_name;
					for (int i = 0; i < num_threads; ++i) {
						file_name = "answer" + std::to_string(i) + ".csv";
						threads[i] = std::thread(process(), i, file_name, std::ref(cli), headers);
					}
				}
				catch (...)
				{
					std::cout << "Request transmission error." << std::endl;
				}
				
				std::cout << std::endl << "Received data were saved in a answer.csv" << std::endl;
				//std::cout << "duration of the 'print' on the server: " << answer << " mc." << std::endl;
				std::cout << "duration of the 'print' request: " << t.elapsed() << " mc." << std::endl;
			}	
			break;
	
			case 9: { // остановка сервера
				std::cout << "'stop' request sent. waiting for an answer...." << std::endl << std::endl;
				t.reset();
				GetAnswer(cli.Get("/stop"));
				std::cout << "duration of the 'stop' request: " << t.elapsed() << " mc." << std::endl;
			}
			break;
		
			case 0: { // завершение программы
				done = true;
			}	
			break;

			default:
				std::cout << "Unknown command. Try again." << std::endl;
			break;

		}
	}

	return 0;
}


int displayMenu() {
	int sel;
	std::cout << std::endl;
	std::cout << "------------------------------------" << std::endl;
	std::cout << "Type a query to the database:"        << std::endl;
	std::cout << "1) Generate the phone database"       << std::endl;
	std::cout << "2) Save the phone database to disk"   << std::endl;
	std::cout << "3) Load the phone database from disk" << std::endl;
	std::cout << "4) Clear the phone database" << std::endl;
	std::cout << "5) Add the record to the phone database" << std::endl;
	std::cout << "6) Delete the record from the phone database" << std::endl;
	std::cout << "7) Find the abonent name" << std::endl;
	std::cout << "8) Print the name and number of active abonent" << std::endl;
	std::cout << "9) Stop the server" << std::endl;
	std::cout << "0) Exit" << std::endl;
	std::cout << std::endl;
	std::cin >> sel;
	std::cout << std::endl;
	return sel;
}

std::string GetAnswer(httplib::Result res) {
	std::string time = "";
	if (res) {
		if (res->has_header("ANSWER")) {
			auto val = res->get_header_value("ANSWER");
			std::cout << val << std::endl;
		}
		if (res->has_header("TIME")) {
			time = res->get_header_value("TIME"); // время выполнения операции
			
		}
		if (res->has_header("ERROR")) {
			auto val = res->get_header_value("ERROR");
			std::cout << val << std::endl;
		}

	}
	else
	{
		std::cout << "the server is not responding" << std::endl;
		std::cout << "error code: " << res.error() << std::endl;
	}
	return time;
}
