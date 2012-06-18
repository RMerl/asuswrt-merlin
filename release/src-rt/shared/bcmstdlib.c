/*
 * stdlib support routines for self-contained images.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmstdlib.c,v 1.51.20.1 2010-03-23 01:20:17 Exp $
 */

/*
 * bcmstdlib.c file should be used only to construct an OSL or alone without any OSL
 * It should not be used with any orbitarary OSL's as there could be a conflict
 * with some of the routines defined here.
*/

#include <typedefs.h>
#if defined(NDIS) || defined(_MINOSL_) || defined(__vxworks) || defined(PCBIOS) || \
	defined(EFI)
/* debatable */
#include <osl.h>
#elif 1
#include <stdio.h>
#endif 

/*
 * Define BCMSTDLIB_WIN32_APP if this is a Win32 Application compile
 */
#if defined(_WIN32) && !defined(NDIS) && !defined(EFI)
#define BCMSTDLIB_WIN32_APP 1
#endif /* _WIN32 && !NDIS */

/*
 * Define BCMSTDLIB_SNPRINTF_ONLY if we only want snprintf & vsnprintf implementations
 */
#if (defined(_WIN32) && !defined(EFI)) || defined(__vxworks) || defined(_CFE_)
#define BCMSTDLIB_SNPRINTF_ONLY 1
#endif 

#include <stdarg.h>
#include <bcmstdlib.h>
#ifndef BCMSTDLIB_WIN32_APP
#include <bcmutils.h>
#endif

#ifdef MSGTRACE
#include <msgtrace.h>
#endif

#ifdef BCMSTDLIB_WIN32_APP

/* for a WIN32 application, use _vsnprintf as basis of vsnprintf/snprintf to 
 * support full set of format specifications.
 */

int
vsnprintf(char *buf, size_t bufsize, const char *fmt, va_list ap)
{
	int r;

	r = _vsnprintf(buf, bufsize, fmt, ap);

	/* Microsoft _vsnprintf() will not null terminate on overflow,
	 * so null terminate at buffer end on error
	 */
	if (r < 0 && bufsize > 0)
		buf[bufsize - 1] = '\0';

	return r;
}

int
snprintf(char *buf, size_t bufsize, const char *fmt, ...)
{
	va_list	ap;
	int	r;

	va_start(ap, fmt);
	r = vsnprintf(buf, bufsize, fmt, ap);
	va_end(ap);

	return r;
}

#else /* BCMSTDLIB_WIN32_APP */


static const char digits[17] = "0123456789ABCDEF";
static const char ldigits[17] = "0123456789abcdef";

static int
__atox(char *buf, char * end, unsigned int num, unsigned int radix, int width,
       const char *digits)
{
	char buffer[16];
	char *op;
	int retval;

	op = &buffer[0];
	retval = 0;

	do {
		*op++ = digits[num % radix];
		retval++;
		num /= radix;
	} while (num != 0);

	if (width && (width > retval)) {
		width = width - retval;
		while (width) {
			*op++ = '0';
			retval++;
			width--;
		}
	}

	while (op != buffer) {
		op--;
		if (buf <= end)
			*buf = *op;
		buf++;
	}

	return retval;
}

