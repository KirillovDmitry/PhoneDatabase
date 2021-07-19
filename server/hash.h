#ifndef HASH_H
#define HASH_H

#include <stdexcept>
#include <utility>
#include <string>

namespace DataBase {
    
    // шаблон хэш-функции
    template <typename T>
    class DefaultHash
    {
        public:
            DefaultHash(int mNumBuckets_ex, int mNumBuckets_in);
            std::pair<int,int> hash(const T& key) const;
            T unhash(const int& ex_index, const int& it_index);

        private:
            int mNumBuckets_ex;
            int mNumBuckets_in;
    };

    // реализация конструктора хэша
    template <typename T>
    DefaultHash<T>::DefaultHash(int NumBuckets_ex, int NumBuckets_in)
    {
        if (NumBuckets_ex < 1 || NumBuckets_in < 1) {
            throw (std::invalid_argument("DefaultHash: the value of numBuckets must be greater than 10."));
        }
		if (NumBuckets_ex > 9 || NumBuckets_in > 9) {
			throw (std::invalid_argument("DefaultHash: the value of numBuckets must be less than 10."));
		}
        if (NumBuckets_ex + NumBuckets_in != 10) {
            throw (std::invalid_argument("DefaultHash: the value of (mNumBuckets_ex+mNumBuckets_in) must be equal to 10."));
        }

        mNumBuckets_ex = NumBuckets_ex;
        mNumBuckets_in = NumBuckets_in;
    };

	// Вычисление хэша в общем случае
	// Солтер, Кеплер, глава 23
	template <typename T>
	std::pair<int, int> DefaultHash<T>::hash(const T& key) const
	{
		unsigned long res1 = 0;
		for (int i = 0; i < mNumBuckets_ex; ++i) {
		    res1 += *((char*)&key + i);
		}

		unsigned long res2 = 0;
		for (int i = mNumBuckets_ex; i < mNumBuckets_ex + mNumBuckets_in; ++i) {
		    res2 += *((char*)&key + i);
		}
		return std::make_pair((res1 % mNumBuckets_ex), (res2 % mNumBuckets_in));
	}

	// Преобразование, обратное хэшированию. В программе используется специализация шаблона для типа std::string
	template <typename T>
	T DefaultHash<T>::unhash(const int& ex_index, const int& it_index) 
	{
	        T t;
	        return t;
	}
    

     // Специализация DefaultHash::hash() для поставленной задачи: метод DefaultHash.hash(std::string) переводит std::string в
     // два int - mNumBuckets_ex и mNumBuckets_in
	template <>
	std::pair<int, int> DefaultHash<std::string>::hash(const std::string& key) const
		{
		if (key.size() != 11)
			throw std::invalid_argument("Hasher: the number must be set in the format '89993332211'.");

		// первые mNumBuckets_ex и последние mNumBuckets_in символов ключа отображаем в std::pair<int, int>
		return std::make_pair(
			std::stoi(key.substr(1, mNumBuckets_ex)),  // первая цифра "8" телефонного номера в преобразовании не участвует.
			std::stoi(key.substr(1 + mNumBuckets_ex, mNumBuckets_in)));
	    }

     // Специализация DefaultHash::unhash() для поставленной задачи: метод DefaultHash<T>::unhash(std::pair<int, int> index)
     // переводит два индекса mNumBuckets_ex и mNumBuckets_in в std::string.
	template <>
	std::string DefaultHash<std::string>::unhash(const int& first_number, const int& second_number)
		{
	             	// преобразование int-индексов в std::string.
					std::string first  = std::to_string(first_number);
					std::string second = std::to_string(second_number);

					// дополнение строк нулями до требуемого размера телефонного номера
					std::string add_first(mNumBuckets_ex -  first.size(), '0');
					std::string add_second(mNumBuckets_in - second.size(), '0');

	                // объединение std::string-представлей входных индексов в один std::string.
	                return "8" + add_first + first + add_second + second;
	    	}

}
#endif