/*
 * $Id: rxml.c 1626 2007-08-05 04:43:25Z rpedde $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daapd.h"
#include "io.h"
#include "rxml.h"

/* Typedefs/Defines */

#define RXML_ERROR(a,b) { (a)->ecode=(b); return 0; };
#define RXML_MAX_LINE 1024
#define RXML_MAX_TEXT 1024
#define RXML_MAX_TAG 256

typedef struct _RXML {
    RXML_EVTHANDLER handler;
    char *fname;
    IOHANDLE hfile;
    void *udata;
    int ecode;
    int line;
    char *estring;
} RXML;


#ifndef FALSE
# define FALSE 0
# define TRUE  1
#endif /* FALSE */

#define E_RXML_SUCCESS    0x00
#define E_RXML_OPEN       0x01 | 0x80
#define E_RXML_READ       0x02 | 0x80
#define E_RXML_NEST       0x03
#define E_RXML_SPLIT      0x04
#define E_RXML_CLOSE      0x05
#define E_RXML_TAGSIZE    0x06
#define E_RXML_ENTITY     0x07
#define E_RXML_MALLOC     0x08

char *rxml_estrings[] = {
    "Success",
    "Could not open xml file: ",
    "Error reading file: ",
    "Parse error: Nested tags",
    "Parse error: Tag split over multiple lines",
    "Parse error: Unexpected '>'",
    "Parse error: tag too big",
    "Parse error: bad entity encoding",
    "Internal malloc error",
    NULL
};

/* Forwards */
static int rxml_decode_string(char *string);

/**
 * decode an xml entity-encoded string in-place
 *
 * @param string string to encode
 * @returns 1 on success, 0 on error
 */
int rxml_decode_string(char *string) {
    char *src, *dst;
    int len;
    int cval;

    src=dst=string;

    while(*src) {
        if((*src) == '&') {
            len = (int)strlen(src);
            if((len > 3) && (strncmp(src,"&gt;",4) == 0)) {
                *dst++ = '>';
                src += 4;
            } else if((len > 3) && (strncmp(src,"&lt;",4) == 0)) {
                *dst++ = '<';
                src += 4;
            } else if((len > 4) && (strncmp(src,"&amp;",5) == 0)) {
                *dst++ = '&';
                src += 5;
            } else if((len > 5) && (strncmp(src,"&quot;",6) == 0)) {
                *dst++ = '"';
                src += 6;
            } else if((len > 5) && (strncmp(src,"&apos;",6) == 0)) {
                *dst ++ = '\'';
                src += 6;
            } else {
                /* &#xx; */
                if(!sscanf((char*)&src[2],"%d;",&cval))
                    return FALSE;

                *dst++ = cval;
                if(src[3] == ';') {
                    src += 4;
                } else if(src[4] == ';') {
                    src += 5;
                } else if(src[5] == ';') {
                    src += 6;
                } else {
                    return FALSE;
                }
            }
        } else {
            *dst++=*src++;
        }
    }

    *dst = '\0';
    return TRUE;
}

/**
 * open a file
 *
 * @param vp pointer to the opaque xml struct
 * @param file filename of file to open
 * @param handler handler function for events
 * @param udata opaque data structure to pass to the callback
 */
int rxml_open(RXMLHANDLE *vp, char *file,
              RXML_EVTHANDLER handler, void *udata) {
    RXML *pnew;

    pnew=(RXML*)malloc(sizeof(RXML));
    if(!pnew) {
        *vp = NULL;
        return FALSE;
    }

    memset(pnew,0x0,sizeof(RXML));
    *vp = pnew;

    pnew->handler = handler;
    pnew->fname = file;
    pnew->hfile = io_new();

    if(!pnew->hfile)
        RXML_ERROR(pnew,E_RXML_MALLOC);

    if(!io_open(pnew->hfile, "file://%U?ascii=1", file)) {
        io_dispose(pnew->hfile);
        pnew->hfile = NULL;
        RXML_ERROR(pnew,E_RXML_OPEN);   
    }

    io_buffer(pnew->hfile);
    pnew->udata = udata;
    pnew->line = 0;

    if(pnew->handler)
        pnew->handler(RXML_EVT_OPEN, pnew->udata, pnew->fname);

    return TRUE;
}

/**
 * close the currently open file
 *
 * @param vp xml struct containing info about the file to close
 */
int rxml_close(RXMLHANDLE vp) {
    RXML *ph = (RXML*)vp;

    if(ph->handler) ph->handler(RXML_EVT_CLOSE,ph->udata,ph->fname);
    if(ph->hfile) {
        io_close(ph->hfile);
        io_dispose(ph->hfile);
    }
    if(ph->estring) free(ph->estring);

    free(ph);
    return TRUE;
}

