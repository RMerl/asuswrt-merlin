/* 
   Unix SMB/CIFS implementation.
   Copyright (C) 2001 by Martin Pool <mbp@samba.org>
   Copyright (C) 2003 by Jim McDonough <jmcd@us.ibm.com>

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

const char *get_dyn_SBINDIR(void);
const char *set_dyn_SBINDIR(const char *newpath);
bool is_default_dyn_SBINDIR(void);

const char *get_dyn_BINDIR(void);
const char *set_dyn_BINDIR(const char *newpath);
bool is_default_dyn_BINDIR(void);

const char *get_dyn_SWATDIR(void);
const char *set_dyn_SWATDIR(const char *newpath);
bool is_default_dyn_SWATDIR(void);

const char *get_dyn_CONFIGFILE(void);
const char *set_dyn_CONFIGFILE(const char *newpath);
bool is_default_dyn_CONFIGFILE(void);

const char *get_dyn_LOGFILEBASE(void);
const char *set_dyn_LOGFILEBASE(const char *newpath);
bool is_default_dyn_LOGFILEBASE(void);

const char *get_dyn_LMHOSTSFILE(void);
const char *set_dyn_LMHOSTSFILE(const char *newpath);
bool is_default_dyn_LMHOSTSFILE(void);

const char *get_dyn_CODEPAGEDIR(void);
const char *set_dyn_CODEPAGEDIR(const char *newpath);
bool is_default_dyn_CODEPAGEDIR(void);

const char *get_dyn_LIBDIR(void);
const char *set_dyn_LIBDIR(const char *newpath);
bool is_default_dyn_LIBDIR(void);

const char *get_dyn_MODULESDIR(void);
const char *set_dyn_MODULESDIR(const char *newpath);
bool is_default_dyn_MODULESDIR(void);

const char *get_dyn_SHLIBEXT(void);
const char *set_dyn_SHLIBEXT(const char *newpath);
bool is_default_dyn_SHLIBEXT(void);

const char *get_dyn_LOCKDIR(void);
const char *set_dyn_LOCKDIR(const char *newpath);
bool is_default_dyn_LOCKDIR(void);

const char *get_dyn_STATEDIR(void);
const char *set_dyn_STATEDIR(const char *newpath);
bool is_default_dyn_STATEDIR(void);

const char *get_dyn_CACHEDIR(void);
const char *set_dyn_CACHEDIR(const char *newpath);
bool is_default_dyn_CACHEDIR(void);

const char *get_dyn_PIDDIR(void);
const char *set_dyn_PIDDIR(const char *newpath);
bool is_default_dyn_PIDDIR(void);

const char *get_dyn_NMBDSOCKETDIR(void);
const char *set_dyn_NMBDSOCKETDIR(const char *newpath);
bool is_default_dyn_NMBDSOCKETDIR(void);

const char *get_dyn_NCALRPCDIR(void);
const char *set_dyn_NCALRPCDIR(const char *newpath);
bool is_default_dyn_NCALRPCDIR(void);

const char *get_dyn_SMB_PASSWD_FILE(void);
const char *set_dyn_SMB_PASSWD_FILE(const char *newpath);
bool is_default_dyn_SMB_PASSWD_FILE(void);

const char *get_dyn_PRIVATE_DIR(void);
const char *set_dyn_PRIVATE_DIR(const char *newpath);
bool is_default_dyn_PRIVATE_DIR(void);

const char *get_dyn_LOCALEDIR(void);
const char *set_dyn_LOCALEDIR(const char *newpath);
bool is_default_dyn_LOCALEDIR(void);
