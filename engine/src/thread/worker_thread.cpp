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

int32_t WorkerThread::start(lua_State *L)
{
    size_t index = 2;
    for (size_t i = index; i < index + params_.size(); i++)
    {
        const char *p = lua_tostring(L, (int32_t)i);
        if (!p) break;
        params_[i - index].assign(p);
    }
    Thread::start(2000);

    return 0;
}

bool WorkerThread::initialize()
{
    L_ = llib::new_state();

    // push一些共用的全局对象到lua，比如全局日志对象
    lcpp::Class<WorkerThread>::push(L_, this, false);
    lua_setglobal(L_, "g_worker");

    lua_pushcfunction(L_, traceback);
    /* 加载程序入口脚本 */
    if (LUA_OK != luaL_loadfile(L_, params_[0].c_str()))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG_R("worker %s load entry error:%s", name_.c_str(), err_msg);

        lua_pop(L_, 2); // pop error message and traceback
        L_ = llib::delete_state(L_);
        return false;
    }

    int32_t n = 0;
    for (int32_t i = 1; i < params_.size(); i++)
    {
        if (params_[i].empty()) break;

        n++;
        lua_pushstring(L_, params_[i].c_str());
    }

    if (LUA_OK != lua_pcall(L_, n, 0, 1))
    {
        const char *err_msg = lua_tostring(L_, -1);
        ELOG_R("worker %s call entry error:%s", name_.c_str(), err_msg);

        lua_pop(L_, 2); // pop error message and trace back
        L_ = llib::delete_state(L_);
        return false;
    }
    lua_pop(L_, 1); // traceback

    return true;
}
bool WorkerThread::uninitialize()
{
    L_ = llib::delete_state(L_);
    return true;
}

void WorkerThread::routine_once(int32_t ev)
{
    int32_t count = 0;
    while (true)
    {
        try
        {
            ThreadMessage m = message_.pop();
            if (-1 == m.src_) return;

            lcpp::call(L_, "on_worker_message", m.src_, m.type_,
                       m.udata_, m.usize_);
            m.dispose();
        }
        catch (const std::runtime_error &e)
        {
            ELOG("%s", e.what());
        }

        count++;
        if (256 == count)
        {
            count = 0;
            // 主线程会不断地派发任务，worker线程可能会无休止地运行
            // 因此执行一定数据的逻辑后，需要更新时间及定时器
        }
    }
}