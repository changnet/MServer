#pragma once

#include "global/global.hpp"

class TimerMgr final
{
public:
    /// 当时间出现偏差时，定时器的调整策略
    enum Policy
    {
        P_NONE  = 0, /// 默认方式，调整为当前时间
        P_ALIGN = 1, /// 对齐到特定时间
        P_SPIN  = 2, /// 自旋
    };

public:
    TimerMgr();
    ~TimerMgr();
    /**
     * @brief 启动定时器
     * @param id 定时器唯一id
     * @param after N毫秒秒后第一次执行
     * @param repeat 重复执行间隔，毫秒数
     * @param policy 定时器重新规则时的策略
     * @return 成功返回》=1,失败返回值<0
     */
    int32_t timer_start(int64_t now, int32_t id, int64_t after, int64_t repeat,
                        int32_t policy);
    /**
     * @brief 停止定时器并从管理器中删除
     * @param id 定时器唯一id
     * @return 成功返回0
     */
    int32_t timer_stop(int32_t id);

    /**
     * @brief 启动utc定时器
     * @param id 定时器唯一id
     * @param after N秒后第一次执行
     * @param repeat 重复执行间隔，秒数
     * @param policy 定时器重新规则时的策略
     * @return 成功返回》=1,失败返回值<0
     */
    int32_t periodic_start(int64_t now, int32_t id, int64_t after,
                           int64_t repeat, int32_t policy);
    /**
     * @brief 停止utc定时器并从管理器删除
     * @param id 定时器唯一id
     * @return 成功返回0
     */
    int32_t periodic_stop(int32_t id);

private:
    struct Timer
    {
        int32_t id_;      // 唯一id
        int32_t pending_; // 在待处理watcher数组中的下标

        int32_t index_; // 当前定时器在二叉树数组中的下标
        int32_t policy_; // 修正定时器时间偏差策略，详见 reschedule 函数
        int64_t at_; // 定时器首次触发延迟的毫秒数（未激活），下次触发时间（已激活）
        int64_t repeat_; // 定时器重复的间隔（毫秒数）
    };
    using HeapNode = Timer *;
    struct HeapTimer
    {
    public:
        HeapTimer();
        ~HeapTimer();

    public:
        int32_t size_;
        int32_t capacity_;
        HeapNode *list_; // timer指针数组，按二叉树排列
        std::unordered_map<int32_t, Timer> hash_;
    };

    void down_heap(HeapNode *heap, int32_t N, int32_t k);
    void up_heap(HeapNode *heap, int32_t k);
    void reheap(HeapNode *heap, int32_t N);
    void adjust_heap(HeapNode *heap, int32_t N, int32_t k);

    int32_t delete_heap(HeapTimer &ht, int32_t id);
    int32_t new_heap(HeapTimer &ht, int64_t now, int32_t id, int64_t after,
                     int64_t repeat, int32_t policy);

private:
    HeapTimer timer_;
    HeapTimer periodic_;
};
