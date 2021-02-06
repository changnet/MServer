#include <fstream>

#include "async_log.hpp"

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
    thread_local std::string k;
    k.assign(path);

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    struct Device &device = _device[path];
    if (!device._type)
    {
        device._type = type;
    }

    assert(device._type == type);
    assert(!device._buff.empty());

    struct Buffer *buff = device_reserve(device, time);

    size_t cpy_len = std::min(sizeof(buff->_buff), len);
    memcpy(buff->_buff, ctx, cpy_len);
    buff->_used += cpy_len;

    // 如果一个缓冲区放不下，后面接多个缓冲区，时间戳为0
    if (EXPECT_FALSE(cpy_len < len))
    {
        size_t cur_len = cpy_len;
        do
        {
            buff = device_reserve(device, 0);

            cpy_len = std::min(sizeof(buff->_buff), len - cur_len);
            memcpy(buff->_buff, ctx + cur_len, cpy_len);

            cur_len += cpy_len;
            buff->_used += cpy_len;
        } while (cur_len < len);
    }

    unlock();
}

void AsyncLog::write_buffer(FILE *stream, const char *prefix,
                            const BufferList &buffers)
{
    assert(!buffers.empty());

    bool first = true;
    for (auto buffer : buffers)
    {
        if (buffer->_time)
        {
            if (!first) fputc('\n', stream);
            write_prefix(stream, prefix, buffer->_time);
        }

        first = false;
        fwrite(buffer->_buff, 1, buffer->_used, stream);
    }
    fputc('\n', stream);
}

void AsyncLog::write_file(const char *path, const char *prefix,
                          const BufferList &buffers)
{
    FILE *stream = ::fopen(path, "ab+");
    if (!stream)
    {
        ERROR_R("can't open log file(%s):%s\n", path, strerror(errno));

        // TODO:这个异常处理有问题
        // 打开不了文件，可能是权限、路径、磁盘满，这里先全部标识为已写入，丢日志
        return;
    }
    write_buffer(stream, prefix, buffers);
    ::fclose(stream);
}

void AsyncLog::write_device(LogType type, const std::string &path,
                            const BufferList &buffers)
{
    switch (type)
    {
    case LT_FILE:
    {
        write_file(path.c_str(), "", buffers);
        break;
    }
    case LT_LPRINTF:
    {
        write_file(path.c_str(), "LP", buffers);
        if (!is_deamon()) write_buffer(stdout, "LP", buffers);
        break;
    }
    case LT_CPRINTF:
    {
        {
            write_file(path.c_str(), "CP", buffers);
            if (!is_deamon()) write_buffer(stdout, "CP", buffers);
            break;
        }
    }
    default: assert(false); break;
    }
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
    while (true)
    {
        LogType type            = LT_NONE;
        const std::string *path = nullptr;
        _writing_buffers.clear();

        lock();
        for (auto iter = _device.begin(); iter != _device.end(); iter++)
        {
            auto &device = iter->second;
            if (!device._buff.empty())
            {
                path         = &(iter->first);
                device._time = now;
                _writing_buffers.assign(device._buff.begin(), device._buff.end());

                // 这里有点问题，如果日志量很大，可能会饿死其他文件。导致某些文件一下没写入
                break;
            }
            else if (std::chrono::duration_cast<std::chrono::seconds>(
                         now - device._time)
                         .count()
                     > 5 * 60)
            {
                iter = _device.erase(iter);
            }
        }
        unlock();

        if (!path) break;

        write_device(type, *path, _writing_buffers);
    }
}
