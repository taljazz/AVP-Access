@echo off
setlocal EnableDelayedExpansion
rem Run every regression runner under tests\ and report each exit code.
rem Usage: run_all.bat [logdir]
rem Each runner's full output goes to <logdir>\<suite>_<runner>.log (default:
rem %TEMP%\avp-access-tests). Exits 1 if any runner fails, 0 if all pass.
rem Runners are called by full path: this machine sets
rem NoDefaultCurrentDirectoryInExePath, so a bare name would not execute.
set "LOGDIR=%~1"
if "%LOGDIR%"=="" set "LOGDIR=%TEMP%\avp-access-tests"
if not exist "%LOGDIR%" mkdir "%LOGDIR%" || exit /b 2
set /a PASSED=0, FAILED=0
for /d %%d in ("%~dp0*") do (
  for %%b in ("%%d\*.bat") do (
    call "%%~fb" > "%LOGDIR%\%%~nxd_%%~nb.log" 2>&1 < nul
    if errorlevel 1 (
      echo FAIL %%~nxd\%%~nxb  [exit !ERRORLEVEL!, see %LOGDIR%\%%~nxd_%%~nb.log]
      set /a FAILED+=1
    ) else (
      echo pass %%~nxd\%%~nxb
      set /a PASSED+=1
    )
  )
)
echo.
echo %PASSED% runners passed, %FAILED% failed. Logs: %LOGDIR%
if %FAILED% gtr 0 exit /b 1
exit /b 0
