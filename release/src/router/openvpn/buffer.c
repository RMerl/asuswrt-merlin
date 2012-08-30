/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2010 OpenVPN Technologies, Inc. <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "syshead.h"

#include "common.h"
#include "buffer.h"
#include "error.h"
#include "mtu.h"

#include "memdbg.h"

size_t
array_mult_safe (const size_t m1, const size_t m2, const size_t extra)
{
  const size_t limit = 0xFFFFFFFF;
  unsigned long long res = (unsigned long long)m1 * (unsigned long long)m2 + (unsigned long long)extra;
  if (unlikely(m1 > limit) || unlikely(m2 > limit) || unlikely(extra > limit) || unlikely(res > (unsigned long long)limit))
    msg (M_FATAL, "attemped allocation of excessively large array");
  return (size_t) res;
}

void
buf_size_error (const size_t size)
{
  msg (M_FATAL, "fatal buffer size error, size=%lu", (unsigned long)size);
}

struct buffer
#ifdef DMALLOC
alloc_buf_debug (size_t size, const char *file, int line)
#else
alloc_buf (size_t size)
#endif
{
#ifdef DMALLOC
  return alloc_buf_gc_debug (size, NULL, file, line);
#else
  return alloc_buf_gc (size, NULL);
#endif
}

struct buffer
#ifdef DMALLOC
alloc_buf_gc_debug (size_t size, struct gc_arena *gc, const char *file, int line)
#else
alloc_buf_gc (size_t size, struct gc_arena *gc)
#endif
{
  struct buffer buf;
  if (!buf_size_valid (size))
    buf_size_error (size);
  buf.capacity = (int)size;
  buf.offset = 0;
  buf.len = 0;
#ifdef DMALLOC
  buf.data = (uint8_t *) gc_malloc_debug (size, false, gc, file, line);
#else
  buf.data = (uint8_t *) gc_malloc (size, false, gc);
#endif
  if (size)
    *buf.data = 0;
  return buf;
}

struct buffer
#ifdef DMALLOC
clone_buf_debug (const struct buffer* buf, const char *file, int line)
#else
clone_buf (const struct buffer* buf)
#endif
{
  struct buffer ret;
  ret.capacity = buf->capacity;
  ret.offset = buf->offset;
  ret.len = buf->len;
#ifdef DMALLOC
  ret.data = (uint8_t *) openvpn_dmalloc (file, line, buf->capacity);
#else
  ret.data = (uint8_t *) malloc (buf->capacity);
#endif
  check_malloc_return (ret.data);
  memcpy (BPTR (&ret), BPTR (buf), BLEN (buf));
  return ret;
}

#ifdef BUF_INIT_TRACKING

bool
buf_init_debug (struct buffer *buf, int offset, const char *file, int line)
{
  buf->debug_file = file;
  buf->debug_line = line;
  return buf_init_dowork (buf, offset);
}

static inline int
buf_debug_line (const struct buffer *buf)
{
  return buf->debug_line;
}

static const char *
buf_debug_file (const struct buffer *buf)
{
  return buf->debug_file;
}

#else

#define buf_debug_line(buf) 0
#define buf_debug_file(buf) "[UNDEF]"

#endif

void
buf_clear (struct buffer *buf)
{
  if (buf->capacity > 0)
    memset (buf->data, 0, buf->capacity);
  buf->len = 0;
  buf->offset = 0;
}

bool
buf_assign (struct buffer *dest, const struct buffer *src)
{
  if (!buf_init (dest, src->offset))
    return false;
  return buf_write (dest, BPTR (src), BLEN (src));
}

struct buffer
clear_buf ()
{
  struct buffer buf;
  CLEAR (buf);
  return buf;
}

void
free_buf (struct buffer *buf)
{
  if (buf->data)
    free (buf->data);
  CLEAR (*buf);
}

/*
 * Return a buffer for write that is a subset of another buffer
 */
