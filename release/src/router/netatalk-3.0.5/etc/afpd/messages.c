/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <atalk/afp.h>
#include <atalk/dsi.h>
#include <atalk/util.h>
#include <atalk/unix.h>
#include <atalk/logger.h>
#include <atalk/globals.h>

#include "misc.h"

#define MAXMESGSIZE 199

/* this is only used by afpd children, so it's okay. */
static char servermesg[MAXPATHLEN] = "";
static char localized_message[MAXPATHLEN] = "";

/*!
 * Copy AFP message to message buffer
 * @param message (r) message to send
 * @returns 0 if this message is being set the first time, return 1 if the preceeding
 *          message was the same
 */
int setmessage(const char *message)
{
    if (strncmp(message, servermesg, MAXMESGSIZE) == 0)
        return 1;
    strlcpy(servermesg, message, MAXMESGSIZE);
    return 0;
}

void readmessage(AFPObj *obj)
{
    /* Read server message from file defined as SERVERTEXT */
#ifdef SERVERTEXT
    FILE *message;
    char * filename;
    unsigned int i; 
    int rc;
    static int c;
    uint32_t maxmsgsize;

    maxmsgsize = MIN(MAX(obj->dsi->attn_quantum, MAXMESGSIZE), MAXPATHLEN);

    i=0;
    /* Construct file name SERVERTEXT/message.[pid] */
    if ( NULL == (filename=(char*) malloc(sizeof(SERVERTEXT)+15)) ) {
	LOG(log_error, logtype_afpd, "readmessage: malloc: %s", strerror(errno) );
        return;
    }

    sprintf(filename, "%s/message.%d", SERVERTEXT, getpid());

#ifdef DEBUG
    LOG(log_debug9, logtype_afpd, "Reading file %s ", filename);
#endif 

    message=fopen(filename, "r");
    if (message==NULL) {
        /* try without the process id */
        sprintf(filename, "%s/message", SERVERTEXT);
        message=fopen(filename, "r");
    }

    /* if either message.pid or message exists */
    if (message!=NULL) {
        /* added while loop to get characters and put in servermesg */
        while ((( c=fgetc(message)) != EOF) && (i < (maxmsgsize - 1))) {
            if ( c == '\n')  c = ' ';
            servermesg[i++] = c;
        }
        servermesg[i] = 0;

        /* cleanup */
        fclose(message);

        become_root();

        if ((rc = unlink(filename)) != 0)
	    LOG(log_error, logtype_afpd, "File '%s' could not be deleted", strerror(errno));

        unbecome_root();

        if (rc < 0) {
            LOG(log_error, logtype_afpd, "Error deleting %s: %s", filename, strerror(rc));
        }
#ifdef DEBUG
        else {
            LOG(log_debug9, logtype_afpd, "Deleted %s", filename);
        }

        LOG(log_debug9, logtype_afpd, "Set server message to \"%s\"", servermesg);
#endif
    }
    free(filename);
#endif /* SERVERTEXT */
}

int afp_getsrvrmesg(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    char *message;
    uint16_t type, bitmap;
    uint16_t msgsize;
    size_t outlen = 0;
    size_t msglen = 0;
    int utf8 = 0;

    *rbuflen = 0;

    msgsize = MAX(obj->dsi->attn_quantum, MAXMESGSIZE);

    memcpy(&type, ibuf + 2, sizeof(type));
    memcpy(&bitmap, ibuf + 4, sizeof(bitmap));

    message = servermesg;
    switch (ntohs(type)) {
    case AFPMESG_LOGIN: /* login */
        /* at least TIGER loses server messages
         * if it receives a server msg attention before
         * it has asked the login msg...
         * Workaround: concatenate the two if any, ugly.
         */
        if (obj->options.loginmesg) {
            if (*message)
                strlcat(message, " - ", MAXMESGSIZE);
            strlcat(message, obj->options.loginmesg, MAXMESGSIZE);
        }
        break;
    case AFPMESG_SERVER: /* server */
        break;
    default:
        return AFPERR_BITMAP;
    }

    /* output format:
     * message type:   2 bytes
     * bitmap:         2 bytes
     * message length: 1 byte ( 2 bytes for utf8)
     * message:        up to 199 bytes (dsi attn_quantum for utf8)
     */
    memcpy(rbuf, &type, sizeof(type));
    rbuf += sizeof(type);
    *rbuflen += sizeof(type);
    memcpy(rbuf, &bitmap, sizeof(bitmap));
    rbuf += sizeof(bitmap);
    *rbuflen += sizeof(bitmap);

    utf8 = ntohs(bitmap) & 2; 
    msglen = strlen(message);
    if (msglen > msgsize)
        msglen = msgsize;

    if (msglen) { 
        if ( (size_t)-1 == (outlen = convert_string(obj->options.unixcharset, utf8?CH_UTF8_MAC:obj->options.maccharset,
                                                    message, msglen, localized_message, msgsize)) )
        {
    	    memcpy(rbuf+((utf8)?2:1), message, msglen); /*FIXME*/
	    outlen = msglen;
        }
        else
        {
	    memcpy(rbuf+((utf8)?2:1), localized_message, outlen);
        }
    }
    
    if ( utf8 ) {
	/* UTF8 message, 2 byte length */
	msgsize = htons(outlen);
    	memcpy(rbuf, &msgsize, sizeof(msgsize));
	*rbuflen += sizeof(msgsize);
    }
    else {
        *rbuf = outlen;
	*rbuflen += 1;
    }
    *rbuflen += outlen;
//    *message = 0;
    return AFP_OK;
}
