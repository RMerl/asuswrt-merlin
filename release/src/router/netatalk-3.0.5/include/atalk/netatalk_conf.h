/*
   Copyright (c) 2012 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#ifndef AFP_CONFIG_H
#define AFP_CONFIG_H

#include <stdint.h>

#include <atalk/globals.h>
#include <atalk/volume.h>

extern int        afp_config_parse(AFPObj *obj, char *processname);
extern void       afp_config_free(AFPObj *obj);
extern int        load_charset(struct vol *vol);
extern int        load_volumes(AFPObj *obj);
extern void       unload_volumes(AFPObj *obj);
extern struct vol *getvolumes(void);
extern struct vol *getvolbyvid(const uint16_t);
extern struct vol *getvolbypath(AFPObj *obj, const char *path);
extern struct vol *getvolbyname(const char *name);
extern void       volume_free(struct vol *vol);
extern void       volume_unlink(struct vol *volume);

/* Extension type/creator mapping */
struct extmap *getdefextmap(void);
struct extmap *getextmap(const char *path);

#endif