struct buffer
buf_sub (struct buffer *buf, int size, bool prepend)
{
  struct buffer ret;
  uint8_t *data;

  CLEAR (ret);
  data = prepend ? buf_prepend (buf, size) : buf_write_alloc (buf, size);
  if (data)
    {
      ret.capacity = size;
      ret.data = data;
    }
  return ret;
}

/*
 * printf append to a buffer with overflow check
 */
bool
buf_printf (struct buffer *buf, const char *format, ...)
{
  int ret = false;
  if (buf_defined (buf))
    {
      va_list arglist;
      uint8_t *ptr = BEND (buf);
      int cap = buf_forward_capacity (buf);

      if (cap > 0)
	{
	  int stat;
	  va_start (arglist, format);
	  stat = vsnprintf ((char *)ptr, cap, format, arglist);
	  va_end (arglist);
	  *(buf->data + buf->capacity - 1) = 0; /* windows vsnprintf needs this */
	  buf->len += (int) strlen ((char *)ptr);
	  if (stat >= 0 && stat < cap)
	    ret = true;
	}
    }
  return ret;
}

/*
 * This is necessary due to certain buggy implementations of snprintf,
 * that don't guarantee null termination for size > 0.
 *
 * This function is duplicated into service-win32/openvpnserv.c
 * Any modifications here should be done to the other place as well.
 */

int openvpn_snprintf(char *str, size_t size, const char *format, ...)
{
  va_list arglist;
  int ret = 0;
  if (size > 0)
    {
      va_start (arglist, format);
      ret = vsnprintf (str, size, format, arglist);
      va_end (arglist);
      str[size - 1] = 0;
    }
  return ret;
}

/*
 * write a string to the end of a buffer that was
 * truncated by buf_printf
 */
void
buf_catrunc (struct buffer *buf, const char *str)
{
  if (buf_forward_capacity (buf) <= 1)
    {
      int len = (int) strlen (str) + 1;
      if (len < buf_forward_capacity_total (buf))
	{
	  strncpynt ((char *)(buf->data + buf->capacity - len), str, len);
	}
    }
}

/*
 * convert a multi-line output to one line
 */
void
convert_to_one_line (struct buffer *buf)
{
  uint8_t *cp = BPTR(buf);
  int len = BLEN(buf);
  while (len--)
    {
      if (*cp == '\n')
	*cp = '|';
      ++cp;
    }
}

/* NOTE: requires that string be null terminated */
void
buf_write_string_file (const struct buffer *buf, const char *filename, int fd)
{
  const int len = strlen ((char *) BPTR (buf));
  const int size = write (fd, BPTR (buf), len);
  if (size != len)
    msg (M_ERR, "Write error on file '%s'", filename);
}

/*
 * Garbage collection
 */

void *
#ifdef DMALLOC
gc_malloc_debug (size_t size, bool clear, struct gc_arena *a, const char *file, int line)
#else
gc_malloc (size_t size, bool clear, struct gc_arena *a)
#endif
{
  void *ret;
  if (a)
    {
      struct gc_entry *e;
#ifdef DMALLOC
      e = (struct gc_entry *) openvpn_dmalloc (file, line, size + sizeof (struct gc_entry));
#else
      e = (struct gc_entry *) malloc (size + sizeof (struct gc_entry));
#endif
      check_malloc_return (e);
      ret = (char *) e + sizeof (struct gc_entry);
      e->next = a->list;
      a->list = e;
    }
  else
    {
#ifdef DMALLOC
      ret = openvpn_dmalloc (file, line, size);
#else
      ret = malloc (size);
#endif
      check_malloc_return (ret);
    }
#ifndef ZERO_BUFFER_ON_ALLOC
  if (clear)
#endif
    memset (ret, 0, size);
  return ret;
}

void
x_gc_free (struct gc_arena *a)
{
  struct gc_entry *e;
  e = a->list;
  a->list = NULL;
  
  while (e != NULL)
    {
      struct gc_entry *next = e->next;
      free (e);
      e = next;
    }
}

/*
 * Transfer src arena to dest, resetting src to an empty arena.
 */
