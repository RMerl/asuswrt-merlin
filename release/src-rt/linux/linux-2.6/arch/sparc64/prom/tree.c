/* $Id: tree.c,v 1.10 1998/01/10 22:39:00 ecd Exp $
 * tree.c: Basic device tree traversal/scanning for the Linux
 *         prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996,1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/openprom.h>
#include <asm/oplib.h>

/* Return the child of node 'node' or zero if no this node has no
 * direct descendent.
 */
__inline__ int
__prom_getchild(int node)
{
	return p1275_cmd ("child", P1275_INOUT(1, 1), node);
}

__inline__ int
prom_getchild(int node)
{
	int cnode;

	if(node == -1) return 0;
	cnode = __prom_getchild(node);
	if(cnode == -1) return 0;
	return (int)cnode;
}

__inline__ int
prom_getparent(int node)
{
	int cnode;

	if(node == -1) return 0;
	cnode = p1275_cmd ("parent", P1275_INOUT(1, 1), node);
	if(cnode == -1) return 0;
	return (int)cnode;
}

/* Return the next sibling of node 'node' or zero if no more siblings
 * at this level of depth in the tree.
 */
__inline__ int
__prom_getsibling(int node)
{
	return p1275_cmd(prom_peer_name, P1275_INOUT(1, 1), node);
}

__inline__ int
prom_getsibling(int node)
{
	int sibnode;

	if (node == -1)
		return 0;
	sibnode = __prom_getsibling(node);
	if (sibnode == -1)
		return 0;

	return sibnode;
}

/* Return the length in bytes of property 'prop' at node 'node'.
 * Return -1 on error.
 */
__inline__ int
prom_getproplen(int node, const char *prop)
{
	if((!node) || (!prop)) return -1;
	return p1275_cmd ("getproplen", 
			  P1275_ARG(1,P1275_ARG_IN_STRING)|
			  P1275_INOUT(2, 1), 
			  node, prop);
}

/* Acquire a property 'prop' at node 'node' and place it in
 * 'buffer' which has a size of 'bufsize'.  If the acquisition
 * was successful the length will be returned, else -1 is returned.
 */
__inline__ int
prom_getproperty(int node, const char *prop, char *buffer, int bufsize)
{
	int plen;

	plen = prom_getproplen(node, prop);
	if ((plen > bufsize) || (plen == 0) || (plen == -1)) {
		return -1;
	} else {
		/* Ok, things seem all right. */
		return p1275_cmd(prom_getprop_name, 
				 P1275_ARG(1,P1275_ARG_IN_STRING)|
				 P1275_ARG(2,P1275_ARG_OUT_BUF)|
				 P1275_INOUT(4, 1), 
				 node, prop, buffer, P1275_SIZE(plen));
	}
}

/* Acquire an integer property and return its value.  Returns -1
 * on failure.
 */
__inline__ int
prom_getint(int node, const char *prop)
{
	int intprop;

	if(prom_getproperty(node, prop, (char *) &intprop, sizeof(int)) != -1)
		return intprop;

	return -1;
}

/* Acquire an integer property, upon error return the passed default
 * integer.
 */

int
prom_getintdefault(int node, const char *property, int deflt)
{
	int retval;

	retval = prom_getint(node, property);
	if(retval == -1) return deflt;

	return retval;
}

/* Acquire a boolean property, 1=TRUE 0=FALSE. */
int
prom_getbool(int node, const char *prop)
{
	int retval;

	retval = prom_getproplen(node, prop);
	if(retval == -1) return 0;
	return 1;
}

/* Acquire a property whose value is a string, returns a null
 * string on error.  The char pointer is the user supplied string
 * buffer.
 */
void
prom_getstring(int node, const char *prop, char *user_buf, int ubuf_size)
{
	int len;

	len = prom_getproperty(node, prop, user_buf, ubuf_size);
	if(len != -1) return;
	user_buf[0] = 0;
	return;
}


/* Does the device at node 'node' have name 'name'?
 * YES = 1   NO = 0
 */
int
prom_nodematch(int node, const char *name)
{
	char namebuf[128];
	prom_getproperty(node, "name", namebuf, sizeof(namebuf));
	if(strcmp(namebuf, name) == 0) return 1;
	return 0;
}

/* Search siblings at 'node_start' for a node with name
 * 'nodename'.  Return node if successful, zero if not.
 */
int
prom_searchsiblings(int node_start, const char *nodename)
{

	int thisnode, error;
	char promlib_buf[128];

	for(thisnode = node_start; thisnode;
	    thisnode=prom_getsibling(thisnode)) {
		error = prom_getproperty(thisnode, "name", promlib_buf,
					 sizeof(promlib_buf));
		/* Should this ever happen? */
		if(error == -1) continue;
		if(strcmp(nodename, promlib_buf)==0) return thisnode;
	}

	return 0;
}

/* Return the first property type for node 'node'.
 * buffer should be at least 32B in length
 */
__inline__ char *
prom_firstprop(int node, char *buffer)
{
	*buffer = 0;
	if(node == -1) return buffer;
	p1275_cmd ("nextprop", P1275_ARG(2,P1275_ARG_OUT_32B)|
			       P1275_INOUT(3, 0), 
			       node, (char *) 0x0, buffer);
	return buffer;
}

/* Return the property type string after property type 'oprop'
 * at node 'node' .  Returns NULL string if no more
 * property types for this node.
 */
__inline__ char *
prom_nextprop(int node, const char *oprop, char *buffer)
{
	char buf[32];

	if(node == -1) {
		*buffer = 0;
		return buffer;
	}
	if (oprop == buffer) {
		strcpy (buf, oprop);
		oprop = buf;
	}
	p1275_cmd ("nextprop", P1275_ARG(1,P1275_ARG_IN_STRING)|
				    P1275_ARG(2,P1275_ARG_OUT_32B)|
				    P1275_INOUT(3, 0), 
				    node, oprop, buffer); 
	return buffer;
}

int
prom_finddevice(const char *name)
{
	if (!name)
		return 0;
	return p1275_cmd(prom_finddev_name,
			 P1275_ARG(0,P1275_ARG_IN_STRING)|
			 P1275_INOUT(1, 1), 
			 name);
}

int prom_node_has_property(int node, const char *prop)
{
	char buf [32];
        
	*buf = 0;
	do {
		prom_nextprop(node, buf, buf);
		if(!strcmp(buf, prop))
			return 1;
	} while (*buf);
	return 0;
}
                                                                                           
/* Set property 'pname' at node 'node' to value 'value' which has a length
 * of 'size' bytes.  Return the number of bytes the prom accepted.
 */
int
prom_setprop(int node, const char *pname, char *value, int size)
{
	if(size == 0) return 0;
	if((pname == 0) || (value == 0)) return 0;
	
	return p1275_cmd ("setprop", P1275_ARG(1,P1275_ARG_IN_STRING)|
					  P1275_ARG(2,P1275_ARG_IN_BUF)|
					  P1275_INOUT(4, 1), 
					  node, pname, value, P1275_SIZE(size));
}

__inline__ int
prom_inst2pkg(int inst)
{
	int node;
	
	node = p1275_cmd ("instance-to-package", P1275_INOUT(1, 1), inst);
	if (node == -1) return 0;
	return node;
}

/* Return 'node' assigned to a particular prom 'path'
 * FIXME: Should work for v0 as well
 */
int
prom_pathtoinode(const char *path)
{
	int node, inst;

	inst = prom_devopen (path);
	if (inst == 0) return 0;
	node = prom_inst2pkg (inst);
	prom_devclose (inst);
	if (node == -1) return 0;
	return node;
}
