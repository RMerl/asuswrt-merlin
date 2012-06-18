/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Shared memory functions - SYSV IPC implementation
   Copyright (C) Andrew Tridgell 1997-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "includes.h"


#ifdef USE_SYSV_IPC

extern int DEBUGLEVEL;

#define SHMEM_KEY ((key_t)0x280267)
#define SEMAPHORE_KEY (SHMEM_KEY+2)

#define SHM_MAGIC 0x53484100
#define SHM_VERSION 2

#ifdef SHM_R
#define IPC_PERMS ((SHM_R | SHM_W) | (SHM_R>>3) | (SHM_R>>6))
#else
#define IPC_PERMS 0644
#endif


#ifdef SECURE_SEMAPHORES
/* secure semaphores are slow because we have to do a become_root()
   on every call! */
#define SEMAPHORE_PERMS IPC_PERMS
#else
#define SEMAPHORE_PERMS 0666
#endif

#define SHMEM_HASH_SIZE 13

#define MIN_SHM_SIZE 0x1000

static int shm_id;
static int sem_id;
static int shm_size;
static int hash_size;
static int global_lock_count;

struct ShmHeader {
   int shm_magic;
   int shm_version;
   int total_size;	/* in bytes */
   BOOL consistent;
   int first_free_off;
   int userdef_off;    /* a userdefined offset. can be used to store
			  root of tree or list */
   struct {		/* a cell is a range of bytes of sizeof(struct
			   ShmBlockDesc) size */
	   int cells_free;
	   int cells_used;
	   int cells_system; /* number of cells used as allocated
				block descriptors */
   } statistics;
};

#define SHM_NOT_FREE_OFF (-1)
struct ShmBlockDesc
{
   int next;	/* offset of next block in the free list or
		   SHM_NOT_FREE_OFF when block in use */
   int size;   /* user size in BlockDescSize units */
};

#define	EOList_Addr	NULL
#define EOList_Off      (0)

#define	CellSize	sizeof(struct ShmBlockDesc)

/* HeaderSize aligned on a 8 byte boundary */
#define	AlignedHeaderSize ((sizeof(struct ShmHeader)+7) & ~7)

static struct ShmHeader *shm_header_p = NULL;

static int read_only;

static BOOL sem_change(int i, int op)
{
#ifdef SECURE_SEMAPHORES
	extern struct current_user current_user;
	int became_root=0;
#endif
	struct sembuf sb;
	int ret;

	if (read_only) return True;

#ifdef SECURE_SEMAPHORES
	if (current_user.uid != 0) {
		become_root(0);
		became_root = 1;
	}
#endif

	sb.sem_num = i;
	sb.sem_op = op;
	sb.sem_flg = 0;

	ret = semop(sem_id, &sb, 1);

	if (ret != 0) {
		DEBUG(0,("ERROR: sem_change(%d,%d) failed (%s)\n", 
			 i, op, strerror(errno)));
	}

#ifdef SECURE_SEMAPHORES
	if (became_root) {
		unbecome_root(0);
	}
#endif

	return ret == 0;
}

static BOOL global_lock(void)
{
	global_lock_count++;
	if (global_lock_count == 1)
		return sem_change(0, -1);
	return True;
}

static BOOL global_unlock(void)
{
	global_lock_count--;
	if (global_lock_count == 0)
		return sem_change(0, 1);
	return True;
}

static void *shm_offset2addr(int offset)
{
   if (offset == 0 )
      return (void *)(0);
   
   if (!shm_header_p)
      return (void *)(0);
   
   return (void *)((char *)shm_header_p + offset);
}

static int shm_addr2offset(void *addr)
{
   if (!addr)
      return 0;
   
   if (!shm_header_p)
      return 0;
   
   return (int)((char *)addr - (char *)shm_header_p);
}


