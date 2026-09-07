@echo off
setlocal
rem Place in tests/controller/. Optional AVP_TEST_PROJECT overrides project root.
rem Usage: run_tests.bat [fixed^|baseline] [path\to\acc_pad.c]
rem Baseline source is supplied externally; never commit the snapshot.
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
set "TEST_SDL=%SDL3_INC%"
set "TEST_MODE=fixed"
set "TEST_DEFINES=/DTEST_FIXED"
set "TEST_SOURCE=%TEST_PROJECT%\src\access\acc_pad.c"
if /i "%~1"=="baseline" set "TEST_MODE=baseline"
if /i "%TEST_MODE%"=="baseline" set "TEST_DEFINES="
if /i "%TEST_MODE%"=="baseline" set "TEST_SOURCE=%~dp0acc_pad.baseline.c"
if not "%~2"=="" set "TEST_SOURCE=%~2"
if not exist "%TEST_SOURCE%" exit /b 2
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE=%%~fs"
pushd "%~dp0"
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS %TEST_DEFINES% /I"%TEST_SDL%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"controller_tests_%TEST_MODE%.exe" "controller_tests.c" "%TEST_SOURCE%"
if errorlevel 1 goto :compile_failed
set "TEST_RESULT=0"
for %%t in (dpad keyboard_idle any_key back_hold shared_key keyboard_release combined_back bindings reconnect gameplay_axes gameplay_triggers gameplay_buttons gameplay_transition) do call :run_case %%t
popd
exit /b %TEST_RESULT%
:run_case
rem NoDefaultCurrentDirectoryInExePath=1 is common on hardened Windows setups;
rem without the explicit . cmd refuses to run an exe from the current directory.
".\controller_tests_%TEST_MODE%.exe" %1
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:compile_failed
popd
exit /b 2
