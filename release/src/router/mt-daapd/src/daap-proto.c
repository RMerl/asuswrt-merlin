/*
 * $Id: daap-proto.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Helper functions for formatting a daap message
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "daap-proto.h"
#include "err.h"
#include "restart.h"

/* Forwards */
DAAP_BLOCK *daap_get_new(void);
DAAP_BLOCK *daap_add_formatted(DAAP_BLOCK *parent, char *tag, 
			       int len, char *value);


GZIP_STREAM *gzip_alloc(void) {
    GZIP_STREAM *gz = malloc(sizeof(GZIP_STREAM));

    if(gz) {
	memset(gz,0x00,sizeof(GZIP_STREAM));

	gz->in_size = GZIP_CHUNK;
	gz->in = malloc(gz->in_size);
	gz->bytes_in = 0;
	gz->out = NULL;
	gz->bytes_out = 0;
    }
    return gz;
}

ssize_t gzip_write(GZIP_STREAM *gz, void *buf, size_t size) {
    int next_size;
    char *in2;
    int new_size;
  
    if (gz->in == NULL) 
	return -1;

    /* make sure input buffer is big enough */
    while (gz->in_size <= gz->bytes_in + size) {
	new_size = 2*gz->in_size;
	in2 = malloc(new_size);
	if (in2 == NULL) {
	    DPRINTF(E_LOG,L_WS|L_DAAP,"out of memory for input buffer\n");
	    free(gz->in);
	    gz->in = NULL;
	    gz->bytes_in = gz->in_size = 0;
	    return -1;
	}
	memcpy(in2, gz->in, gz->in_size);
	free(gz->in);
	gz->in = in2;
	gz->in_size = new_size;
    }
    memcpy(gz->in + gz->bytes_in, buf, size);
    gz->bytes_in += size;
    return size;
}
   
int gzip_compress(GZIP_STREAM *gz) {
    int out_size;
    int status; 
    z_stream strm;

    if (gz->in == NULL)
	return -1;

    out_size = (int)(1.05*gz->in_size) + 40;
    gz->out = malloc(out_size);
    if (gz->out == NULL) {
	DPRINTF(E_INF,L_WS|L_DAAP,"out of memory for output buffer\n");
	gz->bytes_out = 0;
	return -1;
    }

    memset((void*)&strm,0x00,sizeof(strm));

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = gz->in;
    strm.avail_in = gz->bytes_in;
    strm.next_out = gz->out;
    strm.avail_out = out_size;
    deflateInit2(&strm,GZIP_COMPRESSION_LEVEL,Z_DEFLATED,24,8,Z_DEFAULT_STRATEGY);
    while ((status = deflate(&strm,Z_FINISH)) == Z_OK)
	;
    if (status != Z_STREAM_END) {
	DPRINTF(E_LOG,L_WS|L_DAAP,"unable to compress data: %s (%d)\n",strm.msg,status);
	gz->bytes_out = 0;
	return -1;
    }
    gz->bytes_out = strm.total_out;
    deflateEnd(&strm);

    return gz->bytes_out;
}

int gzip_close(GZIP_STREAM *gz, int fd) {
    int bytes_written = gz->bytes_out;
    if (r_write(fd,gz->out,gz->bytes_out) != gz->bytes_out) {
	DPRINTF(E_LOG,L_WS|L_DAAP,"unable to write gzipped data\n");
	return -1;
    }
    if (gz->in != NULL)
	free(gz->in);
    if (gz->out != NULL)
	free(gz->out);
    free(gz);
    return bytes_written;
}


  



/*
 * daap_get_new
 *
 * Initialize a new daap struct
 */
DAAP_BLOCK *daap_get_new(void) {
    DAAP_BLOCK *pnew;

    pnew=(DAAP_BLOCK*)malloc(sizeof(DAAP_BLOCK));
    if(!pnew) {
	DPRINTF(E_WARN,L_DAAP,"Error mallocing a daap block\n");
	return NULL;
    }

    pnew->free=0;
    pnew->value=NULL;
    pnew->parent=NULL;
    pnew->children=NULL;
    pnew->last_child=NULL;
    pnew->next=NULL;

    return pnew;
}

