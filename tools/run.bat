@echo off
rem AVP Access -- run the game against the staged data directory.
rem Any arguments are passed through, e.g. run.bat -w --padtrace
call "%~dp0env.bat" novc >nul || exit /b 1

if not exist "%BUILD%\avp.exe" echo [run] No build yet -- run tools\build.bat first.& exit /b 1

set "AVP_DATA=%GAMEDATA%"
"%BUILD%\avp.exe" %*
echo EXITCODE=%ERRORLEVEL%
