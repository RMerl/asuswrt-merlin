#if MEM_MAN
/* a simple memory manager. All allocates and frees should go through here */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MEM_MAN_MAIN

#include "mem_man.h"

#ifdef MEM_SIGNAL_HANDLER
#include <signal.h>
#endif

/*
   this module is stand alone. typically a define will occur in a C file
   like this
   
   #define malloc(x) smb_mem_malloc(x,__FILE__,__LINE__)
   #define free(x)   smb_mem_free(x,__FILE__,__LINE__)
   
   which redirects all calls to malloc and free through this module
   
   Various configuration options can be set in mem_man.h. This file also
   includes the defines above - so the complete system can be implemented
   with just one include call.
   
   
   */

extern FILE *dbf;

/*
   ACCESSING the memory manager :
   
   mem_init_memory_manager() :
   initialises internal data structures of memory manager
   
   void *malloc(size_t size) :
   allocates memory as per usual. also records lots of info
   
   int free(void *ptr) :
   frees some memory as per usual. writes errors if necessary.
   
   void *smb_mem_resize(void *ptr,size_t newsize) :
   changes the memory assignment size of a pointer. note it may return a
   different pointer than the one given. memory can be sized up or down.
   
   int smb_mem_query_size(void *ptr) :
   returns the size of the allocated memory.
   
   int smb_mem_query_real_size(void *ptr) :
   returns the actual amount of memory allocated to a pointer.
   
   char *smb_mem_query_file(void *ptr) :
   returns the name of the file where the pointer was allocated.
   
   int smb_mem_query_line(void *ptr) :
   returns the line of the file where the memory was allocated.
   
   void smb_mem_write_status(FILE *outfile) :
   writes short summary of memory stats on the stream.
   
   void smb_mem_write_verbose(FILE *outfile) :
   writes lots of info on current allocations to stream.
   
   void smb_mem_write_errors(FILE *outfile) :
   writes info on error blocks
   
   void smb_mem_write_info(void *ptr,FILE *outfile)
   writes info on one pointer to outfile
   
   int smb_mem_test(void *ptr) :
   returns true if the pointer is OK - false if it is not.
   
   void smb_mem_set_multiplier(int multiplier) :
   sets defaults amount of memory allocated to multiplier times
   amount requested.
   
   int smb_mem_total_errors(void) :
   returns the total number of error blocks
   
   void smb_mem_check_buffers(void) :
   checks all buffers for corruption. It marks them as corrupt if they are.
   
   kill -USR1 <pid> :
   this will send a signal to the memory manager to do a mem_write_verbose
   it also checks them for corruption. Note that the signal number can be
   set in the header file mem_man.h. This can also be turned off.
   
   */


void smb_mem_write_errors(FILE *outfile);
void smb_mem_write_verbose(FILE *outfile);
void smb_mem_write_status(FILE *outfile);
static void mem_check_buffers(void);


#define FREE_FAILURE 0
#define FREE_SUCCESS 1
#define FN
#define True (0==0)
#define False (!True)
#define BUF_SIZE        (MEM_CORRUPT_BUFFER * sizeof(char) * 2)
#define BUF_OFFSET      (BUF_SIZE/2)

typedef struct
{
  void *pointer;
  size_t present_size;
  size_t allocated_size;
  unsigned char status;
  short error_number;
  char file[MEM_FILE_STR_LENGTH];
  unsigned short line;
} memory_struct;

/* the order of this enum is important. everything greater than
   S_ALLOCATED is considered an error */
enum status_types {S_UNALLOCATED,S_ALLOCATED,
		     S_ERROR_UNALLOCATED,S_ERROR_FREEING,
		     S_CORRUPT_FRONT,S_CORRUPT_BACK,S_CORRUPT_FRONT_BACK};

/* here is the data memory */

static memory_struct *memory_blocks=NULL; /* these hold the allocation data */
static int mem_blocks_allocated=0; /* how many mem blocks are allocated */
static int mem_multiplier; /* this is the current multiplier mor over allocation */
static int mem_manager_initialised=False; /* has it been initialised ? */
static int last_block_allocated=0; /* a speed up method - this will contain the
			       index of the last block allocated or freed
			       to cut down searching time for a new block */


typedef struct
{
  int status;
  char *label;
} stat_str_type;

