#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "dstrbuf.h"
#include "dutil.h"

/*
 * Create a new dstrbuf.
 *
 * Params
 * size - The initial size of the dstrbuf
 * maxsize - The maximum size the dstrbuf can grow to (0 for infinite)
 *
 * Return
 * A new dstrbuf
 */
dstrbuf *
dsbNew(size_t size)
{
	dstrbuf *ret = xmalloc(sizeof(struct __dstrbuf));
	ret->str = xmalloc(size + 1);
	ret->size = size;
	ret->len = 0;
	return ret;
}

/*
 * Resize the string
 *
 * Params
 * dsb - The dstrbuf to grow
 * newsize - The new size of the string
 */
void
dsbResize(dstrbuf *dsb, size_t newsize)
{
	assert(dsb != NULL);
	dsb->str = xrealloc(dsb->str, newsize + 1);
	dsb->str[newsize] = '\0';
	dsb->size = newsize;
}

/*
 * Destroy a dstrbuf (Free memory and NULL)
 *
 * Params
 * dsb - The dstrbuf to destroy
 */
void 
dsbDestroy(dstrbuf *dsb)
{
	if (dsb) {
		xfree(dsb->str);
		xfree(dsb);
	}
}

/*
 * Clear a dstrbuf (zero buffer memory)
 *
 * Params
 * dsb - dstrbuf to clear
 */
void 
dsbClear(dstrbuf *dsb)
{
	assert(dsb != NULL);
	memset(dsb->str, '\0', dsb->size);
	dsb->len = 0;
}

/*
 * Copy a char buffer into a dstrbuf
 *
 * Params
 * dsb - Destination dstrbuf
 * buf - Buffer to copy from
 * size - total bytes to copy
 *
 * Return
 * Total size that was copied. If size is greater than
 * maxsize, then only maxsize will be copied.
 */
size_t 
dsbCopy(dstrbuf *dsb, const char *buf)
{
	size_t length = strlen(buf);

	assert(dsb != NULL);
	if (length > dsb->size) {
		dsbResize(dsb, length);
	}
	memcpy(dsb->str, buf, length);
	dsb->len = length;
	dsb->str[dsb->len] = '\0';
	return length;
}

/*
 * Copy a char buffer into a dstrbuf up to size bytes
 *
 * Params
 * dsb - Destination dstrbuf
 * buf - Buffer to copy from
 * size - total bytes to copy
 *
 * Return
 * Total size that was copied. If size is greater than
 * maxsize, then only maxsize will be copied.
 */
size_t 
dsbnCopy(dstrbuf *dsb, const char *buf, size_t size)
{
	size_t len = strlen(buf);

	assert(dsb != NULL);
	if (len < size) {
		size = len;
	}
	if (size > dsb->size) {
		dsbResize(dsb, size);
	}
	memcpy(dsb->str, buf, size);
	dsb->len = size;
	dsb->str[dsb->len] = '\0';
	return size;
}

/*
 * Concat data onto the end of a dstrbuf buffer
 *
 * Params
 * dest - The dstrbuf to concat data to
 * src - The char buf to copy from
 *
 * Return
 * total chars (bytes) copied
 */
size_t 
dsbCat(dstrbuf *dest, const char *src)
{
	size_t length = strlen(src);
	size_t totalSize = 0;

	assert(dest != NULL);
	totalSize = dest->len + length;
	if (totalSize > dest->size) {
		dsbResize(dest, totalSize);
	}
	memcpy(dest->str + dest->len, src, length);
	dest->len += length;
	dest->str[dest->len] = '\0';
	return length;
}

/*
 * Concat data onto the end of a dstrbuf buffer up to size bytes
 *
 * Params
 * dest - The dstrbuf to concat data to
 * src - The char buf to copy from
 * size - total bytes to copy
 *
 * Return
 * total chars (bytes) copied
 */
size_t 
dsbnCat(dstrbuf *dest, const char *src, size_t size)
{
	size_t totalSize = 0;
	size_t length = strlen(src);

	assert(dest != NULL);
	if (length < size) {
		size = length;
	}
	totalSize = dest->len + size;
	if (totalSize > dest->size) {
		dsbResize(dest, totalSize);
	}
	memcpy(dest->str + dest->len, src, size);
	dest->len = totalSize;
	dest->str[dest->len] = '\0';
	return size;
}

/*
 * Appends a character to the buffer.
 *
 * Params
 * dsb - The dsb to concat to
 * ch  - The character to concat
 */
void
dsbCatChar(dstrbuf *dest, const u_char ch)
{
	if (dest->len+sizeof(u_char) >= dest->size) {
		dsbResize(dest, dest->len+sizeof(u_char));
	}
	dest->str[dest->len] = ch;
	dest->len++;
	dest->str[dest->len] = '\0';
}

/*
 * Similar to sprintf but appends to the end of the string.
 *
 * Params
 * dsb - The dsb to concat to
 * fmt - The format of the string
 * ... - arguments
 *
 * Return
 * total number of bytes written, -1 if vsnprintf error
 */
int 
dsbPrintf(dstrbuf *dsb, const char *fmt, ...)
{
	va_list ap;
	int actualLen=0, size;

	assert(dsb != NULL);
	while (true) {
		va_start(ap, fmt);
		size = dsb->size - dsb->len;
		actualLen = vsnprintf(dsb->str+dsb->len, size, fmt, ap);
		va_end(ap);
		// Check for failure to write
		if (actualLen > -1 && actualLen < size) {
			// String written properly.
			break;
		} else if (actualLen > -1) {
			size = dsb->size + (actualLen - size);
			dsbResize(dsb, size + 1);
		} else {
			dsbResize(dsb, dsb->size * 2);
		}
	}
	dsb->len += actualLen;
	return actualLen;
}

/*
 * Reads a line from a FILE
 *
 * Params
 * dsb - The dstrbuf to store the data into
 * file - a FILE to read the data from
 *
 * Returns
 * number of bytes read.
 */
size_t
dsbReadline(dstrbuf *dsb, FILE *file)
{
	int ch=0;
	size_t totalLen=0;

	assert(dsb != NULL);
	dsbClear(dsb);
	while ((ch = fgetc(file)) != EOF) {
		totalLen++;
		dsbCatChar(dsb, (char)ch);
		if (ch == '\n') {
			break;
		}
	}
	return totalLen;
}

/*
 * Reads a chunk of data from a FILE
 *
 * Params
 * dsb - The dstrbuf to store the data into
 * bytes - number of bytes to read.
 * file - a FILE to read the data from
 *
 * Returns
 * number of bytes read.
 */
size_t
dsbFread(dstrbuf *dsb, size_t bytes, FILE *file)
{
	size_t actualLen = 0;

	assert(dsb != NULL);
	if (bytes > dsb->size) {
		dsbResize(dsb, bytes);
	}
	actualLen = fread(dsb->str, 1, bytes, file);
	dsb->len = actualLen;
	dsb->str[dsb->len] = '\0';
	return actualLen;
}

/*
 * Reads data from a file
 *
 * Params
 * dsb - The dstrbuf to store the data in
 * size - The size of the data to read
 * fd - A standard unix file descriptor
 *
 * Return
 * Total bytes read
 */
ssize_t
dsbRead(dstrbuf *dsb, size_t size, int fd)
{
	ssize_t actualLen = 0;

	assert(dsb != NULL);
	if (size > dsb->size) {
		dsbResize(dsb, size);
	}
	actualLen = read(fd, dsb->str, size);
	dsb->len = actualLen;
	dsb->str[dsb->len] = '\0';
	return actualLen;
}

