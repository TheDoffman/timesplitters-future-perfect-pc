@echo off
setlocal
set "native_fps=120"
if "%~1"=="60" set "native_fps=60"
if "%~1"=="120" set "native_fps=120"
if "%~1"=="144" set "native_fps=144"
if "%~1"=="180" set "native_fps=180"
if "%~1"=="240" set "native_fps=240"
if not "%~1"=="" if not "%~1"=="60" if not "%~1"=="120" if not "%~1"=="144" if not "%~1"=="180" if not "%~1"=="240" (
    echo Usage: Play-Native.cmd [60^|120^|180^|240]
    exit /b 1
)
set "TSFP_NATIVE_FPS=%native_fps%"
set "RECOMP_FRAME_TARGET=0"
set "RECOMP_FRAME_INTERP=1"
set "RECOMP_FPS_CAP=0"
set "RECOMP_VBLANK_HZ=60"
set "RECOMP_FULLSCREEN=1"
set "RECOMP_VRR=1"
set "RECOMP_FPS_OVERLAY=1"
set "RECOMP_WINDOW_BACKGROUND=0"
set "RECOMP_REFRESH_RATE=%native_fps%"
call "%~dp0Play.cmd"
endlocal