static int shm_alloc(int size)
{
	unsigned num_cells ;
	struct ShmBlockDesc *scanner_p;
	struct ShmBlockDesc *prev_p;
	struct ShmBlockDesc *new_p;
	int result_offset;
   
   
	if (!shm_header_p) {
		/* not mapped yet */
		DEBUG(0,("ERROR shm_alloc : shmem not mapped\n"));
		return 0;
	}
	
	global_lock();
	
	if (!shm_header_p->consistent) {
		DEBUG(0,("ERROR shm_alloc : shmem not consistent\n"));
		global_unlock();
		return 0;
	}
	
	/* calculate the number of cells */
	num_cells = (size + (CellSize-1)) / CellSize;
	
	/* set start of scan */
	prev_p = (struct ShmBlockDesc *)shm_offset2addr(shm_header_p->first_free_off);
	scanner_p =	prev_p ;
	
	/* scan the free list to find a matching free space */
	while ((scanner_p != EOList_Addr) && (scanner_p->size < num_cells)) {
		prev_p = scanner_p;
		scanner_p = (struct ShmBlockDesc *)shm_offset2addr(scanner_p->next);
	}
   
	/* at this point scanner point to a block header or to the end of
	   the list */
	if (scanner_p == EOList_Addr) {
		DEBUG(0,("ERROR shm_alloc : alloc of %d bytes failed\n",size));
		global_unlock();
		return (0);
	}
   
	/* going to modify shared mem */
	shm_header_p->consistent = False;
	
	/* if we found a good one : scanner == the good one */
	if (scanner_p->size > num_cells + 2) {
		/* Make a new one */
		new_p = scanner_p + 1 + num_cells;
		new_p->size = scanner_p->size - (num_cells + 1);
		new_p->next = scanner_p->next;
		scanner_p->size = num_cells;
		scanner_p->next = shm_addr2offset(new_p);

		shm_header_p->statistics.cells_free -= 1;
		shm_header_p->statistics.cells_system += 1;
	}

	/* take it from the free list */
	if (prev_p == scanner_p) {
		shm_header_p->first_free_off = scanner_p->next;
	} else {
		prev_p->next = scanner_p->next;
	}
	shm_header_p->statistics.cells_free -= scanner_p->size;
	shm_header_p->statistics.cells_used += scanner_p->size;

	result_offset = shm_addr2offset(&(scanner_p[1]));
	scanner_p->next = SHM_NOT_FREE_OFF;

	/* end modification of shared mem */
	shm_header_p->consistent = True;

	global_unlock();
	
	DEBUG(6,("shm_alloc : allocated %d bytes at offset %d\n",
		 size,result_offset));

	return result_offset;
}   

static void shm_solve_neighbors(struct ShmBlockDesc *head_p )
{
	struct ShmBlockDesc *next_p;
   
	/* Check if head_p and head_p->next are neighbors and if so
           join them */
	if ( head_p == EOList_Addr ) return ;
	if ( head_p->next == EOList_Off ) return ;
   
	next_p = (struct ShmBlockDesc *)shm_offset2addr(head_p->next);
	if ((head_p + head_p->size + 1) == next_p) {
		head_p->size += next_p->size + 1; /* adapt size */
		head_p->next = next_p->next; /* link out */
      
		shm_header_p->statistics.cells_free += 1;
		shm_header_p->statistics.cells_system -= 1;
	}
}


