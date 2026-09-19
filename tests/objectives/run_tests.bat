@echo off
setlocal
rem Standalone objectives tests; never opens the game or a speech backend.
rem The objective list and string table are mocked.
rem Usage: run_tests.bat [path\to\acc_objectives.c]
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
set "TEST_SOURCE=%TEST_PROJECT%\src\access\acc_objectives.c"
if not "%~1"=="" set "TEST_SOURCE=%~1"
if not exist "%TEST_SOURCE%" exit /b 2
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE=%%~fs"
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"objectives_tests.exe" "objectives_tests.c" "%TEST_SOURCE%"
if errorlevel 1 goto :compile_failed
set "TEST_RESULT=0"
for %%t in (none states no_desc range cycles shrinking reset interrupts) do call :run_case %%t
popd
exit /b %TEST_RESULT%
:run_case
rem See tests/controller/run_tests.bat: NoDefaultCurrentDirectoryInExePath.
".\objectives_tests.exe" %1
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:compile_failed
popd
exit /b 2
