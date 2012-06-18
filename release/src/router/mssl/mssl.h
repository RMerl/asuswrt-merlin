/*

	Minimal MatrixSSL Helper
	Copyright (C) 2006-2009 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#ifndef __MSSL_H__
#define __MSSL_H__

extern FILE *ssl_server_fopen(int sd);
extern FILE *ssl_client_fopen(int sd);
extern int mssl_init(char *cert, char *priv);

#endif
