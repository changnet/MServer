#pragma once

#include <cassert>
#include <cstddef>

/**
 * @brief 惰性线程局部缓冲：thread_local 里只放一个指针，内存首次使用时才分配
 *
 * 直接写 thread_local char buf[65535]，在一些系统上线程创建线程时就会分配内存
 * 即使这个thread_local是一个函数局部变量，该线程也从不执行这个函数也会分配，原
 * 因是链接时，所有thread_local变量就会放到.tdata/.tbss段了，造成极大的浪费
 *
 * 这里把大数组换成指针：thread_local 只剩 sizeof(ThreadLocalBuf) = 16 字节
 * （指针 + 大小），真正的 char[] 在第一次取值时 new，线程退出时析构释放。
 *
 * ★ 必须声明成 thread_local，否则就退化成普通对象，失去按线程隔离的意义。
 *
 * 一、定长：只给一个模板参数，分配一次后大小永远不变
 *
 *     int32_t f()
 *     {
 *         thread_local ThreadLocalBuf<65535> buf;
 *         memcpy(buf, data, len);         // 隐式转 char*，和原生数组一样用
 *         buf[0] = 1;                     // 也能下标
 *         return (int32_t)buf.size();     // 恒为 65535
 *     }
 *
 * 二、变长：给最小值和常规上限，get(n) 时按需伸缩
 *
 *     thread_local ThreadLocalBuf<128 * 1024, 1024 * 1024> buf;
 *     char *p = buf.get(len);            // 保证 >= len
 *     memcpy(p, data, len);
 *
 * 变长的伸缩规则：
 *   - 不够就扩：按 2 的幂向上取整，且不低于 MinSize
 *   - 过大就缩：当前大小超过 4 倍需求量才缩（迟滞，防止 n 在边界抖动时反复
 *     new/delete），缩到 2 倍需求量，且不低于 MinSize、不高于 MaxSize
 *   - 扩容不受 MaxSize 限制（必须满足 n，否则就是缓冲区溢出），MaxSize 只约束
 *     缩容的目标，保证不会长期占着一块超大内存
 *
 * MinSize 的取值要考虑碎片：glibc 的 mmap 阈值是 128KB，>= 128KB 的分配 free
 * 时直接 munmap 归还系统，不会在堆上留下碎片；小于它走 brk，反复 new/delete
 * 容易把堆打碎。本项目历史惯例是 256KB（见 net/buffer.cpp 的 ThreadBuffer）。
 *
 */


/**
 * @brief 惰性线程局部缓冲，替代原生thread_local buffer[]
 */
template <size_t MinSize, size_t MaxSize = MinSize>
class ThreadLocalBuf
{
public:
    enum : size_t
    {
        MIN_SIZE = MinSize,
        MAX_SIZE = MaxSize
    };
    /// 定长模式：MinSize == MaxSize，分配一次后大小永远不变
    static constexpr bool IS_FIXED = (MinSize == MaxSize);

    static_assert(MaxSize >= MinSize, "MaxSize must >= MinSize");

    /// 默认构造是 constexpr：thread_local 实例走常量初始化，不生成线程启动时的
    /// 初始化代码，TLS 里就只是这两个字段，真正的内存留给第一次 get()
    ThreadLocalBuf() = default;

    /// 变长模式取缓冲，保证至少 n 字节
    char *get(size_t n = MIN_SIZE)
    {
        if (size_ >= n) return ptr_;

        return ensure(n);
    }

    /// 当前缓冲大小
    size_t size() const { return size_; }

    ~ThreadLocalBuf()
    {
        // delete 一个 nullptr 是安全的，但 dbg_mem.cpp 那边计数会出错
        if (ptr_) delete[] ptr_;
    }

    ThreadLocalBuf(const ThreadLocalBuf &)            = delete;
    ThreadLocalBuf &operator=(const ThreadLocalBuf &) = delete;

private:
    char *ensure(size_t n)
    {
        if (n < MIN_SIZE) n = MIN_SIZE;

        if (n > size_)
        {
            realloc(IS_FIXED ? (size_t)MIN_SIZE : round_to(n));
        }
        else if (!IS_FIXED && size_ > MIN_SIZE && size_ > n * 4)
        {
            // 当前缓冲区过大，缩小以节省内存
            size_t want = round_to(n);
            if (want > MAX_SIZE) want = MAX_SIZE;
            if (want < n)        want = n;
            if (want != size_) realloc(want);
        }
        return ptr_;
    }

    void realloc(size_t n)
    {
        if (n == size_) return;

        if (ptr_) delete[] ptr_;
        ptr_  = new char[n];
        size_ = n;
    }

    /// 取整：小尺寸取 2 的幂，避免来回抖动时频繁 realloc；
    /// 超过 ROUND_STEP 后改成按 ROUND_STEP 对齐 —— 否则 50MB 的请求会被
    /// pow2 一路抬到 64MB，凭空多出 28%
    static constexpr size_t round_to(size_t n)
    {
        size_t want = n <= ROUND_STEP ? pow2_ceil(n) : round_up(n, ROUND_STEP);
        return want < MIN_SIZE ? MIN_SIZE : want;
    }

    static constexpr size_t round_up(size_t n, size_t step)
    {
        return ((n + step - 1) / step) * step;
    }
    static constexpr size_t pow2_ceil(size_t n)
    {
        size_t v = 1;
        while (v < n) v <<= 1;
        return v;
    }

    enum : size_t
    {
        /// 大块取整粒度（1MB）：大尺寸不再按 2 的幂，避免 50MB 变 64MB
        ROUND_STEP = 1024 * 1024
    };

private:
    char  *ptr_  = nullptr;
    size_t size_ = 0;
};
