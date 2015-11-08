#include <iostream>
#include "../doubly_link.h"
#include "lru.h"

struct Info
{
    int a;
    char x[64];
};

void p(class doubly_link<double> &link)
{
    std::cout << "size  " << link.size() << std::endl;
    class doubly_link<double>::node<double> *node = link.begin();
    while ( node )
    {
        std::cout << "while: " << node->value() << std::endl;
        node = node->next();
    }
}

int main()
{
    class doubly_link<double> link;
    link.push_front( 99.5 );
    link.push_back( 68363 );

    p( link );

    std::cout << *link.front() << std::endl;
    link.pop_front();
    // std::cout << *link.front() << std::endl;
    // std::cout << *link.back() << std::endl;
p( link );
    link.insert( link.begin(),666 );
    link.push_back( 8447.755 );
    link.push_front( 8884 );
    link.erase( link.begin() );
    link.erase( link.end() );
    link.push_back( 8455 );
    link.push_front( 18884 );

    class doubly_link<double>::node<double> *e = link.end();
    e = e->last();
    link.insert( e,9987.9 );
    link.erase( e );
    //link.pop_back();

    p( link );

    class lru<int> _lru;
    _lru.update( 1 );
    _lru.update( 2 );
    _lru.update( 3 );
    _lru.update( 4 );
    _lru.update( 5 );
    _lru.update( 6 );
    _lru.update( 6 );
    std::cout << "oldest: " << *(_lru.oldest()) << std::endl;
    _lru.update( 1 );
    _lru.update( 1 );
    std::cout << "oldest: " << *(_lru.oldest()) << std::endl;
    std::cout << "done" << std::endl;
    return 0;
}
