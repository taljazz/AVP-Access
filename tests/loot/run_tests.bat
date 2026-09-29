@echo off
setlocal
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
pushd "%~dp0"
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"loot_tests.exe" "loot_tests.c" "%TEST_PROJECT%\src\access\acc_loot.c"
if errorlevel 1 goto :compile_failed
.\loot_tests.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
:compile_failed
popd
exit /b 2
