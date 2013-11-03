/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/volume.h>
#include <atalk/directory.h>
#include <atalk/queue.h>
#include <atalk/bstrlib.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>

#include "file.h"
#include "filedir.h"
#include "directory.h"
#include "dircache.h"
#include "hash.h"
#include "afp_config.h"
#include "volume.h"

#include "test.h"
#include "subtests.h"
#include "afpfunc_helpers.h"

/* Stuff from main.c which of cource can't be added as source to testbin */
unsigned char nologin = 0;
static AFPObj obj;
#define ARGNUM 3
static char *args[] = {"test", "-F", "test.conf"};
/* Static variables */

int main(int argc, char **argv)
{
    int reti;
    uint16_t vid;
    struct vol *vol;
    struct dir *retdir;
    struct path *path;

    /* initialize */
    printf("Initializing\n============\n");
    TEST(setuplog("default:note","/dev/tty"));

    TEST( afp_options_parse_cmdline(&obj, 3, &args[0]) );

    TEST_int( afp_config_parse(&obj, NULL), 0);
    TEST_int( configinit(&obj), 0);
    TEST( cnid_init() );
    TEST( load_volumes(&obj) );
    TEST_int( dircache_init(8192), 0);
    obj.afp_version = 32;

    printf("\n");

    /* now run tests */
    printf("Running tests\n=============\n");

    TEST_expr(vid = openvol(&obj, "test"), vid != 0);
    TEST_expr(vol = getvolbyvid(vid), vol != NULL);

    /* test directory.c stuff */
    TEST_expr(retdir = dirlookup(vol, DIRDID_ROOT_PARENT), retdir != NULL);
    TEST_expr(retdir = dirlookup(vol, DIRDID_ROOT), retdir != NULL);
    TEST_expr(path = cname(vol, retdir, cnamewrap("Network Trash Folder")), path != NULL);

    TEST_expr(retdir = dirlookup(vol, DIRDID_ROOT), retdir != NULL);
    TEST_int(getfiledirparms(&obj, vid, DIRDID_ROOT_PARENT, "test"), 0);
    TEST_int(getfiledirparms(&obj, vid, DIRDID_ROOT, ""), 0);

#if 0
// this doesn't work anymore since cnid sheme = last (which we use in the test)
// has been changed to force read-only mode for the volume
    TEST_expr(reti = createdir(&obj, vid, DIRDID_ROOT, "dir1"),
              reti == 0 || reti == AFPERR_EXIST);

    TEST_int(getfiledirparms(&obj, vid, DIRDID_ROOT, "dir1"), 0);
#endif
/*
  FIXME: this doesn't work although it should. "//" get translated to \000 \000 at means ".."
  ie this should getfiledirparms for DIRDID_ROOT_PARENT -- at least afair!
    TEST_int(getfiledirparms(&configs->obj, vid, DIRDID_ROOT, "//"), 0);
*/
#if 0
    TEST_int(createfile(&obj, vid, DIRDID_ROOT, "dir1/file1"), 0);
    TEST_int(delete(&obj, vid, DIRDID_ROOT, "dir1/file1"), 0);
    TEST_int(delete(&obj, vid, DIRDID_ROOT, "dir1"), 0);

    TEST_int(createfile(&obj, vid, DIRDID_ROOT, "file1"), 0);
    TEST_int(getfiledirparms(&obj, vid, DIRDID_ROOT, "file1"), 0);
    TEST_int(delete(&obj, vid, DIRDID_ROOT, "file1"), 0);
#endif

    /* test enumerate.c stuff */
    TEST_int(enumerate(&obj, vid, DIRDID_ROOT), 0);
}
