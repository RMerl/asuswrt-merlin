#ifndef AFPD_CONFIG_H
#define AFPD_CONFIG_H 1

#include <atalk/server_child.h>
#include <atalk/globals.h>
#include <atalk/dsi.h>

extern int configinit (AFPObj *);
extern void configfree (AFPObj *, DSI *);

#endif
