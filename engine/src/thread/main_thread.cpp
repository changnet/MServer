#include "worker_thread.hpp"
#include "lua_cpplib/llib.hpp"

MainThread::MainThread()
{
    L_ = nullptr;
}

MainThread::~MainThread()
{
    assert(!L_);
}
