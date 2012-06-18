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

#ifndef GENLIB_UTIL_MEMBUFFER_H
#define GENLIB_UTIL_MEMBUFFER_H


#include <stdlib.h>
#include "util.h"

#define MINVAL( a, b ) ( (a) < (b) ? (a) : (b) )
#define MAXVAL( a, b ) ( (a) > (b) ? (a) : (b) )

// pointer to a chunk of memory
typedef struct // memptr
{
	char	*buf;			// start of memory (read/write)
	size_t	length;			// length of memory (read-only)
} memptr;


// maintains a block of dynamically allocated memory
// note: Total length/capacity should not exceed MAX_INT
typedef struct // membuffer
{
	char	*buf;			// mem buffer; must not write 
							//   beyond buf[length-1] (read/write)
	size_t	length;			// length of buffer (read-only)
	size_t	capacity;		// total allocated memory (read-only)
	size_t	size_inc;		// used to increase size; MUST be > 0; (read/write)

	// default value of size_inc
	#define MEMBUF_DEF_SIZE_INC		5
} membuffer;

//--------------------------------------------------
//////////////// functions /////////////////////////
//--------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/************************************************************************
*	Function :	str_alloc
*
*	Parameters :
*		IN const char* str ;	input string object
*		IN size_t str_len ;		input string length
*
*	Description :	Allocate memory and copy information from the input 
*		string to the newly allocated memory.
*
*	Return : char* ;
*		Pointer to the newly allocated memory.
*		NULL if memory cannot be allocated.
*
*	Note :
************************************************************************/
char* str_alloc( IN const char* str, IN size_t str_len );

/************************************************************************
*	Function :	memptr_cmp
*
*	Parameters :
*		IN memptr* m ;	input memory object
*		IN const char* s ;	constatnt string for the memory object to be 
*					compared with
*
*	Description : Compares characters of strings passed for number of 
*		bytes. If equal for the number of bytes, the length of the bytes 
*		determines which buffer is shorter.
*
*	Return : int ;
*		< 0 string1 substring less than string2 substring 
*		0 string1 substring identical to string2 substring 
*		> 0 string1 substring greater than string2 substring 
*
*	Note :
************************************************************************/
int memptr_cmp( IN memptr* m, IN const char* s );

/************************************************************************
*	Function :	memptr_cmp_nocase
*
*	Parameters :
*		IN memptr* m ;	input memory object
*		IN const char* s ;	constatnt string for the memory object to be 
*					compared with
*
*	Description : Compares characters of 2 strings irrespective of the 
*		case for a specific count of bytes  If the character comparison 
*		is the same the length of the 2 srings determines the shorter 
*		of the 2 strings.
*
*	Return : int ;
*		< 0 string1 substring less than string2 substring 
*		0 string1 substring identical to string2 substring 
*		> 0 string1 substring greater than string2 substring 
*  
*	Note :
************************************************************************/
int memptr_cmp_nocase( IN memptr* m, IN const char* s );


/************************************************************************
*	Function :	membuffer_set_size
*
*	Parameters :
*		INOUT membuffer* m ;	buffer whose size is to be modified
*		IN size_t new_length ;	new size to which the buffer will be 
*					modified
*
*	Description : Increases or decreases buffer cap so that at least
*	   'new_length' bytes can be stored
*
*	Return : int ;
*		UPNP_E_SUCCESS - On Success
*		UPNP_E_OUTOF_MEMORY - On failure to allocate memory.
*
*	Note :
************************************************************************/
int membuffer_set_size( INOUT membuffer* m, 
			IN size_t new_length );

/************************************************************************
*	Function :	membuffer_init
*
*	Parameters :
*		INOUT membuffer* m ; buffer	to be initialized
*
*	Description : Wrapper to membuffer_initialize().
*		Set the size of the buffer to MEMBUF_DEF_SIZE_INC
*		Initializes m->buf to NULL, length=0
*
*	Return : void ;
*
*	Note :
************************************************************************/
void membuffer_init( INOUT membuffer* m );

/************************************************************************
*	Function :	membuffer_destroy
*
*	Parameters :
*		INOUT membuffer* m ;	buffer to be destroyed
*
*	Description : Free's memory allocated for membuffer* m.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void membuffer_destroy( INOUT membuffer* m );


/************************************************************************
*	Function :	membuffer_assign
*
*	Parameters :
*		INOUT membuffer* m ; buffer whose memory is to be allocated and 
*					assigned.
*		IN const void* buf ; source buffer whose contents will be copied
*		IN size_t buf_len ;	 length of the source buffer
*
*	Description : Allocate memory to membuffer* m and copy the contents 
*		of the in parameter IN const void* buf.
*
*	Return : int ;
*	 UPNP_E_SUCCESS
*	 UPNP_E_OUTOF_MEMORY
*
*	Note :
************************************************************************/
int membuffer_assign( INOUT membuffer* m, IN const void* buf, 
					 IN size_t buf_len );

