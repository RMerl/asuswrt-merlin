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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if LINUX
#include "missing.h"
#else
#include <sys/types.h>
#endif /* __LINUX__ */
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>

#include "cache.h"


#define CACHE_DEBUG  0

/*
 * CacheAllocBlock
 *
 *  Allocate an unused cache block.
 */
void *CacheAllocBlock (Cache_t *cache);

/*
 * CacheFreeBlock
 *
 *  Release an active cache block.
 */
static int 
CacheFreeBlock( Cache_t *cache, Tag_t *tag );

/*
 * CacheLookup
 *
 *  Obtain a cache block. If one already exists, it is returned. Otherwise a
 *  new one is created and inserted into the cache.
 */
int CacheLookup (Cache_t *cache, uint64_t off, Tag_t **tag);

/*
 * CacheRawRead
 *
 *  Perform a direct read on the file.
 */
int CacheRawRead (Cache_t *cache, uint64_t off, uint32_t len, void *buf);

/*
 * CacheRawWrite
 *
 *  Perform a direct write on the file.
 */
int CacheRawWrite (Cache_t *cache, uint64_t off, uint32_t len, void *buf);



/*
 * LRUInit
 *
 *  Initializes the LRU data structures.
 */
static int LRUInit (LRU_t *lru);

/*
 * LRUDestroy
 *
 *  Shutdown the LRU.
 *
 *  NOTE: This is typically a no-op, since the cache manager will be clearing
 *        all cache tags.
 */
static int LRUDestroy (LRU_t *lru);

/*
 * LRUHit
 *
 *  Registers data activity on the given node. If the node is already in the
 *  LRU, it is moved to the front. Otherwise, it is inserted at the front.
 *
 *  NOTE: If the node is not in the LRU, we assume that its pointers are NULL.
 */
static int LRUHit (LRU_t *lru, LRUNode_t *node, int age);

/*
 * LRUEvict
 *
 *  Chooses a buffer to release.
 *
 *  TODO: Under extreme conditions, it shoud be possible to release the buffer
 *        of an actively referenced cache buffer, leaving the tag behind as a
 *        placeholder. This would be required for implementing 2Q-LRU
 *        replacement.
 */
static int LRUEvict (LRU_t *lru, LRUNode_t *node);



/*
 * CacheInit
 *
 *  Initializes the cache for use.
 */
int CacheInit (Cache_t *cache, int fdRead, int fdWrite, uint32_t devBlockSize,
               uint32_t cacheBlockSize, uint32_t cacheSize, uint32_t hashSize)
{
	void **		temp;
	uint32_t	i;
	Buf_t *		buf;
	
	memset (cache, 0x00, sizeof (Cache_t));

	cache->FD_R = fdRead;
	cache->FD_W = fdWrite;
	cache->DevBlockSize = devBlockSize;
	/* CacheFlush requires cleared cache->Hash  */
	cache->Hash = (Tag_t **) calloc( 1, (sizeof (Tag_t *) * hashSize) );
	cache->HashSize = hashSize;
	cache->BlockSize = cacheBlockSize;

	/* Allocate the cache memory */
	cache->FreeHead = mmap (NULL,
	                        cacheSize * cacheBlockSize,
	                        PROT_READ | PROT_WRITE,
	                        MAP_ANON | MAP_PRIVATE,
	                        -1,
	                        0);
	if (cache->FreeHead == NULL) return (ENOMEM);

	/* Initialize the cache memory free list */
	temp = cache->FreeHead;
	for (i = 0; i < cacheSize - 1; i++) {
		*temp = ((char *)temp + cacheBlockSize);
		temp  = (void **)((char *)temp + cacheBlockSize);
	}
	*temp = NULL;
	cache->FreeSize = cacheSize;

	buf = (Buf_t *)malloc(sizeof(Buf_t) * MAXBUFS);
	if (buf == NULL) return (ENOMEM);

	memset (&buf[0], 0x00, sizeof (Buf_t) * MAXBUFS);
	for (i = 1 ; i < MAXBUFS ; i++) {
		(&buf[i-1])->Next = &buf[i];
	}
	cache->FreeBufs = &buf[0];

#if CACHE_DEBUG
	printf( "%s - cacheSize %d cacheBlockSize %d hashSize %d \n", 
			__FUNCTION__, cacheSize, cacheBlockSize, hashSize );
	printf( "%s - cache memory %d \n", __FUNCTION__, (cacheSize * cacheBlockSize) );
#endif  

	return (LRUInit (&cache->LRU));
}


