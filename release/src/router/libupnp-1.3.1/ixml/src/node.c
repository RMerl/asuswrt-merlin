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
*   ixmlNode_init
*       Intializes a node.
*       External function.
*
*=================================================================*/
void
ixmlNode_init( IN IXML_Node * nodeptr )
{
    assert( nodeptr != NULL );
    memset( nodeptr, 0, sizeof( IXML_Node ) );

}

/*================================================================
*   ixmlCDATASection_init
*       Initializes a CDATASection node.
*       External function.
*
*=================================================================*/
void
ixmlCDATASection_init( IN IXML_CDATASection * nodeptr )
{
    memset( nodeptr, 0, sizeof( IXML_CDATASection ) );
}

/*================================================================
*   ixmlCDATASection_free
*       frees a CDATASection node.
*       External function.
*
*=================================================================*/
void
ixmlCDATASection_free( IN IXML_CDATASection * nodeptr )
{
    if( nodeptr != NULL ) {
        ixmlNode_free( ( IXML_Node * ) nodeptr );
    }
}

/*================================================================
*   ixmlNode_freeSingleNode
*       frees a node content.
*       Internal to parser only.
*
*=================================================================*/
void
ixmlNode_freeSingleNode( IN IXML_Node * nodeptr )
{
    IXML_Element *element = NULL;

    if( nodeptr != NULL ) {
        if( nodeptr->nodeName != NULL ) {
            free( nodeptr->nodeName );
        }

        if( nodeptr->nodeValue != NULL ) {
            free( nodeptr->nodeValue );
        }

        if( nodeptr->namespaceURI != NULL ) {
            free( nodeptr->namespaceURI );
        }

        if( nodeptr->prefix != NULL ) {
            free( nodeptr->prefix );
        }

        if( nodeptr->localName != NULL ) {
            free( nodeptr->localName );
        }

        if( nodeptr->nodeType == eELEMENT_NODE ) {
            element = ( IXML_Element * ) nodeptr;
            free( element->tagName );
        }

        free( nodeptr );

    }
}

/*================================================================
*   ixmlNode_free
*       Frees all nodes under nodeptr subtree.
*       External function.
*
*=================================================================*/
void
ixmlNode_free( IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        ixmlNode_free( nodeptr->firstChild );
        ixmlNode_free( nodeptr->nextSibling );
        ixmlNode_free( nodeptr->firstAttr );

        ixmlNode_freeSingleNode( nodeptr );
    }
}

/*================================================================
*   ixmlNode_getNodeName
*       Returns the nodename(the qualified name)
*       External function.
*
*=================================================================*/
const DOMString
ixmlNode_getNodeName( IN IXML_Node * nodeptr )
{

    if( nodeptr != NULL ) {
        return ( nodeptr->nodeName );
    }

    return NULL;
}

/*================================================================
*   ixmlNode_getLocalName
*       Returns the node local name
*       External function.          		
*
*=================================================================*/
const DOMString
ixmlNode_getLocalName( IN IXML_Node * nodeptr )
{

    if( nodeptr != NULL ) {
        return ( nodeptr->localName );
    }

    return NULL;
}

