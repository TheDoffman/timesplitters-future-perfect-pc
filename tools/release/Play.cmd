@echo off
setlocal
if exist "%~dp0TimeSplitters Launcher.exe" (
    start "" "%~dp0TimeSplitters Launcher.exe"
    exit /b
)
if exist "%~dp0Data\default.xbe" goto play
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0Import-game.ps1"
if errorlevel 2 exit /b 2
if errorlevel 1 goto failed
:play
start "" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -STA -WindowStyle Hidden -ExecutionPolicy Bypass -File "%~dp0Start-game.ps1" -Script "%~dp0Launch.ps1"
if errorlevel 1 goto failed
exit /b 0
:failed
echo Setup or launch failed. See the message above and the Logs folder.
pause
exit /b 1
