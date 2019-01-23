/* 
   Unix SMB/CIFS implementation.
   Utility functions for Samba
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Jelmer Vernooij 2005
    
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SAMBA_UTIL_H_
#define _SAMBA_UTIL_H_

#include "lib/util/charset/charset.h"
#include "lib/util/attr.h"

/* for TALLOC_CTX */
#include <talloc.h>

/**
 * @file
 * @brief Helpful macros
 */

struct smbsrv_tcon;

extern const char *logfile;
extern const char *panic_action;

#include "lib/util/time.h"
#include "lib/util/data_blob.h"
#include "lib/util/xfile.h"
#include "lib/util/byteorder.h"
#include "lib/util/talloc_stack.h"

/**
 * assert macros 
 */
#ifdef DEVELOPER
#define SMB_ASSERT(b) do { if (!(b)) { \
        DEBUG(0,("PANIC: assert failed at %s(%d): %s\n", \
		 __FILE__, __LINE__, #b)); smb_panic("assert failed: " #b); }} while(0)
#else
/* redefine the assert macro for non-developer builds */
#define SMB_ASSERT(b) do { if (!(b)) { \
        DEBUG(3,("PANIC: assert failed at %s(%d): %s\n", \
	    __FILE__, __LINE__, #b)); }} while (0)
#endif

#ifndef ABS
#define ABS(a) ((a)>0?(a):(-(a)))
#endif

#include "lib/util/memory.h"

/**
 * Write backtrace to debug log
 */
_PUBLIC_ void call_backtrace(void);

/**
 Something really nasty happened - panic !
**/
_PUBLIC_ _NORETURN_ void smb_panic(const char *why);

#if _SAMBA_BUILD_ == 4
/**
setup our fault handlers
**/
_PUBLIC_ void fault_setup(const char *pname);
_PUBLIC_ void fault_setup_disable(void);
#endif

/**
  register a fault handler. 
  Should only be called once in the execution of smbd.
*/
_PUBLIC_ bool register_fault_handler(const char *name, void (*fault_handler)(int sig));

/* The following definitions come from lib/util/signal.c  */


/**
 Block sigs.
**/
void BlockSignals(bool block, int signum);

/**
 Catch a signal. This should implement the following semantics:

 1) The handler remains installed after being called.
 2) The signal should be blocked during handler execution.
**/
void (*CatchSignal(int signum,void (*handler)(int )))(int);

/**
 Ignore SIGCLD via whatever means is necessary for this OS.
**/
void CatchChild(void);

/**
 Catch SIGCLD but leave the child around so it's status can be reaped.
**/
void CatchChildLeaveStatus(void);

/* The following definitions come from lib/util/system.c  */

/**************************************************************************
A wrapper for gethostbyname() that tries avoids looking up hostnames 
in the root domain, which can cause dial-on-demand links to come up for no
apparent reason.
****************************************************************************/
_PUBLIC_ struct hostent *sys_gethostbyname(const char *name);
_PUBLIC_ struct in_addr sys_inet_makeaddr(int net, int host);

/**
 * Wrapper for fork used to invalid pid cache.
 **/
_PUBLIC_ pid_t sys_fork(void);

/**
 * Wrapper for getpid. Ensures we only do a system call *once*.
 **/
_PUBLIC_ pid_t sys_getpid(void);

_PUBLIC_ int sys_getpeereid( int s, uid_t *uid);

struct sockaddr;

_PUBLIC_ int sys_getnameinfo(const struct sockaddr *psa,
			     int salen,
			     char *host,
			     size_t hostlen,
			     char *service,
			     size_t servlen,
			     int flags);
_PUBLIC_ int sys_connect(int fd, const struct sockaddr * addr);

/* The following definitions come from lib/util/genrand.c  */
/**
 Copy any user given reseed data.
**/
_PUBLIC_ void set_rand_reseed_callback(void (*fn)(void *, int *), void *);

/**
 * Tell the random number generator it needs to reseed.
 */
_PUBLIC_ void set_need_random_reseed(void);

/**
 Interface to the (hopefully) good crypto random number generator.
 Will use our internal PRNG if more than 40 bytes of random generation
 has been requested, otherwise tries to read from /dev/random
**/
_PUBLIC_ void generate_random_buffer(uint8_t *out, int len);

/**
 Interface to the (hopefully) good crypto random number generator.
 Will always use /dev/urandom if available.
**/
_PUBLIC_ void generate_secret_buffer(uint8_t *out, int len);

/**
  generate a single random uint32_t
**/
_PUBLIC_ uint32_t generate_random(void);

/**
  very basic password quality checker
**/
_PUBLIC_ bool check_password_quality(const char *s);

/**
 * Generate a random text password.
 */
_PUBLIC_ char *generate_random_password(TALLOC_CTX *mem_ctx, size_t min, size_t max);

/**
 Use the random number generator to generate a random string.
**/
_PUBLIC_ char *generate_random_str_list(TALLOC_CTX *mem_ctx, size_t len, const char *list);

/**
 * Generate a random text string consisting of the specified length.
 * The returned string will be allocated.
 *
 * Characters used are: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_-#.,
 */
_PUBLIC_ char *generate_random_str(TALLOC_CTX *mem_ctx, size_t len);

/**
 * Generate an array of unique text strings all of the same length.
 * The returned strings will be allocated.
 * Returns NULL if the number of unique combinations cannot be created.
 *
 * Characters used are: ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_-#.,
 */
_PUBLIC_ char** generate_unique_strs(TALLOC_CTX *mem_ctx, size_t len,
                                         uint32_t num);

/* The following definitions come from lib/util/dprintf.c  */
#if _SAMBA_BUILD_ == 4

_PUBLIC_ void d_set_iconv(smb_iconv_t);
_PUBLIC_ int d_vfprintf(FILE *f, const char *format, va_list ap) PRINTF_ATTRIBUTE(2,0);
_PUBLIC_ int d_fprintf(FILE *f, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
_PUBLIC_ int d_printf(const char *format, ...) PRINTF_ATTRIBUTE(1,2);
_PUBLIC_ void display_set_stderr(void);
#endif

/* The following definitions come from lib/util/util_str.c  */

bool next_token_talloc(TALLOC_CTX *ctx,
			const char **ptr,
			char **pp_buff,
			const char *sep);

/**
 * Get the next token from a string, return false if none found.  Handles
 * double-quotes.  This version does not trim leading separator characters
 * before looking for a token.
 */
bool next_token_no_ltrim_talloc(TALLOC_CTX *ctx,
			const char **ptr,
			char **pp_buff,
			const char *sep);


/**
 Trim the specified elements off the front and back of a string.
**/
_PUBLIC_ bool trim_string(char *s, const char *front, const char *back);

/**
 Find the number of 'c' chars in a string
**/
_PUBLIC_ _PURE_ size_t count_chars(const char *s, char c);

/**
 Safe string copy into a known length string. maxlength does not
 include the terminating zero.
**/
_PUBLIC_ char *safe_strcpy(char *dest,const char *src, size_t maxlength);

/**
 Safe string cat into a string. maxlength does not
 include the terminating zero.
**/
_PUBLIC_ char *safe_strcat(char *dest, const char *src, size_t maxlength);

/**
 Routine to get hex characters and turn them into a 16 byte array.
 the array can be variable length, and any non-hex-numeric
 characters are skipped.  "0xnn" or "0Xnn" is specially catered
 for.

 valid examples: "0A5D15"; "0x15, 0x49, 0xa2"; "59\ta9\te3\n"


**/
_PUBLIC_ size_t strhex_to_str(char *p, size_t p_len, const char *strhex, size_t strhex_len);

/** 
 * Parse a hex string and return a data blob. 
 */
_PUBLIC_ _PURE_ DATA_BLOB strhex_to_data_blob(TALLOC_CTX *mem_ctx, const char *strhex) ;

/**
 * Routine to print a buffer as HEX digits, into an allocated string.
 */
_PUBLIC_ void hex_encode(const unsigned char *buff_in, size_t len, char **out_hex_buffer);

/**
 * talloc version of hex_encode()
 */
_PUBLIC_ char *hex_encode_talloc(TALLOC_CTX *mem_ctx, const unsigned char *buff_in, size_t len);

/**
 Substitute a string for a pattern in another string. Make sure there is 
 enough room!

 This routine looks for pattern in s and replaces it with 
 insert. It may do multiple replacements.

 Any of " ; ' $ or ` in the insert string are replaced with _
 if len==0 then the string cannot be extended. This is different from the old
 use of len==0 which was for no length checks to be done.
**/
_PUBLIC_ void string_sub(char *s,const char *pattern, const char *insert, size_t len);


_PUBLIC_ char *string_sub_talloc(TALLOC_CTX *mem_ctx, const char *s, 
				const char *pattern, const char *insert);

/**
 Similar to string_sub() but allows for any character to be substituted. 
 Use with caution!
 if len==0 then the string cannot be extended. This is different from the old
 use of len==0 which was for no length checks to be done.
**/
_PUBLIC_ void all_string_sub(char *s,const char *pattern,const char *insert, size_t len);

/**
 Unescape a URL encoded string, in place.
**/
_PUBLIC_ void rfc1738_unescape(char *buf);


/**
 * rfc1738_escape
 * Returns a static buffer that contains the RFC
 * 1738 compliant, escaped version of the given url. (escapes unsafe and % characters)
 **/
_PUBLIC_ char *rfc1738_escape(TALLOC_CTX *mem_ctx, const char *url);

/**
 * rfc1738_escape_unescaped
 *
 * Returns a static buffer that contains
 * the RFC 1738 compliant, escaped version of the given url (escapes unsafe chars only)
 **/
_PUBLIC_ char *rfc1738_escape_unescaped(TALLOC_CTX *mem_ctx, const char *url);

/**
 * rfc1738_escape_part 
 * Returns a static buffer that contains the RFC
 * 1738 compliant, escaped version of the given url segment. (escapes
 * unsafe, reserved and % chars) It would mangle the :// in http://,
 * and mangle paths (because of /).
 **/
_PUBLIC_ char *rfc1738_escape_part(TALLOC_CTX *mem_ctx, const char *url);

/**
  format a string into length-prefixed dotted domain format, as used in NBT
  and in some ADS structures
**/
_PUBLIC_ const char *str_format_nbt_domain(TALLOC_CTX *mem_ctx, const char *s);

/**
 * Add a string to an array of strings.
 *
 * num should be a pointer to an integer that holds the current 
 * number of elements in strings. It will be updated by this function.
 */
_PUBLIC_ bool add_string_to_array(TALLOC_CTX *mem_ctx,
			 const char *str, const char ***strings, int *num);

/**
  varient of strcmp() that handles NULL ptrs
**/
_PUBLIC_ int strcmp_safe(const char *s1, const char *s2);

/**
return the number of bytes occupied by a buffer in ASCII format
the result includes the null termination
limited by 'n' bytes
**/
_PUBLIC_ size_t ascii_len_n(const char *src, size_t n);

/**
 Set a boolean variable from the text value stored in the passed string.
 Returns true in success, false if the passed string does not correctly 
 represent a boolean.
**/
_PUBLIC_ bool set_boolean(const char *boolean_string, bool *boolean);

/**
 * Parse a string containing a boolean value.
 *
 * val will be set to the read value.
 *
 * @retval true if a boolean value was parsed, false otherwise.
 */
_PUBLIC_ bool conv_str_bool(const char * str, bool * val);

#if _SAMBA_BUILD_ == 4
/**
 * Convert a size specification like 16K into an integral number of bytes. 
 **/
_PUBLIC_ bool conv_str_size(const char * str, uint64_t * val);
#endif

/**
 * Parse a uint64_t value from a string
 *
 * val will be set to the value read.
 *
 * @retval true if parsing was successful, false otherwise
 */
_PUBLIC_ bool conv_str_u64(const char * str, uint64_t * val);

/**
return the number of bytes occupied by a buffer in CH_UTF16 format
the result includes the null termination
**/
_PUBLIC_ size_t utf16_len(const void *buf);

/**
return the number of bytes occupied by a buffer in CH_UTF16 format
the result includes the null termination
limited by 'n' bytes
**/
_PUBLIC_ size_t utf16_len_n(const void *src, size_t n);
_PUBLIC_ size_t ucs2_align(const void *base_ptr, const void *p, int flags);

/**
Do a case-insensitive, whitespace-ignoring string compare.
**/
_PUBLIC_ int strwicmp(const char *psz1, const char *psz2);

/**
 String replace.
**/
_PUBLIC_ void string_replace(char *s, char oldc, char newc);

/**
 * Compare 2 strings.
 *
 * @note The comparison is case-insensitive.
 **/
_PUBLIC_ bool strequal(const char *s1, const char *s2);

/* The following definitions come from lib/util/util_strlist.c  */

/* separators for lists */
#ifndef LIST_SEP
#define LIST_SEP " \t,\n\r"
#endif

/**
  build an empty (only NULL terminated) list of strings (for expansion with str_list_add() etc)
*/
_PUBLIC_ char **str_list_make_empty(TALLOC_CTX *mem_ctx);

/**
  place the only element 'entry' into a new, NULL terminated string list
*/
_PUBLIC_ char **str_list_make_single(TALLOC_CTX *mem_ctx,
	const char *entry);

/**
  build a null terminated list of strings from a input string and a
  separator list. The separator list must contain characters less than
  or equal to 0x2f for this to work correctly on multi-byte strings
*/
_PUBLIC_ char **str_list_make(TALLOC_CTX *mem_ctx, const char *string,
	const char *sep);

/**
 * build a null terminated list of strings from an argv-like input string 
 * Entries are separated by spaces and can be enclosed by quotes.
 * Does NOT support escaping
 */
_PUBLIC_ char **str_list_make_shell(TALLOC_CTX *mem_ctx, const char *string, const char *sep);

/**
 * join a list back to one string 
 */
_PUBLIC_ char *str_list_join(TALLOC_CTX *mem_ctx, const char **list, char separator);

/** join a list back to one (shell-like) string; entries 
 * separated by spaces, using quotes where necessary */
_PUBLIC_ char *str_list_join_shell(TALLOC_CTX *mem_ctx, const char **list, char sep);

/**
  return the number of elements in a string list
*/
_PUBLIC_ size_t str_list_length(const char * const *list);

/**
  copy a string list
*/
_PUBLIC_ char **str_list_copy(TALLOC_CTX *mem_ctx, const char **list);

/**
   Return true if all the elements of the list match exactly.
 */
_PUBLIC_ bool str_list_equal(const char * const *list1, const char * const *list2);

/**
  add an entry to a string list
*/
_PUBLIC_ const char **str_list_add(const char **list, const char *s);

/**
  remove an entry from a string list
*/
_PUBLIC_ void str_list_remove(const char **list, const char *s);

/**
  return true if a string is in a list
*/
_PUBLIC_ bool str_list_check(const char **list, const char *s);

/**
  return true if a string is in a list, case insensitively
*/
_PUBLIC_ bool str_list_check_ci(const char **list, const char *s);
/**
  append one list to another - expanding list1
*/
_PUBLIC_ const char **str_list_append(const char **list1,
	const char * const *list2);

/**
 remove duplicate elements from a list 
*/
_PUBLIC_ const char **str_list_unique(const char **list);

/*
  very useful when debugging complex list related code
 */
_PUBLIC_ void str_list_show(const char **list);


/**
  append one list to another - expanding list1
  this assumes the elements of list2 are const pointers, so we can re-use them
*/
_PUBLIC_ const char **str_list_append_const(const char **list1,
					    const char **list2);

/**
  add an entry to a string list
  this assumes s will not change
*/
_PUBLIC_ const char **str_list_add_const(const char **list, const char *s);

/**
  copy a string list
  this assumes list will not change
*/
_PUBLIC_ const char **str_list_copy_const(TALLOC_CTX *mem_ctx,
					  const char **list);

/**
 * Needed for making an "unconst" list "const"
 */
_PUBLIC_ const char **const_str_list(char **list);


/* The following definitions come from lib/util/util_file.c  */


/**
read a line from a file with possible \ continuation chars. 
Blanks at the start or end of a line are stripped.
The string will be allocated if s2 is NULL
**/
_PUBLIC_ char *fgets_slash(char *s2,int maxlen,XFILE *f);

/**
 * Read one line (data until next newline or eof) and allocate it 
 */
_PUBLIC_ char *afdgets(int fd, TALLOC_CTX *mem_ctx, size_t hint);

/**
load a file into memory from a fd.
**/
_PUBLIC_ char *fd_load(int fd, size_t *size, size_t maxsize, TALLOC_CTX *mem_ctx);


char **file_lines_parse(char *p, size_t size, int *numlines, TALLOC_CTX *mem_ctx);

/**
load a file into memory
**/
_PUBLIC_ char *file_load(const char *fname, size_t *size, size_t maxsize, TALLOC_CTX *mem_ctx);

/**
mmap (if possible) or read a file
**/
_PUBLIC_ void *map_file(const char *fname, size_t size);

/**
load a file into memory and return an array of pointers to lines in the file
must be freed with talloc_free(). 
**/
_PUBLIC_ char **file_lines_load(const char *fname, int *numlines, size_t maxsize, TALLOC_CTX *mem_ctx);

/**
load a fd into memory and return an array of pointers to lines in the file
must be freed with talloc_free(). If convert is true calls unix_to_dos on
the list.
**/
_PUBLIC_ char **fd_lines_load(int fd, int *numlines, size_t maxsize, TALLOC_CTX *mem_ctx);

/**
take a list of lines and modify them to produce a list where \ continues
a line
**/
_PUBLIC_ void file_lines_slashcont(char **lines);

/**
  save a lump of data into a file. Mostly used for debugging 
*/
_PUBLIC_ bool file_save(const char *fname, const void *packet, size_t length);
_PUBLIC_ int vfdprintf(int fd, const char *format, va_list ap) PRINTF_ATTRIBUTE(2,0);
_PUBLIC_ int fdprintf(int fd, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
_PUBLIC_ bool large_file_support(const char *path);

/*
  compare two files, return true if the two files have the same content
 */
bool file_compare(const char *path1, const char *path2);

/* The following definitions come from lib/util/util.c  */


/**
 Find a suitable temporary directory. The result should be copied immediately
 as it may be overwritten by a subsequent call.
**/
_PUBLIC_ const char *tmpdir(void);

/**
 Check if a file exists - call vfs_file_exist for samba files.
**/
_PUBLIC_ bool file_exist(const char *fname);

/**
 Check a files mod time.
**/
_PUBLIC_ time_t file_modtime(const char *fname);

/**
 Check if a directory exists.
**/
_PUBLIC_ bool directory_exist(const char *dname);

/**
 * Try to create the specified directory if it didn't exist.
 *
 * @retval true if the directory already existed and has the right permissions 
 * or was successfully created.
 */
_PUBLIC_ bool directory_create_or_exist(const char *dname, uid_t uid, 
			       mode_t dir_perms);

/**
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
  if SYSV use O_NDELAY
  if BSD use FNDELAY
**/
_PUBLIC_ int set_blocking(int fd, bool set);

/**
 Sleep for a specified number of milliseconds.
**/
_PUBLIC_ void smb_msleep(unsigned int t);

/**
 Get my own name, return in talloc'ed storage.
**/
_PUBLIC_ char* get_myname(TALLOC_CTX *mem_ctx);

/**
 Check if a process exists. Does this work on all unixes?
**/
_PUBLIC_ bool process_exists_by_pid(pid_t pid);

/**
 Simple routine to do POSIX file locking. Cruft in NFS and 64->32 bit mapping
 is dealt with in posix.c
**/
_PUBLIC_ bool fcntl_lock(int fd, int op, off_t offset, off_t count, int type);

/**
 * Write dump of binary data to a callback
 */
void dump_data_cb(const uint8_t *buf, int len,
		  bool omit_zero_bytes,
		  void (*cb)(const char *buf, void *private_data),
		  void *private_data);

/**
 * Write dump of binary data to the log file.
 *
 * The data is only written if the log level is at least level.
 */
_PUBLIC_ void dump_data(int level, const uint8_t *buf,int len);

/**
 * Write dump of binary data to the log file.
 *
 * The data is only written if the log level is at least level.
 * 16 zero bytes in a row are omitted
 */
_PUBLIC_ void dump_data_skip_zeros(int level, const uint8_t *buf, int len);

/**
 malloc that aborts with smb_panic on fail or zero size.
**/
_PUBLIC_ void *smb_xmalloc(size_t size);

/**
 Memdup with smb_panic on fail.
**/
_PUBLIC_ void *smb_xmemdup(const void *p, size_t size);

/**
 strdup that aborts on malloc fail.
**/
_PUBLIC_ char *smb_xstrdup(const char *s);

char *smb_xstrndup(const char *s, size_t n);

/**
 Like strdup but for memory.
**/
_PUBLIC_ void *memdup(const void *p, size_t size);

/**
 * Write a password to the log file.
 *
 * @note Only actually does something if DEBUG_PASSWORD was defined during 
 * compile-time.
 */
_PUBLIC_ void dump_data_pw(const char *msg, const uint8_t * data, size_t len);

/**
 * see if a range of memory is all zero. A NULL pointer is considered
 * to be all zero 
 */
_PUBLIC_ bool all_zero(const uint8_t *ptr, size_t size);

/**
  realloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *realloc_array(void *ptr, size_t el_size, unsigned count, bool free_on_fail);

void *malloc_array(size_t el_size, unsigned int count);

/* The following definitions come from lib/util/fsusage.c  */


/**
 * Retrieve amount of free disk space.
 * this does all of the system specific guff to get the free disk space.
 * It is derived from code in the GNU fileutils package, but has been
 * considerably mangled for use here 
 *
 * results are returned in *dfree and *dsize, in 512 byte units
*/
_PUBLIC_ int sys_fsusage(const char *path, uint64_t *dfree, uint64_t *dsize);

/* The following definitions come from lib/util/ms_fnmatch.c  */


/**
 * @file
 * @brief MS-style Filename matching
 */

#if _SAMBA_BUILD_ == 4
/* protocol types. It assumes that higher protocols include lower protocols
   as subsets. FIXME: Move to one of the smb-specific headers */
enum protocol_types {
	PROTOCOL_NONE,
	PROTOCOL_CORE,
	PROTOCOL_COREPLUS,
	PROTOCOL_LANMAN1,
	PROTOCOL_LANMAN2,
	PROTOCOL_NT1,
	PROTOCOL_SMB2
};

int ms_fnmatch(const char *pattern, const char *string, enum protocol_types protocol);

/** a generic fnmatch function - uses for non-CIFS pattern matching */
int gen_fnmatch(const char *pattern, const char *string);
#endif

/* The following definitions come from lib/util/idtree.c  */


/**
  initialise a idr tree. The context return value must be passed to
  all subsequent idr calls. To destroy the idr tree use talloc_free()
  on this context
 */
_PUBLIC_ struct idr_context *idr_init(TALLOC_CTX *mem_ctx);

/**
  allocate the next available id, and assign 'ptr' into its slot.
  you can retrieve later this pointer using idr_find()
*/
_PUBLIC_ int idr_get_new(struct idr_context *idp, void *ptr, int limit);

/**
   allocate a new id, giving the first available value greater than or
   equal to the given starting id
*/
_PUBLIC_ int idr_get_new_above(struct idr_context *idp, void *ptr, int starting_id, int limit);

/**
  allocate a new id randomly in the given range
*/
_PUBLIC_ int idr_get_new_random(struct idr_context *idp, void *ptr, int limit);

/**
  find a pointer value previously set with idr_get_new given an id
*/
_PUBLIC_ void *idr_find(struct idr_context *idp, int id);

/**
  remove an id from the idr tree
*/
_PUBLIC_ int idr_remove(struct idr_context *idp, int id);

/* The following definitions come from lib/util/become_daemon.c  */

/**
 Close the low 3 fd's and open dev/null in their place
**/
_PUBLIC_ void close_low_fds(bool stderr_too);

/**
 Become a daemon, discarding the controlling terminal.
**/
_PUBLIC_ void become_daemon(bool do_fork, bool no_process_group, bool log_stdout);

/**
 * Load a ini-style file.
 */
bool pm_process( const char *fileName,
                 bool (*sfunc)(const char *, void *),
                 bool (*pfunc)(const char *, const char *, void *),
				 void *userdata);

bool unmap_file(void *start, size_t size);

void print_asc(int level, const uint8_t *buf,int len);
void print_asc_cb(const uint8_t *buf, int len,
		  void (*cb)(const char *buf, void *private_data),
		  void *private_data);

/**
 * Add an id to an array of ids.
 *
 * num should be a pointer to an integer that holds the current
 * number of elements in ids. It will be updated by this function.
 */

bool add_uid_to_array_unique(TALLOC_CTX *mem_ctx, uid_t uid,
			     uid_t **uids, uint32_t *num_uids);
bool add_gid_to_array_unique(TALLOC_CTX *mem_ctx, gid_t gid,
			     gid_t **gids, uint32_t *num_gids);

/**
 * Allocate anonymous shared memory of the given size
 */
void *anonymous_shared_allocate(size_t bufsz);
void anonymous_shared_free(void *ptr);

/*
  run a command as a child process, with a timeout.

  any stdout/stderr from the child will appear in the Samba logs with
  the specified log levels

  If callback is set then the callback is called on completion
  with the return code from the command
 */
struct tevent_context;
struct tevent_req;
struct tevent_req *samba_runcmd_send(TALLOC_CTX *mem_ctx,
				     struct tevent_context *ev,
				     struct timeval endtime,
				     int stdout_log_level,
				     int stderr_log_level,
				     const char * const *argv0, ...);
int samba_runcmd_recv(struct tevent_req *req, int *perrno);

#ifdef DEVELOPER
void samba_start_debugger(void);
#endif

#endif /* _SAMBA_UTIL_H_ */
