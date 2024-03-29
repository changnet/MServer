#include <chrono> // for 1000ms

#include "ev.hpp"
#include "ev_backend.hpp"

// minimum timejump that gets detected (if monotonic clock available)
#define MIN_TIMEJUMP 1000

/*
 * the heap functions want a real array index. array index 0 is guaranteed to
 * not be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP
 * gives the branching factor of the d-tree.
 */

#define HEAP0             1 // 二叉堆的位置0不用，数据从1开始存放
#define HPARENT(k)        ((k) >> 1)
#define UPHEAP_DONE(p, k) (!(p))

EV::EV()
{
    _io_changes.reserve(1024);
    _pendings.reserve(1024);
    _timers.reserve(1024);
    _periodics.reserve(1024);
    _io_fevents.reserve(1024);
    _io_revents.reserve(1024);

    _has_job = false;

    _io_delete_index = 0;

    // 定时时使用_timercnt管理数量，因此提前分配内存以提高效率
    _timer_cnt = 0;
    _timers.resize(1024, nullptr);

    _periodic_cnt = 0;
    _periodics.resize(1024, nullptr);

    // 初始化时间
    _clock_diff               = 0;
    _last_system_clock_update = INT_MIN;
    time_update();

    _busy_time         = 0;
    _next_backend_time = 0;

    _backend = EVBackend::instance();
}

EV::~EV()
{
    _timer_mgr.clear();
    _periodic_mgr.clear();

    EVBackend::uninstance(_backend);
    _backend = nullptr;
}

int32_t EV::loop()
{
    // 脚本可能加载了很久才进入loop，需要及时更新时间
    time_update();

    /*
     * 这个循环里执行的顺序有特殊要求
     * 1. 检测_done必须在invoke_pending之后，中间不能执行wait，不然设置done后无法及时停服
     * 2. wait的前后必须执行time_update，不然计算出来的时间不准
     * 3. 计算wait的时间必须在wait之前，不能在invoke_pending的时候一般执行逻辑一边计算。
     * 因为执行逻辑可能会耗很长时间，那时候计算的时间是不准的
     */

    _done           = false;
    int64_t last_ms = _steady_clock;

    if (!_backend->start(this)) return -1;

    static const int64_t min_wait = 1;     // 最小等待时间，毫秒
    static const int64_t max_wait = 60000; // 最大等待时间，毫秒

    while (EXPECT_TRUE(!_done))
    {
        // 这里可能会出现spurious wakeup(例如收到一个信号)，但不需要额外处理
        // 目前所有的子线程唤醒多次都没有问题，以后有需求再改

        // 把fd变更设置到backend中去
        io_reify();

        time_update();
        _busy_time = _steady_clock - last_ms;

        /// 允许阻塞的最长时间(毫秒)
        int64_t backend_time = _next_backend_time - _steady_clock;
        if (_timer_cnt)
        {
            // wait时间不超过下一个定时器触发时间
            int64_t to = (_timers[HEAP0])->_at - _steady_clock;
            if (backend_time > to) backend_time = to;
        }
        if (_periodic_cnt)
        {
            // utc定时器
            int64_t to = (_periodics[HEAP0])->_at - _system_clock;
            if (backend_time > to) backend_time = to;
        }
        if (EXPECT_FALSE(backend_time < min_wait)) backend_time = min_wait;

        {
            // https://en.cppreference.com/w/cpp/thread/condition_variable
            // 1. wait_for之前必须获得锁
            // 2. wait_for后，自动解锁
            // 3. 当被通知、超时、spurious wakeup时，重新获得锁
            // 退出生命域后，unique_lock销毁，自动解锁，这时程序处理无锁运行状态

            // TODO 每次分配一个unique_lock，效率是不是有点低
            std::unique_lock<std::mutex> ul(_mutex);

            // check the condition, in case it was already updated and notified
            // 在获得锁后，必须重新检测_has_job，避免主线程执行完任务后
            // 子线程再把任务丢给主线程，所以子线程设置任务时也要加锁
            if (!_has_job)
            {
                _cv.wait_for(ul, std::chrono::milliseconds(backend_time));
            }
            _has_job = false;

            // 收集io事件 放这里不再额外加锁
            io_receive_event_reify();
        }

        time_update();

        last_ms            = _steady_clock;
        _next_backend_time = _steady_clock + max_wait;

        // 处理timer超时
        timers_reify();
        // 处理periodic超时
        periodic_reify();

        // 触发io和timer事件
        invoke_pending();

        running(); // 执行其他逻辑
    }

    _backend->stop();

    // 这些对象可能会引用其他资源（如buffer之类的），程序正常关闭时应该严谨地
    // 在脚本关闭，而不是等底层强制删除
    if (!_io_mgr.empty())
    {
        PLOG("io not delete, maybe unsafe, count = %zu", _io_mgr.size());
        _io_mgr.clear();
    }

    if (!_timer_mgr.empty())
    {
        PLOG("timer not delete, maybe unsafe, count = %zu", _timer_mgr.size());
        _timer_mgr.clear();
    }

    if (!_periodic_mgr.empty())
    {
        PLOG("periodic not delete, maybe unsafe, count = %zu",
             _periodic_mgr.size());
        _periodic_mgr.clear();
    }

    return 0;
}

