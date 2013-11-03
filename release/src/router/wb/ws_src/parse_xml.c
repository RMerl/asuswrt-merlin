#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/*
 * COMPAT using xml-config --cflags to get the include path this will
 * work with both 
 */
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <parse_xml.h>
#include <log.h>
#define XML_DBG 1

int
parse_node(char *xml_buff,
#ifdef COMPARE_ROOT_NAME
	char* root_node, 
#endif
	void* fn_struct, 
	size_t fn_struct_size,
 	PROC_XML_DATA proc)
{
    xmlDocPtr doc;        
    xmlNsPtr ns;
    xmlNodePtr cur;
    //void* ret;

#ifdef LIBXML_SAX1_ENABLED
    /*
     * build an XML tree from a the file;
     */
    //doc = xmlParseFile(filename);
    printf("%s>>>>>>>>>>>>>>>>>>>>>>>>parse Memory>>>\n %s\n", __func__, xml_buff);
	int xml_len= strlen(xml_buff);
		//strcpy(test, xml_buff);

//	Cdbg(XML_DBG, "go Parse xml Memory len=%d", xml_len);
    doc = xmlParseMemory(xml_buff, xml_len);
//    doc = xmlParseMemory(test, strlen(test));
    if (doc == NULL) return(NULL);
#else
    /*
     * the library has been compiled without some of the old interfaces
     */
    return(NULL);
#endif /* LIBXML_SAX1_ENABLED */

    /*
     * Check the document is of the right kind
     */
    printf("%s>>>>>>>>>>>>>>>>>>>>>>>>Get Root Doc>>>\n", __func__);
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        fprintf(stderr,"empty document\n");
		xmlFreeDoc(doc);
		return(NULL);
    }
	/*
    ns = xmlSearchNsByHref(doc, cur,
	    (const xmlChar *) "http://www.gnome.org/some-location");
    if (ns == NULL) {
        fprintf(stderr,
	        "document of the wrong type, GJob Namespace not found\n");
	xmlFreeDoc(doc);
	return(NULL);
    }
	*/
#ifdef COMPARE_ROOT_NAME
    if (xmlStrcmp(cur->name, (const xmlChar *) root_node)) {
        fprintf(stderr,"document of the wrong type, root node != Helping");
		xmlFreeDoc(doc);
		return(NULL);
    }
#else
    const xmlChar* root_node = cur->name;
#endif
    /*
     * Allocate the structure to be returned.
     */
    #if 0
    ret = (plogin) malloc(sizeof(login));
    if (ret == NULL) {
        fprintf(stderr,"out of memory\n");
	xmlFreeDoc(doc);
	return(NULL);
    }
    memset(ret, 0, sizeof(plogin));
	#endif
    /*
     * Now, walk the tree.
     */
    /* First level we expect just Jobs */
    cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/*
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Person"))		
			ret->name = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);        
			*/
		Cdbg(XML_DBG, " GetString ... cur =%p", cur);
		xmlChar* value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		Cdbg(XML_DBG, " GetString ... value =%s", value);
		if(strlen((const char*)cur->name)){
			//if(cur->xmlChildrenNode)  printf("childrenNode =%s\n", cur->xmlChildrenNode->name);
			//if(proc) (*proc)(cur->name,xmlNodeListGetString(doc, cur->xmlChildrenNode, 1), fn_struct);
			XML_DATA_ xml_data;
			xml_data.doc = doc;
			xml_data.cur = cur;
			Cdbg(XML_DBG, "proc .....call proc start");
			if(proc) (proc)((const char*)cur->name, (const char*)value, fn_struct, &xml_data);
			Cdbg(XML_DBG, "proc .....call proc end");
			//if(proc) (*proc)(cur->name,xmlNodeListGetString(doc, cur->xmlChildrenNode, 1), NULL);
		}
		Cdbg(XML_DBG, "free ....");
		if(value) free(value);
		Cdbg(XML_DBG, "cur next .... cur=%p", cur);
		cur = cur->next;
		Cdbg(XML_DBG, "cur next ....cur =%p", cur);
	}

    /* Second level is a list of Job, but be laxist */
    //return(ret);
	Cdbg(WB_DBG, " end>>>>>");
    return 0;
}

void
parse_child_node (
	//xmlDocPtr doc, 
	//xmlNodePtr cur,  
	XML_DATA_* 		pXmlData,
	void* 			fn_struct,
	PROC_XML_DATA 	proc) 
{	
	xmlChar *key;
	xmlDocPtr 	doc =  	pXmlData->doc;
	xmlNodePtr 	cur =	pXmlData->cur;
	cur = cur->xmlChildrenNode;
//	if(proc) (*proc)("QQQ","OOO", fn_struct, pXmlData);
#if 1
	while (cur != NULL) {
	 	if(strlen((const char*)cur->name)){
			// charles: don't parse the name of "text", that is useless key.
//	 		if(xmlStrcmp(cur->name, (const xmlChar*)"text")){
				//if(cur->xmlChildrenNode)  printf("childrenNode =%s\n", cur->xmlChildrenNode->name);
				//if(proc) (*proc)(cur->name,xmlNodeListGetString(doc, cur->xmlChildrenNode, 1), fn_struct);
				xmlChar* value = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				Cdbg(XML_DBG, "name =%s, value=%s, ", cur->name, value);	
				if(proc) (*proc)((const char*)cur->name, (const char*)value, fn_struct, pXmlData);
				if(value) free(value);
			//	Cdbg(XML_DBG, "free end");
			//}
			//if(proc) (*proc)(cur->name,xmlNodeListGetString(doc, cur->xmlChildrenNode, 1), NULL);
		}
	 	/*
	    if ((!xmlStrcmp(cur->name, (const xmlChar *)"keyword"))) {
		    key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
		    printf("keyword: %s\n", key);
		    xmlFree(key);
 	    }*/
		cur = cur->next;
	}
#endif
    return;
}
