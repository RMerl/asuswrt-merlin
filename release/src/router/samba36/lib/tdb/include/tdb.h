#ifndef __TDB_H__
#define __TDB_H__

/* 
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell 1999-2004
   
     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#ifdef  __cplusplus
extern "C" {
#endif

#include <signal.h>

/**
 * @defgroup tdb The tdb API
 *
 * tdb is a Trivial database. In concept, it is very much like GDBM, and BSD's
 * DB except that it allows multiple simultaneous writers and uses locking
 * internally to keep writers from trampling on each other. tdb is also
 * extremely small.
 *
 * @section tdb_interface Interface
 *
 * The interface is very similar to gdbm except for the following:
 *
 * <ul>
 * <li>different open interface. The tdb_open call is more similar to a
 * traditional open()</li>
 * <li>no tdbm_reorganise() function</li>
 * <li>no tdbm_sync() function. No operations are cached in the library
 *     anyway</li>
 * <li>added a tdb_traverse() function for traversing the whole database</li>
 * <li>added transactions support</li>
 * </ul>
 *
 * A general rule for using tdb is that the caller frees any returned TDB_DATA
 * structures. Just call free(p.dptr) to free a TDB_DATA return value called p.
 * This is the same as gdbm.
 *
 * @{
 */

/** Flags to tdb_store() */
#define TDB_REPLACE 1		/** Unused */
#define TDB_INSERT 2 		/** Don't overwrite an existing entry */
#define TDB_MODIFY 3		/** Don't create an existing entry    */

/** Flags for tdb_open() */
#define TDB_DEFAULT 0 /** just a readability place holder */
#define TDB_CLEAR_IF_FIRST 1 /** If this is the first open, wipe the db */
#define TDB_INTERNAL 2 /** Don't store on disk */
#define TDB_NOLOCK   4 /** Don't do any locking */
#define TDB_NOMMAP   8 /** Don't use mmap */
#define TDB_CONVERT 16 /** Convert endian (internal use) */
#define TDB_BIGENDIAN 32 /** Header is big-endian (internal use) */
#define TDB_NOSYNC   64 /** Don't use synchronous transactions */
#define TDB_SEQNUM   128 /** Maintain a sequence number */
#define TDB_VOLATILE   256 /** Activate the per-hashchain freelist, default 5 */
#define TDB_ALLOW_NESTING 512 /** Allow transactions to nest */
#define TDB_DISALLOW_NESTING 1024 /** Disallow transactions to nest */
#define TDB_INCOMPATIBLE_HASH 2048 /** Better hashing: can't be opened by tdb < 1.2.6. */

/** The tdb error codes */
enum TDB_ERROR {TDB_SUCCESS=0, TDB_ERR_CORRUPT, TDB_ERR_IO, TDB_ERR_LOCK, 
		TDB_ERR_OOM, TDB_ERR_EXISTS, TDB_ERR_NOLOCK, TDB_ERR_LOCK_TIMEOUT,
		TDB_ERR_NOEXIST, TDB_ERR_EINVAL, TDB_ERR_RDONLY,
		TDB_ERR_NESTING};

/** Debugging uses one of the following levels */
enum tdb_debug_level {TDB_DEBUG_FATAL = 0, TDB_DEBUG_ERROR, 
		      TDB_DEBUG_WARNING, TDB_DEBUG_TRACE};

/** The tdb data structure */
typedef struct TDB_DATA {
	unsigned char *dptr;
	size_t dsize;
} TDB_DATA;

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3)
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

/** This is the context structure that is returned from a db open. */
typedef struct tdb_context TDB_CONTEXT;

typedef int (*tdb_traverse_func)(struct tdb_context *, TDB_DATA, TDB_DATA, void *);
typedef void (*tdb_log_func)(struct tdb_context *, enum tdb_debug_level, const char *, ...) PRINTF_ATTRIBUTE(3, 4);
typedef unsigned int (*tdb_hash_func)(TDB_DATA *key);

struct tdb_logging_context {
        tdb_log_func log_fn;
        void *log_private;
};

