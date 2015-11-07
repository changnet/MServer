#include <iostream>
#include "doubly_link.h"

int main()
{
    class doubly_link<double> link;
    link.push_front( 99.5 );
    link.push_back( 68363 );
    std::cout << link.pop_front() << std::endl;
    std::cout << link.pop_front() << std::endl;
    return 0;
}
