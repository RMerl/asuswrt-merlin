/*
 * showmount.c -- show mount information for an NFS server
 * Copyright (C) 1993 Rick Sladkey <jrs@world.std.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <fcntl.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <mount.h>
#include <unistd.h>

#include "nfsrpc.h"

#define TIMEOUT_UDP	3
#define TOTAL_TIMEOUT	20

static char *	version = "showmount for " VERSION;
static char *	program_name;
static int	headers = 1;
static int	hflag = 0;
static int	aflag = 0;
static int	dflag = 0;
static int	eflag = 0;

static struct option longopts[] =
{
	{ "all", 0, 0, 'a' },
	{ "directories", 0, 0, 'd' },
	{ "exports", 0, 0, 'e' },
	{ "no-headers", 0, &headers, 0 },
	{ "version", 0, 0, 'v' },
	{ "help", 0, 0, 'h' },
	{ NULL, 0, 0, 0 }
};

#define MAXHOSTLEN 256

static int dump_cmp(const void *pv, const void *qv)
{
	const char **p = (const char **)pv;
	const char **q = (const char **)qv;
	return strcmp(*p, *q);
}

static void usage(FILE *fp, int n)
{
	fprintf(fp, "Usage: %s [-adehv]\n", program_name);
	fprintf(fp, "       [--all] [--directories] [--exports]\n");
	fprintf(fp, "       [--no-headers] [--help] [--version] [host]\n");
	exit(n);
}

static const char *nfs_sm_pgmtbl[] = {
	"showmount",
	"mount",
	"mountd",
	NULL,
};

/*
 * Generate an RPC client handle connected to the mountd service
 * at @hostname, or die trying.
 *
 * Supports both AF_INET and AF_INET6 server addresses.
 */
static CLIENT *nfs_get_mount_client(const char *hostname)
{
	rpcprog_t program = nfs_getrpcbyname(MOUNTPROG, nfs_sm_pgmtbl);
	CLIENT *client;

	client = clnt_create(hostname, program, MOUNTVERS, "tcp");
	if (client)
		return client;

	client = clnt_create(hostname, program, MOUNTVERS, "udp");
	if (client)
		return client;

	clnt_pcreateerror("clnt_create");
	exit(1);
}

int main(int argc, char **argv)
{
	char hostname_buf[MAXHOSTLEN];
	char *hostname;
	enum clnt_stat clnt_stat;
	struct timeval total_timeout;
	int c;
	CLIENT *mclient;
	groups grouplist;
	exports exportlist, exl;
	mountlist dumplist;
	mountlist list;
	int i;
	int n;
	int maxlen;
	char **dumpv;

	program_name = argv[0];
	while ((c = getopt_long(argc, argv, "adehv", longopts, NULL)) != EOF) {
		switch (c) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'h':
			usage(stdout, 0);
			break;
		case 'v':
			printf("%s\n", version);
			exit(0);
		case 0:
			break;
		case '?':
		default:
			usage(stderr, 1);
			break;
		}
	}
	argc -= optind;
	argv += optind;

	switch (aflag + dflag + eflag) {
	case 0:
		hflag = 1;
		break;
	case 1:
		break;
	default:
		fprintf(stderr, "%s: only one of -a, -d or -e is allowed\n",
			program_name);
		exit(1);
		break;
	}

	switch (argc) {
	case 0:
		if (gethostname(hostname_buf, MAXHOSTLEN) < 0) {
			perror("getting hostname");
			exit(1);
		}
		hostname = hostname_buf;
		break;
	case 1:
		hostname = argv[0];
		break;
	default:
		fprintf(stderr, "%s: only one hostname is allowed\n",
			program_name);
		exit(1);
		break;
	}

	mclient = nfs_get_mount_client(hostname);
	mclient->cl_auth = authunix_create_default();
	total_timeout.tv_sec = TOTAL_TIMEOUT;
	total_timeout.tv_usec = 0;

	if (eflag) {
		memset(&exportlist, '\0', sizeof(exportlist));

		clnt_stat = clnt_call(mclient, MOUNTPROC_EXPORT,
			(xdrproc_t) xdr_void, NULL,
			(xdrproc_t) xdr_exports, (caddr_t) &exportlist,
			total_timeout);
		if (clnt_stat != RPC_SUCCESS) {
			clnt_perror(mclient, "rpc mount export");
			clnt_destroy(mclient);
			exit(1);
		}
		if (headers)
			printf("Export list for %s:\n", hostname);
		maxlen = 0;
		for (exl = exportlist; exl; exl = exl->ex_next) {
			if ((n = strlen(exl->ex_dir)) > maxlen)
				maxlen = n;
		}
		while (exportlist) {
			printf("%-*s ", maxlen, exportlist->ex_dir);
			grouplist = exportlist->ex_groups;
			if (grouplist)
				while (grouplist) {
					printf("%s%s", grouplist->gr_name,
						grouplist->gr_next ? "," : "");
					grouplist = grouplist->gr_next;
				}
			else
				printf("(everyone)");
			printf("\n");
			exportlist = exportlist->ex_next;
		}
		clnt_destroy(mclient);
		exit(0);
	}

	memset(&dumplist, '\0', sizeof(dumplist));
	clnt_stat = clnt_call(mclient, MOUNTPROC_DUMP,
		(xdrproc_t) xdr_void, NULL,
		(xdrproc_t) xdr_mountlist, (caddr_t) &dumplist,
		total_timeout);
	if (clnt_stat != RPC_SUCCESS) {
		clnt_perror(mclient, "rpc mount dump");
		clnt_destroy(mclient);
		exit(1);
	}
	clnt_destroy(mclient);

	n = 0;
	for (list = dumplist; list; list = list->ml_next)
		n++;
	dumpv = (char **) calloc(n, sizeof (char *));
	if (n && !dumpv) {
		fprintf(stderr, "%s: out of memory\n", program_name);
		exit(1);
	}
	i = 0;

	if (hflag) {
		if (headers)
			printf("Hosts on %s:\n", hostname);
		while (dumplist) {
			dumpv[i++] = dumplist->ml_hostname;
			dumplist = dumplist->ml_next;
		}
	}
	else if (aflag) {
		if (headers)
			printf("All mount points on %s:\n", hostname);
		while (dumplist) {
			char *t;

			t=malloc(strlen(dumplist->ml_hostname)+strlen(dumplist->ml_directory)+2);
			if (!t)
			{
				fprintf(stderr, "%s: out of memory\n", program_name);
				exit(1);
			}
			sprintf(t, "%s:%s", dumplist->ml_hostname, dumplist->ml_directory);
			dumpv[i++] = t;
			dumplist = dumplist->ml_next;
		}
	}
	else if (dflag) {
		if (headers)
			printf("Directories on %s:\n", hostname);
		while (dumplist) {
			dumpv[i++] = dumplist->ml_directory;
			dumplist = dumplist->ml_next;
		}
	}

	qsort(dumpv, n, sizeof (char *), dump_cmp);
	
	for (i = 0; i < n; i++) {
		if (i == 0 || strcmp(dumpv[i], dumpv[i - 1]) != 0)
			printf("%s\n", dumpv[i]);
	}
	exit(0);
}

