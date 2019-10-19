#pragma once

// 内存调试

// 在最开始的版本中，只用了一个内存计数器，是上一个项目中迁移过来的，来源无法考究

// 现在加上内存跟踪，该方法来自:https://github.com/adah1972/nvwa
// 不过原项目直接集成过来过于沉重，部分我想要的功能也没有，因此我自己改了下  xzc at 20191019

#include "types.h"

extern void global_mem_counter(int32 &counter,int32 &counters);

#ifdef _DBG_MEM_TRACE

#define new dbg_mem_tracer(__FILE__, __LINE__) ->* new

class dbg_mem_tracer
{
public:
    const char* _file;
    const int   _line;

    void process(void* ptr);

public:
    dbg_mem_tracer(const char* file, int line) : _file(file), _line(line) {}
    /**
     * Pointer-to-Member Operators: .* and ->*
     * 显式地通过对象指针调用成员函数，不过这里和它本身的功能并没关系
     * 
     * dbg_mem_tracer(__FILE__, __LINE__) ->* new
     * 假如 new Test() 返回一个 Test类型的指针ptr，即
     * dbg_mem_tracer(__FILE__, __LINE__) ->* ptr
     * 就会调用下面的函数进行处理
     */
    template <class T> T* operator->*(T* ptr) { process(ptr); return ptr; }

private:
    dbg_mem_tracer(const dbg_mem_tracer&);
    dbg_mem_tracer& operator=(const dbg_mem_tracer&);
};

#endif /* _DBG_MEM_TRACE */