int
BCMROMFN(vsnprintf)(char *buf, size_t size, const char *fmt, va_list ap)
{
	char *optr;
	char *end;
	const char *iptr;
	unsigned char *tmpptr;
	unsigned int x;
	int i;
	int leadingzero;
	int leadingnegsign;
	int islong;
	int width;
	int width2 = 0;
	int hashash = 0;

	optr = buf;
	end = buf + size - 1;
	iptr = fmt;

	if (end < buf - 1) {
		end = ((void *) -1);
		size = end - buf + 1;
	}

	while (*iptr) {
		if (*iptr != '%') {
			if (optr <= end)
				*optr = *iptr;
			++optr;
			++iptr;
			continue;
		}

		iptr++;

		if (*iptr == '#') {
			hashash = 1;
			iptr++;
		}
		if (*iptr == '-') {
			leadingnegsign = 1;
			iptr++;
		} else
			leadingnegsign = 0;

		if (*iptr == '0')
			leadingzero = 1;
		else
			leadingzero = 0;

		width = 0;
		while (*iptr && bcm_isdigit(*iptr)) {
			width += (*iptr - '0');
			iptr++;
			if (bcm_isdigit(*iptr))
				width *= 10;
		}
		if (*iptr == '.') {
			iptr++;
			width2 = 0;
			while (*iptr && bcm_isdigit(*iptr)) {
				width2 += (*iptr - '0');
				iptr++;
				if (bcm_isdigit(*iptr)) width2 *= 10;
			}
		}

		islong = 0;
		if (*iptr == 'l') {
			islong++;
			iptr++;
		}

		switch (*iptr) {
		case 's':
			tmpptr = (unsigned char *) va_arg(ap, unsigned char *);
			if (!tmpptr) tmpptr = (unsigned char *) "(null)";
			if ((width == 0) & (width2 == 0)) {
				while (*tmpptr) {
					if (optr <= end)
						*optr = *tmpptr;
					++optr;
					++tmpptr;
				}
				break;
			}
			while (width && *tmpptr) {
				if (optr <= end)
					*optr = *tmpptr;
				++optr;
				++tmpptr;
				width--;
			}
			while (width) {
				if (optr <= end)
					*optr = ' ';
				++optr;
				width--;
			}
			break;
		case 'd':
		case 'i':
			i = va_arg(ap, int);
			if (i < 0) {
				if (optr <= end)
					*optr = '-';
				++optr;
				i = -i;
			}
			optr += __atox(optr, end, i, 10, width, digits);
			break;
		case 'u':
			x = va_arg(ap, unsigned int);
			optr += __atox(optr, end, x, 10, width, digits);
			break;
		case 'X':
		case 'x':
			x = va_arg(ap, unsigned int);
			optr += __atox(optr, end, x, 16, width,
			               (*iptr == 'X') ? digits : ldigits);
			break;
		case 'p':
		case 'P':
			x = va_arg(ap, unsigned int);
			optr += __atox(optr, end, x, 16, 8,
			               (*iptr == 'P') ? digits : ldigits);
			break;
		case 'c':
			x = va_arg(ap, int);
			if (optr <= end)
				*optr = x & 0xff;
			optr++;
			break;

		default:
			if (optr <= end)
				*optr = *iptr;
			optr++;
			break;
		}
		iptr++;
	}

	if (optr <= end) {
		*optr = '\0';
		return (int)(optr - buf);
	} else {
		*end = '\0';
		return (int)(end - buf);
	}
}


int
BCMROMFN(snprintf)(char *buf, size_t bufsize, const char *fmt, ...)
{
	va_list		ap;
	int			r;

	va_start(ap, fmt);
	r = vsnprintf(buf, bufsize, fmt, ap);
	va_end(ap);

	return r;
}

#endif /* BCMSTDLIB_WIN32_APP */

#ifndef BCMSTDLIB_SNPRINTF_ONLY


int
BCMROMFN(vsprintf)(char *buf, const char *fmt, va_list ap)
{
	return (vsnprintf(buf, INT_MAX, fmt, ap));
}


int
BCMROMFN(sprintf)(char *buf, const char *fmt, ...)
{
	va_list ap;
	int count;

	va_start(ap, fmt);
	count = vsprintf(buf, fmt, ap);
	va_end(ap);

	return count;
}


void *
BCMROMFN(memmove)(void *dest, const void *src, size_t n)
{
	/* only use memcpy if there is no overlap. otherwise copy each byte in a safe sequence */
	if (((const char *)src >= (char *)dest + n) || ((const char *)src + n <= (char *)dest)) {
		return memcpy(dest, src, n);
	}

	/* Overlapping copy forward or backward */
	if (src < dest) {
		unsigned char *d = (unsigned char *)dest + (n - 1);
		const unsigned char *s = (const unsigned char *)src + (n - 1);
		while (n) {
			*d-- = *s--;
			n--;
		}
	} else if (src > dest) {
		unsigned char *d = (unsigned char *)dest;
		const unsigned char *s = (const unsigned char *)src;
		while (n) {
			*d++ = *s++;
			n--;
		}
	}

	return dest;
}

