/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* Busybox mount uses either /proc/mounts or /etc/mtab to
 * get the list of currently mounted filesystems */
const char bb_path_mtab_file[] ALIGN1 =
IF_FEATURE_MTAB_SUPPORT("/etc/mtab")IF_NOT_FEATURE_MTAB_SUPPORT("/proc/mounts");
