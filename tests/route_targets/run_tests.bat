@echo off
setlocal
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SRC%\src" /I"%SRC%\src\include" /I"%SRC%\src\win95" /I"%SRC%\src\avp" /I"%SRC%\src\avp\win95" /I"%SRC%\src\avp\win95\frontend" /I"%SRC%\src\avp\support" /I"%SRC%\src\access" /Fe"route_targets_tests.exe" "route_targets_tests.c" "%SRC%\src\access\acc_route_targets.c"
if errorlevel 1 goto :failed
".\route_targets_tests.exe"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
:failed
popd
exit /b 2
