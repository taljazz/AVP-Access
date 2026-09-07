@echo off
rem AVP Access -- copy the runtime DLLs next to the executable.
rem
rem Worth having as a script rather than a README step, because one of these is
rem non-obvious: OpenAL Soft ships as soft_oal.dll and must be *renamed* to
rem OpenAL32.dll to be picked up. Nothing is deleted, so a DLL you placed by
rem hand (Tolk, typically) is left alone.
call "%~dp0env.bat" novc >nul || exit /b 1

if not exist "%BUILD%" mkdir "%BUILD%"

copy /y "%SDL3_DIR%\lib\x64\SDL3.dll" "%BUILD%\" >nul && echo [dlls] SDL3.dll

rem The rename is the point: the loader looks for OpenAL32.dll.
copy /y "%OPENAL_DIR%\bin\Win64\soft_oal.dll" "%BUILD%\OpenAL32.dll" >nul && echo [dlls] OpenAL32.dll (from soft_oal.dll)

if not defined FFMPEG_ROOT goto :tolk
for %%f in (avcodec avformat avutil swresample swscale) do (
  for %%d in ("%FFMPEG_ROOT%\bin\%%f-*.dll") do copy /y "%%d" "%BUILD%\" >nul && echo [dlls] %%~nxd
)

:tolk
rem Tolk is not vendored -- it is a separate project with its own licence. Point
rem TOLK_DIR at a folder holding Tolk.dll and its screen-reader clients, or drop
rem them into the build folder by hand. Without it the game runs silently.
if not defined TOLK_DIR if exist "%TP%\tolk" set "TOLK_DIR=%TP%\tolk"
if not defined TOLK_DIR goto :no_tolk

for %%f in (Tolk.dll nvdaControllerClient64.dll SAAPI64.dll) do (
  if exist "%TOLK_DIR%\%%f" copy /y "%TOLK_DIR%\%%f" "%BUILD%\" >nul && echo [dlls] %%f
)
goto :eof

:no_tolk
if exist "%BUILD%\Tolk.dll" echo [dlls] Tolk.dll already present, left alone& goto :eof
echo [dlls] Tolk.dll not found -- speech will be silent. Set TOLK_DIR.
