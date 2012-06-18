# Microsoft Developer Studio Project File - Name="ctaocrypt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=ctaocrypt - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ctaocrypt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ctaocrypt.mak" CFG="ctaocrypt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ctaocrypt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "ctaocrypt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ctaocrypt - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "ctaocrypt - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "ctaocrypt - Win32 Release"
# Name "ctaocrypt - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\aes.c
# End Source File
# Begin Source File

SOURCE=.\src\arc4.c
# End Source File
# Begin Source File

SOURCE=.\src\asn.c
# End Source File
# Begin Source File

SOURCE=.\src\coding.c
# End Source File
# Begin Source File

SOURCE=.\src\des3.c
# End Source File
# Begin Source File

SOURCE=.\src\dh.c
# End Source File
# Begin Source File

SOURCE=.\src\dsa.c
# End Source File
# Begin Source File

SOURCE=.\src\hc128.c
# End Source File
# Begin Source File

SOURCE=.\src\hmac.c
# End Source File
# Begin Source File

SOURCE=.\src\integer.c
# End Source File
# Begin Source File

SOURCE=.\src\md4.c
# End Source File
# Begin Source File

SOURCE=.\src\md5.c
# End Source File
# Begin Source File

SOURCE=.\src\misc.c
# End Source File
# Begin Source File

SOURCE=.\src\rabbit.c
# End Source File
# Begin Source File

SOURCE=.\src\random.c
# End Source File
# Begin Source File

SOURCE=.\src\rsa.c
# End Source File
# Begin Source File

SOURCE=.\src\sha.c
# End Source File
# Begin Source File

SOURCE=.\src\sha256.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\include\aes.h
# End Source File
# Begin Source File

SOURCE=.\include\arc4.h
# End Source File
# Begin Source File

SOURCE=.\include\asn.h
# End Source File
# Begin Source File

SOURCE=.\include\coding.h
# End Source File
# Begin Source File

SOURCE=.\include\des3.h
# End Source File
# Begin Source File

SOURCE=.\include\dh.h
# End Source File
# Begin Source File

SOURCE=.\include\dsa.h
# End Source File
# Begin Source File

SOURCE=.\include\error.h
# End Source File
# Begin Source File

SOURCE=.\include\hc128.h
# End Source File
# Begin Source File

SOURCE=.\include\hmac.h
# End Source File
# Begin Source File

SOURCE=.\include\integer.h
# End Source File
# Begin Source File

SOURCE=.\include\md4.h
# End Source File
# Begin Source File

SOURCE=.\include\md5.h
# End Source File
# Begin Source File

SOURCE=.\include\misc.h
# End Source File
# Begin Source File

SOURCE=.\include\rabbit.h
# End Source File
# Begin Source File

SOURCE=.\include\random.h
# End Source File
# Begin Source File

SOURCE=.\include\rsa.h
# End Source File
# Begin Source File

SOURCE=.\include\sha.h
# End Source File
# Begin Source File

SOURCE=.\include\sha256.h
# End Source File
# Begin Source File

SOURCE=.\include\types.h
# End Source File
# End Group
# End Target
# End Project
