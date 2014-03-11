SUBDIRS = platform
CORELIBS = @CORELIBS@
MORELIBS = @MORELIBS@ @DARWINLIBS@
MOREDEFS = @MOREDEFS@
DEFS = @DEFS@ $(MOREDEFS)
#CFLAGS = -O0 -fprofile-arcs -ftest-coverage
#CFLAGS = -O0 $(CFLAGS)

sbin_PROGRAMS = lldpd
bin_PROGRAMS = lldpneighbors

lldpd_CFLAGS = -I${top_srcdir}/src/tlv @DARWININCS@ $(CFLAGS)
lldpd_SOURCES = tx_sm.h rx_sm.h msap.h lldp_port.h \
		tlv/tlv.h tlv/tlv_common.h \
		tlv/oui/tlv_000ecf.h tlv/oui/tlv_00120f.h \
		tlv/oui/tlv_0012bb.h tlv/oui/tlv_0080c2.h \
		lldp_debug.h lldp_neighbor.h \
		lldp_main.c lldp_debug.c lldp_neighbor.c \
		tlv/tlv.c tlv/tlv_common.c\
		tlv/oui/tlv_000ecf.c tlv/oui/tlv_00120f.c \
		tlv/oui/tlv_0012bb.c tlv/oui/tlv_0080c2.c \
		tx_sm.c rx_sm.c msap.c 
lldpd_LDADD = $(CORELIBS) $(MORELIBS) platform/libplatform.a

lldpneighbors_SOURCES = lldpneighbors.c
lldpneighbors_LDADD = $(CORELIBS) $(MORELIBS)

EXTRA_DIST= getopts.h stdintwin.h