static BOOL shm_free(int offset)
{
	struct ShmBlockDesc *header_p; /* pointer to header of
					  block to free */
	struct ShmBlockDesc *scanner_p; /* used to scan the list */
	struct ShmBlockDesc *prev_p; /* holds previous in the
					list */
   
	if (!shm_header_p) {
		/* not mapped yet */
		DEBUG(0,("ERROR shm_free : shmem not mapped\n"));
		return False;
	}
	
	global_lock();
	
	if (!shm_header_p->consistent) {
		DEBUG(0,("ERROR shm_free : shmem not consistent\n"));
		global_unlock();
		return False;
	}
	
	/* make pointer to header of block */
	header_p = ((struct ShmBlockDesc *)shm_offset2addr(offset) - 1); 
	
	if (header_p->next != SHM_NOT_FREE_OFF) {
		DEBUG(0,("ERROR shm_free : bad offset (%d)\n",offset));
		global_unlock();
		return False;
	}
	
	/* find a place in the free_list to put the header in */
	
	/* set scanner and previous pointer to start of list */
	prev_p = (struct ShmBlockDesc *)
		shm_offset2addr(shm_header_p->first_free_off);
	scanner_p = prev_p ;
	
	while ((scanner_p != EOList_Addr) && 
	       (scanner_p < header_p)) { 
		/* while we didn't scan past its position */
		prev_p = scanner_p ;
		scanner_p = (struct ShmBlockDesc *)
			shm_offset2addr(scanner_p->next);
	}
	
	shm_header_p->consistent = False;
	
	DEBUG(6,("shm_free : freeing %d bytes at offset %d\n",
		 (int)(header_p->size*CellSize),(int)offset));

	/* zero the area being freed - this allows us to find bugs faster */
	memset(shm_offset2addr(offset), 0, header_p->size*CellSize);
	
	if (scanner_p == prev_p) {
		shm_header_p->statistics.cells_free += header_p->size;
		shm_header_p->statistics.cells_used -= header_p->size;
		
		/* we must free it at the beginning of the list */
		shm_header_p->first_free_off = shm_addr2offset(header_p);
		/* set the free_list_pointer to this block_header */
		
		/* scanner is the one that was first in the list */
		header_p->next = shm_addr2offset(scanner_p);
		shm_solve_neighbors(header_p); 
		
		shm_header_p->consistent = True;
	} else {
		shm_header_p->statistics.cells_free += header_p->size;
		shm_header_p->statistics.cells_used -= header_p->size;
		
		prev_p->next = shm_addr2offset(header_p);
		header_p->next = shm_addr2offset(scanner_p);
		shm_solve_neighbors(header_p) ;
		shm_solve_neighbors(prev_p) ;
	   
		shm_header_p->consistent = True;
	}

	global_unlock();
	return True;
}


/* 
 * Function to create the hash table for the share mode entries. Called
 * when smb shared memory is global locked.
 */
static BOOL shm_create_hash_table(unsigned int hash_entries)
{
	int size = hash_entries * sizeof(int);

	global_lock();
	shm_header_p->userdef_off = shm_alloc(size);

	if(shm_header_p->userdef_off == 0) {
		DEBUG(0,("shm_create_hash_table: Failed to create hash table of size %d\n",
			 size));
		global_unlock();
		return False;
	}

	/* Clear hash buckets. */
	memset(shm_offset2addr(shm_header_p->userdef_off), '\0', size);
	global_unlock();
	return True;
}


static BOOL shm_validate_header(int size)
{
	if(!shm_header_p) {
		/* not mapped yet */
		DEBUG(0,("ERROR shm_validate_header : shmem not mapped\n"));
		return False;
	}
   
	if(shm_header_p->shm_magic != SHM_MAGIC) {
		DEBUG(0,("ERROR shm_validate_header : bad magic\n"));
		return False;
	}

	if(shm_header_p->shm_version != SHM_VERSION) {
		DEBUG(0,("ERROR shm_validate_header : bad version %X\n",
			 shm_header_p->shm_version));
		return False;
	}
   
	if(shm_header_p->total_size != size) {
		DEBUG(0,("ERROR shmem size mismatch (old = %d, new = %d)\n",
			 shm_header_p->total_size,size));
		return False;
	}

	if(!shm_header_p->consistent) {
		DEBUG(0,("ERROR shmem not consistent\n"));
		return False;
	}
	return True;
}


static BOOL shm_initialize(int size)
{
	struct ShmBlockDesc * first_free_block_p;
	
	DEBUG(5,("shm_initialize : initializing shmem size %d\n",size));
   
	if( !shm_header_p ) {
		/* not mapped yet */
		DEBUG(0,("ERROR shm_initialize : shmem not mapped\n"));
		return False;
	}
   
	shm_header_p->shm_magic = SHM_MAGIC;
	shm_header_p->shm_version = SHM_VERSION;
	shm_header_p->total_size = size;
	shm_header_p->first_free_off = AlignedHeaderSize;
	shm_header_p->userdef_off = 0;
	
	first_free_block_p = (struct ShmBlockDesc *)
		shm_offset2addr(shm_header_p->first_free_off);
	first_free_block_p->next = EOList_Off;
	first_free_block_p->size = 
		(size - (AlignedHeaderSize+CellSize))/CellSize;   
	shm_header_p->statistics.cells_free = first_free_block_p->size;
	shm_header_p->statistics.cells_used = 0;
	shm_header_p->statistics.cells_system = 1;
   
	shm_header_p->consistent = True;
   
	return True;
}
   
static BOOL shm_close( void )
{
	return True;
}