/*
 * daap_add_formatted
 *
 * add a block exactly as formatted in value.
 *
 * Note that value WILL be freed later in daap_free, so
 * the value paramater must have been malloced
 */
DAAP_BLOCK *daap_add_formatted(DAAP_BLOCK *parent, char *tag, 
			       int size, char *value) {
    DAAP_BLOCK *current,*last;
    DAAP_BLOCK *pnew;

    DPRINTF(E_SPAM,L_DAAP,"Adding daap tag %s\n",tag);
    pnew = daap_get_new();
    if(!pnew)
	return NULL;

    pnew->reported_size=size;
    pnew->parent=parent;
    pnew->size=size;
    memcpy(pnew->tag,tag,4);

    if((size <= 4) && (size >= 0)) { /* we can just put it in svalue */
	memcpy(pnew->svalue,value,size);
	pnew->free=0;
    } else {
	pnew->value=value;
	pnew->free=1;
    }
    pnew->next=NULL;

    /* walk the child list and put it at the end */
    if(parent) {
        last = parent->last_child;
        if (last) {
          last->next = pnew;
        } else {
          parent->children   = pnew;
        } 
        parent->last_child = pnew;
    }

    /* now, walk the chain and update sizes */
    current=pnew->parent;
    while(current) {
	current->reported_size += (8 + pnew->reported_size);
	current=current->parent;
    }

    return pnew;
}

/*
 * daap_add_int
 *
 * Add an int block to a specific parent
 */
DAAP_BLOCK *daap_add_long(DAAP_BLOCK *parent, char *tag, int v1, int v2) {
    char *ivalue;
    ivalue=(char*)malloc(8);
    if(!ivalue)
	return NULL;

    ivalue[0]=(v1 >> 24) & 0xFF;
    ivalue[1]=(v1 >> 16) & 0xFF;
    ivalue[2]=(v1 >> 8) & 0xFF;
    ivalue[3]=v1 & 0xFF;

    ivalue[4]=(v2 >> 24) & 0xFF;
    ivalue[5]=(v2 >> 16) & 0xFF;
    ivalue[6]=(v2 >> 8) & 0xFF;
    ivalue[7]=v2 & 0xFF;

    return daap_add_formatted(parent,tag,8,ivalue);
}

/*
 * daap_add_int
 *
 * Add an int block to a specific parent
 */
DAAP_BLOCK *daap_add_int(DAAP_BLOCK *parent, char *tag, int value) {
    char ivalue[4];

    ivalue[0]=(value >> 24) & 0xFF;
    ivalue[1]=(value >> 16) & 0xFF;
    ivalue[2]=(value >> 8) & 0xFF;
    ivalue[3]=value & 0xFF;

    return daap_add_formatted(parent,tag,4,ivalue);
}

/*
 * daap_add_short
 *
 * Add an int block to a specific parent
 */
DAAP_BLOCK *daap_add_short(DAAP_BLOCK *parent, char *tag, short int value) {
    char ivalue[2];

    ivalue[0]=(value >> 8) & 0xFF;
    ivalue[1]=value & 0xFF;

    return daap_add_formatted(parent,tag,2,ivalue);
}

/* 
 * daap_add_char
 *
 * Add a single char
 */
DAAP_BLOCK *daap_add_char(DAAP_BLOCK *parent, char *tag, char value) {
    return daap_add_formatted(parent,tag,1,&value);
}

/*
 * daap_add_data
 *
 * Add unstructured data to a specific parent
 */
DAAP_BLOCK *daap_add_data(DAAP_BLOCK *parent, char *tag, 
			  int len, void *value) {
    void *pvalue;

    if(len > 4) {
	pvalue=(void*)malloc(len);
	if(!pvalue)
	    return NULL;

	memcpy(pvalue,value,len);
    
	return daap_add_formatted(parent,tag,len,pvalue);
    } 
    return daap_add_formatted(parent,tag,len,value);
}

/*
 * daap_add_string
 *
 * Add a string element to a specific parent
 */
