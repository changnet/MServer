#include "worker_thread.hpp"
#include "lua_cpplib/llib.hpp"

WorkerThread::WorkerThread(const std::string &name) : Thread(name)
{
    L = llib::new_state();
}

WorkerThread::~WorkerThread()
{
    L = llib::delete_state(L);
}

void WorkerThread::routine(int32_t ev)
{

}