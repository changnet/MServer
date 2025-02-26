@echo off
REM chcp 65001
SETLOCAL ENABLEDELAYEDEXPANSION

set DEF_PROC=(gateway game data)

cd ../server/bin

set BIN=%cd%/master.exe

if "%1" == "" (
    for %%a in %DEF_PROC% do (
        start %BIN% --node %%a
        REM timeout 3 > nul
    )
) else (
    REM 这种单独起服的，一般都是特殊测试，不需要start来起独立console
    REM win下参数带等号的，需要用双引号，如win_start.bat test "--filter abc"

    %BIN% --node %1 %2 %3
)
