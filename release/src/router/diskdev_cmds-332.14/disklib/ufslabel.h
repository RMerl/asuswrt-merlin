
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufslabel.h
 * - library routines to read/write the UFS disk label
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */
#ifndef _UFSLABEL
#define _UFSLABEL

#ifdef linux
typedef int boolean_t;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

#define		UFS_MAX_LABEL_UUID	16

boolean_t	ufslabel_get(int fd, struct ufslabel * label);
boolean_t	ufslabel_set(int fd, struct ufslabel * label);
boolean_t	ufslabel_set_name(struct ufslabel * ul_p, char * name, int len);
void		ufslabel_set_uuid(struct ufslabel * ul_p);
void		ufslabel_get_name(struct ufslabel * ul_p, char * name, int * len);
void		ufslabel_get_uuid(struct ufslabel * ul_p, char * uuid);
void		ufslabel_init(struct ufslabel * ul_p);


#endif
