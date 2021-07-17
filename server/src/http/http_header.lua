-- http接口相关宏定义
HTTP = {
    -- 接口错误码定义
    OK = {0, "success"}, -- 0 成功,返回 succes 字符串
    OK_NIL = {0}, -- 0, 成功，返回的内容由对应的模块构建，不根据错误码构建
    PENDING = {1}, -- 阻塞，等待异步返回数据
    INVALID = {2, "invalid arguments"}, -- 1 无效的gm

    -- 返回数据定义
    P404 = 'HTTP/1.1 404 Not Found\r\n\z
        Content-Length: 0\r\n\z
        Content-Type: text/html\r\n\z
        Server: Mini-Game-Distribute-Server/1.0\r\n\z
        Connection: close\r\n\r\n',

    P500 = 'HTTP/1.1 500 Internal server error\r\n\z
        Content-Length: 0\r\n\z
        Content-Type: text/html\r\n\z
        Server: Mini-Game-Distribute-Server/1.0\r\n\z
        Connection: close\r\n\r\n',

    P200 = 'HTTP/1.1 200 OK\r\n\z
        Content-Length: %d\r\n\z
        Content-Type: application/json; charset=UTF-8\r\n\z
        Server: Mini-Game-Distribute-Server/1.0\r\n\z
        Connection: keep-alive\r\n\r\n%s',

    P200_CLOSE = 'HTTP/1.1 200 OK\r\n\z
        Content-Length: %d\r\n\z
        Content-Type: application/json; charset=UTF-8\r\n\z
        Server: Mini-Game-Distribute-Server/1.0\r\n\z
        Connection: close\r\n\r\n%s',

    PGET = 'GET %s%s%s HTTP/1.1\r\n\z
        Host: %s\r\n\z
        Connection: keep-alive\r\n\z
        Upgrade-Insecure-Requests: 1\r\n\z
        Accept: text/plain,application/json\r\n\r\n',

    PPOST = 'POST %s HTTP/1.1\r\n\z
        HOST: %s\r\n\z
        Content-Length: %d\r\n\z
        Connection: keep-alive\r\n\z
        Accept: text/plain,application/json\r\n\r\n%s'
}

return HTTP
