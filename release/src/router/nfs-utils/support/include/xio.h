/*
 * xio.h	Declarations for simple parsing functions.
 *
 */

#ifndef XIO_H
#define XIO_H

#include <stdio.h>

typedef struct XFILE {
	FILE		*x_fp;
	int		x_line;
} XFILE;

XFILE	*xfopen(char *fname, char *type);
int	xflock(char *fname, char *type);
void	xfunlock(int lockid);
void	xfclose(XFILE *xfp);
int	xgettok(XFILE *xfp, char sepa, char *tok, int len);
int	xgetc(XFILE *xfp);
void	xungetc(int c, XFILE *xfp);
void	xskip(XFILE *xfp, char *str);
char	xskipcomment(XFILE *xfp);

#endif /* XIO_H */
