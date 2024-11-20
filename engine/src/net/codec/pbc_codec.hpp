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
    int32_t load(const char *buffer, size_t len);
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

private:
    struct Deleter
    {
        void operator()(struct pbc_env* env) const
        {
            // 最后一个线程删除时，env最终失效。全局也必须重置，免得被新线程赋值
            // 虽然一般情况下最后一个线程失效时就只能是关服
            if (env == _g_env) _g_env = nullptr;

            pbc_delete(env);
        }
    };

    // 获取出错信息
    const char *last_error();
    // 解码一个protobuf的message
    int32_t decode_message(lua_State *L, const char *schema,
                           struct pbc_slice *slice);

    // 解码回调
    void decode_cb(DecodeCtx *ctx, int32_t type, const char *object,
                   union pbc_value *v, int id, const char *key);

    // 解析出来的变量设置到lua
    int32_t push_value(lua_State *L, int type, const char *object,
                       union pbc_value *v);

private:
    struct pbc_wmessage *_write_msg;
    std::string _error_msg;               // 错误信息
    std::vector<std::string> _trace_back; // 出错时，用于跟踪哪个字段有问题
    std::shared_ptr<struct pbc_env> _env; // 当前线程使用的env

    static inline struct pbc_env *_g_env = nullptr;
};
