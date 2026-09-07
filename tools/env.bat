@echo off
rem AVP Access -- shared build environment.
rem
rem Nothing here is hardcoded to one machine. Visual Studio is located with
rem vswhere, and the vendored SDKs are found by globbing their version-stamped
rem folder names, so a newer SDL3 or FFmpeg drops in without editing anything.
rem Every value can still be overridden from the environment.
rem
rem   env.bat        set up paths and the MSVC command-line environment
rem   env.bat novc   paths only -- skips the slow vcvars call, for running

rem Project root: the folder holding NakedAVP, build, game and third_party.
if not defined AVP_ROOT set "AVP_ROOT=%~dp0..\.."

set "SRC=%AVP_ROOT%\NakedAVP"
set "BUILD=%AVP_ROOT%\build"
set "GAMEDATA=%AVP_ROOT%\game"
set "TP=%AVP_ROOT%\third_party"

rem --- Visual Studio -------------------------------------------------------
rem vswhere itself is at a fixed location; VSROOT is not. Note the goto rather
rem than an if() block: %ProgramFiles(x86)% contains a bracket, which would
rem close a parenthesised block early.
if defined VSROOT goto :have_vs
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VSROOT=%%i"
:have_vs
if not defined VSROOT echo [env] No Visual Studio with the C++ tools found. Set VSROOT.& exit /b 1

rem Visual Studio ships its own CMake and Ninja, so neither needs installing.
set "CMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

rem --- vendored SDKs -------------------------------------------------------
if not defined SDL3_DIR    for /d %%d in ("%TP%\SDL3-*")        do set "SDL3_DIR=%%d"
if not defined OPENAL_DIR  for /d %%d in ("%TP%\openal-soft-*") do set "OPENAL_DIR=%%d"
if not defined FFMPEG_ROOT for /d %%d in ("%TP%\ffmpeg-*")      do set "FFMPEG_ROOT=%%d"

if not defined SDL3_DIR   echo [env] SDL3 not found in %TP%. Set SDL3_DIR.& exit /b 1
if not defined OPENAL_DIR echo [env] OpenAL Soft not found in %TP%. Set OPENAL_DIR.& exit /b 1

set "SDL3_INC=%SDL3_DIR%\include"
set "SDL3_LIB=%SDL3_DIR%\lib\x64\SDL3.lib"
set "OAL_INC=%OPENAL_DIR%\include\AL"
set "OAL_LIB=%OPENAL_DIR%\libs\Win64\OpenAL32.lib"

rem FFmpeg is optional: without it the game builds with no music or cutscenes.
if not defined FFMPEG_ROOT echo [env] FFmpeg not found -- building without music or cutscenes.

if /i "%~1"=="novc" goto :eof
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