/*
 * CacheDestroy
 * 
 *  Shutdown the cache.
 */
int CacheDestroy (Cache_t *cache)
{
	CacheFlush( cache );

#if CACHE_DEBUG
	/* Print cache report */
	printf ("Cache Report:\n");
	printf ("\tRead Requests:  %d\n", cache->ReqRead);
	printf ("\tWrite Requests: %d\n", cache->ReqWrite);
	printf ("\tDisk Reads:     %d\n", cache->DiskRead);
	printf ("\tDisk Writes:    %d\n", cache->DiskWrite);
	printf ("\tSpans:          %d\n", cache->Span);
#endif	
	/* Shutdown the LRU */
	LRUDestroy (&cache->LRU);
	
	/* I'm lazy, I'll come back to it :P */
	return (EOK);
}

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
int CacheRead (Cache_t *cache, uint64_t off, uint32_t len, Buf_t **bufp)
{
	Tag_t *		tag;
	Buf_t *		searchBuf;
	Buf_t *		buf;
	uint32_t	coff = (off % cache->BlockSize);
	uint64_t	cblk = (off - coff);
	int			error;

	/* Check for conflicts with other bufs */
	searchBuf = cache->ActiveBufs;
	while (searchBuf != NULL) {
		if ((searchBuf->Offset >= off) && (searchBuf->Offset < off + len)) {
#if CACHE_DEBUG
			printf ("ERROR: CacheRead: Deadlock\n");
#endif
			return (EDEADLK);
		}
		
		searchBuf = searchBuf->Next;
	}
	
	/* get a free buffer */
	if ((buf = cache->FreeBufs) == NULL) {
#if CACHE_DEBUG
		printf ("ERROR: CacheRead: no more bufs!\n");
#endif
		return (ENOBUFS);
	}
	cache->FreeBufs = buf->Next; 
	*bufp = buf;

	/* Clear the buf structure */
	buf->Next	= NULL;
	buf->Prev	= NULL;
	buf->Flags	= 0;
	buf->Offset	= off;
	buf->Length	= len;
	buf->Buffer	= NULL;
	
	/* If this is unaligned or spans multiple cache blocks */
	if ((cblk / cache->BlockSize) != ((off + len - 1) / cache->BlockSize)) {
		buf->Flags |= BUF_SPAN;
	}
	/* Fetch the first cache block */
	error = CacheLookup (cache, cblk, &tag);
	if (error != EOK) {
#if CACHE_DEBUG
		printf ("ERROR: CacheRead: CacheLookup error\n");
#endif
		return (error);
	}

	/* If we live nicely inside a cache block */
	if (!(buf->Flags & BUF_SPAN)) {
		/* Offset the buffer into the cache block */
		buf->Buffer = tag->Buffer + coff;

		/* Bump the cache block's reference count */
		tag->Refs++;
		
		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, 0);

	/* Otherwise, things get ugly */
	} else {
		uint32_t	boff;	/* Offset into the buffer */
		uint32_t	blen;	/* Space to fill in the buffer */
		uint32_t	temp;

		/* Allocate a temp buffer */
		buf->Buffer = (void *)malloc (len);
		if (buf->Buffer == NULL) {
#if CACHE_DEBUG
			printf ("ERROR: CacheRead: No Memory\n");
#endif
			return (ENOMEM);
		}

		/* Blit the first chunk into the buffer */
		boff = cache->BlockSize - coff;
		blen = len - boff;
		memcpy (buf->Buffer, tag->Buffer + coff, boff);
		
		/* Bump the cache block's reference count */
		tag->Refs++;

		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, 0);

		/* Next cache block */
		cblk += cache->BlockSize;
		
		/* Read data a cache block at a time */
		while (blen) {
			/* Fetch the next cache block */
			error = CacheLookup (cache, cblk, &tag);
			if (error != EOK) {
				/* Free the allocated buffer */
				free (buf->Buffer);
				buf->Buffer = NULL;

				/* Release all the held tags */
				cblk -= cache->BlockSize;
				while (!boff) {
					if (CacheLookup (cache, cblk, &tag) != EOK) {
						fprintf (stderr, "CacheRead: Unrecoverable error\n");
						exit (-1);
					}
					tag->Refs--;
					
					/* Kick the node into the right queue */
					LRUHit (&cache->LRU, (LRUNode_t *)tag, 0);
				}

				return (error);
			}

			/* Blit the cache block into the buffer */
			temp = ((blen > cache->BlockSize) ? cache->BlockSize : blen);
			memcpy (buf->Buffer + boff,
			        tag->Buffer,
					temp);

			/* Update counters */
			boff += temp;
			blen -= temp;
			tag->Refs++;

			/* Kick the node into the right queue */
			LRUHit (&cache->LRU, (LRUNode_t *)tag, 0);
		}

		/* Count the spanned access */
		cache->Span++;
	}

	/* Attach to head of active buffers list */
	if (cache->ActiveBufs != NULL) {
		buf->Next = cache->ActiveBufs;
		buf->Prev = NULL;

		cache->ActiveBufs->Prev = buf;

	} else {
		cache->ActiveBufs = buf;
	}

	/* Update counters */
	cache->ReqRead++;
	return (EOK);
}

