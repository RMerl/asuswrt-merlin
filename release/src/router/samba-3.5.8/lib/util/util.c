/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2002
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough (jmcd@us.ibm.com)  2003.
   Copyright (C) James J Myers 2003
   
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

#include "includes.h"
#include "system/network.h"
#include "system/filesys.h"
#include "system/locale.h"
#undef malloc
#undef strcasecmp
#undef strncasecmp
#undef strdup
#undef realloc

/**
 * @file
 * @brief Misc utility functions
 */

/**
 Find a suitable temporary directory. The result should be copied immediately
 as it may be overwritten by a subsequent call.
**/
_PUBLIC_ const char *tmpdir(void)
{
	char *p;
	if ((p = getenv("TMPDIR")))
		return p;
	return "/tmp";
}


/**
 Check if a file exists - call vfs_file_exist for samba files.
**/
_PUBLIC_ bool file_exist(const char *fname)
{
	struct stat st;

	if (stat(fname, &st) != 0) {
		return false;
	}

	return ((S_ISREG(st.st_mode)) || (S_ISFIFO(st.st_mode)));
}

/**
 Check a files mod time.
**/

_PUBLIC_ time_t file_modtime(const char *fname)
{
	struct stat st;
  
	if (stat(fname,&st) != 0) 
		return(0);

	return(st.st_mtime);
}

/**
 Check if a directory exists.
**/

_PUBLIC_ bool directory_exist(const char *dname)
{
	struct stat st;
	bool ret;

	if (stat(dname,&st) != 0) {
		return false;
	}

	ret = S_ISDIR(st.st_mode);
	if(!ret)
		errno = ENOTDIR;
	return ret;
}

/**
 * Try to create the specified directory if it didn't exist.
 *
 * @retval true if the directory already existed and has the right permissions 
 * or was successfully created.
 */
_PUBLIC_ bool directory_create_or_exist(const char *dname, uid_t uid, 
			       mode_t dir_perms)
{
	mode_t old_umask;
  	struct stat st;
      
	old_umask = umask(0);
	if (lstat(dname, &st) == -1) {
		if (errno == ENOENT) {
			/* Create directory */
			if (mkdir(dname, dir_perms) == -1) {
				DEBUG(0, ("error creating directory "
					  "%s: %s\n", dname, 
					  strerror(errno)));
				umask(old_umask);
				return false;
			}
		} else {
			DEBUG(0, ("lstat failed on directory %s: %s\n",
				  dname, strerror(errno)));
			umask(old_umask);
			return false;
		}
	} else {
		/* Check ownership and permission on existing directory */
		if (!S_ISDIR(st.st_mode)) {
			DEBUG(0, ("directory %s isn't a directory\n",
				dname));
			umask(old_umask);
			return false;
		}
		if (st.st_uid != uid && !uwrap_enabled()) {
			DEBUG(0, ("invalid ownership on directory "
				  "%s\n", dname));
			umask(old_umask);
			return false;
		}
		if ((st.st_mode & 0777) != dir_perms) {
			DEBUG(0, ("invalid permissions on directory "
				  "%s\n", dname));
			umask(old_umask);
			return false;
		}
	}
	return true;
}       


/**
 Sleep for a specified number of milliseconds.
**/

_PUBLIC_ void msleep(unsigned int t)
{
	struct timeval tval;  

	tval.tv_sec = t/1000;
	tval.tv_usec = 1000*(t%1000);
	/* this should be the real select - do NOT replace
	   with sys_select() */
	select(0,NULL,NULL,NULL,&tval);
}

/**
 Get my own name, return in talloc'ed storage.
**/

_PUBLIC_ char *get_myname(TALLOC_CTX *ctx)
{
	char *p;
	char hostname[HOST_NAME_MAX];

	/* get my host name */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return NULL;
	}

	/* Ensure null termination. */
	hostname[sizeof(hostname)-1] = '\0';

	/* split off any parts after an initial . */
	p = strchr_m(hostname, '.');
	if (p) {
		*p = 0;
	}

	return talloc_strdup(ctx, hostname);
}

/**
 Check if a process exists. Does this work on all unixes?
**/

