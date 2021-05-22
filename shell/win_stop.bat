@if (@X)==(@Y) @end /* Harmless hybrid line that begins a JScript comment
:: https://stackoverflow.com/questions/15167742/execute-wshshell-command-from-a-batch-script 混合JScript和bat脚本

:: ******* Begin batch code *********
@echo off
REM chcp 65001
SETLOCAL ENABLEDELAYEDEXPANSION

set bin=master.exe
set DEF_ID=1
set DEF_INDEX=1
set DEF_APP=(android gateway  world area area)

REM %~dp0和%cd%都可以 %cd%\..\server\bin
call :to_absolute_path %cd%\..\server\bin bin_dir
set bin_path=%bin_dir%\%bin%
echo %bin_path%

REM wmic被deprecated了，新版可用power shell的get-process
REM powershell get-process master | select-object ProcessName, CommandLine

for %%a in %DEF_APP% do (
    if "%%a" == "!last_app!" (
        set /a last_idx=!last_idx! + 1
    ) else (
        set last_idx=%DEF_INDEX%
        set last_app=%%a
    )
    set proc_id=
    call :find_proc %%a !last_idx! %DEF_ID% proc_id
    if "!proc_id!"=="" (
        echo "%%a --index=!last_idx! not found, maybe not runing"
    ) else (
        echo "find %%a --index=!last_idx!, process id = !proc_id!"
        call :kill_proc !proc_id!
    )
    timeout 3 > nul
)

goto :eof

REM 查找对应的进程 find_proc app index id
REM @return 对应的进程id
:find_proc
REM master.exe  --app=android --index=1 --id=1  E:\dev\MServer\server\bin\master.exe  11700
REM wmic返回的数据共有3行（最后一行是空行），可以用for的skip来跳过第一行，但最后一行跳不过。因此需要findstr来获取中间的一行

REM ^转义字符
REM tokens和delims见 https://ss64.com/nt/for_f.html tokens分解得到的变量会按字母排列下去，如 %%i %%j %%k
REM 2^> nul是不显示错误
for /f "tokens=2,3,4,6" %%i in ('WMIC Process Where ^"Name^=^'%bin%^'^" get executablepath^,commandline^,processid 2^> nul ^| findstr /L %bin_path%') do (
    if "%%i"=="--app=%1" (
        if "%%j"=="--index=%2" (
            if "%%k"=="--id=%3" (
                set "%~4=%%l"
            )
        )
    )
)
goto :eof

REM 终止进程 kill_proc pid
:kill_proc
REM taskkill -T -PID 9424 taskkill只能直接杀，不能安全终止。见C++中Thread::signal注释
cscript //E:JScript //nologo "%~f0" %1
goto :eof

REM 把相对路径转换为绝对路径 https://stackoverflow.com/questions/1645843/resolve-absolute-path-from-relative-path-and-or-file-name
:to_absolute_path
set "%~2=%~f1"
goto :eof

********* Begin JScript code **********/
var ws=WScript.CreateObject("WScript.Shell");
// 激活进程，然后发送ctrl c(不激活收不到的)
ws.AppActivate(WScript.Arguments(0));
ws.sendkeys("^{c}");

