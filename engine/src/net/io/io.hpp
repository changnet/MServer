#pragma once

#include "../../global/global.hpp"

class Buffer;

/* socket input output control */
class IO
{
public:
    typedef enum
    {
        IOT_NONE = 0, ///< 默认IO类型，无特别处理
        IOT_SSL  = 1, ///< 使用SSL加密

        IOT_MAX ///< IO类型最大值
    } IOT;

public:
    virtual ~IO();
    explicit IO(uint32_t conn_id, class Buffer *recv, class Buffer *send);

    /**
     * 接收数据
     * @param byte 接收的数据长度
     * @return < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t recv(int32_t &byte);
    /**
     * 发送数据
     * @param byte 发送的数据长度
     * 返回: < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t send(int32_t &byte);
    /**
     * 初始化接受的连接
     * @return  < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t init_accept(int32_t fd);
    /**
     * 初始化连接
     * @return  < 0 错误，0 成功，1 需要重读，2 需要重写
     */
    virtual int32_t init_connect(int32_t fd);

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
