include $(top_srcdir)/Make_global.am

sbin_PROGRAMS	= ipset
ipset_SOURCES	= ipset.c ui.c
ipset_LDADD	= ../lib/libipset.la

if ENABLE_SETTYPE_MODULES
AM_LDFLAGS  = -shared
else
if ENABLE_STATIC
AM_LDFLAGS	= -static
endif
endif

dist_man_MANS = ipset.8

sparse-check: $(ipset_SOURCES:.c=.d)

%.d: %.c
	$(IPSET_AM_V_CHECK)\
	$(SPARSE) -I.. $(SPARSE_FLAGS) $(AM_CFLAGS) $(AM_CPPFLAGS) $< || :
