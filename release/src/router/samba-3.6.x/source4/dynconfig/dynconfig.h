/* 
   Unix SMB/CIFS implementation.
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   
   
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

/**
 * @file dynconfig.h
 *
 * @brief Exported global configurations.
 **/

#define DEFINE_DYN_CONFIG_PROTO(name)			\
extern const char *dyn_##name;		 			\
const char *get_dyn_##name(void);			\
const char *set_dyn_##name(const char *newpath);	\
bool is_default_dyn_##name(void);

/* these are in common with s3 */
DEFINE_DYN_CONFIG_PROTO(SBINDIR)
DEFINE_DYN_CONFIG_PROTO(BINDIR)
DEFINE_DYN_CONFIG_PROTO(SWATDIR)
DEFINE_DYN_CONFIG_PROTO(CONFIGFILE) /**< Location of smb.conf file. **/
DEFINE_DYN_CONFIG_PROTO(LOGFILEBASE) /** Log file directory. **/
DEFINE_DYN_CONFIG_PROTO(LMHOSTSFILE) /** Statically configured LanMan hosts. **/
DEFINE_DYN_CONFIG_PROTO(CODEPAGEDIR)
DEFINE_DYN_CONFIG_PROTO(LIBDIR)
DEFINE_DYN_CONFIG_PROTO(MODULESDIR)
DEFINE_DYN_CONFIG_PROTO(SHLIBEXT)
DEFINE_DYN_CONFIG_PROTO(LOCKDIR)
DEFINE_DYN_CONFIG_PROTO(STATEDIR) /** Persistent state files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PROTO(CACHEDIR) /** Temporary cache files. Default LOCKDIR */
DEFINE_DYN_CONFIG_PROTO(PIDDIR)
DEFINE_DYN_CONFIG_PROTO(NCALRPCDIR)
DEFINE_DYN_CONFIG_PROTO(SMB_PASSWD_FILE)
DEFINE_DYN_CONFIG_PROTO(PRIVATE_DIR)
DEFINE_DYN_CONFIG_PROTO(LOCALEDIR)
DEFINE_DYN_CONFIG_PROTO(NMBDSOCKETDIR)

/* these are not in s3 */
DEFINE_DYN_CONFIG_PROTO(DATADIR)
DEFINE_DYN_CONFIG_PROTO(SETUPDIR)
DEFINE_DYN_CONFIG_PROTO(WINBINDD_SOCKET_DIR)
DEFINE_DYN_CONFIG_PROTO(WINBINDD_PRIVILEGED_SOCKET_DIR)
DEFINE_DYN_CONFIG_PROTO(NTP_SIGND_SOCKET_DIR)
DEFINE_DYN_CONFIG_PROTO(PYTHONDIR)
DEFINE_DYN_CONFIG_PROTO(PYTHONARCHDIR)
DEFINE_DYN_CONFIG_PROTO(SCRIPTSBINDIR)
