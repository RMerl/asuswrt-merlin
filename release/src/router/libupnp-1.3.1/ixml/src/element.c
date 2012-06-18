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

#include "ixmlparser.h"

/*================================================================
*   ixmlElement_Init
*       Initializes an element node.
*       External function.
*
*=================================================================*/
void
ixmlElement_init( IN IXML_Element * element )
{
    if( element != NULL ) {
        memset( element, 0, sizeof( IXML_Element ) );
    }
}

/*================================================================
*   ixmlElement_getTagName
*       Gets the element node's tagName
*       External function.
*
*=================================================================*/
const DOMString
ixmlElement_getTagName( IN IXML_Element * element )
{

    if( element != NULL ) {
        return element->tagName;
    } else {
        return NULL;
    }
}

/*================================================================
*   ixmlElement_setTagName
*       Sets the given element's tagName.
*   Parameters:
*       tagName: new tagName for the element.
*
*=================================================================*/
int
ixmlElement_setTagName( IN IXML_Element * element,
                        IN char *tagName )
{
    int rc = IXML_SUCCESS;

    assert( ( element != NULL ) && ( tagName != NULL ) );
    if( ( element == NULL ) || ( tagName == NULL ) ) {
        return IXML_FAILED;
    }

    if( element->tagName != NULL ) {
        free( element->tagName );
    }

    element->tagName = strdup( tagName );
    if( element->tagName == NULL ) {
        rc = IXML_INSUFFICIENT_MEMORY;
    }

    return rc;

}

/*================================================================
*   ixmlElement_getAttribute
*       Retrievea an attribute value by name.
*       External function.
*   Parameters:
*       name: the name of the attribute to retrieve.
*   Return Values:
*       attribute value as a string, or the empty string if that attribute
*       does not have a specified value.
*
*=================================================================*/
DOMString
ixmlElement_getAttribute( IN IXML_Element * element,
                          IN DOMString name )
{
    IXML_Node *attrNode;

    if( ( element == NULL ) || ( name == NULL ) ) {
        return NULL;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, name ) == 0 ) { // found it
            return attrNode->nodeValue;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return NULL;
}

/*================================================================
*   ixmlElement_setAttribute
*       Adds a new attribute.  If an attribute with that name is already
*       present in the element, its value is changed to be that of the value
*       parameter. If not, a new attribute is inserted into the element.
*
*       External function.
*   Parameters:
*       name: the name of the attribute to create or alter.
*       value: value to set in string form
*   Return Values:
*       IXML_SUCCESS or failure code.    
*
*=================================================================*/
int
ixmlElement_setAttribute( IN IXML_Element * element,
                          IN char *name,
                          IN char *value )
{
    IXML_Node *attrNode;
    IXML_Attr *newAttrNode;
    short errCode = IXML_SUCCESS;

    if( ( element == NULL ) || ( name == NULL ) || ( value == NULL ) ) {
        errCode = IXML_INVALID_PARAMETER;
        goto ErrorHandler;
    }

    if( Parser_isValidXmlName( name ) == FALSE ) {
        errCode = IXML_INVALID_CHARACTER_ERR;
        goto ErrorHandler;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, name ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode == NULL ) {    // add a new attribute
        errCode =
            ixmlDocument_createAttributeEx( ( IXML_Document * ) element->n.
                                            ownerDocument, name,
                                            &newAttrNode );
        if( errCode != IXML_SUCCESS ) {
            goto ErrorHandler;
        }

        attrNode = ( IXML_Node * ) newAttrNode;

        attrNode->nodeValue = strdup( value );
        if( attrNode->nodeValue == NULL ) {
            ixmlAttr_free( newAttrNode );
            errCode = IXML_INSUFFICIENT_MEMORY;
            goto ErrorHandler;
        }

        errCode =
            ixmlElement_setAttributeNode( element, newAttrNode, NULL );
        if( errCode != IXML_SUCCESS ) {
            ixmlAttr_free( newAttrNode );
            goto ErrorHandler;
        }

    } else {
        if( attrNode->nodeValue != NULL ) { // attribute name has a value already
            free( attrNode->nodeValue );
        }

        attrNode->nodeValue = strdup( value );
        if( attrNode->nodeValue == NULL ) {
            errCode = IXML_INSUFFICIENT_MEMORY;
        }
    }

  ErrorHandler:
    return errCode;
}

