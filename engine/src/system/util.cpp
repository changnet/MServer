#include "util.hpp"

#include <thread>
#include <chrono>
#include "thread/thread.hpp"

namespace util
{
void sleep(int32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void set_thread_name(const char* name)
{
    Thread::set_thread_name(name);
}

}