#ifndef EFI
int
BCMROMFN(memcmp)(const void *s1, const void *s2, size_t n)
{
	const unsigned char *ss1;
	const unsigned char *ss2;

	ss1 = (const unsigned char *)s1;
	ss2 = (const unsigned char *)s2;

	while (n) {
		if (*ss1 < *ss2)
			return -1;
		if (*ss1 > *ss2)
			return 1;
		ss1++;
		ss2++;
		n--;
	}

	return 0;
}

/* Skip over functions that are being used from DriverLibrary to save space */
char *
BCMROMFN(strcpy)(char *dest, const char *src)
{
	char *ptr = dest;

	while ((*ptr++ = *src++) != '\0')
		;

	return dest;
}

char *
BCMROMFN(strncpy)(char *dest, const char *src, size_t n)
{
	char *endp;
	char *p;

	p = dest;
	endp = p + n;

	while (p != endp && (*p++ = *src++) != '\0')
		;

	/* zero fill remainder */
	while (p != endp)
		*p++ = '\0';

	return dest;
}

size_t
BCMROMFN(strlen)(const char *s)
{
	size_t n = 0;

	while (*s) {
		s++;
		n++;
	}

	return n;
}

int
BCMROMFN(strcmp)(const char *s1, const char *s2)
{
	while (*s2 && *s1) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
	}

	if (*s1 && !*s2)
		return 1;
	if (!*s1 && *s2)
		return -1;
	return 0;
}
#endif /* EFI */

int
BCMROMFN(strncmp)(const char *s1, const char *s2, size_t n)
{
	while (*s2 && *s1 && n) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
		n--;
	}

	if (!n)
		return 0;
	if (*s1 && !*s2)
		return 1;
	if (!*s1 && *s2)
		return -1;
	return 0;
}

char *
BCMROMFN(strchr)(const char *str, int c)
{
	char *x = (char *)str;

	while (*x != (char)c) {
		if (*x++ == '\0')
		return (NULL);
	}
	return (x);
}

char *
BCMROMFN(strrchr)(const char *str, int c)
{
	char *save = NULL;

	do {
		if (*str == (char) c)
			save = (char*)(str);
	} while (*str++ != '\0');

	return (save);
}

/* Skip over functions that are being used from DriverLibrary to save space */
#ifndef EFI
char *
BCMROMFN(strcat)(char *d, const char *s)
{
	strcpy(&d[strlen(d)], s);
	return (d);
}
#endif /* EFI */

char *
BCMROMFN(index)(const char *s, int c)
{
	/* Terminating NUL is considered part of string */

	for (; *s != c; s++)
		if (!*s)
			return NULL;

	return (char *)s;
}

/* Skip over functions that are being used from DriverLibrary to save space */
#ifndef EFI
char *
BCMROMFN(strstr)(const char *s, const char *substr)
{
	int substr_len = strlen(substr);

	for (; *s; s++)
		if (strncmp(s, substr, substr_len) == 0)
			return (char *)s;

	return NULL;
}
#endif /* EFI */

size_t
BCMROMFN(strspn)(const char *s, const char *accept)
{
	uint count = 0;

	while (s[count] && index(accept, s[count]))
		count++;

	return count;
}

size_t
BCMROMFN(strcspn)(const char *s, const char *reject)
{
	uint count = 0;

	while (s[count] && !index(reject, s[count]))
		count++;

	return count;
}

void *
BCMROMFN(memchr)(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *ptr = s;

		do {
			if (*ptr == (unsigned char)c)
				return (void *)ptr;
			ptr++;
			n--;
		} while (n != 0);
	}
	return NULL;
}

