#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdlib.h>
#include <sys/resource.h>

int
input_timeout (int filedes, unsigned int seconds)
{
  fd_set set;
  struct timeval timeout;
  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return select (filedes+1,&set, NULL, NULL,&timeout);
}
int
main (void)
{
    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = 65535;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
    {
        perror("setrlimit");
    }
    
    int fd = 0;
    
    int i = 0;
    for ( i = 0;i < 1048;i ++ )
    {
        fd = socket(AF_INET,SOCK_STREAM,0);
        if ( fd < 0 )
        {
            perror( "socket create fail:");
            exit( 1 );
        }
    }
    perror("start wait");
  fprintf (stderr, "max %d,select %d returned %d.\n",FD_SETSIZE,fd,
           input_timeout (fd, 5));
    perror( "exit:");
    sleep( 1 );
  return 0;
}
