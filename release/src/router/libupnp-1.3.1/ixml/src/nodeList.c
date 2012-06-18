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
*   ixmlNodeList_init
*       initializes a nodelist 
*       External function.
*
*=================================================================*/
void
ixmlNodeList_init( IXML_NodeList * nList )
{
    assert( nList != NULL );

    memset( nList, 0, sizeof( IXML_NodeList ) );

}

/*================================================================
*   ixmlNodeList_item
*       Returns the indexth item in the collection. If index is greater
*       than or equal to the number of nodes in the list, this returns 
*       null.
*       External function.
*
*=================================================================*/
IXML_Node *
ixmlNodeList_item( IXML_NodeList * nList,
                   unsigned long index )
{
    IXML_NodeList *next;
    unsigned int i;

    // if the list ptr is NULL
    if( nList == NULL ) {
        return NULL;
    }
    // if index is more than list length
    if( index > ixmlNodeList_length( nList ) - 1 ) {
        return NULL;
    }

    next = nList;
    for( i = 0; i < index && next != NULL; ++i ) {
        next = next->next;
    }

    if( next == NULL ) return NULL;

    return next->nodeItem;

}

/*================================================================
*   ixmlNodeList_addToNodeList
*       Add a node to nodelist
*       Internal to parser only.
*
*=================================================================*/
int
ixmlNodeList_addToNodeList( IN IXML_NodeList ** nList,
                            IN IXML_Node * add )
{
    IXML_NodeList *traverse,
     *p = NULL;
    IXML_NodeList *newListItem;

    assert( add != NULL );

    if( add == NULL ) {
        return IXML_FAILED;
    }

    if( *nList == NULL )        // nodelist is empty
    {
        *nList = ( IXML_NodeList * ) malloc( sizeof( IXML_NodeList ) );
        if( *nList == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }

        ixmlNodeList_init( *nList );
    }

    if( ( *nList )->nodeItem == NULL ) {
        ( *nList )->nodeItem = add;
    } else {
        traverse = *nList;
        while( traverse != NULL ) {
            p = traverse;
            traverse = traverse->next;
        }

        newListItem =
            ( IXML_NodeList * ) malloc( sizeof( IXML_NodeList ) );
        if( newListItem == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
        p->next = newListItem;
        newListItem->nodeItem = add;
        newListItem->next = NULL;
    }

    return IXML_SUCCESS;
}

/*================================================================
*   ixmlNodeList_length
*       Returns the number of nodes in the list.  The range of valid
*       child node indices is 0 to length-1 inclusive.
*       External function.       
*
*=================================================================*/
unsigned long
ixmlNodeList_length( IN IXML_NodeList * nList )
{
    IXML_NodeList *list;
    unsigned long length = 0;

    list = nList;
    while( list != NULL ) {
        ++length;
        list = list->next;
    }

    return length;
}

/*================================================================
*   ixmlNodeList_free
*       frees a nodeList
*       External function
*       
*=================================================================*/
void
ixmlNodeList_free( IN IXML_NodeList * nList )
{
    IXML_NodeList *next;

    while( nList != NULL ) {
        next = nList->next;

        free( nList );
        nList = next;
    }

}
