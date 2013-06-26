/*
   Unix SMB/CIFS implementation.
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   Copyright (C) 2003 by Jim McDonough <jmcd@us.ibm.com>
   Copyright (C) 2007 by Jeremy Allison <jra@samba.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

/**
 * @file dynconfig.c
 *
 * @brief Global configurations, initialized to configured defaults.
 *
 * This file should be the only file that depends on path
 * configuration (--prefix, etc), so that if ./configure is re-run,
 * all programs will be appropriately updated.  Everything else in
 * Samba should import extern variables from here, rather than relying
 * on preprocessor macros.
 *
 * Eventually some of these may become even more variable, so that
 * they can for example consistently be set across the whole of Samba
 * by command-line parameters, config file entries, or environment
 * variables.
 *
 * @todo Perhaps eventually these should be merged into the parameter
 * table?  There's kind of a chicken-and-egg situation there...
 **/

#define DEFINE_DYN_CONFIG_PARAM(name) \
static char *dyn_##name; \
\
 const char *get_dyn_##name(void) \
{\
	if (dyn_##name == NULL) {\
		return name;\
	}\
	return dyn_##name;\
}\
\
 const char *set_dyn_##name(const char *newpath) \
{\
	if (dyn_##name) {\
		SAFE_FREE(dyn_##name);\
	}\
	dyn_##name = SMB_STRDUP(newpath);\
	return dyn_##name;\
}\
\
 bool is_default_dyn_##name(void) \
{\
	return (dyn_##name == NULL);\
}

DEFINE_DYN_CONFIG_PARAM(SBINDIR)
DEFINE_DYN_CONFIG_PARAM(BINDIR)
DEFINE_DYN_CONFIG_PARAM(SWATDIR)
DEFINE_DYN_CONFIG_PARAM(CONFIGFILE) /**< Location of smb.conf file. **/
DEFINE_DYN_CONFIG_PARAM(LOGFILEBASE) /** Log file directory. **/
DEFINE_DYN_CONFIG_PARAM(LMHOSTSFILE) /** Statically configured LanMan hosts. **/
DEFINE_DYN_CONFIG_PARAM(CODEPAGEDIR)
DEFINE_DYN_CONFIG_PARAM(LIBDIR)
DEFINE_DYN_CONFIG_PARAM(MODULESDIR)
DEFINE_DYN_CONFIG_PARAM(SHLIBEXT)
DEFINE_DYN_CONFIG_PARAM(LOCKDIR)
DEFINE_DYN_CONFIG_PARAM(STATEDIR) /** Persistent state files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PARAM(CACHEDIR) /** Temporary cache files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PARAM(PIDDIR)
DEFINE_DYN_CONFIG_PARAM(NMBDSOCKETDIR)
DEFINE_DYN_CONFIG_PARAM(NCALRPCDIR)
DEFINE_DYN_CONFIG_PARAM(SMB_PASSWD_FILE)
DEFINE_DYN_CONFIG_PARAM(PRIVATE_DIR)
DEFINE_DYN_CONFIG_PARAM(LOCALEDIR)