/* 
 * CacheWrite
 *
 *  Writes a buffer through the cache.
 */
int CacheWrite ( Cache_t *cache, Buf_t *buf, int age, uint32_t writeOptions )
{
	Tag_t *		tag;
	uint32_t	coff = (buf->Offset % cache->BlockSize);
	uint64_t	cblk = (buf->Offset - coff);
	int			error;

	/* Fetch the first cache block */
	error = CacheLookup (cache, cblk, &tag);
	if (error != EOK) return (error);
	
	/* If the buffer was a direct reference */
	if (!(buf->Flags & BUF_SPAN)) {
		/* Commit the dirty block */
		if ( (writeOptions & kLazyWrite) != 0 ) 
		{
			/* flag this for lazy write */
			tag->Flags |= kLazyWrite;
		}
		else
		{
			error = CacheRawWrite (cache,
								   tag->Offset,
								   cache->BlockSize,
								   tag->Buffer);
			if (error != EOK) return (error);
		}
		
		/* Release the reference */
		tag->Refs--;

		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, age);

	/* Otherwise, we do the ugly thing again */
	} else {
		uint32_t	boff;	/* Offset into the buffer */
		uint32_t	blen;	/* Space to fill in the buffer */
		uint32_t	temp;

		/* Blit the first chunk back into the cache */
		boff = cache->BlockSize - coff;
		blen = buf->Length - boff;
		memcpy (tag->Buffer + coff, buf->Buffer, boff);
		
		/* Commit the dirty block */
		if ( (writeOptions & kLazyWrite) != 0 ) 
		{
			/* flag this for lazy write */
			tag->Flags |= kLazyWrite;
		}
		else
		{
			error = CacheRawWrite (cache,
								   tag->Offset,
								   cache->BlockSize,
								   tag->Buffer);
			if (error != EOK) return (error);
		}
		
		/* Release the cache block reference */
		tag->Refs--;

		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, age);
			
		/* Next cache block */
		cblk += cache->BlockSize;
		
		/* Write data a cache block at a time */
		while (blen) {
			/* Fetch the next cache block */
			error = CacheLookup (cache, cblk, &tag);
			/* We must go through with the write regardless */

			/* Blit the next buffer chunk back into the cache */
			temp = ((blen > cache->BlockSize) ? cache->BlockSize : blen);
			memcpy (tag->Buffer,
					buf->Buffer + boff,
					temp);

			/* Commit the dirty block */
			if ( (writeOptions & kLazyWrite) != 0 ) 
			{
				/* flag this for lazy write */
				tag->Flags |= kLazyWrite;
			}
			else
			{
				error = CacheRawWrite (cache,
									   tag->Offset,
									   cache->BlockSize,
									   tag->Buffer);
				if (error != EOK) return (error);
			}

			/* Update counters */
			boff += temp;
			blen -= temp;
			tag->Refs--;

			/* Kick the node into the right queue */
			LRUHit (&cache->LRU, (LRUNode_t *)tag, age);
		}

		/* Release the anonymous buffer */
		free (buf->Buffer);
	}

	/* Detach the buffer */
	if (buf->Next != NULL)
		buf->Next->Prev = buf->Prev;
	if (buf->Prev != NULL)
		buf->Prev->Next = buf->Next;
	if (cache->ActiveBufs == buf)
		cache->ActiveBufs = buf->Next;

	/* Clear the buffer and put it back on free list */
	memset (buf, 0x00, sizeof (Buf_t));
	buf->Next = cache->FreeBufs; 
	cache->FreeBufs = buf; 		

	/* Update counters */
	cache->ReqWrite++;

	return (EOK);
}

