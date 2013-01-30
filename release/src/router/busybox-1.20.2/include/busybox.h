/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#ifndef BUSYBOX_H
#define BUSYBOX_H 1

#include "libbb.h"
/* BB_DIR_foo and BB_SUID_bar constants: */
#include "applet_metadata.h"

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

/* Defined in appletlib.c (by including generated applet_tables.h) */
/* Keep in sync with applets/applet_tables.c! */
extern const char applet_names[];
extern int (*const applet_main[])(int argc, char **argv);
extern const uint16_t applet_nameofs[];
extern const uint8_t applet_install_loc[];

#if ENABLE_FEATURE_SUID || ENABLE_FEATURE_PREFER_APPLETS
# define APPLET_NAME(i) (applet_names + (applet_nameofs[i] & 0x0fff))
#else
# define APPLET_NAME(i) (applet_names + applet_nameofs[i])
#endif

#if ENABLE_FEATURE_PREFER_APPLETS
# define APPLET_IS_NOFORK(i) (applet_nameofs[i] & (1 << 12))
# define APPLET_IS_NOEXEC(i) (applet_nameofs[i] & (1 << 13))
#else
# define APPLET_IS_NOFORK(i) 0
# define APPLET_IS_NOEXEC(i) 0
#endif

#if ENABLE_FEATURE_SUID
# define APPLET_SUID(i) ((applet_nameofs[i] >> 14) & 0x3)
#endif

#if ENABLE_FEATURE_INSTALLER
#define APPLET_INSTALL_LOC(i) ({ \
	unsigned v = (i); \
	if (v & 1) v = applet_install_loc[v/2] >> 4; \
	else v = applet_install_loc[v/2] & 0xf; \
	v; })
#endif


/* Length of these names has effect on size of libbusybox
 * and "individual" binaries. Keep them short.
 */
#if ENABLE_BUILD_LIBBUSYBOX
#if ENABLE_FEATURE_SHARED_BUSYBOX
int lbb_main(char **argv) EXTERNALLY_VISIBLE;
#else
int lbb_main(char **argv);
#endif
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
