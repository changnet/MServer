#include <chrono> // for 1000ms

#include "ev.hpp"
#include "ev_backend.hpp"
#include "system/static_global.hpp"

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
    timers_.reserve(1024);
    periodics_.reserve(1024);

    // 定时时使用timercnt_管理数量，因此提前分配内存以提高效率
    timer_cnt_ = 0;
    timers_.resize(1024, nullptr);

    periodic_cnt_ = 0;
    periodics_.resize(1024, nullptr);

    // 初始化时间
    clock_diff_               = 0;
    last_system_clock_update_ = INT_MIN;
    time_update();

    busy_time_         = 0;
    next_backend_time_ = 0;

    backend_ = EVBackend::instance();
}

EV::~EV()
{
    timer_mgr_.clear();
    periodic_mgr_.clear();

    EVBackend::uninstance(backend_);
    backend_ = nullptr;
}

int32_t EV::loop_init()
{
    time_update();

    // 在进入loop之前，要初始化一些必要的数据
    // 因为loop是在lua脚本调用的，在调用loop之前，可能会调用定时器、socket相关接口
    if (!backend_->start(this)) return -1;

    return 0;
}

int32_t EV::loop()
{
    // 脚本可能加载了很久才进入loop，需要及时更新时间
    time_update();

    /*
     * 这个循环里执行的顺序有特殊要求
     * 1. 检测done_必须在invoke_pending之后，中间不能执行wait，不然设置done后无法及时停服
     * 2. wait的前后必须执行time_update，不然计算出来的时间不准
     * 3. 计算wait的时间必须在wait之前，不能在invoke_pending的时候一般执行逻辑一边计算。
     * 因为执行逻辑可能会耗很长时间，那时候计算的时间是不准的
     */

    done_           = false;
    int64_t last_ms = steady_clock_;

    static const int64_t min_wait = 1;     // 最小等待时间，毫秒
    static const int64_t max_wait = 60000; // 最大等待时间，毫秒

    while (likely(!done_))
    {
        // 这里可能会出现spurious wakeup(例如收到一个信号)，但不需要额外处理
        // 目前所有的子线程唤醒多次都没有问题，以后有需求再改

        time_update();
        busy_time_ = steady_clock_ - last_ms;

        /// 允许阻塞的最长时间(毫秒)
        int64_t backend_time = next_backend_time_ - steady_clock_;
        if (timer_cnt_)
        {
            // wait时间不超过下一个定时器触发时间
            int64_t to = (timers_[HEAP0])->at_ - steady_clock_;
            if (backend_time > to) backend_time = to;
        }
        if (periodic_cnt_)
        {
            // utc定时器
            int64_t to = (periodics_[HEAP0])->at_ - system_clock_;
            if (backend_time > to) backend_time = to;
        }
        if (unlikely(backend_time < min_wait)) backend_time = min_wait;
        
        // 等待其他线程的数据
        tcv_.wait_for(backend_time);

        time_update();

        last_ms            = steady_clock_;
        next_backend_time_ = steady_clock_ + max_wait;

        // 处理timer超时
        timers_reify();
        // 处理periodic超时
        periodic_reify();

        // 触发io和timer事件
        invoke_pending();
        invoke_backend_events();

        running(); // 执行其他逻辑
    }

    backend_->stop();

    // 这些对象可能会引用其他资源（如buffer之类的），程序正常关闭时应该严谨地
    // 在脚本关闭，而不是等底层强制删除
    if (fd_mgr_.size())
    {
        PLOG("io not delete, maybe unsafe, count = %zu", fd_mgr_.size());
    }

    if (!timer_mgr_.empty())
    {
        PLOG("timer not delete, maybe unsafe, count = %zu", timer_mgr_.size());
        timer_mgr_.clear();
    }

    if (!periodic_mgr_.empty())
    {
        PLOG("periodic not delete, maybe unsafe, count = %zu",
             periodic_mgr_.size());
        periodic_mgr_.clear();
    }

    return 0;
}

int32_t EV::quit()
{
    done_ = true;
    StaticGlobal::T = true;

    return 0;
}

EVIO *EV::io_start(int32_t fd, int32_t events)
{
    // 这里需要留意fd复用的问题
    // 当一个watcher没有被delete时，请不要关闭其fd。不然内核会复用这个fd
    // 导致有多个同样fd的watcher
    assert(!fd_mgr_.get(fd));

    EVIO *w = new EVIO(fd, this);
    fd_mgr_.set(w);

    if (events) append_event(w, events);

    return w;
}

int32_t EV::io_stop(int32_t fd, bool flush)
{
    // 这里不能直接删除watcher，因为io线程可能还在使用，只是做个标记
    // 这里是由上层逻辑调用，也不要直接加锁去删除watcher，防止堆栈中还有引用
    EVIO *w = fd_mgr_.get(fd);
    if (!w) return -1;

    append_event(w, flush ? EV_FLUSH : EV_CLOSE);

    return 0;
}

