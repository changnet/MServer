#pragma once

#include "global/global.hpp"

#include <cstddef> // offsetof

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h> // inet_ntop inet_pton
    #include <iphlpapi.h> // if_nametoindex 依赖
    #include <netioapi.h> // if_nametoindex
#else
    #include <arpa/inet.h>  /* inet_ntop inet_pton htons */
    #include <net/if.h>     /* if_nametoindex */
    #include <netinet/in.h> /* sockaddr_in sockaddr_in6 */
    #include <sys/socket.h> /* sockaddr sockaddr_storage */
#endif

/**
 * 把 "ip" 或 "ip%zone" 拆成纯地址和ipv6作用域。
 *
 * inet_pton 不认 "%" 后缀（windows和glibc都会返回0），所以必须自己切一刀。
 * zone 两种写法都支持：
 *   数字  "%3"      —— 接口索引，跨平台通用，推荐
 *   名字  "%eth0"   —— 走if_nametoindex，注意各系统命名不同
 *                     （linux是eth0，windows是 ethernet_32773 这种）
 *
 * @param ip       输入，形如 "fe80::1" 或 "fe80::1%eth0"
 * @param host     输出，去掉后缀的纯地址
 * @param host_len host缓冲区长度
 * @param scope    输出，接口索引，没有后缀时为0
 * @return false 表示地址太长，host装不下
 */
inline bool split_scope(const char *ip, char *host, size_t host_len,
                        uint32_t &scope)
{
    scope = 0;
    if (!ip || 0 == host_len) return false;

    const char *pct = strchr(ip, '%');
    size_t n        = pct ? (size_t)(pct - ip) : strlen(ip);
    if (n + 1 > host_len) return false;

    memcpy(host, ip, n);
    host[n] = '\0';

    if (!pct || '\0' == pct[1]) return true;

    const char *zone = pct + 1;
    char *end        = nullptr;
    unsigned long v  = strtoul(zone, &end, 10);
    if (end && '\0' == *end && v <= 0xFFFFFFFFul)
    {
        scope = (uint32_t)v; // 纯数字：直接当接口索引
        return true;
    }

#ifdef __windows__
    scope = (uint32_t)if_nametoindex(zone);
#else
    scope = (uint32_t)::if_nametoindex(zone);
#endif
    return true;
}

/**
 * sendto/recvfrom 用的地址缓冲：只需要装得下 v4/v6 两种。
 *
 * 不要用 sockaddr_storage（128字节）：那是"任意协议族的最坏情况仓库"，
 * 尺寸由 AF_UNIX 的 108 字节路径 + 8 字节对齐凑出来，跟 IPv6 无关。
 * 这里只要 v4(16) / v6(28)，28 字节足够，省掉每次近 100 字节的栈写入。
 *
 * 注意：本结构**不能**放进 #pragma pack(1)，内核会按 sockaddr* 解引用它，
 * 保持自然对齐。
 */
struct UdpSockAddr
{
    union
    {
        struct sockaddr sa; // 直接传给 sendto，不需要再转型
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    };
    socklen_t len; // sizeof(v4) / sizeof(v6)，0 表示地址无效

    UdpSockAddr() : v6{}, len(0)
    {
    }
};

#pragma pack(push, 1)

/**
 * 定长的udp地址，用于把地址塞进缓冲区随数据一起传递。
 *
 * 不能直接用struct sockaddr_storage，因为在不同平台上，这个结构的成员变量是不一
 * 样的，虽然都有sin_family但无法安全取值。
 * 
 * 也不能用family + struct sockaddr_storage，因为struct sockaddr_storage需要兼容
 * 所有网络协议（AF_UNIX（110B）、AF_NETLINK。。。），这个结构比UdpAddr大很多，做
 * hash时也慢很多
 *
 * 关于 scope_id_：ipv6的link-local地址(fe80::/10)只在一条链路上有效，
 * 同一个fe80::1可以同时出现在多块网卡上，光靠16字节地址分不清是哪个客户端。
 * 内核用接口索引(sin6_scope_id)来选链路，这里是它的原样拷贝(主机序)。
 * 不带它的后果：回包可能走错链路或直接失败；更糟的是两个不同链路上的
 * fe80::1会算出同一个key，被当成同一个客户端。
 *
 */
