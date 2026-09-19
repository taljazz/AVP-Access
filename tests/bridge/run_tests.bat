@echo off
setlocal
rem Standalone play-bridge tests; never opens the game, a window or audio.
rem The engine-free core is compiled on its own, with nothing mocked.
rem Usage: run_tests.bat [path\to\acc_bridge_core.c]
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
set "TEST_SOURCE=%TEST_PROJECT%\src\access\acc_bridge_core.c"
if not "%~1"=="" set "TEST_SOURCE=%~1"
if not exist "%TEST_SOURCE%" exit /b 2
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE=%%~fs"
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"bridge_tests.exe" "bridge_tests.c" "%TEST_SOURCE%"
if errorlevel 1 goto :compile_failed
set "TEST_RESULT=0"
if exist "bridge_small.png" del "bridge_small.png"
for %%t in (parse_basic parse_hold parse_turn parse_errors parse_whitespace keys json relative frames png_small png_large halve hold_frames hold_clock tap run turn turn_failures switch_clock inactive) do call :run_case %%t
call :png_decode
popd
exit /b %TEST_RESULT%
:run_case
rem See tests/controller/run_tests.bat: NoDefaultCurrentDirectoryInExePath.
".\bridge_tests.exe" %1
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:png_decode
rem A structurally valid PNG is not proof a real decoder accepts it, so the
rem image png_small wrote is opened with the .NET decoder Windows ships.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0decode_png.ps1" "%~dp0bridge_small.png"
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:compile_failed
popd
exit /b 2
