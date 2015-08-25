/*
 * $Id: out-daap.c 1674 2007-10-07 04:39:30Z rpedde $
 * daap plugin handler and dispatch code
 *
 * Copyright (C) 2003-2006 Ron Pedde (ron@pedde.com)
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
#include <netinet/in.h>
#endif

#include "compat.h"
#include "ff-dbstruct.h"
#include "ff-plugins.h"
#include "out-daap.h"
#include "out-daap-proto.h"

//#undef strsep  /* FIXME */

/**
 * Hold the inf for the output serializer
 */
typedef struct tag_xml_stack {
    char tag[5];
    int bytes_left;
} XML_STACK;

typedef struct tag_output_info {
    int xml_output;
    int readable;
    int browse_response;
    int dmap_response_length;
    int stack_height;
    XML_STACK stack[10];
} OUTPUT_INFO;

/* Forwards */
static void out_daap_server_info(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_login(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_content_codes(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_update(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_dbinfo(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_playlistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_stream(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_browse(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_playlists(WS_CONNINFO *pqsc, PRIVINFO *ppi);
static void out_daap_addplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_addplaylistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_editplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_deleteplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_deleteplaylistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_items(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_logout(WS_CONNINFO *pwsc, PRIVINFO *ppi);
static void out_daap_error(WS_CONNINFO *pwsc, PRIVINFO *ppi, char *container, char *error);
static int out_daap_output_start(WS_CONNINFO *pwsc, PRIVINFO *ppi, int content_length);
static int out_daap_output_write(WS_CONNINFO *pwsc, PRIVINFO *ppi, unsigned char *block, int len);
static int out_daap_output_end(WS_CONNINFO *pwsc, PRIVINFO *ppi);

static DAAP_ITEMS *out_daap_xml_lookup_tag(char *tag);
static char *out_daap_xml_encode(char *original, int len);
static int out_daap_output_xml_write(WS_CONNINFO *pwsc, PRIVINFO *ppi, unsigned char *block, int len);

static void out_daap_cleanup(PRIVINFO *ppi);

void plugin_handler(WS_CONNINFO *pwsc);
int plugin_can_handle(WS_CONNINFO *pwsc);
int plugin_auth(WS_CONNINFO *pwsc, char *username, char *password);

PLUGIN_INFO *plugin_info(void);
PLUGIN_OUTPUT_FN _pofn = { plugin_can_handle, plugin_handler, plugin_auth };
PLUGIN_REND_INFO _pri[] = {
    { "_daap._tcp", NULL },
    { NULL, NULL }
};

PLUGIN_INFO _pi = {
    PLUGIN_VERSION,      /* version */
    PLUGIN_OUTPUT,       /* type */
    "daap/" VERSION,     /* server */
    &_pofn,              /* output fns */
    NULL,                /* event fns */
    NULL,                /* transcode fns */
    _pri,                /* rend info */
    NULL                 /* transcode info */
};


typedef struct tag_response {
    char *uri[10];
    void (*dispatch)(WS_CONNINFO *, PRIVINFO *);
} PLUGIN_RESPONSE;

PLUGIN_RESPONSE daap_uri_map[] = {
    {{"server-info",   NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_server_info },
    {{"content-codes", NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_content_codes },
    {{"login",         NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_login },
    {{"update",        NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_update },
    {{"logout",        NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_logout },
    {{"databases",     NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_dbinfo },
    {{"databases","*","items",  NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_items },
    {{"databases","*","containers",NULL,NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_playlists },
    {{"databases","*","browse","*",NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_browse },
    {{"databases","*","items","*",NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_stream },
    {{"databases","*","containers","add",NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_addplaylist },
    {{"databases","*","containers","del",NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_deleteplaylist },
    {{"databases","*","containers","edit",NULL,NULL,NULL,NULL,NULL,NULL },
     out_daap_editplaylist },
    {{"databases","*","containers","*","items",NULL,NULL,NULL,NULL,NULL },
     out_daap_playlistitems },
    {{"databases","*","containers","*","del",NULL,NULL,NULL,NULL,NULL },
     out_daap_deleteplaylistitems },
    {{"databases","*","containers","*","items","add",NULL,NULL,NULL,NULL },
     out_daap_addplaylistitems },
    {{"databases","*","containers","*","browse","*",NULL,NULL,NULL,NULL },
     out_daap_browse }
};



/**
 * return info about this plugin module
 */
PLUGIN_INFO *plugin_info(void) {
    return &_pi;
}

/**
 * see if the plugin should handle this request
 */
int plugin_can_handle(WS_CONNINFO *pwsc) {
    char *uri = pi_ws_uri(pwsc);

    pi_log(E_DBG,"Checking url %s\n",uri);
    if(strncasecmp(uri,"/databases",10) == 0)
        return TRUE;
    if(strncasecmp(uri,"/server-info",12) == 0)
        return TRUE;
    if(strncasecmp(uri,"/content-codes",14) == 0)
        return TRUE;
    if(strncasecmp(uri,"/login",6) == 0)
        return TRUE;
    if(strncasecmp(uri,"/update",7) == 0)
        return TRUE;
    if(strncasecmp(uri,"/logout",7) == 0)
        return TRUE;
    if(strncasecmp(uri,"/activity",9) == 0)
        return TRUE;

    return FALSE;
}

/**
 * check for auth.  Kind of a ham-handed implementation, but
 * works.
 */
int plugin_auth(WS_CONNINFO *pwsc, char *username, char *password) {
    char *uri = pi_ws_uri(pwsc);

    /* don't auth for stuff we shouldn't */
    if(strncasecmp(uri,"/server-info",12) == 0)
        return TRUE;
    if(strncasecmp(uri,"/logout",7) == 0)
        return TRUE;
    if(strncasecmp(uri,"/databases/1/items/",19) == 0)
        return TRUE;
    if(strncasecmp(uri,"/activity",9) == 0)
        return TRUE;

    return pi_ws_matchesrole(pwsc,username,password,"user");
}


/**
 * do cleanup on the ppi structure... free any allocated memory, etc
 */
void out_daap_cleanup(PRIVINFO *ppi) {
    if(!ppi)
        return;

    if(ppi->output_info)
        free(ppi->output_info);

    free(ppi);
}

char *_strsep(char **stringp, const char *delim) {
        char *ret = *stringp;
        if (ret == NULL) return(NULL); /* grrr */
        if ((*stringp = strpbrk(*stringp, delim)) != NULL) {
                *((*stringp)++) = '\0';
        }
        return(ret);
}

/**
 * Handles authentication for the daap server.  This isn't the
 * authenticator for the web admin page, but rather the iTunes
 * authentication when trying to connect to the server.  Note that most
 * of this is actually handled in the web server registration, which
 * decides when to apply the authentication or not.  If you mess with
 * when and where the webserver applies auth or not, you'll likely
 * break something.  It seems that some requests must be authed, and others
 * not.  If you apply authentication somewhere that iTunes doesn't expect
 * it, it happily disconnects.
 *
 * @param username The username passed by iTunes
 * @param password The password passed by iTunes
 * @returns 1 if auth successful, 0 otherwise
 */
int daap_auth(WS_CONNINFO *pwsc, char *username, char *password) {
    char *readpassword;
    int result;

    readpassword = pi_conf_alloc_string("general","password",NULL);

    if(password == NULL) {
        if((readpassword == NULL)||(strlen(readpassword) == 0)) {
            result = TRUE;
        } else {
            result = FALSE;
        }
    } else {
        if(strcasecmp(password,readpassword)) {
            result = FALSE;
        } else {
            result = TRUE;
        }
    }

    if(readpassword) pi_conf_dispose_string(readpassword);
    return result;
}

/**
 * dispatch handler for web stuff
 */
void plugin_handler(WS_CONNINFO *pwsc) {
    char *string, *save, *token;
    PRIVINFO *ppi;
    int elements;
    int index, part;
    int found;
    char *index_req = NULL;
    long l,h;
    char *ptr;

    pi_log(E_DBG,"Getting uri...\n");

    string = pi_ws_uri(pwsc);
    string++;

    pi_log(E_DBG,"Mallocing privinfo...\n");
    ppi = (PRIVINFO *)malloc(sizeof(PRIVINFO));
    if(ppi) {
        memset(ppi,0,sizeof(PRIVINFO));
    }

    if(!ppi) {
        pi_ws_returnerror(pwsc,500,"Malloc error in plugin_handler");
        return;
    }

    memset((void*)&ppi->dq,0,sizeof(DB_QUERY));

    ppi->empty_strings = pi_conf_get_int("daap","empty_strings",0);
    ppi->pwsc = pwsc;

    pi_ws_addresponseheader(pwsc,"Accept-Ranges","bytes");
    pi_ws_addresponseheader(pwsc,"DAAP-Server","firefly/" VERSION);
    pi_ws_addresponseheader(pwsc,"Content-Type","application/x-dmap-tagged");
    pi_ws_addresponseheader(pwsc,"Cache-Control","no-cache");
    pi_ws_addresponseheader(pwsc,"Expires","-1");

    if(pi_ws_getvar(pwsc,"session-id"))
        ppi->session_id = atoi(pi_ws_getvar(pwsc,"session-id"));

    ppi->dq.offset = 0;
    ppi->dq.limit = 999999;

    l=h=0;
    if(pi_ws_getvar(pwsc,"index")) {
        index_req = pi_ws_getvar(pwsc,"index");
        l = strtol(index_req,&ptr,10);

        if(l<0) { /* "-h"... tail range, last "h" entries */
            pi_log(E_LOG,"Unsupported index range: %s\n",index_req);
        } else if(*ptr == 0) {
            /* single item */
            ppi->dq.offset = l;
            ppi->dq.limit = 1;
        } else if(*ptr == '-') {
            ppi->dq.offset = l;
            if(*++ptr != '\0') { /* l- */
                h = strtol(ptr, &ptr, 10);
                ppi->dq.limit = (h - l) + 1;
            }
        }

                pi_log(E_DBG,"Index %s: offset %d, limit %d\n",index_req,
                  ppi->dq.offset,ppi->dq.limit);
        }


    if(pi_ws_getvar(pwsc,"query")) {
        ppi->dq.filter_type = FILTER_TYPE_APPLE;
        ppi->dq.filter = pi_ws_getvar(pwsc,"query");
    }

    pi_log(E_DBG,"Tokenizing url\n");
    while((ppi->uri_count < 10) && (token=strtok_r(string,"/",&save))) {
        string=NULL;
        ppi->uri_sections[ppi->uri_count++] = token;
    }

    elements = sizeof(daap_uri_map) / sizeof(PLUGIN_RESPONSE);
    pi_log(E_DBG,"Found %d elements\n",elements);

    index = 0;
    found = 0;

    while((!found) && (index < elements)) {
        /* test this set */
        pi_log(E_DBG,"Checking reponse %d\n",index);
        part=0;
        while(part < 10) {
            if((daap_uri_map[index].uri[part]) && (!ppi->uri_sections[part]))
                break;
            if((ppi->uri_sections[part]) && (!daap_uri_map[index].uri[part]))
                break;

            if((daap_uri_map[index].uri[part]) &&
               (strcmp(daap_uri_map[index].uri[part],"*") != 0)) {
                if(strcmp(daap_uri_map[index].uri[part],
                          ppi->uri_sections[part])!= 0)
                    break;
            }
            part++;
        }

        if(part == 10) {
            found = 1;
            pi_log(E_DBG,"Found it! Index: %d\n",index);
        } else {
            index++;
        }
    }

    if(found) {
        daap_uri_map[index].dispatch(pwsc, ppi);
        out_daap_cleanup(ppi);
        return;
    }

    pi_ws_returnerror(pwsc,400,"Bad request");
    pi_ws_will_close(pwsc);
    out_daap_cleanup(ppi);
    return;
}

/**
 * set up whatever necessary to begin streaming the output
 * to the client.
 *
 * @param pwsc pointer to the current conninfo struct
 * @param ppi pointer to the current dbquery struct
 * @param content_length content_length (assuming dmap) of the output
 */
int out_daap_output_start(WS_CONNINFO *pwsc, PRIVINFO *ppi, int content_length) {
    OUTPUT_INFO *poi;

    poi=(OUTPUT_INFO*)calloc(1,sizeof(OUTPUT_INFO));
    if(!poi) {
        pi_log(E_LOG,"Malloc error in out_daap_ouput_start\n");
        return -1;
    }

    ppi->output_info = (void*) poi;
    poi->dmap_response_length = content_length;

    if(pi_ws_getvar(pwsc,"output")) {
        if(strcasecmp(pi_ws_getvar(pwsc,"output"),"readable") == 0)
            poi->readable=1;

        poi->xml_output=1;
        pi_ws_addresponseheader(pwsc,"Content-Type","text/xml");
        pi_ws_addresponseheader(pwsc,"Connection","Close");
        pi_ws_will_close(pwsc);
        pi_ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
        pi_ws_emitheaders(pwsc);
        pi_ws_writefd(pwsc,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
        if(poi->readable)
            pi_ws_writefd(pwsc,"\n");
        return 0;
    }

    pi_ws_addresponseheader(pwsc,"Content-Length","%d",
                               poi->dmap_response_length);
    pi_ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
    pi_ws_emitheaders(pwsc);

    /* I guess now we would start writing the output */
    return 0;
}

/**
 * write the output to wherever it goes.  This expects to be fed
 * full dmap blocks.  In the simplest case, it just streams those
 * dmap blocks out to the client.  In more complex cases, it convert
 * them to xml, or compresses them.
 *
 * @param ppi pointer to the current dbquery info struct
 * @param pwsc pointer to the current conninfo struct
 * @param pblock block of data to write
 * @param len length of block to write
 */
int out_daap_output_write(WS_CONNINFO *pwsc, PRIVINFO *ppi, unsigned char *block, int len) {
    OUTPUT_INFO *poi=(ppi->output_info);
    int result;

    if(poi->xml_output)
        return out_daap_output_xml_write(pwsc, ppi, block, len);

    result=pi_ws_writebinary(pwsc,(char*)block,len);

    if(result != len)
        return -1;

    return 0;
}

/**
 * this is the serializer for xml.  This assumes that (with the exception of
 * containers) blocks are complete dmap blocks
 *
 * @param ppi pointer to the current dbquery info struct
 * @param pwsc pointer to the current conninfo struct
 * @param pblock block of data to write
 * @param len length of block to write
 */
int out_daap_output_xml_write(WS_CONNINFO *pwsc, PRIVINFO *ppi, unsigned char *block, int len) {
    OUTPUT_INFO *poi = ppi->output_info;
    unsigned char *current=block;
    char block_tag[5];
    int block_len;
    int len_left;
    DAAP_ITEMS *pitem;
    unsigned char *data;
    int ivalue;
    long long lvalue;
    int block_done=1;
    int stack_ptr;
    char *encoded_string;

    while(current < (block + len)) {
        block_done=1;
        len_left=(int)((block+len) - current);
        if(len_left < 8) {
            pi_log(E_FATAL,"Badly formatted dmap block - frag size: %d",len_left);
        }

        /* set up block */
        memcpy(block_tag,current,4);
        block_tag[4] = '\0';
        block_len = current[4] << 24 | current[5] << 16 |
            current[6] << 8 | current[7];
        data = &current[8];

        if(strncmp(block_tag,"abro",4) ==0 ) {
            /* browse queries treat mlit as a string, not container */
            poi->browse_response=1;
        }

        /* lookup and serialize */
        pi_log(E_SPAM,"%*s %s: %d\n",poi->stack_height,"",block_tag,block_len);
        pitem=out_daap_xml_lookup_tag(block_tag);
        if(poi->readable)
            pi_ws_writefd(pwsc,"%*s",poi->stack_height,"");
        pi_ws_writefd(pwsc,"<%s>",pitem->description);
        switch(pitem->type) {
        case 0x01: /* byte */
            if(block_len != 1) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 1\n",block_tag, block_len);
            }
            pi_ws_writefd(pwsc,"%d",*((char *)data));
            break;

        case 0x02: /* unsigned byte */
            if(block_len != 1) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 1\n",block_tag, block_len);
            }
            pi_ws_writefd(pwsc,"%ud",*((char *)data));
            break;

        case 0x03: /* short */
            if(block_len != 2) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 2\n",block_tag, block_len);
            }

            ivalue = data[0] << 8 | data[1];
            pi_ws_writefd(pwsc,"%d",ivalue);
            break;

        case 0x05: /* int */
        case 0x0A: /* epoch */
            if(block_len != 4) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 4\n",block_tag, block_len);
            }
            ivalue = data[0] << 24 |
                data[1] << 16 |
                data[2] << 8 |
                data[3];
            pi_ws_writefd(pwsc,"%d",ivalue);
            break;
        case 0x07: /* long long */
            if(block_len != 8) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 8\n",block_tag, block_len);
            }

            ivalue = data[0] << 24 |
                data[1] << 16 |
                data[2] << 8 |
                data[3];
            lvalue=ivalue;
            ivalue = data[4] << 24 |
                data[5] << 16 |
                data[6] << 8 |
                data[7];
            lvalue = (lvalue << 32) | ivalue;
            pi_ws_writefd(pwsc,"%ll",ivalue);
            break;
        case 0x09: /* string */
            if(block_len) {
                encoded_string=out_daap_xml_encode((char*)data,block_len);
                pi_ws_writefd(pwsc,"%s",encoded_string);
                free(encoded_string);
            }
            break;
        case 0x0B: /* version? */
            if(block_len != 4) {
                pi_log(E_FATAL,"tag %s, size %d, wanted 4\n",block_tag, block_len);
            }

            ivalue=data[0] << 8 | data[1];
            pi_ws_writefd(pwsc,"%d.%d.%d",ivalue,data[2],data[3]);
            break;

        case 0x0C:
            if((poi->browse_response)&&(strcmp(block_tag,"mlit") ==0)) {
                if(block_len) {
                    encoded_string=out_daap_xml_encode((char*)data,block_len);
                    pi_ws_writefd(pwsc,"%s",encoded_string);
                    free(encoded_string);
                }
            } else {
                /* we'll need to stack this up and try and remember where we
                 * came from.  Make it an extra 8 so that it gets fixed to
                 * the *right* amount when the stacks are juggled below
                 */

                poi->stack[poi->stack_height].bytes_left=block_len + 8;
                memcpy(poi->stack[poi->stack_height].tag,block_tag,5);
                poi->stack_height++;
                if(poi->stack_height == 10) {
                    pi_log(E_FATAL,"Stack overflow\n");
                }
                block_done=0;
            }
            break;

        default:
            pi_log(E_FATAL,"Bad dmap type: %d, %s\n",
                    pitem->type, pitem->description);
            break;
        }

        if(block_done) {
            pi_ws_writefd(pwsc,"</%s>",pitem->description);
            if(poi->readable)
                pi_ws_writefd(pwsc,"\n");

            block_len += 8;
        } else {
            /* must be a container */
            block_len = 8;
            if(poi->readable)
                pi_ws_writefd(pwsc,"\n");
        }

        current += block_len;

        if(poi->stack_height) {
            stack_ptr=poi->stack_height;
            while(stack_ptr--) {
                poi->stack[stack_ptr].bytes_left -= block_len;
                if(poi->stack[stack_ptr].bytes_left < 0) {
                    pi_log(E_FATAL,"negative container\n");
                }

                if(!poi->stack[stack_ptr].bytes_left) {
                    poi->stack_height--;
                    pitem=out_daap_xml_lookup_tag(poi->stack[stack_ptr].tag);
                    if(poi->readable)
                        pi_ws_writefd(pwsc,"%*s",poi->stack_height,"");
                    pi_ws_writefd(pwsc,"</%s>",pitem->description);
                    if(poi->readable)
                        pi_ws_writefd(pwsc,"\n");
                }
            }
        }
    }

    return 0;
}


/**
 * finish streaming output to the client, freeing any allocated
 * memory, and cleaning up
 *
 * @param pwsc current conninfo struct
 * @param ppi current dbquery struct
 */
int out_daap_output_end(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    OUTPUT_INFO *poi = ppi->output_info;

    if((poi) && (poi->xml_output) && (poi->stack_height)) {
        pi_log(E_LOG,"Badly formed xml -- still stack\n");
    }

    pi_config_set_status(pwsc,ppi->session_id,NULL);

    return 0;
}


DAAP_ITEMS *out_daap_xml_lookup_tag(char *tag) {
    DAAP_ITEMS *pitem;

    pitem=taglist;
    while((pitem->tag) && (strncmp(tag,pitem->tag,4))) {
        pitem++;
    }

    if(!pitem->tag)
        pi_log(E_FATAL,"Unknown daap tag: %c%c%c%c\n",tag[0],tag[1],tag[2],tag[3]);

    return pitem;
}

/**
 * xml entity encoding, stupid style
 */
char *out_daap_xml_encode(char *original, int len) {
    char *new;
    char *s, *d;
    int destsize;
    int truelen;

    /* this is about stupid */
    if(len) {
        truelen=len;
    } else {
        truelen=(int) strlen(original);
    }

    destsize = 6*truelen+1;
    new=(char *)malloc(destsize);
    if(!new) return NULL;

    memset(new,0x00,destsize);

    s=original;
    d=new;

    while(s < (original+truelen)) {
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

void out_daap_stream(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    /* should show sesson id */
    pi_stream(pwsc, ppi->uri_sections[3]);
}

/**
 * add songs to an existing playlist
 */
void out_daap_addplaylistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char playlist_response[20];
    unsigned char *current;
    char *tempstring;
    char *token;
    int playlist_id;

    playlist_id = atoi(ppi->uri_sections[3]);

    if(!pi_ws_getvar(pwsc,"dmap.itemid")) {
        pi_log(E_LOG,"Attempt to add playlist item w/o dmap.itemid\n");
        out_daap_error(pwsc,ppi,"MAPI","No item id specified (dmap.itemid)");
        return;
    }

    tempstring=strdup(pi_ws_getvar(pwsc,"dmap.itemid"));
    current=(unsigned char*)tempstring;

    while((token=_strsep((char**)(char*)&current,","))) {
        if(token) {
            /* FIXME:  error handling */
            pi_db_add_playlist_item(NULL,playlist_id,atoi(token));
        }
    }

    free(tempstring);

    /* success(ish)... spool out a dmap block */
    current = playlist_response;
    current += dmap_add_container(current,"MAPI",12);
    current += dmap_add_int(current,"mstt",200);         /* 12 */

    out_daap_output_start(pwsc,ppi,20);
    out_daap_output_write(pwsc,ppi,playlist_response,20);
    out_daap_output_end(pwsc,ppi);

    pi_ws_will_close(pwsc);

    return;
}

/**
 * delete a playlist
 */
void out_daap_deleteplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char playlist_response[20];
    unsigned char *current;

    if(!pi_ws_getvar(pwsc,"dmap.itemid")) {
        pi_log(E_LOG,"Attempt to delete playlist w/o dmap.itemid\n");
        out_daap_error(pwsc,ppi,"MDPR","No playlist id specified");
        return;
    }

    /* FIXME: error handling */
    pi_db_delete_playlist(NULL,atoi(pi_ws_getvar(pwsc,"dmap.itemid")));

    /* success(ish)... spool out a dmap block */
    current = playlist_response;
    current += dmap_add_container(current,"MDPR",12);
    current += dmap_add_int(current,"mstt",200);         /* 12 */

    out_daap_output_start(pwsc,ppi,20);
    out_daap_output_write(pwsc,ppi,playlist_response,20);
    out_daap_output_end(pwsc,ppi);

    pi_ws_will_close(pwsc);

    return;
}

/**
 * delete a playlist item
 */
void out_daap_deleteplaylistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char playlist_response[20];
    unsigned char *current;
    char *tempstring;
    char *token;
    int playlist_id;

    if(!pi_ws_getvar(pwsc,"dmap.itemid")) {
        pi_log(E_LOG,"Delete playlist item w/o dmap.itemid\n");
        out_daap_error(pwsc,ppi,"MDPI","No playlist item specified");
        return;
    }

    playlist_id = atoi(ppi->uri_sections[3]);

    tempstring=strdup(pi_ws_getvar(pwsc,"dmap.itemid"));
    current=(unsigned char *)tempstring;

    /* this looks strange, but gets rid of gcc 4 warnings */
    while((token=_strsep((char**)(char*)&current,","))) {
        if(token) {
            /* FIXME: Error handling */
            pi_db_delete_playlist_item(NULL,playlist_id,atoi(token));
        }
    }

    free(tempstring);

    /* success(ish)... spool out a dmap block */
    current = playlist_response;
    current += dmap_add_container(current,"MDPI",12);
    current += dmap_add_int(current,"mstt",200);         /* 12 */

    out_daap_output_start(pwsc,ppi,20);
    out_daap_output_write(pwsc,ppi,playlist_response,20);
    out_daap_output_end(pwsc,ppi);

    pi_ws_will_close(pwsc);

    return;
}

/**
 * add a playlist
 */
void out_daap_addplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char playlist_response[32];
    unsigned char *current=playlist_response;
    char *name, *query;
    int type;
    int retval, playlistid;
    char *estring = NULL;

    if((!pi_ws_getvar(pwsc,"org.mt-daapd.playlist-type")) ||
       (!pi_ws_getvar(pwsc,"dmap.itemname"))) {
        pi_log(E_LOG,"attempt to add playlist with invalid type\n");
        out_daap_error(pwsc,ppi,"MAPR","bad playlist info specified");
        return;
    }

    type=atoi(pi_ws_getvar(pwsc,"org.mt-daapd.playlist-type"));
    name=pi_ws_getvar(pwsc,"dmap.itemname");
    query=pi_ws_getvar(pwsc,"org.mt-daapd.smart-playlist-spec");

    retval=pi_db_add_playlist(&estring,name,type,query,NULL,0,&playlistid);
    if(retval) {
        out_daap_error(pwsc,ppi,"MAPR",estring);
        pi_log(E_LOG,"error adding playlist %s: %s\n",name,estring);
        free(estring);
        return;
    }

    /* success... spool out a dmap block */
    current += dmap_add_container(current,"MAPR",24);
    current += dmap_add_int(current,"mstt",200);         /* 12 */
    current += dmap_add_int(current,"miid",playlistid);  /* 12 */

    out_daap_output_start(pwsc,ppi,32);
    out_daap_output_write(pwsc,ppi,playlist_response,32);
    out_daap_output_end(pwsc,ppi);

    pi_ws_will_close(pwsc);
    return;
}

/**
 * edit an existing playlist (by id)
 */
void out_daap_editplaylist(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char edit_response[20];
    unsigned char *current = edit_response;
    char *pe = NULL;
    char *name, *query;
    int id;

    int retval;

    if(!pi_ws_getvar(pwsc,"dmap.itemid")) {
        pi_log(E_LOG,"Missing itemid on playlist edit");
        out_daap_error(pwsc,ppi,"MEPR","No itemid specified");
        return;
    }

    name=pi_ws_getvar(pwsc,"dmap.itemname");
    query=pi_ws_getvar(pwsc,"org.mt-daapd.smart-playlist-spec");
    id=atoi(pi_ws_getvar(pwsc,"dmap.itemid"));

    /* FIXME: Error handling */
    retval=pi_db_edit_playlist(&pe,id,name,query);
    if(retval) {
        pi_log(E_LOG,"error editing playlist.\n");
        out_daap_error(pwsc,ppi,"MEPR",pe);
        if(pe) free(pe);
        return;
    }

    current += dmap_add_container(current,"MEPR",12);
    current += dmap_add_int(current,"mstt",200);      /* 12 */

    out_daap_output_start(pwsc,ppi,20);
    out_daap_output_write(pwsc,ppi,edit_response,20);
    out_daap_output_end(pwsc,ppi);

    pi_ws_will_close(pwsc);
    return;
}


/**
 * enumerate and return playlistitems
 */
void out_daap_playlistitems(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char items_response[61];
    unsigned char *current=items_response;
    int song_count;
    int list_length;
    unsigned char *block;
    char *pe = NULL;
    int mtco;

    if(pi_ws_getvar(pwsc,"meta")) {
        ppi->meta = daap_encode_meta(pi_ws_getvar(pwsc,"meta"));
    } else {
        ppi->meta = ((1ll << metaItemId) |
                     (1ll << metaItemName) |
                     (1ll << metaItemKind) |
                     (1ll << metaContainerItemId) |
                     (1ll << metaParentContainerId));
    }

    ppi->dq.query_type = QUERY_TYPE_ITEMS;
    ppi->dq.playlist_id = atoi(ppi->uri_sections[3]);

    if(pi_db_enum_start(&pe,&ppi->dq)) {
        pi_log(E_LOG,"Could not start enum: %s\n",pe);
        out_daap_error(pwsc,ppi,"apso",pe);
        if(pe) free(pe);
        return;
    }

    if(daap_enum_size(&pe,ppi,&song_count,&list_length)) {
        pi_log(E_LOG,"Could not enum size: %s\n",pe);
        out_daap_error(pwsc,ppi,"apso",pe);
        if(pe) free(pe);
        return;
    }

    pi_log(E_DBG,"Item enum:  got %d songs, dmap size: %d\n",song_count,list_length);

    mtco = song_count;
    if(ppi->dq.offset || ppi->dq.limit)
        mtco = ppi->dq.totalcount;

    current += dmap_add_container(current,"apso",list_length + 53);
    current += dmap_add_int(current,"mstt",200);         /* 12 */
    current += dmap_add_char(current,"muty",0);          /*  9 */
    current += dmap_add_int(current,"mtco",mtco);        /* 12 */
    current += dmap_add_int(current,"mrco",song_count);  /* 12 */
    current += dmap_add_container(current,"mlcl",list_length);

    out_daap_output_start(pwsc,ppi,61+list_length);
    out_daap_output_write(pwsc,ppi,items_response,61);

    /* FIXME: Error checking */
    while((daap_enum_fetch(NULL,ppi,&list_length,&block)==0) &&
          (list_length)) {
        pi_log(E_SPAM,"Got block of size %d\n",list_length);
        out_daap_output_write(pwsc,ppi,block,list_length);
        free(block);
    }

    pi_log(E_DBG,"Done enumerating.\n");

    pi_db_enum_end(NULL);
    pi_db_enum_dispose(NULL,&ppi->dq);

    out_daap_output_end(pwsc,ppi);
    return;
}

void out_daap_browse(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char browse_response[52];
    unsigned char *current=browse_response;
    int item_count;
    int list_length;
    unsigned char *block;
    char *response_type;
    int which_field=5;
    char *pe = NULL;
    int mtco;

    if(strcasecmp(ppi->uri_sections[2],"browse") == 0) {
        which_field = 3;
    }

    pi_log(E_DBG,"Browsing by %s (field %d)\n",
              ppi->uri_sections[which_field],which_field);

    ppi->dq.query_type = QUERY_TYPE_DISTINCT;
    ppi->dq.distinct_field = ppi->uri_sections[which_field];
    //    which_field = 3;

    if(!strcmp(ppi->uri_sections[which_field],"artists")) {
        response_type = "abar";
        ppi->dq.distinct_field = "artist";
    } else if(!strcmp(ppi->uri_sections[which_field],"genres")) {
        response_type = "abgn";
        ppi->dq.distinct_field = "genre";
    } else if(!strcmp(ppi->uri_sections[which_field],"albums")) {
        response_type = "abal";
        ppi->dq.distinct_field = "album";
    } else if(!strcmp(ppi->uri_sections[which_field],"composers")) {
        response_type = "abcp";
        ppi->dq.distinct_field = "composer";
    } else {
        pi_log(E_WARN,"Invalid browse request type %s\n",ppi->uri_sections[3]);
        out_daap_error(pwsc,ppi,"abro","Invalid browse type");
        pi_config_set_status(pwsc,ppi->session_id,NULL);
        return;
    }

    if(pi_db_enum_start(&pe,&ppi->dq)) {
        pi_log(E_LOG,"Could not start enum: %s\n",pe);
        out_daap_error(pwsc,ppi,"abro",pe);
        if(pe) free(pe);
        return;
    }

    pi_log(E_DBG,"Getting enum size.\n");

    /* FIXME: Error handling */
    daap_enum_size(NULL,ppi,&item_count,&list_length);

    pi_log(E_DBG,"Item enum: got %d items, dmap size: %d\n",
            item_count,list_length);

    mtco = item_count;
    if((ppi->dq.offset) || (ppi->dq.limit))
        mtco = ppi->dq.totalcount;

    current += dmap_add_container(current,"abro",list_length + 44);
    current += dmap_add_int(current,"mstt",200);                    /* 12 */
    current += dmap_add_int(current,"mtco",mtco);                   /* 12 */
    current += dmap_add_int(current,"mrco",item_count);             /* 12 */
    current += dmap_add_container(current,response_type,list_length); /* 8+ */

    out_daap_output_start(pwsc,ppi,52+list_length);
    out_daap_output_write(pwsc,ppi,browse_response,52);

    while((daap_enum_fetch(NULL,ppi,&list_length,&block)==0) &&
          (list_length))
    {
        pi_log(E_SPAM,"Got block of size %d\n",list_length);
        out_daap_output_write(pwsc,ppi,block,list_length);
        free(block);
    }

    pi_log(E_DBG,"Done enumerating\n");

    pi_db_enum_end(NULL);
    pi_db_enum_dispose(NULL,&ppi->dq);

    out_daap_output_end(pwsc,ppi);
    return;
}

void out_daap_playlists(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char playlist_response[61];
    unsigned char *current=playlist_response;
    int pl_count;
    int list_length;
    unsigned char *block;
    char *pe = NULL;
    int mtco;

    /* currently, this is ignored for playlist queries */
    if(pi_ws_getvar(pwsc,"meta")) {
        ppi->meta = daap_encode_meta(pi_ws_getvar(pwsc,"meta"));
    } else {
        ppi->meta = ((1ll << metaItemId) |
                     (1ll << metaItemName) |
                     (1ll << metaPersistentId) |
                     (1ll << metaItunesSmartPlaylist));
    }


    ppi->dq.query_type = QUERY_TYPE_PLAYLISTS;

    if(pi_db_enum_start(&pe,&ppi->dq)) {
        pi_log(E_LOG,"Could not start enum: %s\n",pe);
        out_daap_error(pwsc,ppi,"aply",pe);
        if(pe) free(pe);
        return;
    }

    if(daap_enum_size(NULL,ppi,&pl_count,&list_length)) {
        pi_log(E_LOG,"error in enumerating size: %s\n",pe);
        out_daap_error(pwsc,ppi,"aply",pe);
        if(pe) free(pe);
        return;
    }

    pi_log(E_DBG,"Item enum:  got %d playlists, dmap size: %d\n",pl_count,list_length);

    mtco = pl_count;
    if((ppi->dq.offset) || (ppi->dq.limit))
        mtco = ppi->dq.totalcount;

    current += dmap_add_container(current,"aply",list_length + 53);
    current += dmap_add_int(current,"mstt",200);         /* 12 */
    current += dmap_add_char(current,"muty",0);          /*  9 */
    current += dmap_add_int(current,"mtco",mtco);        /* 12 */
    current += dmap_add_int(current,"mrco",pl_count);    /* 12 */
    current += dmap_add_container(current,"mlcl",list_length);

    out_daap_output_start(pwsc,ppi,61+list_length);
    out_daap_output_write(pwsc,ppi,playlist_response,61);

    /* FIXME: error checking */
    while((daap_enum_fetch(NULL,ppi,&list_length,&block)==0) &&
          (list_length))
    {
        pi_log(E_SPAM,"Got block of size %d\n",list_length);
        out_daap_output_write(pwsc,ppi,block,list_length);
        free(block);
    }

    pi_log(E_DBG,"Done enumerating.\n");

    pi_db_enum_end(NULL);
    pi_db_enum_dispose(NULL,&ppi->dq);

    out_daap_output_end(pwsc,ppi);
    return;
}

void out_daap_items(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char items_response[61];
    unsigned char *current=items_response;
    int song_count;
    int list_length;
    unsigned char *block;
    char *pe = NULL;
    int mtco;

    if(pi_ws_getvar(pwsc,"meta")) {
        ppi->meta = daap_encode_meta(pi_ws_getvar(pwsc,"meta"));
    } else {
        ppi->meta = (MetaField_t) -1ll;
    }

    ppi->dq.query_type = QUERY_TYPE_ITEMS;

    if(pi_db_enum_start(&pe,&ppi->dq)) {
        pi_log(E_LOG,"Could not start enum: %s\n",pe);
        out_daap_error(pwsc,ppi,"adbs",pe);
        if(pe) free(pe);
        return;
    }

    /* FIXME: Error handling */
    if(daap_enum_size(&pe,ppi,&song_count,&list_length)) {
        pi_log(E_LOG,"Error getting dmap size: %s\n",pe);
        out_daap_error(pwsc,ppi,"adbs",pe);
        if(pe) free(pe);
        return;
    }

    pi_log(E_DBG,"Item enum:  got %d songs, dmap size: %d\n",song_count,
              list_length);

    mtco = song_count;
    if((ppi->dq.offset) || (ppi->dq.limit))
        mtco = ppi->dq.totalcount;

    current += dmap_add_container(current,"adbs",list_length + 53);
    current += dmap_add_int(current,"mstt",200);         /* 12 */
    current += dmap_add_char(current,"muty",0);          /*  9 */
    current += dmap_add_int(current,"mtco",mtco);        /* 12 */
    current += dmap_add_int(current,"mrco",song_count);  /* 12 */
    current += dmap_add_container(current,"mlcl",list_length);

    out_daap_output_start(pwsc,ppi,61+list_length);
    out_daap_output_write(pwsc,ppi,items_response,61);

    /* FIXME: check errors */
    while((daap_enum_fetch(NULL,ppi,&list_length,&block)==0) &&
          (list_length)) {
        pi_log(E_SPAM,"Got block of size %d\n",list_length);
        out_daap_output_write(pwsc,ppi,block,list_length);
        free(block);
    }
    pi_log(E_DBG,"Done enumerating.\n");
    pi_db_enum_end(NULL);
    pi_db_enum_dispose(NULL,&ppi->dq);
    out_daap_output_end(pwsc,ppi);
    return;
}

void out_daap_update(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char update_response[32];
    unsigned char *current=update_response;

    pi_log(E_DBG,"Preparing to send update response\n");
    pi_config_set_status(pwsc,ppi->session_id,"Waiting for DB update");

    if(!pi_db_wait_update(pwsc)) {
        pi_log(E_DBG,"Update session stopped\n");
        return;
    }

    /* otherwise, send the info about this version */
    current += dmap_add_container(current,"mupd",24);
    current += dmap_add_int(current,"mstt",200);       /* 12 */
    current += dmap_add_int(current,"musr",pi_db_revision());   /* 12 */

    out_daap_output_start(pwsc,ppi,32);
    out_daap_output_write(pwsc,ppi,update_response,32);
    out_daap_output_end(pwsc,ppi);

    return;
}

void out_daap_dbinfo(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char dbinfo_response[255];  /* FIXME: servername limit 255-113 */
    unsigned char *current = dbinfo_response;
    int namelen;
    int count;
    char servername[256];
    int servername_size;

    servername_size = sizeof(servername);
    pi_server_name(servername,&servername_size);

    namelen=(int) strlen(servername);

    current += dmap_add_container(current,"avdb",121 + namelen);
    current += dmap_add_int(current,"mstt",200);                    /* 12 */
    current += dmap_add_char(current,"muty",0);                     /*  9 */
    current += dmap_add_int(current,"mtco",1);                      /* 12 */
    current += dmap_add_int(current,"mrco",1);                      /* 12 */
    current += dmap_add_container(current,"mlcl",68 + namelen);
    current += dmap_add_container(current,"mlit",60 + namelen);
    current += dmap_add_int(current,"miid",1);                      /* 12 */
    current += dmap_add_long(current,"mper",1);                     /* 16 */
    current += dmap_add_string(current,"minm",servername); /* 8 + namelen */
    count = pi_db_count_items(COUNT_SONGS);
    current += dmap_add_int(current,"mimc",count);                  /* 12 */
    count = pi_db_count_items(COUNT_PLAYLISTS);
    current += dmap_add_int(current,"mctc",count);                  /* 12 */

    out_daap_output_start(pwsc,ppi,129+namelen);
    out_daap_output_write(pwsc,ppi,dbinfo_response,129+namelen);
    out_daap_output_end(pwsc,ppi);

    return;
}

void out_daap_logout(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    pi_config_set_status(pwsc,ppi->session_id,NULL);
    pi_ws_returnerror(pwsc,204,"Logout Successful");
}


void out_daap_login(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char login_response[32];
    unsigned char *current = login_response;
    int session;

    session = daap_get_next_session();

    current += dmap_add_container(current,"mlog",24);
    current += dmap_add_int(current,"mstt",200);       /* 12 */
    current += dmap_add_int(current,"mlid",session);   /* 12 */

    out_daap_output_start(pwsc,ppi,32);
    out_daap_output_write(pwsc,ppi,login_response,32);
    out_daap_output_end(pwsc,ppi);
    return;
}

void out_daap_content_codes(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char content_codes[20];
    unsigned char *current=content_codes;
    unsigned char mdcl[256];  /* FIXME: Don't make this static */
    int len;
    DAAP_ITEMS *dicurrent;

    dicurrent=taglist;
    len=0;
    while(dicurrent->type) {
        len += (8 + 12 + 10 + 8 + (int) strlen(dicurrent->description));
        dicurrent++;
    }

    current += dmap_add_container(current,"mccr",len + 12);
    current += dmap_add_int(current,"mstt",200);

    out_daap_output_start(pwsc,ppi,len+20);
    out_daap_output_write(pwsc,ppi,content_codes,20);

    dicurrent=taglist;
    while(dicurrent->type) {
        current=mdcl;
        len = 12 + 10 + 8 + (int) strlen(dicurrent->description);
        current += dmap_add_container(current,"mdcl",len);
        current += dmap_add_string(current,"mcnm",dicurrent->tag);         /* 12 */
        current += dmap_add_string(current,"mcna",dicurrent->description); /* 8 + descr */
        current += dmap_add_short(current,"mcty",dicurrent->type);         /* 10 */
        out_daap_output_write(pwsc,ppi,mdcl,len+8);
        dicurrent++;
    }

    out_daap_output_end(pwsc,ppi);
    return;
}


int out_daap_conf_isset(char *section, char *key) {
    char *value;

    value = pi_conf_alloc_string(section,key,NULL);
    if(value) {
        pi_conf_dispose_string(value);
        return TRUE;
    }

    return FALSE;
}

void out_daap_server_info(WS_CONNINFO *pwsc, PRIVINFO *ppi) {
    unsigned char server_info[256];
    char servername[256];
    int size;
    unsigned char *current = server_info;
    char *client_version;
    int mpro = 2 << 16;
    int apro = 3 << 16;
    int actual_length;
    int supports_update=0;

    size = sizeof(servername);
    pi_server_name(servername,&size);
    //    supports_update = conf_get_int("daap","supports_update",1);

    actual_length=139 + (int) strlen(servername);
    if(!supports_update)
        actual_length -= 9;

    if(actual_length > sizeof(server_info)) {
        pi_log(E_FATAL,"Server name too long.\n");
    }

    client_version=pi_ws_getrequestheader(pwsc,"Client-DAAP-Version");

    current += dmap_add_container(current,"msrv",actual_length - 8);
    current += dmap_add_int(current,"mstt",200);        /* 12 */

    if((client_version) && (!strcmp(client_version,"1.0"))) {
        mpro = 1 << 16;
        apro = 1 << 16;
    }

    if((client_version) && (!strcmp(client_version,"2.0"))) {
        mpro = 1 << 16;
        apro = 2 << 16;
    }

    current += dmap_add_int(current,"mpro",mpro);       /* 12 */
    current += dmap_add_int(current,"apro",apro);       /* 12 */
    current += dmap_add_int(current,"mstm",1800);       /* 12 */
    current += dmap_add_string(current,"minm",servername); /* 8 + strlen(name) */


    current += dmap_add_char(current,"msau",            /* 9 */
                             out_daap_conf_isset("general","password") ? 2 : 0);
    current += dmap_add_char(current,"msex",0);         /* 9 */
    current += dmap_add_char(current,"msix",0);         /* 9 */
    current += dmap_add_char(current,"msbr",0);         /* 9 */
    current += dmap_add_char(current,"msqy",0);         /* 9 */

    current += dmap_add_char(current,"mspi",0);
    current += dmap_add_int(current,"msdc",1);          /* 12 */

    if(supports_update)
        current += dmap_add_char(current,"msup",0);         /* 9 */

    out_daap_output_start(pwsc,ppi,actual_length);
    out_daap_output_write(pwsc,ppi,server_info,actual_length);
    out_daap_output_end(pwsc,ppi);

    return;
}

/**
 * throw out an error, xml style.  This throws  out a dmap block, but with a
 * mstt of 500, and a msts as specified
 */
void out_daap_error(WS_CONNINFO *pwsc, PRIVINFO *ppi, char *container, char *error) {
    unsigned char *block, *current;
    int len;

    len = 12 + 8 + 8 + (int) strlen(error);
    block = (unsigned char *)malloc(len);

    if(!block)
        pi_log(E_FATAL,"Malloc error\n");

    current = block;
    current += dmap_add_container(current,container,len - 8);
    current += dmap_add_int(current,"mstt",500);
    current += dmap_add_string(current,"msts",error);

    out_daap_output_start(pwsc,ppi,len);
    out_daap_output_write(pwsc,ppi,block,len);
    out_daap_output_end(pwsc,ppi);

    free(block);

    pi_ws_will_close(pwsc);
}



