#if  (defined(NOMEMMAN) && !defined(MEM_MAN_MAIN))
#include <malloc.h>
#else

/* user settable parameters */

#define MEM_MANAGER

/* 
this is the maximum number of blocks that can be allocated at one
time. Set this to more than the max number of mallocs you want possible
*/
#define MEM_MAX_MEM_OBJECTS 20000

/* 
maximum length of a file name. This is for the source files only. This
would normally be around 30 - increase it only if necessary
*/
#define MEM_FILE_STR_LENGTH 30

/* 
default mem multiplier. All memory requests will be multiplied by
this number, thus allowing fast resizing. High values chew lots of
memory but allow for easy resizing 
*/
#define MEM_DEFAULT_MEM_MULTIPLIER 1

/*
the length of the corruption buffers before and after each memory block.
Using these can reduce memory overrun errors and catch bad code. The
amount actually allocated is this times 2 times sizeof(char)
*/
#define MEM_CORRUPT_BUFFER      16

/*
the 'seed' to use for the corruption buffer. zero is a very bad choice
*/
#define MEM_CORRUPT_SEED        0x10


/* 
seed increment. This is another precaution. This will be added to the
'seed' for each adjacent element of the corruption buffer 
*/
#define MEM_SEED_INCREMENT      0x1


/*
memory fill. This will be copied over all allocated memory - to aid in
debugging memory dumps. It is one byte long
*/
#define MEM_FILL_BYTE   42


/*
this determines whether free() on your system returns an integer or
nothing. If unsure then leave this un defined.
*/
/* #define MEM_FREE_RETURNS_INT */


/*
  This determines whether a signal handler will be instelled to do
  a mem_write_verbose on request
*/
#define MEM_SIGNAL_HANDLER


/*
This sets which vector to use if MEM_SIGNAL_HANDLER is defined
*/
#define MEM_SIGNAL_VECTOR SIGUSR1

#ifndef MEM_MAN_MAIN
#ifdef malloc
#  undef malloc
#endif
#define malloc(x) smb_mem_malloc(x,__FILE__,__LINE__)
#define free(x)   smb_mem_free(x,__FILE__,__LINE__)
#define realloc(ptr,newsize) smb_mem_resize(ptr,newsize)
#ifdef calloc
#  undef calloc
#endif
#define calloc(nitems,size) malloc(((_mem_size)nitems)*((_mem_size)size))

#ifdef strdup
#  undef strdup
#endif
#define strdup(s) smb_mem_strdup(s, __FILE__, __LINE__)
#endif

#endif
