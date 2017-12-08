#include "ssl_io.h"

ssl_io::~ssl_io()
{
}

ssl_io::ssl_io( class buffer *recv,class buffer *send )
    : io( recv,send )
{

}

/* 接收数据
 * 返回：< 0 错误(包括对方主动断开)，0 需要重试，> 0 成功读取的字节数
 */
int32 ssl_io::recv()
{
    return 0;
}

/* 发送数据
 * 返回：< 0 错误(包括对方主动断开)，0 成功，> 0 仍需要发送的字节数
 */
int32 ssl_io::send()
{
    return 0;
}

/* 准备接受状态
 */
int32 ssl_io::init_accept()
{
    return 0;
}

/* 准备连接状态
 */
int32 ssl_io::init_connect()
{
    return 0;
}
