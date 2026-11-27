@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\generate.ps1" %*
pause
