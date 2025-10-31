#include "stdin_reader.hpp"
#include "thread/thread.hpp"
#include <iostream>
#ifndef __windows__
    #include <unistd.h> // STDIN_FILENO
    #include <sys/select.h>
    #include <cerrno>
#endif

/*
1. 简单地用std::cin >> str或者std::getline，没有输入时，会阻塞并且无法唤醒
2. 在独立线程或者用std::async调用std::cin，这个线程将会永远阻塞并且无法唤醒
3. linux下相当简单，因为linux下stdin是一个文件设备，可以设置为异步。但windows不行
3. windows使用_kbhit()、WaitForSingleObject()等函数可以检测控制台是否有输入
    但这个方案仅限于检测，并不会读取数据
    需要调用getchar()、ReadConsoleInput等异步读取接口，自己处理输入(比如是否回车，是否删除)
    检测到有输入后，直接调用std::cin也是不行的，那样用户只要不按回车，一样阻塞
4. 终端输入是一个很复杂的流程。比如没有调用相关函数(比如std::cin)激活输入流时，控制台根本无法输入任何字符
    使用非阻塞方案，需要设置控制台的行显模式、回显模式
    因此希望使用_kbhit等异步函数时，要很频繁地检测
    获得输入字符后，还是自定义字符怎么显示出来，否则控制台上没有任何显示
    自定义显示出来后，还需要考虑backspace删除字符
5.
使用独立线程读，然后用TerminateThread强制销毁线程，这个是可行的。只是强制销毁线程会导致内存泄漏，有些工具可能会报问题
6. 使用独立线程读，然后另一个线程往stdin写入数据，或者是关闭stdin来唤醒阻塞的线程
    linux下可以直接write或者close就行，但这样是不会唤醒std::getline的
    windows下，stdin不是文件，无法写入。WriteConsoleInput、CloseHandle(GetStdHandle(STD_INPUT_HANDLE))是无法唤醒std::cin
*/

StdinReader::StdinReader()
{
    readed_ = true;
    stop_   = true;
}

StdinReader::~StdinReader()
{
    stop();
}

static int32_t select_read(std::string &input)
{
#ifndef __windows__
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    // linux下没什么好办法唤醒getline的阻塞，直接等超时
    // 如果需要快速唤醒，需要创建一个pipe加入select，主线程通过pipe写入数据唤醒
    timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100 * 1000; // 100ms

    int rc = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
    if (rc < 0)
    {
        if (errno == EINTR) return 0;

        return errno;
    }
    else if (rc == 0)
    {
        return 0; // 超时
    }

    if (FD_ISSET(STDIN_FILENO, &rfds))
    {
        if (!std::getline(std::cin, input)) return -1;
    }
#endif
    return 0;
}

void StdinReader::routine()
{
    Thread::apply_thread_name("stdin_reader");

    std::string input;
    while (!stop_)
    {
        input.clear();

#ifdef __windows__
        // std::cin >> input不支持空格，getline支持
        std::getline(std::cin, input);
#else
        select_read(input);
#endif

        // 仅仅是调试用，所以如果主线程来不及把input取走，将会被覆盖
        if (!input.empty())
        {
            std::scoped_lock sl(mutex_);
            readed_ = false;
            input_  = input;
        }
    }
}

bool StdinReader::start()
{
    if (thread_.joinable()) return false;

    stop_   = false;
    readed_ = true;
    thread_ = std::thread(&StdinReader::routine, this);

    return true;
}

bool StdinReader::stop()
{
    if (stop_) return false;

    stop_ = true;

#ifdef __windows__
    /*
    在读取线程阻塞于 std::getline(std::cin, ...) 时，底层会在 STD_INPUT_HANDLE
    上做同步 ReadFile。 主线程退出时调用CancelSynchronousIo(reader_thread_handle) 
    取消该线程中的同步 IO，getline 会立刻返回失败（通常映射为 ERROR_OPERATION_ABORTED），
    流置failbit，线程即可检测到并安全退出
    */
    // 取消该线程内挂起的同步 I/O（如 ReadFile on STD_INPUT_HANDLE）
    CancelSynchronousIo(thread_.native_handle());

    // 如果系统比较旧，FreeConsole() 后对 STDIN 的读取也会失败从而唤醒
#else
    // 可以强制终止，但可能会引起一些内存泄漏，那就什么都不做，等超时
    // pthread_cancel(thread_.native_handle());
#endif

    if (thread_.joinable()) thread_.join();

    return true;
}
const char *StdinReader::read()
{
    std::scoped_lock sl(mutex_);
    if (readed_) return nullptr;

    readed_ = true;
    return input_.c_str();
}