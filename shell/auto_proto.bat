@echo off

set WORK_DIR=..\server\src\modules\protocol

lua %WORK_DIR%\auto.lua %WORK_DIR%\protocol.lua %WORK_DIR%

pause
