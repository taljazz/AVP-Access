@echo off
setlocal
call "%~dp0..\..\tools\env.bat"
if errorlevel 1 exit /b 2
pushd "%~dp0"
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%SRC%\src" /I"%SRC%\src\include" /I"%SRC%\src\win95" /I"%SRC%\src\avp" /I"%SRC%\src\avp\win95" /I"%SRC%\src\avp\win95\frontend" /I"%SRC%\src\avp\support" /I"%SRC%\src\access" /Fe"messagehistory_tests.exe" "messagehistory_test.c"
if errorlevel 1 goto :failed
.\messagehistory_tests.exe
set "RESULT=%ERRORLEVEL%"
del /q messagehistory_tests.exe messagehistory_test.obj 2>nul
popd
exit /b %RESULT%
:failed
popd
exit /b 2