int32_t EV::quit()
{
    _done = true;

    return 0;
}

EVIO *EV::io_start(int32_t id, int32_t fd, int32_t events)
{
    auto p = _io_mgr.try_emplace(id, id, fd, events, this);
    if (!p.second) return nullptr;

    EVIO *w = &(p.first->second);

    w->_status = EVIO::S_NEW;
    io_change(w);

    // 不能直接设置fd_watcher，因为从backend回来的事件顺序是无法保证的
    // 当fd被复用时，新fd先执行io_start，然后旧fd再执行io_delete就会删掉刚设置的fd
    // _backend->set_fd_watcher(fd, w);

    return w;
}

int32_t EV::io_stop(int32_t id, bool flush)
{
    // 这里不能删除watcher，因为io线程可能还在使用，只是做个标记
    // 这里是由上层逻辑调用，也不要直接加锁去删除watcher，防止堆栈中还有引用
    EVIO *w = get_io(id);
    if (!w) return -1;

    // 不能删event，因为有些event还是需要，比如EV_CLOSE
    // clear_pending(w);

    w->_uevents = static_cast<uint8_t>(flush ? (EV_CLOSE | EV_FLUSH) : EV_CLOSE);

    // S_NEW状态表示还没调用set_fd_watcher，这时不需要再设置
    w->_status = EVIO::S_NEW == w->_status ? EVIO::S_NONE : EVIO::S_STOP;

    io_change(w);

    return 0;
}

int32_t EV::io_delete(int32_t id)
{
    EVIO *w = get_io(id);
    if (!w) return -1;

    w->_status = EVIO::S_DEL;

    // 保证del操作在前面，或者del操作用一个独立的数组来实现？
    if (_io_changes.size() > (size_t)_io_delete_index)
    {
        EVIO *old_w = _io_changes[_io_delete_index];
        if (old_w == w) return 0;

        old_w->_change_index = 0;
        io_change(old_w);

        _io_changes[_io_delete_index] = w;
        w->_change_index              = _io_delete_index + 1;
    }
    else
    {
        io_change(w);
    }
    _io_delete_index++;

    return 0;
}

void EV::clear_io_fast_event(EVIO *w)
{
    if (w->_b_fevent_index)
    {
        assert(w->_b_fevent_index <= (int32_t)_io_fevents.size());
        _io_fevents[w->_b_fevent_index - 1] = nullptr;
        w->_b_fevent_index                  = 0;
    }
}

void EV::clear_io_receive_event(EVIO *w)
{
    if (w->_b_revent_index)
    {
        assert(w->_b_revent_index <= (int32_t)_io_revents.size());
        _io_revents[w->_b_revent_index - 1] = nullptr;
        w->_b_revent_index                  = 0;
    }
}

void EV::io_reify()
{
    if (_io_changes.empty()) return;

    {
        bool non_del = false; // 是否进行过非del操作，仅用于逻辑校验
        std::lock_guard<std::mutex> guard(lock());
        for (auto w : _io_changes)
        {
            w->_change_index = 0;

            int32_t fd = w->_fd;
            switch (w->_status)
            {
            case EVIO::S_NONE: non_del = true; break;
            case EVIO::S_STOP:
                non_del = true;
                _backend->modify(w); // 移除该socket
                break;
            case EVIO::S_START:
                non_del = true;
                _backend->modify(w);
                break;
            case EVIO::S_NEW:
                non_del    = true;
                w->_status = EVIO::S_START;
                _backend->set_fd_watcher(fd, w);
                _backend->modify(w);

                // fast_event可能触发一读写操作而调用get_fd_watcher
                // 因此在第一次set_fd_watcher之前是不允许fast_event执行的
                assert(-1 == w->_b_fevent_index);
                w->_b_fevent_index = 0;
                if (w->_b_fevents) io_fast_event(w, w->_b_fevents);
                break;
            case EVIO::S_DEL:
                // watcher的操作是异步的，同样fd的watcher可能被删掉又被系统复用
                // 一定要保证先删除再进行其他操作
                assert(!non_del);

                // backend线程执行删除时，可能之前有一些fevents或者revents已触发
                clear_pending(w);
                clear_io_fast_event(w);
                clear_io_receive_event(w);

                _io_mgr.erase(w->_id);
                _backend->set_fd_watcher(fd, nullptr);
                break;
            }
        }
    }

    _backend->wake(); /// 唤醒子线程，让它及时处理修改的事件
    _io_changes.clear();
    _io_delete_index = 0;
}

