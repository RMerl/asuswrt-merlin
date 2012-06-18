
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <stdlib.h>

parse(xmlDocPtr doc, xmlNodePtr cur)
{
	cur = cur->xmlChildrenNode;

	while(cur!=NULL) {
		printf("cur name: %s\n", cur->name);

		if(strcmp(cur->name, "login")==0) 
			parse(doc, cur);

		cur = cur->next;
	}
}

main(int argc, char argv[])
{
	xmlNodePtr cur;
	xmlDocPtr doc;

printf("paser\n");
	doc = xmlParseFile("./token");
        if (doc == NULL) {
		printf("error\n");
		return;
	}
printf("paser1\n");
	cur = xmlDocGetRootElement(doc);

	if (cur == NULL) {
		printf("curl error\n");
		xmlFreeDoc(doc);
		return;
	}

	parse(doc, cur);

	xmlFreeDoc(doc);
	return;
}
