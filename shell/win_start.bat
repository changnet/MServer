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
    exit 0
) else (
    REM win下貌似没有function，只能重复写了
    set app=%1
    if "%app:~0, 2%" == "--" (
        %BIN% %1 %2 %3 %4 %5
        exit 0
    )

    if "%app%" == "test" (
        %BIN% --app=test %2 %3 %4 %5
        exit 0
    )

    set index=%2
    if "%index%" == "" (
        set index=%DEF_INDEX%
    )

    set id=%3
    if "%id%" == "" (
        set id=%DEF_ID%
    )

    %BIN% --app=!app! --index=!index! --id=!id!
)
