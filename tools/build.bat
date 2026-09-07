@echo off
rem AVP Access -- build, configuring first if needed, then stage runtime DLLs.
call "%~dp0env.bat" || exit /b 1

if not exist "%BUILD%\build.ninja" call "%~dp0configure.bat" || exit /b 1

"%CMAKE%" --build "%BUILD%" || exit /b 1

call "%~dp0stagedlls.bat"
