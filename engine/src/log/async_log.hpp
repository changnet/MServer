#pragma once

#include "../thread/thread.hpp"
#include "../pool/object_pool.hpp"
#include "log.hpp"

/// 多线程异步日志
class AsyncLog : public Thread
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

    /**
     * 类似linux /var/log下的日志策略
     * runtime%DAILY% 当天日志为runtime，其他的重命名为runtime20210122
     * runtime%SIZE1024% 当前日志为runtime,达到1024大小后，会备份为runtime.1
     */
    class Policy
    {
    public:
        enum PolicyType
        {
            PT_NONE   = 0, /// 未定义
            PT_NORMAL = 1, /// 单个文件
            PT_DAILY  = 2, /// 每天一个文件，文件名包含%DAILY%
            PT_SIZE   = 3, /// 按大小分文件，文件名包含%SIZE1024%
        };

        Policy();
        ~Policy();

        PolicyType get_type() const { return _type; }
        void close_stream();                 /// 关闭文件
        FILE *open_stream(const char *path); /// 获取文件流

        void init_policy(const char *path, int32_t type, int64_t opt_val);
        void trigger_size_rollover(int64_t size);
        void trigger_daily_rollover(int64_t now);
        bool is_daily_rollover(int64_t now)
        {
            return now < _data || now > _data + 86400;
        }

    private:
        bool init_size_policy(int64_t size);
        bool init_daily_policy();

    private:
        FILE *_file; /// 写入的文件句柄，减少文件打开、关闭
        PolicyType _type;      /// 文件切分策略
        int64_t _data;         /// 用于切分文件的参数
        int64_t _data2;        /// 用于切分文件的参数
        std::string _path;     /// 当前写入的文件路径
        std::string _raw_path; /// 原始带格式化的文件路径
    };

    /// 日志设备(如file、stdout)
    class Device
    {
    public:
        friend class AsyncLog;

    private:
        Policy _policy; /// 文件切分策略

        BufferList _buff; /// 待写入的日志
        time_t _time;     /// 上次修改时间
    };

public:
    AsyncLog() : Thread("AsyncLog"), _buffer_pool("AsyncLog"){};
    virtual ~AsyncLog(){};

    size_t busy_job(size_t *finished   = nullptr,
                    size_t *unfinished = nullptr) override;

    void set_policy(const char *path, int32_t type, int64_t opt_val);
    void append(const char *path, LogType type, int64_t time, const char *ctx,
                size_t len);

private:
    using BufferPool = ObjectPool<Buffer, 256, 256>;

    // 线程相关，重写基类相关函数
    void routine(int32_t ev) override;
    bool uninitialize();

    size_t write_buffer(FILE *stream, const char *prefix, const Buffer *buffer,
                        bool beg, bool end);
    void write_device(Policy *policy, const BufferList &buffers,
                      const char *path);

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
