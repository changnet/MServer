@echo off

set WORK_DIR=..\server\src\modules\protocol

cd %WORK_DIR%\
set FULL_WORK_DIR=%CD%
lua auto.lua protocol.lua %FULL_WORK_DIR%

pause
