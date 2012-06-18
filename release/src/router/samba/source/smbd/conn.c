/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Manage connections_struct structures
   Copyright (C) Andrew Tridgell 1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

extern int DEBUGLEVEL;

/* set these to define the limits of the server. NOTE These are on a
   per-client basis. Thus any one machine can't connect to more than
   MAX_CONNECTIONS services, but any number of machines may connect at
   one time. */
#define MAX_CONNECTIONS 128

static connection_struct *Connections;

/* number of open connections */
static struct bitmap *bmap;
static int num_open;

/****************************************************************************
init the conn structures
****************************************************************************/
void conn_init(void)
{
	bmap = bitmap_allocate(MAX_CONNECTIONS);
}

/****************************************************************************
return the number of open connections
****************************************************************************/
int conn_num_open(void)
{
	return num_open;
}


/****************************************************************************
check if a snum is in use
****************************************************************************/
BOOL conn_snum_used(int snum)
{
	connection_struct *conn;
	for (conn=Connections;conn;conn=conn->next) {
		if (conn->service == snum) {
			return(True);
		}
	}
	return(False);
}


/****************************************************************************
find a conn given a cnum
****************************************************************************/
connection_struct *conn_find(int cnum)
{
	int count=0;
	connection_struct *conn;

	for (conn=Connections;conn;conn=conn->next,count++) {
		if (conn->cnum == cnum) {
			if (count > 10) {
				DLIST_PROMOTE(Connections, conn);
			}
			return conn;
		}
	}

	return NULL;
}


/****************************************************************************
  find first available connection slot, starting from a random position.
The randomisation stops problems with the server dieing and clients
thinking the server is still available.
****************************************************************************/
connection_struct *conn_new(void)
{
	connection_struct *conn;
	int i;

	i = bitmap_find(bmap, 1);
	
	if (i == -1) {
		DEBUG(1,("ERROR! Out of connection structures\n"));	       
		return NULL;
	}

	conn = (connection_struct *)malloc(sizeof(*conn));
	if (!conn) return NULL;

	ZERO_STRUCTP(conn);
	conn->cnum = i;

	bitmap_set(bmap, i);

	num_open++;

	string_set(&conn->user,"");
	string_set(&conn->dirpath,"");
	string_set(&conn->connectpath,"");
	string_set(&conn->origpath,"");
	
	DLIST_ADD(Connections, conn);

	return conn;
}

/****************************************************************************
close all conn structures
****************************************************************************/
void conn_close_all(void)
{
	connection_struct *conn, *next;
	for (conn=Connections;conn;conn=next) {
		next=conn->next;
		close_cnum(conn, (uint16)-1);
	}
}

/****************************************************************************
idle inactive connections
****************************************************************************/
BOOL conn_idle_all(time_t t, int deadtime)
{
	BOOL allidle = True;
	connection_struct *conn, *next;

	for (conn=Connections;conn;conn=next) {
		next=conn->next;
		/* close dirptrs on connections that are idle */
		if ((t-conn->lastused) > DPTR_IDLE_TIMEOUT)
			dptr_idlecnum(conn);

		if (conn->num_files_open > 0 || 
		    (t-conn->lastused)<deadtime)
			allidle = False;
	}

	return allidle;
}

/****************************************************************************
free a conn structure
****************************************************************************/
void conn_free(connection_struct *conn)
{
	DLIST_REMOVE(Connections, conn);

	if (conn->ngroups && conn->groups) {
		free(conn->groups);
		conn->groups = NULL;
		conn->ngroups = 0;
	}

	free_namearray(conn->veto_list);
	free_namearray(conn->hide_list);
	free_namearray(conn->veto_oplock_list);
	
	string_free(&conn->user);
	string_free(&conn->dirpath);
	string_free(&conn->connectpath);
	string_free(&conn->origpath);

	bitmap_clear(bmap, conn->cnum);
	num_open--;

	ZERO_STRUCTP(conn);
	free(conn);
}
