@echo off
set ComSpec=C:\Windows\system32\cmd.exe
set DevEnvDir=C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE\
set INCLUDE=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\INCLUDE;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\ATLMFC\INCLUDE;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\include;
set LIB=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\ATLMFC\LIB;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\lib;
set LIBPATH=C:\Windows\Microsoft.NET\Framework\v4.0.30319;C:\Windows\Microsoft.NET\Framework\v3.5;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\LIB;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\ATLMFC\LIB;
set Path=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VSTSDB\Deploy;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE\;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\BIN;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\Tools;C:\Windows\Microsoft.NET\Framework\v4.0.30319;C:\Windows\Microsoft.NET\Framework\v3.5;C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\VCPackages;C:\Program Files (x86)\HTML Help Workshop;C:\Program Files (x86)\HTML Help Workshop;C:\Program Files (x86)\Microsoft Visual Studio 10.0\Team Tools\Performance Tools;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin\NETFX 4.0 Tools;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\idmu\common
set PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC
set PSModulePath=C:\Windows\system32\WindowsPowerShell\v1.0\Modules\
set VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\
set VisualStudioDir=C:\Users\Administrator\Documents\Visual Studio 2010
set VS100COMNTOOLS=C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\Tools\
set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio 10.0\
set WindowsSdkDir=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\
cls
cd C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC
C:
call vcvarsall.bat help

echo #
echo # You may want to enter 'C:' followed by 'vcvarsall.bat amd64' and 'Y:'
echo #
Y:
@cmd.exe
