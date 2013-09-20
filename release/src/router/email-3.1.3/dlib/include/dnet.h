#ifndef __DNET_H
#define __DNET_H 1

#include <netdb.h>
#include <stdio.h>

#include "dlib.h"
#include "dstrbuf.h"

/* Error flags */
enum { 
	SOCKET_ERROR = 0x01,
	SOCKET_EOF = 0x02,
};

enum {
	MAXSOCKBUF = 2048 
};

typedef struct socket dsocket;

int dnetResolveName(const char *name, struct hostent *hent);
dsocket *dnetConnect(const char *hostname, unsigned int port);
int dnetUseTls(dsocket *sd);
int dnetVerifyCert(dsocket *sd);

void dnetClose(dsocket *sd);
int dnetGetc(dsocket *sd);
int dnetPutc(dsocket *sd, int ch);
int dnetReadline(dsocket *sd, dstrbuf *buf);
int dnetWrite(dsocket *sd, const char *buf, size_t len);
int dnetRead(dsocket *sd, char *buf, size_t size);

bool dnetErr(dsocket *sd);
bool dnetEof(dsocket *sd);
char *dnetGetErr(dsocket *sd);
int dnetGetSock(dsocket *sd);

#endif
