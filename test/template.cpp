#include <iostream>

class base
{
public:
    int send(){}
};

class derived : public base
{
public:
    using base::send;
};

template<class T>
class maker
{
public:
    typedef int (T::*pf_t)();

    template< pf_t pf >
    void def( const char *func_name )
    {
        // do something ...
    }
};

int main()
{
    // 函数不是通过继承而来，OK
    // class maker<base> mk_base;
    // mk_base.def<&base::send>( "send" );

    class maker<derived> mk;
    mk.def<&derived::send>( "send" );

    return 0;
}
