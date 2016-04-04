#ifndef _JOB_LIST_H_
#define _JOB_LIST_H_

#include "base.h"

int joblist_append(server *srv, connection *con);
void joblist_free(server *srv, connections *joblist);

int fdwaitqueue_append(server *srv, connection *con);
void fdwaitqueue_free(server *srv, connections *fdwaitqueue);
connection *fdwaitqueue_unshift(server *srv, connections *fdwaitqueue);

#endif
