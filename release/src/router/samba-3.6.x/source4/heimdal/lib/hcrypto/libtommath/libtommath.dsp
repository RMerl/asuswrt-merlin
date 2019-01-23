# Microsoft Developer Studio Project File - Name="libtommath" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libtommath - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libtommath.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libtommath.mak" CFG="libtommath - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libtommath - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libtommath - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "libtommath"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libtommath - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\tommath.lib"

!ELSEIF  "$(CFG)" == "libtommath - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\tommath.lib"

!ENDIF 

# Begin Target

# Name "libtommath - Win32 Release"
# Name "libtommath - Win32 Debug"
# Begin Source File

SOURCE=.\bn_error.c
# End Source File
# Begin Source File

SOURCE=.\bn_fast_mp_invmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_fast_mp_montgomery_reduce.c
# End Source File
# Begin Source File

SOURCE=.\bn_fast_s_mp_mul_digs.c
# End Source File
# Begin Source File

SOURCE=.\bn_fast_s_mp_mul_high_digs.c
# End Source File
# Begin Source File

SOURCE=.\bn_fast_s_mp_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_2expt.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_abs.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_add.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_add_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_addmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_and.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_clamp.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_clear.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_clear_multi.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_cmp.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_cmp_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_cmp_mag.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_cnt_lsb.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_copy.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_count_bits.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_div.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_div_2.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_div_2d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_div_3.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_div_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_dr_is_modulus.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_dr_reduce.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_dr_setup.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_exch.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_expt_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_exptmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_exptmod_fast.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_exteuclid.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_fread.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_fwrite.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_gcd.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_get_int.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_grow.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init_copy.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init_multi.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init_set.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init_set_int.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_init_size.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_invmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_invmod_slow.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_is_square.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_jacobi.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_karatsuba_mul.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_karatsuba_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_lcm.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_lshd.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mod_2d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mod_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_montgomery_calc_normalization.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_montgomery_reduce.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_montgomery_setup.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mul.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mul_2.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mul_2d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mul_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_mulmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_n_root.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_neg.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_or.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_fermat.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_is_divisible.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_is_prime.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_miller_rabin.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_next_prime.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_rabin_miller_trials.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_prime_random_ex.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_radix_size.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_radix_smap.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_rand.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_read_radix.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_read_signed_bin.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_read_unsigned_bin.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_2k.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_2k_l.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_2k_setup.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_2k_setup_l.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_is_2k.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_is_2k_l.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_reduce_setup.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_rshd.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_set.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_set_int.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_shrink.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_signed_bin_size.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_sqrmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_sqrt.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_sub.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_sub_d.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_submod.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_to_signed_bin.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_to_signed_bin_n.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_to_unsigned_bin.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_to_unsigned_bin_n.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_toom_mul.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_toom_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_toradix.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_toradix_n.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_unsigned_bin_size.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_xor.c
# End Source File
# Begin Source File

SOURCE=.\bn_mp_zero.c
# End Source File
# Begin Source File

SOURCE=.\bn_prime_tab.c
# End Source File
# Begin Source File

SOURCE=.\bn_reverse.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_add.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_exptmod.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_mul_digs.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_mul_high_digs.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_sqr.c
# End Source File
# Begin Source File

SOURCE=.\bn_s_mp_sub.c
# End Source File
# Begin Source File

SOURCE=.\bncore.c
# End Source File
# Begin Source File

SOURCE=.\tommath.h
# End Source File
# Begin Source File

SOURCE=.\tommath_class.h
# End Source File
# Begin Source File

SOURCE=.\tommath_superclass.h
# End Source File
# End Target
# End Project
