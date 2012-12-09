/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * milli_httpd - pretty small HTTP server
 *
 * Copyright (C) 2001 ASUSTeK Inc.
 *
 */

#ifndef _httpd_h_
#define _httpd_h_

#include <arpa/inet.h>
#if defined(DEBUG) && defined(DMALLOC)
#include <dmalloc.h>
#endif

/* Basic authorization userid and passwd limit */
#define AUTH_MAX 64

/* Generic MIME type handler */
struct mime_handler {
	char *pattern;
	char *mime_type;
	char *extra_header;
	void (*input)(char *path, FILE *stream, int len, char *boundary);
	void (*output)(char *path, FILE *stream);
	void (*auth)(char *userid, char *passwd, char *realm);
};

extern struct mime_handler mime_handlers[];

#define MIME_EXCEPTION_NOAUTH_ALL 	1<<0
#define MIME_EXCEPTION_NOAUTH_FIRST	1<<1
#define MIME_EXCEPTION_NORESETTIME	1<<2

/* Exception MIME handler */
struct except_mime_handler {
	char *pattern;
	int flag;
};

extern struct except_mime_handler except_mime_handlers[];

#define INC_ITEM        128
#define REALLOC_VECTOR(p, len, size, item_size) {                               \
        assert ((len) >= 0 && (len) <= (size));                                         \
        if (len == size)        {                                                                               \
                int new_size;                                                                                   \
                void *np;                                                                                               \
                /* out of vector, reallocate */                                                 \
                new_size = size + INC_ITEM;                                                             \
                np = malloc (new_size * (item_size));                                   \
                assert (np != NULL);                                                                    \
                bzero (np, new_size * (item_size));                                             \
                memcpy (np, p, len * (item_size));                                              \
                free (p);                                                                                               \
                p = np;                                                                                                 \
                size = new_size;                                                                                \
        }    \
}


/* CGI helper functions */
extern void init_cgi(char *query);
extern char * get_cgi(char *name);
extern char * webcgi_get(const char *name);  //Viz add 2010.08
#if 0
typedef struct kw_s     {
        int len, tlen;                                          // actually / total
        unsigned char **idx;
        unsigned char *buf;
} kw_t, *pkw_t;

extern int load_dictionary (char *lang, pkw_t pkw);
extern void release_dictionary (pkw_t pkw);
extern char* search_desc (pkw_t pkw, char *name);
#endif
#ifdef TRANSLATE_ON_FLY
//2008.10 magic{
struct language_table{
	char *Lang;
	char *Target_Lang;
};

extern struct language_table language_tables[];

//2008.10 magic}
typedef struct kw_s     {
        int len, tlen;                                          // actually / total
        unsigned char **idx;
        unsigned char *buf;
} kw_t, *pkw_t;

#define INC_ITEM        128
#define REALLOC_VECTOR(p, len, size, item_size) {                               \
        assert ((len) >= 0 && (len) <= (size));                                         \
        if (len == size)        {                                                                               \
                int new_size;                                                                                   \
                void *np;                                                                                               \
                /* out of vector, reallocate */                                                 \
                new_size = size + INC_ITEM;                                                             \
                np = malloc (new_size * (item_size));                                   \
                assert (np != NULL);                                                                    \
                bzero (np, new_size * (item_size));                                             \
                memcpy (np, p, len * (item_size));                                              \
                free (p);                                                                                               \
                p = np;                                                                                                 \
                size = new_size;                                                                                \
        }    \
}
#endif  // defined TRANSLATE_ON_FLY


/* Regular file handler */
extern void do_file(char *path, FILE *stream);

/* GoAhead 2.1 compatibility */
typedef FILE * webs_t;
typedef char char_t;
#define T(s) (s)
#define __TMPVAR(x) tmpvar ## x
#define _TMPVAR(x) __TMPVAR(x)
#define TMPVAR _TMPVAR(__LINE__)
#define websWrite(wp, fmt, args...) ({ int TMPVAR = fprintf(wp, fmt, ## args); fflush(wp); TMPVAR; })
#define websError(wp, code, msg, args...) fprintf(wp, msg, ## args)
#define websHeader(wp) fputs("<html lang=\"en\">", wp)
#define websFooter(wp) fputs("</html>", wp)
#define websDone(wp, code) fflush(wp)
#define websGetVar(wp, var, default) (get_cgi(var) ? : default)
#define websDefaultHandler(wp, urlPrefix, webDir, arg, url, path, query) ({ do_ej(path, wp); fflush(wp); 1; })
#define websWriteData(wp, buf, nChars) ({ int TMPVAR = fwrite(buf, 1, nChars, wp); fflush(wp); TMPVAR; })
#define websWriteDataNonBlock websWriteData

extern int ejArgs(int argc, char_t **argv, char_t *fmt, ...);

/* GoAhead 2.1 Embedded JavaScript compatibility */
extern void do_ej(char *path, FILE *stream);
struct ej_handler {
	char *pattern;
	int (*output)(int eid, webs_t wp, int argc, char_t **argv);
};
extern struct ej_handler ej_handlers[];

#ifdef vxworks
#define fopen(path, mode)	tar_fopen((path), (mode))
#define fclose(fp)		tar_fclose((fp))
#undef getc
#define getc(fp)		tar_fgetc((fp))
extern FILE * tar_fopen(const char *path, const char *mode);
extern void tar_fclose(FILE *fp);
extern int tar_fgetc(FILE *fp);
#endif
#ifdef TRANSLATE_ON_FLY

extern int load_dictionary (char *lang, pkw_t pkw);
extern void release_dictionary (pkw_t pkw);
extern char* search_desc (pkw_t pkw, char *name);
//extern char Accept_Language[16];
#endif //defined TRANSLATE_ON_FLY

extern int http_port;

/* api-*.c */
extern int check_imageheader(char *buf, long *filelen);
extern int check_imagefile(char *fname);
extern unsigned int get_radio_status(char *ifname);

/* aspbw.c */
extern void do_f(char *path, webs_t wp);

/* cgi.c */
extern int web_read(void *buffer, int len);
extern void set_cgi(char *name, char *value);

/* httpd.c */
extern void start_ssl(void);
extern char *gethost(void);
extern void http_logout(unsigned int ip);
extern int is_auth(void);
extern int is_firsttime(void);

/* web.c */
extern int ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv);
extern int get_nat_vserver_table(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_route_table(int eid, webs_t wp, int argc, char_t **argv);
extern void copy_index_to_unindex(char *prefix, int unit, int subunit);
extern void logmessage(char *logheader, char *fmt, ...);
extern int is_private_subnet(const char *ip);
extern char* INET6_rresolve(struct sockaddr_in6 *sin6, int numeric);
extern char *trim_r(char *str);

extern void wo_iptbackup(char *url, webs_t wp);

extern void ej_ipt_bandwidth(int eid, webs_t wp, int argc, char **argv);
extern void ej_iptmon(int eid, webs_t wp, int argc, char **argv);
extern void ej_climon(int argc, char **argv, webs_t wp);
extern void ej_iptraffic(int eid, webs_t wp, int argc, char **argv);


/* web-*.c */
extern int ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit);
extern int ej_wl_status_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wps_info_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wps_info(int eid, webs_t wp, int argc, char_t **argv);

#endif /* _httpd_h_ */
