/*

	cstats
	Copyright (C) 2011-2012 Augusto Bott

	based on rstats
	Copyright (C) 2006-2009 Jonathan Zarate


	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

*/

//#define DEBUG_CSTATS
//#define DEBUG_NOISY
//#define DEBUG_STIME

//#ifdef DEBUG_NOISY
//#define _dprintf(args...)	cprintf(args)
//#define _dprintf(args...)	printf(args)
//#else
//#define _dprintf(args...)	do { } while (0)
//#endif

#define K 1024
#define M (1024 * 1024)
#define G (1024 * 1024 * 1024)

#define SMIN	60
#define	SHOUR	(60 * 60)
#define	SDAY	(60 * 60 * 24)
#define Y2K		946684800UL

#define INTERVAL		120

#define MAX_NSPEED		((24 * SHOUR) / INTERVAL)
#define MAX_NDAILY		62
#define MAX_NMONTHLY	25
#define MAX_ROLLOVER	(225 * M)

#define MAX_COUNTER	2
#define RX 			0
#define TX 			1

#define DAILY		0
#define MONTHLY		1

#define ID_V0		0x30305352
#define ID_V1		0x31305352
#define ID_V2		0x32305352
#define CURRENT_ID	ID_V2

#define HI_BACK		5

const char history_fn[] = "/var/lib/misc/cstats-history";
const char uncomp_fn[] = "/var/tmp/cstats-uncomp";
const char source_fn[] = "/var/lib/misc/cstats-source";

typedef struct {
	int mode;
	int kn;
	FILE *stream;
} node_print_mode_t;

typedef struct {
	uint32_t xtime;
	uint64_t counter[MAX_COUNTER];
} data_t;

typedef struct _Node {
	char ipaddr[INET_ADDRSTRLEN];

	uint32_t id;

	data_t daily[MAX_NDAILY];
	int dailyp;
	data_t monthly[MAX_NMONTHLY];
	int monthlyp;

	long utime;
	unsigned long speed[MAX_NSPEED][MAX_COUNTER];
	unsigned long last[MAX_COUNTER];
	int tail;
	char sync;

	TREE_ENTRY(_Node)	linkage;
} Node;

typedef TREE_HEAD(_Tree, _Node) Tree;

TREE_DEFINE(_Node, linkage);

int Node_compare(Node *lhs, Node *rhs);
Node *Node_new(char *ipaddr);

