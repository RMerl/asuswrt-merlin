include $(top_srcdir)/Make_global.am

lib_LTLIBRARIES = libnfnetlink.la

libnfnetlink_la_LDFLAGS = -Wc,-nostartfiles	\
			  -version-info $(LIBVERSION)
libnfnetlink_la_SOURCES = libnfnetlink.c iftable.c rtnl.c

noinst_HEADERS = iftable.h rtnl.h
