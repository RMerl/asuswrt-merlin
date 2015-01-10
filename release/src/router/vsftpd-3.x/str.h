#ifndef VSFTP_STR_H
#define VSFTP_STR_H

/* TODO - document these functions ;-) */

#ifndef VSF_FILESIZE_H
#include "filesize.h"
#endif

struct mystr
{
  char* PRIVATE_HANDS_OFF_p_buf;
  /* Internally, EXCLUDES trailing null */
  unsigned int PRIVATE_HANDS_OFF_len;
  unsigned int PRIVATE_HANDS_OFF_alloc_bytes;
};

#define INIT_MYSTR \
  { (void*)0, 0, 0 }

#ifdef VSFTP_STRING_HELPER
#define str_alloc_memchunk private_str_alloc_memchunk
#endif
void private_str_alloc_memchunk(struct mystr* p_str, const char* p_src,
                                unsigned int len);

void str_alloc_text(struct mystr* p_str, const char* p_src);
/* NOTE: String buffer data does NOT include terminating character */
void str_alloc_alt_term(struct mystr* p_str, const char* p_src, char term);
void str_alloc_ulong(struct mystr* p_str, unsigned long the_ulong);
void str_alloc_filesize_t(struct mystr* p_str, filesize_t the_filesize);
void str_copy(struct mystr* p_dest, const struct mystr* p_src);
const char* str_strdup(const struct mystr* p_str);
void str_empty(struct mystr* p_str);
void str_free(struct mystr* p_str);
void str_trunc(struct mystr* p_str, unsigned int trunc_len);
void str_reserve(struct mystr* p_str, unsigned int res_len);

int str_isempty(const struct mystr* p_str);
unsigned int str_getlen(const struct mystr* p_str);
const char* str_getbuf(const struct mystr* p_str);

int str_strcmp(const struct mystr* p_str1, const struct mystr* p_str2);
int str_equal(const struct mystr* p_str1, const struct mystr* p_str2);
int str_equal_text(const struct mystr* p_str, const char* p_text);

void str_append_str(struct mystr* p_str, const struct mystr* p_other);
void str_append_text(struct mystr* p_str, const char* p_src);
void str_append_ulong(struct mystr* p_str, unsigned long the_long);
void str_append_filesize_t(struct mystr* p_str, filesize_t the_filesize);
void str_append_char(struct mystr* p_str, char the_char);
void str_append_double(struct mystr* p_str, double the_double);

void str_upper(struct mystr* p_str);
void str_rpad(struct mystr* p_str, const unsigned int min_width);
void str_lpad(struct mystr* p_str, const unsigned int min_width);
void str_replace_char(struct mystr* p_str, char from, char to);
void str_replace_text(struct mystr* p_str, const char* p_from,
                      const char* p_to);

void str_split_char(struct mystr* p_src, struct mystr* p_rhs, char c);
void str_split_char_reverse(struct mystr* p_src, struct mystr* p_rhs, char c);
void str_split_text(struct mystr* p_src, struct mystr* p_rhs,
                    const char* p_text);
void str_split_text_reverse(struct mystr* p_src, struct mystr* p_rhs,
                            const char* p_text);

struct str_locate_result
{
  int found;
  unsigned int index;
  char char_found;
};

struct str_locate_result str_locate_char(
  const struct mystr* p_str, char look_char);
struct str_locate_result str_locate_str(
  const struct mystr* p_str, const struct mystr* p_look_str);
struct str_locate_result str_locate_str_reverse(
  const struct mystr* p_str, const struct mystr* p_look_str);
struct str_locate_result str_locate_text(
  const struct mystr* p_str, const char* p_text);
struct str_locate_result str_locate_text_reverse(
  const struct mystr* p_str, const char* p_text);
struct str_locate_result str_locate_chars(
  const struct mystr* p_str, const char* p_chars);

void str_left(const struct mystr* p_str, struct mystr* p_out,
              unsigned int chars);
void str_right(const struct mystr* p_str, struct mystr* p_out,
               unsigned int chars);
void str_mid_to_end(const struct mystr* p_str, struct mystr* p_out,
                    unsigned int indexx);

char str_get_char_at(const struct mystr* p_str, const unsigned int indexx);
int str_contains_space(const struct mystr* p_str);
int str_all_space(const struct mystr* p_str);
int str_contains_unprintable(const struct mystr* p_str);
void str_replace_unprintable(struct mystr* p_str, char new_char);
int str_atoi(const struct mystr* p_str);
filesize_t str_a_to_filesize_t(const struct mystr* p_str);
unsigned int str_octal_to_uint(const struct mystr* p_str);

/* PURPOSE: Extract a line of text (delimited by \n or EOF) from a string
 * buffer, starting at character position 'p_pos'. The extracted line will
 * not contain the '\n' terminator.
 *
 * RETURNS: 0 if no more lines are available, 1 otherwise.
 * The extracted text line is stored in 'p_line_str', which is
 * emptied if there are no more lines. 'p_pos' is updated to point to the
 * first character after the end of the line just extracted.
 */
int str_getline(const struct mystr* p_str, struct mystr* p_line_str,
                unsigned int* p_pos);

/* PURPOSE: Detect whether or not a string buffer contains a specific line
 * of text (delimited by \n or EOF).
 *
 * RETURNS: 1 if there is a matching line, 0 otherwise.
 */
int str_contains_line(const struct mystr* p_str,
                      const struct mystr* p_line_str);

#endif /* VSFTP_STR_H */

