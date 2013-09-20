#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pwd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "dutil.h"

/*
 * Wrapper for malloc
 */
void *
xmalloc(size_t size)
{
	void *ret=NULL;
	
	if (size == 0) {
		fprintf(stderr, "Cannot allocate buffer of size 0.\n");
		abort();
	}
	ret = malloc(size);
	if (!ret) {
		perror("dlib-xmalloc");
		abort();
	}
	memset(ret, '\0', size);
	return ret;
}

/*
 * Wrapper for realloc
 */
void *
xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (!ret) {
		perror("dlib-xrealloc");
		abort();
	}
	return ret;
}

/*
 * Wrapper for strdup
 */
char *
xstrdup(const char *str)
{
	char *ret=NULL;
	if (str) {
		ret = strdup(str);
		if (!ret) {
			perror("dlib-xstrdup");
			abort();
		}
	}
	return ret;
}

/*
 * Wrapper for free
 */
void 
__xfree(void *ptr)
{
	if (ptr) {
		free(ptr);
	}
}

/**
 * Removes any occurance of the passed in items
 *
 * Params
 * str - The string to implode
 * items - Items to remove from string
 *
 * Returns
 * The length of the new string
 */
size_t
strstrip(char *str, char *items)
{
	size_t newlen=0;
	size_t len=strlen(str);
	char *curr=str, *itemptr;

	/*
	 * Loop through the passed in string, if we find
	 * any occurance of the items passed in, cover it
	 * with the remaining portion of the string. This will
	 * always include the NUL character. If one of the not
	 * items is not found, then we'll just pass over it and
	 * allow it to remain in the string.
	 */
	while (*curr != '\0') {
		itemptr = items;
		while (*itemptr != '\0') {
			if (*curr == *itemptr) {
				memmove(curr, curr+1, len);
				break;
			}
			itemptr++;
		}
		// We went through the whole thing and didn't find anything.
		if (*itemptr == '\0') {
			curr++;
			newlen++;
		}
		len--;
	}
	return newlen;
}

/**
 * Gets a sub string based upon a starting position
 * and a length of the sub string you want. The start
 * can be a negative number. If it is, it will wrap
 * around to the end of the string. In other words,
 * -1 is really str[len - 1].  
 *
 *  Params
 *  str - The string to find the sub string in.
 *  start - The starting position.
 *  len - The length of the sub string.
 *
 *  Return
 *  A malloc'd string which is the sub string.
 */
char *
substr(const char *str, int start, size_t len)
{
	size_t slen=0;
	char *ret=NULL;
	
	if (len == 0) {
		return NULL;
	}
	slen = strlen(str);
	ret = xmalloc(len + 1);

	/* A negative number means to wrap.
	 * So, if the string is "Dean" and the caller 
	 * specified the start pos as -1, the starting
	 * position is the letter 'n'. So, we need to
	 * get to that postion appropriately.
	 */
	if (start < 0) {
		start += slen;
	}

	/* Make sure we don't go past the end of the string. */
	if ((start+len) > slen) {
		len = slen - start;
	}
	memcpy(ret, str+start, len);
	return ret;
}

/**
 * Return the next prime number out of the number from the
 * input integer.
 *
 * Params
 * 	n - The number to find the next prime from
 *
 * Return
 * 	The next prime number of n
 */
int
nextprime(int n)
{
	int i, div, ceilsqrt = 0;
	int retval=0;

	for (;; n++) {
		ceilsqrt = ceil(sqrt(n));
		for (i = 2; i <= ceilsqrt; i++) {
			div = n / i;
			if (div * i == n) {
				retval = n;
				break;
			}
		}
		if (retval) {
			break;
		}
	}

	return retval;
}

/**
 * Finds the first occurance of a character in a string and 
 * returns the index to where it is.
 */
int
strfind(const char *str, char ch)
{
	int i;
	for (i=0; *str != '\0'; i++, str++) {
		if (*str == ch) {
			return i;
		}
	}
	return -1;
}

static void
__strexplodeDestr(void *ptr)
{
	xfree(ptr);
}

/**
 * Takes a string and breaks it up based on a specific character
 * and returns it in a dvector
 */
dvector
explode(const char *str, const char *delim)
{
	bool found=false;
	const char *ptr=str, *dtmp=NULL;
	dstrbuf *buf = dsbNew(100);
	dvector vec = dvCreate(5, __strexplodeDestr);

	while (*ptr != '\0') {
		dtmp = delim;
		while (*dtmp != '\0') {
			if (*ptr == *dtmp) {
				if (buf->len > 0) {
					dvAddItem(&vec, xstrdup(buf->str));
					dsbClear(buf);
				}
				found = true;
				break;
			}
			dtmp++;
		}
		if (!found) {
			dsbCatChar(buf, *ptr);
		} else {
			found = false;
		}
		ptr++;
	}
	/* Add the last element if there was one. */
	if (buf->len > 0) {
		dvAddItem(&vec, xstrdup(buf->str));
	}
	dsbDestroy(buf);
	return vec;
}

/**
 * Takes a dvector and implodes it to a string based on a specific 
 * character to delimit by.
 * Returns a dstrbuf
 */
dstrbuf *
implode(dvector vec, char delim)
{
	dstrbuf *buf = dsbNew(100);
	size_t veclen = dvLength(vec);
	uint i=0;

	for (i=0; i < veclen; i++) {
		dsbCat(buf, vec[i]);
		if ((i+1) < veclen) {
			/* Only append a ',' if we're not at the end. */
			dsbCat(buf, &delim);
		}
	}
	return buf;
}

/**
 * Removes the \r and \n at the end of the line provided.
 * This gets rid of standards UNIX \n line endings and 
 * also DOS CRLF or \r\n line endings.
 */
void
chomp(char *str)
{
	char *cp;

	if (str && (cp = strrchr(str, '\n'))) {
		*cp = '\0';
	}
	if (str && (cp = strrchr(str, '\r'))) {
		*cp = '\0';
	}
}


/**
 * Returns the size of a file.
 */
size_t
filesize(const char *file)
{
	struct stat sb;

	memset(&sb, 0, sizeof(struct stat));
	if (stat(file, &sb) < 0) {
		return -1;
	}
	return sb.st_size;
}

