#pragma once

#include <memory> // std::shared_ptr
#include <lua.hpp>
#include <pbc.h>
#include "global/global.hpp"

/*
 * pbc基本是线程安全的，除了加载pb文件时，env是可以多线程共用的
 * 热更时，采用直接丢掉旧env，创建新env的方式。不同线程在热更时，持有的env可能会不一样
 * 当所有线程都热更完后，旧的env将会通过shared_ptr的机制释放。
 * 另外它没有错误回溯机制。为了加这个机制需要一些辅助变量，也需要线程安全
 */

struct DecodeCtx;

// 基于pbc实现的protobuf codec
class PbcCodec final
{
public:
    PbcCodec();
    ~PbcCodec();

    // 重置全局pbc env
    void reset();
    // 加载单个pb文件
    int32_t load(lua_State *L);
    // 更新全局pbc env到当前线程
    void update();

    /* 解码数据包
     * @param  schema protobuf的message名
     * @param buffer 二进制string
     * @return 包含解析出来数据的lua table
     */
    int32_t decode(lua_State *L);
    /* 编码数据包
     * @param schema protobuf的message名
     * @param  pkt 待编码的数据，lua table格式
     * @return 二进制string
     */
    int32_t encode(lua_State *L);

    // 解码回调
    void decode_cb(DecodeCtx *ctx, int32_t type, const char *object,
                   union pbc_value *v, int id, const char *key);

private:
    // 清除上一次的报错信息等
    void clear_last();
    // 获取出错信息
    const char *last_error();
    // 解码一个protobuf的message
    int32_t decode_message(lua_State *L, const char *schema,
                           struct pbc_slice *slice);

    // 解析出来的变量设置到lua
    int32_t decode_field(lua_State *L, int type, const char *object,
                       union pbc_value *v);

    int32_t encode_message(lua_State *L, struct pbc_wmessage *wmsg,
                       const char *schema, int32_t index);

    int32_t encode_field_list(lua_State *L, struct pbc_wmessage *wmsg,
                                   int32_t type, int32_t index, const char *key,
                                   const char *schema);

    int32_t encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                                       int32_t type, int32_t index,
                                       const char *key, const char *schema);

private:
    struct pbc_wmessage *_write_msg;
    std::string _error_msg;               // 错误信息
    std::vector<std::string> _trace_back; // 出错时，用于跟踪哪个字段有问题
    std::shared_ptr<struct pbc_env> _env; // 当前线程使用的env

    static inline std::shared_ptr<struct pbc_env> _g_env = nullptr;
};
