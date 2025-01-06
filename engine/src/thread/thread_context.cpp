#include "thread_context.hpp"
#include "system/static_global.hpp"

ThreadMessage ThreadMessage::Null{-1, -1, -1, nullptr, 0};

void ThreadContextMgr::add_thread_ctx(int32_t addr, ThreadContext* ctx)
{
    StaticGlobal::M->add(addr, ctx);
}

void ThreadContextMgr::del_thread_ctx(int32_t addr)
{
    StaticGlobal::M->del(addr);
}