/**
 * @brief Open the database and creating it if necessary.
 *
 * @param[in]  name     The name of the db to open.
 *
 * @param[in]  hash_size The hash size is advisory, use zero for a default
 *                       value.
 *
 * @param[in]  tdb_flags The flags to use to open the db:\n\n
 *                         TDB_CLEAR_IF_FIRST - Clear database if we are the
 *                                              only one with it open\n
 *                         TDB_INTERNAL - Don't use a file, instaed store the
 *                                        data in memory. The filename is
 *                                        ignored in this case.\n
 *                         TDB_NOLOCK - Don't do any locking\n
 *                         TDB_NOMMAP - Don't use mmap\n
 *                         TDB_NOSYNC - Don't synchronise transactions to disk\n
 *                         TDB_SEQNUM - Maintain a sequence number\n
 *                         TDB_VOLATILE - activate the per-hashchain freelist,
 *                                        default 5.\n
 *                         TDB_ALLOW_NESTING - Allow transactions to nest.\n
 *                         TDB_DISALLOW_NESTING - Disallow transactions to nest.\n
 *
 * @param[in]  open_flags Flags for the open(2) function.
 *
 * @param[in]  mode     The mode for the open(2) function.
 *
 * @return              A tdb context structure, NULL on error.
 */
struct tdb_context *tdb_open(const char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode);

/**
 * @brief Open the database and creating it if necessary.
 *
 * This is like tdb_open(), but allows you to pass an initial logging and
 * hash function. Be careful when passing a hash function - all users of the
 * database must use the same hash function or you will get data corruption.
 *
 * @param[in]  name     The name of the db to open.
 *
 * @param[in]  hash_size The hash size is advisory, use zero for a default
 *                       value.
 *
 * @param[in]  tdb_flags The flags to use to open the db:\n\n
 *                         TDB_CLEAR_IF_FIRST - Clear database if we are the
 *                                              only one with it open\n
 *                         TDB_INTERNAL - Don't use a file, instaed store the
 *                                        data in memory. The filename is
 *                                        ignored in this case.\n
 *                         TDB_NOLOCK - Don't do any locking\n
 *                         TDB_NOMMAP - Don't use mmap\n
 *                         TDB_NOSYNC - Don't synchronise transactions to disk\n
 *                         TDB_SEQNUM - Maintain a sequence number\n
 *                         TDB_VOLATILE - activate the per-hashchain freelist,
 *                                        default 5.\n
 *                         TDB_ALLOW_NESTING - Allow transactions to nest.\n
 *                         TDB_DISALLOW_NESTING - Disallow transactions to nest.\n
 *
 * @param[in]  open_flags Flags for the open(2) function.
 *
 * @param[in]  mode     The mode for the open(2) function.
 *
 * @param[in]  log_ctx  The logging function to use.
 *
 * @param[in]  hash_fn  The hash function you want to use.
 *
 * @return              A tdb context structure, NULL on error.
 *
 * @see tdb_open()
 */
struct tdb_context *tdb_open_ex(const char *name, int hash_size, int tdb_flags,
			 int open_flags, mode_t mode,
			 const struct tdb_logging_context *log_ctx,
			 tdb_hash_func hash_fn);

/**
 * @brief Set the maximum number of dead records per hash chain.
 *
 * @param[in]  tdb      The database handle to set the maximum.
 *
 * @param[in]  max_dead The maximum number of dead records per hash chain.
 */
void tdb_set_max_dead(struct tdb_context *tdb, int max_dead);

/**
 * @brief Reopen a tdb.
 *
 * This can be used after a fork to ensure that we have an independent seek
 * pointer from our parent and to re-establish locks.
 *
 * @param[in]  tdb      The database to reopen.
 *
 * @return              0 on success, -1 on error.
 */
int tdb_reopen(struct tdb_context *tdb);

