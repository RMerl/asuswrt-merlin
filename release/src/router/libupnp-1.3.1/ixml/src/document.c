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

#include <stdio.h>
#include <stdlib.h>

#include "ixmlparser.h"

/*================================================================
*   ixmlDocument_init
*       It initialize the document structure.
*       External function.
*   
*=================================================================*/
void
ixmlDocument_init( IN IXML_Document * doc )
{
    memset( doc, 0, sizeof( IXML_Document ) );
}

/*================================================================
*   ixmlDocument_free
*       It frees the whole document tree.
*       External function.
*
*=================================================================*/
void
ixmlDocument_free( IN IXML_Document * doc )
{
    if( doc != NULL ) {
        ixmlNode_free( ( IXML_Node * ) doc );
    }

}

/*================================================================
*   ixmlDocument_setOwnerDocument
*       
*       When this function is called first time, nodeptr is the root
*       of the subtree, so it is not necessay to do two steps
*       recursion.
*        
*       Internal function called by ixmlDocument_importNode
*
*=================================================================*/
void
ixmlDocument_setOwnerDocument( IN IXML_Document * doc,
                               IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        nodeptr->ownerDocument = doc;
        ixmlDocument_setOwnerDocument( doc,
                                       ixmlNode_getFirstChild( nodeptr ) );
        ixmlDocument_setOwnerDocument( doc,
                                       ixmlNode_getNextSibling
                                       ( nodeptr ) );
    }
}

