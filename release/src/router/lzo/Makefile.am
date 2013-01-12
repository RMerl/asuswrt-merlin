## Process this file with automake to create Makefile.in
#
#  Makefile --- Makefile for building the LZO library
#
#  This file is part of the LZO data compression library.
#
#  Copyright (C) 1996-2011 Markus Franz Xaver Johannes Oberhumer
#  All Rights Reserved.
#

EXTRA_DIST =
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)
LDADD = src/liblzo2.la
lib_LTLIBRARIES =
noinst_PROGRAMS =


##/***********************************************************************
##// TOPDIR
##************************************************************************/

doc_DATA = AUTHORS COPYING NEWS THANKS doc/LZO.FAQ doc/LZO.TXT doc/LZOAPI.TXT
EXTRA_DIST += BUGS autoconf/shtool

# hooks for Automake targets
dist-hook: dist-hook-1
dist-hook-1:
	cp -pR $(srcdir)/B/ $(srcdir)/asm/ $(srcdir)/doc/ $(srcdir)/util/ "$(distdir)/"
	-rm -f $(distdir)/util/*.dat

check-local: all-am
	./lzotest/lzotest -mlzo -n2 -q $(top_srcdir)/COPYING
	@echo
	@echo "All checks passed."
	@echo

# special targets
basic-test: all
	./lzotest/lzotest -mlzo -n2 -q $(top_srcdir)/COPYING
	./tests/align
	./tests/chksum
	./examples/simple
	./minilzo/testmini

test: all
	./lzotest/lzotest -mavail -n10 -q $(top_srcdir)/COPYING
	$(SHELL) -e $(top_srcdir)/util/check.sh $(top_srcdir)
	./tests/align
	./tests/chksum
	./examples/simple
	./minilzo/testmini
	@echo
	@echo "All tests passed. Now you are ready to install LZO."
	@echo


##/***********************************************************************
##// SUBDIR include/lzo
##************************************************************************/

pkginclude_HEADERS = \
    include/lzo/lzo1.h include/lzo/lzo1a.h include/lzo/lzo1b.h \
    include/lzo/lzo1c.h include/lzo/lzo1f.h include/lzo/lzo1x.h \
    include/lzo/lzo1y.h include/lzo/lzo1z.h include/lzo/lzo2a.h \
    include/lzo/lzo_asm.h include/lzo/lzoconf.h include/lzo/lzodefs.h \
    include/lzo/lzoutil.h


##/***********************************************************************
##// SUBDIR src
##************************************************************************/

lib_LTLIBRARIES += src/liblzo2.la

src_liblzo2_la_LDFLAGS = -version-info 2:0:0 $(AM_LDFLAGS)

src_liblzo2_la_SOURCES = \
    src/lzo1.c src/lzo1_99.c src/lzo1a.c src/lzo1a_99.c src/lzo1b_1.c \
    src/lzo1b_2.c src/lzo1b_3.c src/lzo1b_4.c src/lzo1b_5.c src/lzo1b_6.c \
    src/lzo1b_7.c src/lzo1b_8.c src/lzo1b_9.c src/lzo1b_99.c src/lzo1b_9x.c \
    src/lzo1b_cc.c src/lzo1b_d1.c src/lzo1b_d2.c src/lzo1b_rr.c \
    src/lzo1b_xx.c src/lzo1c_1.c src/lzo1c_2.c src/lzo1c_3.c src/lzo1c_4.c \
    src/lzo1c_5.c src/lzo1c_6.c src/lzo1c_7.c src/lzo1c_8.c src/lzo1c_9.c \
    src/lzo1c_99.c src/lzo1c_9x.c src/lzo1c_cc.c src/lzo1c_d1.c \
    src/lzo1c_d2.c src/lzo1c_rr.c src/lzo1c_xx.c src/lzo1f_1.c \
    src/lzo1f_9x.c src/lzo1f_d1.c src/lzo1f_d2.c src/lzo1x_1.c \
    src/lzo1x_1k.c src/lzo1x_1l.c src/lzo1x_1o.c src/lzo1x_9x.c \
    src/lzo1x_d1.c src/lzo1x_d2.c src/lzo1x_d3.c src/lzo1x_o.c \
    src/lzo1y_1.c src/lzo1y_9x.c src/lzo1y_d1.c src/lzo1y_d2.c \
    src/lzo1y_d3.c src/lzo1y_o.c src/lzo1z_9x.c src/lzo1z_d1.c \
    src/lzo1z_d2.c src/lzo1z_d3.c src/lzo2a_9x.c src/lzo2a_d1.c \
    src/lzo2a_d2.c src/lzo_crc.c src/lzo_init.c src/lzo_ptr.c \
    src/lzo_str.c src/lzo_util.c