void
gc_transfer (struct gc_arena *dest, struct gc_arena *src)
{
  if (dest && src)
    {
      struct gc_entry *e = src->list;
      if (e)
	{
	  while (e->next != NULL)
	    e = e->next;
	  e->next = dest->list;
	  dest->list = src->list;
	  src->list = NULL;
	}
    }
}

/*
 * Hex dump -- Output a binary buffer to a hex string and return it.
 */

char *
format_hex_ex (const uint8_t *data, int size, int maxoutput,
	       int space_break, const char* separator,
	       struct gc_arena *gc)
{
  struct buffer out = alloc_buf_gc (maxoutput ? maxoutput :
				    ((size * 2) + (size / space_break) * (int) strlen (separator) + 2),
				    gc);
  int i;
  for (i = 0; i < size; ++i)
    {
      if (separator && i && !(i % space_break))
	buf_printf (&out, "%s", separator);
      buf_printf (&out, "%02x", data[i]);
    }
  buf_catrunc (&out, "[more...]");
  return (char *)out.data;
}

/*
 * remove specific trailing character
 */

void
buf_rmtail (struct buffer *buf, uint8_t remove)
{
  uint8_t *cp = BLAST(buf);
  if (cp && *cp == remove)
    {
      *cp = '\0';
      --buf->len;
    }
}

/*
 * force a null termination even it requires
 * truncation of the last char.
 */
void
buf_null_terminate (struct buffer *buf)
{
  char *last = (char *) BLAST (buf);
  if (last && *last == '\0') /* already terminated? */
    return;

  if (!buf_safe (buf, 1))    /* make space for trailing null */
    buf_inc_len (buf, -1);

  buf_write_u8 (buf, 0);
}

/*
 * Remove trailing \r and \n chars and ensure
 * null termination.
 */
void
buf_chomp (struct buffer *buf)
{
  while (true)
    {
      char *last = (char *) BLAST (buf);
      if (!last)
	break;
      if (char_class (*last, CC_CRLF|CC_NULL))
	{
	  if (!buf_inc_len (buf, -1))
	    break;
	}
      else
	break;
    }
  buf_null_terminate (buf);
}

const char *
skip_leading_whitespace (const char *str)
{
  while (*str)
    {
      const char c = *str;
      if (!(c == ' ' || c == '\t'))
	break;
      ++str;
    }
  return str;
}

/*
 * like buf_null_terminate, but operate on strings
 */
void
string_null_terminate (char *str, int len, int capacity)
{
  ASSERT (len >= 0 && len <= capacity && capacity > 0);
  if (len < capacity)
    *(str + len) = '\0';
  else if (len == capacity)
    *(str + len - 1) = '\0';
}

/*
 * Remove trailing \r and \n chars.
 */
void
chomp (char *str)
{
  rm_trailing_chars (str, "\r\n");
}

/*
 * Remove trailing chars
 */
void
rm_trailing_chars (char *str, const char *what_to_delete)
{
  bool modified;
  do {
    const int len = strlen (str);
    modified = false;
    if (len > 0)
      {
	char *cp = str + (len - 1);
	if (strchr (what_to_delete, *cp) != NULL)
	  {
	    *cp = '\0';
	    modified = true;
	  }
      }
  } while (modified);
}

/*
 * Allocate a string
 */
char *
#ifdef DMALLOC
string_alloc_debug (const char *str, struct gc_arena *gc, const char *file, int line)
#else
string_alloc (const char *str, struct gc_arena *gc)
#endif
{
  if (str)
    {
      const int n = strlen (str) + 1;
      char *ret;

#ifdef DMALLOC
      ret = (char *) gc_malloc_debug (n, false, gc, file, line);
#else
      ret = (char *) gc_malloc (n, false, gc);
#endif
      memcpy (ret, str, n);
      return ret;
    }
  else
    return NULL;
}

/*
 * Erase all characters in a string
 */
void
string_clear (char *str)
{
  if (str)
    {
      const int len = strlen (str);
      if (len > 0)
	memset (str, 0, len);
    }
}

/*
 * Return the length of a string array
 */
