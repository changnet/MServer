#include <lua.hpp>

#include <filesystem> // create_directories

#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include "lutil.hpp"
#include "ltools.hpp"
#include "net/socket.hpp" // get_addr_info
#include "net/net_compat.hpp"

#ifdef __windows__
    #include <rpc.h>
#else
    #include <uuid/uuid.h>
#endif

/**
 * 以阻塞方式获取域名对应的ip
 * @param name 域名，如：www.openssl.com
 * @param v4 只获取ipv4 ip
 * @return ip1 ip2 ...
 */
static int32_t get_addr_info(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    if (!name) return luaL_error(L, "gethost illegal argument");

    bool v4 = lua_toboolean(L, 2);

    std::vector<std::string> addrs;
    if (0 != Socket::get_addr_info(addrs, name, v4))
    {
        return luaL_error(L, "get_addr_info fail");
    }

    for (auto &x : addrs)
    {
        lua_pushstring(L, x.c_str());
    }
    return static_cast<int32_t>(addrs.size());
}

/**
 * 计算字符串的md5值
 * @param upper 可选参数，返回值是否转换为大写
 * @param ... 需要计算的字符串，可以值多个
 * @return md5值
 */
static int32_t md5(lua_State *L)
{
    size_t len;
    MD5_CTX md;
    const char *ptr;
    unsigned char dgst[MD5_DIGEST_LENGTH];

    int32_t index   = 1;
    const char *fmt = "%02x"; // default format to lower
    if (!lua_isstring(L, 1))
    {
        index = 2;

        if (lua_toboolean(L, 1)) fmt = "%02X";
    }

    MD5_Init(&md);
    for (int32_t i = index; i <= lua_gettop(L); ++i)
    {
        ptr = lua_tolstring(L, i, &len);
        if (!ptr)
        {
            // not a string and can not convert to string
            return luaL_error(L, "argument #%d expect string,got %s", i,
                              lua_typename(L, lua_type(L, i)));
        }

        MD5_Update(&md, ptr, len);
    }

    MD5_Final(dgst, &md);
    char buf[MD5_DIGEST_LENGTH * 2 + 1];
    for (int32_t i = 0; i < 16; ++i)
    {
        //--%02x即16进制输出，占2个字节
        snprintf(buf + i * 2, 3, fmt, dgst[i]);
    }
    lua_pushlstring(L, buf, MD5_DIGEST_LENGTH * 2);

    return 1;
}

/**
 * 产生一个新的uuid(Universally Unique Identifier)
 * @param upper 可选参数，返回值是否转换为大写
 * @return uuid
 */
static int32_t uuid(lua_State *L)
{
    bool upper = lua_toboolean(L, 1);
#ifdef __windows__
    UUID uuid;
    UuidCreate(&uuid);
    char *b;
    UuidToStringA(&uuid, (RPC_CSTR *)&b);
    if (upper)
    {
        int32_t i = 0;
        while (b[i])
        {
            b[i] = toupper(b[i]);
            i++;
        }
    }
    lua_pushstring(L, b);
    RpcStringFreeA((RPC_CSTR *)&b);
#else
    uuid_t u;
    char b[40] = {0};

    uuid_generate(u);
    upper ? uuid_unparse_upper(u, b) : uuid_unparse_lower(u, b);
    lua_pushstring(L, b);
#endif

    return 1;
}

/**
 * 利用非标准base64编码uuid，产生一个22个字符串
 * @return 22个字符的短uuid
 */
static int32_t uuid_short(lua_State *L)
{
#ifdef __windows__
    luaL_error(L, "uuid_short unsupported in this platform");
#else
    static const char *digest =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_";
    uuid_t u;
    uuid_generate(u);

    /* typedef unsigned char uuid_t[16]
     * a8d99a61-0a06-4c20-b770-ac911432019e
     * 总长度：16*8=128bit
     * 一个16进制字符(0~F)为0000~1111，可以表示4bit。128/4 = 32个字符(减去4个"-")
     * 一个64进制字符(0~_)为00 0000~11 1111,可以表示6bit。128/6 = 22个字符
     */
    char val;
    char fragment;
    char out[23]     = {0};
    const char *uuid = reinterpret_cast<const char *>(u);

    /* 3*8 = 4*6 = 24,128/24 = 5
     * 64bit表示6bit，一个char为8bit，每次编码3个char，产生4个符。5次共编码120bit
     */
    char *cur_char = out;
    for (int32_t index = 0; index < 5; index++)
    {
        /* 1111 1100取高6bit，再右移2bit得到前6bit */
        /* 0000 0011取低2bit，左移4bit存到val的2~3bit */
        fragment    = *uuid++;
        val         = (fragment & 0x0fc) >> 2;
        *cur_char++ = digest[(int)val];
        val         = (fragment & 0x003) << 4;

        /* 1111 0000取高4bit，右移4bit得到低4bit，加上上一步val的高2bit，得6bit */
        /* 0000 1111取低4bit，左移2bit暂存到val的2~5bit */
        fragment = *uuid++;
        val |= char((fragment & 0x0f0) >> 4);
        *cur_char++ = digest[(int)val];
        val         = (fragment & 0x00f) << 2;

        /* 1100 0000先取高2bit，右移6bit，加上上一步val的高4bit,得6bit */
        fragment = *uuid++;
        val |= char((fragment & 0x0c0) >> 6);
        *cur_char++ = digest[(int)val];
        val         = (fragment & 0x03f) << 0;

        /* 0011 1111 完整的低6bit */
        *cur_char++ = digest[(int)val];
    }

    // 余下的8bit，特殊处理
    fragment    = *uuid;
    val         = (fragment & 0x0fc) >> 2;
    *cur_char++ = digest[(int)val];

    /* 最后一个只有2bit */
    val       = (fragment & 0x003);
    *cur_char = digest[(int)val];

    lua_pushstring(L, out);
#endif
    return 1;
}