EXTRA_DIST += \
    src/compr1b.h src/compr1c.h src/config1.h src/config1a.h src/config1b.h \
    src/config1c.h src/config1f.h src/config1x.h src/config1y.h \
    src/config1z.h src/config2a.h src/lzo1_cm.ch src/lzo1_d.ch \
    src/lzo1a_cm.ch src/lzo1a_cr.ch src/lzo1a_de.h src/lzo1b_c.ch \
    src/lzo1b_cc.h src/lzo1b_cm.ch src/lzo1b_cr.ch src/lzo1b_d.ch \
    src/lzo1b_de.h src/lzo1b_r.ch src/lzo1b_sm.ch src/lzo1b_tm.ch \
    src/lzo1c_cc.h src/lzo1f_d.ch  src/lzo1x_c.ch src/lzo1x_d.ch \
    src/lzo1x_oo.ch src/lzo2a_d.ch src/lzo_conf.h src/lzo_dict.h \
    src/lzo_dll.ch src/lzo_func.ch src/lzo_mchw.ch src/lzo_ptr.h src/lzo_swd.ch \
    src/miniacc.h src/stats1a.h src/stats1b.h src/stats1c.h

LZO_ASM_SOURCES_i386_src_gas = \
    asm/i386/src_gas/lzo1c_s1.S asm/i386/src_gas/lzo1c_s2.S \
    asm/i386/src_gas/lzo1f_f1.S asm/i386/src_gas/lzo1f_f2.S \
    asm/i386/src_gas/lzo1x_f1.S asm/i386/src_gas/lzo1x_f2.S \
    asm/i386/src_gas/lzo1x_s1.S asm/i386/src_gas/lzo1x_s2.S \
    asm/i386/src_gas/lzo1y_f1.S asm/i386/src_gas/lzo1y_f2.S \
    asm/i386/src_gas/lzo1y_s1.S asm/i386/src_gas/lzo1y_s2.S
LZO_ASM_OBJECTS_i386_obj_elf32 = \
    asm/i386/obj/elf32/lzo1c_s1.o asm/i386/obj/elf32/lzo1c_s2.o \
    asm/i386/obj/elf32/lzo1f_f1.o asm/i386/obj/elf32/lzo1f_f2.o \
    asm/i386/obj/elf32/lzo1x_f1.o asm/i386/obj/elf32/lzo1x_f2.o \
    asm/i386/obj/elf32/lzo1x_s1.o asm/i386/obj/elf32/lzo1x_s2.o \
    asm/i386/obj/elf32/lzo1y_f1.o asm/i386/obj/elf32/lzo1y_f2.o \
    asm/i386/obj/elf32/lzo1y_s1.o asm/i386/obj/elf32/lzo1y_s2.o
if LZO_USE_ASM_i386_src_gas
LZO_ASM_SOURCES = $(LZO_ASM_SOURCES_i386_src_gas)
endif
if LZO_USE_ASM_i386_obj_elf32
LZO_ASM_OBJECTS = $(LZO_ASM_OBJECTS_i386_obj_elf32)
endif
nodist_src_liblzo2_la_SOURCES = $(LZO_ASM_SOURCES)
src_liblzo2_la_LIBADD         = $(LZO_ASM_OBJECTS)


##/***********************************************************************
##// SUBDIR examples
##************************************************************************/

noinst_PROGRAMS += examples/dict
noinst_PROGRAMS += examples/lzopack
noinst_PROGRAMS += examples/overlap
noinst_PROGRAMS += examples/precomp
noinst_PROGRAMS += examples/precomp2
noinst_PROGRAMS += examples/simple

examples_dict_SOURCES     = examples/dict.c
examples_lzopack_SOURCES  = examples/lzopack.c
examples_overlap_SOURCES  = examples/overlap.c
examples_precomp_SOURCES  = examples/precomp.c
examples_precomp2_SOURCES = examples/precomp2.c
examples_simple_SOURCES   = examples/simple.c

EXTRA_DIST += examples/portab.h examples/portab_a.h


##/***********************************************************************
##// SUBDIR lzotest
##************************************************************************/

noinst_PROGRAMS += lzotest/lzotest

lzotest_lzotest_SOURCES = lzotest/lzotest.c

EXTRA_DIST += lzotest/asm.h lzotest/db.h lzotest/wrap.h lzotest/wrapmisc.h


##/***********************************************************************
##// SUBDIR tests
##************************************************************************/

noinst_PROGRAMS += tests/align
noinst_PROGRAMS += tests/chksum
noinst_PROGRAMS += tests/promote
noinst_PROGRAMS += tests/sizes

tests_align_SOURCES   = tests/align.c
tests_chksum_SOURCES  = tests/chksum.c
tests_promote_SOURCES = tests/promote.c
tests_sizes_SOURCES   = tests/sizes.c

tests_promote_LDADD   =
tests_sizes_LDADD     =


##/***********************************************************************
##// SUBDIR minilzo
##************************************************************************/

noinst_PROGRAMS += minilzo/testmini

minilzo_testmini_SOURCES   = minilzo/testmini.c minilzo/minilzo.c
minilzo_testmini_SHORTNAME = t
minilzo_testmini_CPPFLAGS  = -DMINILZO_HAVE_CONFIG_H=1
minilzo_testmini_CPPFLAGS += -I$(top_srcdir)/include/lzo
minilzo_testmini_CPPFLAGS += $(AM_CPPFLAGS)
minilzo_testmini_LDADD     =

EXTRA_DIST += minilzo/Makefile.minilzo minilzo/README.LZO minilzo/minilzo.h

@MAINT@-include $(top_srcdir)/Makefile.maint
