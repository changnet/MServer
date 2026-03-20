@echo off

setlocal EnableDelayedExpansion

set CURRENT_DIR=%CD%

cd ../server/pb

set counter=0
for /r %%a in (*.proto) do (
	set /a counter += 1
    echo compiling %%~na.proto ...

    ..\..\tools\protoc -o %%~na.pb %%~na.proto
)

echo protobuf schema file complie finish,files: !counter!

cd /d "%CURRENT_DIR%"
set WORK_DIR=..\server\src\modules\protocol

cd %WORK_DIR%\
set FULL_WORK_DIR=%CD%
lua auto.lua protocol.lua %FULL_WORK_DIR%

pause