static stat_str_type stat_str_struct[] =
{
{S_UNALLOCATED,"S_UNALLOCATED"},
{S_ALLOCATED,"S_ALLOCATED"},
{S_ERROR_UNALLOCATED,"S_ERROR_UNALLOCATED"},
{S_ERROR_FREEING,"S_ERROR_FREEING"},
{S_CORRUPT_FRONT,"S_CORRUPT_FRONT"},
{S_CORRUPT_BACK,"S_CORRUPT_BACK"},
{S_CORRUPT_FRONT_BACK,"S_CORRUPT_FRONT_BACK"},
{-1,NULL}
};


#define INIT_MANAGER() if (!mem_manager_initialised) mem_init_memory_manager()

/*******************************************************************
  returns a pointer to a static string for each status
  ********************************************************************/
static char *status_to_str(int status)
{
  int i=0;
  while (stat_str_struct[i].label != NULL)
    {
      if (stat_str_struct[i].status == status)
	return(stat_str_struct[i].label);
      i++;
    }
  return(NULL);
}



#ifdef MEM_SIGNAL_HANDLER
/*******************************************************************
  this handles signals - causes a mem_write_verbose on stderr 
  ********************************************************************/
static void mem_signal_handler()
{
  mem_check_buffers();
  smb_mem_write_verbose(dbf);
  signal(MEM_SIGNAL_VECTOR,mem_signal_handler);
}
#endif

#ifdef MEM_SIGNAL_HANDLER
/*******************************************************************
  this handles error signals - causes a mem_write_verbose on stderr 
  ********************************************************************/
static void error_signal_handler()
{
  fprintf(dbf,"Received error signal!\n");
  mem_check_buffers();
  smb_mem_write_status(dbf);
  smb_mem_write_errors(dbf);
  abort();
}
#endif


/*******************************************************************
  initialise memory manager data structures
  ********************************************************************/
static void mem_init_memory_manager(void)
{
  int i;
  /* allocate the memory_blocks array */
  mem_blocks_allocated = MEM_MAX_MEM_OBJECTS;

  while (mem_blocks_allocated > 0)
    {
      memory_blocks = (memory_struct *)
	calloc(mem_blocks_allocated,sizeof(memory_struct));
      if (memory_blocks != NULL) break;
      mem_blocks_allocated /= 2;
    }

  if (memory_blocks == NULL)
    {
      fprintf(dbf,"Panic ! can't allocate mem manager blocks!\n");
      abort();
    }

  /* just loop setting status flag to unallocated */
  for (i=0;i<mem_blocks_allocated;i++)
    memory_blocks[i].status = S_UNALLOCATED;

  /* also set default mem multiplier */
  mem_multiplier = MEM_DEFAULT_MEM_MULTIPLIER;
  mem_manager_initialised=True;

#ifdef MEM_SIGNAL_HANDLER
  signal(MEM_SIGNAL_VECTOR,mem_signal_handler);
  signal(SIGSEGV,error_signal_handler);
  signal(SIGBUS,error_signal_handler);
#endif

}


/*******************************************************************
  finds first available slot in memory blocks 
  ********************************************************************/
static int mem_first_avail_slot(void)
{
  int i;
  for (i=last_block_allocated;i<mem_blocks_allocated;i++)
    if (memory_blocks[i].status == S_UNALLOCATED)
      return(last_block_allocated=i);
  for (i=0;i<last_block_allocated;i++)
    if (memory_blocks[i].status == S_UNALLOCATED)
      return(last_block_allocated=i);
  return(-1);
}


/*******************************************************************
  find which Index a pointer refers to 
  ********************************************************************/
static int mem_find_Index(void *ptr)
{
  int i;
  int start = last_block_allocated+mem_blocks_allocated/50;
  if (start > mem_blocks_allocated-1) start = mem_blocks_allocated-1;
  for (i=start;i>=0;i--)
    if ((memory_blocks[i].status == S_ALLOCATED) &&
	(memory_blocks[i].pointer == ptr))
      return(i);
  for (i=(start+1);i<mem_blocks_allocated;i++)
    if ((memory_blocks[i].status == S_ALLOCATED) &&
	(memory_blocks[i].pointer == ptr))
      return(i);
  /* it's not there! */
  return(-1);
}

/*******************************************************************
  fill the buffer areas of a mem block 
  ********************************************************************/
static void mem_fill_bytes(void *p,int size,int Index)
{
  memset(p,Index%256,size);
}

/*******************************************************************
  fill the buffer areas of a mem block 
  ********************************************************************/
