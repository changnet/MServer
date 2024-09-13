@echo off

set /p filter="please input filter, eg: metatable;http  "

if "%filter%" == "" (
	call win_start.bat test
) else (
	call win_start.bat test "--filter=%filter%"
)
REM call win_start.bat android 1 1
