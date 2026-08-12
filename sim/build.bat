@echo off
setlocal
cd /d "%~dp0"

set VENDOR=%~dp0vendor
set MINGW=%VENDOR%\w64devkit\bin
set SDL2=%VENDOR%\SDL2-2.30.11\x86_64-w64-mingw32

if not exist "%MINGW%\gcc.exe" (
    echo [ERROR] MinGW not found. Run download_tools.bat first.
    exit /b 1
)
if not exist "%SDL2%\include\SDL2\SDL.h" (
    echo [ERROR] SDL2 dev package not found. Run download_tools.bat first.
    exit /b 1
)

set "PATH=%MINGW%;%PATH%"

gcc -O2 -std=gnu99 -DBM_SIM=1 -DUI_BM_EN=1 ^
  -I..\HAL -I..\HAL\include -I"%SDL2%\include" -I"%SDL2%\include\SDL2" ^
  main.c sim_nv3007.c ..\HAL\bm_ui.c ..\HAL\bm_font.c ^
  -L"%SDL2%\lib" -lmingw32 -lSDL2main -lSDL2 ^
  -o nv3007_sim.exe
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

copy /y "%SDL2%\bin\SDL2.dll" . >nul
echo.
echo Build OK: nv3007_sim.exe
echo Run: nv3007_sim.exe
