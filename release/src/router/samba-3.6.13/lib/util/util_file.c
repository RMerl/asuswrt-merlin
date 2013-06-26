/*
 * Unix SMB/CIFS implementation.
 * SMB parameters and setup
 * Copyright (C) Andrew Tridgell 1992-1998 Modified by Jeremy Allison 1995.
 *
 * Added afdgets() Jelmer Vernooij 2005
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "system/shmem.h"
#include "system/filesys.h"
#if _SAMBA_BUILD_ == 3
#undef malloc
#undef realloc
#endif

/**
 * @file
 * @brief File-related utility functions
 */

/**
read a line from a file with possible \ continuation chars. 
Blanks at the start or end of a line are stripped.
The string will be allocated if s2 is NULL
**/
_PUBLIC_ char *fgets_slash(char *s2,int maxlen,XFILE *f)
{
  char *s=s2;
  int len = 0;
  int c;
  bool start_of_line = true;

  if (x_feof(f))
    return(NULL);

  if (maxlen <2) return(NULL);

  if (!s2)
    {
      maxlen = MIN(maxlen,8);
      s = (char *)malloc(maxlen);
    }

  if (!s) return(NULL);

  *s = 0;

  while (len < maxlen-1)
    {
      c = x_getc(f);
      switch (c)
	{
	case '\r':
	  break;
	case '\n':
	  while (len > 0 && s[len-1] == ' ')
	    {
	      s[--len] = 0;
	    }
	  if (len > 0 && s[len-1] == '\\')
	    {
	      s[--len] = 0;
	      start_of_line = true;
	      break;
	    }
	  return(s);
	case EOF:
	  if (len <= 0 && !s2) 
	    SAFE_FREE(s);
	  return(len>0?s:NULL);
	case ' ':
	  if (start_of_line)
	    break;
	  /* fall through */
	default:
	  start_of_line = false;
	  s[len++] = c;
	  s[len] = 0;
	}
      if (!s2 && len > maxlen-3)
	{
	  char *t;
	  
	  maxlen *= 2;
	  t = realloc_p(s, char, maxlen);
	  if (!t) {
	    DEBUG(0,("fgets_slash: failed to expand buffer!\n"));
	    SAFE_FREE(s);
	    return(NULL);
	  } else s = t;
	}
    }
  return(s);
}

/**
 * Read one line (data until next newline or eof) and allocate it 
 */
_PUBLIC_ char *afdgets(int fd, TALLOC_CTX *mem_ctx, size_t hint)
{
	char *data = NULL;
	ssize_t alloc_size = 0, offset = 0, ret;
	int p;

	if (hint <= 0) hint = 0x100;

	do {
		alloc_size += hint;

		data = talloc_realloc(mem_ctx, data, char, alloc_size);

		if (!data)
			return NULL;

		ret = read(fd, data + offset, hint);

		if (ret == 0) {
			return NULL;
		}

		if (ret == -1) {
			talloc_free(data);
			return NULL;
		}

		/* Find newline */
		for (p = 0; p < ret; p++) {
			if (data[offset + p] == '\n')
				break;
		}

		if (p < ret) {
			data[offset + p] = '\0';

			/* Go back to position of newline */
			lseek(fd, p - ret + 1, SEEK_CUR);
			return data;
		}

		offset += ret;

	} while (ret == hint);

	data[offset] = '\0';

	return data;
}


/**
load a file into memory from a fd.
**/
_PUBLIC_ char *fd_load(int fd, size_t *psize, size_t maxsize, TALLOC_CTX *mem_ctx)
{
	struct stat sbuf;
	char *p;
	size_t size;

	if (fstat(fd, &sbuf) != 0) return NULL;

	size = sbuf.st_size;

	if (maxsize) {
		size = MIN(size, maxsize);
	}

	p = (char *)talloc_size(mem_ctx, size+1);
	if (!p) return NULL;

	if (read(fd, p, size) != size) {
		talloc_free(p);
		return NULL;
	}
	p[size] = 0;

	if (psize) *psize = size;

	return p;
}

/**
load a file into memory
**/
_PUBLIC_ char *file_load(const char *fname, size_t *size, size_t maxsize, TALLOC_CTX *mem_ctx)
{
	int fd;
	char *p;

	if (!fname || !*fname) return NULL;
	
	fd = open(fname,O_RDONLY);
	if (fd == -1) return NULL;

	p = fd_load(fd, size, maxsize, mem_ctx);

	close(fd);

	return p;
}


/**
mmap (if possible) or read a file
**/
_PUBLIC_ void *map_file(const char *fname, size_t size)
{
	size_t s2 = 0;
	void *p = NULL;
#ifdef HAVE_MMAP
	int fd;
	fd = open(fname, O_RDONLY, 0);
	if (fd == -1) {
		DEBUG(2,("Failed to load %s - %s\n", fname, strerror(errno)));
		return NULL;
	}
	p = mmap(NULL, size, PROT_READ, MAP_SHARED|MAP_FILE, fd, 0);
	close(fd);
	if (p == MAP_FAILED) {
		DEBUG(1,("Failed to mmap %s - %s\n", fname, strerror(errno)));
		return NULL;
	}
#endif
	if (!p) {
		p = file_load(fname, &s2, 0, NULL);
		if (!p) return NULL;
		if (s2 != size) {
			DEBUG(1,("incorrect size for %s - got %d expected %d\n",
				 fname, (int)s2, (int)size));
			talloc_free(p);
			return NULL;
		}
	}

	return p;
}

