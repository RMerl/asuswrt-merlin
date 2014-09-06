@echo off
REM Install the Net-SNMP Project files on the local machine.
REM
REM Run this script from the base Net-SNMP source directory
REM after the successful build has completed.

REM  **** IMPORTANT NOTE ****
REM The value for INSTALL_BASE in win32\net-snmp\net-snmp-config.h, and
REM The value for INSTALL_BASE below **MUST** match

REM Use backslashes to delimit sub-directories in path.
set INSTALL_BASE="c:\usr"

if "%1" == "-?" goto help
if "%1" == "/?" goto help
if "%1" == "-h" goto help
if "%1" == "/h" goto help
if "%1" == "-help" goto help
if "%1" == "/help" goto help
goto start

:help
echo .
echo This script will compile the Net-SNMP Perl modules.  Net-SNMP must 
echo already be installed.
echo .
echo The current install base is %INSTALL_BASE%.  
echo This must match the directory that Net-SNMP has been installed in.
echo .
echo To change the installation directory, modify the INSTALL_BASE variable
echo inside this script.
echo .
echo Run this script from the base of the source directory, NOT the win32 
echo directory.
echo .
goto end

:start

set progVer=release
if "%1" NEQ "-debug" goto nodebug
set progVer=debug
shift

:nodebug

REM make sure script runs from above the win32 directory
cd win32 > NUL: 2>&1
cd .. > NUL: 2>&1

echo Installing %progVer% versions

echo Remember to run this script from the base of the source directory.

echo Creating %INSTALL_BASE% sub-directories

mkdir %INSTALL_BASE% > NUL:
mkdir %INSTALL_BASE%\bin > NUL:
mkdir %INSTALL_BASE%\etc > NUL:
mkdir %INSTALL_BASE%\etc\snmp > NUL:
mkdir %INSTALL_BASE%\share > NUL:
mkdir %INSTALL_BASE%\share\snmp > NUL:
mkdir %INSTALL_BASE%\share\snmp\mibs > NUL:
mkdir %INSTALL_BASE%\share\snmp\snmpconf-data > NUL:
mkdir %INSTALL_BASE%\share\snmp\snmpconf-data\snmp-data > NUL:
mkdir %INSTALL_BASE%\share\snmp\snmpconf-data\snmpd-data > NUL:
mkdir %INSTALL_BASE%\share\snmp\snmpconf-data\snmptrapd-data > NUL:
mkdir %INSTALL_BASE%\share\snmp\mib2c-data > NUL:
mkdir %INSTALL_BASE%\snmp > NUL:
mkdir %INSTALL_BASE%\snmp\persist > NUL:
mkdir %INSTALL_BASE%\temp > NUL:
mkdir %INSTALL_BASE%\include > NUL:
mkdir %INSTALL_BASE%\include\net-snmp > NUL:
mkdir %INSTALL_BASE%\include\ucd-snmp > NUL:
mkdir %INSTALL_BASE%\lib > NUL:

echo Copying MIB files to %INSTALL_BASE%\share\snmp\mibs
Copy mibs\*.txt %INSTALL_BASE%\share\snmp\mibs > NUL:

echo Copying compiled programs to %INSTALL_BASE%\bin
Copy win32\bin\%progVer%\*.exe %INSTALL_BASE%\bin > NUL:
Copy local\snmpconf %INSTALL_BASE%\bin > NUL:
Copy local\snmpconf.bat %INSTALL_BASE%\bin > NUL:
Copy local\mib2c %INSTALL_BASE%\bin > NUL:
Copy local\mib2c.bat %INSTALL_BASE%\bin > NUL:
Copy local\traptoemail %INSTALL_BASE%\bin > NUL:
Copy local\traptoemail.bat %INSTALL_BASE%\bin > NUL:

echo Copying snmpconf files to %INSTALL_BASE%\share\snmp\snmpconf-data\snmp-data
Copy local\snmpconf.dir\snmp-data\*.* %INSTALL_BASE%\share\snmp\snmpconf-data\snmp-data > NUL:
Copy local\snmpconf.dir\snmpd-data\*.* %INSTALL_BASE%\share\snmp\snmpconf-data\snmpd-data > NUL:
Copy local\snmpconf.dir\snmptrapd-data\*.* %INSTALL_BASE%\share\snmp\snmpconf-data\snmptrapd-data > NUL:

echo Copying mib2c config files to %INSTALL_BASE%\share\snmp
Copy local\mib2c*.conf %INSTALL_BASE%\share\snmp > NUL:
Copy local\mib2c-conf.d\*.* %INSTALL_BASE%\share\snmp\mib2c-data > NUL:

REM
REM Copy the remaining files used only to develop
REM other software that uses Net-SNMP libraries.
REM
echo Copying link libraries to %INSTALL_BASE%\lib
Copy win32\lib\%progVer%\*.*   %INSTALL_BASE%\lib > NUL:

echo Copying header files to %INSTALL_BASE%\include
xcopy /E /Y include\net-snmp\*.h %INSTALL_BASE%\include\net-snmp > NUL:
xcopy /E /Y include\ucd-snmp\*.h %INSTALL_BASE%\include\ucd-snmp > NUL:
xcopy /E /Y win32\net-snmp\*.* %INSTALL_BASE%\include\net-snmp > NUL:

REM
REM If built with OpenSSL, we need the DLL library, too.
REM
echo Copying DLL files to %INSTALL_BASE%
Copy win32\bin\%progVer%\*.dll %INSTALL_BASE%\bin > NUL:

echo Copying DLL files to %SYSTEMROOT%\System32
Copy win32\bin\%progVer%\*.dll %SYSTEMROOT%\System32 > NUL:

echo Done copying files to %INSTALL_BASE%