int64_t EV::system_clock()
{
    const auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
}

int64_t EV::steady_clock()
{
    // steady_clock在linux下应该也是用clock_gettime实现
    // 但是在debug模式下，steady_clock的效率仅为clock_gettime的一半
    // 执行一百万次，steady_clock花127毫秒，clock_gettime花54毫秒
    // https://www.cnblogs.com/coding-my-life/p/16182717.html
    /*
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    */

    static const auto beg = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - beg)
        .count();
}

void EV::time_update()
{
    _steady_clock = steady_clock();

    /**
     * 直接计算出UTC时间而不通过get_time获取
     * 例如主循环为5ms时，0.5s同步一次省下了100次系统调用(get_time是一个syscall，比较慢)
     * libevent是5秒同步一次CLOCK_SYNC_INTERVAL，libev是0.5秒
     */
    if (_steady_clock - _last_system_clock_update < MIN_TIMEJUMP / 2)
    {
        _system_clock = _clock_diff + _steady_clock;
        _system_now   = _system_clock / 1000; // 转换为秒
        return;
    }

    _last_system_clock_update = _steady_clock;
    _system_clock             = system_clock();
    _system_now               = _system_clock / 1000; // 转换为秒

    /**
     * 当两次diff相差比较大时，说明有人调了UTC时间
     * 由于获取时间(clock_gettime等函数）是一个syscall，有优化级调用，可能出现前一个
     * clock获取到的时间为调整前，后一个clock为调整后的情况
     * 必须循环几次保证取到的都是调整后的时间
     *
     * 参考libev
     * loop a few times, before making important decisions.
     * on the choice of "4": one iteration isn't enough,
     * in case we get preempted during the calls to
     * get_time and get_clock. a second call is almost guaranteed
     * to succeed in that case, though. and looping a few more times
     * doesn't hurt either as we only do this on time-jumps or
     * in the unlikely event of having been preempted here.
     */
    int64_t old_diff = _clock_diff;
    for (int32_t i = 4; --i;)
    {
        _clock_diff = _system_clock - _steady_clock;

        int64_t diff = old_diff - _clock_diff;
        if (EXPECT_TRUE((diff < 0 ? -diff : diff) < MIN_TIMEJUMP))
        {
            return;
        }

        _system_clock = system_clock();
        _system_now   = _system_clock / 1000; // 转换为秒

        _steady_clock             = steady_clock();
        _last_system_clock_update = _steady_clock;
    }
}

void EV::io_change(EVIO *w)
{
    // 通过_change_index判断是否重复添加
    if (!w->_change_index)
    {
        _io_changes.emplace_back(w);
        w->_change_index = static_cast<int32_t>(_io_changes.size());
    }
}

void EV::io_receive_event(EVIO *w, int32_t revents)
{
    w->_b_revents |= static_cast<uint8_t>(revents);
    if (EXPECT_TRUE(!w->_b_revent_index))
    {
        _io_revents.emplace_back(w);
        w->_b_revent_index = static_cast<int32_t>(_io_revents.size());
    }
}

void EV::io_fast_event(EVIO *w, int32_t events)
{
    bool wake = false;
    {
        std::lock_guard<SpinLock> guard(_spin_lock);

        w->_b_fevents |= static_cast<uint8_t>(events);

        // 玩家登录的时候，可能有上百次数据发送，不要每次都插入队列
        // _b_fevent_index为-1时，表示该watcher尚未执行第一次io_reify，也不放入队列
        if (0 == w->_b_fevent_index)
        {
            wake = _io_fevents.empty();
            _io_fevents.emplace_back(w);
            w->_b_fevent_index = static_cast<int32_t>(_io_fevents.size());
        }
    }

    if (wake) _backend->wake();
}

