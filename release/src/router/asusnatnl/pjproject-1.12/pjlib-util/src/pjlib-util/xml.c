/* $Id: xml.c 3958 2012-02-25 00:55:37Z bennylp $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util/xml.h>
#include <pjlib-util/scanner.h>
#include <pj/except.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/log.h>
#include <pj/os.h>

#define EX_SYNTAX_ERROR	12
#define THIS_FILE	"xml.c"

static void on_syntax_error(struct pj_scanner *scanner)
{
    PJ_THROW(scanner->inst_id, EX_SYNTAX_ERROR);
}

static pj_xml_node *alloc_node( pj_pool_t *pool )
{
    pj_xml_node *node;

    node = PJ_POOL_ZALLOC_T(pool, pj_xml_node);
    pj_list_init( &node->attr_head );
    pj_list_init( &node->node_head );

    return node;
}

static pj_xml_attr *alloc_attr( pj_pool_t *pool )
{
    return PJ_POOL_ZALLOC_T(pool, pj_xml_attr);
}

/* This is a recursive function! */
static pj_xml_node *xml_parse_node( pj_pool_t *pool, pj_scanner *scanner)
{
    pj_xml_node *node;
    pj_str_t end_name;

    PJ_CHECK_STACK();

    if (*scanner->curptr != '<')
	on_syntax_error(scanner);

    /* Handle Processing Instructino (PI) construct (i.e. "<?") */
    if (*scanner->curptr == '<' && *(scanner->curptr+1) == '?') {
	pj_scan_advance_n(scanner, 2, PJ_FALSE);
	for (;;) {
	    pj_str_t dummy;
	    pj_scan_get_until_ch(scanner, '?', &dummy);
	    if (*scanner->curptr=='?' && *(scanner->curptr+1)=='>') {
		pj_scan_advance_n(scanner, 2, PJ_TRUE);
		break;
	    } else {
		pj_scan_advance_n(scanner, 1, PJ_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Handle comments construct (i.e. "<!") */
    if (pj_scan_strcmp(scanner, "<!", 2) == 0) {
	pj_scan_advance_n(scanner, 2, PJ_FALSE);
	for (;;) {
	    pj_str_t dummy;
	    pj_scan_get_until_ch(scanner, '>', &dummy);
	    if (pj_scan_strcmp(scanner, ">", 1) == 0) {
		pj_scan_advance_n(scanner, 1, PJ_TRUE);
		break;
	    } else {
		pj_scan_advance_n(scanner, 1, PJ_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Alloc node. */
    node = alloc_node(pool);

    /* Get '<' */
    pj_scan_get_char(scanner);

    /* Get node name. */
    pj_scan_get_until_chr( scanner, " />\t\r\n", &node->name);

    /* Get attributes. */
    while (*scanner->curptr != '>' && *scanner->curptr != '/') {
	pj_xml_attr *attr = alloc_attr(pool);
	
	pj_scan_get_until_chr( scanner, "=> \t\r\n", &attr->name);
	if (*scanner->curptr == '=') {
	    pj_scan_get_char( scanner );
            pj_scan_get_quotes(scanner, "\"'", "\"'", 2, &attr->value);
	    /* remove quote characters */
	    ++attr->value.ptr;
	    attr->value.slen -= 2;
	}
	
	pj_list_push_back( &node->attr_head, attr );
    }

    if (*scanner->curptr == '/') {
	pj_scan_get_char(scanner);
	if (pj_scan_get_char(scanner) != '>')
	    on_syntax_error(scanner);
	return node;
    }

    /* Enclosing bracket. */
    if (pj_scan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    /* Sub nodes. */
    while (*scanner->curptr == '<' && *(scanner->curptr+1) != '/') {
	pj_xml_node *sub_node = xml_parse_node(pool, scanner);
	pj_list_push_back( &node->node_head, sub_node );
    }

    /* Content. */
    if (!pj_scan_is_eof(scanner) && *scanner->curptr != '<') {
	pj_scan_get_until_ch(scanner, '<', &node->content);
    }

    /* Enclosing node. */
    if (pj_scan_get_char(scanner) != '<' || pj_scan_get_char(scanner) != '/')
	on_syntax_error(scanner);

    pj_scan_get_until_chr(scanner, " \t>", &end_name);

    /* Compare name. */
    if (pj_stricmp(&node->name, &end_name) != 0)
	on_syntax_error(scanner);

    /* Enclosing '>' */
    if (pj_scan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    return node;
}

PJ_DEF(pj_xml_node*) pj_xml_parse( int inst_id, pj_pool_t *pool, char *msg, pj_size_t len)
{
    pj_xml_node *node = NULL;
    pj_scanner scanner;
    PJ_USE_EXCEPTION;

    if (!msg || !len || !pool)
	return NULL;

    pj_scan_init( inst_id, &scanner, msg, len, 
		  PJ_SCAN_AUTOSKIP_WS|PJ_SCAN_AUTOSKIP_NEWLINE, 
		  &on_syntax_error);
    PJ_TRY(inst_id) {
	node =  xml_parse_node(pool, &scanner);
    }
    PJ_CATCH_ANY {
	PJ_LOG(4,(THIS_FILE, "Syntax error parsing XML in line %d column %d",
		  scanner.line, pj_scan_get_col(&scanner)));
    }
    PJ_END(inst_id);
    pj_scan_fini( &scanner );
    return node;
}

/* This is a recursive function. */
static int xml_print_node( const pj_xml_node *node, int indent, 
			   char *buf, pj_size_t len )
{
    int i;
    char *p = buf;
    pj_xml_attr *attr;
    pj_xml_node *sub_node;

#define SIZE_LEFT()	((int)(len - (p-buf)))

    PJ_CHECK_STACK();

    /* Print name. */
    if (SIZE_LEFT() < node->name.slen + indent + 5)
	return -1;
    for (i=0; i<indent; ++i)
	*p++ = ' ';
    *p++ = '<';
    pj_memcpy(p, node->name.ptr, node->name.slen);
    p += node->name.slen;

    /* Print attributes. */
    attr = node->attr_head.next;
    while (attr != &node->attr_head) {

	if (SIZE_LEFT() < attr->name.slen + attr->value.slen + 4)
	    return -1;

	*p++ = ' ';

	/* Attribute name. */
	pj_memcpy(p, attr->name.ptr, attr->name.slen);
	p += attr->name.slen;

	/* Attribute value. */
	if (attr->value.slen) {
	    *p++ = '=';
	    *p++ = '"';
	    pj_memcpy(p, attr->value.ptr, attr->value.slen);
	    p += attr->value.slen;
	    *p++ = '"';
	}

	attr = attr->next;
    }

    /* Check for empty node. */
    if (node->content.slen==0 &&
	node->node_head.next==(pj_xml_node*)&node->node_head)
    {
	*p++ = ' ';
	*p++ = '/';
	*p++ = '>';
	return p-buf;
    }

    /* Enclosing '>' */
    if (SIZE_LEFT() < 1) return -1;
    *p++ = '>';

    /* Print sub nodes. */
    sub_node = node->node_head.next;
    while (sub_node != (pj_xml_node*)&node->node_head) {
	int printed;

	if (SIZE_LEFT() < indent + 3)
	    return -1;
	//*p++ = '\r';
	*p++ = '\n';

	printed = xml_print_node(sub_node, indent + 1, p, SIZE_LEFT());
	if (printed < 0)
	    return -1;

	p += printed;
	sub_node = sub_node->next;
    }

    /* Content. */
    if (node->content.slen) {
	if (SIZE_LEFT() < node->content.slen) return -1;
	pj_memcpy(p, node->content.ptr, node->content.slen);
	p += node->content.slen;
    }

    /* Enclosing node. */
    if (node->node_head.next != (pj_xml_node*)&node->node_head) {
	if (SIZE_LEFT() < node->name.slen + 5 + indent)
	    return -1;
	//*p++ = '\r';
	*p++ = '\n';
	for (i=0; i<indent; ++i)
	    *p++ = ' ';
    } else {
	if (SIZE_LEFT() < node->name.slen + 3)
	    return -1;
    }
    *p++ = '<';
    *p++ = '/';
    pj_memcpy(p, node->name.ptr, node->name.slen);
    p += node->name.slen;
    *p++ = '>';

#undef SIZE_LEFT

    return p - buf;
}

PJ_DEF(int) pj_xml_print(const pj_xml_node *node, char *buf, pj_size_t len,
			 pj_bool_t include_prolog)
{
    int prolog_len = 0;
    int printed;

    if (!node || !buf || !len)
	return 0;

    if (include_prolog) {
	pj_str_t prolog = {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", 39};
	if ((int)len < prolog.slen)
	    return -1;
	pj_memcpy(buf, prolog.ptr, prolog.slen);
	prolog_len = prolog.slen;
    }

    printed = xml_print_node(node, 0, buf+prolog_len, len-prolog_len) + prolog_len;
    if (printed > 0 && len-printed >= 1) {
	buf[printed++] = '\n';
    }
    return printed;
}

PJ_DEF(pj_xml_node*) pj_xml_node_new(pj_pool_t *pool, const pj_str_t *name)
{
    pj_xml_node *node = alloc_node(pool);
    pj_strdup(pool, &node->name, name);
    return node;
}

PJ_DEF(pj_xml_attr*) pj_xml_attr_new( pj_pool_t *pool, const pj_str_t *name,
				      const pj_str_t *value)
{
    pj_xml_attr *attr = alloc_attr(pool);
    pj_strdup( pool, &attr->name, name);
    pj_strdup( pool, &attr->value, value);
    return attr;
}

PJ_DEF(void) pj_xml_add_node( pj_xml_node *parent, pj_xml_node *node )
{
    pj_list_push_back(&parent->node_head, node);
}

PJ_DEF(void) pj_xml_add_attr( pj_xml_node *node, pj_xml_attr *attr )
{
    pj_list_push_back(&node->attr_head, attr);
}

PJ_DEF(pj_xml_node*) pj_xml_find_node(const pj_xml_node *parent, 
				      const pj_str_t *name)
{
    const pj_xml_node *node = parent->node_head.next;

    PJ_CHECK_STACK();

    while (node != (void*)&parent->node_head) {
	if (pj_stricmp(&node->name, name) == 0)
	    return (pj_xml_node*)node;
	node = node->next;
    }
    return NULL;
}

PJ_DEF(pj_xml_node*) pj_xml_find_node_rec(const pj_xml_node *parent, 
					  const pj_str_t *name)
{
    const pj_xml_node *node = parent->node_head.next;

    PJ_CHECK_STACK();

    while (node != (void*)&parent->node_head) {
	pj_xml_node *found;
	if (pj_stricmp(&node->name, name) == 0)
	    return (pj_xml_node*)node;
	found = pj_xml_find_node_rec(node, name);
	if (found)
	    return (pj_xml_node*)found;
	node = node->next;
    }
    return NULL;
}

PJ_DEF(pj_xml_node*) pj_xml_find_next_node( const pj_xml_node *parent, 
					    const pj_xml_node *node,
					    const pj_str_t *name)
{
    PJ_CHECK_STACK();

    node = node->next;
    while (node != (void*)&parent->node_head) {
	if (pj_stricmp(&node->name, name) == 0)
	    return (pj_xml_node*)node;
	node = node->next;
    }
    return NULL;
}


PJ_DEF(pj_xml_attr*) pj_xml_find_attr( const pj_xml_node *node, 
				       const pj_str_t *name,
				       const pj_str_t *value)
{
    const pj_xml_attr *attr = node->attr_head.next;
    while (attr != (void*)&node->attr_head) {
	if (pj_stricmp(&attr->name, name)==0) {
	    if (value) {
		if (pj_stricmp(&attr->value, value)==0)
		    return (pj_xml_attr*)attr;
	    } else {
		return (pj_xml_attr*)attr;
	    }
	}
	attr = attr->next;
    }
    return NULL;
}



PJ_DEF(pj_xml_node*) pj_xml_find( const pj_xml_node *parent, 
				  const pj_str_t *name,
				  const void *data, 
				  pj_bool_t (*match)(const pj_xml_node *, 
						     const void*))
{
    const pj_xml_node *node = (const pj_xml_node *)parent->node_head.next;

    if (!name && !match)
	return NULL;

    while (node != (const pj_xml_node*) &parent->node_head) {
	if (name) {
	    if (pj_stricmp(&node->name, name)!=0) {
		node = node->next;
		continue;
	    }
	}
	if (match) {
	    if (match(node, data))
		return (pj_xml_node*)node;
	} else {
	    return (pj_xml_node*)node;
	}

	node = node->next;
    }
    return NULL;
}

PJ_DEF(pj_xml_node*) pj_xml_find_rec( const pj_xml_node *parent, 
				      const pj_str_t *name,
				      const void *data, 
				      pj_bool_t (*match)(const pj_xml_node*, 
							 const void*))
{
    const pj_xml_node *node = (const pj_xml_node *)parent->node_head.next;

    if (!name && !match)
	return NULL;

    while (node != (const pj_xml_node*) &parent->node_head) {
	pj_xml_node *found;

	if (name) {
	    if (pj_stricmp(&node->name, name)==0) {
		if (match) {
		    if (match(node, data))
			return (pj_xml_node*)node;
		} else {
		    return (pj_xml_node*)node;
		}
	    }

	} else if (match) {
	    if (match(node, data))
		return (pj_xml_node*)node;
	}

	found = pj_xml_find_rec(node, name, data, match);
	if (found)
	    return found;

	node = node->next;
    }
    return NULL;
}

PJ_DEF(pj_xml_node*) pj_xml_clone( pj_pool_t *pool, const pj_xml_node *rhs)
{
    pj_xml_node *node;
    const pj_xml_attr *r_attr;
    const pj_xml_node *child;

    node = alloc_node(pool);

    pj_strdup(pool, &node->name, &rhs->name);
    pj_strdup(pool, &node->content, &rhs->content);

    /* Clone all attributes */
    r_attr = rhs->attr_head.next;
    while (r_attr != &rhs->attr_head) {

	pj_xml_attr *attr;

	attr = alloc_attr(pool);
	pj_strdup(pool, &attr->name, &r_attr->name);
	pj_strdup(pool, &attr->value, &r_attr->value);

	pj_list_push_back(&node->attr_head, attr);

	r_attr = r_attr->next;
    }

    /* Clone all child nodes. */
    child = rhs->node_head.next;
    while (child != (pj_xml_node*) &rhs->node_head) {
	pj_xml_node *new_child;

	new_child = pj_xml_clone(pool, child);
	pj_list_push_back(&node->node_head, new_child);

	child = child->next;
    }

    return node;
}
