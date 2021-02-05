#include <fstream>

#include "async_log.hpp"
#include "../system/static_global.hpp"

const char *format_time(int64_t time)
{
    thread_local char time_str[32];
    thread_local int64_t time_cache = 0;

    struct tm ntm;
    ::localtime_r(&time, &ntm);

    //    snprintf(time_str, "[%s%s%02d-%02d %02d:%02d:%02d]", app_name, prefix,
    //                       (ntm.tm_mon + 1), ntm.tm_mday, ntm.tm_hour,
    //                       ntm.tm_min, ntm.tm_sec)

    return time_str;
}

size_t AsyncLog::busy_job(size_t *finished, size_t *unfinished)
{
    lock();
    size_t unfinished_sz = _log.pending_size();

    if (is_busy()) unfinished_sz += 1;
    unlock();

    if (finished) *finished = 0;
    if (unfinished) *unfinished = unfinished_sz;

    return unfinished_sz;
}

void AsyncLog::write(const char *path, const char *ctx, size_t len,
                     LogType out_type)
{
    assert(path);
    thread_local std::string k;
    k.assign(path);

    int64_t now = StaticGlobal::ev()->now();

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    struct Device &device = _device[path];
    if (!device._type)
    {
        device._type = out_type;
        device_reserve(device, now);
    }

    assert(device._type == out_type);
    assert(!device._buff.empty());

    struct Buffer *buff = device_reserve(device, now);

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

void AsyncLog::raw_write(const char *path, LogType out, const char *fmt,
                         va_list args)
{
    static char ctx_buff[LOG_MAX_LENGTH];

    int32_t len = vsnprintf(ctx_buff, LOG_MAX_LENGTH, fmt, args);

    /* snprintf
     * 错误返回-1
     * 缓冲区不足，返回需要的缓冲区长度(即返回值>=缓冲区长度表示被截断)
     * 正确返回写入的字符串长度
     * 上面的返回值都不包含0结尾的长度
     */

    if (len < 0)
    {
        ERROR("raw_write snprintf fail");
        return;
    }

    write(path, ctx_buff, len > LOG_MAX_LENGTH ? LOG_MAX_LENGTH : len, out);
}
void AsyncLog::raw_write(const char *path, LogType out, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    raw_write(path, out, fmt, args);
    va_end(args);
}

void AsyncLog::write_device(LogType type, const std::string &path,
                            const BufferList &buffers)
{
    switch (type)
    {
    case LT_FILE: break;
    case LT_LPRINTF: break;
    case LT_CPRINTF: break;
    }

    std::ofstream file;
    file.open(path, std::ios::out | std::ios::app | std::ios::binary);

    if (!file.good()) return;

    for (auto buffer : buffers)
    {
        file.write(buffer->_buff, buffer->_used);
    }

    file.close();
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