/*================================================================
*   ixmlDocument_importNode
*       Imports a node from another document to this document. The
*       returned node has no parent; (parentNode is null). The source
*       node is not altered or removed from the original document;
*       this method creates a new copy of the source node.
 
*       For all nodes, importing a node creates a node object owned
*       by the importing document, with attribute values identical to
*       the source node's nodeName and nodeType, plus the attributes
*       related to namespaces (prefix, localName, and namespaceURI).
*       As in the cloneNode operation on a node, the source node is
*       not altered.
*       
*       External function.
*
*=================================================================*/
int
ixmlDocument_importNode( IN IXML_Document * doc,
                         IN IXML_Node * importNode,
                         IN BOOL deep,
                         OUT IXML_Node ** rtNode )
{
    unsigned short nodeType;
    IXML_Node *newNode;

    *rtNode = NULL;

    if( ( doc == NULL ) || ( importNode == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    nodeType = ixmlNode_getNodeType( importNode );
    if( nodeType == eDOCUMENT_NODE ) {
        return IXML_NOT_SUPPORTED_ERR;
    }

    newNode = ixmlNode_cloneNode( importNode, deep );
    if( newNode == NULL ) {
        return IXML_FAILED;
    }

    ixmlDocument_setOwnerDocument( doc, newNode );
    *rtNode = newNode;

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlDocument_createElementEx
*       Creates an element of the type specified. 
*       External function.
*   Parameters:
*       doc:        pointer to document
*       tagName:    The name of the element, it is case-sensitive.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either doc or tagName is NULL
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createElementEx( IN IXML_Document * doc,
                              IN const DOMString tagName,
                              OUT IXML_Element ** rtElement )
{

    int errCode = IXML_SUCCESS;
    IXML_Element *newElement = NULL;

    if( ( doc == NULL ) || ( tagName == NULL ) ) {
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    newElement = ( IXML_Element * ) malloc( sizeof( IXML_Element ) );
    if( newElement == NULL ) {
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    ixmlElement_init( newElement );
    newElement->tagName = strdup( tagName );
    if( newElement->tagName == NULL ) {
        ixmlElement_free( newElement );
        newElement = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }
    // set the node fields 
    newElement->n.nodeType = eELEMENT_NODE;
    newElement->n.nodeName = strdup( tagName );
    if( newElement->n.nodeName == NULL ) {
        ixmlElement_free( newElement );
        newElement = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    newElement->n.ownerDocument = doc;

  ErrorHandler:
    *rtElement = newElement;
    return errCode;

}

/*================================================================
*   ixmlDocument_createElement
*       Creates an element of the type specified. 
*       External function.
*   Parameters:
*       doc:        pointer to document
*       tagName:    The name of the element, it is case-sensitive.
*   Return Value: 
*       A new element object with the nodeName set to tagName, and
*       localName, prefix and namespaceURI set to null.
*
*=================================================================*/
IXML_Element *
ixmlDocument_createElement( IN IXML_Document * doc,
                            IN const DOMString tagName )
{
    IXML_Element *newElement = NULL;

    ixmlDocument_createElementEx( doc, tagName, &newElement );
    return newElement;

}

/*================================================================
*   ixmlDocument_createDocumentEx
*       Creates an document object
*       Internal function.
*   Parameters:
*       rtDoc:  the document created or NULL on failure
*   Return Value:
*       IXML_SUCCESS
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createDocumentEx( OUT IXML_Document ** rtDoc )
{
    IXML_Document *doc;
    int errCode = IXML_SUCCESS;

    doc = NULL;
    doc = ( IXML_Document * ) malloc( sizeof( IXML_Document ) );
    if( doc == NULL ) {
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    ixmlDocument_init( doc );

    doc->n.nodeName = strdup( DOCUMENTNODENAME );
    if( doc->n.nodeName == NULL ) {
        ixmlDocument_free( doc );
        doc = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    doc->n.nodeType = eDOCUMENT_NODE;
    doc->n.ownerDocument = doc;

  ErrorHandler:
    *rtDoc = doc;
    return errCode;
}

/*================================================================
*   ixmlDocument_createDocument
*       Creates an document object
*       Internal function.
*   Parameters:
*       none
*   Return Value:
*       A new document object with the nodeName set to "#document".
*
*=================================================================*/
IXML_Document *
ixmlDocument_createDocument(  )
{
    IXML_Document *doc = NULL;

    ixmlDocument_createDocumentEx( &doc );

    return doc;

}

/*================================================================
*   ixmlDocument_createTextNodeEx
*       Creates an text node. 
*       External function.
*   Parameters:
*       data: text data for the text node. It is stored in nodeValue field.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either doc or data is NULL
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createTextNodeEx( IN IXML_Document * doc,
                               IN const char *data,
                               OUT IXML_Node ** textNode )
{
    IXML_Node *returnNode;
    int rc = IXML_SUCCESS;

    returnNode = NULL;
    if( ( doc == NULL ) || ( data == NULL ) ) {
        rc = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    returnNode = ( IXML_Node * ) malloc( sizeof( IXML_Node ) );
    if( returnNode == NULL ) {
        rc = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }
    // initialize the node
    ixmlNode_init( returnNode );

    returnNode->nodeName = strdup( TEXTNODENAME );
    if( returnNode->nodeName == NULL ) {
        ixmlNode_free( returnNode );
        returnNode = NULL;
        rc = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }
    // add in node value
    if( data != NULL ) {
        returnNode->nodeValue = strdup( data );
        if( returnNode->nodeValue == NULL ) {
            ixmlNode_free( returnNode );
            returnNode = NULL;
            rc = IXML_INSUFFICIENT_MEMORY;
            goto ErrorHandler;
        }
    }

    returnNode->nodeType = eTEXT_NODE;
    returnNode->ownerDocument = doc;

  ErrorHandler:
    *textNode = returnNode;
    return rc;

}

/*================================================================
*   ixmlDocument_createTextNode
*       Creates an text node. 
*       External function.
*   Parameters:
*       data: text data for the text node. It is stored in nodeValue field.
*   Return Value:
*       The new text node.
*
*=================================================================*/
IXML_Node *
ixmlDocument_createTextNode( IN IXML_Document * doc,
                             IN const char *data )
{
    IXML_Node *returnNode = NULL;

    ixmlDocument_createTextNodeEx( doc, data, &returnNode );

    return returnNode;
}

/*================================================================
*   ixmlDocument_createAttributeEx
*       Creates an attribute of the given name.             
*       External function.
*   Parameters:
*       name: The name of the Attribute node.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
================================================================*/
int
ixmlDocument_createAttributeEx( IN IXML_Document * doc,
                                IN char *name,
                                OUT IXML_Attr ** rtAttr )
{
    IXML_Attr *attrNode = NULL;
    int errCode = IXML_SUCCESS;

    attrNode = ( IXML_Attr * ) malloc( sizeof( IXML_Attr ) );
    if( attrNode == NULL ) {
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    if( ( doc == NULL ) || ( name == NULL ) ) {
        ixmlAttr_free( attrNode );
        attrNode = NULL;
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    ixmlAttr_init( attrNode );

    attrNode->n.nodeType = eATTRIBUTE_NODE;

    // set the node fields
    attrNode->n.nodeName = strdup( name );
    if( attrNode->n.nodeName == NULL ) {
        ixmlAttr_free( attrNode );
        attrNode = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    attrNode->n.ownerDocument = doc;

  ErrorHandler:
    *rtAttr = attrNode;
    return errCode;

}

/*================================================================
*   ixmlDocument_createAttribute
*       Creates an attribute of the given name.             
*       External function.
*   Parameters:
*       name: The name of the Attribute node.
*   Return Value:   
*       A new attr object with the nodeName attribute set to the
*       given name, and the localName, prefix and namespaceURI set to NULL.
*       The value of the attribute is the empty string.
*
================================================================*/
IXML_Attr *
ixmlDocument_createAttribute( IN IXML_Document * doc,
                              IN char *name )
{
    IXML_Attr *attrNode = NULL;

    ixmlDocument_createAttributeEx( doc, name, &attrNode );
    return attrNode;

}

/*================================================================
*   ixmlDocument_createAttributeNSEx
*       Creates an attrbute of the given name and namespace URI
*       External function.
*   Parameters:
*       namespaceURI: the namespace fo the attribute to create
*       qualifiedName: qualifiedName of the attribute to instantiate
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either doc,namespaceURI or qualifiedName is NULL
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createAttributeNSEx( IN IXML_Document * doc,
                                  IN DOMString namespaceURI,
                                  IN DOMString qualifiedName,
                                  OUT IXML_Attr ** rtAttr )
{
    IXML_Attr *attrNode = NULL;
    int errCode = IXML_SUCCESS;

    if( ( doc == NULL ) || ( namespaceURI == NULL )
        || ( qualifiedName == NULL ) ) {
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    errCode =
        ixmlDocument_createAttributeEx( doc, qualifiedName, &attrNode );
    if( errCode != IXML_SUCCESS ) {
        goto ErrorHandler;
    }
    // set the namespaceURI field 
    attrNode->n.namespaceURI = strdup( namespaceURI );
    if( attrNode->n.namespaceURI == NULL ) {
        ixmlAttr_free( attrNode );
        attrNode = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }
    // set the localName and prefix 
    errCode =
        ixmlNode_setNodeName( ( IXML_Node * ) attrNode, qualifiedName );
    if( errCode != IXML_SUCCESS ) {
        ixmlAttr_free( attrNode );
        attrNode = NULL;
        goto ErrorHandler;
    }

  ErrorHandler:
    *rtAttr = attrNode;
    return errCode;

}

/*================================================================
*   ixmlDocument_createAttributeNS
*       Creates an attrbute of the given name and namespace URI
*       External function.
*   Parameters:
*       namespaceURI: the namespace fo the attribute to create
*       qualifiedName: qualifiedName of the attribute to instantiate
*   Return Value:   
*       Creates an attribute node with the given namespaceURI and
*       qualifiedName. The prefix and localname are extracted from 
*       the qualifiedName. The node value is empty.
*	
*=================================================================*/
IXML_Attr *
ixmlDocument_createAttributeNS( IN IXML_Document * doc,
                                IN DOMString namespaceURI,
                                IN DOMString qualifiedName )
{
    IXML_Attr *attrNode = NULL;

    ixmlDocument_createAttributeNSEx( doc, namespaceURI, qualifiedName,
                                      &attrNode );
    return attrNode;
}

/*================================================================
*   ixmlDocument_createCDATASectionEx
*       Creates an CDATASection node whose value is the specified string
*       External function.
*   Parameters:
*       data: the data for the CDATASection contents.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either doc or data is NULL
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createCDATASectionEx( IN IXML_Document * doc,
                                   IN DOMString data,
                                   OUT IXML_CDATASection ** rtCD )
{
    int errCode = IXML_SUCCESS;
    IXML_CDATASection *cDSectionNode = NULL;

    if( ( doc == NULL ) || ( data == NULL ) ) {
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    cDSectionNode =
        ( IXML_CDATASection * ) malloc( sizeof( IXML_CDATASection ) );
    if( cDSectionNode == NULL ) {
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    ixmlCDATASection_init( cDSectionNode );

    cDSectionNode->n.nodeType = eCDATA_SECTION_NODE;
    cDSectionNode->n.nodeName = strdup( CDATANODENAME );
    if( cDSectionNode->n.nodeName == NULL ) {
        ixmlCDATASection_free( cDSectionNode );
        cDSectionNode = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    cDSectionNode->n.nodeValue = strdup( data );
    if( cDSectionNode->n.nodeValue == NULL ) {
        ixmlCDATASection_free( cDSectionNode );
        cDSectionNode = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    cDSectionNode->n.ownerDocument = doc;

  ErrorHandler:
    *rtCD = cDSectionNode;
    return errCode;

}

/*================================================================
*   ixmlDocument_createCDATASection
*       Creates an CDATASection node whose value is the specified string
*       External function.
*   Parameters:
*       data: the data for the CDATASection contents.
*   Return Value:   
*       The new CDATASection object.
*	
*=================================================================*/
IXML_CDATASection *
ixmlDocument_createCDATASection( IN IXML_Document * doc,
                                 IN DOMString data )
{

    IXML_CDATASection *cDSectionNode = NULL;

    ixmlDocument_createCDATASectionEx( doc, data, &cDSectionNode );
    return cDSectionNode;
}

/*================================================================
*   ixmlDocument_createElementNSEx
*       Creates an element of the given qualified name and namespace URI.
*       External function.
*   Parameters:
*       namespaceURI: the namespace URI of the element to create.
*       qualifiedName: the qualified name of the element to instantiate.
*   Return Value:   
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either doc,namespaceURI or qualifiedName is NULL
*       IXML_INSUFFICIENT_MEMORY:   if not enough memory to finish this operations.
*
*=================================================================*/
int
ixmlDocument_createElementNSEx( IN IXML_Document * doc,
                                IN DOMString namespaceURI,
                                IN DOMString qualifiedName,
                                OUT IXML_Element ** rtElement )
{

    IXML_Element *newElement = NULL;
    int errCode = IXML_SUCCESS;

    if( ( doc == NULL ) || ( namespaceURI == NULL )
        || ( qualifiedName == NULL ) ) {
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    errCode =
        ixmlDocument_createElementEx( doc, qualifiedName, &newElement );
    if( errCode != IXML_SUCCESS ) {
        goto ErrorHandler;
    }
    // set the namespaceURI field 
    newElement->n.namespaceURI = strdup( namespaceURI );
    if( newElement->n.namespaceURI == NULL ) {
        ixmlElement_free( newElement );
        newElement = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }
    // set the localName and prefix 
    errCode =
        ixmlNode_setNodeName( ( IXML_Node * ) newElement, qualifiedName );
    if( errCode != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        newElement = NULL;
        errCode = IXML_INSUFFICIENT_MEMORY;
        goto ErrorHandler;
    }

    newElement->n.nodeValue = NULL;

  ErrorHandler:
    *rtElement = newElement;
    return errCode;

}

/*================================================================
*   ixmlDocument_createElementNS
*       Creates an element of the given qualified name and namespace URI.
*       External function.
*   Parameters:
*       namespaceURI: the namespace URI of the element to create.
*       qualifiedName: the qualified name of the element to instantiate.
*   Return Value:   
*       The new element object with tagName qualifiedName, prefix and
*       localName extraced from qualfiedName, nodeName of qualfiedName,
*	    namespaceURI of namespaceURI.
*
*=================================================================*/
IXML_Element *
ixmlDocument_createElementNS( IN IXML_Document * doc,
                              IN DOMString namespaceURI,
                              IN DOMString qualifiedName )
{
    IXML_Element *newElement = NULL;

    ixmlDocument_createElementNSEx( doc, namespaceURI, qualifiedName,
                                    &newElement );
    return newElement;
}

/*================================================================
*   ixmlDocument_getElementsByTagName
*       Returns a nodeList of all the Elements with a given tag name
*       in the order in which they are encountered in a preorder traversal
*       of the document tree.
*       External function.
*   Parameters:
*       tagName: the name of the tag to match on. The special value "*"
*                matches all tags.
*   Return Value:
*       A new nodeList object containing all the matched Elements.    
*
*=================================================================*/
IXML_NodeList *
ixmlDocument_getElementsByTagName( IN IXML_Document * doc,
                                   IN char *tagName )
{
    IXML_NodeList *returnNodeList = NULL;

    if( ( doc == NULL ) || ( tagName == NULL ) ) {
        return NULL;
    }

    ixmlNode_getElementsByTagName( ( IXML_Node * ) doc, tagName,
                                   &returnNodeList );
    return returnNodeList;
}

/*================================================================
*   ixmlDocument_getElementsByTagNameNS
*       Returns a nodeList of all the Elements with a given local name and
*       namespace URI in the order in which they are encountered in a 
*       preorder traversal of the document tree.
*       External function.
*   Parameters:
*       namespaceURI: the namespace of the elements to match on. The special
*               value "*" matches all namespaces.
*       localName: the local name of the elements to match on. The special
*               value "*" matches all local names.
*   Return Value:
*       A new nodeList object containing all the matched Elements.    
*
*=================================================================*/
IXML_NodeList *
ixmlDocument_getElementsByTagNameNS( IN IXML_Document * doc,
                                     IN DOMString namespaceURI,
                                     IN DOMString localName )
{
    IXML_NodeList *returnNodeList = NULL;

    if( ( doc == NULL ) || ( namespaceURI == NULL )
        || ( localName == NULL ) ) {
        return NULL;
    }

    ixmlNode_getElementsByTagNameNS( ( IXML_Node * ) doc, namespaceURI,
                                     localName, &returnNodeList );
    return returnNodeList;
}

/*================================================================
*   ixmlDocument_getElementById
*       Returns the element whose ID is given by tagName. If no such
*       element exists, returns null. 
*       External function.
*   Parameter:
*       tagName: the tag name for an element.
*   Return Values:
*       The matching element.
*
*=================================================================*/
IXML_Element *
ixmlDocument_getElementById( IN IXML_Document * doc,
                             IN DOMString tagName )
{
    IXML_Element *rtElement = NULL;
    IXML_Node *nodeptr = ( IXML_Node * ) doc;
    const char *name;

    if( ( nodeptr == NULL ) || ( tagName == NULL ) ) {
        return rtElement;
    }

    if( ixmlNode_getNodeType( nodeptr ) == eELEMENT_NODE ) {
        name = ixmlNode_getNodeName( nodeptr );
        if( name == NULL ) {
            return rtElement;
        }

        if( strcmp( tagName, name ) == 0 ) {
            rtElement = ( IXML_Element * ) nodeptr;
            return rtElement;
        } else {
            rtElement = ixmlDocument_getElementById( ( IXML_Document * )
                                                     ixmlNode_getFirstChild
                                                     ( nodeptr ),
                                                     tagName );
            if( rtElement == NULL ) {
                rtElement = ixmlDocument_getElementById( ( IXML_Document
                                                           * )
                                                         ixmlNode_getNextSibling
                                                         ( nodeptr ),
                                                         tagName );
            }
        }
    } else {
        rtElement = ixmlDocument_getElementById( ( IXML_Document * )
                                                 ixmlNode_getFirstChild
                                                 ( nodeptr ), tagName );
        if( rtElement == NULL ) {
            rtElement = ixmlDocument_getElementById( ( IXML_Document * )
                                                     ixmlNode_getNextSibling
                                                     ( nodeptr ),
                                                     tagName );
        }
    }

    return rtElement;
}
