#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

/* 对象内存池
 * 1. 只提供基本的分配、缓存功能
 * 2. 会一次性申请大量对象缓存起来，但这些对象并不一定是连续的
 * 3. 对象申请出去后，内存池不再维护该对象，调用purge也不会释放该对象
 * 4. 回收对象时，不会检验该对象是否来自当前内存池
 */

template <typename T>
class object_pool
{
public:
    object_pool();
    ~object_pool();

    void purge();

    // 当对象池为空时，一次申请的对象数量
    void set_next_size(const uint32 size){ _next_size = size; };

    T *construct();
    void destroy(T *const obj,bool free = false);
private:
    T *_anpts;    /* 空闲对象数组 */
    uint32 _anptmax; /* 对象数组的最大长度 */
    uint32 _anptsize; /* 对象数组的当前长度 */
    uint32 _next_size; /* 下一次申请对象的数量 */
};

#endif /* __OBJECT_POOL_H__ */
