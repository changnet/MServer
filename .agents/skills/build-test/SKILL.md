---
name: build-test
description: MServer 的编译 + 单元测试 + 日志查看工作流。编译（增量/debug/release/refresh/单个 target）、启动测试节点（test1 --filter X --skip Y）、按节点名读取 server/var/log 日志并汇报通过/失败数。触发词：编译、构建、重编、rebuild、build、跑测试、单元测试、unit test、test1、--filter、--skip、看测试日志、测试失败。
---

# MServer 编译与单元测试

一条命令完成全部三步：

```bash
bash .agents/skills/build-test/scripts/run_test.sh test1 --filter kcp --skip udp
```

（脚本内部走的是 `shell/win_start.bat` / `shell/start.sh`，自动收工并汇总日志。编译仍要单独做一次。）

## 路径速查

| 名称 | 路径 | 说明 |
| --- | --- | --- |
| 构建目录 | `server/var/win-x64`（Windows）/ `server/var/linux-x64` | cmake 缓存 |
| 编译产物 | `server/var/master.exe` / `server/var/master` | `EXECUTABLE_OUTPUT_PATH` |
| 安装目录 | `server/bin` | `CMAKE_INSTALL_PREFIX`，启动脚本用的就是这个副本 |
| 运行目录 | `server/var` | 进程 cwd，读 `setting.lua`，写 `log/` |
| 日志目录 | `server/var/log/<节点名><3位序号>` | 见「步骤 4」 |

## 步骤 1：编译

Windows（全部用 VS 自带的 vcvars64 + cmake + ninja，不需要额外装东西）：

```bat
shell\win_build.bat              :: 增量编译 + 安装到 server\bin
shell\win_build.bat debug        :: 切 Debug 再编译安装
shell\win_build.bat release      :: 切 Release 再编译安装
shell\win_build.bat refresh      :: 删 cmake 缓存重新配置
shell\win_build.bat llhttp       :: 只编某个 target（不安装）
shell\win_build.bat verbose      :: 显示完整编译命令行
```

Linux：`sh shell/build.sh`，参数含义完全一样。

注意：

- **必须带 install 步骤**。编译产物落在 `server/var`，而启动脚本跑的是 `server/bin/master.exe`，少了 install 跑的还是旧程序。
- 源码用 `file(GLOB_RECURSE)` 收集，**新增 .cpp 文件后要 `refresh`** 才会被编进去。
- 增量编译 30 来个文件约 25s，全量更久 → 用较大的 timeout 或后台执行。
- 构建目录绑定了控制台代码页（脚本固定 `chcp 65001`，并在 `.dep_cp` 里记下）。换代码页后第一次运行会自动重新探测编译器，多花 10 几秒，属正常。

## 步骤 2：启动单元测试

必须在 `shell` 目录下（脚本里是 `cd ../server` 的相对路径）：

```bat
cd shell
win_start.bat test1 --filter kcp --skip udp
```

- 第一个参数是节点名，**必须带数字**：`test1`。写 `test` 会在 `startup.lua` 断言失败。
- 不传任何参数 → 启动默认的 gateway/game/data 三个进程，不是测试。
- 只要 filter：`win_start.bat test1 --filter kcp`
- 只要 skip：`win_start.bat test1 --skip "udp;kcp"`（多个模式用 `;` 分隔）
- 都不传：`win_start.bat test1` → 跑全部测试
- `--filter` 是 **lua 模式匹配**，只写标题里的普通单词；标题里带 `-`（量词）会过滤不到。

Linux 对应：`./start.sh test1 --filter kcp --skip udp`。

## 步骤 3：等它跑完（关键）

测试节点跑完**不会自己退出**，进程还在事件循环里。判据是日志里出现：

```
N passing, M failing (Xms)
```

看到这行才说明测试结束，然后必须结束进程：

```powershell
# 只杀测试节点，按命令行匹配，别误杀开发服
Get-CimInstance Win32_Process -Filter "Name='master.exe'" |
  Where-Object { $_.CommandLine -match '--node test1' } |
  ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
```

## 步骤 4：读日志

命名规则（`startup.lua` 的 `set_process_log`）：`log/<节点名><3位序号>`，
`--node test1` 就是 `log/test001`。

| 文件 | 含义 |
| --- | --- |
| `server/var/log/test001` | **本次/最新**，不带日期后缀 |
| `server/var/log/test00120261017` | 旧日志，跨天启动时旧文件被加上日期后缀 |
| `server/var/log/test001_error` | 只含 error 级别（告警用），崩溃先看它 |

```bash
tail -n 200 server/var/log/test001
grep -n "\[FAIL\]" server/var/log/test001
grep -E "[0-9]+ passing, [0-9]+ failing" server/var/log/test001 | tail -1
```

同一个进程的所有 worker 写同一个文件，行前缀形如 `[L10-20 18:26:24 test_0  ]`，
不用按 worker 找文件。彩色输出会带 ANSI 转义，读之前可以 `sed 's/\x1b\[[0-9;]*m//g'` 去掉。

## 步骤 5：汇报

至少给出：通过/失败条数、失败用例名、日志文件路径。失败时再贴失败行的上下文（got/expect、traceback）。

## 常见坑

- 节点名漏数字 → `startup.lua` 断言失败，进程直接挂。
- 忘了 install → 改了代码但测试行为没变化，先确认 `server/bin/master.exe` 的修改时间。
- 用 `--filter` 过滤不到 → 检查是不是用了 `-`、`(` 等模式字符。
- `win_start.bat` 传 `--filter "a b"` 要整体加双引号，bat 已用 `%*` 透传，不要自己再拼参数。
- 同时只能跑一个同名节点，第二个会抢端口/日志文件；先确认没有残留的 `master.exe --node test1`。
- 编译时刷屏 `注意: 包含文件: D:\...` → 构建目录是在**别的控制台代码页**下配置的。
  cmake 把 cl 的 `/showIncludes` 前缀按原样字节写进 `CMakeFiles/rules.ninja`，
  代码页一变前缀就匹配不上，ninja 既过滤不掉这些行，也**静默丢失头文件依赖**（改 .h 不重编）。
  用 `win_build.bat` 跑（它固定 `chcp 65001` 并做一次性重新探测），不要绕过它手工 `cmake`。
