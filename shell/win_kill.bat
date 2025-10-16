@echo off
REM chcp 65001
SETLOCAL ENABLEDELAYEDEXPANSION

REM 用于程序出错时，无法正常关闭，直接kill掉进程

cd ../server
set BIN=%cd%\bin\master.exe
:: 把路径中的\转为\\
set wmic_path=!BIN:\=\\!

:: wmic返回的pid包含空格和换行符，需要findstr截取pid
for /f "tokens=1" %%i in (
	'wmic process where "executablepath like '%%!wmic_path!%%'" get processid ^| findstr [0-9]'
) do (
	set pid=%%i
	:: echo kill pid=!pid!
	taskkill /F /PID !pid!
)

echo kill %BIN% done
pause
