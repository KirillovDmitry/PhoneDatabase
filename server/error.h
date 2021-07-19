#ifndef error_H
#define error_H

#include <exception>
namespace DataBase {
	/*
	* В базе данных используются следующие типы исключений:
	* - исключения, связанные с записью/чтением файла данных с диска (FileOpenError, FileReadError),
	*   которые наследуются от FileError (runtime_error);
	* - окончание времени ожидания (WaitTimeError);
	* - исключения, связанные с нарушением последовательности доступа к базе данных (SequenceError);
	* - превышение числа потоков для одновременной обработки запросов (MaxThreadError);
	* - стандартные исключения std::bad_alloc и std::invalid_argument. 
	*/

	// ошибка при работе с файлами
	class FileError : public std::runtime_error
	{
	public:
		FileError(const std::string& fileIn) : std::runtime_error(""), mFile(fileIn) {}
		virtual ~FileError() noexcept {}
		virtual const char* what() const throw ()
		{
			return mMsg.c_str();
		}
		std::string getFileName() { return mFile; }
	protected:
		std::string mFile, mMsg;
	};

	// ошибка открытия файла
	class FileOpenError : public FileError
	{
	public:
		FileOpenError(const std::string& fileNameIn) : FileError(fileNameIn) {
			mMsg = "Не удалось открыть файл " + fileNameIn + ".";
		}
		virtual ~FileOpenError() noexcept {}
	};

	// ошибка чтения файла
	class FileReadError : public FileError
	{
	public:
		FileReadError(const std::string& fileNameIn) : FileError(fileNameIn) {
			mMsg = "Ошибка чтения файла " + fileNameIn + ".";
		}
		virtual ~FileReadError() noexcept {}
	};

	// окончание времени ожидания
	class WaitTimeError : public std::runtime_error
	{
	public:
		WaitTimeError(const std::string& message) : std::runtime_error(""), mMsg(message) {}
		virtual ~WaitTimeError() noexcept {}
		virtual const char* what() const throw ()
		{
			return mMsg.c_str();
		}
	protected:
		std::string mMsg;
	};

	//  нарушение последовательности доступа к базе данных
	class SequenceError : public std::runtime_error
	{
	public:
		SequenceError(const std::string& message) : std::runtime_error(""), mMsg(message) {}
		virtual ~SequenceError() noexcept {}
		virtual const char* what() const throw ()
		{
			return mMsg.c_str();
		}
	protected:
		std::string mMsg;
	};

	//  превышение максимального числа потоков
	class MaxThreadError : public std::runtime_error
	{
	public:
		MaxThreadError(const std::string& message) : std::runtime_error(""), mMsg(message) {}
		virtual ~MaxThreadError() noexcept {}
		virtual const char* what() const throw ()
		{
			return mMsg.c_str();
		}
	protected:
		std::string mMsg;
	};

/*
	// тип ошибки при работе с данными
	class DataError : public std::runtime_error
	{
	public:
		DataError() : std::runtime_error("") {}
		virtual ~DataError() noexcept {}
		virtual const char* what() const throw ()
		{
			return mMsg.c_str();
		}
		std::string getError() { return mMsg; }
	protected:
		std::string mMsg;
	};


	// тип ошибки данных в памяти
	class DataErrorInMemory : public DataError
	{
	public:
		DataErrorInMemory() : DataError() {
			mMsg = "Данные отсутсвуют в памяти.";
		}
		virtual ~DataErrorInMemory() noexcept {}
	};

	// тип ошибки данных в памяти
	class WrongParametr : public DataError
	{
	public:
		WrongParametr() : DataError() {
			mMsg = "L_ex + L_in !== 10.";
		}
		virtual ~WrongParametr() noexcept {}
	};
	*/
}
#endif
