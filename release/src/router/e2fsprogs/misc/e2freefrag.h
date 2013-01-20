#include <sys/types.h>

#define DEFAULT_CHUNKSIZE (1024*1024)

#define MAX_HIST	32
struct free_chunk_histogram {
	unsigned long fc_chunks[MAX_HIST];
	unsigned long fc_blocks[MAX_HIST];
};

struct chunk_info {
	unsigned long chunkbytes;	/* chunk size in bytes */
	int chunkbits;			/* chunk size in bits */
	unsigned long free_chunks;	/* total free chunks of given size */
	unsigned long real_free_chunks; /* free chunks of any size */
	int blocksize_bits;		/* fs blocksize in bits */
	int blks_in_chunk;		/* number of blocks in a chunk */
	unsigned long min, max, avg;	/* chunk size stats */
	struct free_chunk_histogram histogram; /* histogram of all chunk sizes*/
};