/**
 * return a string describing the last error
 *
 * @param vp handle of the document with the error
 */
char *rxml_errorstring(RXMLHANDLE vp) {
    RXML *ph = (RXML*)vp;
    int len;
    char *estring=NULL;

    if(!ph) {
        return "Malloc error";
    }

    if(ph->estring) free(ph->estring);

    len = (int)strlen(rxml_estrings[ph->ecode]) + 16;
    if((ph->ecode & 0x80)) {
        estring=io_errstr(ph->hfile);
        len += (int)strlen(estring);
    }

    ph->estring=(char*)malloc(len);
    if(!ph->estring)
        return "Double-fault malloc error";


    if(((ph->ecode & 0x80) && (ph->hfile))) {
        snprintf(ph->estring,len,"%s%s",rxml_estrings[ph->ecode],io_errstr(ph->hfile));
    } else {
        if(strncmp(rxml_estrings[ph->ecode],"Parse",5)==0) {
            snprintf(ph->estring, len, "%s (Line:%d)",
                     rxml_estrings[ph->ecode], ph->line);
        } else {
            snprintf(ph->estring,len, "%s", rxml_estrings[ph->ecode]);
        }
    }

    return ph->estring;
}

/**
 * walk through the xml file, sending events to the
 * callback handler.
 *
 * @param vp opaque doc struct of the doc to parse
 */
int rxml_parse(RXMLHANDLE vp) {
    char linebuffer[RXML_MAX_LINE];
    char tagbuffer[RXML_MAX_TAG];
    char textbuffer[RXML_MAX_TEXT];
    int in_text=0;
    int text_offset=0;
    int size;
    int offset;
    int in_tag=0;
    int tag_end=0;
    int tag_start=0;
    int single_tag;
    RXML *ph = (RXML*)vp;
    uint32_t len;

    ph->line = 0;

    textbuffer[0] = '\0';

    len = sizeof(linebuffer);
    
    /* walk through and read row by row */
    while(io_readline(ph->hfile,(unsigned char *)linebuffer,&len) && len) {
        ph->line++;
        offset=0;
        size=(int)strlen(linebuffer);
        in_text=0;
        text_offset=0;
        while(offset < size) {
            switch(linebuffer[offset]) {
            case '<':
                if(in_tag)
                    RXML_ERROR(ph, E_RXML_NEST);
            
                in_tag=TRUE;
                tag_start=offset+1;
                tag_end=FALSE;
                if(linebuffer[tag_start] == '/') {
                    tag_end = TRUE;
                    offset++;
                    tag_start++;
                }
            
                offset++;
            
                in_text=0;
                break;

            case '>':
                if(!in_tag)
                    RXML_ERROR(ph, E_RXML_CLOSE);

                in_tag=FALSE;
                if((offset - tag_start) > RXML_MAX_TAG)
                    RXML_ERROR(ph, E_RXML_TAGSIZE);

                strncpy(tagbuffer,&linebuffer[tag_start],offset-tag_start);
                tagbuffer[offset-tag_start] = '\0';

                if(tag_end) {
                    /* send the text before the tag end */
                    if((ph->handler) && (strlen(textbuffer))) {
                        if(!rxml_decode_string(textbuffer))
                            RXML_ERROR(ph,E_RXML_ENTITY);

                        ph->handler(RXML_EVT_TEXT,ph->udata,textbuffer);
                    }
                }

                in_text=1;
                text_offset=0;
                textbuffer[0] = '\0';

                single_tag=0;
                if(tagbuffer[strlen(tagbuffer)-1] == '/') {
                    tagbuffer[strlen(tagbuffer)-1] = '\0';
                    single_tag=1;
                }

                if(ph->handler)
                    ph->handler(tag_end ? RXML_EVT_END : RXML_EVT_BEGIN,
                                ph->udata,tagbuffer);

                /* send a follow-up end on a <tag/> - style tag */
                if((single_tag) && (ph->handler))
                    ph->handler(RXML_EVT_END,ph->udata,tagbuffer);

                offset++;
                break;

            default:
                if((in_text) && (text_offset < (sizeof(textbuffer)-1))) {
                    /* get rid of EOL */
                    if((linebuffer[offset] != '\r') &&
                       (linebuffer[offset] != '\n')) {
                        textbuffer[text_offset] = linebuffer[offset];
                        text_offset++;
                        textbuffer[text_offset] = '\x0';
                    }
                } else if (in_text) {
                    /* should warn of an overflow */
                }
                offset++;
                break;
            }
        }
        len = sizeof(linebuffer);
    }

    if(len)
        RXML_ERROR(ph,E_RXML_READ);

    return TRUE;
}
