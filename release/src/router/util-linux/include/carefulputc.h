#ifndef _CAREFUULPUTC_H
#define _CAREFUULPUTC_H

/* putc() for use in write and wall (that sometimes are sgid tty) */
/* Avoid control characters in our locale, and also ASCII control characters.
   Note that the locale of the recipient is unknown. */
#include <stdio.h>
#include <ctype.h>

#define iso8859x_iscntrl(c) \
	(((c) & 0x7f) < 0x20 || (c) == 0x7f)

static inline int carefulputc(int c, FILE *fp) {
	int ret;

	if (c == '\007' || c == '\t' || c == '\r' || c == '\n' ||
	    (!iso8859x_iscntrl(c) && (isprint(c) || isspace(c))))
		ret = putc(c, fp);
	else if ((c & 0x80) || !isprint(c^0x40))
		ret = fprintf(fp, "\\%3o", (unsigned char) c);
	else {
		ret = putc('^', fp);
		if (ret != EOF)
			ret = putc(c^0x40, fp);
	}
	return (ret < 0) ? EOF : 0;
}

#endif  /*  _CAREFUULPUTC_H  */
