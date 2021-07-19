#ifndef RECORD_H
#define RECORD_H

#include <string>
namespace DataBase {

	// элемент базы данных (запись)
	struct record {
	private:
		std::string last_name;
		std::string first_name;
		std::string patronymic;

	public:
		friend class Access;
		record(std::string&&, std::string&&, std::string&&);
		record();
		record(const record&) = delete;
		bool operator==(const record&) const;
		bool operator!=(const record&) const;
		const std::string& get_last_name() const;
		const std::string& get_first_name() const;
		const std::string& get_patronymic() const;
		std::string get_name() const;
		
		void set_last_name(std::string&);
		void set_first_name(std::string&);
		void set_patronymic(std::string&);

		size_t last_name_size()   const;
		size_t first_name_size()  const;
		size_t patronymic_size()  const;
		size_t size()  const;
	};

} // namespace DataBase

#endif //RECORD_H