/*================================================================
*   ixmlNode_setNamespaceURI
*       sets the namespace URI of the node.
*       Internal function.
*	Return:
*       IXML_SUCCESS or failure	
*
*=================================================================*/
int
ixmlNode_setNamespaceURI( IN IXML_Node * nodeptr,
                          IN char *namespaceURI )
{

    if( nodeptr == NULL ) {
        return IXML_INVALID_PARAMETER;
    }

    if( nodeptr->namespaceURI != NULL ) {
        free( nodeptr->namespaceURI );
        nodeptr->namespaceURI = NULL;
    }

    if( namespaceURI != NULL ) {
        nodeptr->namespaceURI = strdup( namespaceURI );
        if( nodeptr->namespaceURI == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlNode_setPrefix
*       set the prefix of the node.
*       Internal to parser only.
*	Returns:	
*       IXML_SUCCESS or failure.
*
*=================================================================*/
int
ixmlNode_setPrefix( IN IXML_Node * nodeptr,
                    IN char *prefix )
{

    if( nodeptr == NULL ) {
        return IXML_INVALID_PARAMETER;
    }

    if( nodeptr->prefix != NULL ) {
        free( nodeptr->prefix );
        nodeptr->prefix = NULL;
    }

    if( prefix != NULL ) {
        nodeptr->prefix = strdup( prefix );
        if( nodeptr->prefix == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
    }

    return IXML_SUCCESS;

}

/*================================================================
*   ixmlNode_setLocalName
*	    set the localName of the node.
*       Internal to parser only.
*	Returns:	
*       IXML_SUCCESS or failure.
*
*=================================================================*/
int
ixmlNode_setLocalName( IN IXML_Node * nodeptr,
                       IN char *localName )
{

    assert( nodeptr != NULL );

    if( nodeptr->localName != NULL ) {
        free( nodeptr->localName );
        nodeptr->localName = NULL;
    }

    if( localName != NULL ) {
        nodeptr->localName = strdup( localName );
        if( nodeptr->localName == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlNode_getNodeNamespaceURI
*       Returns the node namespaceURI
*       External function.
*   Returns:		
*       the namespaceURI of the node
*
*=================================================================*/
const DOMString
ixmlNode_getNamespaceURI( IN IXML_Node * nodeptr )
{
    DOMString retNamespaceURI = NULL;

    if( nodeptr != NULL ) {
        retNamespaceURI = nodeptr->namespaceURI;
    }

    return retNamespaceURI;
}

/*================================================================
*   ixmlNode_getPrefix
*       Returns the node prefix
*       External function.
*   Returns:		
*       the prefix of the node.
*
*=================================================================*/
DOMString
ixmlNode_getPrefix( IN IXML_Node * nodeptr )
{
    DOMString prefix = NULL;

    if( nodeptr != NULL ) {
        prefix = nodeptr->prefix;
    }

    return prefix;

}

/*================================================================
*   ixmlNode_getNodeValue
*       Returns the nodeValue of this node
*       External function.
*   Return:
*       the nodeValue of the node.
*
*=================================================================*/
DOMString
ixmlNode_getNodeValue( IN IXML_Node * nodeptr )
{

    if( nodeptr != NULL ) {
        return ( nodeptr->nodeValue );
    }

    return NULL;
}

/*================================================================
*   ixmlNode_setNodeValue
*       Sets the nodeValue
*       Internal function.
*   Returns:    
*       IXML_SUCCESS or failure
*
*=================================================================*/
int
ixmlNode_setNodeValue( IN IXML_Node * nodeptr,
                       IN char *newNodeValue )
{
    int rc = IXML_SUCCESS;

    if( nodeptr == NULL ) {
        return IXML_INVALID_PARAMETER;
    }

    if( nodeptr->nodeValue != NULL ) {
        free( nodeptr->nodeValue );
        nodeptr->nodeValue = NULL;
    }

    if( newNodeValue != NULL ) {
        nodeptr->nodeValue = strdup( newNodeValue );
        if( nodeptr->nodeValue == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
    }

    return rc;
}

/*================================================================
*   ixmlNode_getNodeType
*       Gets the NodeType of this node
*       External function.
*
*=================================================================*/
unsigned short
ixmlNode_getNodeType( IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        return ( nodeptr->nodeType );
    } else {
        return ( eINVALID_NODE );
    }
}

/*================================================================
*   ixmlNode_getParentNode
*       Get the parent node
*       External function.
*   Return:    
*       
*=================================================================*/
IXML_Node *
ixmlNode_getParentNode( IN IXML_Node * nodeptr )
{

    if( nodeptr != NULL ) {
        return nodeptr->parentNode;
    } else {
        return NULL;
    }
}

/*================================================================
*   ixmlNode_getFirstChild
*       Returns the first child of nodeptr.
*       External function.
*
*=================================================================*/
IXML_Node *
ixmlNode_getFirstChild( IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        return nodeptr->firstChild;
    } else {
        return NULL;
    }
}

/*================================================================
*   ixmlNode_getLastChild
*       Returns the last child of nodeptr.
*       External function.
*
*=================================================================*/
IXML_Node *
ixmlNode_getLastChild( IN IXML_Node * nodeptr )
{
    IXML_Node *prev,
     *next;

    if( nodeptr != NULL ) {
        prev = nodeptr;
        next = nodeptr->firstChild;

        while( next != NULL ) {
            prev = next;
            next = next->nextSibling;
        }
        return prev;
    } else {
        return NULL;
    }

}

/*================================================================
*   ixmlNode_getPreviousSibling
*       returns the previous sibling node of nodeptr.
*       External function.
*
*=================================================================*/
IXML_Node *
ixmlNode_getPreviousSibling( IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        return nodeptr->prevSibling;
    } else {
        return NULL;
    }
}

/*================================================================
*   ixmlNode_getNextSibling
*       Returns the next sibling node.
*       External function.
*
*=================================================================*/
IXML_Node *
ixmlNode_getNextSibling( IN IXML_Node * nodeptr )
{

    if( nodeptr != NULL ) {
        return nodeptr->nextSibling;
    } else {
        return NULL;
    }

}

/*================================================================
*   ixmlNode_getOwnerDocument
*       Returns the owner document node.
*       External function.
*
*=================================================================*/
IXML_Document *
ixmlNode_getOwnerDocument( IN IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        return ( IXML_Document * ) nodeptr->ownerDocument;
    } else {
        return NULL;
    }
}

/*================================================================
*   ixmlNode_isAncestor
*       check if ancestorNode is ancestor of toFind 
*       Internal to parser only.
*   Returns:
*       TRUE or FALSE
*
*=================================================================*/
BOOL
ixmlNode_isAncestor( IXML_Node * ancestorNode,
                     IXML_Node * toFind )
{

    BOOL found = FALSE;

    if( ( ancestorNode != NULL ) && ( toFind != NULL ) ) {
        if( toFind->parentNode == ancestorNode ) {
            return TRUE;
        } else {
            found =
                ixmlNode_isAncestor( ancestorNode->firstChild, toFind );
            if( found == FALSE ) {
                found =
                    ixmlNode_isAncestor( ancestorNode->nextSibling,
                                         toFind );
            }
        }
    }

    return found;
}

/*================================================================
*   ixmlNode_isParent
*       Check whether toFind is a children of nodeptr.
*       Internal to parser only.
*   Return:
*       TRUE or FALSE       
*
*=================================================================*/
BOOL
ixmlNode_isParent( IXML_Node * nodeptr,
                   IXML_Node * toFind )
{
    BOOL found = FALSE;

    assert( nodeptr != NULL && toFind != NULL );
    if( toFind->parentNode == nodeptr ) {
        found = TRUE;
    }

    return found;
}

/*================================================================
*   ixmlNode_allowChildren
*       Check to see whether nodeptr allows children of type newChild.    
*       Internal to parser only.
*   Returns:      
*       TRUE, if nodeptr can have newChild as children
*       FALSE,if nodeptr cannot have newChild as children
*
*=================================================================*/
BOOL
ixmlNode_allowChildren( IXML_Node * nodeptr,
                        IXML_Node * newChild )
{

    assert( nodeptr != NULL && newChild != NULL );
    switch ( nodeptr->nodeType ) {
        case eATTRIBUTE_NODE:
        case eTEXT_NODE:
        case eCDATA_SECTION_NODE:
            return FALSE;
            break;

        case eELEMENT_NODE:
            if( ( newChild->nodeType == eATTRIBUTE_NODE ) ||
                ( newChild->nodeType == eDOCUMENT_NODE ) ) {
                return FALSE;
            }
            break;

        case eDOCUMENT_NODE:
            if( newChild->nodeType != eELEMENT_NODE ) {
                return FALSE;
            }

        default:
            break;
    }

    return TRUE;
}

/*================================================================
*   ixmlNode_compare
*       Compare two nodes to see whether they are the same node.
*       Parent, sibling and children node are ignored.
*       Internal to parser only.
*   Returns:
*       TRUE, the two nodes are the same.
*       FALSE, the two nodes are not the same.
*
*=================================================================*/
BOOL
ixmlNode_compare( IXML_Node * srcNode,
                  IXML_Node * destNode )
{
    assert( srcNode != NULL && destNode != NULL );
    if( ( srcNode == destNode ) ||
        ( strcmp( srcNode->nodeName, destNode->nodeName ) == 0 &&
          strcmp( srcNode->nodeValue, destNode->nodeValue ) == 0 &&
          ( srcNode->nodeType == destNode->nodeType ) &&
          strcmp( srcNode->namespaceURI, destNode->namespaceURI ) == 0 &&
          strcmp( srcNode->prefix, destNode->prefix ) == 0 &&
          strcmp( srcNode->localName, destNode->localName ) == 0 ) ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*================================================================
*   ixmlNode_insertBefore
*       Inserts the node newChild before the existing child node refChild.
*       If refChild is null, insert newChild at the end of the list of
*       children. If the newChild is already in the tree, it is first
*       removed.   
*       External function.
*   Parameters:
*       newChild: the node to insert.
*   Returns:
*
*=================================================================*/
int
ixmlNode_insertBefore( IN IXML_Node * nodeptr,
                       IN IXML_Node * newChild,
                       IN IXML_Node * refChild )
{

    int ret = IXML_SUCCESS;

    if( ( nodeptr == NULL ) || ( newChild == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }
    // whether nodeptr allow children of the type of newChild 
    if( ixmlNode_allowChildren( nodeptr, newChild ) == FALSE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }
    // or if newChild is one of nodeptr's ancestors
    if( ixmlNode_isAncestor( newChild, nodeptr ) == TRUE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }
    // if newChild was created from a different document 
    if( nodeptr->ownerDocument != newChild->ownerDocument ) {
        return IXML_WRONG_DOCUMENT_ERR;
    }
    // if refChild is not a child of nodeptr
    if( ixmlNode_isParent( nodeptr, refChild ) == FALSE ) {
        return IXML_NOT_FOUND_ERR;
    }

    if( refChild != NULL ) {
        if( ixmlNode_isParent( nodeptr, newChild ) == TRUE ) {
            ixmlNode_removeChild( nodeptr, newChild, NULL );
            newChild->nextSibling = NULL;
            newChild->prevSibling = NULL;
        }

        newChild->nextSibling = refChild;
        if( refChild->prevSibling != NULL ) {
            ( refChild->prevSibling )->nextSibling = newChild;
            newChild->prevSibling = refChild->prevSibling;
        }

        refChild->prevSibling = newChild;

        if( newChild->prevSibling == NULL ) {
            nodeptr->firstChild = newChild;
        }

        newChild->parentNode = nodeptr;

    } else {
        ret = ixmlNode_appendChild( nodeptr, newChild );
    }

    return ret;
}

/*================================================================
*   ixmlNode_replaceChild
*       Replaces the child node oldChild with newChild in the list of children,
*       and returns the oldChild node.
*       External function.
*   Parameters:
*       newChild:   the new node to put in the child list.
*       oldChild:   the node being replaced in the list.
*       returnNode: the node replaced.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if anyone of nodeptr, newChild or oldChild is NULL.
*       IXML_HIERARCHY_REQUEST_ERR: if the newChild is ancestor of nodeptr or nodeptr
*                                   is of a type that does not allow children of the
*                                   type of the newChild node.
*       IXML_WRONG_DOCUMENT_ERR:    if newChild was created from a different document than
*                                   the one that created this node.
*       IXML_NOT_FOUND_ERR:         if oldChild is not a child of nodeptr.
*
*=================================================================*/
int
ixmlNode_replaceChild( IN IXML_Node * nodeptr,
                       IN IXML_Node * newChild,
                       IN IXML_Node * oldChild,
                       OUT IXML_Node ** returnNode )
{
    int ret = IXML_SUCCESS;

    if( ( nodeptr == NULL ) || ( newChild == NULL )
        || ( oldChild == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }
    // if nodetype of nodeptr does not allow children of the type of newChild 
    // needs to add later

    // or if newChild is one of nodeptr's ancestors
    if( ixmlNode_isAncestor( newChild, nodeptr ) == TRUE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }

    if( ixmlNode_allowChildren( nodeptr, newChild ) == FALSE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }
    // if newChild was created from a different document 
    if( nodeptr->ownerDocument != newChild->ownerDocument ) {
        return IXML_WRONG_DOCUMENT_ERR;
    }
    // if refChild is not a child of nodeptr
    if( ixmlNode_isParent( nodeptr, oldChild ) != TRUE ) {
        return IXML_NOT_FOUND_ERR;
    }

    ret = ixmlNode_insertBefore( nodeptr, newChild, oldChild );
    if( ret != IXML_SUCCESS ) {
        return ret;
    }

    ret = ixmlNode_removeChild( nodeptr, oldChild, returnNode );
    return ret;
}

/*================================================================
*   ixmlNode_removeChild
*       Removes the child node indicated by oldChild from the list of
*       children, and returns it.
*       External function.
*   Parameters:
*       oldChild: the node being removed.
*       returnNode: the node removed.
*   Return Value:
*       IXML_SUCCESS
*       IXML_NOT_FOUND_ERR: if oldChild is not a child of this node.
*       IXML_INVALID_PARAMETER: if either oldChild or nodeptr is NULL
*
*=================================================================*/
int
ixmlNode_removeChild( IN IXML_Node * nodeptr,
                      IN IXML_Node * oldChild,
                      OUT IXML_Node ** returnNode )
{

    if( ( nodeptr == NULL ) || ( oldChild == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }

    if( ixmlNode_isParent( nodeptr, oldChild ) == FALSE ) {
        return IXML_NOT_FOUND_ERR;
    }

    if( oldChild->prevSibling != NULL ) {
        ( oldChild->prevSibling )->nextSibling = oldChild->nextSibling;
    }

    if( nodeptr->firstChild == oldChild ) {
        nodeptr->firstChild = oldChild->nextSibling;
    }

    if( oldChild->nextSibling != NULL ) {
        ( oldChild->nextSibling )->prevSibling = oldChild->prevSibling;
    }

    oldChild->nextSibling = NULL;
    oldChild->prevSibling = NULL;
    oldChild->parentNode = NULL;

    if( returnNode != NULL ) {
        *returnNode = oldChild;
    }
    return IXML_SUCCESS;
}

/*=============================================================================
*   ixmlNode_appendChild
*       Adds the node newChild to the end of the list of children of this node.
*       If the newChild is already in the tree, it is first removed.
*       External function.   
*   Parameter:
*       newChild: the node to add.
*   Return Value:
*       IXML_SUCCESS
*       IXML_INVALID_PARAMETER:     if either nodeptr or newChild is NULL
*       IXML_WRONG_DOCUMENT_ERR:    if newChild was created from a different document than
*                                   the one that created nodeptr.
*       IXML_HIERARCHY_REQUEST_ERR: if newChild is ancestor of nodeptr or if nodeptr is of
*                                   a type that does not allow children of the type of the
*                                   newChild node.
*
*=================================================================*/
int
ixmlNode_appendChild( IN IXML_Node * nodeptr,
                      IN IXML_Node * newChild )
{

    IXML_Node *prev = NULL,
     *next = NULL;

    if( ( nodeptr == NULL ) || ( newChild == NULL ) ) {
        return IXML_INVALID_PARAMETER;
    }
    // if newChild was created from a different document 
    if( ( newChild->ownerDocument != NULL ) &&
        ( nodeptr->ownerDocument != newChild->ownerDocument ) ) {
        return IXML_WRONG_DOCUMENT_ERR;
    }
    // if newChild is an ancestor of nodeptr
    if( ixmlNode_isAncestor( newChild, nodeptr ) == TRUE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }
    // if nodeptr does not allow to have newChild as children
    if( ixmlNode_allowChildren( nodeptr, newChild ) == FALSE ) {
        return IXML_HIERARCHY_REQUEST_ERR;
    }

    if( ixmlNode_isParent( nodeptr, newChild ) == TRUE ) {
        ixmlNode_removeChild( nodeptr, newChild, NULL );
    }
    // set the parent node pointer
    newChild->parentNode = nodeptr;
    newChild->ownerDocument = nodeptr->ownerDocument;

    //if the first child
    if( nodeptr->firstChild == NULL ) {
        nodeptr->firstChild = newChild;
    } else {
        prev = nodeptr->firstChild;
        next = prev->nextSibling;
        while( next != NULL ) {
            prev = next;
            next = prev->nextSibling;
        }
        prev->nextSibling = newChild;
        newChild->prevSibling = prev;
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlNode_cloneTextNode
*       Returns a clone of nodeptr
*       Internal to parser only.
*
*=================================================================*/
IXML_Node *
ixmlNode_cloneTextNode( IN IXML_Node * nodeptr )
{
    IXML_Node *newNode = NULL;

    assert( nodeptr != NULL );

    newNode = ( IXML_Node * ) malloc( sizeof( IXML_Node ) );
    if( newNode == NULL ) {
        return NULL;
    } else {
        ixmlNode_init( newNode );

        ixmlNode_setNodeName( newNode, nodeptr->nodeName );
        ixmlNode_setNodeValue( newNode, nodeptr->nodeValue );
        newNode->nodeType = eTEXT_NODE;
    }

    return newNode;
}

/*================================================================
*   ixmlNode_cloneCDATASect
*       Return a clone of CDATASection node.
*       Internal to parser only.
*
*=================================================================*/
IXML_CDATASection *
ixmlNode_cloneCDATASect( IN IXML_CDATASection * nodeptr )
{
    IXML_CDATASection *newCDATA = NULL;
    IXML_Node *newNode;
    IXML_Node *srcNode;

    assert( nodeptr != NULL );
    newCDATA =
        ( IXML_CDATASection * ) malloc( sizeof( IXML_CDATASection ) );
    if( newCDATA != NULL ) {
        newNode = ( IXML_Node * ) newCDATA;
        ixmlNode_init( newNode );

        srcNode = ( IXML_Node * ) nodeptr;
        ixmlNode_setNodeName( newNode, srcNode->nodeName );
        ixmlNode_setNodeValue( newNode, srcNode->nodeValue );
        newNode->nodeType = eCDATA_SECTION_NODE;
    }

    return newCDATA;
}

/*================================================================
*   ixmlNode_cloneElement
*       returns a clone of element node
*       Internal to parser only.
*
*=================================================================*/
IXML_Element *
ixmlNode_cloneElement( IN IXML_Element * nodeptr )
{
    IXML_Element *newElement;
    IXML_Node *elementNode;
    IXML_Node *srcNode;
    int rc;

    assert( nodeptr != NULL );

    newElement = ( IXML_Element * ) malloc( sizeof( IXML_Element ) );
    if( newElement == NULL ) {
        return NULL;
    }

    ixmlElement_init( newElement );
    rc = ixmlElement_setTagName( newElement, nodeptr->tagName );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
	return NULL;
    }

    elementNode = ( IXML_Node * ) newElement;
    srcNode = ( IXML_Node * ) nodeptr;
    rc = ixmlNode_setNodeName( elementNode, srcNode->nodeName );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        return NULL;
    }

    rc = ixmlNode_setNodeValue( elementNode, srcNode->nodeValue );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        return NULL;
    }

    rc = ixmlNode_setNamespaceURI( elementNode, srcNode->namespaceURI );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        return NULL;
    }

    rc = ixmlNode_setPrefix( elementNode, srcNode->prefix );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        return NULL;
    }

    rc = ixmlNode_setLocalName( elementNode, srcNode->localName );
    if( rc != IXML_SUCCESS ) {
        ixmlElement_free( newElement );
        return NULL;
    }

    elementNode->nodeType = eELEMENT_NODE;

    return newElement;

}

/*================================================================
*   ixmlNode_cloneDoc
*       Returns a clone of document node
*       Internal to parser only.
*
*=================================================================*/
IXML_Document *
ixmlNode_cloneDoc( IN IXML_Document * nodeptr )
{
    IXML_Document *newDoc;
    IXML_Node *docNode;
    int rc;

    assert( nodeptr != NULL );
    newDoc = ( IXML_Document * ) malloc( sizeof( IXML_Document ) );
    if( newDoc == NULL ) {
        return NULL;
    }

    ixmlDocument_init( newDoc );
    docNode = ( IXML_Node * ) newDoc;

    rc = ixmlNode_setNodeName( docNode, DOCUMENTNODENAME );
    if( rc != IXML_SUCCESS ) {
        ixmlDocument_free( newDoc );
        return NULL;
    }

    newDoc->n.nodeType = eDOCUMENT_NODE;

    return newDoc;

}

/*================================================================
*   ixmlNode_cloneAttr
*       Returns a clone of attribute node
*       Internal to parser only
*
*=================================================================*/
IXML_Attr *
ixmlNode_cloneAttr( IN IXML_Attr * nodeptr )
{
    IXML_Attr *newAttr;
    IXML_Node *attrNode;
    IXML_Node *srcNode;
    int rc;

    assert( nodeptr != NULL );
    newAttr = ( IXML_Attr * ) malloc( sizeof( IXML_Attr ) );
    if( newAttr == NULL ) {
        return NULL;
    }

    ixmlAttr_init( newAttr );
    attrNode = ( IXML_Node * ) newAttr;
    srcNode = ( IXML_Node * ) nodeptr;

    rc = ixmlNode_setNodeName( attrNode, srcNode->nodeName );
    if( rc != IXML_SUCCESS ) {
        ixmlAttr_free( newAttr );
        return NULL;
    }

    rc = ixmlNode_setNodeValue( attrNode, srcNode->nodeValue );
    if( rc != IXML_SUCCESS ) {
        ixmlAttr_free( newAttr );
        return NULL;
    }
    //check to see whether we need to split prefix and localname for attribute
    rc = ixmlNode_setNamespaceURI( attrNode, srcNode->namespaceURI );
    if( rc != IXML_SUCCESS ) {
        ixmlAttr_free( newAttr );
        return NULL;
    }

    rc = ixmlNode_setPrefix( attrNode, srcNode->prefix );
    if( rc != IXML_SUCCESS ) {
        ixmlAttr_free( newAttr );
        return NULL;
    }

    rc = ixmlNode_setLocalName( attrNode, srcNode->localName );
    if( rc != IXML_SUCCESS ) {
        ixmlAttr_free( newAttr );
        return NULL;
    }

    attrNode->nodeType = eATTRIBUTE_NODE;

    return newAttr;
}

/*================================================================
*   ixmlNode_cloneAttrDirect
*       Return a clone of attribute node, with specified field set
*       to TRUE.
*
*=================================================================*/
IXML_Attr *
ixmlNode_cloneAttrDirect( IN IXML_Attr * nodeptr )
{

    IXML_Attr *newAttr;

    assert( nodeptr != NULL );

    newAttr = ixmlNode_cloneAttr( nodeptr );
    if( newAttr != NULL ) {
        newAttr->specified = TRUE;
    }

    return newAttr;
}

void
ixmlNode_setSiblingNodesParent( IXML_Node * nodeptr )
{
    IXML_Node *parentNode = nodeptr->parentNode;
    IXML_Node *nextptr = nodeptr->nextSibling;

    while( nextptr != NULL ) {
        nextptr->parentNode = parentNode;
        nextptr = nextptr->nextSibling;
    }
}

/*================================================================
*   ixmlNode_cloneNodeTreeRecursive
*       recursive functions that clones node tree of nodeptr.
*       Internal to parser only.
*       
*=================================================================*/
IXML_Node *
ixmlNode_cloneNodeTreeRecursive( IN IXML_Node * nodeptr,
                                 IN BOOL deep )
{
    IXML_Node *newNode = NULL;
    IXML_Element *newElement;
    IXML_Attr *newAttr = NULL;
    IXML_CDATASection *newCDATA = NULL;
    IXML_Document *newDoc;
    IXML_Node *nextSib;

    if( nodeptr != NULL ) {
        switch ( nodeptr->nodeType ) {
            case eELEMENT_NODE:
                newElement =
                    ixmlNode_cloneElement( ( IXML_Element * ) nodeptr );
                newElement->n.firstAttr =
                    ixmlNode_cloneNodeTreeRecursive( nodeptr->firstAttr,
                                                     deep );
                if( deep ) {
                    newElement->n.firstChild =
                        ixmlNode_cloneNodeTreeRecursive( nodeptr->
                                                         firstChild,
                                                         deep );
                    if( newElement->n.firstChild != NULL ) {
                        ( newElement->n.firstChild )->parentNode =
                            ( IXML_Node * ) newElement;
                        ixmlNode_setSiblingNodesParent( newElement->n.
                                                        firstChild );
                    }
                    nextSib =
                        ixmlNode_cloneNodeTreeRecursive( nodeptr->
                                                         nextSibling,
                                                         deep );
                    newElement->n.nextSibling = nextSib;
                    if( nextSib != NULL ) {
                        nextSib->prevSibling = ( IXML_Node * ) newElement;
                    }
                }

                newNode = ( IXML_Node * ) newElement;
                break;

            case eATTRIBUTE_NODE:
                newAttr = ixmlNode_cloneAttr( ( IXML_Attr * ) nodeptr );
                nextSib =
                    ixmlNode_cloneNodeTreeRecursive( nodeptr->nextSibling,
                                                     deep );
                newAttr->n.nextSibling = nextSib;

                if( nextSib != NULL ) {
                    nextSib->prevSibling = ( IXML_Node * ) newAttr;
                }
                newNode = ( IXML_Node * ) newAttr;
                break;

            case eTEXT_NODE:
                newNode = ixmlNode_cloneTextNode( nodeptr );
                break;

            case eCDATA_SECTION_NODE:
                newCDATA =
                    ixmlNode_cloneCDATASect( ( IXML_CDATASection * )
                                             nodeptr );
                newNode = ( IXML_Node * ) newCDATA;
                break;

            case eDOCUMENT_NODE:
                newDoc = ixmlNode_cloneDoc( ( IXML_Document * ) nodeptr );
                newNode = ( IXML_Node * ) newDoc;
                if( deep ) {
                    newNode->firstChild =
                        ixmlNode_cloneNodeTreeRecursive( nodeptr->
                                                         firstChild,
                                                         deep );
                    if( newNode->firstChild != NULL ) {
                        newNode->firstChild->parentNode = newNode;
                    }
                }

                break;

            case eINVALID_NODE:
            case eENTITY_REFERENCE_NODE:
            case eENTITY_NODE:
            case ePROCESSING_INSTRUCTION_NODE:
            case eCOMMENT_NODE:
            case eDOCUMENT_TYPE_NODE:
            case eDOCUMENT_FRAGMENT_NODE:
            case eNOTATION_NODE:
                break;
        }
    }

    return newNode;
}

/*================================================================
*   ixmlNode_cloneNodeTree
*       clones a node tree.
*       Internal to parser only.
*
*=================================================================*/
IXML_Node *
ixmlNode_cloneNodeTree( IN IXML_Node * nodeptr,
                        IN BOOL deep )
{
    IXML_Node *newNode = NULL;
    IXML_Element *newElement;
    IXML_Node *childNode;

    assert( nodeptr != NULL );

    switch ( nodeptr->nodeType ) {
        case eELEMENT_NODE:
            newElement =
                ixmlNode_cloneElement( ( IXML_Element * ) nodeptr );
            newElement->n.firstAttr =
                ixmlNode_cloneNodeTreeRecursive( nodeptr->firstAttr,
                                                 deep );
            if( deep ) {
                newElement->n.firstChild =
                    ixmlNode_cloneNodeTreeRecursive( nodeptr->firstChild,
                                                     deep );
                childNode = newElement->n.firstChild;
                while( childNode != NULL ) {
                    childNode->parentNode = ( IXML_Node * ) newElement;
                    childNode = childNode->nextSibling;
                }
                newElement->n.nextSibling = NULL;
            }

            newNode = ( IXML_Node * ) newElement;
            break;

        case eATTRIBUTE_NODE:
        case eTEXT_NODE:
        case eCDATA_SECTION_NODE:
        case eDOCUMENT_NODE:
            newNode = ixmlNode_cloneNodeTreeRecursive( nodeptr, deep );
            break;

        case eINVALID_NODE:
        case eENTITY_REFERENCE_NODE:
        case eENTITY_NODE:
        case ePROCESSING_INSTRUCTION_NODE:
        case eCOMMENT_NODE:
        case eDOCUMENT_TYPE_NODE:
        case eDOCUMENT_FRAGMENT_NODE:
        case eNOTATION_NODE:
            break;
    }

    // by spec, the duplicate node has no parent
    newNode->parentNode = NULL;

    return newNode;
}

/*================================================================
*   ixmlNode_cloneNode
*       Clones a node, if deep==TRUE, clones subtree under nodeptr.
*       External function.
*   Returns:
*       the cloned node or NULL if error occurs.
*
*=================================================================*/
IXML_Node *
ixmlNode_cloneNode( IN IXML_Node * nodeptr,
                    IN BOOL deep )
{

    IXML_Node *newNode;
    IXML_Attr *newAttrNode;

    if( nodeptr == NULL ) {
        return NULL;
    }

    switch ( nodeptr->nodeType ) {
        case eATTRIBUTE_NODE:
            newAttrNode =
                ixmlNode_cloneAttrDirect( ( IXML_Attr * ) nodeptr );
            return ( IXML_Node * ) newAttrNode;
            break;

        default:
            newNode = ixmlNode_cloneNodeTree( nodeptr, deep );
            return newNode;
            break;
    }

}

/*================================================================
*   ixmlNode_getChildNodes
*       Returns a IXML_NodeList of all the child nodes of nodeptr.
*       External function.
*   
*=================================================================*/
IXML_NodeList *
ixmlNode_getChildNodes( IN IXML_Node * nodeptr )
{
    IXML_Node *tempNode;
    IXML_NodeList *newNodeList;
    int rc;

    if( nodeptr == NULL ) {
        return NULL;
    }

    newNodeList = ( IXML_NodeList * ) malloc( sizeof( IXML_NodeList ) );
    if( newNodeList == NULL ) {
        return NULL;
    }

    ixmlNodeList_init( newNodeList );

    tempNode = nodeptr->firstChild;
    while( tempNode != NULL ) {
        rc = ixmlNodeList_addToNodeList( &newNodeList, tempNode );
        if( rc != IXML_SUCCESS ) {
            ixmlNodeList_free( newNodeList );
            return NULL;
        }

        tempNode = tempNode->nextSibling;
    }
    return newNodeList;
}

/*================================================================
*   ixmlNode_getAttributes
*       returns a namedNodeMap of attributes of nodeptr
*       External function.
*   Returns:
*
*=================================================================*/
IXML_NamedNodeMap *
ixmlNode_getAttributes( IN IXML_Node * nodeptr )
{
    IXML_NamedNodeMap *returnNamedNodeMap = NULL;
    IXML_Node *tempNode;
    int rc;

    if( nodeptr == NULL ) {
        return NULL;
    }

    if( nodeptr->nodeType == eELEMENT_NODE ) {
        returnNamedNodeMap =
            ( IXML_NamedNodeMap * ) malloc( sizeof( IXML_NamedNodeMap ) );
        if( returnNamedNodeMap == NULL ) {
            return NULL;
        }

        ixmlNamedNodeMap_init( returnNamedNodeMap );

        tempNode = nodeptr->firstAttr;
        while( tempNode != NULL ) {
            rc = ixmlNamedNodeMap_addToNamedNodeMap( &returnNamedNodeMap,
                                                     tempNode );
            if( rc != IXML_SUCCESS ) {
                ixmlNamedNodeMap_free( returnNamedNodeMap );
                return NULL;
            }

            tempNode = tempNode->nextSibling;
        }
        return returnNamedNodeMap;
    } else {                    // if not an ELEMENT_NODE
        return NULL;
    }
}

/*================================================================
*   ixmlNode_hasChildNodes
*       External function.
*
*=================================================================*/
BOOL
ixmlNode_hasChildNodes( IXML_Node * nodeptr )
{
    if( nodeptr == NULL ) {
        return FALSE;
    }

    return ( nodeptr->firstChild != NULL );

}

/*================================================================
*   ixmlNode_hasAttributes
*       External function.
*
*=================================================================*/
BOOL
ixmlNode_hasAttributes( IXML_Node * nodeptr )
{
    if( nodeptr != NULL ) {
        if( ( nodeptr->nodeType == eELEMENT_NODE )
            && ( nodeptr->firstAttr != NULL ) ) {
            return TRUE;
        }
    }
    return FALSE;

}

/*================================================================
*   ixmlNode_getElementsByTagNameRecursive
*       Recursively traverse the whole tree, search for element
*       with the given tagname.
*       Internal to parser.
*
*=================================================================*/
void
ixmlNode_getElementsByTagNameRecursive( IN IXML_Node * n,
                                        IN char *tagname,
                                        OUT IXML_NodeList ** list )
{
    const char *name;

    if( n != NULL ) {
        if( ixmlNode_getNodeType( n ) == eELEMENT_NODE ) {
            name = ixmlNode_getNodeName( n );
            if( strcmp( tagname, name ) == 0
                || strcmp( tagname, "*" ) == 0 ) {
                ixmlNodeList_addToNodeList( list, n );
            }
        }

        ixmlNode_getElementsByTagNameRecursive( ixmlNode_getFirstChild
                                                ( n ), tagname, list );
        ixmlNode_getElementsByTagNameRecursive( ixmlNode_getNextSibling
                                                ( n ), tagname, list );
    }

}

/*================================================================
*   ixmlNode_getElementsByTagName
*       Returns a nodeList of all descendant Elements with a given 
*       tagName, in the order in which they are encountered in a
*       traversal of this element tree.
*       External function.		
*
*=================================================================*/
void
ixmlNode_getElementsByTagName( IN IXML_Node * n,
                               IN char *tagname,
                               OUT IXML_NodeList ** list )
{
    const char *name;

    assert( n != NULL && tagname != NULL );

    if( ixmlNode_getNodeType( n ) == eELEMENT_NODE ) {
        name = ixmlNode_getNodeName( n );
        if( strcmp( tagname, name ) == 0 || strcmp( tagname, "*" ) == 0 ) {
            ixmlNodeList_addToNodeList( list, n );
        }
    }

    ixmlNode_getElementsByTagNameRecursive( ixmlNode_getFirstChild( n ),
                                            tagname, list );

}

/*================================================================
*   ixmlNode_getElementsByTagNameNSRecursive
*	    Internal function to parser.	
*		
*
*=================================================================*/
void
ixmlNode_getElementsByTagNameNSRecursive( IN IXML_Node * n,
                                          IN char *namespaceURI,
                                          IN char *localName,
                                          OUT IXML_NodeList ** list )
{
    const DOMString nsURI;
    const DOMString name;

    if( n != NULL ) {
        if( ixmlNode_getNodeType( n ) == eELEMENT_NODE ) {
            name = ixmlNode_getLocalName( n );
            nsURI = ixmlNode_getNamespaceURI( n );

            if( ( name != NULL ) && ( nsURI != NULL ) &&
                ( strcmp( namespaceURI, nsURI ) == 0
                  || strcmp( namespaceURI, "*" ) == 0 )
                && ( strcmp( name, localName ) == 0
                     || strcmp( localName, "*" ) == 0 ) ) {
                ixmlNodeList_addToNodeList( list, n );
            }
        }

        ixmlNode_getElementsByTagNameNSRecursive( ixmlNode_getFirstChild
                                                  ( n ), namespaceURI,
                                                  localName, list );
        ixmlNode_getElementsByTagNameNSRecursive( ixmlNode_getNextSibling
                                                  ( n ), namespaceURI,
                                                  localName, list );
    }

}

/*================================================================
*   ixmlNode_getElementsByTagNameNS
*       Returns a nodeList of all the descendant Elements with a given
*       local name and namespace URI in the order in which they are
*       encountered in a preorder traversal of this Elememt tree.		
*		External function.
*
*=================================================================*/
void
ixmlNode_getElementsByTagNameNS( IN IXML_Node * n,
                                 IN char *namespaceURI,
                                 IN char *localName,
                                 OUT IXML_NodeList ** list )
{
    const DOMString nsURI;
    const DOMString name;

    assert( n != NULL && namespaceURI != NULL && localName != NULL );

    if( ixmlNode_getNodeType( n ) == eELEMENT_NODE ) {
        name = ixmlNode_getLocalName( n );
        nsURI = ixmlNode_getNamespaceURI( n );

        if( ( name != NULL ) && ( nsURI != NULL ) &&
            ( strcmp( namespaceURI, nsURI ) == 0
              || strcmp( namespaceURI, "*" ) == 0 )
            && ( strcmp( name, localName ) == 0
                 || strcmp( localName, "*" ) == 0 ) ) {
            ixmlNodeList_addToNodeList( list, n );
        }
    }

    ixmlNode_getElementsByTagNameNSRecursive( ixmlNode_getFirstChild( n ),
                                              namespaceURI, localName,
                                              list );

}

/*================================================================
*   ixmlNode_setNodeName
*       Internal to parser only.
*
*=================================================================*/
int
ixmlNode_setNodeName( IN IXML_Node * node,
                      IN DOMString qualifiedName )
{
    int rc = IXML_SUCCESS;

    assert( node != NULL );

    if( node->nodeName != NULL ) {
        free( node->nodeName );
        node->nodeName = NULL;
    }

    if( qualifiedName != NULL ) {
        // set the name part
        node->nodeName = strdup( qualifiedName );
        if( node->nodeName == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }

        rc = Parser_setNodePrefixAndLocalName( node );
        if( rc != IXML_SUCCESS ) {
            free( node->nodeName );
        }
    }

    return rc;
}

/*================================================================
*   ixmlNode_setNodeProperties
*       Internal to parser only.
*
*=================================================================*/
int
ixmlNode_setNodeProperties( IN IXML_Node * destNode,
                            IN IXML_Node * src )
{

    int rc;

    assert( destNode != NULL || src != NULL );

    rc = ixmlNode_setNodeValue( destNode, src->nodeValue );
    if( rc != IXML_SUCCESS ) {
        goto ErrorHandler;
    }

    rc = ixmlNode_setLocalName( destNode, src->localName );
    if( rc != IXML_SUCCESS ) {
        goto ErrorHandler;
    }

    rc = ixmlNode_setPrefix( destNode, src->prefix );
    if( rc != IXML_SUCCESS ) {
        goto ErrorHandler;
    }
    // set nodetype
    destNode->nodeType = src->nodeType;

    return IXML_SUCCESS;

  ErrorHandler:
    if( destNode->nodeName != NULL ) {
        free( destNode->nodeName );
        destNode->nodeName = NULL;
    }
    if( destNode->nodeValue != NULL ) {
        free( destNode->nodeValue );
        destNode->nodeValue = NULL;
    }
    if( destNode->localName != NULL ) {
        free( destNode->localName );
        destNode->localName = NULL;
    }

    return IXML_INSUFFICIENT_MEMORY;
}