_PUBLIC_ bool process_exists_by_pid(pid_t pid)
{
	/* Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want. */
	SMB_ASSERT(pid > 0);
	return(kill(pid,0) == 0 || errno != ESRCH);
}

/**
 Simple routine to do POSIX file locking. Cruft in NFS and 64->32 bit mapping
 is dealt with in posix.c
**/

_PUBLIC_ bool fcntl_lock(int fd, int op, off_t offset, off_t count, int type)
{
	struct flock lock;
	int ret;

	DEBUG(8,("fcntl_lock %d %d %.0f %.0f %d\n",fd,op,(double)offset,(double)count,type));

	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = offset;
	lock.l_len = count;
	lock.l_pid = 0;

	ret = fcntl(fd,op,&lock);

	if (ret == -1 && errno != 0)
		DEBUG(3,("fcntl_lock: fcntl lock gave errno %d (%s)\n",errno,strerror(errno)));

	/* a lock query */
	if (op == F_GETLK) {
		if ((ret != -1) &&
				(lock.l_type != F_UNLCK) && 
				(lock.l_pid != 0) && 
				(lock.l_pid != getpid())) {
			DEBUG(3,("fcntl_lock: fd %d is locked by pid %d\n",fd,(int)lock.l_pid));
			return true;
		}

		/* it must be not locked or locked by me */
		return false;
	}

	/* a lock set or unset */
	if (ret == -1) {
		DEBUG(3,("fcntl_lock: lock failed at offset %.0f count %.0f op %d type %d (%s)\n",
			(double)offset,(double)count,op,type,strerror(errno)));
		return false;
	}

	/* everything went OK */
	DEBUG(8,("fcntl_lock: Lock call successful\n"));

	return true;
}

void print_asc(int level, const uint8_t *buf,int len)
{
	int i;
	for (i=0;i<len;i++)
		DEBUGADD(level,("%c", isprint(buf[i])?buf[i]:'.'));
}

/**
 * Write dump of binary data to the log file.
 *
 * The data is only written if the log level is at least level.
 */
static void _dump_data(int level, const uint8_t *buf, int len,
		       bool omit_zero_bytes)
{
	int i=0;
	static const uint8_t empty[16] = { 0, };
	bool skipped = false;

	if (len<=0) return;

	if (!DEBUGLVL(level)) return;

	for (i=0;i<len;) {

		if (i%16 == 0) {
			if ((omit_zero_bytes == true) &&
			    (i > 0) &&
			    (len > i+16) &&
			    (memcmp(&buf[i], &empty, 16) == 0))
			{
				i +=16;
				continue;
			}

			if (i<len)  {
				DEBUGADD(level,("[%04X] ",i));
			}
		}

		DEBUGADD(level,("%02X ",(int)buf[i]));
		i++;
		if (i%8 == 0) DEBUGADD(level,("  "));
		if (i%16 == 0) {

			print_asc(level,&buf[i-16],8); DEBUGADD(level,(" "));
			print_asc(level,&buf[i-8],8); DEBUGADD(level,("\n"));

			if ((omit_zero_bytes == true) &&
			    (len > i+16) &&
			    (memcmp(&buf[i], &empty, 16) == 0)) {
				if (!skipped) {
					DEBUGADD(level,("skipping zero buffer bytes\n"));
					skipped = true;
				}
			}
		}
	}

	if (i%16) {
		int n;
		n = 16 - (i%16);
		DEBUGADD(level,(" "));
		if (n>8) DEBUGADD(level,(" "));
		while (n--) DEBUGADD(level,("   "));
		n = MIN(8,i%16);
		print_asc(level,&buf[i-(i%16)],n); DEBUGADD(level,( " " ));
		n = (i%16) - n;
		if (n>0) print_asc(level,&buf[i-n],n);
		DEBUGADD(level,("\n"));
	}

}

/**
 * Write dump of binary data to the log file.
 *
 * The data is only written if the log level is at least level.
 */
_PUBLIC_ void dump_data(int level, const uint8_t *buf, int len)
{
	_dump_data(level, buf, len, false);
}

/**
 * Write dump of binary data to the log file.
 *
 * The data is only written if the log level is at least level.
 * 16 zero bytes in a row are ommited
 */
