@echo off

set /p filter="please input filter, eg: metatable;http  "

cd ../server/bin

set ASAN_OPTIONS=detect_leaks=1
set BIN=%cd%/master.exe

if "%filter%" == "" (
	%BIN% --node=test
) else (
REM call win_start.bat "--filter=%filter%" 没法传包含空格的参数
	%BIN% --node=test "--filter=%filter%"
)
REM call win_start.bat android 1 1
