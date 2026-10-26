@echo off
rem TimeSplitters 2 -- a debugging launcher.
rem
rem You do not need this to play. The executable runs the game on its own:
rem
rem   titles\timesplitters2\build\Release\timesplitters2_recomp.exe
rem
rem Double-click it, or make a shortcut to it from anywhere -- it finds the
rem game beside itself, and the vblank, the audio hardware and the renderer
rem are all on unless something turns them off. This file exists only to set
rem the switches a debugging session wants, and to write down the keys.
rem
rem It is a windowed program, so there is no console. Its diagnostics go to
rem timesplitters2_recomp.log beside the executable when it is double-clicked,
rem or to this window when it is started from here.
rem
rem Where the game files are looked for, in order:
rem   1. a folder called "game" next to the executable
rem   2. games\<title>\ in this repository, relative to the executable
rem   3. RECOMP_GAME_DIR, which overrides both
rem
rem While it runs:
rem   F9   show or hide the frame rate
rem   F10  step the frame cap: adaptive, 60, 30, off
rem   F11  save the frame on screen -- a picture and a replayable capture,
rem        written next to the executable, for reporting a rendering bug
rem
rem Keyboard, unless it has been rebound: arrows = D-pad, Enter = START,
rem Backspace = BACK, Z = A, X = B, A = X, S = Y, Q = White, W = Black,
rem E = left trigger, R = right trigger. An XInput controller on port 0 works
rem as itself, and pads in slots 1-3 become controllers 2-4. To rebind, or to
rem pick which device drives which controller:
rem   py -3 -m tools.input_ui
rem
rem The game saves under games\Time Splitters 2\UDATA\4553000a\. A profile
rem saved by an earlier run changes the menu path ("save exists, overwrite?"),
rem which is the console's behaviour too.
rem
rem Useful environment, none of it required:
rem   RECOMP_FPS=5            frame-rate line every 5 s on stderr
rem   RECOMP_FPS_CAP=60       strict console pacing (30 = every second vblank,
rem                           adaptive = the default, 0 = uncapped)
rem   RECOMP_HLE_D3D8=off     no host rendering, for measuring the title alone
rem   RECOMP_AC97_READY=0     no emulated audio hardware
rem   RECOMP_VBLANK=0         no vblank
rem   RECOMP_INPUT_SEQ=...    scripted presses (see src/hle/input_host.c)
rem   RECOMP_INPUT_LOG=1      what the title reads from the pad
rem   RECOMP_GAME_DIR=...     run this build against another copy of the game
setlocal
cd /d "%~dp0build\Release" || exit /b 1
set RECOMP_FPS=5
timesplitters2_recomp.exe %*
