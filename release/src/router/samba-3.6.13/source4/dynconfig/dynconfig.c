/* 
   Unix SMB/CIFS implementation.
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) Stefan Metzmacher	2003
   
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

#include "dynconfig.h"

#define DEFINE_DYN_CONFIG_PARAM(name) \
const char *dyn_##name = name; \
\
bool is_default_dyn_##name(void) \
{\
	if (strcmp(name, dyn_##name) == 0) { \
		return true; \
	} \
	return false; \
}\
\
const char *get_dyn_##name(void) \
{\
	return dyn_##name;\
}\
\
const char *set_dyn_##name(const char *newpath) \
{\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (strcmp(name, newpath) == 0) { \
		return dyn_##name; \
	} \
	newpath = strdup(newpath);\
	if (newpath == NULL) { \
		return NULL; \
	} \
	if (is_default_dyn_##name()) { \
		/* do not free a static string */ \
	} else if (dyn_##name) {\
		free(discard_const(dyn_##name)); \
	}\
	dyn_##name = newpath; \
	return dyn_##name;\
}

/* these are in common with s3 */
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
DEFINE_DYN_CONFIG_PARAM(NCALRPCDIR)
DEFINE_DYN_CONFIG_PARAM(SMB_PASSWD_FILE)
DEFINE_DYN_CONFIG_PARAM(PRIVATE_DIR)
DEFINE_DYN_CONFIG_PARAM(LOCALEDIR)
DEFINE_DYN_CONFIG_PARAM(NMBDSOCKETDIR)

/* these are not in s3 */
DEFINE_DYN_CONFIG_PARAM(DATADIR)
DEFINE_DYN_CONFIG_PARAM(SETUPDIR)
DEFINE_DYN_CONFIG_PARAM(WINBINDD_SOCKET_DIR)
DEFINE_DYN_CONFIG_PARAM(WINBINDD_PRIVILEGED_SOCKET_DIR)
DEFINE_DYN_CONFIG_PARAM(NTP_SIGND_SOCKET_DIR)
DEFINE_DYN_CONFIG_PARAM(PYTHONDIR)
DEFINE_DYN_CONFIG_PARAM(PYTHONARCHDIR)
DEFINE_DYN_CONFIG_PARAM(SCRIPTSBINDIR)
