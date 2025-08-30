@echo off

set /p filter="please input filter, eg: metatable;http  "

cd ../server

set ASAN_OPTIONS=detect_leaks=1
set BIN=%cd%/bin/master.exe

cd var

if "%filter%" == "" (
	%BIN% --node test
) else (
	%BIN% --node test --filter "%filter%"
)
REM call win_start.bat android 1 1
