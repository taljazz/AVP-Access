@echo off
rem AVP Access -- generate the build system.
call "%~dp0env.bat" || exit /b 1

"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DSDL3_INCLUDE="%SDL3_INC%" ^
  -DSDL3_LIBRARY="%SDL3_LIB%" ^
  -DOPENAL_INCLUDE_DIR="%OAL_INC%" ^
  -DOPENAL_LIBRARY="%OAL_LIB%" ^
  -DFFMPEG_ROOT="%FFMPEG_ROOT%"
