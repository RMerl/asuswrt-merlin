include $(top_srcdir)/config/include-Makefile.am

bin_PROGRAMS =
usrbin_exec_PROGRAMS =
sbin_PROGRAMS =
usrsbin_exec_PROGRAMS =
dist_man_MANS =

EXTRA_DIST = README.poeigl

if BUILD_LAST
usrbin_exec_PROGRAMS += last
dist_man_MANS += last.1
endif

if BUILD_LOGIN_UTILS

bin_PROGRAMS += login
usrbin_exec_PROGRAMS += chfn chsh newgrp
usrsbin_exec_PROGRAMS += vipw
dist_man_MANS += chfn.1 chsh.1 login.1 newgrp.1 vipw.8 vigr.8

# login, chfn and chsh libs
login_ldadd_common =

chfn_SOURCES = chfn.c $(chfn_chsh_common)
chsh_SOURCES = chsh.c $(chfn_chsh_common)
chfn_chsh_common = islocal.c setpwnam.c islocal.h setpwnam.h \
	$(top_srcdir)/lib/env.c
login_SOURCES = login.c login.h $(top_srcdir)/lib/setproctitle.c
vipw_SOURCES = vipw.c setpwnam.h

chfn_LDADD = $(login_ldadd_common)
chsh_LDADD = $(login_ldadd_common)
login_LDADD = $(login_ldadd_common)
newgrp_LDADD =
vipw_LDADD =

chfn_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)
chsh_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)
newgrp_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)

chfn_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
chsh_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)
newgrp_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)

if HAVE_PAM
login_ldadd_common += -lpam -lpam_misc
else
if NEED_LIBCRYPT
login_ldadd_common += -lcrypt
endif
login_SOURCES += checktty.c
endif #! HAVE_PAM

if NEED_LIBCRYPT
newgrp_LDADD += -lcrypt
endif

if HAVE_AUDIT
login_LDADD += -laudit
endif

if HAVE_SELINUX
chfn_SOURCES += selinux_utils.c selinux_utils.h
chsh_SOURCES += selinux_utils.c selinux_utils.h
vipw_LDADD += -lselinux
login_ldadd_common += -lselinux
endif


install-exec-hook::
	cd $(DESTDIR)$(usrsbin_execdir) && ln -sf vipw vigr

endif

noinst_PROGRAMS = checktty_test islocal_test
checktty_test_SOURCES = checktty.c login.h
checktty_test_CPPFLAGS = -DMAIN_TEST_CHECKTTY $(AM_CPPFLAGS)
islocal_test_SOURCES = islocal.c
islocal_test_CPPFLAGS = -DMAIN_TEST_ISLOCAL $(AM_CPPFLAGS)

