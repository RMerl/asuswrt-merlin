/* this tests tdb by doing lots of ops from several simultaneous
   writers - that stresses the locking code. 
*/

#include "replace.h"
#include "tdb.h"
#include "system/time.h"
#include "system/wait.h"
#include "system/filesys.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif


#define REOPEN_PROB 30
#define DELETE_PROB 8
#define STORE_PROB 4
#define APPEND_PROB 6
#define TRANSACTION_PROB 10
#define LOCKSTORE_PROB 5
#define TRAVERSE_PROB 20
#define TRAVERSE_READ_PROB 20
#define CULL_PROB 100
#define KEYLEN 3
#define DATALEN 100

static struct tdb_context *db;
static int in_transaction;
static int error_count;

#ifdef PRINTF_ATTRIBUTE
static void tdb_log(struct tdb_context *tdb, enum tdb_debug_level level, const char *format, ...) PRINTF_ATTRIBUTE(3,4);
#endif
static void tdb_log(struct tdb_context *tdb, enum tdb_debug_level level, const char *format, ...)
{
	va_list ap;
    
	error_count++;

	va_start(ap, format);
	vfprintf(stdout, format, ap);
	va_end(ap);
	fflush(stdout);
#if 0
	{
		char *ptr;
		asprintf(&ptr,"xterm -e gdb /proc/%d/exe %d", getpid(), getpid());
		system(ptr);
		free(ptr);
	}
#endif	
}

static void fatal(const char *why)
{
	perror(why);
	error_count++;
}

static char *randbuf(int len)
{
	char *buf;
	int i;
	buf = (char *)malloc(len+1);

	for (i=0;i<len;i++) {
		buf[i] = 'a' + (rand() % 26);
	}
	buf[i] = 0;
	return buf;
}

static int cull_traverse(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
			 void *state)
{
#if CULL_PROB
	if (random() % CULL_PROB == 0) {
		tdb_delete(tdb, key);
	}
#endif
	return 0;
}

static void addrec_db(void)
{
	int klen, dlen;
	char *k, *d;
	TDB_DATA key, data;

	klen = 1 + (rand() % KEYLEN);
	dlen = 1 + (rand() % DATALEN);

	k = randbuf(klen);
	d = randbuf(dlen);

	key.dptr = (unsigned char *)k;
	key.dsize = klen+1;

	data.dptr = (unsigned char *)d;
	data.dsize = dlen+1;

#if TRANSACTION_PROB
	if (in_transaction == 0 && random() % TRANSACTION_PROB == 0) {
		if (tdb_transaction_start(db) != 0) {
			fatal("tdb_transaction_start failed");
		}
		in_transaction++;
		goto next;
	}
	if (in_transaction && random() % TRANSACTION_PROB == 0) {
		if (tdb_transaction_commit(db) != 0) {
			fatal("tdb_transaction_commit failed");
		}
		in_transaction--;
		goto next;
	}
	if (in_transaction && random() % TRANSACTION_PROB == 0) {
		if (tdb_transaction_cancel(db) != 0) {
			fatal("tdb_transaction_cancel failed");
		}
		in_transaction--;
		goto next;
	}
#endif

#if REOPEN_PROB
	if (in_transaction == 0 && random() % REOPEN_PROB == 0) {
		tdb_reopen_all(0);
		goto next;
	} 
#endif

#if DELETE_PROB
	if (random() % DELETE_PROB == 0) {
		tdb_delete(db, key);
		goto next;
	}
#endif

#if STORE_PROB
	if (random() % STORE_PROB == 0) {
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		goto next;
	}
#endif

#if APPEND_PROB
	if (random() % APPEND_PROB == 0) {
		if (tdb_append(db, key, data) != 0) {
			fatal("tdb_append failed");
		}
		goto next;
	}
#endif

#if LOCKSTORE_PROB
	if (random() % LOCKSTORE_PROB == 0) {
		tdb_chainlock(db, key);
		data = tdb_fetch(db, key);
		if (tdb_store(db, key, data, TDB_REPLACE) != 0) {
			fatal("tdb_store failed");
		}
		if (data.dptr) free(data.dptr);
		tdb_chainunlock(db, key);
		goto next;
	} 
#endif

#if TRAVERSE_PROB
	if (random() % TRAVERSE_PROB == 0) {
		tdb_traverse(db, cull_traverse, NULL);
		goto next;
	}
#endif

#if TRAVERSE_READ_PROB
	if (random() % TRAVERSE_READ_PROB == 0) {
		tdb_traverse_read(db, NULL, NULL);
		goto next;
	}
#endif

	data = tdb_fetch(db, key);
	if (data.dptr) free(data.dptr);

next:
	free(k);
	free(d);
}

