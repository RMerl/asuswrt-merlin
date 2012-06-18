## Process this file with automake to create Makefile.in
#
# Copyright (C) 1996-2008 Markus F.X.J. Oberhumer
#

VPATH = @srcdir@@LZO_ASM_VPATH@
SUFFIXES = .S

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)
CPPFLAGS = $(LZO_CPPFLAGS) $(configure_CPPFLAGS) $(LZO_EXTRA_CPPFLAGS)
CFLAGS = $(LZO_CFLAGS) $(configure_CFLAGS) $(LZO_EXTRA_CFLAGS)

lib_LTLIBRARIES = liblzo2.la

liblzo2_la_LDFLAGS = -version-info 2:0:0

liblzo2_la_SOURCES = \
    lzo_crc.c lzo_init.c lzo_ptr.c lzo_str.c lzo_util.c \
    lzo1.c lzo1_99.c \
    lzo1a.c lzo1a_99.c \
    lzo1b_1.c lzo1b_2.c lzo1b_3.c lzo1b_4.c lzo1b_5.c lzo1b_6.c \
    lzo1b_7.c lzo1b_8.c lzo1b_9.c lzo1b_99.c lzo1b_9x.c \
    lzo1b_cc.c lzo1b_d1.c lzo1b_d2.c lzo1b_rr.c lzo1b_xx.c \
    lzo1c_1.c lzo1c_2.c lzo1c_3.c lzo1c_4.c lzo1c_5.c lzo1c_6.c \
    lzo1c_7.c lzo1c_8.c lzo1c_9.c lzo1c_99.c lzo1c_9x.c \
    lzo1c_cc.c lzo1c_d1.c lzo1c_d2.c lzo1c_rr.c lzo1c_xx.c \
    lzo1f_1.c lzo1f_9x.c lzo1f_d1.c lzo1f_d2.c \
    lzo1x_1.c lzo1x_9x.c lzo1x_d1.c lzo1x_d2.c lzo1x_d3.c \
    lzo1x_o.c lzo1x_1k.c lzo1x_1l.c lzo1x_1o.c \
    lzo1y_1.c lzo1y_9x.c lzo1y_d1.c lzo1y_d2.c lzo1y_d3.c lzo1y_o.c \
    lzo1z_9x.c lzo1z_d1.c lzo1z_d2.c lzo1z_d3.c \
    lzo2a_9x.c lzo2a_d1.c lzo2a_d2.c

EXTRA_DIST = \
    lzo1_cm.ch lzo1_d.ch lzo1a_cm.ch lzo1a_cr.ch lzo1b_c.ch \
    lzo1b_cm.ch lzo1b_cr.ch lzo1b_d.ch lzo1b_r.ch \
    lzo1b_sm.ch lzo1b_tm.ch lzo1f_d.ch lzo1x_c.ch \
    lzo1x_d.ch lzo1x_oo.ch lzo2a_d.ch \
    lzo_dll.ch lzo_mchw.ch lzo_swd.ch \
    compr1b.h compr1c.h config1.h config1a.h config1b.h config1c.h \
    config1f.h config1x.h config1y.h config1z.h config2a.h \
    lzo1a_de.h lzo1b_cc.h lzo1b_de.h lzo1c_cc.h lzo_conf.h lzo_dict.h \
    lzo_ptr.h stats1a.h stats1b.h stats1c.h miniacc.h

liblzo2_la_LIBADD = $(LZO_ASM_OBJECTS)
liblzo2_la_DEPENDENCIES = $(liblzo2_la_LIBADD)


# /***********************************************************************
# // assembler support
# ************************************************************************/

if LZO_USE_ASM

LZO_ASM_OBJECTS = \
	lzo1c_s1.lo lzo1c_s2.lo \
	lzo1f_f1.lo lzo1f_f2.lo \
	lzo1x_f1.lo lzo1x_f2.lo lzo1x_s1.lo lzo1x_s2.lo \
	lzo1y_f1.lo lzo1y_f2.lo lzo1y_s1.lo lzo1y_s2.lo

.S.o:
	$(CC) -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(CC) -c $<

endif