DAAP_BLOCK *daap_add_string(DAAP_BLOCK *parent, char *tag, char *value) {
    char *newvalue;

    if(value) {
	if(strlen(value) > 4) {
	    newvalue=strdup(value);
	    
	    if(!newvalue)
		return NULL;
	    
	    return daap_add_formatted(parent,tag,strlen(newvalue),newvalue);
	} 
	return daap_add_formatted(parent,tag,strlen(value),value);
    }
    return daap_add_formatted(parent,tag,0,"");
}

/*
 * daap_add_empty
 *
 * add a tag whose only value is to act as an aggregator
 */
DAAP_BLOCK *daap_add_empty(DAAP_BLOCK *parent, char *tag) {
    return daap_add_formatted(parent,tag,0,NULL);
}

/*
 * daap_serialmem
 *
 * Serialize a daap tree to a fd;
 */
int daap_serialize(DAAP_BLOCK *root, int fd, GZIP_STREAM *gz) {
    char size[4];

    while(root) {
	if (gz == NULL)
	    r_write(fd,root->tag,4);
	else
	    gzip_write(gz,root->tag,4);

	size[0] = (root->reported_size >> 24) & 0xFF;
	size[1] = (root->reported_size >> 16) & 0xFF;
	size[2] = (root->reported_size >> 8 ) & 0xFF;
	size[3] = (root->reported_size) & 0xFF;
      
	if (gz == NULL)
	    r_write(fd,&size,4);
	else
	    gzip_write(gz,&size,4);
	
	if(root->size) {
	    if(root->free) {
		if (gz == NULL) {
		    r_write(fd,root->value,root->size);
		}
		else {
		    gzip_write(gz,root->value,root->size);
		}
	    }
	    else {
		if (gz == NULL) {
		    r_write(fd,root->svalue,root->size);
		}
		else {
		    gzip_write(gz,root->svalue,root->size);
		}
	    }
	}
    
	if(root->children) {
	    if(daap_serialize(root->children,fd,gz))
		return -1;
	}

	root=root->next;
    }
    
    return 0;
}

/*
 * daap_remove
 *
 * remove a node from it's parent node and release it
 */
void daap_remove(DAAP_BLOCK* node)
{
    DAAP_BLOCK*	parent = node->parent;

    if(0 != parent)
	{
	    DAAP_BLOCK**	ptr = &parent->children;

	    while(*ptr && *ptr != node)
		ptr = &(**ptr).next;

	    assert(0 != *ptr);
	
	    // remove us from the chain
	    *ptr = node->next;

	    // update sizes in parent chain
	    for(parent = node->parent ; parent ; parent = parent->parent)
		parent->reported_size -= (8 + node->reported_size);
	
	    // clear parent and next pointers so daap_free doesn't get ambitious
	    node->parent = 0;
	    node->next = 0;
	}
    
    daap_free(node);
}

/*
 * find a child block of the parent node
 */
DAAP_BLOCK *daap_find(DAAP_BLOCK *parent, char* tag)
{
    for(parent = parent->children ; parent ; parent = parent->next)
	if(!strncmp(parent->tag, tag, 4))
	    break;

    return parent;
}

/*
 * daap_free
 *
 * Free an entire daap formatted block
 */
void daap_free(DAAP_BLOCK *root) {
    DAAP_BLOCK *pnext;

    while(root) {
	DPRINTF(E_SPAM,L_DAAP,"Freeing %c%c%c%c\n",root->tag[0],root->tag[1],
		root->tag[2],root->tag[3]);

	if((root->size) && (root->free))
	    free(root->value); /* otherwise, static value */

	daap_free(root->children);
	
	pnext=root->next;
	free(root);
	root=pnext;
    }
    return;
}

// search a block's children and change an integer value
int daap_set_int(DAAP_BLOCK* parent, char* tag, int value)
{
    DAAP_BLOCK*	child = daap_find(parent, tag);

    if(0 == child || child->size != sizeof(int))
	return 0;

    child->svalue[0]=(value >> 24) & 0xFF;
    child->svalue[1]=(value >> 16) & 0xFF;
    child->svalue[2]=(value >> 8) & 0xFF;
    child->svalue[3]=value & 0xFF;

    return 1;
}

