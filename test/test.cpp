#include <iostream>
#include "lru_k.h"
#include "lru_kv.h"

int main()
{
    class lru_k<int> l;
    l.update( 1 );
    l.update( 4 );
    l.update( 5 );
    l.update( 6 );
    l.update( 8 );
    l.update( 1 );
    l.erase( 4 );
    
    std::cout << l.oldest() << std::endl;
    
    class lru_kv<int,double> lkv(3);
    lkv.insert( 1,998.8 );
    lkv.insert( 4,9934.8 );
    lkv.insert( 5,91238.8 );
    lkv.update( 4 );
    lkv.insert( 8,784.8 );
    lkv.insert( 19,222.8 );
    
    //std::cout << *(lkv.find(1)) << std::endl;
    std::cout << *(lkv.find(8)) << std::endl;
    std::cout << *(lkv.find(4)) << std::endl;
    std::cout << *(lkv.find(19)) << std::endl;
    return 0;
}
