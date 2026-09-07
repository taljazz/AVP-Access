@echo off
setlocal
rem Usage: run_tests.bat [source root]. Set AVP_TEST_PROJECT when testing staged files.
if not defined AVP_TEST_PROJECT set "AVP_TEST_PROJECT=%~dp0..\.."
call "%AVP_TEST_PROJECT%\tools\env.bat"
if errorlevel 1 exit /b 2
set "TEST_SOURCE_ROOT=%SRC%"
if not "%~1"=="" set "TEST_SOURCE_ROOT=%~f1"
rem PowerShell rather than Python: every other suite here needs only MSVC, and
rem Windows always ships PowerShell. Set AVP_TEST_EXTRACT to override.
if not defined AVP_TEST_EXTRACT set "AVP_TEST_EXTRACT=powershell -NoProfile -ExecutionPolicy Bypass -File extract_menu.ps1"
pushd "%~dp0"
if errorlevel 1 exit /b 2
%AVP_TEST_EXTRACT% "%TEST_SOURCE_ROOT%" menu_source.generated.h
if errorlevel 1 goto :failed
cl.exe /nologo /TC /MD /W3 /Fe"test_menu_music.exe" test_menu_music.c
if errorlevel 1 goto :failed
.\test_menu_music.exe
if errorlevel 1 goto :failed
cl.exe /nologo /TC /MD /W3 /I"%SDL3_INC%" /I"%TEST_SOURCE_ROOT%\src\access" /Fe"test_media_fallback.exe" test_media_fallback.c "%TEST_SOURCE_ROOT%\src\access\acc_media.c"
if errorlevel 1 goto :failed
.\test_media_fallback.exe
if errorlevel 1 goto :failed
echo Media fallback passed.
popd
exit /b 0
:failed
popd
exit /b 2
