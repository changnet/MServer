@echo off
rem ---------------------------------------------------------------------------
rem vcpkg.bat install <vcpkg root>
rem     git pull in <vcpkg root>, then install all dependencies
rem vcpkg.bat export [vcpkg root]
rem     export and rename the output directory to vcpkg-export
rem
rem <vcpkg root> is only used to find vcpkg.exe, if omitted vcpkg comes from
rem PATH. all other paths are relative to the project root(engine's parent)
rem and use fixed names, so nothing here needs to be changed between exports.
rem ---------------------------------------------------------------------------

rem cd MServer\shell
rem .\vcpkg.bat install D:\dev\vcpkg
rem .\vcpkg.bat export

setlocal
cd /d "%~dp0.."

if "%2"=="" (set VCPKG=vcpkg) else (set VCPKG=%2\vcpkg.exe)

if "%1"=="export" goto do_export

:do_install
git -C "%2" pull
"%VCPKG%" install --x-manifest-root=.\project --x-install-root=.\engine\vcpkg_win
goto :eof

:do_export
rem remove the whole output directory, otherwise the previous vcpkg-export and
rem the .txt notes keep piling up. vcpkg recreates it during export
rd /s /q .\engine\vcpkg_win_x64

rem windows 的删除操作是异步的，后续要马上创建同名文件夹要等删除完成
ping -n 2 127.0.0.1 >nul

"%VCPKG%" export --raw --x-manifest-root=.\project --x-install-root=.\engine\vcpkg_win --output-dir=.\engine\vcpkg_win_x64

rem vcpkg always names the output directory vcpkg-export-<date>-<time>
for /f %%d in ('dir /b /ad /od ".\engine\vcpkg_win_x64\vcpkg-export-*"') do set EXPORT_DIR=%%d

echo rename ".\engine\vcpkg_win_x64\%EXPORT_DIR%" to ".\engine\vcpkg_win_x64\vcpkg-export"

rem vcpkg导出时，进程退出不代码文件完全导出完。重命名要等导出完成
ping -n 2 127.0.0.1 >nul
ren ".\engine\vcpkg_win_x64\%EXPORT_DIR%" vcpkg-export

rem keep the original directory name, it is lost after the rename
echo %EXPORT_DIR%>.\engine\vcpkg_win_x64\%EXPORT_DIR%.txt

rem vcpkg installs the mariadb headers as include\mysql while the source uses
rem <mariadb/mysql.h> the same way apt does on debian
xcopy /e /i /y .\engine\vcpkg_win_x64\vcpkg-export\installed\x64-windows\include\mysql .\engine\vcpkg_win_x64\vcpkg-export\installed\x64-windows\include\mariadb
