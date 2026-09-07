@echo off
setlocal
rem Tests actual optional diagnostic with mocked input/time/audio. Opens no window.
rem Usage: run_listening_tests.bat [path\to\acc_tracker_test.c]
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
set "TEST_PROJECT=%SRC%"
set "TEST_SOURCE=%SRC%\src\access\acc_tracker_test.c"
if not "%~1"=="" set "TEST_SOURCE=%~f1"
if not exist "%TEST_SOURCE%" exit /b 2
for %%s in ("%TEST_SOURCE%") do set "TEST_SOURCE_DIR=%%~dps"
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%TEST_SOURCE_DIR%." /I"%SDL3_INC%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"test_tracker_listening.exe" "test_tracker_listening.c" "%TEST_SOURCE%"
if errorlevel 1 goto :failed
.\test_tracker_listening.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
:failed
popd
exit /b 2