static void mem_fill_buffer(int Index)
{
  char *iptr,*tailptr;
  int i;
  int seed;

  /* fill the front and back ends */
  seed = MEM_CORRUPT_SEED;
  iptr = (char *)((char *)memory_blocks[Index].pointer - BUF_OFFSET);
  tailptr = (char *)((char *)memory_blocks[Index].pointer +
		     memory_blocks[Index].present_size);

  for (i=0;i<MEM_CORRUPT_BUFFER;i++)
    {
      iptr[i] = seed;
      tailptr[i] = seed;
      seed += MEM_SEED_INCREMENT;
    }
}

/*******************************************************************
  check if a mem block is corrupt 
  ********************************************************************/
static int mem_buffer_ok(int Index)
{
  char *iptr;
  int i;
  int corrupt_front = False;
  int corrupt_back = False;
  
  /* check the front end */
  iptr = (char *)((char *)memory_blocks[Index].pointer - BUF_OFFSET);
  for (i=0;i<MEM_CORRUPT_BUFFER;i++)
    if (iptr[i] != (char)(MEM_CORRUPT_SEED + i*MEM_SEED_INCREMENT))
      corrupt_front = True;

  /* now check the tail end */
  iptr = (char *)((char *)memory_blocks[Index].pointer +
		  memory_blocks[Index].present_size);
  for (i=0;i<MEM_CORRUPT_BUFFER;i++)
    if (iptr[i] != (char)(MEM_CORRUPT_SEED + i*MEM_SEED_INCREMENT))
      corrupt_back = True;
  
  if (corrupt_front && !corrupt_back)
    memory_blocks[Index].status = S_CORRUPT_FRONT;
  if (corrupt_back && !corrupt_front)
    memory_blocks[Index].status = S_CORRUPT_BACK;
  if (corrupt_front && corrupt_back)
    memory_blocks[Index].status = S_CORRUPT_FRONT_BACK;
  if (!corrupt_front && !corrupt_back)
    return(True);
  return(False);
}


/*******************************************************************
  check all buffers for corruption 
  ********************************************************************/
static void mem_check_buffers(void)
{
  int i;
  for (i=0;i<mem_blocks_allocated;i++)
    if (memory_blocks[i].status == S_ALLOCATED)
      mem_buffer_ok(i);
}


/*******************************************************************
  record stats and alloc memory 
  ********************************************************************/
void *smb_mem_malloc(size_t size,char *file,int line)
{
  int Index;
  INIT_MANAGER();

  /* find an open spot */

  Index = mem_first_avail_slot();
  if (Index<0) return(NULL);

  /* record some info */
  memory_blocks[Index].present_size = size;
  memory_blocks[Index].allocated_size = size*mem_multiplier;
  memory_blocks[Index].line = line;
  strncpy(memory_blocks[Index].file,file,MEM_FILE_STR_LENGTH);
  memory_blocks[Index].file[MEM_FILE_STR_LENGTH-1] = 0;
  memory_blocks[Index].error_number = 0;

  /* now try and actually get the memory */
  memory_blocks[Index].pointer = malloc(size*mem_multiplier + BUF_SIZE);

  /* if that failed then try and get exactly what was actually requested */
  if (memory_blocks[Index].pointer == NULL)
    {
      memory_blocks[Index].allocated_size = size;
      memory_blocks[Index].pointer = malloc(size + BUF_SIZE);
    }

  /* if it failed then return NULL */
  if (memory_blocks[Index].pointer == NULL) return(NULL);


  /* it succeeded - set status flag and return */
  memory_blocks[Index].status = S_ALLOCATED;

  /* add an offset */
  memory_blocks[Index].pointer =
    (void *)((char *)memory_blocks[Index].pointer + BUF_OFFSET);

  /* fill the buffer appropriately */
  mem_fill_buffer(Index);

  /* and set the fill byte */
  mem_fill_bytes(memory_blocks[Index].pointer,memory_blocks[Index].present_size,Index);

  /* return the allocated memory */
  return(memory_blocks[Index].pointer);
}


/*******************************************************************
dup a string
  ********************************************************************/
char *smb_mem_strdup(char *s, char *file, int line)
{
	char *ret = (char *)smb_mem_malloc(strlen(s)+1, file, line);
	strcpy(ret, s);
	return ret;
}

/*******************************************************************
  free some memory 
  ********************************************************************/
