#include "base.h"
#include "joblist.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

int joblist_append(server *srv, connection *con) {
	if (con->in_joblist) return 0;

	if (srv->joblist->size == 0) {
		srv->joblist->size  = 16;
		srv->joblist->ptr   = malloc(sizeof(*srv->joblist->ptr) * srv->joblist->size);
	} else if (srv->joblist->used == srv->joblist->size) {
		srv->joblist->size += 16;
		srv->joblist->ptr   = realloc(srv->joblist->ptr, sizeof(*srv->joblist->ptr) * srv->joblist->size);
	}

	srv->joblist->ptr[srv->joblist->used++] = con;

	return 0;
}

void joblist_free(server *srv, connections *joblist) {
	UNUSED(srv);

	free(joblist->ptr);
	free(joblist);
}

connection *fdwaitqueue_unshift(server *srv, connections *fdwaitqueue) {
	connection *con;
	UNUSED(srv);


	if (fdwaitqueue->used == 0) return NULL;

	con = fdwaitqueue->ptr[0];

	memmove(fdwaitqueue->ptr, &(fdwaitqueue->ptr[1]), --fdwaitqueue->used * sizeof(*(fdwaitqueue->ptr)));

	return con;
}

int fdwaitqueue_append(server *srv, connection *con) {
	if (srv->fdwaitqueue->size == 0) {
		srv->fdwaitqueue->size  = 16;
		srv->fdwaitqueue->ptr   = malloc(sizeof(*(srv->fdwaitqueue->ptr)) * srv->fdwaitqueue->size);
	} else if (srv->fdwaitqueue->used == srv->fdwaitqueue->size) {
		srv->fdwaitqueue->size += 16;
		srv->fdwaitqueue->ptr   = realloc(srv->fdwaitqueue->ptr, sizeof(*(srv->fdwaitqueue->ptr)) * srv->fdwaitqueue->size);
	}

	srv->fdwaitqueue->ptr[srv->fdwaitqueue->used++] = con;

	return 0;
}

void fdwaitqueue_free(server *srv, connections *fdwaitqueue) {
	UNUSED(srv);
	free(fdwaitqueue->ptr);
	free(fdwaitqueue);
}