/**
 * @brief Reopen all tdb's
 *
 * If the parent is longlived (ie. a parent daemon architecture), we know it
 * will keep it's active lock on a tdb opened with CLEAR_IF_FIRST. Thus for
 * child processes we don't have to add an active lock. This is essential to
 * improve performance on systems that keep POSIX locks as a non-scalable data
 * structure in the kernel.
 *
 * @param[in]  parent_longlived Wether the parent is longlived or not.
 *
 * @return              0 on success, -1 on error.
 */
int tdb_reopen_all(int parent_longlived);

/**
 * @brief Set a different tdb logging function.
 *
 * @param[in]  tdb      The tdb to set the logging function.
 *
 * @param[in]  log_ctx  The logging function to set.
 */
void tdb_set_logging_function(struct tdb_context *tdb, const struct tdb_logging_context *log_ctx);

/**
 * @brief Get the tdb last error code.
 *
 * @param[in]  tdb      The tdb to get the error code from.
 *
 * @return              A TDB_ERROR code.
 *
 * @see TDB_ERROR
 */
enum TDB_ERROR tdb_error(struct tdb_context *tdb);

/**
 * @brief Get a error string for the last tdb error
 *
 * @param[in]  tdb      The tdb to get the error code from.
 *
 * @return              An error string.
 */
const char *tdb_errorstr(struct tdb_context *tdb);

/**
 * @brief Fetch an entry in the database given a key.
 *
 * The caller must free the resulting data.
 *
 * @param[in]  tdb      The tdb to fetch the key.
 *
 * @param[in]  key      The key to fetch.
 *
 * @return              The key entry found in the database, NULL on error with
 *                      TDB_ERROR set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
TDB_DATA tdb_fetch(struct tdb_context *tdb, TDB_DATA key);

/**
 * @brief Hand a record to a parser function without allocating it.
 *
 * This function is meant as a fast tdb_fetch alternative for large records
 * that are frequently read. The "key" and "data" arguments point directly
 * into the tdb shared memory, they are not aligned at any boundary.
 *
 * @warning The parser is called while tdb holds a lock on the record. DO NOT
 * call other tdb routines from within the parser. Also, for good performance
 * you should make the parser fast to allow parallel operations.
 *
 * @param[in]  tdb      The tdb to parse the record.
 *
 * @param[in]  key      The key to parse.
 *
 * @param[in]  parser   The parser to use to parse the data.
 *
 * @param[in]  private_data A private data pointer which is passed to the parser
 *                          function.
 *
 * @return              -1 if the record was not found. If the record was found,
 *                      the return value of "parser" is passed up to the caller.
 */
int tdb_parse_record(struct tdb_context *tdb, TDB_DATA key,
			      int (*parser)(TDB_DATA key, TDB_DATA data,
					    void *private_data),
			      void *private_data);

/**
 * @brief Delete an entry in the database given a key.
 *
 * @param[in]  tdb      The tdb to delete the key.
 *
 * @param[in]  key      The key to delete.
 *
 * @return              0 on success, -1 if the key doesn't exist.
 */
int tdb_delete(struct tdb_context *tdb, TDB_DATA key);

/**
 * @brief Store an element in the database.
 *
 * This replaces any existing element with the same key.
 *
 * @param[in]  tdb      The tdb to store the entry.
 *
 * @param[in]  key      The key to use to store the entry.
 *
 * @param[in]  dbuf     The data to store under the key.
 *
 * @param[in]  flag     The flags to store the key:\n\n
 *                      TDB_INSERT: Don't overwrite an existing entry.\n
 *                      TDB_MODIFY: Don't create a new entry\n
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf, int flag);

/**
 * @brief Append data to an entry.
 *
 * If the entry doesn't exist, it will create a new one.
 *
 * @param[in]  tdb      The database to use.
 *
 * @param[in]  key      The key to append the data.
 *
 * @param[in]  new_dbuf The data to append to the key.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_append(struct tdb_context *tdb, TDB_DATA key, TDB_DATA new_dbuf);

/**
 * @brief Close a database.
 *
 * @param[in]  tdb      The database to close.
 *
 * @return              0 for success, -1 on error.
 */
