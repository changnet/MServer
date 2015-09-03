#include "global/global.h"
#include "ev/ev.h"
#include "ev/ev_watcher.h"

/* test class */

class CTest
{
private:
    ev_io    m_io;
    ev_timer m_timer;
    
    ev_loop *loop;
public:
    explicit CTest( ev_loop *loop)
    : loop(loop)
    {
        
    }

    void io_cb(ev_io &w,int revents)
    {
        if ( EV_ERROR & revents )
        {
            std::cerr << "ev error" << std::endl;
            w.stop();
            return;
        }
        
        std::cout << "io_cb ... break" << std::endl;
        w.stop();
    }

    void timer_cb(ev_timer &w,int revents)
    {
        if ( EV_ERROR & revents ) // ev error
        {
            std::cerr << "ev error" << std::endl;
            w.stop();
            return;
        }

        std::cout << "timer_cb ..." << std::endl;
    }

    void start()
    {
        m_io.set( loop );
        m_io.set<CTest,&CTest::io_cb>(this);
        m_io.start( 0,EV_READ );
        m_io.stop();
        m_io.start();
        
        m_timer.set(loop);
        m_timer.set<CTest,&CTest::timer_cb>(this);
        m_timer.start( 5 );
    }
};


void io_cb_ex(ev_io &w,int revents)
{
    if ( EV_ERROR & revents )
    {
        std::cerr << "ev error" << std::endl;
        w.stop();
        return;
    }
    
    std::cout << "io_cb_ex ... " << std::endl;
}

int main()
{
    //atexit(onexit);

    //assert( "no need to do this",99 == 33 );
    //struct ev_loop *loop = ev_loop_new();

    ev_loop loop;
    loop.init();

    CTest t( &loop );
    t.start();
    // ev_io io;
    // io.set( &loop );
    // io.set<io_cb_ex>();
    // io.start( 0,EV_WRITE );

    loop.run();

    std::cout << "done ..." << std::endl;
    return 0;
}
