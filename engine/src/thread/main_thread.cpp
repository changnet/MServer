#include "main_thread.hpp"
#include "lua_cpplib/llib.hpp"
#include "system/static_global.hpp"

// minimum timejump that gets detected (if monotonic clock available)
#define MIN_TIMEJUMP 1000

MainThread::MainThread()
{
    L_ = nullptr;

    clock_diff_      = 0;
    last_utc_update_ = INT_MIN;
    time_update();
}

MainThread::~MainThread()
{
    assert(!L_);
}

void MainThread::routinue()
{
    static const int64_t min_wait = 1;     // ��С�ȴ�ʱ�䣬����
    static const int64_t max_wait = 60000; // ���ȴ�ʱ�䣬����

    while (likely(!StaticGlobal::T))
    {
        time_update();

        int64_t wait_time = timer_mgr_.next_interval(steady_clock_, utc_ms_);
        if (-1 == wait_time) wait_time = max_wait;
        if (unlikely(wait_time < min_wait)) wait_time = min_wait;

        // �ȴ������̵߳�����
        tcv_.wait_for(wait_time);

        time_update();

        timer_mgr_.update_timeout(steady_clock_, utc_ms_);

        dispatch_message();
    }
}


void MainThread::time_update()
{
    steady_clock_ = steady_clock();

    /**
     * ֱ�Ӽ����UTCʱ�����ͨ��get_time��ȡ
     * ������ѭ��Ϊ5msʱ��0.5sͬ��һ��ʡ����100��ϵͳ����(get_time��һ��syscall���Ƚ���)
     * libevent��5��ͬ��һ��CLOCK_SYNC_INTERVAL��libev��0.5��
     */
    if (steady_clock_ - last_utc_update_ < MIN_TIMEJUMP / 2)
    {
        utc_ms_ = clock_diff_ + steady_clock_;
        utc_sec_ = utc_ms_ / 1000; // ת��Ϊ��
        return;
    }

    last_utc_update_ = steady_clock_;
    utc_ms_                   = system_clock();
    utc_sec_                  = utc_ms_ / 1000; // ת��Ϊ��

    /**
     * ������diff���Ƚϴ�ʱ��˵�����˵���UTCʱ��
     * ���ڻ�ȡʱ��(clock_gettime�Ⱥ�������һ��syscall�����Ż������ã����ܳ���ǰһ��
     * clock��ȡ����ʱ��Ϊ����ǰ����һ��clockΪ����������
     * ����ѭ�����α�֤ȡ���Ķ��ǵ������ʱ��
     *
     * �ο�libev
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
        clock_diff_ = utc_ms_ - steady_clock_;

        int64_t diff = old_diff - clock_diff_;
        if (likely((diff < 0 ? -diff : diff) < MIN_TIMEJUMP))
        {
            return;
        }

        utc_ms_     = system_clock();
        utc_sec_ = utc_ms_ / 1000; // ת��Ϊ��

        steady_clock_             = steady_clock();
        last_utc_update_ = steady_clock_;
    }
}

void MainThread::dispatch_message()
{
    try
    {
        ThreadMessage m = message_.pop();
    }
    catch (const std::out_of_range& e)
    {
        return;
    }
}