void EV::io_receive_event_reify()
{
    // 外部加锁
    // std::lock_guard<std::mutex> guard(lock());

    for (auto w : _io_revents)
    {
        // watcher被删掉时会置为nullptr
        if (!w) continue;

        feed_event(w, w->_b_revents);
        w->_b_revents      = 0;
        w->_b_revent_index = 0;
    }

    _io_revents.clear();
}

void EV::feed_event(EVWatcher *w, int32_t revents)
{
    // 已经在待处理队列里了，则设置事件即可
    w->_revents |= static_cast<uint8_t>(revents);
    if (EXPECT_TRUE(!w->_pending))
    {
        _pendings.emplace_back(w);
        w->_pending = (int32_t)_pendings.size();
    }
}

void EV::invoke_pending()
{
    for (auto w : _pendings)
    {
        // 可能其他事件调用了clear_pending导致当前watcher无效了
        if (EXPECT_TRUE(w && w->_pending))
        {
            int32_t events = w->_revents;

            w->_pending = 0;
            w->_revents = 0;

            // callback之后，不要对w进行任何操作
            // 因为callback到脚本后，脚本可能直接删除该w
            w->callback(events);
        }
    }
    _pendings.clear();
}

void EV::clear_pending(EVWatcher *w)
{
    // 如果这个watcher在pending队列中，从队列中删除
    if (w->_pending)
    {
        assert(w == _pendings[w->_pending - 1]);
        _pendings[w->_pending - 1] = nullptr;

        w->_revents = 0;
        w->_pending = 0;
    }
}

void EV::timers_reify()
{
    while (_timer_cnt && (_timers[HEAP0])->_at <= _steady_clock)
    {
        EVTimer *w = _timers[HEAP0];

        assert(w->_index);

        if (w->_repeat)
        {
            w->_at += w->_repeat;

            // 如果时间出现偏差，重新调整定时器
            if (EXPECT_FALSE(w->_at < _steady_clock))
                w->reschedule(_steady_clock);

            assert(w->_repeat > 0);

            down_heap(_timers.data(), _timer_cnt, HEAP0);
        }
        else
        {
            timer_stop(w); // 这里不能从管理器删除，还要回调到脚本
        }

        feed_event(w, EV_TIMER);
    }
}

void EV::periodic_reify()
{
    while (_periodic_cnt && (_periodics[HEAP0])->_at <= _system_clock)
    {
        EVTimer *w = _periodics[HEAP0];

        assert(w->_index);
        // 如果_system_clock超时，则秒度精度的_system_now也应该超时
        // 因为脚本逻辑都是用_system_now作为时间基准
        assert(w->_at <= _system_now * 1000);

        feed_event(w, EV_TIMER);
        if (w->_repeat)
        {
            w->_at += w->_repeat;

            // 如果时间出现偏差，重新调整定时器
            if (EXPECT_FALSE(w->_at < _system_clock))
            {
                w->reschedule(_system_clock);
            }

            down_heap(_periodics.data(), _periodic_cnt, HEAP0);
        }
        else
        {
            periodic_stop(w); // 这里不能从管理器删除，还要回调到脚本
        }
    }
}

int32_t EV::timer_start(int32_t id, int64_t after, int64_t repeat, int32_t policy)
{
    assert(repeat >= 0);

    // 如果不支持try_emplace，使用std::forward_as_tuple实现
    auto p = _timer_mgr.try_emplace(id, id, this);
    if (!p.second) return -1;

    EVTimer *w = &(p.first->second);

    w->_at     = _steady_clock + after;
    w->_repeat = repeat;
    w->_policy = policy;

    assert(w->_repeat >= 0);

    ++_timer_cnt;
    int32_t index = _timer_cnt + HEAP0 - 1;
    if (_timers.size() < (size_t)index + 1)
    {
        _timers.resize(index + 1024, nullptr);
    }

    _timers[index] = w;
    up_heap(_timers.data(), index);

    assert(index >= 1 && _timers[w->_index] == w);

    return index;
}

int32_t EV::timer_stop(int32_t id)
{
    auto found = _timer_mgr.find(id);
    if (found == _timer_mgr.end())
    {
        return -1;
    }

    timer_stop(&(found->second));

    _timer_mgr.erase(found);
    return 0;
}