static int shm_get_userdef_off(void)
{
   if (!shm_header_p)
      return 0;
   else
      return shm_header_p->userdef_off;
}


/*******************************************************************
  Lock a particular hash bucket entry.
  ******************************************************************/
static BOOL shm_lock_hash_entry(unsigned int entry)
{
	return sem_change(entry+1, -1);
}

/*******************************************************************
  Unlock a particular hash bucket entry.
  ******************************************************************/
static BOOL shm_unlock_hash_entry(unsigned int entry)
{
	return sem_change(entry+1, 1);
}


/*******************************************************************
  Gather statistics on shared memory usage.
  ******************************************************************/
static BOOL shm_get_usage(int *bytes_free,
			  int *bytes_used,
			  int *bytes_overhead)
{
	if(!shm_header_p) {
		/* not mapped yet */
		DEBUG(0,("ERROR shm_free : shmem not mapped\n"));
		return False;
	}

	*bytes_free = shm_header_p->statistics.cells_free * CellSize;
	*bytes_used = shm_header_p->statistics.cells_used * CellSize;
	*bytes_overhead = shm_header_p->statistics.cells_system * CellSize + 
		AlignedHeaderSize;
	
	return True;
}


/*******************************************************************
hash a number into a hash_entry
  ******************************************************************/
static unsigned shm_hash_size(void)
{
	return hash_size;
}


static struct shmem_ops shmops = {
	shm_close,
	shm_alloc,
	shm_free,
	shm_get_userdef_off,
	shm_offset2addr,
	shm_addr2offset,
	shm_lock_hash_entry,
	shm_unlock_hash_entry,
	shm_get_usage,
	shm_hash_size,
};

/*******************************************************************
  open the shared memory
  ******************************************************************/

struct shmem_ops *sysv_shm_open(int ronly)
{
	BOOL other_processes;
	struct shmid_ds shm_ds;
	struct semid_ds sem_ds;
	union semun su;
	int i;
	pid_t pid;
	struct passwd *root_pwd = sys_getpwuid((uid_t)0);
	gid_t root_gid = root_pwd ? root_pwd->pw_gid : (gid_t)0;

	read_only = ronly;

	shm_size = lp_shmem_size();

	DEBUG(4,("Trying sysv shmem open of size %d\n", shm_size));

	/* first the semaphore */
	sem_id = semget(SEMAPHORE_KEY, 0, 0);
	if (sem_id == -1) {
		if (read_only) return NULL;

		hash_size = SHMEM_HASH_SIZE;

		while (hash_size > 1) {
			sem_id = semget(SEMAPHORE_KEY, hash_size+1, 
					IPC_CREAT|IPC_EXCL| SEMAPHORE_PERMS);
			if (sem_id != -1 || 
			    (errno != EINVAL && errno != ENOSPC)) break;
			hash_size -= 5;
		}

		if (sem_id == -1) {
			DEBUG(0,("Can't create or use semaphore [1]. Error was %s\n", 
				 strerror(errno)));
            return NULL;
		}   

		if (sem_id != -1) {
			su.val = 1;
			for (i=0;i<hash_size+1;i++) {
				if (semctl(sem_id, i, SETVAL, su) != 0) {
					DEBUG(1,("Failed to init semaphore %d. Error was %s\n",
                          i, strerror(errno)));
                    return NULL;
				}
			}
		}
	}
	if (shm_id == -1) {
		sem_id = semget(SEMAPHORE_KEY, 0, 0);
	}
	if (sem_id == -1) {
		DEBUG(0,("Can't create or use semaphore [2]. Error was %s\n", 
			 strerror(errno)));
		return NULL;
	}   

	su.buf = &sem_ds;
	if (semctl(sem_id, 0, IPC_STAT, su) != 0) {
		DEBUG(0,("ERROR semctl: can't IPC_STAT. Error was %s\n",
              strerror(errno)));
		return NULL;
	}
	hash_size = sem_ds.sem_nsems-1;

