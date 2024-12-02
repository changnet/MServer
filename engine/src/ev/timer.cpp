#include "timer.hpp"


#define HEAP0             1 // 二叉堆的位置0不用，数据从1开始存放
#define HPARENT(k)        ((k) >> 1)
#define UPHEAP_DONE(p, k) (!(p))

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

int32_t TimerMgr::timer_start(int32_t id, int64_t after, int64_t repeat, int32_t policy)
{
    assert(repeat >= 0);

    // 如果不支持try_emplace，使用std::forward_as_tuple实现
    auto p = timer_mgr_.try_emplace(id, id, this);
    if (!p.second) return -1;

    Timer *timer = &(p.first->second);

    timer->at_     = steady_clock_ + after;
    timer->repeat_ = repeat;
    timer->policy_ = policy;

    assert(timer->repeat_ >= 0);

    timers_.emplace_back(timer);
    int32_t index = (int32_t)timers_.size() + HEAP0 - 1;

    up_heap(timers_.data(), index);

    assert(index >= 1 && timers_[timer->index_] == timer);

    return index;
}
