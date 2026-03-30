#pragma once

#include "thread/thread.hpp"
#include "thread/null_mutex.hpp"
#include "pool/flexible_pool.hpp"
#include "log.hpp"

/// 多线程异步日志
class AsyncLog : public Thread
{
public:
    // 日志策略
    enum
    {
        POLICY_NONE  = 0, /// 未定义
        POLICY_DAILY = 1, /// 每天一个文件
        POLICY_SIZE  = 2, /// 按大小分文件
    };

    // 日志掩码
    enum
    {
        MASK_C_R = 1, // 颜色 red
        MASK_C_G = 2, // 颜色 green
        MASK_C_B = 4, // 颜色 blue
        MASK_C_Y = 8, // 颜色 yellow
        MASK_CL  = MASK_C_R | MASK_C_G | MASK_C_B | MASK_C_Y,
        MASK_S_L = 128,  // 来源 source lua
        MASK_S_C = 256,  // 来源 source c
        MASK_BEG = 512,  // 单次日志开始
        MASK_END = 1024, // 单次日志结束
    };

    /// 日志缓冲区
    class Buffer
    {
    public:
        explicit Buffer(int64_t time, int32_t log_mask, const char *prefix)
        {
            time_     = time;
            log_mask_ = log_mask;
            prefix_   = prefix;
            used_     = 0;
            mask_     = 0;
            ref_      = 0;
        }
        void set_buffer(const char *str, size_t len)
        {
            memcpy(this->buffer(), str, len);
            used_ = static_cast<int32_t>(len);
        }
        char *buffer() noexcept
        {
            // C++ 不支持Flexible Array Member，直接强转.C++ 20可用std::span
            return reinterpret_cast<char *>(this + 1);
        }
        const char *buffer() const noexcept
        {
            return reinterpret_cast<const char *>(this + 1);
        }

        int64_t time_; /// 日志UTC时间戳
        int32_t used_; /// 缓冲区已使用大小
        // size_t size_;    /// 缓冲区容量
        int32_t log_mask_;   /// 日志掩码，颜色等
        int32_t mask_;       /// FlexiblePool使用的掩码
        int32_t ref_;        /// 引用计数
        const char *prefix_; // 前置名称
    };
    using BufferList = std::vector<Buffer *>;

    /// 日志设备(如file、stdout)
    class Device
    {
    public:
        Device();
        ~Device();
        void close_stream(); /// 关闭文件
        FILE *open_stream(); /// 获取文件流

        int32_t policy_; /// 文件切分策略
        int32_t alive_time_; // 文件保持打开的时间，0表示每次写入完就关闭
        int64_t policy_ud1_; /// 用于切分文件的参数
        int64_t policy_ud2_; /// 用于切分文件的参数
        time_t time_;        /// 上次修改时间
        FILE *file_; /// 写入的文件句柄，减少文件打开、关闭
        std::string path_;     /// 当前写入的文件路径
        BufferList buff_;      /// 待写入的缓冲区
        Device *multi_device_; // 关联的多个设备（同时输出到文件和控制台），目前仅允许关联一个
    };

public:
    virtual ~AsyncLog();
    explicit AsyncLog(const std::string &name);

    /**
     * @brief 添加日志设备
     * @param name 日志名字
     * @param path 日志文件路径
     * @param alive 日志文件保持打开的时间
     * @param policy 日志文件策略类型
     * @param policy_u1 日志文件策略参数（比如按大小截断时，文件的大小）
     * @param multi 同时输出的多个设备名字（目前仅支持一个）
     */
    void add_device(const char *name, const char *path, int32_t alive,
                    int32_t policy, int64_t policy_u1, const char *multi);
    /**
     * @brief 删除日志设备
     * @param name 日志名字
     */
    void del_device(const char *name);
    /**
     * @brief 追加日志到设备
     * @param name 日志名字
     * @param name 日志掩码，详见掩码定义，如 MASK_TIME
     * @param time 日志时间戳
     * @param str 日志内容
     * @param len 日志内容长度
     */
    void append(const char *name, int32_t mask, int64_t time, const char *str,
                size_t len);
    void set_log_name(const char *name);
    const char *get_log_name();

private:
    static constexpr size_t BLOCK_SIZE = 256;
    using BufferPool                   = FlexiblePool<BLOCK_SIZE, 512, NullMutex>;

    void remove_device(Device &device);
    void trigger_size_rollover(Device &device, int64_t size);
    void trigger_daily_rollover(Device &device, int64_t now);
    static bool is_daily_rollover(const Device &device, int64_t now)
    {
        return now < device.policy_ud1_ || now > device.policy_ud1_ + 86400;
    }
    bool init_size_policy(Device &device);
    bool init_daily_policy(Device &device);
    // 获取指定时间戳的当天0点时间戳
    static time_t day_begin(time_t now);

    // 线程相关，重写基类相关函数
    void routine_once(int32_t ev) override;
    bool uninitialize() override;

    // 写入控制台颜色
    void write_color(FILE *stream, int32_t mask);
    size_t write_prefix(FILE *stream, int32_t mask, const char *name,
                        int64_t time);
    size_t write_to_one_device(Device *device, const Buffer *buffer);
    void write_to_device(Device &device, const BufferList &buffers);

    Buffer *allocate(Device &device, size_t len, int64_t time, int32_t mask);

private:
    std::mutex mutex_;
    BufferPool buffer_pool_;
    BufferList writing_buffers_;
    std::vector<std::string *> name_; // 各个线程的日志名
    std::unordered_map<std::string, Device> device_;
};
