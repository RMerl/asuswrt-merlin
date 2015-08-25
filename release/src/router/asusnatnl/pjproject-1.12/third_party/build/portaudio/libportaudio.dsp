# Microsoft Developer Studio Project File - Name="libportaudio" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libportaudio - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libportaudio.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libportaudio.mak" CFG="libportaudio - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libportaudio - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libportaudio - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libportaudio - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "output\pa-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "output\pa-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "output\pa-i386-win32-vc6-release"
# PROP Intermediate_Dir "output\pa-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\portaudio\src\common" /I "..\..\portaudio\include" /I "..\..\portaudio\src\os\win" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PA_ENABLE_DEBUG_OUTPUT" /D "_CRT_SECURE_NO_DEPRECATE" /D "PA_NO_ASIO" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libportaudio-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "libportaudio - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "output\pa-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "output\pa-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "output\pa-i386-win32-vc6-debug"
# PROP Intermediate_Dir "output\pa-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\portaudio\src\common" /I "..\..\portaudio\include" /I "..\..\portaudio\src\os\win" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "PA_ENABLE_DEBUG_OUTPUT" /D "_CRT_SECURE_NO_DEPRECATE" /D "PA_NO_ASIO" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libportaudio-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "libportaudio - Win32 Release"
# Name "libportaudio - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_allocation.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_converters.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_cpuload.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_debugprint.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_dither.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_front.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_process.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_skeleton.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\common\pa_stream.c
# End Source File
# End Group
# Begin Group "hostapi"

# PROP Default_Filter ""
# Begin Group "dsound"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\portaudio\src\hostapi\dsound\pa_win_ds.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\hostapi\dsound\pa_win_ds_dynlink.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\hostapi\dsound\pa_win_ds_dynlink.h
# End Source File
# End Group
# Begin Group "wmme"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\portaudio\src\hostapi\wmme\pa_win_wmme.c
# End Source File
# End Group
# End Group
# Begin Group "os"

# PROP Default_Filter ""
# Begin Group "win"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\portaudio\src\os\win\pa_win_hostapis.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\os\win\pa_win_util.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\os\win\pa_win_waveformat.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\os\win\pa_x86_plain_converters.c
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\src\os\win\pa_x86_plain_converters.h
# End Source File
# End Group
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\portaudio\include\pa_win_wmme.h
# End Source File
# Begin Source File

SOURCE=..\..\portaudio\include\portaudio.h
# End Source File
# End Group
# End Target
# End Project