int
string_array_len (const char **array)
{
  int i = 0;
  if (array)
    {
      while (array[i])
        ++i;
    }
  return i;
}

char *
print_argv (const char **p, struct gc_arena *gc, const unsigned int flags)
{
  struct buffer out = alloc_buf_gc (256, gc);
  int i = 0;
  for (;;)
    {
      const char *cp = *p++;
      if (!cp)
	break;
      if (i)
	buf_printf (&out, " ");
      if (flags & PA_BRACKET)
	buf_printf (&out, "[%s]", cp);
      else
	buf_printf (&out, "%s", cp);
      ++i;
    }
  return BSTR (&out);
}

/*
 * Allocate a string inside a buffer
 */
struct buffer
#ifdef DMALLOC
string_alloc_buf_debug (const char *str, struct gc_arena *gc, const char *file, int line)
#else
string_alloc_buf (const char *str, struct gc_arena *gc)
#endif
{
  struct buffer buf;

  ASSERT (str);

#ifdef DMALLOC
  buf_set_read (&buf, (uint8_t*) string_alloc_debug (str, gc, file, line), strlen (str) + 1);
#else
  buf_set_read (&buf, (uint8_t*) string_alloc (str, gc), strlen (str) + 1);
#endif

  if (buf.len > 0) /* Don't count trailing '\0' as part of length */
    --buf.len;

  return buf;
}

/*
 * String comparison
 */

bool
buf_string_match_head_str (const struct buffer *src, const char *match)
{
  const int size = strlen (match);
  if (size < 0 || size > src->len)
    return false;
  return memcmp (BPTR (src), match, size) == 0;
}

bool
buf_string_compare_advance (struct buffer *src, const char *match)
{
  if (buf_string_match_head_str (src, match))
    {
      buf_advance (src, strlen (match));
      return true;
    }
  else
    return false;
}

int
buf_substring_len (const struct buffer *buf, int delim)
{
  int i = 0;
  struct buffer tmp = *buf;
  int c;

  while ((c = buf_read_u8 (&tmp)) >= 0)
    {
      ++i;
      if (c == delim)
	return i;
    }
  return -1;
}

/*
 * String parsing
 */

bool
buf_parse (struct buffer *buf, const int delim, char *line, const int size)
{
  bool eol = false;
  int n = 0;
  int c;

  ASSERT (size > 0);

  do
    {
      c = buf_read_u8 (buf);
      if (c < 0)
	eol = true;
      if (c <= 0 || c == delim)
	c = 0;
      if (n >= size)
	break;
      line[n++] = c;
    }
  while (c);

  line[size-1] = '\0';
  return !(eol && !strlen (line));
}

/*
 * Print a string which might be NULL
 */
const char *
np (const char *str)
{
  if (str)
    return str;
  else
    return "[NULL]";
}

/*
 * Classify and mutate strings based on character types.
 */

bool
char_class (const unsigned char c, const unsigned int flags)
{
  if (!flags)
    return false;
  if (flags & CC_ANY)
    return true;

  if ((flags & CC_NULL) && c == '\0')
    return true;

  if ((flags & CC_ALNUM) && isalnum (c))
    return true;
  if ((flags & CC_ALPHA) && isalpha (c))
    return true;
  if ((flags & CC_ASCII) && isascii (c))
    return true;
  if ((flags & CC_CNTRL) && iscntrl (c))
    return true;
  if ((flags & CC_DIGIT) && isdigit (c))
    return true;
  if ((flags & CC_PRINT) && isprint (c))
    return true;
  if ((flags & CC_PUNCT) && ispunct (c))
    return true;    
  if ((flags & CC_SPACE) && isspace (c))
    return true;
  if ((flags & CC_XDIGIT) && isxdigit (c))
    return true;

  if ((flags & CC_BLANK) && (c == ' ' || c == '\t'))
    return true;
  if ((flags & CC_NEWLINE) && c == '\n')
    return true;
  if ((flags & CC_CR) && c == '\r')
    return true;

  if ((flags & CC_BACKSLASH) && c == '\\')
    return true;
  if ((flags & CC_UNDERBAR) && c == '_')
    return true;
  if ((flags & CC_DASH) && c == '-')
    return true;
  if ((flags & CC_DOT) && c == '.')
    return true;
  if ((flags & CC_COMMA) && c == ',')
    return true;
  if ((flags & CC_COLON) && c == ':')
    return true;
  if ((flags & CC_SLASH) && c == '/')
    return true;
  if ((flags & CC_SINGLE_QUOTE) && c == '\'')
    return true;
  if ((flags & CC_DOUBLE_QUOTE) && c == '\"')
    return true;
  if ((flags & CC_REVERSE_QUOTE) && c == '`')
    return true;
  if ((flags & CC_AT) && c == '@')
    return true;
  if ((flags & CC_EQUAL) && c == '=')
    return true;

  return false;
}

