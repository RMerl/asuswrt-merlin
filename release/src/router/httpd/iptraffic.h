/*

	IPTraffic monitoring extensions for Tomato
	Copyright (C) 2011-2012 Augusto Bott

	Tomato Firmware
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

#include <tree.h>

void iptraffic_conntrack_init();

typedef struct _Node {
	char ipaddr[INET_ADDRSTRLEN];
	unsigned int tcp_conn;
	unsigned int udp_conn;

	TREE_ENTRY(_Node)	linkage;
} Node;

typedef TREE_HEAD(_Tree, _Node) Tree;

TREE_DEFINE(_Node, linkage);

Node *Node_new(char *ipaddr) {
	Node *self;
	if ((self = malloc(sizeof(Node))) != NULL) {
		memset(self, 0, sizeof(Node));
		strncpy(self->ipaddr, ipaddr, INET_ADDRSTRLEN);
		self->tcp_conn = 0;
		self->udp_conn = 0;
//		printf("+++ %s: new node ip=%s, sizeof(Node)=%d (bytes)\n", __FUNCTION__, self->ipaddr, sizeof(Node));
	}
	return self;
}

int Node_compare(Node *lhs, Node *rhs) {
	return strncmp(lhs->ipaddr, rhs->ipaddr, INET_ADDRSTRLEN);
}

Tree tree = TREE_INITIALIZER(Node_compare);

void Node_housekeeping(Node *self, void *info) {
//	printf("--- freed: %s\n", self->ipaddr);
	free(self);
}

// DEBUG

void Node_print(Node *self, FILE *stream) {
	fprintf(stream, "%s/%d/%d", self->ipaddr, self->tcp_conn, self->udp_conn);
}

void Node_printer(Node *self, void *stream) {
	Node_print(self, (FILE *)stream);
	fprintf((FILE *)stream, " ");
}

void Tree_info(void) {
	printf("Tree = ");
	TREE_FORWARD_APPLY(&tree, _Node, linkage, Node_printer, stdout);
	printf("\n");
	printf("Tree depth = %d\n", TREE_DEPTH(&tree, linkage));
}