/*
 * CacheRelease
 *
 *  Releases a clean buffer.
 *
 *  NOTE: We don't verify whether it's dirty or not.
 */
int CacheRelease (Cache_t *cache, Buf_t *buf, int age)
{
	Tag_t *		tag;
	uint32_t	coff = (buf->Offset % cache->BlockSize);
	uint64_t	cblk = (buf->Offset - coff);
	int			error;

	/* Fetch the first cache block */
	error = CacheLookup (cache, cblk, &tag);
	if (error != EOK) {
#if CACHE_DEBUG
		printf ("ERROR: CacheRelease: CacheLookup error\n");
#endif
		return (error);
	}
	
	/* If the buffer was a direct reference */
	if (!(buf->Flags & BUF_SPAN)) {
		/* Release the reference */
		tag->Refs--;

		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, age);

	/* Otherwise, we do the ugly thing again */
	} else {
		uint32_t	blen;	/* Space to fill in the buffer */

		/* Blit the first chunk back into the cache */
		blen = buf->Length - cache->BlockSize + coff;
		
		/* Release the cache block reference */
		tag->Refs--;

		/* Kick the node into the right queue */
		LRUHit (&cache->LRU, (LRUNode_t *)tag, age);

		/* Next cache block */
		cblk += cache->BlockSize;
		
		/* Release cache blocks one at a time */
		while (blen) {
			/* Fetch the next cache block */
			error = CacheLookup (cache, cblk, &tag);
			/* We must go through with the write regardless */

			/* Update counters */
			blen -= ((blen > cache->BlockSize) ? cache->BlockSize : blen);
			tag->Refs--;

			/* Kick the node into the right queue */
			LRUHit (&cache->LRU, (LRUNode_t *)tag, age);
		}

		/* Release the anonymous buffer */
		free (buf->Buffer);
	}

	/* Detach the buffer */
	if (buf->Next != NULL)
		buf->Next->Prev = buf->Prev;
	if (buf->Prev != NULL)
		buf->Prev->Next = buf->Next;
	if (cache->ActiveBufs == buf)
		cache->ActiveBufs = buf->Next;

	/* Clear the buffer and put it back on free list */
	memset (buf, 0x00, sizeof (Buf_t));
	buf->Next = cache->FreeBufs; 
	cache->FreeBufs = buf; 		

	return (EOK);
}

/*
 * CacheRemove
 *
 *  Disposes of a particular buffer.
 */
int CacheRemove (Cache_t *cache, Tag_t *tag)
{
	int			error;

	/* Make sure it's not busy */
	if (tag->Refs) return (EBUSY);
	
	/* Detach the tag */
	if (tag->Next != NULL)
		tag->Next->Prev = tag->Prev;
	if (tag->Prev != NULL)
		tag->Prev->Next = tag->Next;
	else
		cache->Hash[tag->Offset % cache->HashSize] = tag->Next;
	
	/* Make sure the head node doesn't have a back pointer */
	if ((cache->Hash[tag->Offset % cache->HashSize] != NULL) &&
	    (cache->Hash[tag->Offset % cache->HashSize]->Prev != NULL)) {
#if CACHE_DEBUG
		printf ("ERROR: CacheRemove: Corrupt hash chain\n");
#endif
	}
	
	/* Release it's buffer (if it has one) */
	if (tag->Buffer != NULL)
	{
		error = CacheFreeBlock (cache, tag);
		if ( EOK != error )
			return( error );
	}

	/* Zero the tag (for easy debugging) */
	memset (tag, 0x00, sizeof (Tag_t));

	/* Free the tag */
	free (tag);

	return (EOK);
}

