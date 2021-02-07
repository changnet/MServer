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
    thread_local std::string str_path;
    str_path.assign(path);

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    Device &device = _device[str_path];
    if (!device._type)
    {
        device._type = type;
    }

    assert(device._type == type);

    Buffer *buff = device_reserve(device, time);

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

void AsyncLog::write_device(LogType type, const char *path,
                            const BufferList &buffers)
{
    switch (type)
    {
    case LT_FILE:
    {
        write_file(path, "", buffers);
        break;
    }
    case LT_LPRINTF:
    {
        write_file(path, "LP", buffers);
        if (!is_deamon()) write_buffer(stdout, "LP", buffers);
        break;
    }
    case LT_LERROR:
    {
        write_file(path, "LE", buffers);
        if (!is_deamon()) write_buffer(stdout, "LE", buffers);
        break;
    }
    case LT_CPRINTF:
    {
        write_file(path, "CP", buffers);
        if (!is_deamon()) write_buffer(stdout, "CP", buffers);
        break;
    }
    case LT_CERROR:
    {
        write_file(path, "CE", buffers);
        if (!is_deamon()) write_buffer(stdout, "CE", buffers);
        break;
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

    lock();
    while (true)
    {
        LogType type     = LT_NONE;
        const char *path = nullptr;

        // 这里有点问题，如果日志量很大，可能会饿死其他文件。导致某些文件一下没写入
        for (auto iter = _device.begin(); iter != _device.end(); iter++)
        {
            auto &device = iter->second;
            if (!device._buff.empty())
            {
                type = device._type;
                path = iter->first.c_str();
                _writing_buffers.assign(device._buff.begin(), device._buff.end());

                device._time = now;
                device._buff.clear();
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

        if (!path) break;

        unlock();
        write_device(type, path, _writing_buffers);
        lock();

        // 回收缓冲区
        for (auto buffer : _writing_buffers) _buffer_pool.destroy(buffer);
        _writing_buffers.clear();
    }
    unlock();
}
