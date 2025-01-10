@ECHO off
SetLocal EnableDelayedExpansion
IF NOT EXIST bin mkdir bin
IF NOT EXIST bin\int mkdir bin\int

SET name=compile

REM call vcvarsall.bat x64
SET cc=clang

REM ------------------
REM    Main Project
REM ------------------

REM ==============
REM Gets list of all C files
SET c_filenames=
FOR %%f in (source\base\*.c) do SET c_filenames=!c_filenames! %%f
FOR %%f in (source\os\*.c) do SET c_filenames=!c_filenames! %%f
FOR %%f in (source\*.c) do SET c_filenames=!c_filenames! %%f
REM ==============

REM ==============
if %cc% == cl.exe (
  SET compiler_flags=/Zc:preprocessor /wd4090 /wd5105 /nologo
  SET include_flags=/I.\source\ /I.\third_party\include\ /I.\third_party\source\
  SET linker_flags=/link /DEBUG:FULL Winmm.lib Shell32.lib Userenv.lib
  SET output=/Fe.\bin\%name% /Fo.\bin\int\
  SET defines=/D_DEBUG /D_CRT_SECURE_NO_WARNINGS
)

if %cc% == clang (
  SET compiler_flags=-Wall -Wvarargs -Werror -Wno-unused-function -Wno-format-security -Wno-incompatible-pointer-types-discards-qualifiers -Wno-unused-but-set-variable -Wno-int-to-void-pointer-cast
  SET include_flags=-Isource -Ithird_party/include -Ithird_party/source
  SET linker_flags=-g -lwinmm -lshell32 -luserenv
  SET output=-obin/%name%.exe
  SET defines=-D_DEBUG -D_CRT_SECURE_NO_WARNINGS
)
REM ==============

REM SET compiler_flags=!compiler_flags! -fsanitize=address

ECHO Building %name%.exe...
%cc% %compiler_flags% %c_filenames% %defines% %include_flags% %output% %linker_flags%