	if (!read_only) {
		if (sem_ds.sem_perm.cuid != 0 || ((sem_ds.sem_perm.cgid != root_gid) && (sem_ds.sem_perm.cgid != 0))) {
			DEBUG(0,("ERROR: root did not create the semaphore: semgid=%u, rootgid=%u\n",
                  (unsigned int)sem_ds.sem_perm.cgid, (unsigned int)root_gid));
			return NULL;
		}

		if (semctl(sem_id, 0, GETVAL, su) == 0 &&
		    !process_exists((pid=(pid_t)semctl(sem_id, 0, GETPID, su)))) {
			DEBUG(0,("WARNING: clearing global IPC lock set by dead process %d\n",
				 (int)pid));
			su.val = 1;
			if (semctl(sem_id, 0, SETVAL, su) != 0) {
				DEBUG(0,("ERROR: Failed to clear global lock. Error was %s\n",
                      strerror(errno)));
                return NULL;
			}
		}

		sem_ds.sem_perm.mode = SEMAPHORE_PERMS;
		if (semctl(sem_id, 0, IPC_SET, su) != 0) {
			DEBUG(0,("ERROR shmctl : can't IPC_SET. Error was %s\n",
                  strerror(errno)));
            return NULL;
		}
	}

	if (!global_lock())
		return NULL;


	for (i=1;i<hash_size+1;i++) {
		if (semctl(sem_id, i, GETVAL, su) == 0 && 
		    !process_exists((pid=(pid_t)semctl(sem_id, i, GETPID, su)))) {
			DEBUG(1,("WARNING: clearing IPC lock %d set by dead process %d\n", 
				 i, (int)pid));
			su.val = 1;
			if (semctl(sem_id, i, SETVAL, su) != 0) {
				DEBUG(0,("ERROR: Failed to clear IPC lock %d. Error was %s\n",
                      i, strerror(errno)));
		        global_unlock();
                return NULL;
			}
		}
	}
	
	/* 
     * Try to use an existing key. Note that
     * in order to use an existing key successfully
     * size must be zero else shmget returns EINVAL.
     * Thanks to Veselin Terzic <vterzic@systems.DHL.COM>
     * for pointing this out.
     */

	shm_id = shmget(SHMEM_KEY, 0, 0);
	
	/* if that failed then create one */
	if (shm_id == -1) {
		if (read_only) return NULL;
		while (shm_size > MIN_SHM_SIZE) {
			shm_id = shmget(SHMEM_KEY, shm_size, 
					IPC_CREAT | IPC_EXCL | IPC_PERMS);
			if (shm_id != -1 || 
			    (errno != EINVAL && errno != ENOSPC)) break;
			shm_size *= 0.8;
		}
	}
	
	if (shm_id == -1) {
		DEBUG(0,("Can't create or use IPC area. Error was %s\n", strerror(errno)));
		global_unlock();
		return NULL;
	}   
	
	
	shm_header_p = (struct ShmHeader *)shmat(shm_id, 0, 
						 read_only?SHM_RDONLY:0);
	if ((long)shm_header_p == -1) {
		DEBUG(0,("Can't attach to IPC area. Error was %s\n", strerror(errno)));
		global_unlock();
		return NULL;
	}

	/*
	 * Get information on what process created the shared memory segment.
	 */

	if (shmctl(shm_id, IPC_STAT, &shm_ds) != 0) {
		DEBUG(0,("ERROR shmctl : can't IPC_STAT. Error was %s\n", strerror(errno)));
        global_unlock();
        return NULL;
	}

	if (!read_only) {
		if (shm_ds.shm_perm.cuid != 0 || ((shm_ds.shm_perm.cgid != root_gid) && (shm_ds.shm_perm.cgid != 0))) {
			DEBUG(0,("ERROR: root did not create the shmem\n"));
			global_unlock();
			return NULL;
		}
	}

	shm_size = shm_ds.shm_segsz;

	other_processes = (shm_ds.shm_nattch > 1);

	if (!read_only && !other_processes) {
		memset((char *)shm_header_p, 0, shm_size);
		shm_initialize(shm_size);
		shm_create_hash_table(hash_size);
		DEBUG(3,("Initialised IPC area of size %d\n", shm_size));
	} else if (!shm_validate_header(shm_size)) {
		/* existing file is corrupt, samba admin should remove
                   it by hand */
		DEBUG(0,("ERROR shm_open : corrupt IPC area - remove it!\n"));
		global_unlock();
		return NULL;
	}
   
	global_unlock();
	return &shmops;
}



#else 
 int ipc_dummy_procedure(void)
{return 0;}
#endif 
