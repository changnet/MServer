#include <thread>
#include "worker_thread.hpp"

#include "lpp/llib.hpp"
#include "thread.hpp"
#include "system/static_global.hpp"
#include "ev/time.hpp"

WorkerThread::WorkerThread(const std::string &name)
{
    stop_ = true;
    L_ = nullptr;
    name_ = name;
}

WorkerThread::~WorkerThread()
{
    // 即使线程函数已经stop，在析构前必须保证线程对象join，否则销毁时会抛异常
    stop(true);

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

    stop_   = false;
    thread_ = std::thread(&WorkerThread::routine, this);

    return 0;
}

void WorkerThread::stop(bool join)
{
    stop_ = true;

    if (join && thread_.joinable())
    {
        // 只能在主线程调用
        assert(std::this_thread::get_id() != thread_.get_id());

        // 唤醒子线程，上面已经设置stop_ = true，这里发消息时，worker线程可能已退出
        emplace_message(0, 0, ThreadMessage::NONE, nullptr, 0);

        thread_.join();
    }
}

bool WorkerThread::initialize()
{
    L_ = llib::new_state();
    timing::update();

    // push一些共用的全局对象到lua，比如全局日志对象
    lcpp::Class<WorkerThread>::push(L_, this, false);
    lua_setglobal(L_, "g_thread");

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
    for (size_t i = 1; i < params_.size(); i++)
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

void WorkerThread::dispatch_message()
{
    // 其他线程会不断地派发任务，worker线程可能会无休止地运行
    // 因此执行一定数量的逻辑后，需要更新时间及定时器
    for (int i = 1; i < 256; i++)
    {
        try
        {
            ThreadMessage *m = pop_message();
            if (!m) return;

            cb_message_ = m;
            void *buffer =
                (m->mask_ & FlexiblePool::BUFFER) ? m->buffer() : nullptr;
            lcpp::call(L_, "on_worker_message", m->src_, m->type_, buffer,
                       m->usize_);
            // 如果这个消息被转到其他线程或者复用，则cb_message_为nullptr
            if (cb_message_ == m)
            {
                cb_message_ = nullptr;
                dispose_message(m);
            }
        }
        catch (const std::runtime_error &e)
        {
            ELOG("%s", e.what());
        }
    }
}

void WorkerThread::routine()
{
    Thread::apply_thread_name(name_.c_str());

    if (!initialize()) /* 初始化 */
    {
        ELOG("%s thread initialize fail", name_.c_str());
        return;
    }

    while (likely(!stop_))
    {
        timing::update();
        int64_t wait_time = timer_mgr_.next_interval();
        if (wait_time < 0) wait_time = 5000;

        wait_for(wait_time);

        timing::update();
        timer_mgr_.update_timeout(this);

        dispatch_message();
    }
    // 收到停止消息时，再处理一次消息
    // 注意业务层是有关服顺序的，要等业务层关服完成后，底层才会发出停止消息
    // 这时候不应该会存在很多消息的
    dispatch_message();
    // 这只是个警告，不是错误。因为主线程让worker线程退出时，会发一个消息唤醒worker线程
    // 但这时候worker线程可能刚好已经退出了
    size_t ms = message_size();
    if (0 != ms)
    {
        PLOG("worker thread message queue not clean: %zu", ms);
    }

    if (!uninitialize()) /* 清理 */
    {
        ELOG("%s thread uninitialize fail", name_.c_str());
        return;
    }
}

int32_t WorkerThread::push(lua_State *L, bool gc)
{
    lcpp::Class<WorkerThread>::push(L, this, gc);
    return 1;
}
