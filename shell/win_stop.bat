@echo off
REM chcp 65001
SETLOCAL ENABLEDELAYEDEXPANSION

原生win处理
https://github.com/libuv/libuv/blob/dec0723864c2a1d41cfbab8164c5683f5cffff14/src/win/signal.c#L130

按理来说，win的c库有把原生的事件转换为posix信号，这种方式可以测试下
https://stackoverflow.com/questions/27104559/after-sending-ctrl-c-to-a-child-proc-how-do-i-re-attach-handling-to-parent-wit

相关讨论
https://stackoverflow.com/questions/40762545/how-to-send-a-ctrlc-sigint-to-a-subprocess-in-windows

$ taskkill -T -PID 9424
ERROR: The process with PID 1340 (child process of PID 9424) could not be terminated.
Reason: This process can only be terminated forcefully (with /F option).
powershell get-process master | select-object ProcessName, CommandLine