#pragma once

#include <memory> // std::shared_ptr
#include <string>
#include <vector>

#include <lua.hpp>
#include <pbc.h>
#include "global/global.hpp"

/*
 * pbc基本是线程安全的，除了加载pb文件时，env是可以多线程共用的
 * 热更时，采用直接丢掉旧env，创建新env的方式。不同线程在热更时，持有的env可能会不一样
 * 当所有线程都热更完后，旧的env将会通过shared_ptr的机制释放。
 * 另外它没有错误回溯机制。为了加这个机制需要一些辅助变量，也需要线程安全
 */

// 解码扁平快速路径的上下文，每层 message 一个栈变量
struct FastCtx;

// 基于pbc实现的protobuf codec
class PbcCodec final
{
public:
    PbcCodec();
    ~PbcCodec();
    static void uninitialize();

    // 创建待加载的pbc env，此时不影响全局g_env_
    void reset();
    // 加载单个pb文件到待加载的env
    int32_t load(lua_State *L);
    // 全部pb加载成功后，把待加载的env发布到全局（唯一写g_env_的地方）
    void load_finish();
    // 更新全局pbc env到当前线程
    void update();

    // 反射：字段是不是 proto3 map，供上层做通用序列化与配置校验
    bool is_map_field(const char *schema, const char *key);
    // Lua 入口：codec:is_map(schema, field) -> boolean
    int32_t is_map(lua_State *L);

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
    /**
     * 从二进制buffer指针解码数据包
     * @param buffer 待解码的char*
     * @param size 待解码的buff大小
     * @return <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode_from_buffer(lua_State *L);
    /**
     * 编码数据包
     * @param ... 待编码的参数
     * @return buffer大小，buffer指针（该指针在下一次调用encode时有效）。出错时，buffer大小返回-1
     */
    int32_t encode_to_buffer(lua_State *L);

    /* 解码扁平快速路径：按 _message* 指针递归，内部只做 lua rawset。
     * 声明为 public 是因为静态回调 fast_decode_cb 需要调用它。 */
    int32_t decode_fast_message(lua_State *L, struct pbc_msg *msg,
                                struct pbc_slice *slice);

    /* 解码单个 MapEntry，在栈顶留下 key、value 两个值（缺省字段按 proto3 默认值）。
     * 声明为 public 是因为静态回调 map_entry_cb 需要调用它。 */
    int32_t decode_map_entry(lua_State *L, struct pbc_msg *entry,
                             struct pbc_slice *slice);

private:
    int32_t raw_decode(lua_State *L, const char *schema, const char *buffer,
                       size_t size);
    int32_t raw_encode(lua_State *L, const char *schema, int32_t index);

    // 清除上一次的报错信息等
    void clear_last();
    // 获取出错信息
    const char *last_error();

    /* 编码：遍历 schema 的字段表，逐个从 Lua table 回取值。
     * msg 是 seal 阶段预编译好的 message 指针，字段表随 env 只读。 */
    int32_t encode_message(lua_State *L, struct pbc_wmessage *wmsg,
                           struct pbc_msg *msg, int32_t index);

    // 单字段分派：子 message / 标量（含 repeated）
    int32_t encode_field(lua_State *L, struct pbc_wmessage *wmsg,
                         const struct pbc_ffield *ff, int32_t index);

    // 子 message 字段：repeated 时逐个元素，否则单个
    int32_t encode_sub(lua_State *L, struct pbc_wmessage *wmsg,
                       struct pbc_wfield *field, const struct pbc_ffield *ff,
                       int32_t index);

    // proto3 map 字段：遍历 kv table，逐项写成 MapEntry 子消息
    int32_t encode_map(lua_State *L, struct pbc_wmessage *wmsg,
                       struct pbc_wfield *field, const struct pbc_ffield *ff,
                       int32_t index);

    // 标量字段：repeated 时按 packed / unpacked 两种形态写入
    int32_t encode_scalar_list(lua_State *L, struct pbc_wmessage *wmsg,
                               struct pbc_wfield *field, int32_t type,
                               int32_t index, const char *key);

    // 单个标量值
    int32_t encode_scalar(lua_State *L, struct pbc_wmessage *wmsg,
                          struct pbc_wfield *field, int32_t type,
                          int32_t index, const char *key);

private:
    struct pbc_wmessage *write_msg_;          // 复用的 wmessage（每次编码 reset，不再 new/delete）
    std::string error_msg_;                   // 错误信息
    std::vector<std::string> trace_back_;     // 出错时，用于跟踪哪个字段有问题
    std::shared_ptr<struct pbc_env> env_;      // 当前线程使用的env
    std::shared_ptr<struct pbc_env> load_env_; // 正在加载的env，load_finish后发布

    // 所有读写都必须走 atomic_load_explicit / atomic_store_explicit
    static inline std::shared_ptr<struct pbc_env> g_env_ = nullptr;

    // C++20起shared_ptr的原子自由函数已废弃，届时改用std::atomic<std::shared_ptr<>>
#ifdef _MSVC_LANG
    static_assert(_MSVC_LANG < 202002L,
        "use std::atomic<std::shared_ptr<pbc_env>> for g_env_ in C++20");
#else
    static_assert(__cplusplus < 202002L,
        "use std::atomic<std::shared_ptr<pbc_env>> for g_env_ in C++20");
#endif
};