/*================================================================
*   ixmlElement_removeAttribute
*       Removes an attribute value by name. The attribute node is
*       not removed.
*       External function.
*   Parameters:
*       name: the name of the attribute to remove.
*   Return Values:
*       IXML_SUCCESS or error code.
*
*=================================================================*/
int
ixmlElement_removeAttribute( IN IXML_Element * element,
                             IN char *name )
{

    IXML_Node *attrNode;

    if( ( element == NULL ) || ( name == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, name ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode != NULL ) {    // has the attribute
        if( attrNode->nodeValue != NULL ) {
            free( attrNode->nodeValue );
            attrNode->nodeValue = NULL;
        }
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlElement_getAttributeNode
*       Retrieve an attribute node by name.
*       External function.        
*   Parameters:
*       name: the name(nodeName) of the attribute to retrieve.
*   Return Value:
*       The attr node with the specified name (nodeName) or NULL if
*       there is no such attribute.  
*
*=================================================================*/
IXML_Attr *
ixmlElement_getAttributeNode( IN IXML_Element * element,
                              IN char *name )
{

    IXML_Node *attrNode;

    if( ( element == NULL ) || ( name == NULL ) ) {
        return NULL;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, name ) == 0 ) { // found it
            break;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return ( IXML_Attr * ) attrNode;

}

/*================================================================
*   ixmlElement_setAttributeNode
*       Adds a new attribute node.  If an attribute with that name(nodeName)
*       is already present in the element, it is replaced by the new one.
*       External function.
*   Parameters:
*       The attr node to add to the attribute list.
*   Return Value:
*       if newAttr replaces an existing attribute, the replaced
*       attr node is returned, otherwise NULL is returned.           
*
*=================================================================*/
int
ixmlElement_setAttributeNode( IN IXML_Element * element,
                              IN IXML_Attr * newAttr,
                              OUT IXML_Attr ** rtAttr )
{

    IXML_Node *attrNode;
    IXML_Node *node;
    IXML_Node *nextAttr = NULL;
    IXML_Node *prevAttr = NULL;
    IXML_Node *preSib,
     *nextSib;

    if( ( element == NULL ) || ( newAttr == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    if( newAttr->n.ownerDocument != element->n.ownerDocument ) {
        return IXML_WRONG_DOCUMENT_ERR;
    }

    if( newAttr->ownerElement != NULL ) {
        return IXML_INUSE_ATTRIBUTE_ERR;
    }

    newAttr->ownerElement = element;
    node = ( IXML_Node * ) newAttr;

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, node->nodeName ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode != NULL )      // already present, will replace by newAttr
    {
        preSib = attrNode->prevSibling;
        nextSib = attrNode->nextSibling;

        if( preSib != NULL ) {
            preSib->nextSibling = node;
        }

        if( nextSib != NULL ) {
            nextSib->prevSibling = node;
        }

        if( element->n.firstAttr == attrNode ) {
            element->n.firstAttr = node;
        }

        if( rtAttr != NULL ) {
            *rtAttr = ( IXML_Attr * ) attrNode;
        }
    } else                      // add this attribute 
    {
        if( element->n.firstAttr != NULL ) {
            prevAttr = element->n.firstAttr;
            nextAttr = prevAttr->nextSibling;
            while( nextAttr != NULL ) {
                prevAttr = nextAttr;
                nextAttr = prevAttr->nextSibling;
            }
            prevAttr->nextSibling = node;
            node->prevSibling = prevAttr;
        } else                  // this is the first attribute node
        {
            element->n.firstAttr = node;
            node->prevSibling = NULL;
            node->nextSibling = NULL;
        }

        if( rtAttr != NULL ) {
            *rtAttr = NULL;
        }
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlElement_findAttributeNode
*       Find a attribute node whose contents are the same as the oldAttr.
*       Internal only to parser.
*   Parameter:
*       oldAttr: the attribute node to match
*   Return:
*       if found it, the attribute node is returned,
*       otherwise, return NULL.
*
*=================================================================*/
IXML_Node *
ixmlElement_findAttributeNode( IN IXML_Element * element,
                               IN IXML_Attr * oldAttr )
{
    IXML_Node *attrNode;
    IXML_Node *oldAttrNode = ( IXML_Node * ) oldAttr;

    assert( ( element != NULL ) && ( oldAttr != NULL ) );

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) { // parentNode, prevSib, nextSib and ownerDocument doesn't matter
        if( ixmlNode_compare( attrNode, oldAttrNode ) == TRUE ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return attrNode;

}

/*================================================================
*   ixmlElement_removeAttributeNode	
*       Removes the specified attribute node.
*       External function.
*
*   Parameters:
*       oldAttr: the attr node to remove from the attribute list.
*       
*   Return Value:
*       IXML_SUCCESS or failure
*
*=================================================================*/
int
ixmlElement_removeAttributeNode( IN IXML_Element * element,
                                 IN IXML_Attr * oldAttr,
                                 OUT IXML_Attr ** rtAttr )
{
    IXML_Node *attrNode;
    IXML_Node *preSib,
     *nextSib;

    if( ( element == NULL ) || ( oldAttr == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    attrNode = ixmlElement_findAttributeNode( element, oldAttr );
    if( attrNode != NULL ) {    // has the attribute
        preSib = attrNode->prevSibling;
        nextSib = attrNode->nextSibling;

        if( preSib != NULL ) {
            preSib->nextSibling = nextSib;
        }

        if( nextSib != NULL ) {
            nextSib->prevSibling = preSib;
        }

        if( element->n.firstAttr == attrNode ) {
            element->n.firstAttr = nextSib;
        }

        /* ( IXML_Attr * ) */ attrNode->parentNode = NULL;
        /* ( IXML_Attr * ) */ attrNode->prevSibling = NULL;
        /* ( IXML_Attr * ) */ attrNode->nextSibling = NULL;
        *rtAttr = ( IXML_Attr * ) attrNode;
        return IXML_SUCCESS;

    } else {
        return IXML_NOT_FOUND_ERR;
    }

}

/*================================================================
*   ixmlElement_getElementsByTagName
*       Returns a nodeList of all descendant Elements with a given
*       tag name, in the order in which they are encountered in a preorder
*       traversal of this element tree.
*       External function.
*
*   Parameters:
*       tagName: the name of the tag to match on. The special value "*"
*       matches all tags.
*
*   Return Value:
*       a nodeList of matching element nodes.
*
*=================================================================*/
IXML_NodeList *
ixmlElement_getElementsByTagName( IN IXML_Element * element,
                                  IN char *tagName )
{
    IXML_NodeList *returnNodeList = NULL;

    if( ( element != NULL ) && ( tagName != NULL ) ) {
        ixmlNode_getElementsByTagName( ( IXML_Node * ) element, tagName,
                                       &returnNodeList );
    }
    return returnNodeList;
}

/*================================================================
*   ixmlElement_getAttributeNS
*       Retrieves an attribute value by local name and namespace URI.
*       External function.
*
*   Parameters:
*       namespaceURI: the namespace URI of the attribute to retrieve.
*       localName: the local name of the attribute to retrieve.
*
*   Return Value:
*       the attr value as a string, or NULL if that attribute does
*       not have the specified value.
*
*=================================================================*/
DOMString
ixmlElement_getAttributeNS( IN IXML_Element * element,
                            IN DOMString namespaceURI,
                            IN DOMString localName )
{
    IXML_Node *attrNode;

    if( ( element == NULL ) || ( namespaceURI == NULL )
        || ( localName == NULL ) ) {
        return NULL;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, localName ) == 0 && strcmp( attrNode->namespaceURI, namespaceURI ) == 0 ) {    // found it
            return attrNode->nodeValue;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return NULL;

}

/*================================================================
*   ixmlElement_setAttributeNS
*       Adds a new attribute. If an attribute with the same local name
*       and namespace URI is already present on the element, its prefix
*       is changed to be the prefix part of the qualifiedName, and its
*       value is changed to be the value parameter.  This value is a
*       simple string.
*       External function.
*
*   Parameter:
*       namespaceURI: the namespace of the attribute to create or alter.
*       qualifiedName: the qualified name of the attribute to create or alter.
*       value: the value to set in string form.
*
*   Return Value:
*       IXML_SUCCESS or failure 
*
*=================================================================*/
int
ixmlElement_setAttributeNS( IN IXML_Element * element,
                            IN DOMString namespaceURI,
                            IN DOMString qualifiedName,
                            IN DOMString value )
{
    IXML_Node *attrNode = NULL;
    IXML_Node newAttrNode;
    IXML_Attr *newAttr;
    int rc;

    if( ( element == NULL ) || ( namespaceURI == NULL ) ||
        ( qualifiedName == NULL ) || ( value == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    if( Parser_isValidXmlName( qualifiedName ) == FALSE ) {
        return IXML_INVALID_CHARACTER_ERR;
    }

    ixmlNode_init( &newAttrNode );

    newAttrNode.nodeName = strdup( qualifiedName );
    if( newAttrNode.nodeName == NULL ) {
        return IXML_INSUFFICIENT_MEMORY;
    }

    rc = Parser_setNodePrefixAndLocalName( &newAttrNode );
    if( rc != IXML_SUCCESS ) {
        Parser_freeNodeContent( &newAttrNode );
        return rc;
    }
    // see DOM 2 spec page 59
    if( ( newAttrNode.prefix != NULL && namespaceURI == NULL ) ||
        ( strcmp( newAttrNode.prefix, "xml" ) == 0 &&
          strcmp( namespaceURI,
                  "http://www.w3.org/XML/1998/namespace" ) != 0 )
        || ( strcmp( qualifiedName, "xmlns" ) == 0
             && strcmp( namespaceURI,
                        "http://www.w3.org/2000/xmlns/" ) != 0 ) ) {
        Parser_freeNodeContent( &newAttrNode );
        return IXML_NAMESPACE_ERR;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, newAttrNode.localName ) == 0 &&
            strcmp( attrNode->namespaceURI, namespaceURI ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode != NULL ) {
        if( attrNode->prefix != NULL ) {
            free( attrNode->prefix );   // remove the old prefix
        }
        // replace it with the new prefix
        attrNode->prefix = strdup( newAttrNode.prefix );
        if( attrNode->prefix == NULL ) {
            Parser_freeNodeContent( &newAttrNode );
            return IXML_INSUFFICIENT_MEMORY;
        }

        if( attrNode->nodeValue != NULL ) {
            free( attrNode->nodeValue );
        }

        attrNode->nodeValue = strdup( value );
        if( attrNode->nodeValue == NULL ) {
            free( attrNode->prefix );
            Parser_freeNodeContent( &newAttrNode );
            return IXML_INSUFFICIENT_MEMORY;
        }

    } else {
        // add a new attribute
        rc = ixmlDocument_createAttributeNSEx( ( IXML_Document * )
                                               element->n.ownerDocument,
                                               namespaceURI, qualifiedName,
                                               &newAttr );
        if( rc != IXML_SUCCESS ) {
            return rc;
        }

        newAttr->n.nodeValue = strdup( value );
        if( newAttr->n.nodeValue == NULL ) {
            ixmlAttr_free( newAttr );
            return IXML_INSUFFICIENT_MEMORY;
        }

        if( ixmlElement_setAttributeNodeNS( element, newAttr, NULL ) !=
            IXML_SUCCESS ) {
            ixmlAttr_free( newAttr );
            return IXML_FAILED;
        }

    }

    Parser_freeNodeContent( &newAttrNode );
    return IXML_SUCCESS;
}

/*================================================================
*   ixmlElement_removeAttributeNS
*       Removes an attribute by local name and namespace URI. The replacing
*       attribute has the same namespace URI and local name, as well as
*       the original prefix.
*       External function.
*
*   Parameters:
*       namespaceURI: the namespace URI of the attribute to remove.
*       localName: the local name of the atribute to remove.
*
*   Return Value:
*       IXML_SUCCESS or failure.
*
*=================================================================*/
int
ixmlElement_removeAttributeNS( IN IXML_Element * element,
                               IN DOMString namespaceURI,
                               IN DOMString localName )
{
    IXML_Node *attrNode;

    if( ( element == NULL ) || ( namespaceURI == NULL )
        || ( localName == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, localName ) == 0 &&
            strcmp( attrNode->namespaceURI, namespaceURI ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode != NULL ) {    // has the attribute
        if( attrNode->nodeValue != NULL ) {
            free( attrNode->nodeValue );
            attrNode->nodeValue = NULL;
        }
    }

    return IXML_SUCCESS;

}

/*================================================================
*   ixmlElement_getAttributeNodeNS
*       Retrieves an attr node by local name and namespace URI. 
*       External function.
*
*   Parameter:
*       namespaceURI: the namespace of the attribute to retrieve.
*       localName: the local name of the attribute to retrieve.
*
*   Return Value:
*       The attr node with the specified attribute local name and 
*       namespace URI or null if there is no such attribute.
*
*=================================================================*/
IXML_Attr *
ixmlElement_getAttributeNodeNS( IN IXML_Element * element,
                                IN DOMString namespaceURI,
                                IN DOMString localName )
{

    IXML_Node *attrNode;

    if( ( element == NULL ) || ( namespaceURI == NULL )
        || ( localName == NULL ) ) {
        return NULL;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, localName ) == 0 && strcmp( attrNode->namespaceURI, namespaceURI ) == 0 ) {    // found it
            break;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return ( IXML_Attr * ) attrNode;

}

/*================================================================
*   ixmlElement_setAttributeNodeNS
*       Adds a new attribute. If an attribute with that local name and
*       that namespace URI is already present in the element, it is replaced
*       by the new one.
*       External function.
*
*   Parameter:
*       newAttr: the attr node to add to the attribute list.
*
*   Return Value:
*       If the newAttr attribute replaces an existing attribute with the
*       same local name and namespace, the replaced attr node is returned,
*       otherwise null is returned.
*
*=================================================================*/
int
ixmlElement_setAttributeNodeNS( IN IXML_Element * element,
                                IN IXML_Attr * newAttr,
                                OUT IXML_Attr ** rtAttr )
{
    IXML_Node *attrNode;
    IXML_Node *node;
    IXML_Node *prevAttr = NULL,
     *nextAttr = NULL;
    IXML_Node *preSib,
     *nextSib;

    if( ( element == NULL ) || ( newAttr == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    if( newAttr->n.ownerDocument != element->n.ownerDocument ) {
        return IXML_WRONG_DOCUMENT_ERR;
    }

    if( ( newAttr->ownerElement != NULL )
        && ( newAttr->ownerElement != element ) ) {
        return IXML_INUSE_ATTRIBUTE_ERR;
    }

    newAttr->ownerElement = element;
    node = ( IXML_Node * ) newAttr;

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, node->localName ) == 0 &&
            strcmp( attrNode->namespaceURI, node->namespaceURI ) == 0 ) {
            break;              //found it
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    if( attrNode != NULL )      // already present, will replace by newAttr
    {
        preSib = attrNode->prevSibling;
        nextSib = attrNode->nextSibling;

        if( preSib != NULL ) {
            preSib->nextSibling = node;
        }

        if( nextSib != NULL ) {
            nextSib->prevSibling = node;
        }

        if( element->n.firstAttr == attrNode ) {
            element->n.firstAttr = node;
        }

        *rtAttr = ( IXML_Attr * ) attrNode;

    } else                      // add this attribute 
    {
        if( element->n.firstAttr != NULL )  // element has attribute already
        {
            prevAttr = element->n.firstAttr;
            nextAttr = prevAttr->nextSibling;
            while( nextAttr != NULL ) {
                prevAttr = nextAttr;
                nextAttr = prevAttr->nextSibling;
            }
            prevAttr->nextSibling = node;
        } else                  // this is the first attribute node
        {
            element->n.firstAttr = node;
            node->prevSibling = NULL;
            node->nextSibling = NULL;
        }

        if( rtAttr != NULL ) {
            *rtAttr = NULL;
        }
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlElement_getElementsByTagNameNS
*       Returns a nodeList of all the descendant Elements with a given
*       local name and namespace in the order in which they are encountered
*       in a preorder traversal of the element tree.
*       External function.
*
*   Parameters:
*       namespaceURI: the namespace URI of the elements to match on. The
*               special value "*" matches all namespaces.
*       localName: the local name of the elements to match on. The special
*               value "*" matches all local names.
*
*   Return Value:
*       A new nodeList object containing all the matched Elements.
*
*=================================================================*/
IXML_NodeList *
ixmlElement_getElementsByTagNameNS( IN IXML_Element * element,
                                    IN DOMString namespaceURI,
                                    IN DOMString localName )
{
    IXML_Node *node = ( IXML_Node * ) element;
    IXML_NodeList *nodeList = NULL;

    if( ( element != NULL ) && ( namespaceURI != NULL )
        && ( localName != NULL ) ) {
        ixmlNode_getElementsByTagNameNS( node, namespaceURI, localName,
                                         &nodeList );
    }

    return nodeList;
}

/*================================================================
*   ixmlElement_hasAttribute
*       Returns true when an attribute with a given name is specified on
*       this element, false otherwise.
*       External function.
*
*   Parameters:
*       name: the name of the attribute to look for.
*
*   Return Value:
*       ture if an attribute with the given name is specified on this
*       element, false otherwise.
*
*=================================================================*/
BOOL
ixmlElement_hasAttribute( IN IXML_Element * element,
                          IN DOMString name )
{

    IXML_Node *attrNode;

    if( ( element == NULL ) || ( name == NULL ) ) {
        return FALSE;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->nodeName, name ) == 0 ) {
            return TRUE;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return FALSE;
}

/*================================================================
*   ixmlElement_hasAttributeNS
*       Returns true when attribute with a given local name and namespace
*       URI is specified on this element, false otherwise.
*       External function.
*
*   Parameters:
*       namespaceURI: the namespace URI of the attribute to look for.
*       localName: the local name of the attribute to look for.
*
*   Return Value:
*       true if an attribute with the given local name and namespace URI
*       is specified, false otherwise.
*
*=================================================================*/
BOOL
ixmlElement_hasAttributeNS( IN IXML_Element * element,
                            IN DOMString namespaceURI,
                            IN DOMString localName )
{

    IXML_Node *attrNode;

    if( ( element == NULL ) || ( namespaceURI == NULL )
        || ( localName == NULL ) ) {
        return FALSE;
    }

    attrNode = element->n.firstAttr;
    while( attrNode != NULL ) {
        if( strcmp( attrNode->localName, localName ) == 0 &&
            strcmp( attrNode->namespaceURI, namespaceURI ) == 0 ) {
            return TRUE;
        } else {
            attrNode = attrNode->nextSibling;
        }
    }

    return FALSE;
}

/*================================================================
*   ixmlElement_free
*       frees a element node.
*       External function.
*
*=================================================================*/
void
ixmlElement_free( IN IXML_Element * element )
{
    if( element != NULL ) {
        ixmlNode_free( ( IXML_Node * ) element );
    }
}
