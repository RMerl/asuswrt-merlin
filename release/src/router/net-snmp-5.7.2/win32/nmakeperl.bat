@echo off
REM
REM build Net-SNMP Perl module using nmake
REM

REM INSTALL_BASE must point to the directory ABOVE the library files.
REM Generally follows what is the install-net-snmp.bat setting.

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

echo Remember to run this script from the base of the source directory.

cd perl

REM choose the installed location...
perl Makefile.PL CAPI=TRUE -NET-SNMP-PATH=%INSTALL_BASE%

REM Or, if the libraries have been built, look back in the build directory.
REM perl Makefile.PL CAPI=TRUE -NET-SNMP-IN-SOURCE=TRUE

echo Make the Perl SNMP modules.
nmake /nologo > nmake.out
echo If errors are seen stop here and review perl\nmake.out.
pause

echo Test the Perl SNMP modules.
nmake /nologo test > nmaketest.out 2>&1
echo If no errors are seen, review test results in perl\nmaketest.out.

cd ..

:end

