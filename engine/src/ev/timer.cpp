#include "timer.hpp"


#define HEAP0             1 // 二叉堆的位置0不用，数据从1开始存放
#define HPARENT(k)        ((k) >> 1)
#define UPHEAP_DONE(p, k) (!(p))

TimerMgr::HeapTimer::HeapTimer()
{
    size_ = 0;
    capacity_ = 512;
    list_     = new HeapNode[capacity_]();
}

TimerMgr::HeapTimer::~HeapTimer()
{
    delete[] list_;
}

TimerMgr::TimerMgr()
{
}

TimerMgr::~TimerMgr()
{
}

void TimerMgr::down_heap(HeapNode *heap, int32_t N, int32_t k)
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

void TimerMgr::up_heap(HeapNode *heap, int32_t k)
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


void TimerMgr::adjust_heap(HeapNode *heap, int32_t N, int32_t k)
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

void TimerMgr::reheap(HeapNode *heap, int32_t N)
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

void TimerMgr::delete_heap(HeapTimer &ht, Timer *timer)
{
    if (likely(timer->index_))
    {
        int32_t index = timer->index_;
        auto list     = ht.list_;
        assert(list[index] == timer);

        int32_t size = ht.size_ - 1;

        // 如果这个定时器刚好在最后，就不用调整二叉堆
        if (likely(index < size + HEAP0))
        {
            // 把当前最后一个timer(timercnt_ + HEAP0)覆盖当前timer的位置，再重新调整
            list[index] = list[size + HEAP0];
            adjust_heap(list, size, index);
        }
        ht.size_ = size;
    }
}

int32_t TimerMgr::delete_heap(HeapTimer &ht, int32_t id)
{
    auto& hash  = ht.hash_;
    auto found = hash.find(id);
    if (found == hash.end())
    {
        return -1;
    }

    delete_heap(ht, &(found->second));

    hash.erase(found);
    return 0;
}

int64_t TimerMgr::next_heap_interval(HeapTimer &ht, int64_t now)
{
    if (ht.size_)
    {
        // wait时间不超过下一个定时器触发时间
        return (ht.list_[HEAP0])->at_ - now;
    }

    return -1;
}

int32_t TimerMgr::new_heap(HeapTimer &ht, int64_t now, int32_t id,
                           int64_t after, int64_t repeat, int32_t policy)
{
    assert(repeat >= 0);

    // 如果不支持try_emplace，使用std::forward_as_tuple实现
    auto p = ht.hash_.try_emplace(id, id, this);
    if (!p.second) return -1;

    Timer *timer = &(p.first->second);

    timer->at_     = now + after;
    timer->repeat_ = repeat;
    timer->policy_ = policy;

    assert(timer->repeat_ >= 0);

    ++ht.size_;
    int32_t index = ht.size_ + HEAP0 - 1;
    if (unlikely(ht.capacity_ < (size_t)index + 1))
    {
        ht.capacity_ += 512;
        HeapNode *new_timers = new HeapNode[ht.capacity_]();
        // 上面index + 1，最末总有一个空闲，所以这里memcpy时不用size - 1
        memcpy(new_timers, ht.list_, sizeof(HeapNode) * ht.size_);
        ht.list_ = new_timers;
    }

    ht.list_[index] = timer;
    up_heap(ht.list_, index);

    assert(index >= 1 && ht.list_[timer->index_] == timer);

    return index;
}

void TimerMgr::heap_timeout(HeapTimer &ht, int64_t now)
{
    auto list = ht.list_;
    while (ht.size_ && (list[HEAP0])->at_ <= now)
    {
        Timer *timer = list[HEAP0];

        assert(timer->index_);

        if (timer->repeat_)
        {
            timer->at_ += timer->repeat_;

            // 如果时间出现偏差，重新调整定时器
            if (unlikely(timer->at_ < now)) timer_reschedule(timer, now);

            assert(timer->repeat_ > 0);

            down_heap(list, ht.size_, HEAP0);
        }
        else
        {
            // 不重复的定义器，删除
            delete_heap(ht, timer);
            ht.hash_.erase(timer->id_);
        }

        // add_pending(w, EV_TIMER);
    }
}

void TimerMgr::timer_reschedule(Timer *timer, int64_t now)
{
    /**
     * 当前用的是CLOCK_MONOTONIC时间，所以不存在用户调时间的问题
     * 但可能存在卡主循环的情况，libev默认情况下是修正为当前时间
     */
    switch (timer->policy_)
    {
    case P_ALIGN:
    {
        // 严格对齐到特定时间，比如一个定时器每5秒触发一次，那必须是在 0 5 10 15
        // 触发 即使主线程卡了，也不允许在其他秒数触发
        assert(timer->repeat_ > 0);
        while (timer->at_ < now) timer->at_ += timer->repeat_;
        break;
    }
    case P_SPIN:
    {
        // 自旋到时间恢复正常
        // 假如定时器1秒触发1次，现在过了5秒，则会在很短时间内触发5次回调
        break;
    }
    default:
    {
        // 按当前时间重新计时，这是最常用的定时器，libev、libevent都是这种处理方式
        timer->at_ = now;
        break;
    }
    }
}

int32_t TimerMgr::timer_start(int64_t now, int32_t id, int64_t after, int64_t repeat, int32_t policy)
{
    return new_heap(timer_, now, id, after, repeat, policy);
}


int32_t TimerMgr::timer_stop(int32_t id)
{
    return delete_heap(timer_, id);
}

int32_t TimerMgr::periodic_start(int64_t now, int32_t id, int64_t after,
                                 int64_t repeat, int32_t policy)
{
    return new_heap(periodic_, now, id, after, repeat, policy);
}

int32_t TimerMgr::periodic_stop(int32_t id)
{
    return delete_heap(periodic_, id);
}

void TimerMgr::update_timeout(int64_t now, int64_t utc)
{
    heap_timeout(timer_, now);
    heap_timeout(periodic_, utc);
}

int64_t TimerMgr::next_interval(int64_t now, int64_t utc)
{
    int64_t ni1 = next_heap_interval(timer_, now);
    int64_t ni2 = next_heap_interval(periodic_, utc);

    return ni1 < ni2 ? ni1 : ni2;
}