int32_t EV::io_delete(int32_t fd)
{
    EVIO *w = fd_mgr_.get(fd);
    if (!w) return -1;

    fd_mgr_.unset(w);
    delete w;

    return 0;
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
    steady_clock_ = steady_clock();

    /**
     * 直接计算出UTC时间而不通过get_time获取
     * 例如主循环为5ms时，0.5s同步一次省下了100次系统调用(get_time是一个syscall，比较慢)
     * libevent是5秒同步一次CLOCK_SYNC_INTERVAL，libev是0.5秒
     */
    if (steady_clock_ - last_system_clock_update_ < MIN_TIMEJUMP / 2)
    {
        system_clock_ = clock_diff_ + steady_clock_;
        system_now_   = system_clock_ / 1000; // 转换为秒
        return;
    }

    last_system_clock_update_ = steady_clock_;
    system_clock_             = system_clock();
    system_now_               = system_clock_ / 1000; // 转换为秒

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
    int64_t old_diff = clock_diff_;
    for (int32_t i = 4; --i;)
    {
        clock_diff_ = system_clock_ - steady_clock_;

        int64_t diff = old_diff - clock_diff_;
        if (likely((diff < 0 ? -diff : diff) < MIN_TIMEJUMP))
        {
            return;
        }

        system_clock_ = system_clock();
        system_now_   = system_clock_ / 1000; // 转换为秒

        steady_clock_             = steady_clock();
        last_system_clock_update_ = steady_clock_;
    }
}

void EV::add_pending(EVTimer *w, int32_t revents)
{
    //.定时器触发时，可能会在回调事件删改定时器
    // 因此在触发不直接回调到脚本，而是暂存起来再触发回调

    // 已经在待处理队列里了，则设置事件即可
    w->revents_ |= static_cast<uint8_t>(revents);
    if (likely(!w->pending_))
    {
        pendings_.emplace_back(w);
        w->pending_ = (int32_t)pendings_.size();
    }
}

 void EV::invoke_pending()
{
    for (auto w : pendings_)
    {
        // 可能其他事件调用了clear_pending导致当前watcher无效了
        if (likely(w && w->pending_))
        {
            int32_t events = w->revents_;
            w->pending_  = 0;
            w->revents_    = 0;
            // callback之后，不要对w进行任何操作
            // 因为callback到脚本后，脚本可能直接删除该w
            w->callback(events);
        }
    }
    pendings_.clear();
}

void EV::del_pending(EVTimer *w)
{
    // 如果这个watcher在pending队列中，从队列中删除
    if (w->pending_)
    {
        assert(w == pendings_[w->pending_ - 1]);
        pendings_[w->pending_ - 1] = nullptr;
        w->revents_                = 0;
        w->pending_                = 0;
    }
}

void EV::invoke_backend_events()
{
    std::vector<WatcherEvent> &events = backend_->fetch_event();
    for (const auto& x : events)
    {
        assert(x.ev_);
        x.w_->callback(x.ev_);
    }
    events.clear();
}

void EV::timers_reify()
{
    while (timer_cnt_ && (timers_[HEAP0])->at_ <= steady_clock_)
    {
        EVTimer *w = timers_[HEAP0];

        assert(w->index_);

        if (w->repeat_)
        {
            w->at_ += w->repeat_;

            // 如果时间出现偏差，重新调整定时器
            if (unlikely(w->at_ < steady_clock_))
                w->reschedule(steady_clock_);

            assert(w->repeat_ > 0);

            down_heap(timers_.data(), timer_cnt_, HEAP0);
        }
        else
        {
            timer_stop(w); // 这里不能从管理器删除，还要回调到脚本
        }

        add_pending(w, EV_TIMER);
    }
}

