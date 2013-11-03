/*
 * Copyright (c) 2010 Mark Williams
 *
 * File change event API for netatalk
 *
 * for every detected filesystem change a UDP packet is sent to an arbitrary list
 * of listeners. Each packet contains unix path of modified filesystem element,
 * event reason, and a consecutive event id (32 bit). Technically we are UDP client and are sending
 * out packets synchronuosly as they are created by the afp functions. This should not affect
 * performance measurably. The only delaying calls occur during initialization, if we have to
 * resolve non-IP hostnames to IP. All numeric data inside the packet is network byte order, so use
 * ntohs / ntohl to resolve length and event id. Ideally a listener receives every packet with
 * no gaps in event ids, starting with event id 1 and mode FCE_CONN_START followed by
 * data events from id 2 up to 0xFFFFFFFF, followed by 0 to 0xFFFFFFFF and so on.
 *
 * A gap or not starting with 1 mode FCE_CONN_START or receiving mode FCE_CONN_BROKEN means that
 * the listener has lost at least one filesystem event
 * 
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/unix.h>
#include <atalk/fce_api.h>
#include <atalk/globals.h>

#include "fork.h"
#include "file.h"
#include "directory.h"
#include "desktop.h"
#include "volume.h"

// ONLY USED IN THIS FILE
#include "fce_api_internal.h"

/* We store our connection data here */
static uint32_t coalesce = 0;
static struct fce_history fce_history_list[FCE_HISTORY_LEN];

/****
* With coalesce we try to reduce the events over UDP, the eventlistener would throw these 
* events away anyway.
* This works only, if the connected listener uses the events on a "per directory" base
* It is a very simple aproach, but saves a lot of events sent to listeners.
* Every "child element" event is ignored as long as its parent event is not older 
* than MAX_COALESCE_TIME_MS ms. If large directory trees or large files are created or deleted, 
* this probably will not work recursive, because the time to copy data will exceed this 
* event timeout. 
* 
****/

static long get_ms_difftime (  struct timeval *tv1, struct timeval *tv2 )
{
	unsigned long s = tv2->tv_sec - tv1->tv_sec;
	long us = tv2->tv_usec - tv1->tv_usec;

	return s * 1000 + us/1000;
}

/******************************************************************************
 * Public functions follow
 ******************************************************************************/

void fce_initialize_history()
{
	for (int i = 0; i < FCE_HISTORY_LEN; i++) {
		memset( &fce_history_list[i], 0, sizeof(fce_history_list[i]) );
	}
}

bool fce_handle_coalescation(int event, const char *path, fce_obj_t type)
{
	/* These two are used to eval our next index in history */
	/* the history is unsorted, speed should not be a problem, length is 10 */
	unsigned long oldest_entry = (unsigned long )((long)-1);
	int oldest_entry_idx = -1;
	struct timeval tv;

	if (coalesce == 0)
		return false;

	/* After a file creation *ALWAYS* a file modification is produced */
	if ((event == FCE_FILE_CREATE) && (coalesce & FCE_COALESCE_CREATE))
        return true;

	/* get timestamp */
	gettimeofday(&tv, 0);

	/* Now detect events in the very near history */
	for (int i = 0; i < FCE_HISTORY_LEN; i++) {
		struct fce_history *fh = &fce_history_list[i];

		/* Not inited ? */
		if (fh->fce_h_tv.tv_sec == 0) {
			/* we can use it for new elements */
			oldest_entry = 0;
			oldest_entry_idx = i;
			continue;
		}

		/* Too old ? */
		if (get_ms_difftime(&fh->fce_h_tv, &tv ) > MAX_COALESCE_TIME_MS) {
			/* Invalidate entry */
			fh->fce_h_tv.tv_sec = 0;
			oldest_entry = 0;
			oldest_entry_idx = i;			
			continue;
		}


		/* If we find a parent dir wich was created we are done */
		if ((coalesce & FCE_COALESCE_CREATE) && (fh->fce_h_event == FCE_DIR_CREATE)) {
			/* Parent dir ? */
			if (!strncmp(fh->fce_h_path, path, strlen(fh->fce_h_path)))
				return true;
		}

		/* If we find a parent dir we should be DELETED we are done */
		if ((coalesce & FCE_COALESCE_DELETE)
            && fh->fce_h_type
            && (event == FCE_FILE_DELETE || event == FCE_DIR_DELETE)) {
			/* Parent dir ? */
			if (!strncmp(fh->fce_h_path, path, strlen(fh->fce_h_path)))
				return true;
		}

		/* Detect oldest entry for next new entry */
		if (oldest_entry_idx == -1 || fh->fce_h_tv.tv_sec < oldest_entry) {
			oldest_entry = fh->fce_h_tv.tv_sec;
			oldest_entry_idx = i;
		}
	}

	/* We have a new entry for the history, register it */
	fce_history_list[oldest_entry_idx].fce_h_tv = tv;
	fce_history_list[oldest_entry_idx].fce_h_event = event;
	fce_history_list[oldest_entry_idx].fce_h_type = type;
	strncpy(fce_history_list[oldest_entry_idx].fce_h_path, path, MAXPATHLEN);

	/* we have to handle this event */
	return false;
}

/*
 * Set event coalescation to reduce number of events sent over UDP 
 * all|delete|create
 */

int fce_set_coalesce(const char *opt)
{
    char *e;
    char *p;
    
    if (opt == NULL)
        return AFPERR_PARAM;

    e = strdup(opt);

    for (p = strtok(e, ","); p; p = strtok(NULL, ",")) {
        if (strcmp(p, "all") == 0) {
            coalesce = FCE_COALESCE_ALL;
        } else if (strcmp(p, "delete") == 0) {
            coalesce = FCE_COALESCE_DELETE;
        } else if (strcmp(p, "create") == 0) {
            coalesce = FCE_COALESCE_CREATE;
        }
    }

    free(e);

    return AFP_OK;
}