_PUBLIC_ void dump_data_skip_zeros(int level, const uint8_t *buf, int len)
{
	_dump_data(level, buf, len, true);
}


/**
 malloc that aborts with smb_panic on fail or zero size.
**/

_PUBLIC_ void *smb_xmalloc(size_t size)
{
	void *p;
	if (size == 0)
		smb_panic("smb_xmalloc: called with zero size.\n");
	if ((p = malloc(size)) == NULL)
		smb_panic("smb_xmalloc: malloc fail.\n");
	return p;
}

/**
 Memdup with smb_panic on fail.
**/

_PUBLIC_ void *smb_xmemdup(const void *p, size_t size)
{
	void *p2;
	p2 = smb_xmalloc(size);
	memcpy(p2, p, size);
	return p2;
}

/**
 strdup that aborts on malloc fail.
**/

char *smb_xstrdup(const char *s)
{
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strdup
#undef strdup
#endif
#endif

#ifndef HAVE_STRDUP
#define strdup rep_strdup
#endif

	char *s1 = strdup(s);
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strdup
#undef strdup
#endif
#define strdup(s) __ERROR_DONT_USE_STRDUP_DIRECTLY
#endif
	if (!s1) {
		smb_panic("smb_xstrdup: malloc failed");
	}
	return s1;

}

/**
 strndup that aborts on malloc fail.
**/

char *smb_xstrndup(const char *s, size_t n)
{
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strndup
#undef strndup
#endif
#endif

#if (defined(BROKEN_STRNDUP) || !defined(HAVE_STRNDUP))
#undef HAVE_STRNDUP
#define strndup rep_strndup
#endif

	char *s1 = strndup(s, n);
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strndup
#undef strndup
#endif
#define strndup(s,n) __ERROR_DONT_USE_STRNDUP_DIRECTLY
#endif
	if (!s1) {
		smb_panic("smb_xstrndup: malloc failed");
	}
	return s1;
}



/**
 Like strdup but for memory.
**/

_PUBLIC_ void *memdup(const void *p, size_t size)
{
	void *p2;
	if (size == 0)
		return NULL;
	p2 = malloc(size);
	if (!p2)
		return NULL;
	memcpy(p2, p, size);
	return p2;
}

/**
 * Write a password to the log file.
 *
 * @note Only actually does something if DEBUG_PASSWORD was defined during 
 * compile-time.
 */
_PUBLIC_ void dump_data_pw(const char *msg, const uint8_t * data, size_t len)
{
#ifdef DEBUG_PASSWORD
	DEBUG(11, ("%s", msg));
	if (data != NULL && len > 0)
	{
		dump_data(11, data, len);
	}
#endif
}


/**
 * see if a range of memory is all zero. A NULL pointer is considered
 * to be all zero 
 */
_PUBLIC_ bool all_zero(const uint8_t *ptr, size_t size)
{
	int i;
	if (!ptr) return true;
	for (i=0;i<size;i++) {
		if (ptr[i]) return false;
	}
	return true;
}

/**
  realloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *realloc_array(void *ptr, size_t el_size, unsigned count, bool free_on_fail)
{
#define MAX_MALLOC_SIZE 0x7fffffff
	if (count == 0 ||
	    count >= MAX_MALLOC_SIZE/el_size) {
		if (free_on_fail)
			SAFE_FREE(ptr);
		return NULL;
	}
	if (!ptr) {
		return malloc(el_size * count);
	}
	return realloc(ptr, el_size * count);
}

/****************************************************************************
 Type-safe malloc.
****************************************************************************/

void *malloc_array(size_t el_size, unsigned int count)
{
	return realloc_array(NULL, el_size, count, false);
}

