# Microsoft Developer Studio Project File - Name="libsrtp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libsrtp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libsrtp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libsrtp.mak" CFG="libsrtp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libsrtp - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libsrtp - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libsrtp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "output\libsrtp-i386-win32-vc6-release"
# PROP BASE Intermediate_Dir "output\libsrtp-i386-win32-vc6-release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "output\libsrtp-i386-win32-vc6-release"
# PROP Intermediate_Dir "output\libsrtp-i386-win32-vc6-release"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I "../../srtp/include" /I "../../srtp/crypto/include" /I "../../../pjlib/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libsrtp-i386-win32-vc6-release.lib"

!ELSEIF  "$(CFG)" == "libsrtp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "output\libsrtp-i386-win32-vc6-debug"
# PROP BASE Intermediate_Dir "output\libsrtp-i386-win32-vc6-debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "output\libsrtp-i386-win32-vc6-debug"
# PROP Intermediate_Dir "output\libsrtp-i386-win32-vc6-debug"
# PROP Target_Dir ""
F90=df.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /I "../../srtp/include" /I "../../srtp/crypto/include" /I "../../../pjlib/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib\libsrtp-i386-win32-vc6-debug.lib"

!ENDIF 

# Begin Target

# Name "libsrtp - Win32 Release"
# Name "libsrtp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\srtp\srtp\srtp.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\pjlib\srtp_err.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\srtp\include\rtp.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\include\srtp.h
# End Source File
# Begin Source File

SOURCE=.\srtp_config.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\include\ut_sim.h
# End Source File
# End Group
# Begin Group "crypto"

# PROP Default_Filter ""
# Begin Group "ae_xfm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\ae_xfm\xfm.c
# End Source File
# End Group
# Begin Group "cipher"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\cipher\aes.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\cipher\aes_cbc.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\cipher\aes_icm.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\cipher\cipher.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\cipher\null_cipher.c
# End Source File
# End Group
# Begin Group "hash"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\hash\auth.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\hash\hmac.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\hash\null_auth.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\hash\sha1.c
# End Source File
# End Group
# Begin Group "include"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\include\aes.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\aes_cbc.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\aes_icm.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\alloc.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\auth.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\cipher.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\crypto.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\crypto_kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\crypto_math.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\crypto_types.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\cryptoalg.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\datatypes.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\err.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\gf2_8.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\hmac.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\integers.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\kernel_compat.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\key.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\null_auth.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\null_cipher.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\prng.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\rand_source.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\rdb.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\rdbx.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\sha1.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\stat.h
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\include\xfm.h
# End Source File
# End Group
# Begin Group "kernel"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\kernel\alloc.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\kernel\crypto_kernel.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\kernel\key.c
# End Source File
# End Group
# Begin Group "math"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\math\datatypes.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\math\gf2_8.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\math\stat.c
# End Source File
# End Group
# Begin Group "replay"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\replay\rdb.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\replay\rdbx.c
# End Source File
# End Group
# Begin Group "rng"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\srtp\crypto\rng\ctr_prng.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\rng\prng.c
# End Source File
# Begin Source File

SOURCE=..\..\srtp\crypto\rng\rand_source.c
# End Source File
# End Group
# End Group
# End Target
# End Project
