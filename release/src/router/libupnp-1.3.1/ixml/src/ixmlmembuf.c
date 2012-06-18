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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ixmlmembuf.h"
#include "ixml.h"

/*================================================================
*   ixml_membuf_set_size
*
*   Increases or decreases buffer cap so that at least
*   'new_length' bytes can be stored
*
*   On error, m's fields do not change.
*
*   returns:
*       UPNP_E_SUCCESS
*       UPNP_E_OUTOF_MEMORY
*
*=================================================================*/
static int
ixml_membuf_set_size( INOUT ixml_membuf * m,
                      IN size_t new_length )
{
    size_t diff;
    size_t alloc_len;
    char *temp_buf;

    if( new_length >= m->length )   // increase length
    {
        // need more mem?
        if( new_length <= m->capacity ) {
            return 0;           // have enough mem; done
        }

        diff = new_length - m->length;
        alloc_len = MAXVAL( m->size_inc, diff ) + m->capacity;
    } else                      // decrease length
    {
        assert( new_length <= m->length );

        // if diff is 0..m->size_inc, don't free
        if( ( m->capacity - new_length ) <= m->size_inc ) {
            return 0;
        }

        alloc_len = new_length + m->size_inc;
    }

    assert( alloc_len >= new_length );

    temp_buf = realloc( m->buf, alloc_len + 1 );
    if( temp_buf == NULL ) {
        // try smaller size
        alloc_len = new_length;
        temp_buf = realloc( m->buf, alloc_len + 1 );

        if( temp_buf == NULL ) {
            return IXML_INSUFFICIENT_MEMORY;
        }
    }
    // save
    m->buf = temp_buf;
    m->capacity = alloc_len;
    return 0;
}

/*================================================================
*   membuffer_init
*
*
*=================================================================*/
void
ixml_membuf_init( INOUT ixml_membuf * m )
{
    assert( m != NULL );

    m->size_inc = MEMBUF_DEF_SIZE_INC;
    m->buf = NULL;
    m->length = 0;
    m->capacity = 0;
}

/*================================================================
*   membuffer_destroy
*
*
*=================================================================*/
void
ixml_membuf_destroy( INOUT ixml_membuf * m )
{
    if( m == NULL ) {
        return;
    }

    free( m->buf );
    ixml_membuf_init( m );
}

/*================================================================
*   ixml_membuf_assign
*
*
*=================================================================*/
int
ixml_membuf_assign( INOUT ixml_membuf * m,
                    IN const void *buf,
                    IN size_t buf_len )
{
    int return_code;

    assert( m != NULL );

    // set value to null
    if( buf == NULL ) {
        ixml_membuf_destroy( m );
        return IXML_SUCCESS;
    }
    // alloc mem
    return_code = ixml_membuf_set_size( m, buf_len );
    if( return_code != 0 ) {
        return return_code;
    }
    // copy
    memcpy( m->buf, buf, buf_len );
    m->buf[buf_len] = 0;        // null-terminate

    m->length = buf_len;

    return IXML_SUCCESS;

}

/*================================================================
*   ixml_membuf_assign_str
*
*
*=================================================================*/
int
ixml_membuf_assign_str( INOUT ixml_membuf * m,
                        IN const char *c_str )
{
    return ixml_membuf_assign( m, c_str, strlen( c_str ) );
}

/*================================================================
*   ixml_membuf_append
*
*
*=================================================================*/
int
ixml_membuf_append( INOUT ixml_membuf * m,
                    IN const void *buf )
{
    assert( m != NULL );

    return ixml_membuf_insert( m, buf, 1, m->length );
}

/*================================================================
*   ixml_membuf_append_str
*
*
*=================================================================*/
int
ixml_membuf_append_str( INOUT ixml_membuf * m,
                        IN const char *c_str )
{
    return ixml_membuf_insert( m, c_str, strlen( c_str ), m->length );
}

/*================================================================
*   ixml_membuf_insert
*
*
*=================================================================*/
int
ixml_membuf_insert( INOUT ixml_membuf * m,
                    IN const void *buf,
                    IN size_t buf_len,
                    int index )
{
    int return_code;

    assert( m != NULL );

    if( index < 0 || index > ( int )m->length )
        return IXML_INDEX_SIZE_ERR;

    if( buf == NULL || buf_len == 0 ) {
        return 0;
    }
    // alloc mem
    return_code = ixml_membuf_set_size( m, m->length + buf_len );
    if( return_code != 0 ) {
        return return_code;
    }
    // insert data
    // move data to right of insertion point
    memmove( m->buf + index + buf_len, m->buf + index, m->length - index );
    memcpy( m->buf + index, buf, buf_len );
    m->length += buf_len;
    m->buf[m->length] = 0;      // null-terminate

    return 0;
}
