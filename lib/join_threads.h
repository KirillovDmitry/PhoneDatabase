#ifndef JOIN_THREADS
#define JOIN_THREADS

#include <vector>
#include <thread>

// безопасная относительно исключений обертка вектора потоков,
// которая присоединяет все завершившиеся потоки.
// (Уильямс, Паралльное программирование на С++ в действии. стр.350)
class join_threads
{
	std::vector<std::thread>& threads;
public:
	explicit join_threads(std::vector<std::thread>& threads_) :
		threads(threads_)
	{}
	~join_threads()
	{
		for (unsigned long i = 0; i < threads.size(); ++i)
		{
			if (threads[i].joinable())
				threads[i].join();
		}
	}
};
#endif