void EV::periodic_reify()
{
    while (periodic_cnt_ && (periodics_[HEAP0])->at_ <= system_clock_)
    {
        EVTimer *w = periodics_[HEAP0];

        assert(w->index_);
        // 如果system_clock_超时，则秒度精度的system_now_也应该超时
        // 因为脚本逻辑都是用system_now_作为时间基准
        assert(w->at_ <= system_now_ * 1000);

        add_pending(w, EV_TIMER);
        if (w->repeat_)
        {
            w->at_ += w->repeat_;

            // 如果时间出现偏差，重新调整定时器
            if (unlikely(w->at_ < system_clock_))
            {
                w->reschedule(system_clock_);
            }

            down_heap(periodics_.data(), periodic_cnt_, HEAP0);
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
    auto p = timer_mgr_.try_emplace(id, id, this);
    if (!p.second) return -1;

    EVTimer *w = &(p.first->second);

    w->at_     = steady_clock_ + after;
    w->repeat_ = repeat;
    w->policy_ = policy;

    assert(w->repeat_ >= 0);

    ++timer_cnt_;
    int32_t index = timer_cnt_ + HEAP0 - 1;
    if (timers_.size() < (size_t)index + 1)
    {
        timers_.resize(index + 1024, nullptr);
    }

    timers_[index] = w;
    up_heap(timers_.data(), index);

    assert(index >= 1 && timers_[w->index_] == w);

    return index;
}

int32_t EV::timer_stop(int32_t id)
{
    auto found = timer_mgr_.find(id);
    if (found == timer_mgr_.end())
    {
        return -1;
    }

    timer_stop(&(found->second));

    timer_mgr_.erase(found);
    return 0;
}

int32_t EV::timer_stop(EVTimer *w)
{
    del_pending(w);
    if (unlikely(!w->index_)) return 0;

    {
        int32_t index = w->index_;

        assert(timers_[index] == w);

        --timer_cnt_;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (likely(index < timer_cnt_ + HEAP0))
        {
            // 把当前最后一个timer(timercnt_ + HEAP0)覆盖当前timer的位置，再重新调整
            timers_[index] = timers_[timer_cnt_ + HEAP0];
            adjust_heap(timers_.data(), timer_cnt_, index);
        }
    }

    w->at_ -= steady_clock_;
    w->index_ = 0;

    return 0;
}

int32_t EV::periodic_start(int32_t id, int64_t after, int64_t repeat,
                           int32_t policy)
{
    assert(repeat >= 0);

    // 如果不支持try_emplace，使用std::forward_as_tuple实现
    auto p = periodic_mgr_.try_emplace(id, id, this);
    if (!p.second) return -1;

    EVTimer *w = &(p.first->second);

    /**
     * 传进来的单位是秒，要转换为毫秒。实际上精度是毫秒
     * 这些起始时间用system_now_而不是system_clock_
     *
     * 比如1秒定时器当前utc时间戳system_now_为100，
     * 实际utc时间system_clock_为100.5，那定时器应该在system_now_为101时触发
     */
    w->at_     = (system_now_ + after) * 1000;
    w->repeat_ = repeat * 1000;
    w->policy_ = policy;

    ++periodic_cnt_;
    int32_t index = periodic_cnt_ + HEAP0 - 1;
    if (periodics_.size() < (size_t)index + 1)
    {
        periodics_.resize(index + 1024, nullptr);
    }

    periodics_[index] = w;
    up_heap(periodics_.data(), index);

    assert(index >= 1 && periodics_[w->index_] == w);

    return index;
}

int32_t EV::periodic_stop(int32_t id)
{
    auto found = periodic_mgr_.find(id);
    if (found == periodic_mgr_.end())
    {
        return -1;
    }

    periodic_stop(&(found->second));

    periodic_mgr_.erase(found);

    return 0;
}

int32_t EV::periodic_stop(EVTimer *w)
{
    del_pending(w);
    if (unlikely(!w->index_)) return 0;

    {
        int32_t index = w->index_;

        assert(periodics_[index] == w);

        --periodic_cnt_;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (likely(index < periodic_cnt_ + HEAP0))
        {
            // 把当前最后一个timer(timercnt_ + HEAP0)覆盖当前timer的位置，再重新调整
            periodics_[index] = periodics_[periodic_cnt_ + HEAP0];
            adjust_heap(periodics_.data(), periodic_cnt_, index);
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
        c += c + 1 < N + HEAP0 && (heap[c])->at_ > (heap[c + 1])->at_ ? 1 : 0;

        if (he->at_ <= (heap[c])->at_) break;

        heap[k]           = heap[c];
        (heap[k])->index_ = k;
        k                 = c;
    }

    heap[k]    = he;
    he->index_ = k;
}

void EV::up_heap(HeapNode *heap, int32_t k)
{
    HeapNode he = heap[k];

    for (;;)
    {
        int p = HPARENT(k);

        if (UPHEAP_DONE(p, k) || (heap[p])->at_ <= he->at_) break;

        heap[k]           = heap[p];
        (heap[k])->index_ = k;
        k                 = p;
    }

    heap[k]    = he;
    he->index_ = k;
}

void EV::adjust_heap(HeapNode *heap, int32_t N, int32_t k)
{
    if (k > HEAP0 && (heap[k])->at_ <= (heap[HPARENT(k)])->at_)
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

void EV::append_event(EVIO *w, int32_t ev)
{
    int32_t flag = events_.append_backend_event(w, ev);
    if (0 == flag) return;

    backend_->wake();
    if (2 == flag)
    {
        fd_mgr_.for_each(
            [w](EVIO *watcher)
            {
                if (w != watcher) watcher->ev_counter_ = 0;
            });
    }
}
