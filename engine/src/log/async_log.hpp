#pragma once

#include <chrono>

#include "../thread/thread.hpp"
#include "../pool/object_pool.hpp"
#include "log.hpp"

// 多线程日志
class AsyncLog final : public Thread
{
public:
    /// 日志缓冲区
    struct Buffer
    {
        int64_t _time;   /// 日志UTC时间戳
        size_t _used;    /// 缓冲区已使用大小
        char _buff[512]; /// 日志缓冲区
    };
    using BufferList = std::vector<struct Buffer *>;

    /// 日志设备(如file、stdout)
    struct Device
    {
        LogType _type;                               /// 设备类型
        BufferList _buff;                            /// 待写入的日志
        std::chrono::steady_clock::time_point _time; /// 上次修改时间
    };

public:
    AsyncLog() : Thread("AsyncLog"), _buffer_pool("AsyncLog"){};
    ~AsyncLog(){};

    size_t busy_job(size_t *finished   = nullptr,
                    size_t *unfinished = nullptr) override;

    void append(const char *path, LogType type, int64_t time, const char *ctx,
                size_t len);

private:
    using BufferPool = ObjectPool<Buffer, 256, 256>;

    // 线程相关，重写基类相关函数
    void routine(int32_t ev) override;

    void write_buffer(FILE *stream, const char *prefix,
                      const BufferList &buffers);
    void write_file(const char *path, const char *prefix,
                    const BufferList &buffers);
    void write_device(LogType type, const std::string &path,
                      const BufferList &buffers);

    struct Buffer *device_reserve(struct Device &device, int64_t time)
    {
        struct Buffer *buff = _buffer_pool.construct();

        buff->_time = time;
        device._buff.push_back(buff);

        return buff;
    }

private:
    BufferPool _buffer_pool;
    BufferList _writing_buffers;
    std::unordered_map<std::string, Device> _device;
};