/**
 Trim the specified elements off the front and back of a string.
**/
_PUBLIC_ bool trim_string(char *s, const char *front, const char *back)
{
	bool ret = false;
	size_t front_len;
	size_t back_len;
	size_t len;

	/* Ignore null or empty strings. */
	if (!s || (s[0] == '\0'))
		return false;

	front_len	= front? strlen(front) : 0;
	back_len	= back? strlen(back) : 0;

	len = strlen(s);

	if (front_len) {
		while (len && strncmp(s, front, front_len)==0) {
			/* Must use memmove here as src & dest can
			 * easily overlap. Found by valgrind. JRA. */
			memmove(s, s+front_len, (len-front_len)+1);
			len -= front_len;
			ret=true;
		}
	}
	
	if (back_len) {
		while ((len >= back_len) && strncmp(s+len-back_len,back,back_len)==0) {
			s[len-back_len]='\0';
			len -= back_len;
			ret=true;
		}
	}
	return ret;
}

/**
 Find the number of 'c' chars in a string
**/
_PUBLIC_ _PURE_ size_t count_chars(const char *s, char c)
{
	size_t count = 0;

	while (*s) {
		if (*s == c) count++;
		s ++;
	}

	return count;
}

/**
 Routine to get hex characters and turn them into a 16 byte array.
 the array can be variable length, and any non-hex-numeric
 characters are skipped.  "0xnn" or "0Xnn" is specially catered
 for.

 valid examples: "0A5D15"; "0x15, 0x49, 0xa2"; "59\ta9\te3\n"


**/
_PUBLIC_ size_t strhex_to_str(char *p, size_t p_len, const char *strhex, size_t strhex_len)
{
	size_t i;
	size_t num_chars = 0;
	uint8_t   lonybble, hinybble;
	const char     *hexchars = "0123456789ABCDEF";
	char           *p1 = NULL, *p2 = NULL;

	for (i = 0; i < strhex_len && strhex[i] != 0; i++) {
		if (strncasecmp(hexchars, "0x", 2) == 0) {
			i++; /* skip two chars */
			continue;
		}

		if (!(p1 = strchr(hexchars, toupper((unsigned char)strhex[i]))))
			break;

		i++; /* next hex digit */

		if (!(p2 = strchr(hexchars, toupper((unsigned char)strhex[i]))))
			break;

		/* get the two nybbles */
		hinybble = PTR_DIFF(p1, hexchars);
		lonybble = PTR_DIFF(p2, hexchars);

		if (num_chars >= p_len) {
			break;
		}

		p[num_chars] = (hinybble << 4) | lonybble;
		num_chars++;

		p1 = NULL;
		p2 = NULL;
	}
	return num_chars;
}

/** 
 * Parse a hex string and return a data blob. 
 */
_PUBLIC_ _PURE_ DATA_BLOB strhex_to_data_blob(TALLOC_CTX *mem_ctx, const char *strhex) 
{
	DATA_BLOB ret_blob = data_blob_talloc(mem_ctx, NULL, strlen(strhex)/2+1);

	ret_blob.length = strhex_to_str((char *)ret_blob.data, ret_blob.length,
					strhex,
					strlen(strhex));

	return ret_blob;
}


/**
 * Routine to print a buffer as HEX digits, into an allocated string.
 */
_PUBLIC_ void hex_encode(const unsigned char *buff_in, size_t len, char **out_hex_buffer)
{
	int i;
	char *hex_buffer;

	*out_hex_buffer = malloc_array_p(char, (len*2)+1);
	hex_buffer = *out_hex_buffer;

	for (i = 0; i < len; i++)
		slprintf(&hex_buffer[i*2], 3, "%02X", buff_in[i]);
}

/**
 * talloc version of hex_encode()
 */
_PUBLIC_ char *hex_encode_talloc(TALLOC_CTX *mem_ctx, const unsigned char *buff_in, size_t len)
{
	int i;
	char *hex_buffer;

	hex_buffer = talloc_array(mem_ctx, char, (len*2)+1);
	if (!hex_buffer) {
		return NULL;
	}

	for (i = 0; i < len; i++)
		slprintf(&hex_buffer[i*2], 3, "%02X", buff_in[i]);

	talloc_set_name_const(hex_buffer, hex_buffer);
	return hex_buffer;
}

/**
  varient of strcmp() that handles NULL ptrs
**/
_PUBLIC_ int strcmp_safe(const char *s1, const char *s2)
{
	if (s1 == s2) {
		return 0;
	}
	if (s1 == NULL || s2 == NULL) {
		return s1?-1:1;
	}
	return strcmp(s1, s2);
}