static inline bool
char_inc_exc (const char c, const unsigned int inclusive, const unsigned int exclusive)
{
  return char_class (c, inclusive) && !char_class (c, exclusive);
}

bool
string_class (const char *str, const unsigned int inclusive, const unsigned int exclusive)
{
  char c;
  ASSERT (str);
  while ((c = *str++))
    {
      if (!char_inc_exc (c, inclusive, exclusive))
	return false;
    }
  return true;
}

/*
 * Modify string in place.
 * Guaranteed to not increase string length.
 */
bool
string_mod (char *str, const unsigned int inclusive, const unsigned int exclusive, const char replace)
{
  const char *in = str;
  bool ret = true;

  ASSERT (str);

  while (true)
    {
      char c = *in++;
      if (c)
	{
	  if (!char_inc_exc (c, inclusive, exclusive))
	    {
	      c = replace;
	      ret = false;
	    }
	  if (c)
	    *str++ = c;
	}
      else
	{
	  *str = '\0';
	  break;
	}
    }
  return ret;
}

const char *
string_mod_const (const char *str,
		  const unsigned int inclusive,
		  const unsigned int exclusive,
		  const char replace,
		  struct gc_arena *gc)
{
  if (str)
    {
      char *buf = string_alloc (str, gc);
      string_mod (buf, inclusive, exclusive, replace);
      return buf;
    }
  else
    return NULL;
}

void
string_replace_leading (char *str, const char match, const char replace)
{
  ASSERT (match != '\0');
  while (*str)
    {
      if (*str == match)
	*str = replace;
      else
	break;
      ++str;
    }
}

#ifdef CHARACTER_CLASS_DEBUG

#define CC_INCLUDE    (CC_PRINT)
#define CC_EXCLUDE    (0)
#define CC_REPLACE    ('.')

void
character_class_debug (void)
{
  char buf[256];

  while (fgets (buf, sizeof (buf), stdin) != NULL)
    {
      string_mod (buf, CC_INCLUDE, CC_EXCLUDE, CC_REPLACE);
      printf ("%s", buf);
    }
}

#endif

#ifdef VERIFY_ALIGNMENT
void
valign4 (const struct buffer *buf, const char *file, const int line)
{
  if (buf && buf->len)
    {
      int msglevel = D_ALIGN_DEBUG;
      const unsigned int u = (unsigned int) BPTR (buf);

      if (u & (PAYLOAD_ALIGN-1))
	msglevel = D_ALIGN_ERRORS;

      msg (msglevel, "%sAlignment at %s/%d ptr=" ptr_format " OLC=%d/%d/%d I=%s/%d",
	   (msglevel == D_ALIGN_ERRORS) ? "ERROR: " : "",
	   file,
	   line,
	   (ptr_type)buf->data,
	   buf->offset,
	   buf->len,
	   buf->capacity,
	   buf_debug_file (buf),
	   buf_debug_line (buf));
    }
}
#endif

/*
 * struct buffer_list
 */

#ifdef ENABLE_BUFFER_LIST

