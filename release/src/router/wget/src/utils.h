/* Declarations for utils.c.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011, 2015 Free Software
   Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#ifndef UTILS_H
#define UTILS_H

# include <stdlib.h>

/* Constant is using when we don`t know attempted size exactly */
#define UNKNOWN_ATTEMPTED_SIZE -3

#ifndef MAX_PINNED_PUBKEY_SIZE
#define MAX_PINNED_PUBKEY_SIZE 1048576 /* 1MB */
#endif

/* Macros that interface to malloc, but know about type sizes, and
   cast the result to the appropriate type.  The casts are not
   necessary in standard C, but Wget performs them anyway for the sake
   of pre-standard environments and possibly C++.  */

#define xnew(type) (xmalloc (sizeof (type)))
#define xnew0(type) (xcalloc (1, sizeof (type)))
#define xnew_array(type, len) (xmalloc ((len) * sizeof (type)))
#define xnew0_array(type, len) (xcalloc ((len), sizeof (type)))

#define alloca_array(type, size) ((type *) alloca ((size) * sizeof (type)))

#define xfree(p) do { free ((void *) (p)); p = NULL; } while (0)

struct hash_table;

struct file_memory {
  char *content;
  long length;
  int mmap_p;
};

#define HYPHENP(x) (*(x) == '-' && !*((x) + 1))

char *time_str (time_t);
char *datetime_str (time_t);

char *xstrdup_lower (const char *);

char *strdupdelim (const char *, const char *);
char **sepstring (const char *);
bool subdir_p (const char *, const char *);
void fork_to_background (void);

char *aprintf (const char *, ...) GCC_FORMAT_ATTR (1, 2);
char *concat_strings (const char *, ...);

typedef struct file_stat_s {
  int access_err;               /* Error in accecssing file : Not present vs permission */
  ino_t st_ino;                 /* st_ino from stats() on the file before open() */
  dev_t st_dev;                 /* st_dev from stats() on the file before open() */
} file_stats_t;

void touch (const char *, time_t);
int remove_link (const char *);
bool file_exists_p (const char *, file_stats_t *);
bool file_non_directory_p (const char *);
wgint file_size (const char *);
int make_directory (const char *);
char *unique_name (const char *, bool);
FILE *unique_create (const char *, bool, char **);
FILE *fopen_excl (const char *, int);
FILE *fopen_stat (const char *, const char *, file_stats_t *);
int   open_stat  (const char *, int, mode_t, file_stats_t *);
char *file_merge (const char *, const char *);

int fnmatch_nocase (const char *, const char *, int);
bool acceptable (const char *);
bool accept_url (const char *);
bool accdir (const char *s);
char *suffix (const char *s);
bool match_tail (const char *, const char *, bool);
bool has_wildcards_p (const char *);

bool has_html_suffix_p (const char *);

struct file_memory *wget_read_file (const char *);
void wget_read_file_free (struct file_memory *);

void free_vec (char **);
char **merge_vecs (char **, char **);
char **vec_append (char **, const char *);

void string_set_add (struct hash_table *, const char *);
int string_set_contains (struct hash_table *, const char *);
void string_set_to_array (struct hash_table *, char **);
void string_set_free (struct hash_table *);
void free_keys_and_values (struct hash_table *);

const char *with_thousand_seps (wgint);

/* human_readable must be able to accept wgint and SUM_SIZE_INT
   arguments.  On machines where wgint is 32-bit, declare it to accept
   double.  */
#if SIZEOF_WGINT >= 8
# define HR_NUMTYPE wgint
#else
# define HR_NUMTYPE double
#endif
char *human_readable (HR_NUMTYPE, const int, const int);


int numdigit (wgint);
char *number_to_string (char *, wgint);
char *number_to_static_string (wgint);
wgint convert_to_bits (wgint);

int determine_screen_width (void);
int random_number (int);
double random_float (void);

bool run_with_timeout (double, void (*) (void *), void *);
void xsleep (double);

/* How many bytes it will take to store LEN bytes in base64.  */
#define BASE64_LENGTH(len) (4 * (((len) + 2) / 3))

size_t wget_base64_encode (const void *, size_t, char *);
ssize_t wget_base64_decode (const char *, void *, size_t);

#ifdef HAVE_LIBPCRE
void *compile_pcre_regex (const char *);
bool match_pcre_regex (const void *, const char *);
#endif

void *compile_posix_regex (const char *);
bool match_posix_regex (const void *, const char *);

void stable_sort (void *, size_t, size_t, int (*) (const void *, const void *));

const char *print_decimal (double);

long get_max_length (const char *path, int length, int name);

#ifndef HAVE_STRLCPY
size_t strlcpy (char *dst, const char *src, size_t size);
#endif

void wg_hex_to_string (char *str_buffer, const char *hex_buffer, size_t hex_len);

extern unsigned char char_prop[];

#ifdef HAVE_SSL
/* Check pinned public key. */
bool wg_pin_peer_pubkey (const char *pinnedpubkey, const char *pubkey, size_t pubkeylen);
#endif

#endif /* UTILS_H */
