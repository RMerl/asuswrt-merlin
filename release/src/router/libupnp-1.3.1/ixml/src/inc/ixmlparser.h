///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////
 
#ifndef _IXMLPARSER_H
#define _IXMLPARSER_H

#include "ixml.h"
#include "ixmlmembuf.h"

// Parser definitions
#define QUOT        "&quot;"
#define LT          "&lt;"
#define GT          "&gt;"
#define APOS        "&apos;"
#define AMP         "&amp;"
#define ESC_HEX     "&#x"
#define ESC_DEC     "&#"

typedef struct _IXML_NamespaceURI 
{
    char                        *nsURI;
    char                        *prefix;
    struct _IXML_NamespaceURI   *nextNsURI;
} IXML_NamespaceURI;


typedef struct _IXML_ElementStack
{
    char                    *element;
    char                    *prefix;
    char                    *namespaceUri;
    IXML_NamespaceURI            *pNsURI;
    struct _IXML_ElementStack    *nextElement;
} IXML_ElementStack;


typedef enum
{
    eELEMENT,
    eATTRIBUTE,
    eCONTENT,
} PARSER_STATE;

typedef struct _Parser
{
    char            *dataBuffer;	//data buffer
    char            *curPtr;		//ptr to the token parsed 
    char            *savePtr;		//Saves for backup
    ixml_membuf     lastElem;
    ixml_membuf     tokenBuf;    

    IXML_Node           *pNeedPrefixNode;
    IXML_ElementStack   *pCurElement;
    IXML_Node           *currentNodePtr;
    PARSER_STATE        state;

    BOOL                bHasTopLevel;

} Parser;



int     Parser_LoadDocument( IXML_Document **retDoc, char * xmlFile, BOOL file);
BOOL    Parser_isValidXmlName( DOMString name);
int     Parser_setNodePrefixAndLocalName(IXML_Node *newIXML_NodeIXML_Attr);
void    Parser_freeNodeContent( IXML_Node *IXML_Nodeptr);

void    Parser_setErrorChar( char c );

void    ixmlAttr_free(IXML_Attr *attrNode);
void    ixmlAttr_init(IXML_Attr *attrNode);

int     ixmlElement_setTagName(IXML_Element *element, char *tagName);

void    ixmlNamedNodeMap_init(IXML_NamedNodeMap *nnMap);
int     ixmlNamedNodeMap_addToNamedNodeMap(IXML_NamedNodeMap **nnMap, IXML_Node *add);

void    ixmlNode_init(IXML_Node *IXML_Nodeptr);
BOOL    ixmlNode_compare(IXML_Node *srcIXML_Node, IXML_Node *destIXML_Node);

void    ixmlNode_getElementsByTagName( IXML_Node *n, char *tagname, IXML_NodeList **list);
void    ixmlNode_getElementsByTagNameNS( IXML_Node *IXML_Node, char *namespaceURI,
                char *localName, IXML_NodeList **list);

int     ixmlNode_setNodeProperties(IXML_Node* node, IXML_Node *src);
int     ixmlNode_setNodeName( IXML_Node* node, DOMString qualifiedName);

void    ixmlNodeList_init(IXML_NodeList *nList);
int     ixmlNodeList_addToNodeList(IXML_NodeList **nList, IXML_Node *add);

#endif  // _IXMLPARSER_H

