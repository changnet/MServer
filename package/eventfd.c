#include <sys/eventfd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#define handle_error(msg)     do { perror(msg); exit(1); } while (0)

int main( int argc, char **argv )
{
    uint64_t u;
    ssize_t s;
    int j;
    if ( argc < 2 ) {
        fprintf(stderr, "input <num> in command argument");
        exit(1);
    }

    int efd;
    if ( (efd = eventfd(0, EFD_NONBLOCK)) == -1 )
    handle_error("eventfd failed");
    
    switch (fork()) {
        case 0:
            for( j = 1; j < argc; j ++ ) {
                printf("Child writing %s to efd\n", argv[j] );
            
                u = strtoull(argv[j], NULL, 0);  /* analogesly atoi */
                s = write(efd, &u, sizeof(uint64_t)); /* append u to counter */ 
                if ( s != sizeof(uint64_t) )
                    handle_error("write efd failed");
                    
            }
            printf("child completed write loop\n");

            exit(0);
        default:
            sleep (2);
            
            printf("parent about to read\n");
            u = strtoull("2", NULL, 0);  /* analogesly atoi */
            write( efd,&u,sizeof(uint64_t) );
            s = read(efd, &u, sizeof(uint64_t));
            if ( s != sizeof(uint64_t) ) {
                if (errno = EAGAIN) {
                    printf("Parent read value %d\n", s);
                    return 1;
                }
                handle_error("parent read failed");
            }
            printf("parent read %d , %llu (0x%llx) from efd\n", 
                    s, (unsigned long long)u, (unsigned long long) u);
            exit(0);

        case -1:
            handle_error("fork ");
    }
    return 0;
}