/************************************************************************
*	Function :	membuffer_assign_str
*
*	Parameters :
*		INOUT membuffer* m ;	buffer to be allocated and assigned
*		IN const char* c_str ;	source buffer whose contents will be 
*					copied
*
*	Description : Wrapper function for membuffer_assign()
*
*	Return : int ;
*	 UPNP_E_SUCCESS
*	 UPNP_E_OUTOF_MEMORY
*
*	Note :
************************************************************************/
int membuffer_assign_str( INOUT membuffer* m, IN const char* c_str );

/************************************************************************
*	Function :	membuffer_append
*
*	Parameters :
*		INOUT membuffer* m ; buffer whose memory is to be appended.
*		IN const void* buf ; source buffer whose contents will be 
*					copied
*		IN size_t buf_len ;	length of the source buffer
*
*	Description : Invokes function to appends data from a constant buffer 
*		to the buffer 
*
*	Return : int ;
*
*	Note :
************************************************************************/
int membuffer_append( INOUT membuffer* m, IN const void* buf,
					 IN size_t buf_len );

/************************************************************************
*	Function :	membuffer_append_str
*
*	Parameters :
*		INOUT membuffer* m ;	buffer whose memory is to be appended.
*		IN const char* c_str ;	source buffer whose contents will be 
*					copied
*
*	Description : Invokes function to appends data from a constant string 
*		to the buffer 	
*
*	Return : int ;
*
*	Note :
************************************************************************/
int membuffer_append_str( INOUT membuffer* m, IN const char* c_str );

/************************************************************************
*	Function :	membuffer_insert
*
*	Parameters :
*		INOUT membuffer* m ; buffer whose memory size is to be increased  
*					and appended.
*		IN const void* buf ; source buffer whose contents will be 
*					copied
*		 IN size_t buf_len ; size of the source buffer
*		int index ;	index to determine the bounds while movinf the data
*
*	Description : Allocates memory for the new data to be inserted. Does
*		memory management by moving the data from the existing memory to 
*		the newly allocated memory and then appending the new data.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int membuffer_insert( INOUT membuffer* m, IN const void* buf, 
					 IN size_t buf_len, int index );


/************************************************************************
*	Function :	membuffer_delete
*
*	Parameters :
*		INOUT membuffer* m ; buffer whose memory size is to be decreased
*					and copied to the odified location
*		IN int index ;	index to determine bounds while moving data
*		IN size_t num_bytes ;	number of bytes that the data needs to 
*					shrink by
*
*	Description : Shrink the size of the buffer depending on the current 
*		size of the bufer and te input parameters. Move contents from the 
*		old buffer to the new sized buffer.
*
*	Return : void ;
*
*	Note :
************************************************************************/
void membuffer_delete( INOUT membuffer* m, IN int index, 
					 IN size_t num_bytes );


/************************************************************************
*	Function :	membuffer_detach
*
*	Parameters :
*		INOUT membuffer* m ; buffer to be returned and updated.	
*
*	Description : Detaches current buffer and returns it. The caller
*		must free the returned buffer using free().
*		After this call, length becomes 0.
*
*	Return : char* ;
*		a pointer to the current buffer
*
*	Note :
************************************************************************/
char* membuffer_detach( INOUT membuffer* m );

/************************************************************************
*	Function :	membuffer_attach
*
*	Parameters :
*		INOUT membuffer* m ;	buffer to be updated
*		IN char* new_buf ;	 source buffer which will be assigned to the 
*					buffer to be updated
*		IN size_t buf_len ;	length of the source buffer 
*
*	Description : Free existing memory in membuffer and assign the new 
*		buffer in its place.
*
*	Return : void ;
*
*	Note : 'new_buf' must be allocted using malloc or realloc so
*		that it can be freed using free()
************************************************************************/
void membuffer_attach( INOUT membuffer* m, IN char* new_buf,
					   IN size_t buf_len );
#ifdef __cplusplus
}		// extern "C"
#endif	// __cplusplus

#endif // GENLIB_UTIL_H
