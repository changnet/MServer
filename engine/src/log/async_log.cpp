#include "global/global.hpp"
#include "async_log.hpp"
#include "system/static_global.hpp"

#include <filesystem>

////////////////////////////////////////////////////////////////////////////////
AsyncLog::Policy::Policy()
{
    data_  = 0;
    data2_ = 0;
    file_  = nullptr;
    type_  = PT_NONE;
}
AsyncLog::Policy::~Policy()
{
    close_stream();
}

void AsyncLog::Policy::close_stream()
{
    if (file_)
    {
        ::fflush(file_);
        ::fclose(file_);
        file_ = nullptr;
    }
}

time_t AsyncLog::Policy::day_begin(time_t now)
{
    struct tm ntm;
    ::localtime_r(&now, &ntm);
    ntm.tm_hour = 0;
    ntm.tm_min  = 0;
    ntm.tm_sec  = 0;

    return std::mktime(&ntm);
}

void AsyncLog::Policy::trigger_daily_rollover(int64_t now)
{
    // 不是按天写入，或者还在同一天now < 0表示这个是拼接到上一个缓冲区的日志，不需要检测
    if (PT_DAILY != type_ || now < 0 || (now >= data_ && now <= data_ + 86400))
    {
        return;
    }

    close_stream();

    struct tm ntm;
    ::localtime_r(&data_, &ntm);

    // 修正文件名 runtime 转换为 runtime20210228
    char new_path[256];
    snprintf(new_path, sizeof(new_path), "%s%04d%02d%02d", path_.c_str(),
             ntm.tm_year + 1900, ntm.tm_mon + 1, ntm.tm_mday);

    data_ = day_begin(now);

    std::error_code e;
    bool ok = std::filesystem::exists(path_, e);
    if (e)
    {
        ELOG_R("rename daily check file exist file error %s %s", path_.c_str(),
               e.message().c_str());
        return;
    }
    if (ok)
    {
        std::filesystem::rename(path_, new_path, e);
        if (e)
        {
            ELOG_R("rename daily log file error %s %s", path_.c_str(),
                   e.message().c_str());
        }
    }
}

void AsyncLog::Policy::trigger_size_rollover(int64_t size)
{
    if (PT_SIZE != type_) return;

    data2_ += size;
    if (data2_ < data_) return;

    data2_ = 0;
    close_stream();

    std::string old_path;
    std::string new_path;

    // TODO C++20之后直接用std::string::format而不需要用c的snprintf
    char old_buff[1024];
    char new_buff[1024];

    std::error_code e;
    int32_t max_index = 0;
    for (int32_t i = max_index + 1; i < 1024; i++)
    {
        snprintf(old_buff, sizeof(old_buff), "%s.%03d", path_.c_str(), i);

        old_path.assign(old_buff);
        bool ok = std::filesystem::exists(old_path, e);
        if (e)
        {
            ELOG_R("rename size check file exist file error %s %s",
                   path_.c_str(), e.message().c_str());
            return;
        }

        if (!ok) break;

        max_index++;
    }

    for (int32_t i = max_index + 1; i > 1; i--)
    {
        snprintf(new_buff, sizeof(new_buff), "%s.%03d", path_.c_str(), i);
        snprintf(old_buff, sizeof(old_buff), "%s.%03d", path_.c_str(), i - 1);

        new_path.assign(new_buff);
        old_path.assign(old_buff);
        std::filesystem::rename(old_path, new_path, e);
        if (e)
        {
            ELOG_R("rename size file error %s %s", new_path.c_str(),
                   e.message().c_str());
            return;
        }
    }

    // 把runtime命名为runtime.1
    bool ok = std::filesystem::exists(path_, e);
    if (e)
    {
        ELOG_R("rename size check current file exist file error %s %s",
               path_.c_str(), e.message().c_str());
        return;
    }

    if (ok)
    {
        snprintf(new_buff, sizeof(new_buff), "%s.%03d", path_.c_str(), 1);
        new_path.assign(new_buff);
        std::filesystem::rename(path_, new_path, e);
        if (e)
        {
            ELOG_R("rename size current file error %s %s", path_.c_str(),
                   e.message().c_str());
            return;
        }
    }
}

