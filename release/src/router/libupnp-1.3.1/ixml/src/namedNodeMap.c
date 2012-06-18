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
*   NamedNodeMap_getItemNumber
*       return the item number of a item in NamedNodeMap.
*       Internal to parser only.
*   Parameters:
*       name: the name of the item to find
*   
*=================================================================*/
unsigned long
ixmlNamedNodeMap_getItemNumber( IN IXML_NamedNodeMap * nnMap,
                                IN char *name )
{
    IXML_Node *tempNode;
    unsigned long returnItemNo = 0;

    assert( nnMap != NULL && name != NULL );
    if( ( nnMap == NULL ) || ( name == NULL ) ) {
        return IXML_INVALID_ITEM_NUMBER;
    }

    tempNode = nnMap->nodeItem;
    while( tempNode != NULL ) {
        if( strcmp( name, tempNode->nodeName ) == 0 ) {
            return returnItemNo;
        }

        tempNode = tempNode->nextSibling;
        returnItemNo++;
    }

    return IXML_INVALID_ITEM_NUMBER;
}

/*================================================================
*   NamedNodeMap_init
*       Initializes a NamedNodeMap object.
*       External function.
*
*=================================================================*/
void
ixmlNamedNodeMap_init( IN IXML_NamedNodeMap * nnMap )
{
    assert( nnMap != NULL );
    memset( nnMap, 0, sizeof( IXML_NamedNodeMap ) );
}

/*================================================================
*   NamedNodeMap_getNamedItem
*       Retrieves a node specified by name.
*       External function.
*
*   Parameter:
*       name: type nodeName of a node to retrieve.
*
*   Return Value:
*       A Node with the specified nodeName, or null if it
*       does not identify any node in this map.
*
*=================================================================*/
IXML_Node *
ixmlNamedNodeMap_getNamedItem( IN IXML_NamedNodeMap * nnMap,
                               IN char *name )
{
    long index;

    if( ( nnMap == NULL ) || ( name == NULL ) ) {
        return NULL;
    }

    index = ixmlNamedNodeMap_getItemNumber( nnMap, name );
    if( index == IXML_INVALID_ITEM_NUMBER ) {
        return NULL;
    } else {
        return ( ixmlNamedNodeMap_item( nnMap, ( unsigned long )index ) );
    }
}

/*================================================================
*   NamedNodeMap_item
*       Returns the indexth item in the map. If index is greater than or
*       equal to the number of nodes in this map, this returns null.
*       External function.
*
*   Parameter:
*       index: index into this map.
*
*   Return Value:
*       The node at the indexth position in the map, or null if that is
*       not a valid index.
*
*=================================================================*/
IXML_Node *
ixmlNamedNodeMap_item( IN IXML_NamedNodeMap * nnMap,
                       IN unsigned long index )
{
    IXML_Node *tempNode;
    unsigned int i;

    if( nnMap == NULL ) {
        return NULL;
    }

    if( index > ixmlNamedNodeMap_getLength( nnMap ) - 1 ) {
        return NULL;
    }

    tempNode = nnMap->nodeItem;
    for( i = 0; i < index && tempNode != NULL; ++i ) {
        tempNode = tempNode->nextSibling;
    }

    return tempNode;
}

/*================================================================
*   NamedNodeMap_getLength	
*       Return the number of Nodes in this map.       
*       External function.
*   
*   Parameters:
*
*=================================================================*/
unsigned long
ixmlNamedNodeMap_getLength( IN IXML_NamedNodeMap * nnMap )
{
    IXML_Node *tempNode;
    unsigned long length = 0;

    if( nnMap != NULL ) {
        tempNode = nnMap->nodeItem;
        for( length = 0; tempNode != NULL; ++length ) {
            tempNode = tempNode->nextSibling;
        }
    }
    return length;
}

/*================================================================
*   ixmlNamedNodeMap_free
*       frees a NamedNodeMap.
*       External function.
*
*=================================================================*/
void
ixmlNamedNodeMap_free( IXML_NamedNodeMap * nnMap )
{
    IXML_NamedNodeMap *pNext;

    while( nnMap != NULL ) {
        pNext = nnMap->next;
        free( nnMap );
        nnMap = pNext;
    }
}

/*================================================================
*   NamedNodeMap_addToNamedNodeMap
*       add a node to a NamedNodeMap.
*       Internal to parser only.
*   Parameters:
*       add: the node to add into NamedNodeMap.
*   Return:
*       IXML_SUCCESS or failure.
*
*=================================================================*/
int
ixmlNamedNodeMap_addToNamedNodeMap( IN IXML_NamedNodeMap ** nnMap,
                                    IN IXML_Node * add )
{
    IXML_NamedNodeMap *traverse = NULL,
     *p = NULL;
    IXML_NamedNodeMap *newItem = NULL;

    if( add == NULL ) {
        return IXML_SUCCESS;
    }

    if( *nnMap == NULL )        // nodelist is empty
    {
        *nnMap =
            ( IXML_NamedNodeMap * ) malloc( sizeof( IXML_NamedNodeMap ) );
        if( *nnMap == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
        ixmlNamedNodeMap_init( *nnMap );
    }

    if( ( *nnMap )->nodeItem == NULL ) {
        ( *nnMap )->nodeItem = add;
    } else {
        traverse = *nnMap;
        p = traverse;
        while( traverse != NULL ) {
            p = traverse;
            traverse = traverse->next;
        }

        newItem =
            ( IXML_NamedNodeMap * ) malloc( sizeof( IXML_NamedNodeMap ) );
        if( newItem == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
        p->next = newItem;
        newItem->nodeItem = add;
        newItem->next = NULL;
    }

    return IXML_SUCCESS;
}
