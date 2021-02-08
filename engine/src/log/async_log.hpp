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
    class Buffer
    {
    public:
        friend class AsyncLog;
        explicit Buffer(int64_t time, LogType type)
        {
            _time = time;
            _type = type;
            _used = 0;
        }

    private:
        int64_t _time;   /// 日志UTC时间戳
        size_t _used;    /// 缓冲区已使用大小
        LogType _type;   /// 日志类型
        char _buff[512]; /// 日志缓冲区
    };
    using BufferList = std::vector<Buffer *>;

    /// 日志设备(如file、stdout)
    class Device
    {
    public:
        friend class AsyncLog;

    private:
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

    void write_buffer(FILE *stream, const char *prefix, const Buffer *buffer,
                      bool beg, bool end);
    void write_device(const char *path, const BufferList &buffers);

    Buffer *device_reserve(Device &device, int64_t time, LogType type)
    {
        Buffer *buff = _buffer_pool.construct(time, type);

        device._buff.push_back(buff);

        return buff;
    }

private:
    BufferPool _buffer_pool;
    BufferList _writing_buffers;
    std::unordered_map<std::string, Device> _device;
};