bool AsyncLog::Policy::init_size_policy(int64_t size)
{
    data_ = size;
    if (data_ <= 0)
    {
        data_ = 1024 * 1024 * 10;
        ELOG_R("size illegal, reset to default 1024 * 1024 * 10");
    }

    // 读取文件大小
    std::error_code e;
    bool ok = std::filesystem::exists(path_, e);
    if (e)
    {
        ELOG_R("init size policy check file exist file error %s %s",
               path_.c_str(), e.message().c_str());
        return true;
    }
    if (ok)
    {
        data2_ = std::filesystem::file_size(path_, e);
        if (e)
        {
            ELOG_R("init size policy file size exist file error %s %s",
                   path_.c_str(), e.message().c_str());
            return true;
        }
    }

    return true;
}

bool AsyncLog::Policy::init_daily_policy()
{
    data_ = StaticGlobal::ev()->now();

    // 读取已有日志文件的日期
    std::error_code e;
    bool ok = std::filesystem::exists(path_, e);
    if (e)
    {
        ELOG_R("rename daily initfile exist file error %s %s", path_.c_str(),
               e.message().c_str());
        ok = false; // 即使获取不了上次文件的时间，也按天切分文件
    }
    if (ok)
    {
        auto ftime = std::filesystem::last_write_time(path_);
#ifdef __windows__
        // https://developercommunity.visualstudio.com/t/stdfilesystemfile-time-type-does-not-allow-easy-co/251213
        // windows的文件时间戳从1601年开始，在C++20之前，无法用clock_cast把ftime转换为time_t
        // Between Jan 1, 1601 and Jan 1, 1970 there are 11644473600 seconds
        static_assert(__cplusplus < 202002L);
        using namespace std::chrono_literals;
        data_ = std::chrono::duration_cast<std::chrono::seconds>(
                    ftime.time_since_epoch() - 11644473600s)
                    .count();
#else
        data_ = decltype(ftime)::clock::to_time_t(ftime);
#endif
    }

    data_ = day_begin(data_);
    return true;
}

void AsyncLog::Policy::init_policy(const char *path, int32_t type, int64_t opt_val)
{
    path_.assign(path);
    type_ = static_cast<Policy::PolicyType>(type);
    switch (type)
    {
    case PT_DAILY: init_daily_policy(); break;
    case PT_SIZE: init_size_policy(opt_val); break;
    default: break;
    }
}

FILE *AsyncLog::Policy::open_stream(const char *path)
{
    if (PT_NONE == type_) path_.assign(path);

    if (!file_)
    {
        file_ = ::fopen(path_.c_str(), "ab+");
        if (!file_)
        {
            ELOG_R("can't open log file(%s):%s\n", path_.c_str(),
                   strerror(errno));
            return nullptr;
        }
    }

    return file_;
}
////////////////////////////////////////////////////////////////////////////////
void AsyncLog::set_policy(const char *path, int32_t type, int64_t opt_val)
{
    std::lock_guard<std::mutex> guard(mutex_);

    // 这里面可能会触发磁盘io操作，会比较慢
    Device &device = device_[path];
    device.policy_.init_policy(path, type, opt_val);
}

void AsyncLog::append(const char *path, LogType type, int64_t time,
                      const char *ctx, size_t len)
{
    assert(path);

    // 在thread_local上用gdb断点，会触发SIGSEGV，直到gcc 9.1才修复
    // 不在这个位置断点程序可正常运行
    // https://stackoverflow.com/questions/33429912/program-compiled-with-fpic-crashes-while-stepping-over-thread-local-variable-in
    thread_local std::string str_path;
    str_path.assign(path);

    std::lock_guard<std::mutex> guard(mutex_);
    Device &device = device_[str_path];
    Buffer *buff   = device_reserve(device, time, type);

    size_t cpy_len = std::min(sizeof(buff->buff_), len);
    memcpy(buff->buff_, ctx, cpy_len);
    buff->used_ += cpy_len;

    // 如果一个缓冲区放不下，后面接多个缓冲区，时间戳为-1
    if (unlikely(cpy_len < len))
    {
        size_t cur_len = cpy_len;
        do
        {
            buff = device_reserve(device, -1, type);

            cpy_len = std::min(sizeof(buff->buff_), len - cur_len);
            memcpy(buff->buff_, ctx + cur_len, cpy_len);

            cur_len += cpy_len;
            buff->used_ += cpy_len;
        } while (cur_len < len);
    }
}

