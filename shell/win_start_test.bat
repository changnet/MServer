@echo off

set /p filter="please input filter, eg: metatable;http  "

cd ../server/bin

set BIN=%cd%/master.exe

if "%filter%" == "" (
	%BIN% --app=test --index=1 --id=1
) else (
REM call win_start.bat "--filter=%filter%" 没法传包含空格的参数
	%BIN% --app=test --index=1 --id=1 "--filter=%filter%"
)
REM call win_start.bat android 1 1
