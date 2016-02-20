
#include "../master/ev/ev.h"
#include "../master/ev/ev_watcher.h"

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>

static int *pipes;
static int num_pipes, num_active,num_read;
static struct ev_io *evio;
ev_loop *loop;

#define BUFF_LEN  16

const char *str = "e";
const int str_len = strlen(str);

void read_cb(struct ev_io &w, int revents)
{
  char buf[BUFF_LEN];
  int ret = read( w.fd,buf,BUFF_LEN );
  if ( ret != str_len )
  {
	  perror("read_cb");
	  exit(1);
  }
  
  num_read ++;
  if ( num_read == num_active )
  {
	  loop->done();
  }
}

void run_once()
{
	num_read = 0;
	//往pipe写入数据激活其中一些ev_io
	//space尽可能使活动io平均分布
	int space = num_pipes / num_active;
	space = space * 2;
	for ( int i = 0; i < num_active; i++)
	{
		int ret = write( pipes[i * space + 1], str, str_len );
		if ( ret != str_len )
		{
			perror("run_once write");
			exit(1);
		}
	}

	clock_t start = clock();
	loop->run();
	clock_t stop = clock();
	
	printf( "time cost %f\n",float(stop-start)/CLOCKS_PER_SEC );
}

int main (int argc, char **argv)
{
	num_pipes = 5000;
	num_active = 1000;

	struct rlimit rl;
	rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
	{
		perror("setrlimit");
	}
	
	loop = new ev_loop();
	loop->init();

	evio  = new ev_io[num_pipes];
	pipes = new int[num_pipes*2];
	
	//创建对应数量的pipe
	int i = 0;
	int *cp = pipes;
	for ( ;i < num_pipes;i ++,cp += 2 )
	{
#ifdef USE_PIPES
		if (pipe(cp) == -1)
		{
#else
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, cp) == -1)
		{
#endif
			perror("pipe");
			exit(1);
		}
	
		ev_io *w = &(evio[i]);
		w->set<read_cb>();
		w->set( loop );
		w->start( *cp,EV_READ );  //监听读端口，待会从写端口写入数据
	}

	run_once();

	exit(0);
}