int smb_mem_free(void *ptr,char *file,int line)
{
  int Index;
  int free_ret;
  static int count;
  INIT_MANAGER();

  if (count % 100 == 0) {
	  smb_mem_write_errors(dbf);
  }
  count++;

  Index = mem_find_Index(ptr);

  if (Index<0)			/* we are freeing a pointer that hasn't been allocated ! */
    {
      /* set up an error block */
      Index = mem_first_avail_slot();
      if (Index < 0)		/* I can't even allocate an Error! */
	{
	  fprintf(dbf,"Panic in memory manager - can't allocate error block!\n");
	  fprintf(dbf,"freeing un allocated pointer at %s(%d)\n",file,line);
	  abort();
	}
      /* fill in error block */
      memory_blocks[Index].present_size = 0;
      memory_blocks[Index].allocated_size = 0;
      memory_blocks[Index].line = line;
      strncpy(memory_blocks[Index].file,file,MEM_FILE_STR_LENGTH);
      memory_blocks[Index].file[MEM_FILE_STR_LENGTH-1] = 0;
      memory_blocks[Index].status = S_ERROR_UNALLOCATED;
      memory_blocks[Index].pointer = ptr;
      return(FREE_FAILURE);
    }

  /* it is a valid pointer - check for corruption */
  if (!mem_buffer_ok(Index))
    /* it's bad ! return an error */
    return(FREE_FAILURE);

  /* the pointer is OK - try to free it */
#ifdef MEM_FREE_RETURNS_INT
  free_ret = free((char *)ptr - BUF_OFFSET);
#else
  free((char *)ptr - BUF_OFFSET);
  free_ret = FREE_SUCCESS;
#endif


  /* if this failed then make an error block again */
  if (free_ret == FREE_FAILURE)
    {
      memory_blocks[Index].present_size = 0;
      memory_blocks[Index].allocated_size = 0;
      memory_blocks[Index].line = line;
      strncpy(memory_blocks[Index].file,file,MEM_FILE_STR_LENGTH);
      memory_blocks[Index].file[MEM_FILE_STR_LENGTH-1] = 0;
      memory_blocks[Index].status = S_ERROR_FREEING;
      memory_blocks[Index].pointer = ptr;
      memory_blocks[Index].error_number = errno;
      return(FREE_FAILURE);
    }

  /* all is OK - set status and return */
  memory_blocks[Index].status = S_UNALLOCATED;

  /* this is a speedup - if it is freed then it can be allocated again ! */
  last_block_allocated = Index;

  return(FREE_SUCCESS);
}



/*******************************************************************
  writes info on just one Index
  it must not be un allocated to do this 
  ********************************************************************/
static void mem_write_Index_info(int Index,FILE *outfile)
{
  if (memory_blocks[Index].status != S_UNALLOCATED)
    fprintf(outfile,"block %d file %s(%d) : size %d, alloc size %d, status %s\n",
	    Index,memory_blocks[Index].file,memory_blocks[Index].line,
	    memory_blocks[Index].present_size,
	    memory_blocks[Index].allocated_size,
	    status_to_str(memory_blocks[Index].status));
}



/*******************************************************************
  writes info on one pointer  
  ********************************************************************/
void smb_mem_write_info(void *ptr,FILE *outfile)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return;
  mem_write_Index_info(Index,outfile);
}




/*******************************************************************
  return the size of the mem block 
  ********************************************************************/
size_t smb_mem_query_size(void *ptr)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return(0);
  return(memory_blocks[Index].present_size);
}

/*******************************************************************
  return the allocated size of the mem block 
  ********************************************************************/
size_t smb_mem_query_real_size(void *ptr)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return(0);
  return(memory_blocks[Index].allocated_size);
}




/*******************************************************************
  return the file of caller of the mem block 
  ********************************************************************/
char *smb_mem_query_file(void *ptr)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return(NULL);
  return(memory_blocks[Index].file);
}



/*******************************************************************
  return the line in the file of caller of the mem block 
  ********************************************************************/
int smb_mem_query_line(void *ptr)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return(0);
  return(memory_blocks[Index].line);
}

/*******************************************************************
  return True if the pointer is OK
  ********************************************************************/
int smb_mem_test(void *ptr)
{
  int Index;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);
  if (Index<0) return(False);

  return(mem_buffer_ok(Index));
}


/*******************************************************************
  write brief info on mem status 
  ********************************************************************/
