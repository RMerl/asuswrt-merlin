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

#include "directory.h"
#include "dircache.h"
#include "hash.h"
#include "afp_config.h"
#include "volume.h"

#include "test.h"
#include "subtests.h"

static int reti;                /* for the TEST_int macro */

int test001_add_x_dirs(const struct vol *vol, cnid_t start, cnid_t end)
{
    struct dir *dir;
    char dirname[20];
    while (start++ < end) {
        sprintf(dirname, "dir%04u", start);
        dir = dir_new(dirname, dirname, vol, DIRDID_ROOT, htonl(start), bfromcstr(vol->v_path), 0);
        if (dir == NULL)
            return -1;
        if (dircache_add(vol, dir) != 0)
            return -1;
    }

    return 0;
}

int test002_rem_x_dirs(const struct vol *vol, cnid_t start, cnid_t end)
{
    struct dir *dir;
    while (start++ < end) {
        if ((dir = dircache_search_by_did(vol, htonl(start))))
            if (dir_remove(vol, dir) != 0)
                return -1;
    }

    return 0;
}
