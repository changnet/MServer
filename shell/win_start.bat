@echo off
REM chcp 65001
SETLOCAL ENABLEDELAYEDEXPANSION

set DEF_ID=1
set DEF_INDEX=1
set DEF_APP=(gateway  world area area)

cd ../server/bin

set BIN=%cd%/master.exe

if "%1" == "" (
    for %%a in %DEF_APP% do (
        if "%%a" == "!last_app!" (
            set /a last_idx=!last_idx! + 1
        ) else (
            set last_idx=%DEF_INDEX%
            set last_app=%%a
        )

        start "%%a --index=!last_idx!" %BIN% --app=%%a --index=!last_idx! --id=%DEF_ID%
        timeout 3 > nul
    )
) else (
    call :fmt_args app test %1
    call :fmt_args index %DEF_INDEX% %2
    call :fmt_args id %DEF_ID% %3

    REM 这种单独起服的，一般都是特殊测试，不需要start来起独立console
    REM win下参数带等号的，需要用双引号，如win_start.bat test "--filter=string"
    %BIN% !app! !index! !id! %4 %5 %6
)
goto :eof

REM 格式化参数，如果是以--开头，则不处理，否则前3个参数补上 --app --index --id，减少起服时输入的参数
:fmt_args
set p=%1
set d=%2
REM 当参数里带等号时，只能用双引号传参(单引号不行)，而这个双引号会引起IF语法错误
REM 这里用%~3而不是%3可以去掉两边的双引号
set v=%~3

REM 不传参，则使用默认参数，这里不能用双引号，因为参数里可能带双引号，bat会直接报语法错误
if "%v%" == "" (
    set v=--%p%=%d%
)

REM 传的参数不以--开头，则补上--
if not "%v:~0, 2%" == "--" (
    set v=--%p%=%v%
)

set "%~1=%v%"
goto :eof