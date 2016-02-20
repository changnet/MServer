
#include <stdio.h>
#include <stdlib.h>

const int s = sizeof(int);

#if s > 1
    #define test(x) printf("yes\n")
#else
    #define test(x) printf("no\n")
#endif

int main()
{
    test( x );
    printf("%d\n",s );
    return 0;
}
