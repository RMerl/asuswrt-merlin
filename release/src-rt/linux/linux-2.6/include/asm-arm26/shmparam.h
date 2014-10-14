#ifndef _ASMARM_SHMPARAM_H
#define _ASMARM_SHMPARAM_H

#ifndef SHMMAX
#define SHMMAX          0x003fa000
#endif

/*
 * This should be the size of the virtually indexed cache/ways,
 * or page size, whichever is greater since the cache aliases
 * every size/ways bytes.
 */
#define	SHMLBA PAGE_SIZE		 /* attach addr a multiple of this */

#endif /* _ASMARM_SHMPARAM_H */
