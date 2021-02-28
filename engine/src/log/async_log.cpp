#include <fstream>

#include "async_log.hpp"

////////////////////////////////////////////////////////////////////////////////
AsyncLog::Policy::Policy()
{
    _data = 0;
    _data2 = 0;
    _file = nullptr;
    _type = PT_NONE;
}
AsyncLog::Policy::~Policy()
{
    close_stream();
}

void AsyncLog::Policy::close_stream()
{
    if (_file) ::fclose(_file);
}

void AsyncLog::Policy::init_policy(const char *path)
{
    std::string str(path);

    /// 按天切分
    size_t pos = str.find("%DAILY%");
    if (pos != std::string::npos)
    {
        _data = -86400;
        _type = PT_DAILY;
        return;
    }

    /// 按文件大小切分 runtime%SIZE1024%表示文件大小为1024
    pos = str.find("%SIZE");
    if (pos != std::string::npos)
    {
        size_t spos = str.find("%", pos + 5);
        if (spos != std::string::npos)
        {
            _data = std::stoll(str.substr(pos + 5, spos));
            _data2 = _data + 1; // 保证下次触发文件切换
            _type = PT_SIZE;
            return;
        }
    }

    /// 不需要切分，传的即文件名
    _type = PT_NORMAL;
    _path.assign(path);
}

FILE *AsyncLog::Policy::get_stream(const char *path, int64_t param)
{
    if (PT_NONE == _type) init_policy(path);

    switch(_type)
    {
    case PT_DAILY:
    {
        if (param < _data || param > _data + 86400)
        {
            close_stream();

            struct tm ntm;
            ::localtime_r(&param, &ntm);

            ntm.tm_hour = 0;
            ntm.tm_min  = 0;
            ntm.tm_sec  = 0;
            _data = std::mktime(&ntm);

            // 修正文件名 runtime&DAILY% 转换为 runtime2021-02-28
            char date[64];
            int len = snprintf(date, sizeof(date), "%04d-%02d-%02d",
                ntm.tm_year, ntm.tm_mon + 1, ntm.tm_mday);

            _path.assign(path);
            _path.replace(_path.begin(), _path.end(), date, len);
        }
        break;
    }
    case PT_SIZE:
    {
        if (param > _data)
        {
            close_stream();

            // runtime%SIZE1024% 将会依次替换成文件runtime.1、runtime.2、runtime.3
            // TODO 这个没想好怎么做，原本这个是参照linux /var/log里的日志机制
            // 但是看了下 runtime.1、runtime.2、runtime.3 是1最新，也就是说，产生一个新
            // 文件需要修改所有旧文件。。。这个机制不太适合吧

            _path.assign(path);
            size_t pos = _path.find("%SIZE");
            size_t spos = _path.find("%", pos + 5);
            _path.replace(pos, spos - pos, ".1");
        }
        break;
    }
    case PT_NORMAL: break;
    default : assert(false); return nullptr;
    }

    if (!_file)
    {
        _file = ::fopen(_path.c_str(), "ab+");
        if (!_file)
        {
            ERROR_R("can't open log file(%s):%s\n", _path.c_str(), strerror(errno));
            return nullptr;
        }
    }

    return _file;
}
////////////////////////////////////////////////////////////////////////////////

size_t AsyncLog::busy_job(size_t *finished, size_t *unfinished)
{
    lock();
    size_t unfinished_sz = 0;
    for (auto iter = _device.begin(); iter != _device.end(); iter++)
    {
        unfinished_sz += iter->second._buff.size();
    }

    if (is_busy()) unfinished_sz += 1;
    unlock();

    if (finished) *finished = 0;
    if (unfinished) *unfinished = unfinished_sz;

    return unfinished_sz;
}

