#include "util.hpp"

#include <thread>
#include <chrono>

namespace util
{
void sleep(int32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
}