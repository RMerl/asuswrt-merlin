/*
 * $Id: xml-rpc.c 970 2006-04-20 06:52:21Z rpedde $
 *
 * This really isn't xmlrpc.  It's xmlrpc-ish.  Emphasis on -ish.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "compat.h"
#include "ff-dbstruct.h"
#include "ff-plugins.h"
#include "rsp.h"
#include "xml-rpc.h"


/* typedefs */
typedef struct tag_xmlstack {
    char *tag;
    struct tag_xmlstack *next;
} XMLSTACK;

#define XML_STREAM_BLOCK 4096
typedef struct tag_xml_streambuffer {
    z_stream strm;
    unsigned char *in_buffer;
    unsigned char *out_buffer;
} XML_STREAMBUFFER;

struct tag_xmlstruct {
    WS_CONNINFO *pwsc;
    int stack_level;
    XMLSTACK stack;
    XML_STREAMBUFFER *psb;
};

/* Forwards */
void xml_get_stats(WS_CONNINFO *pwsc);
void xml_set_config(WS_CONNINFO *pwsc);
void xml_return_error(WS_CONNINFO *pwsc, int errno, char *errstr);
char *xml_entity_encode(char *original);

XML_STREAMBUFFER *xml_stream_open(void);
int xml_stream_write(XMLSTRUCT *pxml, char *out);
int xml_stream_close(XMLSTRUCT *pxml);

int xml_write(XMLSTRUCT *pxml, char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    int result=0;

    va_start(ap, fmt);
    vsnprintf(buffer, 1024, fmt, ap);
    va_end(ap);

    if(pxml->psb) {
        result=xml_stream_write(pxml, buffer);
        if(!result)
            result = -1;
    } else {
        result=pi_ws_writefd(pxml->pwsc,"%s",buffer);
    }

    return result;
}

void xml_return_error(WS_CONNINFO *pwsc, int errno, char *errstr) {
    XMLSTRUCT *pxml;

    pxml=xml_init(pwsc,TRUE);
    xml_push(pxml,"results");

    xml_output(pxml,"status","%d",errno);
    xml_output(pxml,"statusstring","%s",errstr);

    xml_pop(pxml); /* results */
    xml_deinit(pxml);
    return;
}


/**
 * open a gzip stream
 */
XML_STREAMBUFFER *xml_stream_open(void) {
    XML_STREAMBUFFER *psb;

    psb = (XML_STREAMBUFFER*) malloc(sizeof(XML_STREAMBUFFER));
    if(!psb) {
        pi_log(E_FATAL,"xml_stream_open: malloc\n");
    }

    psb->out_buffer = (unsigned char*) malloc(XML_STREAM_BLOCK);
    psb->in_buffer = (unsigned char*) malloc(XML_STREAM_BLOCK);

    if((!psb->out_buffer) || (!psb->in_buffer)) {
        pi_log(E_FATAL,"xml_stream_open: malloc\n");
    }

    psb->strm.zalloc = Z_NULL;
    psb->strm.zfree = Z_NULL;
    psb->strm.opaque = Z_NULL;

    psb->strm.next_in = psb->in_buffer;
    psb->strm.next_out = psb->out_buffer;

    deflateInit2(&psb->strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                 24, 8, Z_DEFAULT_STRATEGY);
    return psb;
}

/**
 * write a block to the stream
 */
int xml_stream_write(XMLSTRUCT *pxml, char *out) {
    int done = 0;
    int result;
    XML_STREAMBUFFER *psb = pxml->psb;

    if((!out)||(!strlen(out)))
        return TRUE;

    if(strlen(out) > 1024)
        return TRUE;

    memcpy(psb->in_buffer,out,(int)strlen(out));
    psb->strm.avail_in = (int)strlen(out);
    psb->strm.next_in = psb->in_buffer;
    psb->strm.next_out = psb->out_buffer;
    psb->strm.avail_out = XML_STREAM_BLOCK;

    while(!done) {
        result = deflate(&psb->strm, Z_NO_FLUSH);
        if(result != Z_OK) {
            pi_log(E_FATAL,"Error in zlib: %d\n",result);
        }
        pi_ws_writebinary(pxml->pwsc,(char*)psb->out_buffer,
                             XML_STREAM_BLOCK-psb->strm.avail_out);
        if(psb->strm.avail_out != 0) {
            done=1;
        } else {
            psb->strm.avail_out = XML_STREAM_BLOCK;
            psb->strm.next_out = psb->out_buffer;
        }
    }
    return TRUE;
}

/**
 * close the stream
 */
int xml_stream_close(XMLSTRUCT *pxml) {
    int done = 0;
    XML_STREAMBUFFER *psb = pxml->psb;

    /* flush what's left */
    while(!done) {
        psb->strm.avail_out = XML_STREAM_BLOCK;
        psb->strm.next_out = psb->out_buffer;
        psb->strm.avail_in = 0;
        psb->strm.next_in = psb->in_buffer;

        deflate(&psb->strm,Z_FINISH);
        pi_ws_writebinary(pxml->pwsc,(char*)psb->out_buffer,
                             XML_STREAM_BLOCK - psb->strm.avail_out);

        if(psb->strm.avail_out != 0)
            done=1;
    }

    pi_log(E_DBG,"Done sending xml stream\n");
    deflateEnd(&psb->strm);
    if(psb->out_buffer != NULL)
        free(psb->out_buffer);
    if(psb->in_buffer != NULL)
        free(psb->in_buffer);
    free(psb);

    return TRUE;
}

