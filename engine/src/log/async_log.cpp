#include "global/global.hpp"
#include "async_log.hpp"
#include "ev/time.hpp"

#include <filesystem>

////////////////////////////////////////////////////////////////////////////////
AsyncLog::Device::Device()
{
    policy_     = 0;
    policy_ud1_ = 0;
    policy_ud2_ = 0;
    time_       = 0;
    file_       = nullptr;
    multi_device_ = nullptr;
}

AsyncLog::Device::~Device()
{
    close_stream();
}

void AsyncLog::Device::close_stream()
{
    if (file_)
    {
        ::fflush(file_);
        ::fclose(file_);
        file_ = nullptr;
    }
}

FILE *AsyncLog::Device::open_stream()
{
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
time_t AsyncLog::day_begin(time_t now)
{
    struct tm ntm;
    ::localtime_r(&now, &ntm);
    ntm.tm_hour = 0;
    ntm.tm_min  = 0;
    ntm.tm_sec  = 0;

    return std::mktime(&ntm);
}

bool AsyncLog::init_size_policy(Device &device)
{
    if (device.policy_ud1_ <= 0)
    {
        device.policy_ud1_ = 1024 * 1024 * 10;
        ELOG_R("size illegal, reset to default 1024 * 1024 * 10");
    }

    // 读取文件大小
    std::error_code e;
    const std::string &path = device.path_;
    bool ok = std::filesystem::exists(device.path_, e);
    if (e)
    {
        ELOG_R("init size policy check file exist file error %s %s",
               path.c_str(), e.message().c_str());
        return true;
    }
    if (ok)
    {
        device.policy_ud2_ = std::filesystem::file_size(path, e);
        if (e)
        {
            ELOG_R("init size policy file size exist file error %s %s",
                   path.c_str(), e.message().c_str());
            return true;
        }
    }

    return true;
}

bool AsyncLog::init_daily_policy(Device &device)
{
    std::error_code e;
    const std::string &path = device.path_;
    bool ok = std::filesystem::exists(path, e);
    if (e)
    {
        ELOG_R("rename daily initfile exist file error %s %s", path.c_str(),
               e.message().c_str());
        ok = false; // 即使获取不了上次文件的时间，也按天切分文件
    }
    int64_t old_time = 0;
    if (ok)
    {
        // 读取已有日志文件的日期
        auto ftime = std::filesystem::last_write_time(path);
#ifdef __windows__
        // https://developercommunity.visualstudio.com/t/stdfilesystemfile-time-type-does-not-allow-easy-co/251213
        // windows的文件时间戳从1601年开始，在C++20之前，无法用clock_cast把ftime转换为time_t
        // Between Jan 1, 1601 and Jan 1, 1970 there are 11644473600 seconds
        static_assert(__cplusplus < 202002L);
        using namespace std::chrono_literals;
        old_time = std::chrono::duration_cast<std::chrono::seconds>(
                    ftime.time_since_epoch() - 11644473600s)
                    .count();
#else
        old_time = decltype(ftime)::clock::to_time_t(ftime);
#endif
    }
    else
    {
        old_time = timing::try_frame_time();
    }

    device.policy_ud1_ = day_begin(old_time);
    return true;
}

void AsyncLog::set_device(const char *name, const char *path, int32_t alive,
                               int32_t policy, int64_t policy_u1)
{
    std::lock_guard<std::mutex> guard(mutex_);

    // 这里面可能会触发磁盘io操作，会比较慢
    // 曾经想过由log线程改动日志时才初始化日志，但那样的话跨天的日志会出现不写日志就不改文件名的情况
    Device &device = device_[name];

    // 路径可能有变化，先关闭文件(另一个线程可能正在写入，不能关闭)
    // device.close_stream();
    device.alive_time_ = alive;
    device.policy_     = policy;
    device.policy_ud1_ = policy_u1;
    device.path_       = path;

    switch (device.policy_)
    {
    case POLICY_DAILY: init_daily_policy(device); break;
    case POLICY_SIZE: init_size_policy(device); break;
    default: break;
    }
}

void AsyncLog::append(const char *name, int32_t mask, int64_t time,
                      const char *str, size_t len)
{
    assert(name);

    // 在thread_local上用gdb断点，会触发SIGSEGV，直到gcc 9.1才修复
    // 不在这个位置断点程序可正常运行
    // https://stackoverflow.com/questions/33429912/program-compiled-with-fpic-crashes-while-stepping-over-thread-local-variable-in
    thread_local std::string str_name;
    str_name.assign(name);

    std::lock_guard<std::mutex> guard(mutex_);
    auto &it = device_.find(str_name);
    if (it == device_.end())
    {
        ELOG_R("no log device found: %s", name);
        return;
    }
    Device &device = it->second;
    Buffer *buff   = device_reserve(device, time, mask);

    size_t cpy_len = std::min(sizeof(buff->buff_), len);
    memcpy(buff->buff_, str, cpy_len);
    buff->used_ += cpy_len;

    // 如果一个缓冲区放不下，后面接多个缓冲区，时间戳为-1
    if (unlikely(cpy_len < len))
    {
        size_t cur_len = cpy_len;
        do
        {
            buff = device_reserve(device, -1, mask);

            cpy_len = std::min(sizeof(buff->buff_), len - cur_len);
            memcpy(buff->buff_, str + cur_len, cpy_len);

            cur_len += cpy_len;
            buff->used_ += cpy_len;
        } while (cur_len < len);
    }
}

void AsyncLog::trigger_daily_rollover(Device &device, int64_t now)
{
    // 不是按天写入，或者还在同一天now < 0表示这个是拼接到上一个缓冲区的日志，不需要检测
    if (POLICY_DAILY != device.policy_ || now < 0
        || (now >= device.policy_ud1_ && now <= now >= device.policy_ud1_ + 86400))
    {
        return;
    }

    device.close_stream();

    struct tm ntm;
    ::localtime_r(&device.policy_ud1_, &ntm);

    // 修正文件名 xxx_info 转换为 xxx_info20210228
    char new_path[256];
    const std::string &path = device.path_;
    snprintf(new_path, sizeof(new_path), "%s%04d%02d%02d", path.c_str(),
             ntm.tm_year + 1900, ntm.tm_mon + 1, ntm.tm_mday);

    device.policy_ud1_ = day_begin(now);

    std::error_code e;
    bool ok = std::filesystem::exists(path, e);
    if (e)
    {
        ELOG_R("rename daily check file exist file error %s %s", path.c_str(),
               e.message().c_str());
        return;
    }
    if (ok)
    {
        std::filesystem::rename(path, new_path, e);
        if (e)
        {
            ELOG_R("rename daily log file error %s %s", path.c_str(),
                   e.message().c_str());
        }
    }
}

void AsyncLog::trigger_size_rollover(Device &device, int64_t size)
{
    if (POLICY_SIZE != device.policy_) return;

    device.policy_ud2_ += size;
    if (device.policy_ud2_ < device.policy_ud1_) return;

    device.policy_ud2_ = 0;
    device.close_stream();

    std::string old_path;
    std::string new_path;

    // TODO C++20之后直接用std::string::format而不需要用c的snprintf
    char old_buff[1024];
    char new_buff[1024];

    std::error_code e;
    int32_t max_index = 0;
    const std::string &path = device.path_;
    for (int32_t i = max_index + 1; i < 1024; i++)
    {
        snprintf(old_buff, sizeof(old_buff), "%s.%03d", path.c_str(), i);

        old_path.assign(old_buff);
        bool ok = std::filesystem::exists(old_path, e);
        if (e)
        {
            ELOG_R("rename size check file exist file error %s %s",
                   path.c_str(), e.message().c_str());
            return;
        }

        if (!ok) break;

        max_index++;
    }

    for (int32_t i = max_index + 1; i > 1; i--)
    {
        snprintf(new_buff, sizeof(new_buff), "%s.%03d", path.c_str(), i);
        snprintf(old_buff, sizeof(old_buff), "%s.%03d", path.c_str(), i - 1);

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

    // 把xx_info命名为xx_info.1
    bool ok = std::filesystem::exists(path, e);
    if (e)
    {
        ELOG_R("rename size check current file exist file error %s %s",
               path.c_str(), e.message().c_str());
        return;
    }

    if (ok)
    {
        snprintf(new_buff, sizeof(new_buff), "%s.%03d", path.c_str(), 1);
        new_path.assign(new_buff);
        std::filesystem::rename(path, new_path, e);
        if (e)
        {
            ELOG_R("rename size current file error %s %s", path.c_str(),
                   e.message().c_str());
            return;
        }
    }
}

size_t AsyncLog::write_to_one_device(Device *device, const Buffer *buffer,
                                     bool beg, bool end)
{
    trigger_daily_rollover(*device, buffer->time_);
    FILE *stream = device->open_stream();
    if (!stream) return 0;

    size_t bytes = 0;
    if (buffer->time_ >= 0)
    {
        if (!beg)
        {
            bytes++;
            fputc('\n', stream);
        }
        bytes +=
            log_util::write_prefix(stream, buffer->prefix_, type, buffer->time_);
    }

    bytes += fwrite(buffer->buff_, 1, buffer->used_, stream);

    if (end)
    {
        bytes++;
        fputc('\n', stream);
#ifdef __windows__
        // 在win的MYSY2(包括git bash)会缓存输出，直到程序关闭或者缓存区满，因此需要手动刷新
        if (stdout == stream || stderr == stream) fflush(stream);
#endif
    }

    return bytes;
}

void AsyncLog::write_to_device(Device &device, const BufferList &buffers)
{
    size_t size = buffers.size();
    Device *multi_device = device.multi_device_;
    for (size_t i = 0; i < size; i++)
    {
        size_t bytes         = 0;
        bool beg             = 0 == i;
        bool end             = i == size - 1;
        const Buffer *buffer = buffers[i];

        write_to_one_device(&device, buffer, beg, end);
        if (multi_device) write_to_one_device(multi_device, buffer, beg, end);

        switch (buffer->type_)
        {
        case log_util::LT_LOGFILE:
        {
            bytes = write_buffer(stream, "", buffer, beg, end);
            break;
        }
        case log_util::LT_LPRINTF:
        {
            bytes = write_buffer(stream, "LP", buffer, beg, end);
            if (!log_util::is_deamon())
                write_buffer(stdout, "LP", buffer, beg, end);
            break;
        }
        case log_util::LT_LERROR:
        {
            bytes = write_buffer(stream, "LE", buffer, beg, end);
            if (!log_util::is_deamon())
                write_buffer(stderr, "LE", buffer, beg, end);
            break;
        }
        case log_util::LT_CPRINTF:
        {
            bytes = write_buffer(stream, "CP", buffer, beg, end);
            if (!log_util::is_deamon())
                write_buffer(stdout, "CP", buffer, beg, end);
            break;
        }
        case log_util::LT_CERROR:
        {
            bytes = write_buffer(stream, "CE", buffer, beg, end);
            if (!log_util::is_deamon())
                write_buffer(stderr, "CE", buffer, beg, end);
            break;
        }
        case log_util::LT_FILE:
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
    auto now  = timing::try_frame_time();

    std::unique_lock<std::mutex> ul(mutex_);

    while (busy)
    {
        busy = false;
        for (auto iter = device_.begin(); iter != device_.end(); iter++)
        {
            Device &device = iter->second;
            if (!device.buff_.empty())
            {
                device.time_ = now;
                writing_buffers_.swap(device.buff_);

                ul.unlock();
                write_to_device(&policy, writing_buffers_, iter->first.c_str());
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
