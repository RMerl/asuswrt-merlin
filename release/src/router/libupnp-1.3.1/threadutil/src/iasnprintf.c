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

#include <stdarg.h>
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include "iasnprintf.h"

#ifndef NULL
#define NULL 0
#endif

/**
 * Allocates enough memory for the
 * Formatted string, up to max
 * specified.
 * With max set to -1, it allocates as
 * much size as needed.
 * Memory must be freed using free.
 */

int
iasnprintf( char **ret,
            int incr,
            int max,
            const char *fmt,
            ... )
{
    int size = incr;
    int retc = 0;
    va_list ap;
    char *temp = NULL;

    assert( ret );
    assert( fmt );
    ( *ret ) = ( char * )malloc( incr );

    if( ( *ret ) == NULL ) return -1;

    while( 1 ) {
        va_start( ap, fmt );
        retc = vsnprintf( ( *ret ), size, fmt, ap );
        va_end( ap );

        if( retc < 0 ) {
            //size not big enough
            //and vsnprintf does NOT return the
            //necessary number of bytes
            if( ( max != -1 ) && ( size == max ) )  //max reached
            {
                break;
            }

            incr *= 2;          //increase increment
            //increase size and try again  
            if( ( max != -1 ) && ( ( size + incr ) > max ) ) {
                incr = ( max - size );
            }

            temp = ( char * )realloc( ( *ret ), size + incr );
            if( temp == NULL ) {
                break;
            }
            size += incr;
            ( *ret ) = temp;

        } else {
            if( ( retc + 1 ) > size ) {
                //size not big enough
                //and vsnprintf 
                //returns the necessary 
                //number of bytes
                if( ( max != -1 ) && ( retc + 1 > max ) ) {
                    retc = -1;
                    break;
                }

                temp = ( char * )realloc( ( *ret ), retc + 1 );
                if( temp == NULL ) {
                    retc = -1;
                    break;
                }
                size = retc + 1;
                ( *ret ) = temp;    //size increased try again
            } else if( ( retc + 1 ) < size ) {
                //size is bigger than needed
                //try and reallocate smaller

                temp = ( char * )realloc( ( *ret ), retc + 1 );
                if( temp != NULL ) {
                    ( *ret ) = temp;
                }
                break;
            } else              //size is just right, exit
            {
                break;
            }

        }
    }

    if( retc < 0 ) {
        free( ( *ret ) );
        ( *ret ) = NULL;
    }

    return retc;
}

void
iasnprintfFree( char *fChar )
{
    free( fChar );
    fChar = NULL;
}
