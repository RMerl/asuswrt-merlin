/*
 *  setpwnam.h --
 *  define several paths
 *
 *  (c) 1994 Martin Schulze <joey@infodrom.north.de>
 *  This file is based on setpwnam.c which is
 *  (c) 1994 Salvatore Valente <svalente@mit.edu>
 *
 *  This file is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 */

#include "pathnames.h"

#ifndef DEBUG
#define PASSWD_FILE    _PATH_PASSWD
#define PTMP_FILE      _PATH_PTMP
#define PTMPTMP_FILE   _PATH_PTMPTMP

#define GROUP_FILE     _PATH_GROUP
#define GTMP_FILE      _PATH_GTMP
#define GTMPTMP_FILE   _PATH_GTMPTMP

#define SHADOW_FILE	_PATH_SHADOW_PASSWD
#define SPTMP_FILE	_PATH_SHADOW_PTMP
#define SPTMPTMP_FILE	_PATH_SHADOW_PTMPTMP

#define SGROUP_FILE	_PATH_SHADOW_GROUP
#define SGTMP_FILE	_PATH_SHADOW_GTMP
#define SGTMPTMP_FILE	_PATH_SHADOW_GTMPTMP

#else
#define PASSWD_FILE    "/tmp/passwd"
#define PTMP_FILE      "/tmp/ptmp"
#define PTMPTMP_FILE   "/tmp/ptmptmp"

#define GROUP_FILE     "/tmp/group"
#define GTMP_FILE      "/tmp/gtmp"
#define GTMPTMP_FILE   "/tmp/gtmptmp"

#define SHADOW_FILE	"/tmp/shadow"
#define SPTMP_FILE	"/tmp/sptmp"
#define SPTMPTMP_FILE	"/tmp/sptmptmp"

#define SGROUP_FILE	"/tmp/gshadow"
#define SGTMP_FILE	"/tmp/sgtmp"
#define SGTMPTMP_FILE	"/tmp/sgtmptmp"
#endif

extern int setpwnam (struct passwd *pwd);
