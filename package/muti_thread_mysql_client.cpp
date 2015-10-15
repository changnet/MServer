/*
    muti_thread_mysql_client_pool.cpp
    
执行
make muti_thread_mysql_client_pool LDFLAGS="-g -O2 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lssl -lcrypto -ldl"

即可获得对应单线程二进制。

上述代码就是利用队列来保持mysql连接，达到优化连接数。

总结
mysql连接与多线程处理不好，可能会造成很多问题，如

*MySQL Connection failed (#2058): This handle is already connected. Use a separate handle for each connection.*
Error in my_thread_global_end(): 1 threads didn't exit
甚至出现coredump
关于多线程连接mysql优化的思想，其实可以扩展到其他连接，如HTTP、Socket等连接中；
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <string>

#define THREAD_NUM  4
#define DBHOST      "localhost"
#define DBUSER      "pca"
#define DBPASS      "pca"
#define DBPORT      3306
#define DBNAME      "dxponline"
#define DBSOCK      NULL //"/var/lib/mysql/mysql.sock"
#define DBPCNT      0

using namespace std;

class CBlockQueue;
typedef struct ThreadArgsST
{
    int id;
    pthread_t *thread_id;
    CBlockQueue *pQueue;
} ThreadArgs;

class CMutex
{
public:
    CMutex()
    {
        pthread_mutex_init(&_mutex, NULL);
    }
    ~CMutex()
    {
        pthread_mutex_destroy(&_mutex);
    }

    int32_t lock()
    {
        return pthread_mutex_lock(&_mutex);
    }

    int32_t unlock()
    {
        return pthread_mutex_unlock(&_mutex);
    }

    int32_t trylock()
    {
        return pthread_mutex_trylock(&_mutex);
    }

private:
    pthread_mutex_t _mutex;
};

class CGlobalFunction
{
public:
    static MYSQL *connect()
    {
        unsigned int timeout = 3000;
        mysql_thread_init();
        MYSQL *mysql = mysql_init(NULL);

        if (mysql == NULL)
        {
            printf("mysql init failed: %s\n", mysql_error(mysql));
            return NULL;
        }

        mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

        if (mysql_real_connect(mysql, DBHOST, DBUSER, DBPASS, DBNAME, DBPORT, DBSOCK, DBPCNT) == NULL)
        {
            printf("connect failed: %s\n", mysql_error(mysql));
            mysql_close(mysql);
            mysql_thread_end();
            return NULL;
        }

        printf("connect succssfully\n");
        return mysql;
    }
};

class CBlockQueue : public CMutex
{
public:
    CBlockQueue() : _size(512)
    {
    }
    ~CBlockQueue()
    {
    }
    void set_size(int size)
    {
        _size = size;
    }
    int size()
    {
        this->lock();
        int size = q.size();
        this->unlock();
        return size;
    }
    bool push(void *m)
    {
        this->lock();
        // TODO
        /*
        if (q.size() > _size)
        {
            this->unlock();
            fprintf(stderr, "[QUEUE_IS_FULL]queue size over limit from push: %d\n", _size);
            return false;
        }
        */
        q.push(m);
        this->unlock();
        return true;
    }

    void *pop()
    {
        this->lock();

        if (q.empty())
        {
            this->unlock();
            fprintf(stderr, "[QUEUE_IS_EMPTY]queue is no item from pop");
            return NULL;
        }

        void *m = q.front();
        q.pop();
        this->unlock();
        return m;
    }

private:
    queue q;
    int _size;
};

void *func(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    MYSQL_RES *result;
    MYSQL_ROW row;
    MYSQL_FIELD *field;
    bool pushed = true;
    unsigned int num_fields;
    unsigned int i;
    const char *pStatement = "SHOW TABLES";
    MYSQL *db = (MYSQL *)args->pQueue->pop();

    if (db == NULL)
    {
        db = CGlobalFunction::connect();

        if (db == NULL)
        {
            printf("[%ld][%d]mysql connect failed\n", *args->thread_id, args->id);
            return (void *)0;
        }
    }

    if (0 != mysql_real_query(db, pStatement, strlen(pStatement)))
    {
        printf("[%ld][%d]query failed: %s\n", *args->thread_id, args->id, mysql_error(db));
        args->pQueue->push(db);
        return (void *)0;
    }

    result = mysql_store_result(db);

    if (result == NULL)
    {
        printf("[%ld][%d]fetch result failed: %s\n", *args->thread_id, args->id, mysql_error(db));
        args->pQueue->push(db);
        return (void *)0;
    }

    num_fields = mysql_num_fields(result);
    printf("[%ld][%d]numbers of result: %d\n", *args->thread_id, args->id, num_fields);

    while (NULL != (field = mysql_fetch_field(result)))
    {
        printf("[%ld][%d]field name: %s\n", *args->thread_id, args->id, field->name);
    }

    while (NULL != (row = mysql_fetch_row(result)))
    {
        unsigned long *lengths;
        lengths = mysql_fetch_lengths(result);

        for (i = 0; i < num_fields; i++)
        {
            printf("[%ld][%d]{%.*s} ", *args->thread_id, args->id, (int) lengths[i], row[i] ? row[i] : "NULL");
        }

        printf("\n");
    }

    mysql_free_result(result);
    args->pQueue->push(db);
    return (void *)0;
}

int main(int argc, char *argv[])
{
    CBlockQueue queue;
    int thread_num;

    if (argc == 2)
    {
        thread_num = atoi(argv[1]);
    }
    else
    {
        thread_num = THREAD_NUM;
    }

    mysql_library_init(0, NULL, NULL);
    printf("argc: %d and thread_num: %d\n", argc, thread_num);

    do
    {
        int i;  
        pthread_t *pTh = new pthread_t[thread_num];
        ThreadArgs *pArgs = new ThreadArgs[thread_num];

        for (i = 0; i < thread_num; i ++)
        {
            pArgs[i].id = i;
            pArgs[i].thread_id = &pTh[i];
            pArgs[i].pQueue = &queue;

            if (0 != pthread_create(&pTh[i], NULL, func, &pArgs[i]))
            {
                printf("pthread_create failed\n");
                continue;
            }
        }

        for (i = 0; i < thread_num; i ++)
        {
            pthread_join(pTh[i], NULL);
        }

        delete[] pTh;
        delete[] pArgs;
        int qsize = queue.size();

        for (i = 0; i < qsize; i ++)
        {
            MYSQL *db = (MYSQL *)queue.pop();

            if (NULL != db)
            {
                mysql_close(db);
                mysql_thread_end();
            }
        }
    }
    while (0);

    mysql_library_end();
    return 0;
}