int tdb_close(struct tdb_context *tdb);

/**
 * @brief Find the first entry in the database and return its key.
 *
 * The caller must free the returned data.
 *
 * @param[in]  tdb      The database to use.
 *
 * @return              The first entry of the database, an empty TDB_DATA entry
 *                      if the database is empty.
 */
TDB_DATA tdb_firstkey(struct tdb_context *tdb);

/**
 * @brief Find the next entry in the database, returning its key.
 *
 * The caller must free the returned data.
 *
 * @param[in]  tdb      The database to use.
 *
 * @param[in]  key      The key from which you want the next key.
 *
 * @return              The next entry of the current key, an empty TDB_DATA
 *                      entry if there is no entry.
 */
TDB_DATA tdb_nextkey(struct tdb_context *tdb, TDB_DATA key);

/**
 * @brief Traverse the entire database.
 *
 * While travering the function fn(tdb, key, data, state) is called on each
 * element. If fn is NULL then it is not called. A non-zero return value from
 * fn() indicates that the traversal should stop. Traversal callbacks may not
 * start transactions.
 *
 * @warning The data buffer given to the callback fn does NOT meet the alignment
 * restrictions malloc gives you.
 *
 * @param[in]  tdb      The database to traverse.
 *
 * @param[in]  fn       The function to call on each entry.
 *
 * @param[in]  private_data The private data which should be passed to the
 *                          traversing function.
 *
 * @return              The record count traversed, -1 on error.
 */
int tdb_traverse(struct tdb_context *tdb, tdb_traverse_func fn, void *private_data);

/**
 * @brief Traverse the entire database.
 *
 * While traversing the database the function fn(tdb, key, data, state) is
 * called on each element, but marking the database read only during the
 * traversal, so any write operations will fail. This allows tdb to use read
 * locks, which increases the parallelism possible during the traversal.
 *
 * @param[in]  tdb      The database to traverse.
 *
 * @param[in]  fn       The function to call on each entry.
 *
 * @param[in]  private_data The private data which should be passed to the
 *                          traversing function.
 *
 * @return              The record count traversed, -1 on error.
 */
int tdb_traverse_read(struct tdb_context *tdb, tdb_traverse_func fn, void *private_data);

/**
 * @brief Check if an entry in the database exists.
 *
 * @note 1 is returned if the key is found and 0 is returned if not found this
 * doesn't match the conventions in the rest of this module, but is compatible
 * with gdbm.
 *
 * @param[in]  tdb      The database to check if the entry exists.
 *
 * @param[in]  key      The key to check if the entry exists.
 *
 * @return              1 if the key is found, 0 if not.
 */
int tdb_exists(struct tdb_context *tdb, TDB_DATA key);

/**
 * @brief Lock entire database with a write lock.
 *
 * @param[in]  tdb      The database to lock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall(struct tdb_context *tdb);

/**
 * @brief Lock entire database with a write lock.
 *
 * This is the non-blocking call.
 *
 * @param[in]  tdb      The database to lock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_lockall()
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall_nonblock(struct tdb_context *tdb);

/**
 * @brief Unlock entire database with write lock.
 *
 * @param[in]  tdb      The database to unlock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_lockall()
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_unlockall(struct tdb_context *tdb);

/**
 * @brief Lock entire database with a read lock.
 *
 * @param[in]  tdb      The database to lock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall_read(struct tdb_context *tdb);

/**
 * @brief Lock entire database with a read lock.
 *
 * This is the non-blocking call.
 *
 * @param[in]  tdb      The database to lock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_lockall_read()
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall_read_nonblock(struct tdb_context *tdb);

/**
 * @brief Unlock entire database with read lock.
 *
 * @param[in]  tdb      The database to unlock.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_lockall_read()
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_unlockall_read(struct tdb_context *tdb);

/**
 * @brief Lock entire database with write lock - mark only.
 *
 * @todo Add more details.
 *
 * @param[in]  tdb      The database to mark.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall_mark(struct tdb_context *tdb);

/**
 * @brief Lock entire database with write lock - unmark only.
 *
 * @todo Add more details.
 *
 * @param[in]  tdb      The database to mark.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_lockall_unmark(struct tdb_context *tdb);

/**
 * @brief Get the name of the current tdb file.
 *
 * This is useful for external logging functions.
 *
 * @param[in]  tdb      The database to get the name from.
 *
 * @return              The name of the database.
 */
