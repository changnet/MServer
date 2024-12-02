#include "worker_thread.hpp"
#include "lua_cpplib/llib.hpp"

WorkerThread::WorkerThread(const std::string &name) : Thread(name)
{
    L_ = nullptr;
}

WorkerThread::~WorkerThread()
{
    assert(!L_);
}

void WorkerThread::start(const char *path)
{
    entry_.assign(path);
    Thread::start(2000);
}

bool WorkerThread::initialize()
{
    L_ = llib::new_state();

    // TODO push一些共用的全局对象到lua，比如全局日志对象

    /* 加载程序入口脚本 */
    if (LUA_OK != luaL_loadfile(L_, LUA_ENTERANCE))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG_R("worker %s load entry error:%s", name_.c_str(), err_msg);

        return false;
    }

    if (LUA_OK != lua_pcall(L_, 0, 0, 0))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG_R("worker %s call entry error:%s", name_.c_str(), err_msg);

        return 1;
    }

    return true;
}
bool WorkerThread::uninitialize()
{
    L_ = llib::delete_state(L_);
    return true;
}

void WorkerThread::routine_once(int32_t ev)
{
    while (ThreadMessage *m = message_.pop())
    {
        try
        {
            lcpp::call(L_, "on_worker_message", m->addr_, m->udata_);
        }
        catch (const std::runtime_error& e)
        {
            ELOG("%s", e.what());
        }
    }
}