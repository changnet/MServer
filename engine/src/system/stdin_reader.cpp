#include "stdin_reader.hpp"
#include "thread/thread.hpp"
#include <iostream>
#ifndef __windows__
    #include <unistd.h> // STDIN_FILENO
    #include <sys/select.h>
    #include <cerrno>
    #include <termios.h>
    #include <vector>
    #include <cstdio>
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

#ifndef __windows__
/*
 * windows使用powershell时，只简单的用std::getline就可以实现箭头输入历史
 * Linux则是不行的。如果不需要输入历史，则简单的select一下直接std::getline即可
 * 如果需要输入历史，则需要开启终端的行模式，禁用canonical模式，禁用echo模式
 * 开启了之后就需要手动自己处理显示、回车、退格等，并且程序崩溃会导致当前终端
 * 的回显无法还原，只能重开一个终端或者使用reset及及stty sane命令恢复
 */
struct LinuxInputContext
{
    std::string current_line;
    std::vector<std::string> history;
    size_t history_idx = 0;
    fd_set rfds;
    struct timeval tv;
    struct termios orig_termios;
    bool termios_active = false;

    LinuxInputContext()
    {
        // 将终端设置为 raw 模式（禁用 canonical 模式和 echo），才能自定义输入的字符
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0)
        {
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            termios_active = true;
        }
    }

    ~LinuxInputContext()
    {
        if (termios_active)
        {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    }
};

static bool is_readable(int32_t usec)
{
    // read是同步的，如果不预先select，那么read会阻塞
    struct timeval timeout = {0, usec};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &timeout) > 0;
}

static bool try_read_char(char &c)
{
    // read是同步的，如果不预先select，那么read会阻塞
    if (!is_readable(0)) return false;

    return read(STDIN_FILENO, &c, 1) == 1;
}

static void do_arrow_key(char direction, LinuxInputContext &ctx)
{
    if (direction == 'A') // UP
    {
        if (ctx.history_idx > 0)
        {
            ctx.history_idx--;
            printf("\r\033[K%s", ctx.history[ctx.history_idx].c_str());
            ctx.current_line = ctx.history[ctx.history_idx];
            fflush(stdout);
        }
    }
    else if (direction == 'B') // DOWN
    {
        if (ctx.history_idx < ctx.history.size())
        {
            ctx.history_idx++;
            printf("\r\033[K");
            if (ctx.history_idx < ctx.history.size())
            {
                ctx.current_line = ctx.history[ctx.history_idx];
                printf("%s", ctx.current_line.c_str());
            }
            else
            {
                ctx.current_line.clear();
            }
            fflush(stdout);
        }
    }
}

static void do_escape_sequence(LinuxInputContext &ctx)
{
    // 向上箭头键是[A，向下箭头键是[B
    char c2;
    if (!try_read_char(c2) || c2 != '[') return;

    char c3;
    if (!try_read_char(c3)) return;

    if (c3 == 'A' || c3 == 'B')
    {
        do_arrow_key(c3, ctx);
    }
}

static bool do_input_char(char c, std::string &input, LinuxInputContext &ctx)
{
    if (c == 27) // ESC
    {
        do_escape_sequence(ctx);
    }
    else if (c == '\n' || c == '\r') // 回车Enter
    {
        printf("\n");
        input         = ctx.current_line;
        auto &history = ctx.history;
        if (!input.empty() && (history.empty() || input != history.back()))
        {
            if (history.size() > 128) history.erase(history.begin());
            history.push_back(input);
        }
        ctx.current_line.clear();
        ctx.history_idx = history.size();
        return true;
    }
    else if (c == 127 || c == 8) // Backspace
    {
        if (!ctx.current_line.empty())
        {
            ctx.current_line.pop_back();
            printf("\b \b");
            fflush(stdout);
        }
    }
    else if (static_cast<unsigned char>(c) >= 32)
    {
        ctx.current_line += c;
        putchar(c);
        fflush(stdout);
    }
    return false;
}

static bool select_read(std::string &input, LinuxInputContext &ctx)
{
    if (!is_readable(100 * 1000)) return false;

    char c;
    if (try_read_char(c))
    {
        return do_input_char(c, input, ctx);
    }
    return false;
}
#endif

void StdinReader::routine()
{
    Thread::apply_thread_name("stdin_reader");

    std::string input;

#ifndef __windows__
    LinuxInputContext ctx;
#endif

    while (!stop_)
    {
        input.clear();

#ifdef __windows__
        // std::cin >> input不支持空格，getline支持
        std::getline(std::cin, input);
#else
        if (!select_read(input, ctx))
        {
            continue;
        }
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