#pragma once

#include "global/global.hpp"

#ifdef __windows__
    #include <winsock2.h>
    #include <ws2tcpip.h> // inet_ntop inet_pton
#else
    #include <arpa/inet.h>  /* inet_ntop inet_pton htons */
    #include <netinet/in.h> /* sockaddr_in sockaddr_in6 */
    #include <sys/socket.h> /* sockaddr sockaddr_storage */
#endif

/**
 * 定长的udp地址，用于把地址塞进缓冲区随数据一起传递。
 *
 * v4只用前4字节，其余必须为0。因为整个结构体会被原样(lua_pushlstring)传给lua
 * 当hash key，未初始化的字节会让同一个客户端每次算出不同的key，所以：
 *     1. 构造函数和from_sockaddr都会先清零
 *     2. 从缓冲区里取地址必须memcpy，不能reinterpret_cast(alignof == 1)
 */
#pragma pack(push, 1)
struct UdpAddr
{
    uint16_t family_;   // AF_INET / AF_INET6 / AF_UNSPEC(已connect，用::send发送)
    uint16_t port_;     // 网络序
    uint8_t addr_[16];  // v4用前4字节，其余必须为0

    UdpAddr()
    {
        clear();
    }

    void clear()
    {
        family_  = AF_UNSPEC;
        port_    = 0;
        addr_[0] = '\0';
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
        clear(); // ★ 双保险
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
            family_ = AF_INET6;
            port_   = s6->sin6_port;
            memcpy(addr_, &s6->sin6_addr, sizeof(s6->sin6_addr)); // 16字节
        }
    }

    /// 给sendto用，返回地址长度，0表示地址无效
    socklen_t to_sockaddr(sockaddr_storage &ss) const
    {
        memset(&ss, 0, sizeof(ss));

        if (AF_INET == family_)
        {
            struct sockaddr_in *s4 = reinterpret_cast<struct sockaddr_in *>(&ss);
            s4->sin_family = AF_INET;
            s4->sin_port   = port_;
            memcpy(&s4->sin_addr, addr_, sizeof(s4->sin_addr));
            return (socklen_t)sizeof(struct sockaddr_in);
        }
        if (AF_INET6 == family_)
        {
            struct sockaddr_in6 *s6 =
                reinterpret_cast<struct sockaddr_in6 *>(&ss);
            s6->sin6_family = AF_INET6;
            s6->sin6_port   = port_;
            memcpy(&s6->sin6_addr, addr_, sizeof(s6->sin6_addr));
            return (socklen_t)sizeof(struct sockaddr_in6);
        }

        return 0;
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

        return buf;
    }

    /// 从ip字符串解析，port为主机序
    bool from_string(const char *ip, uint16_t port)
    {
        clear();
        if (!ip) return false;

        struct in_addr v4;
        if (1 == inet_pton(AF_INET, ip, &v4))
        {
            family_ = AF_INET;
            port_   = htons(port);
            memcpy(addr_, &v4, sizeof(v4));
            return true;
        }

        struct in6_addr v6;
        if (1 == inet_pton(AF_INET6, ip, &v6))
        {
            family_ = AF_INET6;
            port_   = htons(port);
            memcpy(addr_, &v6, sizeof(v6));
            return true;
        }

        return false;
    }
};
#pragma pack(pop)

static_assert(sizeof(UdpAddr) == 20, "UdpAddr must be 20 bytes");
static_assert(alignof(UdpAddr) == 1, "UdpAddr must be packed");

/// 单个udp数据包的最大长度(ipv4: 65535 - 20(ip头) - 8(udp头) = 65507)
static constexpr int32_t UDP_MAX_DGRAM = 65507;

/// udp帧头长度 = size(4) + UdpAddr(20)，size含自身
static constexpr int32_t UDP_FRAME_HEAD =
    (int32_t)(sizeof(uint32_t) + sizeof(UdpAddr));
