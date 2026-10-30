@echo off
rem Burnout 2 -- a debugging launcher.
rem
rem You do not need this to play: burnout2_recomp.exe in build\Release runs the
rem game on its own, from a double-click or a shortcut anywhere. See
rem titles\timesplitters2\run.bat for where the game files are looked for, the
rem keys, and the switches a debugging session might want; they are the same
rem here.
setlocal
cd /d "%~dp0build\Release" || exit /b 1
set RECOMP_FPS=5
burnout2_recomp.exe %*