const char *tdb_name(struct tdb_context *tdb);

/**
 * @brief Get the underlying file descriptor being used by tdb.
 *
 * This is useful for external routines that want to check the device/inode
 * of the fd.
 *
 * @param[in]  tdb      The database to get the fd from.
 *
 * @return              The file descriptor or -1.
 */
int tdb_fd(struct tdb_context *tdb);

/**
 * @brief Get the current logging function.
 *
 * This is useful for external tdb routines that wish to log tdb errors.
 *
 * @param[in]  tdb      The database to get the logging function from.
 *
 * @return              The logging function of the database.
 *
 * @see tdb_get_logging_private()
 */
tdb_log_func tdb_log_fn(struct tdb_context *tdb);

/**
 * @brief Get the private data of the logging function.
 *
 * @param[in]  tdb      The database to get the data from.
 *
 * @return              The private data pointer of the logging function.
 *
 * @see tdb_log_fn()
 */
void *tdb_get_logging_private(struct tdb_context *tdb);

/**
 * @brief Start a transaction.
 *
 * All operations after the transaction start can either be committed with
 * tdb_transaction_commit() or cancelled with tdb_transaction_cancel().
 *
 * If you call tdb_transaction_start() again on the same tdb context while a
 * transaction is in progress, then the same transaction buffer is re-used. The
 * number of tdb_transaction_{commit,cancel} operations must match the number
 * of successful tdb_transaction_start() calls.
 *
 * Note that transactions are by default disk synchronous, and use a recover
 * area in the database to automatically recover the database on the next open
 * if the system crashes during a transaction. You can disable the synchronous
 * transaction recovery setup using the TDB_NOSYNC flag, which will greatly
 * speed up operations at the risk of corrupting your database if the system
 * crashes.
 *
 * Operations made within a transaction are not visible to other users of the
 * database until a successful commit.
 *
 * @param[in]  tdb      The database to start the transaction.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_transaction_start(struct tdb_context *tdb);

/**
 * @brief Start a transaction, non-blocking.
 *
 * @param[in]  tdb      The database to start the transaction.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 * @see tdb_transaction_start()
 */
int tdb_transaction_start_nonblock(struct tdb_context *tdb);

/**
 * @brief Prepare to commit a current transaction, for two-phase commits.
 *
 * Once prepared for commit, the only allowed calls are tdb_transaction_commit()
 * or tdb_transaction_cancel(). Preparing allocates disk space for the pending
 * updates, so a subsequent commit should succeed (barring any hardware
 * failures).
 *
 * @param[in]  tdb      The database to prepare the commit.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_transaction_prepare_commit(struct tdb_context *tdb);

/**
 * @brief Commit a current transaction.
 *
 * This updates the database and releases the current transaction locks.
 *
 * @param[in]  tdb      The database to commit the transaction.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_transaction_commit(struct tdb_context *tdb);

/**
 * @brief Cancel a current transaction.
 *
 * This discards all write and lock operations that have been made since the
 * transaction started.
 *
 * @param[in]  tdb      The tdb to cancel the transaction on.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_transaction_cancel(struct tdb_context *tdb);

/**
 * @brief Get the tdb sequence number.
 *
 * Only makes sense if the writers opened with TDB_SEQNUM set. Note that this
 * sequence number will wrap quite quickly, so it should only be used for a
 * 'has something changed' test, not for code that relies on the count of the
 * number of changes made. If you want a counter then use a tdb record.
 *
 * The aim of this sequence number is to allow for a very lightweight test of a
 * possible tdb change.
 *
 * @param[in]  tdb      The database to get the sequence number from.
 *
 * @return              The sequence number or 0.
 *
 * @see tdb_open()
 * @see tdb_enable_seqnum()
 */
