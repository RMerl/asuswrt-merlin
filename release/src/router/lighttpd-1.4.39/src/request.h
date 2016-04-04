#ifndef _REQUEST_H_
#define _REQUEST_H_

#include "server.h"

int http_request_parse(server *srv, connection *con);
int http_request_header_finished(server *srv, connection *con);

#endif
