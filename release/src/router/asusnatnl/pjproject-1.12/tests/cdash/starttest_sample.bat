@echo off

rem ***
rem ************** VS 2005 **************
rem ***
set OLD_PATH=%PATH%
set OLD_INCLUDE=%INCLUDE%
set OLD_LIB=%LIB%
set OLD_LIBPATH=%LIBPATH%

call "C:\Program Files\Microsoft Visual Studio 8\VC\bin\vcvars32.bat" x86
python main.py cfg_msvc -t "Debug|Win32"
python main.py cfg_msvc -t "Release|Win32"

set PATH=%OLD_PATH%
set INCLUDE=%OLD_INCLUDE%
set LIB=%OLD_LIB%
set LIBPATH=%OLD_LIBPATH%


rem ***
rem ************** S60 3rd FP1 **************
rem ***
set EPOCROOT=\symbian\9.2\S60_3rd_FP1\
devices -setdefault @S60_3rd_FP1:com.nokia.s60
python main.py cfg_symbian -t "winscw udeb"
python main.py cfg_symbian -t "gcce udeb"
python main.py cfg_symbian -t "gcce urel"


rem ***
rem ************** Mingw **************
rem ***
set MSYSTEM=MINGW32
set DISPLAY=
C:\msys\1.0\bin\sh -c "python main.py cfg_gnu"


rem ***
rem ************** Linux **************
rem ***
set PATH=%PATH%;c:\msys\1.0\bin
set HOME=C:\msys\1.0\home\Administrator
C:\mingw\bin\ssh test@192.168.0.12 "cd project/pjproject/tests/cdash && python main.py cfg_gnu"