int tdb_get_seqnum(struct tdb_context *tdb);

/**
 * @brief Get the hash size.
 *
 * @param[in]  tdb      The database to get the hash size from.
 *
 * @return              The hash size.
 */
int tdb_hash_size(struct tdb_context *tdb);

/**
 * @brief Get the map size.
 *
 * @param[in]  tdb     The database to get the map size from.
 *
 * @return             The map size.
 */
size_t tdb_map_size(struct tdb_context *tdb);

/**
 * @brief Get the tdb flags set during open.
 *
 * @param[in]  tdb      The database to get the flags form.
 *
 * @return              The flags set to on the database.
 */
int tdb_get_flags(struct tdb_context *tdb);

/**
 * @brief Add flags to the database.
 *
 * @param[in]  tdb      The database to add the flags.
 *
 * @param[in]  flag     The tdb flags to add.
 */
void tdb_add_flags(struct tdb_context *tdb, unsigned flag);

/**
 * @brief Remove flags from the database.
 *
 * @param[in]  tdb      The database to remove the flags.
 *
 * @param[in]  flag     The tdb flags to remove.
 */
void tdb_remove_flags(struct tdb_context *tdb, unsigned flag);

/**
 * @brief Enable sequence number handling on an open tdb.
 *
 * @param[in]  tdb      The database to enable sequence number handling.
 *
 * @see tdb_get_seqnum()
 */
void tdb_enable_seqnum(struct tdb_context *tdb);

/**
 * @brief Increment the tdb sequence number.
 *
 * This only works if the tdb has been opened using the TDB_SEQNUM flag or
 * enabled useing tdb_enable_seqnum().
 *
 * @param[in]  tdb      The database to increment the sequence number.
 *
 * @see tdb_enable_seqnum()
 * @see tdb_get_seqnum()
 */
void tdb_increment_seqnum_nonblock(struct tdb_context *tdb);

/**
 * @brief Create a hash of the key.
 *
 * @param[in]  key      The key to hash
 *
 * @return              The hash.
 */
unsigned int tdb_jenkins_hash(TDB_DATA *key);

/**
 * @brief Check the consistency of the database.
 *
 * This check the consistency of the database calling back the check function
 * (if non-NULL) on each record.  If some consistency check fails, or the
 * supplied check function returns -1, tdb_check returns -1, otherwise 0.
 *
 * @note The logging function (if set) will be called with additional
 * information on the corruption found.
 *
 * @param[in]  tdb      The database to check.
 *
 * @param[in]  check    The check function to use.
 *
 * @param[in]  private_data the private data to pass to the check function.
 *
 * @return              0 on success, -1 on error with error code set.
 *
 * @see tdb_error()
 * @see tdb_errorstr()
 */
int tdb_check(struct tdb_context *tdb,
	      int (*check) (TDB_DATA key, TDB_DATA data, void *private_data),
	      void *private_data);

/* @} ******************************************************************/

/* Low level locking functions: use with care */
int tdb_chainlock(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainlock_nonblock(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainlock_mark(struct tdb_context *tdb, TDB_DATA key);
int tdb_chainlock_unmark(struct tdb_context *tdb, TDB_DATA key);

void tdb_setalarm_sigptr(struct tdb_context *tdb, volatile sig_atomic_t *sigptr);

/* wipe and repack */
int tdb_wipe_all(struct tdb_context *tdb);
int tdb_repack(struct tdb_context *tdb);

/* Debug functions. Not used in production. */
void tdb_dump_all(struct tdb_context *tdb);
int tdb_printfreelist(struct tdb_context *tdb);
int tdb_validate_freelist(struct tdb_context *tdb, int *pnum_entries);
int tdb_freelist_size(struct tdb_context *tdb);
char *tdb_summary(struct tdb_context *tdb);

extern TDB_DATA tdb_null;

#ifdef  __cplusplus
}
#endif

#endif /* tdb.h */
