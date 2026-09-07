@echo off
setlocal
rem Usage: run_input_tests.bat [path\to\usr_io.c]
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
set "TEST_INPUT_SOURCE=%SRC%\src\avp\win95\usr_io.c"
if not "%~1"=="" set "TEST_INPUT_SOURCE=%~f1"
if not exist "%TEST_INPUT_SOURCE%" exit /b 2
pushd "%~dp0"
>input_source.generated.h echo #include "%TEST_INPUT_SOURCE%"
cl.exe /nologo /TC /MD /Od /Gy /Gw /DNDEBUG /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%SRC%\src" /I"%SRC%\src\include" /I"%SRC%\src\win95" /I"%SRC%\src\avp" /I"%SRC%\src\avp\win95" /I"%SRC%\src\avp\win95\frontend" /I"%SRC%\src\avp\win95\gadgets" /I"%SRC%\src\avp\support" /I"%SRC%\src\access" /Fe"test_gameplay_input.exe" "test_gameplay_input.c" /link /OPT:REF
if errorlevel 1 goto :compile_failed
rem See note in tests/controller/run_tests.bat about NoDefaultCurrentDirectoryInExePath.
.\test_gameplay_input.exe
set "TEST_INPUT_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_INPUT_RESULT%
:compile_failed
popd
exit /b 2
