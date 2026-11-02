# 框架基础功能（极少变动）

1. 玩家：玩家相关的逻辑在server\src\modules\player\player.lua
2. 日志：在server\src\log\log.lua
3. 资源：发放奖励、扣除消耗等资源（道具、货币）由server\src\modules\res\res.lua处理
4. 事件：事件在server\src\modules\event\event_header.lua中定义

## 日志文件规则

文件名由 server\src\engine\startup.lua 的 `set_process_log` 决定，路径相对进程工作目录
（server\var），即 var\log\。写入/轮转在 engine\src\log\async_log.cpp。

1. **名字 = 节点名 + 三位序号**：`test1` → `var\log\test001`，`gateway2` → `var\log\gateway002`
   （`string.format("log/%s%03d", node_name, node_index)`）。节点名必须带数字，
   只有名字时序号为空，日志名就对不上。
2. **当前日志恒为不带任何后缀的名字**（如 `test001`）。跨天时旧文件被改名为
   `test001YYYYMMDD`（如 `test00120261102`），之后继续往 `test001` 写
   （`AsyncLog::trigger_daily_rollover`）。所以取「最新日志」永远读不带日期的那一个，
   不要去翻带日期的文件。
3. `info` 设备按天轮转（`POLICY_DAILY`）；`error` 设备按大小轮转（`POLICY_SIZE`，10MB），
   轮转后是 `test001_error.001`、`.002`…… 这是另一套命名，别和跨天的日期后缀混了。
4. 错误日志既汇总进 info（`test001` 里有同样的行），也单独写 `test001_error`。

当需要读取日志时（比如单元测试、CI），按上面的规则去读取