int32_t EV::timer_stop(EVTimer *w)
{
    clear_pending(w);
    if (EXPECT_FALSE(!w->_index)) return 0;

    {
        int32_t index = w->_index;

        assert(_timers[index] == w);

        --_timer_cnt;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (EXPECT_TRUE(index < _timer_cnt + HEAP0))
        {
            // 把当前最后一个timer(_timercnt + HEAP0)覆盖当前timer的位置，再重新调整
            _timers[index] = _timers[_timer_cnt + HEAP0];
            adjust_heap(_timers.data(), _timer_cnt, index);
        }
    }

    w->_at -= _steady_clock;
    w->_index = 0;

    return 0;
}

int32_t EV::periodic_start(int32_t id, int64_t after, int64_t repeat,
                           int32_t policy)
{
    assert(repeat >= 0);

    // 如果不支持try_emplace，使用std::forward_as_tuple实现
    auto p = _periodic_mgr.try_emplace(id, id, this);
    if (!p.second) return -1;

    EVTimer *w = &(p.first->second);

    /**
     * 传进来的单位是秒，要转换为毫秒。实际上精度是毫秒
     * 这些起始时间用_system_now而不是_system_clock
     *
     * 比如1秒定时器当前utc时间戳_system_now为100，
     * 实际utc时间_system_clock为100.5，那定时器应该在_system_now为101时触发
     */
    w->_at     = (_system_now + after) * 1000;
    w->_repeat = repeat * 1000;
    w->_policy = policy;

    ++_periodic_cnt;
    int32_t index = _periodic_cnt + HEAP0 - 1;
    if (_periodics.size() < (size_t)index + 1)
    {
        _periodics.resize(index + 1024, nullptr);
    }

    _periodics[index] = w;
    up_heap(_periodics.data(), index);

    assert(index >= 1 && _periodics[w->_index] == w);

    return index;
}

int32_t EV::periodic_stop(int32_t id)
{
    auto found = _periodic_mgr.find(id);
    if (found == _periodic_mgr.end())
    {
        return -1;
    }

    periodic_stop(&(found->second));

    _periodic_mgr.erase(found);

    return 0;
}

int32_t EV::periodic_stop(EVTimer *w)
{
    clear_pending(w);
    if (EXPECT_FALSE(!w->_index)) return 0;

    {
        int32_t index = w->_index;

        assert(_periodics[index] == w);

        --_periodic_cnt;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (EXPECT_TRUE(index < _periodic_cnt + HEAP0))
        {
            // 把当前最后一个timer(_timercnt + HEAP0)覆盖当前timer的位置，再重新调整
            _periodics[index] = _periodics[_periodic_cnt + HEAP0];
            adjust_heap(_periodics.data(), _periodic_cnt, index);
        }
    }

    return 0;
}

void EV::down_heap(HeapNode *heap, int32_t N, int32_t k)
{
    HeapNode he = heap[k];

    for (;;)
    {
        // 二叉堆的规则：N*2、N*2+1则为child的下标
        int c = k << 1;

        if (c >= N + HEAP0) break;

        // 取左节点(N*2)还是取右节点(N*2+1)
        c += c + 1 < N + HEAP0 && (heap[c])->_at > (heap[c + 1])->_at ? 1 : 0;

        if (he->_at <= (heap[c])->_at) break;

        heap[k]           = heap[c];
        (heap[k])->_index = k;
        k                 = c;
    }

    heap[k]    = he;
    he->_index = k;
}

void EV::up_heap(HeapNode *heap, int32_t k)
{
    HeapNode he = heap[k];

    for (;;)
    {
        int p = HPARENT(k);

        if (UPHEAP_DONE(p, k) || (heap[p])->_at <= he->_at) break;

        heap[k]           = heap[p];
        (heap[k])->_index = k;
        k                 = p;
    }

    heap[k]    = he;
    he->_index = k;
}

void EV::adjust_heap(HeapNode *heap, int32_t N, int32_t k)
{
    if (k > HEAP0 && (heap[k])->_at <= (heap[HPARENT(k)])->_at)
    {
        up_heap(heap, k);
    }
    else
    {
        down_heap(heap, N, k);
    }
}

void EV::reheap(HeapNode *heap, int32_t N)
{
    /* we don't use floyds algorithm, upheap is simpler and is more
     * cache-efficient also, this is easy to implement and correct
     * for both 2-heaps and 4-heaps
     */
    for (int32_t i = 0; i < N; ++i)
    {
        up_heap(heap, i + HEAP0);
    }
}
