#pragma once

#include "../../global/global.hpp"

class Buffer;

/* socket input output control */
class IO
{
public:
    /// io类型
    enum IOType
    {
        IOT_NONE = 0, ///< 默认IO类型，无特别处理
        IOT_SSL  = 1, ///< 使用SSL加密

        IOT_MAX ///< IO类型最大值
    };

    /// io状态码
    enum IOStatus
    {
        IOS_OK    = 0,   /// 无错误
        IOS_READ  = 1,   /// 需要重读
        IOS_WRITE = 2,   /// 需要重写
        IOS_BUSY  = 3,   /// 繁忙，比如缓冲区满之类的
        IOS_CLOSE = 4,   /// 连接被关闭
        IOS_ERROR = 0xFF /// 错误
    };

public:
    virtual ~IO();
    explicit IO(uint32_t conn_id, class Buffer *recv, class Buffer *send);

    /**
     * 接收数据
     * @return IOStatus
     */
    virtual IOStatus recv();
    /**
     * 发送数据
     * @return IOStatus
     */
    virtual IOStatus send();
    /**
     * 发起初始化接受的连接
     * @return 需要异步执行的事件
     */
    virtual int32_t init_accept(int32_t fd);
    /**
     * 发起初始化连接
     * @return 需要异步执行的事件
     */
    virtual int32_t init_connect(int32_t fd);
    /**
     * 执行初始化接受的连接
     * @return IOStatus
     */
    virtual IOStatus do_init_accept()
    {
        assert(false);
        return IOS_OK;
    };
    /**
     * 执行初始化连接
     * @return IOStatus
     */
    virtual IOStatus do_init_connect()
    {
        assert(false);
        return IOS_OK;
    };
    /**
     * @brief init_accept、init_connect是否执行完
     */
    virtual bool is_ready() const
    {
        return true;
    }

    /**
     * 初始化完成
     */
    void init_ok() const;

protected:
    int32_t _fd;         /// 用于读写的文件描述符
    uint32_t _conn_id;   /// 所属socket的id
    class Buffer *_recv; /// 接受缓冲区
    class Buffer *_send; /// 发送缓冲区
};
