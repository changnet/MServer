@echo off

setlocal EnableDelayedExpansion

cd ../server/pb

set counter=0
for /r %%a in (*.proto) do (
	set /a counter += 1
    echo compiling %%~na.proto ...

    ..\..\tools\protoc -o %%~na.pb %%~na.proto
)

echo protobuf schema file complie finish,files: !counter!

pause