void AsyncLog::append(const char *path, LogType type, int64_t time,
                      const char *ctx, size_t len)
{
    assert(path);
    thread_local std::string str_path;
    str_path.assign(path);

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    Device &device = _device[str_path];
    Buffer *buff   = device_reserve(device, time, type);

    size_t cpy_len = std::min(sizeof(buff->_buff), len);
    memcpy(buff->_buff, ctx, cpy_len);
    buff->_used += cpy_len;

    // 如果一个缓冲区放不下，后面接多个缓冲区，时间戳为0
    if (EXPECT_FALSE(cpy_len < len))
    {
        size_t cur_len = cpy_len;
        do
        {
            buff = device_reserve(device, 0, type);

            cpy_len = std::min(sizeof(buff->_buff), len - cur_len);
            memcpy(buff->_buff, ctx + cur_len, cpy_len);

            cur_len += cpy_len;
            buff->_used += cpy_len;
        } while (cur_len < len);
    }

    unlock();
}

void AsyncLog::write_buffer(FILE *stream, const char *prefix,
                            const Buffer *buffer, bool beg, bool end)
{

    if (buffer->_time)
    {
        if (!beg) fputc('\n', stream);
        write_prefix(stream, prefix, buffer->_time);
    }

    fwrite(buffer->_buff, 1, buffer->_used, stream);

    if (end) fputc('\n', stream);
}

void AsyncLog::write_device(const char *path, Policy *policy, const BufferList &buffers)
{
    size_t size = buffers.size();
    for (size_t i = 0; i < size; i++)
    {
        bool beg             = 0 == i;
        bool end             = i == size - 1;
        const Buffer *buffer = buffers[i];
        FILE *stream = policy->get_stream(path, buffer->_time);
        switch (buffer->_type)
        {
        case LT_FILE:
        {
            write_buffer(stream, "", buffer, beg, end);
            break;
        }
        case LT_LPRINTF:
        {
            write_buffer(stream, "LP", buffer, beg, end);
            if (!is_deamon()) write_buffer(stdout, "LP", buffer, beg, end);
            break;
        }
        case LT_LERROR:
        {
            write_buffer(stream, "LE", buffer, beg, end);
            if (!is_deamon()) write_buffer(stderr, "LE", buffer, beg, end);
            break;
        }
        case LT_CPRINTF:
        {
            write_buffer(stream, "CP", buffer, beg, end);
            if (!is_deamon()) write_buffer(stdout, "CP", buffer, beg, end);
            break;
        }
        case LT_CERROR:
        {
            write_buffer(stream, "CE", buffer, beg, end);
            if (!is_deamon()) write_buffer(stderr, "CE", buffer, beg, end);
            break;
        }
        default: assert(false); break;
        }
    }

    // policy->close_stream();
}

// 线程主循环
void AsyncLog::routine(int32_t ev)
{
    UNUSED(ev);

    // https://en.cppreference.com/w/cpp/container/unordered_map/erase
    // https://stackoverflow.com/questions/38468844/erasing-elements-from-unordered-map-in-a-loop
    // The order of the elements that are not erased is preserved. (This makes
    // it possible to erase individual elements while iterating through the
    // container.)
    // C++14以后，允许在循环中删除
    static_assert(__cplusplus > 201402L);

    auto now = std::chrono::steady_clock::now();

    lock();
    while (true)
    {
        Policy *policy = nullptr;
        const char *path = nullptr;

        // 这里有点问题，如果日志量很大，可能会饿死其他文件。导致某些文件一下没写入
        for (auto iter = _device.begin(); iter != _device.end(); iter++)
        {
            auto &device = iter->second;
            if (!device._buff.empty())
            {
                path = iter->first.c_str();
                policy = &device._policy;
                _writing_buffers.assign(device._buff.begin(), device._buff.end());

                device._time = now;
                device._buff.clear();
                break;
            }

            int64_t sec = std::chrono::duration_cast<std::chrono::seconds>(
                         now - device._time).count();
            if (sec > 5 * 60)
            {
                iter = _device.erase(iter);
            }
            else if (sec > 10)
            {
                // 每次都关闭文件，性能太差
                // 一直不关闭文件，有时候需要在外部修改文件又没办法，暂每10秒关闭一次
                device._policy.close_stream();
            }
        }

        if (!path) break;

        unlock();
        write_device(path, policy, _writing_buffers);
        lock();

        // 回收缓冲区
        for (auto buffer : _writing_buffers) _buffer_pool.destroy(buffer);
        _writing_buffers.clear();
    }
    unlock();
}
