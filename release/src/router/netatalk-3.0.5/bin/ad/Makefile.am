# Makefile.am for bin/ad/

noinst_HEADERS = ad.h

if USE_BDB
bin_PROGRAMS = ad

ad_SOURCES = \
	ad.c \
	ad_find.c \
	ad_util.c \
	ad_ls.c \
	ad_cp.c \
	ad_mv.c \
	ad_rm.c \
	ad_set.c

ad_CFLAGS = -D_PATH_AD=\"$(bindir)/ad\"

ad_LDADD = \
	$(top_builddir)/libatalk/cnid/libcnid.la \
	$(top_builddir)/libatalk/libatalk.la \
	@ACL_LIBS@

endif
