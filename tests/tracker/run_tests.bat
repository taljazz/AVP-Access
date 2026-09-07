@echo off
setlocal
rem Standalone tracker tests; never opens the game, audio, or speech backends.
rem Usage: run_tests.bat [path\to\acc_tracker.c]
rem AVP_TEST_PROJECT optionally points to the project providing tools/env.bat.
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
set "TEST_SDL=%SDL3_INC%"
set "TEST_SOURCE=%TEST_PROJECT%\src\access\acc_tracker.c"
if not "%~1"=="" set "TEST_SOURCE=%~1"
if not exist "%TEST_SOURCE%" exit /b 2
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE=%%~fs"
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE_DIR=%%~dps"
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%TEST_SOURCE_DIR%." /I"%TEST_SDL%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"tracker_tests.exe" "tracker_tests.c" "%TEST_SOURCE%"
if errorlevel 1 goto :compile_failed
set "TEST_RESULT=0"
for %%t in (availability snapshot bearings rotations selection nearby announcement spatial_sound volumes sound_fallback buffers) do call :run_case %%t
popd
exit /b %TEST_RESULT%
:run_case
".\tracker_tests.exe" %1
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:compile_failed
popd
exit /b 2