size_t AsyncLog::write_buffer(FILE *stream, const char *prefix,
                              const Buffer *buffer, bool beg, bool end)
{
    size_t bytes = 0;
    if (buffer->time_ >= 0)
    {
        if (!beg)
        {
            bytes++;
            fputc('\n', stream);
        }
        bytes += write_prefix(stream, prefix, buffer->time_);
    }

    bytes += fwrite(buffer->buff_, 1, buffer->used_, stream);

    if (end)
    {
        bytes++;
        fputc('\n', stream);
    }

#ifdef __windows__
    // 在win的MYSY2(包括git bash)会缓存输出，直到程序关闭或者缓存区满，因此需要手动刷新
    if (stdout == stream || stderr == stream) fflush(stream);
#endif

    return bytes;
}

void AsyncLog::write_device(Policy *policy, const BufferList &buffers,
                            const char *path)
{
    size_t size = buffers.size();
    for (size_t i = 0; i < size; i++)
    {
        size_t bytes         = 0;
        bool beg             = 0 == i;
        bool end             = i == size - 1;
        const Buffer *buffer = buffers[i];

        policy->trigger_daily_rollover(buffer->time_);
        FILE *stream = policy->open_stream(path);
        if (!stream)
        {
            ELOG_R("unable to open log stream: %s", path);
            continue;
        }
        switch (buffer->type_)
        {
        case LT_LOGFILE:
        {
            bytes = write_buffer(stream, "", buffer, beg, end);
            break;
        }
        case LT_LPRINTF:
        {
            bytes = write_buffer(stream, "LP", buffer, beg, end);
            if (!is_deamon()) write_buffer(stdout, "LP", buffer, beg, end);
            break;
        }
        case LT_LERROR:
        {
            bytes = write_buffer(stream, "LE", buffer, beg, end);
            if (!is_deamon()) write_buffer(stderr, "LE", buffer, beg, end);
            break;
        }
        case LT_CPRINTF:
        {
            bytes = write_buffer(stream, "CP", buffer, beg, end);
            if (!is_deamon()) write_buffer(stdout, "CP", buffer, beg, end);
            break;
        }
        case LT_CERROR:
        {
            bytes = write_buffer(stream, "CE", buffer, beg, end);
            if (!is_deamon()) write_buffer(stderr, "CE", buffer, beg, end);
            break;
        }
        case LT_FILE:
        {
            bytes = fwrite(buffer->buff_, 1, buffer->used_, stream);
            break;
        }
        default: assert(false); break;
        }

        policy->trigger_size_rollover(bytes);
    }
}

// 线程主循环
void AsyncLog::routine_once(int32_t ev)
{
    UNUSED(ev);

    // https://en.cppreference.com/w/cpp/container/unordered_map/erase
    // https://stackoverflow.com/questions/38468844/erasing-elements-from-unordered-map-in-a-loop
    // The order of the elements that are not erased is preserved. (This makes
    // it possible to erase individual elements while iterating through the
    // container.)
    // C++14以后，允许在循环中删除
    static_assert(__cplusplus > 201402L);

    bool busy = true;
    auto now  = StaticGlobal::ev()->now();

    std::unique_lock<std::mutex> ul(mutex_);

    while (busy)
    {
        busy = false;
        for (auto iter = device_.begin(); iter != device_.end(); iter++)
        {
            Device &device = iter->second;
            Policy &policy = device.policy_;
            if (!device.buff_.empty())
            {
                device.time_ = now;
                writing_buffers_.swap(device.buff_);

                ul.unlock();
                write_device(&policy, writing_buffers_, iter->first.c_str());
                ul.lock();

                // 回收缓冲区
                for (auto buffer : writing_buffers_)
                {
                    buffer_pool_.destroy(buffer);
                }
                writing_buffers_.clear();

                busy = true;
            }
            else
            {
                // 关闭长时间不使用的设备
                int64_t sec = now - device.time_;
                // 定时关闭文件，当缓存区没满时，不关闭是不会输出到文件的(用flush ??)
                if (sec > 10) policy.close_stream();

                auto type = policy.get_type();
                if (Policy::PT_DAILY == type && sec > 10)
                {
                    // 当没有日志写入时，10秒检测一次日期切换
                    device.time_ = now;
                    if (policy.is_daily_rollover(now))
                    {
                        ul.unlock();
                        policy.trigger_daily_rollover(now);
                        ul.lock();
                    }
                }
                else if (Policy::PT_NORMAL == type && sec > 300)
                {
                    iter = device_.erase(iter);
                }
            }
        }
    }
}

bool AsyncLog::uninitialize()
{
    routine_once(0);

    device_.clear(); // 保证文件句柄被销毁并写入文件

    return true;
}