inline char uuid_short_char(lua_State *L, const char c)
{
    static const char digest[] = {
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, //  0 ~ 15
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, // 16 ~ 31
        -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, -1, // 32 ~ 47
        52, 53, 54, 55, 56, 57, 58, 59,
        60, 61, -1, -1, -1, -1, -1, -1, // 48 ~ 63
        -1, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, // 64 ~ 79
        15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, 63, // 80 ~ 95
        -1, 26, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40, // 96 ~ 111
        41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1 // 112 ~ 127
    };

    if (c < 0 || digest[(int)c] < 0)
    {
        luaL_error(L, "uuid short string invalid character:%c", c);
        return 0;
    }

    return digest[(int)c];
}

/**
 * 解析一个由uuid_short产生的短uuid
 * @param uuid 短uuid字符串
 * @return 标准uuid
 */
static int32_t uuid_short_parse(lua_State *L)
{
#ifdef __windows__
    luaL_error(L, "uuid_short_parse unsupported in this platform");
#else
    size_t len           = 0;
    const char *str_uuid = lua_tolstring(L, 1, &len);

    if (len != 22)
    {
        return luaL_error(L, "invalid uuid short string");
    }

    uuid_t u;
    char *uuid = reinterpret_cast<char *>(u);

    char fragment;
    /* 每次取4个字符填充到3个char，5次填充20字符,120bit */
    for (int32_t index = 0; index < 5; index++)
    {
        fragment = uuid_short_char(L, *str_uuid++);
        // 0011 1111 填充高6bit
        *uuid = char((fragment & 0x03f) << 2);

        fragment = uuid_short_char(L, *str_uuid++);
        // 0011 0000 取6bit中的高2bit，加上一步骤的6bit，填充完一个char
        *uuid++ |= char((fragment & 0x030) >> 4);
        // 0000 1111 取剩余下4bit填充到一个新char的高4bit
        *uuid = char((fragment & 0x00f) << 4);

        fragment = uuid_short_char(L, *str_uuid++);
        // 0011 1100 取6bit中的高4bit，加上一步骤的4bit，填充完一个char
        *uuid++ |= char((fragment & 0x03c) >> 2);
        // 0000 0011 取剩余的2bit，填充到一个新char的高2bit
        *uuid = char((fragment & 0x003) << 6);

        fragment = uuid_short_char(L, *str_uuid++);
        // 0011 1111 填充低6bit,加上一步骤的2bit，填充完一个char
        *uuid++ |= char(fragment & 0x03f);
    }

    fragment = uuid_short_char(L, *str_uuid++);
    // 0011 1111 填充高6bit
    *uuid = char((fragment & 0x03f) << 2);

    fragment = uuid_short_char(L, *str_uuid);
    // 0000 0011 取剩余的2bit，填充到一个char的低2bit
    *uuid |= (fragment & 0x003);

    char b[40] = {0};
    uuid_unparse(u, b);
    lua_pushstring(L, b);
#endif
    return 1;
}

/**
 * 取linux下errno或者win下WSAGetLastError对应的错误描述字符串
 * @param error 错误码
 * @return 错误描述字符串
 */
static int32_t what_error(lua_State *L)
{
    int32_t e = luaL_checkinteger32(L, 1);

    lua_pushstring(L, netcompat::strerror(e));

    return 1;
}

int32_t _raw_sha1(lua_State *L, int32_t index,
                  unsigned char sha1[SHA_DIGEST_LENGTH])
{
    SHA_CTX ctx;
    if (!SHA1_Init(&ctx))
    {
        return luaL_error(L, "sha1 init error");
    }

    for (int32_t i = index; i <= lua_gettop(L); ++i)
    {
        size_t len;
        const char *ptr = lua_tolstring(L, i, &len);
        if (!ptr)
        {
            // not a string and can not convert to string
            return luaL_error(L, "argument #%d expect string,got %s", i,
                              lua_typename(L, lua_type(L, i)));
        }

        SHA1_Update(&ctx, ptr, len);
    }

    SHA1_Final(sha1, &ctx);

    return 0;
}

