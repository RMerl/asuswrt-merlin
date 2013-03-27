#
# $Id$
#

all: vfork fork sig skodic clone leaderkill childthread

leaderkill: LDFLAGS += -pthread

childthread: LDFLAGS += -pthread

clean distclean:
	rm -f clone vfork fork sig leaderkill *.o core
