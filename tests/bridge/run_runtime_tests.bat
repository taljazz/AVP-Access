@echo off
setlocal
call "%~dp0..\..\tools\env.bat"
if errorlevel 1 exit /b 2
set "PATH=%SDL3_DIR%\lib\x64;%PATH%"
pushd "%~dp0"
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%SRC%\src" /I"%SRC%\src\include" /I"%SRC%\src\win95" /I"%SRC%\src\avp" /I"%SRC%\src\avp\win95" /I"%SRC%\src\avp\win95\frontend" /I"%SRC%\src\avp\support" /I"%SRC%\src\access" /Fe"runtime_tests.exe" "runtime_tests.c" "%SRC%\src\access\acc_bridge_core.c" /link "%SDL3_LIB%"
if errorlevel 1 goto :failed
set "TEST_RESULT=0"
for %%t in (reply_retry invalid locked_command rename_retry startup_log startup_ready held_close) do call :run_case %%t
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0client_tests.ps1" -RuntimeExe "%~dp0runtime_tests.exe"
if errorlevel 1 set "TEST_RESULT=1"
popd
exit /b %TEST_RESULT%
:run_case
.\runtime_tests.exe %1 "%TEMP%\avp-bridge-%RANDOM%-%RANDOM%"
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:failed
popd
exit /b 2
