@echo off
cd /d "%~dp0"

python ../../../exceltools.py %*

echo .
pause
