/***
  This file is part of libdaemon.

  Copyright 2003-2008 Lennart Poettering

  libdaemon is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2.1 of the
  License, or (at your option) any later version.

  libdaemon is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with libdaemon. If not, see
  <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>

#include "dnonblock.h"

int daemon_nonblock(int fd, int b) {
    int a, c;

    if ((a = fcntl(fd, F_GETFL)) < 0)
        return -1;

    if (b)
        c = a | O_NONBLOCK;
    else
        c = a & ~O_NONBLOCK;

    if (c == a)
        return 0;

    return fcntl(fd, F_SETFL, c);
}