struct UdpAddr
{
    uint16_t family_;   // AF_INET / AF_INET6 / AF_UNSPEC(已connect，用::send发送)
    uint16_t port_;     // 网络序
    uint8_t addr_[16];  // v4用前4字节，其余必须为0
    uint32_t scope_id_; // v6的link-local(fe80::)支持

    UdpAddr()
    {
        clear();
    }

    void clear()
    {
        family_   = AF_UNSPEC;
        port_     = 0;
        scope_id_ = 0;
        // v4只用前4字节，其余必须是0：整个结构体会原样传给lua当hash key
        memset(addr_, 0, sizeof(addr_));
    }

    bool operator==(const UdpAddr &o) const
    {
        return 0 == memcmp(this, &o, sizeof(UdpAddr));
    }

    /// 是否未指定地址(已connect的udp socket，用::send发送即可)
    bool is_default() const
    {
        return AF_UNSPEC == family_;
    }

    /// recvfrom后填充
    void from_sockaddr(const sockaddr *sa)
    {
        // 复用对象时不能留下上一轮的字节，否则同一个客户端每次算出的key不同
        clear();
        if (!sa) return;

        if (AF_INET == sa->sa_family)
        {
            const struct sockaddr_in *s4 =
                reinterpret_cast<const struct sockaddr_in *>(sa);
            family_ = AF_INET;
            port_   = s4->sin_port;
            memcpy(addr_, &s4->sin_addr, sizeof(s4->sin_addr)); // 4字节
        }
        else if (AF_INET6 == sa->sa_family)
        {
            const struct sockaddr_in6 *s6 =
                reinterpret_cast<const struct sockaddr_in6 *>(sa);
            family_    = AF_INET6;
            port_      = s6->sin6_port;
            scope_id_  = s6->sin6_scope_id; // link-local必须保留，否则回不了包
            memcpy(addr_, &s6->sin6_addr, sizeof(s6->sin6_addr)); // 16字节
        }
    }

    /**
     * 转成 sendto 需要的结构，返回false表示地址无效(AF_UNSPEC/未知族)
     *
     * 只初始化真正会被内核读到的那 16/28 字节，不做整块 memset：
     * sin_zero 必须为0所以清掉，v6 尾部的 flowinfo/scope_id 一并清0，
     * 但 union 之后、超出 addrlen 的部分一律不碰。
     */
    bool to_sockaddr(UdpSockAddr &out) const
    {
        if (AF_INET == family_)
        {
            memset(&out.v4, 0, sizeof(out.v4));
            out.v4.sin_family = AF_INET;
            out.v4.sin_port   = port_;
            memcpy(&out.v4.sin_addr, addr_, sizeof(out.v4.sin_addr));
            out.len = (socklen_t)sizeof(out.v4);
            return true;
        }

        if (AF_INET6 == family_)
        {
            memset(&out.v6, 0, sizeof(out.v6));
            out.v6.sin6_family   = AF_INET6;
            out.v6.sin6_port     = port_;
            out.v6.sin6_scope_id = scope_id_; // link-local的链路选择器
            memcpy(&out.v6.sin6_addr, addr_, sizeof(out.v6.sin6_addr));
            out.len = (socklen_t)sizeof(out.v6);
            return true;
        }

        out.len = 0;
        return false;
    }

    /// 转成可读的ip字符串，buf建议不小于INET6_ADDRSTRLEN
    const char *to_string(char *buf, size_t len) const
    {
        if (0 == len) return "";
        buf[0] = '\0';

        if (AF_INET == family_ || AF_INET6 == family_)
        {
#ifdef __windows__
            inet_ntop(family_, (void *)addr_, buf, len);
#else
            inet_ntop(family_, (const void *)addr_, buf, (socklen_t)len);
#endif
        }

        // link-local要带上作用域，否则这个字符串拿去from_string/connect会丢链路
        if (AF_INET6 == family_ && 0 != scope_id_)
        {
            size_t n = strlen(buf);
            if (n + 12 < len) snprintf(buf + n, len - n, "%%%u", scope_id_);
        }

        return buf;
    }

