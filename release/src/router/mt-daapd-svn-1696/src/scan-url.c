/*
 * $Id: scan-url.c 1626 2007-08-05 04:43:25Z rpedde $
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daapd.h"
#include "err.h"
#include "mp3-scanner.h"

/**
 * Get info from a "url" file -- a media stream file.
 * This should really get more metainfo, but I'll leave that
 * to later.
 *
 * @param filename .url file to process
 * @param pmp3 MP3FILE structure that must be filled
 * @returns TRUE if file should be added to db, FALSE otherwise
 */
int scan_get_urlinfo(char *filename, MP3FILE *pmp3) {
    IOHANDLE hfile;
    char *head, *tail;
    char linebuffer[256];
    uint32_t len;

    DPRINTF(E_DBG,L_SCAN,"Getting URL file info\n");

    if(!(hfile = io_new())) {
        DPRINTF(E_LOG,L_SCAN,"Can't create file handle\n");
        return FALSE;
    }
    
    if(!io_open(hfile,"file://%U?ascii=1",filename)) {
        DPRINTF(E_WARN,L_SCAN,"Could not open %s for reading: %s\n",filename,
            io_errstr(hfile));
        io_dispose(hfile);
        return FALSE;
    }

    io_buffer(hfile);
    len = sizeof(linebuffer);
    if(!io_readline(hfile,(unsigned char *)linebuffer,&len)) {
        DPRINTF(E_WARN,L_SCAN,"Error reading from file %s: %s",filename,io_errstr(hfile));
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    while((linebuffer[strlen(linebuffer)-1] == '\n') ||
          (linebuffer[strlen(linebuffer)-1] == '\r')) {
        linebuffer[strlen(linebuffer)-1] = '\0';
    }

    head=linebuffer;
    tail=strchr(head,',');
    if(!tail) {
        DPRINTF(E_LOG,L_SCAN,"Badly formatted .url file - must be bitrate,descr,url\n");
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    pmp3->bitrate=atoi(head);
    head=++tail;
    tail=strchr(head,',');
    if(!tail) {
        DPRINTF(E_LOG,L_SCAN,"Badly formatted .url file - must be bitrate,descr,url\n");
        io_close(hfile);
        io_dispose(hfile);
        return FALSE;
    }

    *tail++='\0';

    pmp3->title=strdup(head);
    pmp3->url=strdup(tail);
    
    io_close(hfile);
    io_dispose(hfile);

    DPRINTF(E_DBG,L_SCAN,"  Title:    %s\n",pmp3->title);
    DPRINTF(E_DBG,L_SCAN,"  Bitrate:  %d\n",pmp3->bitrate);
    DPRINTF(E_DBG,L_SCAN,"  URL:      %s\n",pmp3->url);

    return TRUE;
}

