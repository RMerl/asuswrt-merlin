/* 
   Unix SMB/CIFS implementation.
   stdio replacement
   Copyright (C) Andrew Tridgell 2001
   
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

#ifndef _XFILE_H_
#define _XFILE_H_
/*
  see xfile.c for explanations
*/

typedef struct _XFILE {
	int fd;
	char *buf;
	char *next;
	int bufsize;
	int bufused;
	int open_flags;
	int buftype;
	int flags;
} XFILE;

extern XFILE *x_stdin, *x_stdout, *x_stderr;

/* buffering type */
#define X_IOFBF 0
#define X_IOLBF 1
#define X_IONBF 2

#define x_getc(f) x_fgetc(f)

int x_vfprintf(XFILE *f, const char *format, va_list ap) PRINTF_ATTRIBUTE(2, 0);
int x_fprintf(XFILE *f, const char *format, ...) PRINTF_ATTRIBUTE(2, 3);

/** simulate setvbuf() */
int x_setvbuf(XFILE *f, char *buf, int mode, size_t size);

/** this looks more like open() than fopen(), but that is quite deliberate.
   I want programmers to *think* about O_EXCL, O_CREAT etc not just
   get them magically added 
*/
XFILE *x_fopen(const char *fname, int flags, mode_t mode);

/** simulate fclose() */
int x_fclose(XFILE *f);

/** simulate fwrite() */
size_t x_fwrite(const void *p, size_t size, size_t nmemb, XFILE *f);

/** thank goodness for asprintf() */
int x_fileno(const XFILE *f);

/** simulate fflush() */
int x_fflush(XFILE *f);

/** simulate setbuffer() */
void x_setbuffer(XFILE *f, char *buf, size_t size);

/** simulate setbuf() */
void x_setbuf(XFILE *f, char *buf);

/** simulate setlinebuf() */
void x_setlinebuf(XFILE *f);

/** simulate feof() */
int x_feof(XFILE *f);

/** simulate ferror() */
int x_ferror(XFILE *f);

/** simulate fgetc() */
int x_fgetc(XFILE *f);

/** simulate fread */
size_t x_fread(void *p, size_t size, size_t nmemb, XFILE *f);

/** simulate fgets() */
char *x_fgets(char *s, int size, XFILE *stream) ;

/** 
 * trivial seek, works only for SEEK_SET and SEEK_END if SEEK_CUR is
 * set then an error is returned */
off_t x_tseek(XFILE *f, off_t offset, int whence);

XFILE *x_fdup(const XFILE *f);

#endif /* _XFILE_H_ */
