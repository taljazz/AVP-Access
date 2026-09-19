@echo off
setlocal
rem Headless tests for the real spoken-menu implementation.
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
if not defined SRC exit /b 2
if not defined SDL3_INC exit /b 2
where cl.exe >nul 2>&1
if errorlevel 1 exit /b 2
set "TEST_PROJECT=%SRC%"
pushd "%~dp0"
if errorlevel 1 exit /b 2
cl.exe /nologo /TC /MD /Od /W3 /DWIN32 /D_WINDOWS /I"%SDL3_INC%" /I"%TEST_PROJECT%\src" /I"%TEST_PROJECT%\src\include" /I"%TEST_PROJECT%\src\win95" /I"%TEST_PROJECT%\src\avp" /I"%TEST_PROJECT%\src\avp\win95" /I"%TEST_PROJECT%\src\avp\win95\frontend" /I"%TEST_PROJECT%\src\avp\support" /I"%TEST_PROJECT%\src\access" /Fe"menu_tests.exe" "menu_tests.c" "%TEST_PROJECT%\src\access\acc_menu.c"
if errorlevel 1 goto :compile_failed
set "TEST_RESULT=0"
for %%t in (graphic_fallback capture_end clear_transition title_once slider profile briefing) do call :run_case %%t
popd
exit /b %TEST_RESULT%
:run_case
".\menu_tests.exe" %1
if errorlevel 1 set "TEST_RESULT=1"
exit /b 0
:compile_failed
popd
exit /b 2
