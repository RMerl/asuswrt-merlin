/* $Id: xml.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_XML_H__
#define __PJ_XML_H__

/**
 * @file xml.h
 * @brief PJLIB XML Parser/Helper.
 */

#include <pj/types.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_TINY_XML Mini/Tiny XML Parser/Helper
 * @ingroup PJ_FILE_FMT
 * @{
 */

/** Typedef for XML attribute. */
typedef struct pj_xml_attr pj_xml_attr;

/** Typedef for XML nodes. */
typedef struct pj_xml_node pj_xml_node;

/** This structure declares XML attribute. */
struct pj_xml_attr
{
    PJ_DECL_LIST_MEMBER(pj_xml_attr);   /**< Standard list elements.    */
    pj_str_t	name;	                /**< Attribute name.            */
    pj_str_t	value;	                /**< Attribute value.           */
};

/** This structure describes XML node head inside XML node structure.
 */
typedef struct pj_xml_node_head
{
    PJ_DECL_LIST_MEMBER(pj_xml_node);   /**< Standard list elements.    */
} pj_xml_node_head;

/** This structure describes XML node. */
struct pj_xml_node
{
    PJ_DECL_LIST_MEMBER(pj_xml_node);   /**< List @a prev and @a next member */
    pj_str_t		name;		/**< Node name.                      */
    pj_xml_attr		attr_head;      /**< Attribute list.                 */
    pj_xml_node_head	node_head;      /**< Node list.                      */
    pj_str_t		content;	/**< Node content.                   */
};

/**
 * Parse XML message into XML document with a single root node. The parser
 * is capable of parsing XML processing instruction construct ("<?") and 
 * XML comments ("<!--"), however such constructs will be ignored and will not 
 * be included in the resulted XML node tree.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool	    Pool to allocate memory from.
 * @param msg	    The XML message to parse.
 * @param len	    The length of the message.
 *
 * @return	    XML root node, or NULL if the XML document can not be parsed.
 */
PJ_DECL(pj_xml_node*) pj_xml_parse( int inst_id, pj_pool_t *pool, char *msg, pj_size_t len);


/**
 * Print XML into XML message. Note that the function WILL NOT NULL terminate
 * the output.
 *
 * @param node	    The XML node to print.
 * @param buf	    Buffer to hold the output message.
 * @param len	    The length of the buffer.
 * @param prolog    If set to nonzero, will print XML prolog ("<?xml..")
 *
 * @return	    The size of the printed message, or -1 if there is not 
 *		    sufficient space in the buffer to print the message.
 */
PJ_DECL(int) pj_xml_print( const pj_xml_node *node, char *buf, pj_size_t len,
			   pj_bool_t prolog);

/**
 * Clone XML node and all subnodes.
 *
 * @param pool	    Pool to allocate memory for new nodes.
 * @param rhs	    The node to clone.
 *
 * @return	    Cloned XML node, or NULL on fail.
 */
PJ_DECL(pj_xml_node*) pj_xml_clone( pj_pool_t *pool, const pj_xml_node *rhs);


/**
 * Create an empty node.
 *
 * @param pool	    Pool.
 * @param name	    Node name.
 *
 * @return	    The new node.
 */
PJ_DECL(pj_xml_node*) pj_xml_node_new(pj_pool_t *pool, const pj_str_t *name);


/**
 * Create new XML attribute.
 *
 * @param pool	    Pool.
 * @param name	    Attribute name.
 * @param value	    Attribute value.
 *
 * @return	    The new XML attribute.
 */
PJ_DECL(pj_xml_attr*) pj_xml_attr_new(pj_pool_t *pool, const pj_str_t *name,
				      const pj_str_t *value);

/**
 * Add node to another node.
 *
 * @param parent    Parent node.
 * @param node	    Node to be added to parent.
 */
PJ_DECL(void) pj_xml_add_node( pj_xml_node *parent, pj_xml_node *node );


/**
 * Add attribute to a node.
 *
 * @param node	    Node.
 * @param attr	    Attribute to add to node.
 */
PJ_DECL(void) pj_xml_add_attr( pj_xml_node *node, pj_xml_attr *attr );

/**
 * Find first direct child node with the specified name.
 *
 * @param parent    Parent node.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
PJ_DECL(pj_xml_node*) pj_xml_find_node(const pj_xml_node *parent, 
				       const pj_str_t *name);

/**
 * Find next direct child node with the specified name.
 *
 * @param parent    Parent node.
 * @param node	    node->next is the starting point.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
PJ_DECL(pj_xml_node*) pj_xml_find_next_node(const pj_xml_node *parent, 
					    const pj_xml_node *node,
					    const pj_str_t *name);

/**
 * Recursively find the first node with the specified name in the child nodes
 * and their children.
 *
 * @param parent    Parent node.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
PJ_DECL(pj_xml_node*) pj_xml_find_node_rec(const pj_xml_node *parent, 
					   const pj_str_t *name);


/**
 * Find first attribute within a node with the specified name and optional 
 * value.
 *
 * @param node	    XML Node.
 * @param name	    Attribute name to find.
 * @param value	    Optional value to match.
 *
 * @return	    XML attribute found, or NULL.
 */
PJ_DECL(pj_xml_attr*) pj_xml_find_attr(const pj_xml_node *node, 
				       const pj_str_t *name,
				       const pj_str_t *value);


/**
 * Find a direct child node with the specified name and match the function.
 *
 * @param parent    Parent node.
 * @param name	    Optional name. If this is NULL, the name will not be
 *		    matched.
 * @param data	    Data to be passed to matching function.
 * @param match	    Optional matching function.
 *
 * @return	    The first matched node, or NULL.
 */
PJ_DECL(pj_xml_node*) pj_xml_find( const pj_xml_node *parent, 
				   const pj_str_t *name,
				   const void *data, 
				   pj_bool_t (*match)(const pj_xml_node *, 
						      const void*));


/**
 * Recursively find a child node with the specified name and match the 
 * function.
 *
 * @param parent    Parent node.
 * @param name	    Optional name. If this is NULL, the name will not be
 *		    matched.
 * @param data	    Data to be passed to matching function.
 * @param match	    Optional matching function.
 *
 * @return	    The first matched node, or NULL.
 */
PJ_DECL(pj_xml_node*) pj_xml_find_rec(const pj_xml_node *parent, 
				      const pj_str_t *name,
				      const void *data, 
				      pj_bool_t (*match)(const pj_xml_node*, 
							 const void*));


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_XML_H__ */