    /// 从ip字符串解析，port为主机序。ip可带"%zone"后缀(见split_scope)
    bool from_string(const char *ip, uint16_t port)
    {
        clear();
        if (!ip) return false;

        // inet_pton不认"%zone"，先切掉
        char host[INET6_ADDRSTRLEN];
        uint32_t scope = 0;
        if (!split_scope(ip, host, sizeof(host), scope)) return false;

        struct in_addr v4;
        if (1 == inet_pton(AF_INET, host, &v4))
        {
            family_ = AF_INET;
            port_   = htons(port);
            memcpy(addr_, &v4, sizeof(v4));
            return true;
        }

        struct in6_addr v6;
        if (1 == inet_pton(AF_INET6, host, &v6))
        {
            family_   = AF_INET6;
            port_     = htons(port);
            scope_id_ = scope;
            memcpy(addr_, &v6, sizeof(v6));
            return true;
        }

        return false;
    }
};
#pragma pack(pop)


/**
 * UdpAddr 的hash函数（UdpAddr 已有 operator==，直接 memcmp 24 字节）
 *
 * FNV-1a，但一次吃4字节而不是逐字节：24字节的key每个包都要算一次
 * （established_.find / accepting_.find），本机实测约快3倍。
 * 尾部异或右移是为了让乘法产生的高位进位扩散回低位。
 *
 * ★ UdpAddr 是 #pragma pack(1)（alignof == 1），不能直接解引用 uint32_t，
 *   一律 memcpy 到局部变量，编译器会把它优化成单次load。
 */
struct UdpAddrHash
{
    size_t operator()(const UdpAddr &a) const
    {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&a);
        size_t h         = 1469598103934665603ull; // FNV-1a offset basis

        size_t i = 0;
        for (; i + sizeof(uint32_t) <= sizeof(UdpAddr); i += sizeof(uint32_t))
        {
            uint32_t v = 0;
            memcpy(&v, p + i, sizeof(v));
            h = (h ^ v) * 1099511628211ull;
            h ^= h >> 29;
        }

        // 兜底：sizeof(UdpAddr) 不是4的倍数时也不漏字节（现在正好24，循环跑满）
        for (; i < sizeof(UdpAddr); ++i)
        {
            h ^= p[i];
            h *= 1099511628211ull;
        }

        return h;
    }
};

static_assert(sizeof(UdpSockAddr) >= sizeof(struct sockaddr_in6),
              "UdpSockAddr must be able to hold sockaddr_in6");

static_assert(sizeof(UdpAddr) == 24, "UdpAddr must be 24 bytes");
static_assert(alignof(UdpAddr) == 1, "UdpAddr must be packed");

// udp_io.cpp的send path按offsetof直接从缓冲区里取字段，
// 布局变了这里会编译报错，而不是运行期算出错误的地址
static_assert(offsetof(UdpAddr, family_) == 0, "UdpAddr layout changed");
static_assert(offsetof(UdpAddr, port_) == 2, "UdpAddr layout changed");
static_assert(offsetof(UdpAddr, addr_) == 4, "UdpAddr layout changed");
static_assert(offsetof(UdpAddr, scope_id_) == 20, "UdpAddr layout changed");

/// 单个udp数据包的最大长度(ipv4: 65535 - 20(ip头) - 8(udp头) = 65507)
static constexpr int32_t UDP_MAX_DGRAM = 65507;

/// udp帧头长度 = size(4) + UdpAddr(24)，size含自身
static constexpr int32_t UDP_FRAME_HEAD =
    (int32_t)(sizeof(uint32_t) + sizeof(UdpAddr));