struct buffer_list *
buffer_list_new (const int max_size)
{
  struct buffer_list *ret;
  ALLOC_OBJ_CLEAR (ret, struct buffer_list);
  ret->max_size = max_size;
  ret->size = 0;
  return ret;
}

void
buffer_list_free (struct buffer_list *ol)
{
  if (ol)
    {
      buffer_list_reset (ol);
      free (ol);
    }
}

bool
buffer_list_defined (const struct buffer_list *ol)
{
  return ol->head != NULL;
}

void
buffer_list_reset (struct buffer_list *ol)
{
  struct buffer_entry *e = ol->head;
  while (e)
    {
      struct buffer_entry *next = e->next;
      free_buf (&e->buf);
      free (e);
      e = next;
    }
  ol->head = ol->tail = NULL;
  ol->size = 0;
}

void
buffer_list_push (struct buffer_list *ol, const unsigned char *str)
{
  if (str)
    {
      const size_t len = strlen ((const char *)str);
      struct buffer_entry *e = buffer_list_push_data (ol, str, len+1);
      if (e)
	e->buf.len = len; /* Don't count trailing '\0' as part of length */
    }
}

struct buffer_entry *
buffer_list_push_data (struct buffer_list *ol, const uint8_t *data, size_t size)
{
  struct buffer_entry *e = NULL;
  if (data && (!ol->max_size || ol->size < ol->max_size))
    {
      ALLOC_OBJ_CLEAR (e, struct buffer_entry);

      ++ol->size;
      if (ol->tail)
	{
	  ASSERT (ol->head);
	  ol->tail->next = e;
	}
      else
	{
	  ASSERT (!ol->head);
	  ol->head = e;
	}
      e->buf = alloc_buf (size);
      memcpy (e->buf.data, data, size);
      e->buf.len = (int)size;
      ol->tail = e;
    }
  return e;
}

struct buffer *
buffer_list_peek (struct buffer_list *ol)
{
  if (ol && ol->head)
    return &ol->head->buf;
  else
    return NULL;
}

void
buffer_list_aggregate (struct buffer_list *bl, const size_t max)
{
  if (bl->head)
    {
      struct buffer_entry *more = bl->head;
      size_t size = 0;
      int count = 0;
      for (count = 0; more && size <= max; ++count)
	{
	  size += BLEN(&more->buf);
	  more = more->next;
	}

      if (count >= 2)
	{
	  int i;
	  struct buffer_entry *e = bl->head, *f;

	  ALLOC_OBJ_CLEAR (f, struct buffer_entry);
	  f->buf.data = malloc (size);
	  check_malloc_return (f->buf.data);
	  f->buf.capacity = size;
	  for (i = 0; e && i < count; ++i)
	    {
	      struct buffer_entry *next = e->next;
	      buf_copy (&f->buf, &e->buf);
	      free_buf (&e->buf);
	      free (e);
	      e = next;
	    }
	  bl->head = f;
	  f->next = more;
	  if (!more)
	    bl->tail = f;
	}
    }
}

void
buffer_list_pop (struct buffer_list *ol)
{
  if (ol && ol->head)
    {
      struct buffer_entry *e = ol->head->next;
      free_buf (&ol->head->buf);
      free (ol->head);
      ol->head = e;
      --ol->size;
      if (!e)
	ol->tail = NULL;
    }
}

void
buffer_list_advance (struct buffer_list *ol, int n)
{
  if (ol->head)
    {
      struct buffer *buf = &ol->head->buf;
      ASSERT (buf_advance (buf, n));
      if (!BLEN (buf))
	buffer_list_pop (ol);
    }
}

struct buffer_list *
buffer_list_file (const char *fn, int max_line_len)
{
  FILE *fp = fopen (fn, "r");
  struct buffer_list *bl = NULL;

  if (fp)
    {
      char *line = (char *) malloc (max_line_len);
      if (line)
	{
	  bl = buffer_list_new (0);
	  while (fgets (line, max_line_len, fp) != NULL)
	    buffer_list_push (bl, (unsigned char *)line);
	  free (line);
	}
      fclose (fp);
    }
  return bl;
}

#endif
