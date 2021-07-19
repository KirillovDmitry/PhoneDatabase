#include "record.h"

namespace DataBase {

    record::record(std::string&& LastName, std::string&& FirstName, std::string&& Patronymic) :
        last_name(LastName), first_name(FirstName), patronymic(Patronymic)
    {
    }
    record::record() :
        last_name(""), first_name(""), patronymic("")
    {
    }
    
    bool record::operator==(const record& rec) const {
        return (last_name == rec.last_name) &&
            (first_name == rec.first_name) && (patronymic == rec.patronymic);
    }

    bool record::operator!=(const record& rec) const {
        return (last_name != rec.last_name) ||
            (first_name != rec.first_name) || (patronymic != rec.patronymic);
    }

    const std::string& record::get_last_name() const
    {
        return last_name;
    }

    const std::string& record::get_first_name() const
    {
        return first_name;
    }

    const std::string& record::get_patronymic() const
    {
        return patronymic;
    }

    std::string record::get_name() const
    {
        return last_name + ", " + first_name + ", " + patronymic + ", ";
    }
    

    void record::set_last_name(std::string& name) {
        last_name = name;
    }

    void record::set_first_name(std::string& name) {
        first_name = name;
    }

    void record::set_patronymic(std::string& name) {
        patronymic = name;
    }

    size_t record::last_name_size()  const { return  last_name.size();  }
    size_t record::first_name_size() const { return  first_name.size(); }
    size_t record::patronymic_size() const { return  patronymic.size(); }
    size_t record::size() const { return  last_name.size() + patronymic.size() + patronymic.size(); }
}
