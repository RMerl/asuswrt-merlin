/*
 * Copyright (c) 2013 Frank Lahm <franklahm@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef AFPD_AFPSTATS_H
#define AFPD_AFPSTATS_H

#include <atalk/server_child.h>

extern int afpstats_init(server_child_t *);
extern server_child_t *afpstats_get_and_lock_childs(void);
extern void afpstats_unlock_childs(void);
#endif
