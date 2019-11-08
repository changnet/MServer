#pragma once

// 内存调试

// 在最开始的版本中，只用了一个内存计数器，是上一个项目中迁移过来的，来源无法考究

// 后来尝试加上内存跟踪，该方法来自:https://github.com/adah1972/nvwa
// 测试后发现，#define new dbg_mem_tracer(__FILE__, __LINE__) ->* new
// 这种宏定义解决了placement new的问题，不过没法解决
// ::operator new这种显式调用operator new的写法，而stl中有这种写法
// 作者给出的解决方案是必须把incclude dbg_mem.h放到其他头文件之后

// 这种方法限制较大，因此不用。还不如直接在operator new中记录所有调用堆栈
// 但是这和valgrind的massif有什么区别呢，因此我还是用massif
// 注：用massif时把内存计数器也关掉 xzc at 20191027

#include "types.h"

extern void global_mem_counter(int32_t &counter, int32_t &counters);

#ifndef NDBG_MEM_TRACE

    #define new dbg_mem_tracer(__FILE__, __LINE__)->*new

class DbgMemTracer
{
public:
    const char *_file;
    const int _line;

    void process(void *ptr)
    {
        /* 重载operator new函数，记录每一次分配的指针，以地址为Key放到一个unordered_map
         * 这里根据 ptr 可以查找对应的记录
         */
    }

public:
    explicit DbgMemTracer(const char *file, int line) : _file(file), _line(line)
    {
    }
    /**
     * Pointer-to-Member Operators: .* and ->*
     * 显式地通过对象指针调用成员函数，不过这里和它本身的功能并没关系
     *
     * dbg_mem_tracer(__FILE__, __LINE__) ->* new
     * 假如 new Test() 返回一个 Test类型的指针ptr，即
     * dbg_mem_tracer(__FILE__, __LINE__) ->* ptr
     * 就会调用下面的函数进行处理
     */
    template <class T> T *operator->*(T *ptr)
    {
        process(ptr);
        return ptr;
    }

private:
    DbgMemTracer(const DbgMemTracer &);
    DbgMemTracer &operator=(const DbgMemTracer &);
};

#endif /* NDBG_MEM_TRACE */
