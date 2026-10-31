@echo off
rem Runs the Makefile with the make, shell and tools that come with Simplicity
rem Studio (the same ones Studio builds with).  Examples:
rem     make build          (cmd)
rem     .\make flash        (PowerShell)
rem     ./make.cmd ota      (Git Bash)
setlocal
if "%STUDIO_DIR%"=="" set STUDIO_DIR=C:\SiliconLabs\SimplicityStudio\v5
set PATH=%STUDIO_DIR%\support\common\build\msys\1.0\bin;%STUDIO_DIR%\developer\toolchains\gnu_arm\12.2.rel1_2023.7\bin;%PATH%
cd /d "%~dp0"
make.exe %*
