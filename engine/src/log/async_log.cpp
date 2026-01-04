#include "global/global.hpp"
#include "async_log.hpp"
#include "ev/time.hpp"

#include <filesystem>

// 多线程中，可能某个线程写了日志就退出了，数据存用thread_local会被立马释放掉
// 日志线程执行写入时，就取不到数据了，只能存指针
// thread_local char log_name[128] = {0};
thread_local const char *thread_name = nullptr;

////////////////////////////////////////////////////////////////////////////////
AsyncLog::Device::Device()
{
    alive_time_ = 0;
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
        if (stdout != file_ && stderr != file_) ::fclose(file_);
        file_ = nullptr;
    }
}

FILE *AsyncLog::Device::open_stream()
{
    if (!file_)
    {
        if (path_.empty())
        {
            ELOG_R("can't open log file, path is null");
            return nullptr;
        }
        if (path_ == "stdout")
        {
            file_ = stdout;
        }
        else if (path_ == "stderr")
        {
            file_ = stderr;
        }
        else
        {
            file_ = ::fopen(path_.c_str(), "ab+");
            if (!file_)
            {
                ELOG_R("can't open log file(%s):%s\n", path_.c_str(),
                       strerror(errno));
                return nullptr;
            }
        }
    }

    return file_;
}
////////////////////////////////////////////////////////////////////////////////
AsyncLog::AsyncLog(const std::string &name)
    : Thread(name), buffer_pool_("AsyncLog")
{
}

AsyncLog::~AsyncLog()
{
    for (auto &v : name_)
    {
        delete v;
    }
}

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
        // 转换为 system_clock 时间点
        auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now()
                + std::chrono::system_clock::now());

        // 转换为 time_t
        old_time = std::chrono::system_clock::to_time_t(sctp);
#endif
    }
    else
    {
        old_time = timing::try_frame_time();
    }

    device.policy_ud1_ = day_begin(old_time);
    return true;
}

void AsyncLog::remove_device(Device &device)
{
    // 如果有其他device引用当前的，则移除

    Device *p = &device;
    for (auto iter = device_.begin(); iter != device_.end(); iter++)
    {
        if (iter->second.multi_device_ == p)
            iter->second.multi_device_ = nullptr;
    }
}

void AsyncLog::add_device(const char *name, const char *path,
                                       int32_t alive, int32_t policy,
                                       int64_t policy_u1, const char *multi)
{
    // TODO 目前只能增，不能安全删改。因为日志线程可能正在写入，这时修改device会出问题
    // 如果后续有删改需求，可能要新增一套机制
    std::lock_guard<std::mutex> guard(mutex_);

    Device *multi_device = nullptr;
    if (multi)
    {
        auto found = device_.find(multi);
        if (found == device_.end())
        {
            ELOG_R("no multi device found: %s", multi);
            return;
        }
        multi_device = &found->second;
    }

    auto result = device_.emplace(name, Device{});
    if (!result.second)
    {
        ELOG_R("log device %s already exist", name);
        return;
    }
    if (0 == alive) alive = 1; // alive为0用于表示删除设备

    Device &device = result.first->second;
    // 路径可能有变化，先关闭文件(另一个线程可能正在写入，不能关闭)
    // device.close_stream();
    device.alive_time_ = alive;
    device.policy_     = policy;
    device.policy_ud1_ = policy_u1;
    device.path_       = path;
    device.multi_device_ = multi_device;

    // 这里面可能会触发磁盘io操作，会比较慢
    // 曾经想过由log线程改动日志时才初始化日志，但那样的话跨天的日志会出现不写日志就不改文件名的情况
    switch (device.policy_)
    {
    case POLICY_DAILY: init_daily_policy(device); break;
    case POLICY_SIZE: init_size_policy(device); break;
    default: break;
    }
}

void AsyncLog::del_device(const char *name)
{
    std::lock_guard<std::mutex> guard(mutex_);

    auto found = device_.find(name);
    if (found == device_.end())
    {
        ELOG_R("no multi device found: %s", name);
        return;
    }

    // 这里不能删除，因为日志线程写入文件时没加锁
    // 修改这个alive_time_也不安全，但应该不会出大问题
    found->second.alive_time_ = 0;
}

void AsyncLog::append(const char *name, int32_t mask, int64_t time,
                      const char *str, size_t len)
{
    assert(name);

    std::lock_guard<std::mutex> guard(mutex_);
    // 高版本的STL有透明对比，std::string可直接和const char *对比而无须先构建std::string
    auto it = device_.find(name);
    if (it == device_.end())
    {
        ELOG_R("no log device found: %s", name);
        if (len < 1024) ELOG_R("%s", str);
        return;
    }
    Device &device = it->second;
    Buffer *buff   = device_reserve(device, time, mask);

    size_t cpy_len = std::min(sizeof(buff->buff_), len);
    memcpy(buff->buff_, str, cpy_len);
    buff->used_ += cpy_len;
    buff->mask_ |= MASK_BEG;

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
    buff->mask_ |= MASK_END;
}

