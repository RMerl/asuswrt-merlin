AM_CFLAGS = -Wall
LDADD = -lm

man_MANS = ethtool.8
EXTRA_DIST = LICENSE ethtool.8 ethtool.spec.in aclocal.m4 ChangeLog autogen.sh

sbin_PROGRAMS = ethtool
ethtool_SOURCES = ethtool.c ethtool-copy.h internal.h net_tstamp-copy.h \
		  rxclass.c
if ETHTOOL_ENABLE_PRETTY_DUMP
ethtool_SOURCES += \
		  amd8111e.c de2104x.c e100.c e1000.c et131x.c igb.c	\
		  fec_8xx.c ibm_emac.c ixgb.c ixgbe.c natsemi.c	\
		  pcnet32.c realtek.c tg3.c marvell.c vioc.c	\
		  smsc911x.c at76c50x-usb.c sfc.c stmmac.c	\
		  sff-common.c sff-common.h sfpid.c sfpdiag.c	\
		  ixgbevf.c tse.c vmxnet3.c qsfp.c qsfp.h
endif

TESTS = test-cmdline test-features
check_PROGRAMS = test-cmdline test-features
test_cmdline_SOURCES = test-cmdline.c test-common.c $(ethtool_SOURCES) 
test_cmdline_CFLAGS = -DTEST_ETHTOOL
test_features_SOURCES = test-features.c test-common.c $(ethtool_SOURCES) 
test_features_CFLAGS = -DTEST_ETHTOOL

dist-hook:
	cp $(top_srcdir)/ethtool.spec $(distdir)

