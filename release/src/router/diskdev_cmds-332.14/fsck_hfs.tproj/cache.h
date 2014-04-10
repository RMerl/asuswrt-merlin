/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * User Land Cache Manager
 *
 *  A user land cache manager.
 */
#ifndef _CACHE_H_
#define _CACHE_H_
#include <stdint.h>

/*
 * Some nice lowercase shortcuts.
 */
#define EOK					0

#define BUF_SPAN	0x80000000	/* Buffer spans several cache blocks */

typedef struct LRUNode_t
{
	struct LRUNode_t *	Next;	/* Next node in the LRU */
	struct LRUNode_t *	Prev;	/* Previous node in the LRU */
} LRUNode_t;

typedef struct LRU_t
{
	LRUNode_t			Head;	/* Dummy node for the head of the LRU */
	LRUNode_t			Busy;	/* List of busy nodes */
} LRU_t;


#define MAXBUFS  48
/*
 * Buf_t
 *
 *  Buffer structure exchanged between the cache and client. It contains the
 *  data buffer with the requested data, as well as housekeeping information
 *  that the cache needs.
 */
typedef struct Buf_t
{
	struct Buf_t *	Next;	/* Next active buffer */
	struct Buf_t *	Prev;	/* Previous active buffer */
	
	uint32_t		Flags;	/* Buffer flags */
	uint64_t		Offset;	/* Start offset of the buffer */
	uint32_t		Length;	/* Size of the buffer in bytes */

	void *			Buffer;	/* Buffer */
} Buf_t;

/*
 * Tag_t
 *
 *  The cache tag structure is a header for a cache buffer. It contains a
 *  pointer to the cache block and housekeeping information. The type of LRU
 *  algorithm can be swapped out easily.
 *
 *  NOTE: The LRU field must be the first field, so we can easily cast between
 *        the two.
 */
typedef struct Tag_t
{
	LRUNode_t		LRU;	/* LRU specific data, must be first! */
	
	struct Tag_t *	Next;	/* Next tag in hash chain */
	struct Tag_t *	Prev;	/* Previous tag in hash chain */

	uint32_t		Flags;	
	uint32_t		Refs;	/* Reference count */
	uint64_t		Offset;	/* Offset of the buffer */
	
	void *			Buffer;	/* Cache page */
} Tag_t;


/* Tag_t.Flags bit settings */
enum {
	kLazyWrite		 = 0x00000001 	/* only write this page when evicting or forced */
};

/*
 * Cache_t
 *
 *  The main cache data structure. The cache manages access between an open
 *  file and the cache client program.
 *
 *  NOTE: The LRU field must be the first field, so we can easily cast between
 *        the two.
 */
typedef struct Cache_t
{
	LRU_t		LRU;		/* LRU replacement data structure */
	
	int		FD_R;		/* File descriptor (read-only) */
	int		FD_W;		/* File descriptor (write-only) */
	uint32_t	DevBlockSize;	/* Device block size */
	
	Tag_t **	Hash;		/* Lookup hash table (move to front) */
	uint32_t	HashSize;	/* Size of the hash table */
	uint32_t	BlockSize;	/* Size of the cache page */

	void *		FreeHead;	/* Head of the free list */
	uint32_t	FreeSize;	/* Size of the free list */

	Buf_t *		ActiveBufs;	/* List of active buffers */
	Buf_t *		FreeBufs;	/* List of free buffers */

	uint32_t	ReqRead;	/* Number of read requests */
	uint32_t	ReqWrite;	/* Number of write requests */
	
	uint32_t	DiskRead;	/* Number of actual disk reads */
	uint32_t	DiskWrite;	/* Number of actual disk writes */

	uint32_t	Span;		/* Requests that spanned cache blocks */
} Cache_t;


/*
 * CacheInit
 *
 *  Initializes the cache for use.
 */
int CacheInit (Cache_t *cache, int fdRead, int fdWrite, uint32_t devBlockSize,
               uint32_t cacheBlockSize, uint32_t cacheSize, uint32_t hashSize);

/*
 * CacheDestroy
 * 
 *  Shutdown the cache.
 */
int CacheDestroy (Cache_t *cache);

/*
 * CacheRead
 *
 *  Reads a range of bytes from the cache, returning a pointer to a buffer
 *  containing the requested bytes.
 *
 *  NOTE: The returned buffer may directly refer to a cache block, or an
 *        anonymous buffer. Do not make any assumptions about the nature of
 *        the returned buffer, except that it is contiguous.
 */
int CacheRead (Cache_t *cache, uint64_t start, uint32_t len, Buf_t **buf);

/* 
 * CacheWrite
 *
 *  Writes a buffer through the cache.
 */
int CacheWrite ( Cache_t *cache, Buf_t *buf, int age, uint32_t writeOptions );

/*
 * CacheRelease
 *
 *  Releases a clean buffer.
 *
 *  NOTE: We don't verify whether it's dirty or not.
 */
int CacheRelease (Cache_t *cache, Buf_t *buf, int age);

/* CacheRemove
 *
 *  Disposes of a particular tag and buffer.
 */
int CacheRemove (Cache_t *cache, Tag_t *tag);

/*
 * CacheEvict
 *
 *  Only dispose of the buffer, leave the tag intact.
 */
int CacheEvict (Cache_t *cache, Tag_t *tag);

/*
 * CacheFlush
 *
 *  Write out any blocks that are marked for lazy write.
 */
int 
CacheFlush( Cache_t *cache );

/*
 * CacheFlushRange
 *
 * Flush, and optionally remove, all cache blocks that intersect
 * a given range.
 */
int
CacheFlushRange( Cache_t *cache, uint64_t start, uint64_t len, int remove);

#endif