void AsyncLog::trigger_daily_rollover(Device &device, int64_t now)
{
    // 不是按天写入，或者还在同一天now < 0表示这个是拼接到上一个缓冲区的日志，不需要检测
    if (POLICY_DAILY != device.policy_ || now < 0
        || (now >= device.policy_ud1_ && now <= device.policy_ud1_ + 86400))
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

void AsyncLog::write_color(FILE *stream, int32_t mask)
{
    const char *str = nullptr;
    if (0 == mask)
    {
        str = "\033[0m"; // 重置
    }
    else if (mask & MASK_C_R)
    {
        str = "\033[31m";
    }
    else if (mask & MASK_C_G)
    {
        str = "\033[32m";
    }
    else if (mask & MASK_C_B)
    {
        str = "\033[34m";
    }
    else if (mask & MASK_C_Y)
    {
        str = "\033[33m";
    }
    if (str) fwrite(str, 1, strlen(str), stream);
}

size_t AsyncLog::write_prefix(FILE *stream, int32_t mask, const char *name,
                    int64_t time)
{
    // 如果无mask并且time <= 0，则不添加任何前缀，通常用于写入文件
    if (0 == mask && time <= 0) return 0;

    // [L12-17 16:15:53  gateway1] 线程名为gateway1，L表示来源为lua

    size_t bytes = 2; // []
    fwrite("[", 1, 1, stream);
    if (mask & MASK_S_L)
    {
        bytes++;
        fwrite("L", 1, 1, stream);
    }
    else if (mask & MASK_S_C)
    {
        bytes++;
        fwrite("C", 1, 1, stream);
    }

    thread_local char time_str[128] = {0};
    thread_local int64_t time_cache = 0;
    thread_local int32_t time_len   = 0;

    // 时间戳精度是1秒，同一秒写入的日志，直接取缓存。从日志结果看，这个缓存命中很高
    if (time != time_cache)
    {
        struct tm ntm;
        ::localtime_r(&time, &ntm);
        time_len = snprintf(time_str, sizeof(time_str),
                            "%02d-%02d %02d:%02d:%02d", ntm.tm_mon + 1,
                            ntm.tm_mday, ntm.tm_hour, ntm.tm_min, ntm.tm_sec);

        time_cache = time;
    }

    bytes += fwrite(time_str, 1, time_len, stream);
    if (name) bytes += fwrite(name, 1, strlen(name), stream);
    fwrite("]", 1, 1, stream);

    return bytes;
}

size_t AsyncLog::write_to_one_device(Device *device, const Buffer *buffer)
{
    FILE *stream = device->open_stream();
    if (!stream) return 0;

    size_t bytes = 0;
    int32_t mask = buffer->mask_;
    if (mask & MASK_BEG)
    {
        // 只在开始和结束的时候触发日志拆分，避免同一行日志被拆分到不同文件
        trigger_daily_rollover(*device, buffer->time_);
        stream = device->open_stream(); // 可能会触发日志文件关闭，重新获取
        if (!stream) return 0;

        // 写入颜色
        if (mask & MASK_CL && (stream == stdout || stream == stderr))
        {
            write_color(stream, mask);
        }
        bytes += write_prefix(stream, mask, buffer->prefix_, buffer->time_);
    }

    bytes += fwrite(buffer->buff_, 1, buffer->used_, stream);

    if (mask & MASK_END)
    {
        if (mask & MASK_CL && (stream == stdout || stream == stderr))
        {
            write_color(stream, 0);
        }
        bytes++;
        fputc('\n', stream);

        trigger_size_rollover(*device, bytes);
#ifdef __windows__
        // 在win的MYSY2(包括git bash)会缓存输出，直到程序关闭或者缓存区满，因此需要手动刷新
        if (stdout == stream || stderr == stream) fflush(stream);
#endif
    }

    return bytes;
}

void AsyncLog::write_to_device(Device &device, const BufferList &buffers)
{
    Device *d = &device;
    while (d)
    {
        for (const auto &buffer : buffers)
        {
            write_to_one_device(d, buffer);
        }

        // 存在 A-B-C 这样连续输出到多个device的情况，死循环则由上层保证
        d = d->multi_device_;
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

    auto now  = timing::try_frame_time();

    std::unique_lock<std::mutex> ul(mutex_);

    auto iter = device_.begin();
    while (iter != device_.end())
    {
        Device &device = iter->second;
        if (device.buff_.empty())
        {
            int32_t alive_time = device.alive_time_;

            // 写入完成即关闭的设备
            if (0 == alive_time)
            {
                device.close_stream();
                remove_device(device);
                iter = device_.erase(iter);
                continue;
            }
            // 关闭长时间不使用的设备
            int64_t sec = now - device.time_;
            // 定时关闭文件，当缓存区没满时，不关闭是不会输出到文件的(用flush ??)
            // stdout和stderr等不关闭的会设置alive_time为-1
            if (alive_time > 0 && sec > alive_time) device.close_stream();

            // 当没有日志写入时，10秒检测一次日期切换
            if (POLICY_DAILY == device.policy_ && sec > 10)
            {
                device.time_ = now;
                if (now < device.policy_ud1_ || now > device.policy_ud1_ + 86400)
                {
                    ul.unlock();
                    trigger_daily_rollover(device, now);
                    ul.lock();
                }
            }
        }
        else
        {
            device.time_ = now;
            writing_buffers_.swap(device.buff_);

            ul.unlock();
            write_to_device(device, writing_buffers_);
            ul.lock();

            // 回收缓冲区
            for (auto buffer : writing_buffers_)
            {
                buffer_pool_.destroy(buffer);
            }
            writing_buffers_.clear();
        }
        iter++;
    }
}

bool AsyncLog::uninitialize()
{
    routine_once(0);

    device_.clear(); // 保证文件句柄被销毁并写入文件

    return true;
}


void AsyncLog::set_thread_name(const char *name)
{
    // 某个线程写异步日志，退出后释放了thread_local
    // 因此把数据存全局，保证日志线程写日志时还能获取到名字

    // null表示清空
    if (!name)
    {
        thread_name = nullptr;
        return;
    }

    std::scoped_lock<std::mutex> sl(mutex_);
    std::string n(name);
    for (auto &s : name_)
    {
        if (n == *s)
        {
            thread_name = s->c_str();
            return;
        }
    }

    name_.push_back(new std::string(name));
    thread_name = name_.back()->c_str();
}

const char *AsyncLog::get_thread_name()
{
    return thread_name;
}