/**
return the number of bytes occupied by a buffer in ASCII format
the result includes the null termination
limited by 'n' bytes
**/
_PUBLIC_ size_t ascii_len_n(const char *src, size_t n)
{
	size_t len;

	len = strnlen(src, n);
	if (len+1 <= n) {
		len += 1;
	}

	return len;
}

/**
 Set a boolean variable from the text value stored in the passed string.
 Returns true in success, false if the passed string does not correctly 
 represent a boolean.
**/

_PUBLIC_ bool set_boolean(const char *boolean_string, bool *boolean)
{
	if (strwicmp(boolean_string, "yes") == 0 ||
	    strwicmp(boolean_string, "true") == 0 ||
	    strwicmp(boolean_string, "on") == 0 ||
	    strwicmp(boolean_string, "1") == 0) {
		*boolean = true;
		return true;
	} else if (strwicmp(boolean_string, "no") == 0 ||
		   strwicmp(boolean_string, "false") == 0 ||
		   strwicmp(boolean_string, "off") == 0 ||
		   strwicmp(boolean_string, "0") == 0) {
		*boolean = false;
		return true;
	}
	return false;
}

/**
return the number of bytes occupied by a buffer in CH_UTF16 format
the result includes the null termination
**/
_PUBLIC_ size_t utf16_len(const void *buf)
{
	size_t len;

	for (len = 0; SVAL(buf,len); len += 2) ;

	return len + 2;
}

/**
return the number of bytes occupied by a buffer in CH_UTF16 format
the result includes the null termination
limited by 'n' bytes
**/
_PUBLIC_ size_t utf16_len_n(const void *src, size_t n)
{
	size_t len;

	for (len = 0; (len+2 < n) && SVAL(src, len); len += 2) ;

	if (len+2 <= n) {
		len += 2;
	}

	return len;
}

/**
 * @file
 * @brief String utilities.
 **/

static bool next_token_internal_talloc(TALLOC_CTX *ctx,
				const char **ptr,
                                char **pp_buff,
                                const char *sep,
                                bool ltrim)
{
	char *s;
	char *saved_s;
	char *pbuf;
	bool quoted;
	size_t len=1;

	*pp_buff = NULL;
	if (!ptr) {
		return(false);
	}

	s = (char *)*ptr;

	/* default to simple separators */
	if (!sep) {
		sep = " \t\n\r";
	}

	/* find the first non sep char, if left-trimming is requested */
	if (ltrim) {
		while (*s && strchr_m(sep,*s)) {
			s++;
		}
	}

	/* nothing left? */
	if (!*s) {
		return false;
	}

	/* When restarting we need to go from here. */
	saved_s = s;

	/* Work out the length needed. */
	for (quoted = false; *s &&
			(quoted || !strchr_m(sep,*s)); s++) {
		if (*s == '\"') {
			quoted = !quoted;
		} else {
			len++;
		}
	}

	/* We started with len = 1 so we have space for the nul. */
	*pp_buff = talloc_array(ctx, char, len);
	if (!*pp_buff) {
		return false;
	}

	/* copy over the token */
	pbuf = *pp_buff;
	s = saved_s;
	for (quoted = false; *s &&
			(quoted || !strchr_m(sep,*s)); s++) {
		if ( *s == '\"' ) {
			quoted = !quoted;
		} else {
			*pbuf++ = *s;
		}
	}

	*ptr = (*s) ? s+1 : s;
	*pbuf = 0;

	return true;
}

bool next_token_talloc(TALLOC_CTX *ctx,
			const char **ptr,
			char **pp_buff,
			const char *sep)
{
	return next_token_internal_talloc(ctx, ptr, pp_buff, sep, true);
}

/*
 * Get the next token from a string, return false if none found.  Handles
 * double-quotes.  This version does not trim leading separator characters
 * before looking for a token.
 */

bool next_token_no_ltrim_talloc(TALLOC_CTX *ctx,
			const char **ptr,
			char **pp_buff,
			const char *sep)
{
	return next_token_internal_talloc(ctx, ptr, pp_buff, sep, false);
}


