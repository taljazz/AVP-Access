@echo off
setlocal
rem Usage: run_hud_tests.bat [path\to\hud.c]
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
set "TEST_HUD_SOURCE=%SRC%\src\avp\hud.c"
if not "%~1"=="" set "TEST_HUD_SOURCE=%~f1"
if not defined TEST_ACCESS_INCLUDE set "TEST_ACCESS_INCLUDE=%SRC%\src\access"
pushd "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\extract_hud_fixture.ps1" -Source "%TEST_HUD_SOURCE%" -Output "%~dp0hud_source.generated.h"
if errorlevel 1 goto :failed
cl.exe /nologo /TC /MD /Od /DNDEBUG /DWIN32 /D_WINDOWS /I"%TEST_ACCESS_INCLUDE%" /I"%SDL3_INC%" /I"%SRC%\src" /I"%SRC%\src\include" /I"%SRC%\src\win95" /I"%SRC%\src\avp" /I"%SRC%\src\avp\win95" /I"%SRC%\src\avp\win95\frontend" /I"%SRC%\src\avp\win95\gadgets" /I"%SRC%\src\avp\support" /I"%SRC%\src\access" /Fe"test_tracker_hud.exe" "test_tracker_hud.c" "%SRC%\src\tables.c"
if errorlevel 1 goto :failed
.\test_tracker_hud.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
:failed
popd
exit /b 2
