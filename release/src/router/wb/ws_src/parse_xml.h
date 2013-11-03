#ifndef __PARSE_XML_H__
#define __PARSE_XML_H__

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

typedef struct _XML_DATA_{
	xmlDocPtr 	doc; 
	xmlNodePtr 	cur;
}XML_DATA_;


typedef int 	(*PROC_XML_DATA)(const char* feild_name, const char* value , void* data_struct, XML_DATA_ * xmldata);


void
parse_child_node (
	XML_DATA_* 		pXmlData,
	void* 			fn_struct,
	PROC_XML_DATA 	proc
	) ;

int parse_node(char *xml_buff, 
#ifdef COMPARE_ROOT_NAME
	char* root_node, 
#endif
	void* fn_struct, 
	size_t fn_struct_size,
 	PROC_XML_DATA proc);

#endif