/*
 * CacheEvict
 *
 *  Only dispose of the buffer, leave the tag intact.
 */
int CacheEvict (Cache_t *cache, Tag_t *tag)
{
	int			error;

	/* Make sure it's not busy */
	if (tag->Refs) return (EBUSY);

	/* Release the buffer */
	if (tag->Buffer != NULL)
	{
		error = CacheFreeBlock (cache, tag);
		if ( EOK != error )
			return( error );
	}
	tag->Buffer = NULL;

	return (EOK);
}

/*
 * CacheAllocBlock
 *
 *  Allocate an unused cache block.
 */
void *CacheAllocBlock (Cache_t *cache)
{
	void *	temp;
	
	if (cache->FreeHead == NULL)
		return (NULL);
	if (cache->FreeSize == 0)
		return (NULL);

	temp = cache->FreeHead;
	cache->FreeHead = *((void **)cache->FreeHead);
	cache->FreeSize--;

	return (temp);
}

/*
 * CacheFreeBlock
 *
 *  Release an active cache block.
 */
static int 
CacheFreeBlock( Cache_t *cache, Tag_t *tag )
{
	int			error;
	
	if ( (tag->Flags & kLazyWrite) != 0 )
	{
		/* this cache block has been marked for lazy write - do it now */
		error = CacheRawWrite( cache,
							   tag->Offset,
							   cache->BlockSize,
							   tag->Buffer );
		if ( EOK != error ) 
		{
#if CACHE_DEBUG
			printf( "%s - CacheRawWrite failed with error %d \n", __FUNCTION__, error );
#endif 
			return ( error );
		}
		tag->Flags &= ~kLazyWrite;
	}

	*((void **)tag->Buffer) = cache->FreeHead;
	cache->FreeHead = (void **)tag->Buffer;
	cache->FreeSize++;
	
	return( EOK );
}


/*
 * CacheFlush
 *
 *  Write out any blocks that are marked for lazy write.
 */

int 
CacheFlush( Cache_t *cache )
{
	int			error;
	int			i;
	Tag_t *		myTagPtr;
	
	for ( i = 0; i < cache->HashSize; i++ )
	{
		myTagPtr = cache->Hash[ i ];
		
		while ( NULL != myTagPtr )
		{
			if ( (myTagPtr->Flags & kLazyWrite) != 0 )
			{
				/* this cache block has been marked for lazy write - do it now */
				error = CacheRawWrite( cache,
									   myTagPtr->Offset,
									   cache->BlockSize,
									   myTagPtr->Buffer );
				if ( EOK != error ) 
				{
#if CACHE_DEBUG
					printf( "%s - CacheRawWrite failed with error %d \n", __FUNCTION__, error );
#endif 
					return( error );
				}
				myTagPtr->Flags &= ~kLazyWrite;
			}
			myTagPtr = myTagPtr->Next; 
		} /* while */
	} /* for */

	return( EOK );
		
} /* CacheFlush */


/*
 * RangeIntersect
 *
 * Return true if the two given ranges intersect.
 *
 */
static int
RangeIntersect(uint64_t start1, uint64_t len1, uint64_t start2, uint64_t len2)
{
	uint64_t end1 = start1 + len1 - 1;
	uint64_t end2 = start2 + len2 - 1;
	
	if (end1 < start2 || start1 > end2)
		return 0;
	else
		return 1;
}


/*
 * CacheFlushRange
 *
 * Flush, and optionally remove, all cache blocks that intersect
 * a given range.
 */
