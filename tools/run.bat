@echo off
setlocal
rem AVP Access -- run the game against the staged data directory.
rem Any arguments are passed through, e.g. run.bat -w --padtrace
call "%~dp0env.bat" novc >nul || exit /b 1

if not exist "%BUILD%\avp.exe" (
    echo [run] No build yet -- run tools\build.bat first.
    exit /b 1
)

set "AVP_DATA=%GAMEDATA%"
rem Without HOME the engine stores profiles in .avp beneath the working folder.
rem Use the project root for Explorer, terminal and fullscreen launches alike.
pushd "%AVP_ROOT%" || exit /b 1
"%BUILD%\avp.exe" %*
set "AVP_EXITCODE=%ERRORLEVEL%"
popd
echo EXITCODE=%AVP_EXITCODE%
exit /b %AVP_EXITCODE%