static int traverse_fn(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
                       void *state)
{
	tdb_delete(tdb, key);
	return 0;
}

static void usage(void)
{
	printf("Usage: tdbtorture [-n NUM_PROCS] [-l NUM_LOOPS] [-s SEED] [-H HASH_SIZE]\n");
	exit(0);
}

 int main(int argc, char * const *argv)
{
	int i, seed = -1;
	int num_procs = 3;
	int num_loops = 5000;
	int hash_size = 2;
	int c;
	extern char *optarg;
	pid_t *pids;

	struct tdb_logging_context log_ctx;
	log_ctx.log_fn = tdb_log;

	while ((c = getopt(argc, argv, "n:l:s:H:h")) != -1) {
		switch (c) {
		case 'n':
			num_procs = strtol(optarg, NULL, 0);
			break;
		case 'l':
			num_loops = strtol(optarg, NULL, 0);
			break;
		case 'H':
			hash_size = strtol(optarg, NULL, 0);
			break;
		case 's':
			seed = strtol(optarg, NULL, 0);
			break;
		default:
			usage();
		}
	}

	unlink("torture.tdb");

	pids = calloc(sizeof(pid_t), num_procs);
	pids[0] = getpid();

	for (i=0;i<num_procs-1;i++) {
		if ((pids[i+1]=fork()) == 0) break;
	}

	db = tdb_open_ex("torture.tdb", hash_size, TDB_CLEAR_IF_FIRST, 
			 O_RDWR | O_CREAT, 0600, &log_ctx, NULL);
	if (!db) {
		fatal("db open failed");
	}

	if (seed == -1) {
		seed = (getpid() + time(NULL)) & 0x7FFFFFFF;
	}

	if (i == 0) {
		printf("testing with %d processes, %d loops, %d hash_size, seed=%d\n", 
		       num_procs, num_loops, hash_size, seed);
	}

	srand(seed + i);
	srandom(seed + i);

	for (i=0;i<num_loops && error_count == 0;i++) {
		addrec_db();
	}

	if (error_count == 0) {
		tdb_traverse_read(db, NULL, NULL);
		tdb_traverse(db, traverse_fn, NULL);
		tdb_traverse(db, traverse_fn, NULL);
	}

	tdb_close(db);

	if (getpid() != pids[0]) {
		return error_count;
	}

	for (i=1;i<num_procs;i++) {
		int status, j;
		pid_t pid;
		if (error_count != 0) {
			/* try and stop the test on any failure */
			for (j=1;j<num_procs;j++) {
				if (pids[j] != 0) {
					kill(pids[j], SIGTERM);
				}
			}
		}
		pid = waitpid(-1, &status, 0);
		if (pid == -1) {
			perror("failed to wait for child\n");
			exit(1);
		}
		for (j=1;j<num_procs;j++) {
			if (pids[j] == pid) break;
		}
		if (j == num_procs) {
			printf("unknown child %d exited!?\n", (int)pid);
			exit(1);
		}
		if (WEXITSTATUS(status) != 0) {
			printf("child %d exited with status %d\n",
			       (int)pid, WEXITSTATUS(status));
			error_count++;
		}
		pids[j] = 0;
	}

	if (error_count == 0) {
		printf("OK\n");
	}

	return error_count;
}
