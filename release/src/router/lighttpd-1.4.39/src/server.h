#ifndef _SERVER_H_
#define _SERVER_H_

#include "base.h"

int config_read(server *srv, const char *fn);
int config_set_defaults(server *srv);

#endif