int
CacheFlushRange( Cache_t *cache, uint64_t start, uint64_t len, int remove)
{
	int error;
	int i;
	Tag_t *currentTag, *nextTag;
	
	for ( i = 0; i < cache->HashSize; i++ )
	{
		currentTag = cache->Hash[ i ];
		
		while ( NULL != currentTag )
		{
			/* Keep track of the next block, in case we remove the current block */
			nextTag = currentTag->Next;

			if ( currentTag->Flags & kLazyWrite &&
				 RangeIntersect(currentTag->Offset, cache->BlockSize, start, len))
			{
				error = CacheRawWrite( cache,
									   currentTag->Offset,
									   cache->BlockSize,
									   currentTag->Buffer );
				if ( EOK != error )
				{
#if CACHE_DEBUG
					printf( "%s - CacheRawWrite failed with error %d \n", __FUNCTION__, error );
#endif 
					return error;
				}
				currentTag->Flags &= ~kLazyWrite;

				if ( remove )
					CacheRemove( cache, currentTag );
			}
			
			currentTag = nextTag;
		} /* while */
	} /* for */
	
	return EOK;
} /* CacheFlushRange */


/*
 * CacheLookup
 *
 *  Obtain a cache block. If one already exists, it is returned. Otherwise a
 *  new one is created and inserted into the cache.
 */
int CacheLookup (Cache_t *cache, uint64_t off, Tag_t **tag)
{
	Tag_t *		temp;
	uint32_t	hash = off % cache->HashSize;
	int			error;

	*tag = NULL;
	
	/* Search the hash table */
	error = 0;
	temp = cache->Hash[hash];
	while (temp != NULL) {
		if (temp->Offset == off) break;
		temp = temp->Next;
	}

	/* If it's a hit */
	if (temp != NULL) {
		/* Perform MTF if necessary */
		if (cache->Hash[hash] != temp) {
			/* Disconnect the tag */
			if (temp->Next != NULL)
				temp->Next->Prev = temp->Prev;
			temp->Prev->Next = temp->Next;
		}
		
	/* Otherwise, it's a miss */
	} else {
		/* Allocate a new tag */
		temp = (Tag_t *)calloc (sizeof (Tag_t), 1);/* We really only need to zero the
													 LRU portion though */
		temp->Offset = off;

		/* Kick the tag onto the LRU */
		//LRUHit (&cache->LRU, (LRUNode_t *)temp, 0);
	}

	/* Insert at the head (if it's not already there) */
	if (cache->Hash[hash] != temp) {
		temp->Prev = NULL;
		temp->Next = cache->Hash[hash];
		if (temp->Next != NULL)
			temp->Next->Prev = temp;
		cache->Hash[hash] = temp;
	}

	/* Make sure there's a buffer */
	if (temp->Buffer == NULL) {
		/* Find a free buffer */
		temp->Buffer = CacheAllocBlock (cache);
		if (temp->Buffer == NULL) {
			/* Try to evict a buffer */
			error = LRUEvict (&cache->LRU, (LRUNode_t *)temp);
			if (error != EOK) return (error);

			/* Try again */
			temp->Buffer = CacheAllocBlock (cache);
			if (temp->Buffer == NULL) return (ENOMEM);
		}

		/* Load the block from disk */
		error = CacheRawRead (cache, off, cache->BlockSize, temp->Buffer);
		if (error != EOK) return (error);
	}

	*tag = temp;	
	return (EOK);
}

/*
 * CacheRawRead
 *
 *  Perform a direct read on the file.
 */
int CacheRawRead (Cache_t *cache, uint64_t off, uint32_t len, void *buf)
{
	uint64_t	result;
		
	/* Both offset and length must be multiples of the device block size */
	if (off % cache->DevBlockSize) return (EINVAL);
	if (len % cache->DevBlockSize) return (EINVAL);
	
	/* Seek to the position */
	result = lseek (cache->FD_R, off, SEEK_SET);
	if (result < 0) return (errno);
	if (result != off) return (ENXIO);
	/* Read into the buffer */
	result = read (cache->FD_R, buf, len);
	if (result < 0) return (errno);
	if (result == 0) return (ENXIO);

	/* Update counters */
	cache->DiskRead++;
	
	return (EOK);
}