unsigned long
BCMROMFN(strtoul)(const char *cp, char **endp, int base)
{
	ulong result, value;
	bool minus;

	minus = FALSE;

	while (bcm_isspace(*cp))
		cp++;

	if (cp[0] == '+')
		cp++;
	else if (cp[0] == '-') {
		minus = TRUE;
		cp++;
	}

	if (base == 0) {
		if (cp[0] == '0') {
			if ((cp[1] == 'x') || (cp[1] == 'X')) {
				base = 16;
				cp = &cp[2];
			} else {
				base = 8;
				cp = &cp[1];
			}
		} else
			base = 10;
	} else if (base == 16 && (cp[0] == '0') && ((cp[1] == 'x') || (cp[1] == 'X'))) {
		cp = &cp[2];
	}

	result = 0;

	while (bcm_isxdigit(*cp) &&
	       (value = bcm_isdigit(*cp) ? *cp - '0' : bcm_toupper(*cp) - 'A' + 10) <
	       (ulong) base) {
		result = result * base + value;
		cp++;
	}

	if (minus)
		result = (ulong)(result * -1);

	if (endp)
		*endp = (char *)cp;

	return (result);
}

#ifndef EFI
/* memset is not in ROM offload because it is used directly by the compiler in
 * structure assignments/character array initialization with "".
 */
void *
memset(void *dest, int c, size_t n)
{
	uint32 w, *dw;
	unsigned char *d;


	dw = (uint32 *)dest;

	/* 8 min because we have to create w */
	if ((n >= 8) && (((uintptr)dest & 3) == 0)) {
		if (c == 0)
			w = 0;
		else {
			unsigned char ch;

			ch = (unsigned char)(c & 0xff);
			w = (ch << 8) | ch;
			w |= w << 16;
		}
		while (n >= 4) {
			*dw++ = w;
			n -= 4;
		}
	}
	d = (unsigned char *)dw;

	while (n) {
		*d++ = (unsigned char)c;
		n--;
	}

	return d;
}

/* memcpy is not in ROM offload because it is used directly by the compiler in
 * structure assignments.
 */
void *
memcpy(void *dest, const void *src, size_t n)
{
	uint32 *dw;
	const uint32 *sw;
	unsigned char *d;
	const unsigned char *s;

	sw = (const uint32 *)src;
	dw = (uint32 *)dest;
	if ((n >= 4) && (((uintptr)src & 3) == 0) && (((uintptr)dest & 3) == 0)) {
		while (n >= 4) {
			*dw++ = *sw++;
			n -= 4;
		}
	}
	d = (unsigned char *)dw;
	s = (const unsigned char *)sw;
	while (n) {
		*d++ = *s++;
		n--;
	}

	return dest;
}
#endif /* EFI */

/* Include printf if it has already not been defined as NULL */
#ifndef printf
int
printf(const char *fmt, ...)
{
	va_list ap;
	int count, i;
	char buffer[PRINTF_BUFLEN + 1];

	va_start(ap, fmt);
	count = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	for (i = 0; i < count; i++) {
		putc(buffer[i]);

#ifdef EFI
		if (buffer[i] == '\n')
			putc('\r');
#endif
	}

#ifdef MSGTRACE
	msgtrace_put(buffer, count);
#endif

	return count;
}
#endif /* printf */


#if !defined(_WIN32) && !defined(_CFE_) && !defined(EFI)
int
fputs(const char *s, FILE *stream /* UNUSED */)
{
	char c;
	while ((c = *s++))
		putchar(c);
	return 0;
}

int
puts(const char *s)
{
	fputs(s, stdout);
	putchar('\n');
	return 0;
}

int
fputc(int c, FILE *stream /* UNUSED */)
{
	putc(c);
	return (int)(unsigned char)c;
}


unsigned long
rand(void)
{
	static unsigned long seed = 1;
	long x, hi, lo, t;

	x = seed;
	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t <= 0) t += 0x7fffffff;
	seed = t;
	return t;
}
#endif 
#endif /* BCMSTDLIB_SNPRINTF_ONLY */