/**
 * create an xml response structure, a helper struct for
 * building xml responses.
 *
 * @param pwsc the pwsc we are emitting to
 * @param emit_header whether or not to throw out html headers and xml header
 * @returns XMLSTRUCT on success, or NULL if failure
 */
XMLSTRUCT *xml_init(WS_CONNINFO *pwsc, int emit_header) {
    XMLSTRUCT *pxml;
    char *accept;
    char *nogzip;

    pxml=(XMLSTRUCT*)malloc(sizeof(XMLSTRUCT));
    if(!pxml) {
        pi_log(E_FATAL,"Malloc error\n");
    }

    memset(pxml,0,sizeof(XMLSTRUCT));

    pxml->pwsc = pwsc;

    /* should we compress output? */
    nogzip = pi_ws_getvar(pwsc,"nogzip");
    accept = pi_ws_getrequestheader(pwsc,"accept-encoding");

    if((!nogzip) && (accept) && (strcasestr(accept,"gzip"))) {
        pi_log(E_DBG,"Gzipping output\n");
        pxml->psb = xml_stream_open();
        if(pxml->psb) {
            pi_ws_addresponseheader(pwsc,"Content-Encoding","gzip");
            pi_ws_addresponseheader(pwsc,"Vary","Accept-Encoding");
            pi_ws_addresponseheader(pwsc,"Connection","Close");
        }
    }

    /* the world would be a wonderful place without ie */
    pi_ws_addresponseheader(pwsc,"Cache-Control","no-cache");
    pi_ws_addresponseheader(pwsc,"Expires","-1");

    if(emit_header) {
        pi_ws_addresponseheader(pwsc,"Content-Type","text/xml; charset=utf-8");
        pi_ws_writefd(pwsc,"HTTP/1.0 200 OK\r\n");
        pi_ws_emitheaders(pwsc);


        xml_write(pxml,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
    }

    return pxml;
}


/**
 * push a new term on the stack
 *
 * @param pxml xml struct obtained from xml_init
 * @param term next xlm section to start
 */
void xml_push(XMLSTRUCT *pxml, char *term) {
    XMLSTACK *pstack;

    pstack = (XMLSTACK *)malloc(sizeof(XMLSTACK));
    pstack->next=pxml->stack.next;
    pstack->tag=strdup(term);
    pxml->stack.next=pstack;

    pxml->stack_level++;

    xml_write(pxml,"<%s>",term);
}

/**
 * end an xml section
 *
 * @param pxml xml struct we are working with
 */
void xml_pop(XMLSTRUCT *pxml) {
    XMLSTACK *pstack;

    pstack=pxml->stack.next;
    if(!pstack) {
        pi_log(E_LOG,"xml_pop: tried to pop an empty stack\n");
        return;
    }

    pxml->stack.next = pstack->next;

    xml_write(pxml,"</%s>",pstack->tag);
    free(pstack->tag);
    free(pstack);

    pxml->stack_level--;
}

/* FIXME: Fixed at 256?  And can't I get an expandable sprintf/cat? */

/**
 * output a string
 */
int xml_output(XMLSTRUCT *pxml, char *section, char *fmt, ...) {
    va_list ap;
    char buf[256];
    char *output;
    int result=0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    output = xml_entity_encode(buf);
    if(section) {
        xml_push(pxml,section);
    }

    result = xml_write(pxml,"%s",output);
    free(output);
    if(section) {
        xml_pop(pxml);
    }

    return result;
}

/**
 * clean up an xml struct
 *
 * @param pxml xml struct to clean up
 */
void xml_deinit(XMLSTRUCT *pxml) {
    XMLSTACK *pstack;

    if(pxml->stack.next) {
        pi_log(E_LOG,"xml_deinit: entries still on stack (%s)\n",
                pxml->stack.next->tag);
    }

    while((pstack=pxml->stack.next)) {
        pxml->stack.next=pstack->next;
        free(pstack->tag);
        free(pstack);
    }

    if(pxml->psb) {
        xml_stream_close(pxml);
    }

    free(pxml);
}

/**
 * xml entity encoding, stupid style
 */
char *xml_entity_encode(char *original) {
    char *new;
    char *s, *d;
    int destsize;

    destsize = 6*(int)strlen(original)+1;
    new=(char *)malloc(destsize);
    if(!new) return NULL;

    memset(new,0x00,destsize);

    s=original;
    d=new;

    while(*s) {
        switch(*s) {
        case '>':
            strcat(d,"&gt;");
            d += 4;
            s++;
            break;
        case '<':
            strcat(d,"&lt;");
            d += 4;
            s++;
            break;
        case '"':
            strcat(d,"&quot;");
            d += 6;
            s++;
            break;
        case '\'':
            strcat(d,"&apos;");
            d += 6;
            s++;
            break;
        case '&':
            strcat(d,"&amp;");
            d += 5;
            s++;
            break;
        default:
            *d++ = *s++;
        }
    }

    return new;
}