/**
 unmap or free memory
**/

bool unmap_file(void *start, size_t size)
{
#ifdef HAVE_MMAP
	if (munmap( start, size ) != 0) {
		DEBUG( 1, ("map_file: Failed to unmap address %p "
			"of size %u - %s\n", 
			start, (unsigned int)size, strerror(errno) ));
		return false;
	}
	return true;
#else
	talloc_free(start);
	return true;
#endif
}

/**
parse a buffer into lines
'p' will be freed on error, and otherwise will be made a child of the returned array
**/
char **file_lines_parse(char *p, size_t size, int *numlines, TALLOC_CTX *mem_ctx)
{
	int i;
	char *s, **ret;

	if (!p) return NULL;

	for (s = p, i=0; s < p+size; s++) {
		if (s[0] == '\n') i++;
	}

	ret = talloc_array(mem_ctx, char *, i+2);
	if (!ret) {
		talloc_free(p);
		return NULL;
	}	
	
	talloc_steal(ret, p);
	
	memset(ret, 0, sizeof(ret[0])*(i+2));

	ret[0] = p;
	for (s = p, i=0; s < p+size; s++) {
		if (s[0] == '\n') {
			s[0] = 0;
			i++;
			ret[i] = s+1;
		}
		if (s[0] == '\r') s[0] = 0;
	}

	/* remove any blank lines at the end */
	while (i > 0 && ret[i-1][0] == 0) {
		i--;
	}

	if (numlines) *numlines = i;

	return ret;
}


/**
load a file into memory and return an array of pointers to lines in the file
must be freed with talloc_free(). 
**/
_PUBLIC_ char **file_lines_load(const char *fname, int *numlines, size_t maxsize, TALLOC_CTX *mem_ctx)
{
	char *p;
	size_t size;

	p = file_load(fname, &size, maxsize, mem_ctx);
	if (!p) return NULL;

	return file_lines_parse(p, size, numlines, mem_ctx);
}

/**
load a fd into memory and return an array of pointers to lines in the file
must be freed with talloc_free(). If convert is true calls unix_to_dos on
the list.
**/
_PUBLIC_ char **fd_lines_load(int fd, int *numlines, size_t maxsize, TALLOC_CTX *mem_ctx)
{
	char *p;
	size_t size;

	p = fd_load(fd, &size, maxsize, mem_ctx);
	if (!p) return NULL;

	return file_lines_parse(p, size, numlines, mem_ctx);
}


/**
take a list of lines and modify them to produce a list where \ continues
a line
**/
_PUBLIC_ void file_lines_slashcont(char **lines)
{
	int i, j;

	for (i=0; lines[i];) {
		int len = strlen(lines[i]);
		if (lines[i][len-1] == '\\') {
			lines[i][len-1] = ' ';
			if (lines[i+1]) {
				char *p = &lines[i][len];
				while (p < lines[i+1]) *p++ = ' ';
				for (j = i+1; lines[j]; j++) lines[j] = lines[j+1];
			}
		} else {
			i++;
		}
	}
}

/**
  save a lump of data into a file. Mostly used for debugging 
*/
_PUBLIC_ bool file_save(const char *fname, const void *packet, size_t length)
{
	int fd;
	fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		return false;
	}
	if (write(fd, packet, length) != (size_t)length) {
		close(fd);
		return false;
	}
	close(fd);
	return true;
}

_PUBLIC_ int vfdprintf(int fd, const char *format, va_list ap)
{
	char *p;
	int len, ret;
	va_list ap2;

	va_copy(ap2, ap);
	len = vasprintf(&p, format, ap2);
	va_end(ap2);
	if (len <= 0) return len;
	ret = write(fd, p, len);
	SAFE_FREE(p);
	return ret;
}

_PUBLIC_ int fdprintf(int fd, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vfdprintf(fd, format, ap);
	va_end(ap);
	return ret;
}


/*
  try to determine if the filesystem supports large files
*/
_PUBLIC_ bool large_file_support(const char *path)
{
	int fd;
	ssize_t ret;
	char c;

	fd = open(path, O_RDWR|O_CREAT, 0600);
	unlink(path);
	if (fd == -1) {
		/* have to assume large files are OK */
		return true;
	}
	ret = pread(fd, &c, 1, ((uint64_t)1)<<32);
	close(fd);
	return ret == 0;
}


/*
  compare two files, return true if the two files have the same content
 */
bool file_compare(const char *path1, const char *path2)
{
	size_t size1, size2;
	char *p1, *p2;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	p1 = file_load(path1, &size1, 0, mem_ctx);
	p2 = file_load(path2, &size2, 0, mem_ctx);
	if (!p1 || !p2 || size1 != size2) {
		talloc_free(mem_ctx);
		return false;
	}
	if (memcmp(p1, p2, size1) != 0) {
		talloc_free(mem_ctx);
		return false;
	}
	talloc_free(mem_ctx);
	return true;
}
