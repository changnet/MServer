#include "env.hpp"

void Env::init(const char *source)
{
    set("source", source);
    set("__OS_NAME__", __OS_NAME__);
    set("__COMPLIER_", __COMPLIER_);
    set("__BACKEND__", __BACKEND__);
    set("__TIMESTAMP__", __TIMESTAMP__); // 这个时间不准，只有被编译到才会更新
}