void smb_mem_write_status(FILE *outfile)
{
  int num_allocated=0;
  int total_size=0;
  int total_alloc_size=0;
  int num_errors=0;
  int i;
  INIT_MANAGER();
  mem_check_buffers();
  for (i=0;i<mem_blocks_allocated;i++)
    switch (memory_blocks[i].status)
      {
      case S_UNALLOCATED :
	break;
      case S_ALLOCATED :
	num_allocated++;
	total_size += memory_blocks[i].present_size;
	total_alloc_size += memory_blocks[i].allocated_size;
	break;
      case S_ERROR_UNALLOCATED :
      case S_ERROR_FREEING :
      case S_CORRUPT_BACK :
      case S_CORRUPT_FRONT :
	num_errors++;
	break;
      }
  
  fprintf(outfile,
	  "Mem Manager : %d blocks, allocation %dK, real allocation %dK, %d errors\n",
	  num_allocated,(int)(total_size/1024),(int)(total_alloc_size/1024),
	  num_errors);
  fflush(outfile);
}


/*******************************************************************
  write verbose info on allocated blocks 
  ********************************************************************/
void smb_mem_write_verbose(FILE *outfile)
{
  int Index;
  /* first write a summary */
  INIT_MANAGER();
  smb_mem_write_status(outfile);
  
  /* just loop writing info on relevant indices */
  for (Index=0;Index<mem_blocks_allocated;Index++)
    if (memory_blocks[Index].status != S_UNALLOCATED)
      mem_write_Index_info(Index,outfile);
}

/*******************************************************************
  write verbose info on error blocks 
  ********************************************************************/
void smb_mem_write_errors(FILE *outfile)
{
  int Index;
  INIT_MANAGER();
  mem_check_buffers();
  /* just loop writing info on relevant indices */
  for (Index=0;Index<mem_blocks_allocated;Index++)
    if (((int)memory_blocks[Index].status) > ((int)S_ALLOCATED))
      mem_write_Index_info(Index,outfile);
}


/*******************************************************************
  sets the memory multiplier 
  ********************************************************************/
void smb_mem_set_multiplier(int multiplier)
{
  /* check it is valid */
  if (multiplier < 1) return;
  mem_multiplier = multiplier;
}

/*******************************************************************
  increases or decreases the memory assigned to a pointer 
  ********************************************************************/
void *smb_mem_resize(void *ptr,size_t newsize)
{
  int Index;
  size_t allocsize;
  void *temp_ptr;
  INIT_MANAGER();
  Index = mem_find_Index(ptr);

  /* if invalid return NULL */
  if (Index<0) 
    {
#ifdef BUG
      int Error();
      Error("Invalid mem_resize to size %d\n",newsize);
#endif
      return(NULL);
    }
  
  /* now - will it fit in the current allocation ? */
  if (newsize <= memory_blocks[Index].allocated_size)
    {
      memory_blocks[Index].present_size = newsize;
      mem_fill_buffer(Index);
      return(ptr);
    }

  /* can it be allocated ? */
  allocsize = newsize*mem_multiplier;
  temp_ptr = malloc(newsize*mem_multiplier + BUF_SIZE);
  
  /* no? try with just the size asked for */
  if (temp_ptr == NULL)
    {
      allocsize=newsize;
      temp_ptr = malloc(newsize + BUF_SIZE);
    }
  
  /* if it's still NULL give up */
  if (temp_ptr == NULL)
    return(NULL);
  
  /* copy the old data to the new memory area */
  memcpy(temp_ptr,(char *)memory_blocks[Index].pointer - BUF_OFFSET,
	 memory_blocks[Index].allocated_size + BUF_SIZE);
  
  /* fill the extra space */
  mem_fill_bytes((char *)temp_ptr + BUF_OFFSET + memory_blocks[Index].present_size,newsize - memory_blocks[Index].present_size,Index);
  
  
  /* free the old mem and set vars */
  free((char *)ptr - BUF_OFFSET);
  memory_blocks[Index].pointer = (void *)((char *)temp_ptr + BUF_OFFSET);
  memory_blocks[Index].present_size = newsize;
  memory_blocks[Index].allocated_size = allocsize;
  
  /* fill the buffer appropriately */
  mem_fill_buffer(Index);
  
  
  /* now return the new pointer */
  return((char *)temp_ptr + BUF_OFFSET);
}

#else
 void dummy_mem_man(void) {} 
#endif