/**
 * 计算字符串sha1编码
 * @param upper 可选参数，结果是否转换为大写
 * @param ... 需要计算的字符串，可以多个，sha1(true, str1, str2, ...)
 * @return sha1字符串
 */
static int32_t sha1(lua_State *L)
{
    unsigned char sha1[SHA_DIGEST_LENGTH];

    int32_t index   = 1;
    const char *fmt = "%02x"; // default format to lower
    if (!lua_isstring(L, 1))
    {
        index = 2;
        if (lua_toboolean(L, 1)) fmt = "%02X";
    }

    _raw_sha1(L, index, sha1);

    char buf[SHA_DIGEST_LENGTH * 2 + 1];
    for (int32_t i = 0; i < SHA_DIGEST_LENGTH; ++i)
    {
        //--%02x即16进制输出，占2个字节
        snprintf(buf + i * 2, 3, fmt, sha1[i]);
    }
    lua_pushlstring(L, buf, SHA_DIGEST_LENGTH * 2);

    return 1;
}

/**
 * sha1编码，返回20byte的十六进制原始数据而不是字符串
 * @param ... 需要计算的字符串，可以多个，sha1_raw(str1,str2,...)
 * @return 20byte的十六进制原始sha1数据
 */
static int32_t sha1_raw(lua_State *L)
{
    unsigned char sha1[SHA_DIGEST_LENGTH];
    _raw_sha1(L, 1, sha1);

    lua_pushlstring(L, (const char *)sha1, SHA_DIGEST_LENGTH);

    return 1;
}

/**
 * 计算base64编码
 * @param str 需要计算的字符串
 * @return base6字符串
 */
static int32_t base64(lua_State *L)
{
    /* base64需要预先计算长度并分配内存，故无法像md5那样把需要编码字符串分片传参
     */
    size_t len      = 0;
    const char *str = luaL_checklstring(L, 1, &len);

    BIO *base64 = BIO_new(BIO_f_base64());
    BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);

    BIO *bio = BIO_new(BIO_s_mem());
    base64   = BIO_push(base64, bio);

    // warning: value computed is not used [-Wunused-value]
    (void)BIO_set_close(base64, BIO_CLOSE);

    if (BIO_write(base64, str, (int32_t)len) && BIO_flush(base64))
    {
        BUF_MEM *bptr = nullptr;
        BIO_get_mem_ptr(base64, &bptr);
        lua_pushlstring(L, bptr->data, bptr->length);
    }

    BIO_free_all(base64);

    return 1;
}

/**
 * 如果不存在则创建多层目录，和shell指令mkdir -p效果一致
 * @param path linux下路径 path/to/dir
 * @return boolean
 */
static int32_t mkdir_p(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    // 未创建对返回false（包括目录已存在不需要创建）
    std::error_code e;
    bool ok = std::filesystem::create_directories(path, e);
    if (!ok && e)
    {
        ELOG("create_directories %s fail:%s", path, e.message().c_str());

        lua_pushboolean(L, 0);
    }
    else
    {
        lua_pushboolean(L, 1);
    }
    return 1;
}

/**
 * 递归列出指定目录中的文件，和shell指令ls效果一致
 * @param path linux下路径 path/to/dir
 * @return table 指定目录下的文件数组(包括目录，但不包括.和..这两个特殊目录)
 */
static int32_t ls(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);

    // https://en.cppreference.com/w/cpp/filesystem/recursive_directory_iterator
    // The special pathnames dot and dot-dot are skipped.
    std::error_code e;
    std::filesystem::recursive_directory_iterator dir_iter(path, e);
    if (e)
    {
        luaL_error(L, "can not open directory(%s):%s", path, e.message());

        return 0;
    }

    int32_t index = 0;
    lua_newtable(L);
    for (auto &p : dir_iter)
    {
        if (!p.is_regular_file()) continue;

        const std::string &&s_path = p.path().string();

        lua_pushstring(L, s_path.c_str());
        lua_rawseti(L, -2, ++index);
    }

    return 1;
}

static int32_t getcwd(lua_State *L)
{
    // C++ 17
    // cwd返回的是系统编码，比如utf8或者gbk
    std::filesystem::path cwd = std::filesystem::current_path();
    lua_pushstring(L, cwd.string().c_str());
    return 1;
}

static const luaL_Reg utillib[] = {{"ls", ls},
                                   {"md5", md5},
                                   {"uuid", uuid},
                                   {"sha1", sha1},
                                   {"base64", base64},
                                   {"getcwd", getcwd},
                                   {"mkdir_p", mkdir_p},
                                   {"sha1_raw", sha1_raw},
                                   {"what_error", what_error},
                                   {"uuid_short", uuid_short},
                                   {"get_addr_info", get_addr_info},
                                   {"uuid_short_parse", uuid_short_parse},
                                   {nullptr, nullptr}};

int32_t luaopen_util(lua_State *L)
{
    luaL_newlib(L, utillib);
    return 1;
}