/*
 * CacheRawWrite
 *
 *  Perform a direct write on the file.
 */
int CacheRawWrite (Cache_t *cache, uint64_t off, uint32_t len, void *buf)
{
	uint64_t	result;
	
	/* Both offset and length must be multiples of the device block size */
	if (off % cache->DevBlockSize) return (EINVAL);
	if (len % cache->DevBlockSize) return (EINVAL);
	
	/* Seek to the position */
	result = lseek (cache->FD_W, off, SEEK_SET);
	if (result < 0) return (errno);
	if (result != off) return (ENXIO);
	
	/* Write into the buffer */
	result = write (cache->FD_W, buf, len);
	if (result < 0) return (errno);
	if (result == 0) return (ENXIO);
	
	/* Update counters */
	cache->DiskWrite++;
	
	return (EOK);
}



/*
 * LRUInit
 *
 *  Initializes the LRU data structures.
 */
static int LRUInit (LRU_t *lru)
{
	/* Make the dummy nodes point to themselves */
	lru->Head.Next = &lru->Head;
	lru->Head.Prev = &lru->Head;

	lru->Busy.Next = &lru->Busy;
	lru->Busy.Prev = &lru->Busy;

	return (EOK);
}

/*
 * LRUDestroy
 *
 *  Shutdown the LRU.
 *
 *  NOTE: This is typically a no-op, since the cache manager will be clearing
 *        all cache tags.
 */
static int LRUDestroy (LRU_t *lru)
{
	/* Nothing to do */
	return (EOK);
}

/*
 * LRUHit
 *
 *  Registers data activity on the given node. If the node is already in the
 *  LRU, it is moved to the front. Otherwise, it is inserted at the front.
 *
 *  NOTE: If the node is not in the LRU, we assume that its pointers are NULL.
 */
static int LRUHit (LRU_t *lru, LRUNode_t *node, int age)
{
	/* Handle existing nodes */
	if ((node->Next != NULL) && (node->Prev != NULL)) {
		/* Detach the node */
		node->Next->Prev = node->Prev;
		node->Prev->Next = node->Next;
	}

	/* If it's busy (we can't evict it) */
	if (((Tag_t *)node)->Refs) {
		/* Insert at the head of the Busy queue */
		node->Next = lru->Busy.Next;
		node->Prev = &lru->Busy;

	} else if (age) {
		/* Insert at the head of the LRU */
		node->Next = &lru->Head;
		node->Prev = lru->Head.Prev;
		
	} else {
		/* Insert at the head of the LRU */
		node->Next = lru->Head.Next;
		node->Prev = &lru->Head;
	}

	node->Next->Prev = node;
	node->Prev->Next = node;

	return (EOK);
}

/*
 * LRUEvict
 *
 *  Chooses a buffer to release.
 *
 *  TODO: Under extreme conditions, it shoud be possible to release the buffer
 *        of an actively referenced cache buffer, leaving the tag behind as a
 *        placeholder. This would be required for implementing 2Q-LRU
 *        replacement.
 *
 *  NOTE: Make sure we never evict the node we're trying to find a buffer for!
 */
static int LRUEvict (LRU_t *lru, LRUNode_t *node)
{
	LRUNode_t *	temp;

	/* Find a victim */
	while (1) {
		/* Grab the tail */
		temp = lru->Head.Prev;
		
		/* Stop if we're empty */
		if (temp == &lru->Head) return (ENOMEM);

		/* Detach the tail */
		temp->Next->Prev = temp->Prev;
		temp->Prev->Next = temp->Next;

		/* If it's not busy, we have a victim */
		if (!((Tag_t *)temp)->Refs) break;

		/* Insert at the head of the Busy queue */
		temp->Next = lru->Busy.Next;
		temp->Prev = &lru->Busy;
				
		temp->Next->Prev = temp;
		temp->Prev->Next = temp;

		/* Try again */
	}

	/* Remove the tag */
	CacheRemove ((Cache_t *)lru, (Tag_t *)temp);

	return (EOK);
}

