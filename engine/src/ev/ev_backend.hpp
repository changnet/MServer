#pragma once

/**
 * @brief 用于执行io操作的后台类
 */
class EVBackend
{
public:
    /// 待处理的io事件
    class ModifyEvent final
    {
    public:
        ModifyEvent(int32_t fd, int32_t old_ev, int32_t new_ev)
        {
            _fd = fd;
            _old_ev = old_ev;
            _new_ev = new_ev;
        }
    public:
        int32_t _fd;
        int32_t _old_ev;
        int32_t _new_ev;
    };

public:
    EVBackend()
    {
        _done = false;
        _ev   = nullptr;
    }
    virtual ~EVBackend(){};

    /// 唤醒子进程
    virtual void wake() = 0;

    /// 启动backend线程
    virtual bool start(class EV *ev)
    {
        _ev = ev;
        return true;
    }

    /// 中止backend线程
    virtual bool stop()
    {
        _done = true;

        wake();
        return true;
    }

    /// 修改io事件(包括删除)
    virtual void modify(int32_t fd, int32_t old_ev, int32_t new_ev) = 0;

protected:
    bool _done;    /// 是否终止进程
    class EV *_ev; /// 主循环
};