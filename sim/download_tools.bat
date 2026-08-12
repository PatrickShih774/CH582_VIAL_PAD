@echo off
setlocal
cd /d "%~dp0"
mkdir vendor 2>nul
cd vendor

echo Downloading w64devkit (MinGW-w64 toolchain)...
powershell -NoProfile -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://github.com/skeeto/w64devkit/releases/download/v2.3.0/w64devkit-x64-2.3.0.zip' -OutFile 'w64devkit.zip'"
if errorlevel 1 goto fail
powershell -NoProfile -Command "Expand-Archive -Path 'w64devkit.zip' -DestinationPath '.' -Force"
if errorlevel 1 goto fail

echo Downloading SDL2 mingw dev package...
powershell -NoProfile -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-devel-2.30.11-mingw.zip' -OutFile 'sdl2.zip'"
if errorlevel 1 goto fail
powershell -NoProfile -Command "Expand-Archive -Path 'sdl2.zip' -DestinationPath '.' -Force"
if errorlevel 1 goto fail

echo.
echo Tools ready. Run build.bat
exit /b 0

:fail
echo [ERROR] Download/extract failed. Check network and retry.
exit /b 1
