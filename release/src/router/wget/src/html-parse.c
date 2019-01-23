/* HTML parser for Wget.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2008, 2009, 2010, 2011, 2015 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

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

/* The only entry point to this module is map_html_tags(), which see.  */

/* TODO:

   - Allow hooks for callers to process contents outside tags.  This
     is needed to implement handling <style> and <script>.  The
     taginfo structure already carries the information about where the
     tags are, but this is not enough, because one would also want to
     skip the comments.  (The funny thing is that for <style> and
     <script> you *don't* want to skip comments!)

   - Create a test suite for regression testing. */

/* HISTORY:

   This is the third HTML parser written for Wget.  The first one was
   written some time during the Geturl 1.0 beta cycle, and was very
   inefficient and buggy.  It also contained some very complex code to
   remember a list of parser states, because it was supposed to be
   reentrant.

   The second HTML parser was written for Wget 1.4 (the first version
   by the name `Wget'), and was a complete rewrite.  Although the new
   parser behaved much better and made no claims of reentrancy, it
   still shared many of the fundamental flaws of the old version -- it
   only regarded HTML in terms tag-attribute pairs, where the
   attribute's value was a URL to be returned.  Any other property of
   HTML, such as <base href=...>, or strange way to specify a URL,
   such as <meta http-equiv=Refresh content="0; URL=..."> had to be
   crudely hacked in -- and the caller had to be aware of these hacks.
   Like its predecessor, this parser did not support HTML comments.

   After Wget 1.5.1 was released, I set out to write a third HTML
   parser.  The objectives of the new parser were to: (1) provide a
   clean way to analyze HTML lexically, (2) separate interpretation of
   the markup from the parsing process, (3) be as correct as possible,
   e.g. correctly skipping comments and other SGML declarations, (4)
   understand the most common errors in markup and skip them or be
   relaxed towrds them, and (5) be reasonably efficient (no regexps,
   minimum copying and minimum or no heap allocation).

   I believe this parser meets all of the above goals.  It is
   reasonably well structured, and could be relatively easily
   separated from Wget and used elsewhere.  While some of its
   intrinsic properties limit its value as a general-purpose HTML
   parser, I believe that, with minimum modifications, it could serve
   as a backend for one.

   Due to time and other constraints, this parser was not integrated
   into Wget until the version 1.7. */

/* DESCRIPTION:

   The single entry point of this parser is map_html_tags(), which
   works by calling a function you specify for each tag.  The function
   gets called with the pointer to a structure describing the tag and
   its attributes.  */

/* To test as standalone, compile with `-DSTANDALONE -I.'.  You'll
   still need Wget headers to compile.  */

#include "wget.h"

#ifdef STANDALONE
# define I_REALLY_WANT_CTYPE_MACROS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "html-parse.h"

#ifdef STANDALONE
# undef xmalloc
# undef xrealloc
# undef xfree
# define xmalloc malloc
# define xrealloc realloc
# define xfree free

# undef c_isspace
# undef c_isdigit
# undef c_isxdigit
# undef c_isalpha
# undef c_isalnum
# undef c_tolower
# undef c_toupper

# define c_isspace(x) isspace (x)
# define c_isdigit(x) isdigit (x)
# define c_isxdigit(x) isxdigit (x)
# define c_isalpha(x) isalpha (x)
# define c_isalnum(x) isalnum (x)
# define c_tolower(x) tolower (x)
# define c_toupper(x) toupper (x)

struct hash_table {
  int dummy;
};
static void *
hash_table_get (const struct hash_table *ht, void *ptr)
{
  return ptr;
}
#else  /* not STANDALONE */
# include "hash.h"
#endif

/* Pool support.  A pool is a resizable chunk of memory.  It is first
   allocated on the stack, and moved to the heap if it needs to be
   larger than originally expected.  map_html_tags() uses it to store
   the zero-terminated names and values of tags and attributes.

   Thus taginfo->name, and attr->name and attr->value for each
   attribute, do not point into separately allocated areas, but into
   different parts of the pool, separated only by terminating zeros.
   This ensures minimum amount of allocation and, for most tags, no
   allocation because the entire pool is kept on the stack.  */

struct pool {
  char *contents;               /* pointer to the contents. */
  int size;                     /* size of the pool. */
  int tail;                     /* next available position index. */
  bool resized;                 /* whether the pool has been resized
                                   using malloc. */

  char *orig_contents;          /* original pool contents, usually
                                   stack-allocated.  used by POOL_FREE
                                   to restore the pool to the initial
                                   state. */
  int orig_size;
};

/* Initialize the pool to hold INITIAL_SIZE bytes of storage. */

#define POOL_INIT(p, initial_storage, initial_size) do {        \
  struct pool *P = (p);                                         \
  P->contents = (initial_storage);                              \
  P->size = (initial_size);                                     \
  P->tail = 0;                                                  \
  P->resized = false;                                           \
  P->orig_contents = P->contents;                               \
  P->orig_size = P->size;                                       \
} while (0)

/* Grow the pool to accommodate at least SIZE new bytes.  If the pool
   already has room to accommodate SIZE bytes of data, this is a no-op.  */

#define POOL_GROW(p, increase)                                  \
  GROW_ARRAY ((p)->contents, (p)->size, (p)->tail + (increase), \
              (p)->resized, char)

/* Append text in the range [beg, end) to POOL.  No zero-termination
   is done.  */

#define POOL_APPEND(p, beg, end) do {                   \
  const char *PA_beg = (beg);                           \
  int PA_size = (end) - PA_beg;                         \
  POOL_GROW (p, PA_size);                               \
  memcpy ((p)->contents + (p)->tail, PA_beg, PA_size);  \
  (p)->tail += PA_size;                                 \
} while (0)

/* Append one character to the pool.  Can be used to zero-terminate
   pool strings.  */

#define POOL_APPEND_CHR(p, ch) do {             \
  char PAC_char = (ch);                         \
  POOL_GROW (p, 1);                             \
  (p)->contents[(p)->tail++] = PAC_char;        \
} while (0)

/* Forget old pool contents.  The allocated memory is not freed. */
#define POOL_REWIND(p) (p)->tail = 0

/* Free heap-allocated memory for contents of POOL.  This calls
   xfree() if the memory was allocated through malloc.  It also
   restores `contents' and `size' to their original, pre-malloc
   values.  That way after POOL_FREE, the pool is fully usable, just
   as if it were freshly initialized with POOL_INIT.  */

#define POOL_FREE(p) do {                       \
  struct pool *P = p;                           \
  if (P->resized)                               \
    xfree (P->contents);                        \
  P->contents = P->orig_contents;               \
  P->size = P->orig_size;                       \
  P->tail = 0;                                  \
  P->resized = false;                           \
} while (0)

/* Used for small stack-allocated memory chunks that might grow.  Like
   DO_REALLOC, this macro grows BASEVAR as necessary to take
   NEEDED_SIZE items of TYPE.

   The difference is that on the first resize, it will use
   malloc+memcpy rather than realloc.  That way you can stack-allocate
   the initial chunk, and only resort to heap allocation if you
   stumble upon large data.

   After the first resize, subsequent ones are performed with realloc,
   just like DO_REALLOC.  */

#define GROW_ARRAY(basevar, sizevar, needed_size, resized, type) do {           \
  long ga_needed_size = (needed_size);                                          \
  long ga_newsize = (sizevar);                                                  \
  while (ga_newsize < ga_needed_size)                                           \
    ga_newsize <<= 1;                                                           \
  if (ga_newsize != (sizevar))                                                  \
    {                                                                           \
      if (resized)                                                              \
        basevar = xrealloc (basevar, ga_newsize * sizeof (type));               \
      else                                                                      \
        {                                                                       \
          void *ga_new = xmalloc (ga_newsize * sizeof (type));                  \
          memcpy (ga_new, basevar, (sizevar) * sizeof (type));                  \
          (basevar) = ga_new;                                                   \
          resized = true;                                                       \
        }                                                                       \
      (sizevar) = ga_newsize;                                                   \
    }                                                                           \
} while (0)

/* Test whether n+1-sized entity name fits in P.  We don't support
   IE-style non-terminated entities, e.g. "&ltfoo" -> "<foo".
   However, "&lt;foo" will work, as will "&lt!foo", "&lt", etc.  In
   other words an entity needs to be terminated by either a
   non-alphanumeric or the end of string.  */
#define FITS(p, n) (p + n == end || (p + n < end && !c_isalnum (p[n])))

/* Macros that test entity names by returning true if P is followed by
   the specified characters.  */
#define ENT1(p, c0) (FITS (p, 1) && p[0] == c0)
#define ENT2(p, c0, c1) (FITS (p, 2) && p[0] == c0 && p[1] == c1)
#define ENT3(p, c0, c1, c2) (FITS (p, 3) && p[0]==c0 && p[1]==c1 && p[2]==c2)

/* Increment P by INC chars.  If P lands at a semicolon, increment it
   past the semicolon.  This ensures that e.g. "&lt;foo" is converted
   to "<foo", but "&lt,foo" to "<,foo".  */
#define SKIP_SEMI(p, inc) (p += inc, p < end && *p == ';' ? ++p : p)

struct tagstack_item {
  const char *tagname_begin;
  const char *tagname_end;
  const char *contents_begin;
  struct tagstack_item *prev;
  struct tagstack_item *next;
};

static struct tagstack_item *
tagstack_push (struct tagstack_item **head, struct tagstack_item **tail)
{
  struct tagstack_item *ts = xmalloc(sizeof(struct tagstack_item));
  if (*head == NULL)
    {
      *head = *tail = ts;
      ts->prev = ts->next = NULL;
    }
  else
    {
      (*tail)->next = ts;
      ts->prev = *tail;
      *tail = ts;
      ts->next = NULL;
    }

  return ts;
}

/* remove ts and everything after it from the stack */
static void
tagstack_pop (struct tagstack_item **head, struct tagstack_item **tail,
              struct tagstack_item *ts)
{
  if (*head == NULL)
    return;

  if (ts == *tail)
    {
      if (ts == *head)
        {
          xfree (ts);
          *head = *tail = NULL;
        }
      else
        {
          ts->prev->next = NULL;
          *tail = ts->prev;
          xfree (ts);
        }
    }
  else
    {
      if (ts == *head)
        {
          *head = NULL;
        }
      *tail = ts->prev;

      if (ts->prev)
        {
          ts->prev->next = NULL;
        }
      while (ts)
        {
          struct tagstack_item *p = ts->next;
          xfree (ts);
          ts = p;
        }
    }
}

static struct tagstack_item *
tagstack_find (struct tagstack_item *tail, const char *tagname_begin,
               const char *tagname_end)
{
  int len = tagname_end - tagname_begin;
  while (tail)
    {
      if (len == (tail->tagname_end - tail->tagname_begin))
        {
          if (0 == strncasecmp (tail->tagname_begin, tagname_begin, len))
            return tail;
        }
      tail = tail->prev;
    }
  return NULL;
}

/* Decode the HTML character entity at *PTR, considering END to be end
   of buffer.  It is assumed that the "&" character that marks the
   beginning of the entity has been seen at *PTR-1.  If a recognized
   ASCII entity is seen, it is returned, and *PTR is moved to the end
   of the entity.  Otherwise, -1 is returned and *PTR left unmodified.

   The recognized entities are: &lt, &gt, &amp, &apos, and &quot.  */

static int
decode_entity (const char **ptr, const char *end)
{
  const char *p = *ptr;
  int value = -1;

  if (++p == end)
    return -1;

  switch (*p++)
    {
    case '#':
      /* Process numeric entities "&#DDD;" and "&#xHH;".  */
      {
        int digits = 0;
        value = 0;
        if (*p == 'x')
          for (++p; value < 256 && p < end && c_isxdigit (*p); p++, digits++)
            value = (value << 4) + XDIGIT_TO_NUM (*p);
        else
          for (; value < 256 && p < end && c_isdigit (*p); p++, digits++)
            value = (value * 10) + (*p - '0');
        if (!digits)
          return -1;
        /* Don't interpret 128+ codes and NUL because we cannot
           portably reinserted them into HTML.  */
        if (!value || (value & ~0x7f))
          return -1;
        *ptr = SKIP_SEMI (p, 0);
        return value;
      }
    /* Process named ASCII entities.  */
    case 'g':
      if (ENT1 (p, 't'))
        value = '>', *ptr = SKIP_SEMI (p, 1);
      break;
    case 'l':
      if (ENT1 (p, 't'))
        value = '<', *ptr = SKIP_SEMI (p, 1);
      break;
    case 'a':
      if (ENT2 (p, 'm', 'p'))
        value = '&', *ptr = SKIP_SEMI (p, 2);
      else if (ENT3 (p, 'p', 'o', 's'))
        /* handle &apos for the sake of the XML/XHTML crowd. */
        value = '\'', *ptr = SKIP_SEMI (p, 3);
      break;
    case 'q':
      if (ENT3 (p, 'u', 'o', 't'))
        value = '\"', *ptr = SKIP_SEMI (p, 3);
      break;
    }
  return value;
}
#undef ENT1
#undef ENT2
#undef ENT3
#undef FITS
#undef SKIP_SEMI

enum {
  AP_DOWNCASE           = 1,
  AP_DECODE_ENTITIES    = 2,
  AP_TRIM_BLANKS        = 4
};

/* Copy the text in the range [BEG, END) to POOL, optionally
   performing operations specified by FLAGS.  FLAGS may be any
   combination of AP_DOWNCASE, AP_DECODE_ENTITIES and AP_TRIM_BLANKS
   with the following meaning:

   * AP_DOWNCASE -- downcase all the letters;

   * AP_DECODE_ENTITIES -- decode the named and numeric entities in
     the ASCII range when copying the string.

   * AP_TRIM_BLANKS -- ignore blanks at the beginning and at the end
     of text, as well as embedded newlines.  */

static void
convert_and_copy (struct pool *pool, const char *beg, const char *end, int flags)
{
  int old_tail = pool->tail;

  /* Skip blanks if required.  We must do this before entities are
     processed, so that blanks can still be inserted as, for instance,
     `&#32;'.  */
  if (flags & AP_TRIM_BLANKS)
    {
      while (beg < end && c_isspace (*beg))
        ++beg;
      while (end > beg && c_isspace (end[-1]))
        --end;
    }

  if (flags & AP_DECODE_ENTITIES)
    {
      /* Grow the pool, then copy the text to the pool character by
         character, processing the encountered entities as we go
         along.

         It's safe (and necessary) to grow the pool in advance because
         processing the entities can only *shorten* the string, it can
         never lengthen it.  */
      const char *from = beg;
      char *to;
      bool squash_newlines = !!(flags & AP_TRIM_BLANKS);

      POOL_GROW (pool, end - beg);
      to = pool->contents + pool->tail;

      while (from < end)
        {
          if (*from == '&')
            {
              int entity = decode_entity (&from, end);
              if (entity != -1)
                *to++ = entity;
              else
                *to++ = *from++;
            }
          else if ((*from == '\n' || *from == '\r') && squash_newlines)
            ++from;
          else
            *to++ = *from++;
        }
      /* Verify that we haven't exceeded the original size.  (It
         shouldn't happen, hence the assert.)  */
      assert (to - (pool->contents + pool->tail) <= end - beg);

      /* Make POOL's tail point to the position following the string
         we've written.  */
      pool->tail = to - pool->contents;
      POOL_APPEND_CHR (pool, '\0');
    }
  else
    {
      /* Just copy the text to the pool.  */
      POOL_APPEND (pool, beg, end);
      POOL_APPEND_CHR (pool, '\0');
    }

  if (flags & AP_DOWNCASE)
    {
      char *p = pool->contents + old_tail;
      for (; *p; p++)
        *p = c_tolower (*p);
    }
}

/* Originally we used to adhere to rfc 1866 here, and allowed only
   letters, digits, periods, and hyphens as names (of tags or
   attributes).  However, this broke too many pages which used
   proprietary or strange attributes, e.g. <img src="a.gif"
   v:shapes="whatever">.

   So now we allow any character except:
     * whitespace
     * 8-bit and control chars
     * characters that clearly cannot be part of name:
       '=', '<', '>', '/'.

   This only affects attribute and tag names; attribute values allow
   an even greater variety of characters.  */

#define NAME_CHAR_P(x) ((x) > 32 && (x) < 127                           \
                        && (x) != '=' && (x) != '<' && (x) != '>'       \
                        && (x) != '/')

#ifdef STANDALONE
static int comment_backout_count;
#endif

/* Advance over an SGML declaration, such as <!DOCTYPE ...>.  In
   strict comments mode, this is used for skipping over comments as
   well.

   To recap: any SGML declaration may have comments associated with
   it, e.g.
       <!MY-DECL -- isn't this fun? -- foo bar>

   An HTML comment is merely an empty declaration (<!>) with a comment
   attached, like this:
       <!-- some stuff here -->

   Several comments may be embedded in one comment declaration:
       <!-- have -- -- fun -->

   Whitespace is allowed between and after the comments, but not
   before the first comment.  Additionally, this function attempts to
   handle double quotes in SGML declarations correctly.  */

static const char *
advance_declaration (const char *beg, const char *end)
{
  const char *p = beg;
  char quote_char = '\0';       /* shut up, gcc! */
  char ch;

  enum {
    AC_S_DONE,
    AC_S_BACKOUT,
    AC_S_BANG,
    AC_S_DEFAULT,
    AC_S_DCLNAME,
    AC_S_DASH1,
    AC_S_DASH2,
    AC_S_COMMENT,
    AC_S_DASH3,
    AC_S_DASH4,
    AC_S_QUOTE1,
    AC_S_IN_QUOTE,
    AC_S_QUOTE2
  } state = AC_S_BANG;

  if (beg == end)
    return beg;
  ch = *p++;

  /* It looked like a good idea to write this as a state machine, but
     now I wonder...  */

  while (state != AC_S_DONE && state != AC_S_BACKOUT)
    {
      if (p == end)
        state = AC_S_BACKOUT;
      switch (state)
        {
        case AC_S_DONE:
        case AC_S_BACKOUT:
          break;
        case AC_S_BANG:
          if (ch == '!')
            {
              ch = *p++;
              state = AC_S_DEFAULT;
            }
          else
            state = AC_S_BACKOUT;
          break;
        case AC_S_DEFAULT:
          switch (ch)
            {
            case '-':
              state = AC_S_DASH1;
              break;
            case ' ':
            case '\t':
            case '\r':
            case '\n':
              ch = *p++;
              break;
            case '<':
            case '>':
              state = AC_S_DONE;
              break;
            case '\'':
            case '\"':
              state = AC_S_QUOTE1;
              break;
            default:
              if (NAME_CHAR_P (ch))
                state = AC_S_DCLNAME;
              else
                state = AC_S_BACKOUT;
              break;
            }
          break;
        case AC_S_DCLNAME:
          if (ch == '-')
            state = AC_S_DASH1;
          else if (NAME_CHAR_P (ch))
            ch = *p++;
          else
            state = AC_S_DEFAULT;
          break;
        case AC_S_QUOTE1:
          /* We must use 0x22 because broken assert macros choke on
             '"' and '\"'.  */
          assert (ch == '\'' || ch == 0x22);
          quote_char = ch;      /* cheating -- I really don't feel like
                                   introducing more different states for
                                   different quote characters. */
          ch = *p++;
          state = AC_S_IN_QUOTE;
          break;
        case AC_S_IN_QUOTE:
          if (ch == quote_char)
            state = AC_S_QUOTE2;
          else
            ch = *p++;
          break;
        case AC_S_QUOTE2:
          assert (ch == quote_char);
          ch = *p++;
          state = AC_S_DEFAULT;
          break;
        case AC_S_DASH1:
          assert (ch == '-');
          ch = *p++;
          state = AC_S_DASH2;
          break;
        case AC_S_DASH2:
          switch (ch)
            {
            case '-':
              ch = *p++;
              state = AC_S_COMMENT;
              break;
            default:
              state = AC_S_BACKOUT;
            }
          break;
        case AC_S_COMMENT:
          switch (ch)
            {
            case '-':
              state = AC_S_DASH3;
              break;
            default:
              ch = *p++;
              break;
            }
          break;
        case AC_S_DASH3:
          assert (ch == '-');
          ch = *p++;
          state = AC_S_DASH4;
          break;
        case AC_S_DASH4:
          switch (ch)
            {
            case '-':
              ch = *p++;
              state = AC_S_DEFAULT;
              break;
            default:
              state = AC_S_COMMENT;
              break;
            }
          break;
        }
    }

  if (state == AC_S_BACKOUT)
    {
#ifdef STANDALONE
      ++comment_backout_count;
#endif
      return beg + 1;
    }
  return p;
}

/* Find the first occurrence of the substring "-->" in [BEG, END) and
   return the pointer to the character after the substring.  If the
   substring is not found, return NULL.  */

static const char *
find_comment_end (const char *beg, const char *end)
{
  /* Open-coded Boyer-Moore search for "-->".  Examine the third char;
     if it's not '>' or '-', advance by three characters.  Otherwise,
     look at the preceding characters and try to find a match.  */

  const char *p = beg - 1;

  while ((p += 3) < end)
    switch (p[0])
      {
      case '>':
        if (p[-1] == '-' && p[-2] == '-')
          return p + 1;
        break;
      case '-':
      at_dash:
        if (p[-1] == '-')
          {
          at_dash_dash:
            if (++p == end) return NULL;
            switch (p[0])
              {
              case '>': return p + 1;
              case '-': goto at_dash_dash;
              }
          }
        else
          {
            if ((p += 2) >= end) return NULL;
            switch (p[0])
              {
              case '>':
                if (p[-1] == '-')
                  return p + 1;
                break;
              case '-':
                goto at_dash;
              }
          }
      }
  return NULL;
}

/* Return true if the string containing of characters inside [b, e) is
   present in hash table HT.  */

static bool
name_allowed (const struct hash_table *ht, const char *b, const char *e)
{
  char *copy;
  if (!ht)
    return true;
  BOUNDED_TO_ALLOCA (b, e, copy);
  return hash_table_get (ht, copy) != NULL;
}

/* Advance P (a char pointer), with the explicit intent of being able
   to read the next character.  If this is not possible, go to finish.  */

#define ADVANCE(p) do {                         \
  ++p;                                          \
  if (p >= end)                                 \
    goto finish;                                \
} while (0)

/* Skip whitespace, if any. */

#define SKIP_WS(p) do {                         \
  while (c_isspace (*p)) {                        \
    ADVANCE (p);                                \
  }                                             \
} while (0)

#ifdef STANDALONE
static int tag_backout_count;
#endif

/* Map MAPFUN over HTML tags in TEXT, which is SIZE characters long.
   MAPFUN will be called with two arguments: pointer to an initialized
   struct taginfo, and MAPARG.

   ALLOWED_TAGS and ALLOWED_ATTRIBUTES are hash tables the keys of
   which are the tags and attribute names that this function should
   use.  If ALLOWED_TAGS is NULL, all tags are processed; if
   ALLOWED_ATTRIBUTES is NULL, all attributes are returned.

   (Obviously, the caller can filter out unwanted tags and attributes
   just as well, but this is just an optimization designed to avoid
   unnecessary copying of tags/attributes which the caller doesn't
   care about.)  */

void
map_html_tags (const char *text, int size,
               void (*mapfun) (struct taginfo *, void *), void *maparg,
               int flags,
               const struct hash_table *allowed_tags,
               const struct hash_table *allowed_attributes)
{
  /* storage for strings passed to MAPFUN callback; if 256 bytes is
     too little, POOL_APPEND allocates more with malloc. */
  char pool_initial_storage[256];
  struct pool pool;

  const char *p = text;
  const char *end = text + size;

  struct attr_pair attr_pair_initial_storage[8];
  int attr_pair_size = countof (attr_pair_initial_storage);
  bool attr_pair_resized = false;
  struct attr_pair *pairs = attr_pair_initial_storage;

  struct tagstack_item *head = NULL;
  struct tagstack_item *tail = NULL;

  if (!size)
    return;

  POOL_INIT (&pool, pool_initial_storage, countof (pool_initial_storage));

  {
    int nattrs, end_tag;
    const char *tag_name_begin, *tag_name_end;
    const char *tag_start_position;
    bool uninteresting_tag;

  look_for_tag:
    POOL_REWIND (&pool);

    nattrs = 0;
    end_tag = 0;

    /* Find beginning of tag.  We use memchr() instead of the usual
       looping with ADVANCE() for speed. */
    p = memchr (p, '<', end - p);
    if (!p)
      goto finish;

    tag_start_position = p;
    ADVANCE (p);

    /* Establish the type of the tag (start-tag, end-tag or
       declaration).  */
    if (*p == '!')
      {
        if (!(flags & MHT_STRICT_COMMENTS)
            && p + 3 < end && p[1] == '-' && p[2] == '-')
          {
            /* If strict comments are not enforced and if we know
               we're looking at a comment, simply look for the
               terminating "-->".  Non-strict is the default because
               it works in other browsers and most HTML writers can't
               be bothered with getting the comments right.  */
            const char *comment_end = find_comment_end (p + 3, end);
            if (comment_end)
              p = comment_end;
          }
        else
          {
            /* Either in strict comment mode or looking at a non-empty
               declaration.  Real declarations are much less likely to
               be misused the way comments are, so advance over them
               properly regardless of strictness.  */
            p = advance_declaration (p, end);
          }
        if (p == end)
          goto finish;
        goto look_for_tag;
      }
    else if (*p == '/')
      {
        end_tag = 1;
        ADVANCE (p);
      }
    tag_name_begin = p;
    while (NAME_CHAR_P (*p))
      ADVANCE (p);
    if (p == tag_name_begin)
      goto look_for_tag;
    tag_name_end = p;
    SKIP_WS (p);

    if (!end_tag)
      {
        struct tagstack_item *ts = tagstack_push (&head, &tail);
        if (ts)
          {
            ts->tagname_begin  = tag_name_begin;
            ts->tagname_end    = tag_name_end;
            ts->contents_begin = NULL;
          }
      }

    if (end_tag && *p != '>' && *p != '<')
      goto backout_tag;

    if (!name_allowed (allowed_tags, tag_name_begin, tag_name_end))
      /* We can't just say "goto look_for_tag" here because we need
         the loop below to properly advance over the tag's attributes.  */
      uninteresting_tag = true;
    else
      {
        uninteresting_tag = false;
        convert_and_copy (&pool, tag_name_begin, tag_name_end, AP_DOWNCASE);
      }

    /* Find the attributes. */
    while (1)
      {
        const char *attr_name_begin, *attr_name_end;
        const char *attr_value_begin, *attr_value_end;
        const char *attr_raw_value_begin, *attr_raw_value_end;
        int operation = AP_DOWNCASE; /* stupid compiler. */

        SKIP_WS (p);

        if (*p == '/')
          {
            /* A slash at this point means the tag is about to be
               closed.  This is legal in XML and has been popularized
               in HTML via XHTML.  */
            /* <foo a=b c=d /> */
            /*              ^  */
            ADVANCE (p);
            SKIP_WS (p);
            if (*p != '<' && *p != '>')
              goto backout_tag;
          }

        /* Check for end of tag definition. */
        if (*p == '<' || *p == '>')
          break;

        /* Establish bounds of attribute name. */
        attr_name_begin = p;    /* <foo bar ...> */
                                /*      ^        */
        while (NAME_CHAR_P (*p))
          ADVANCE (p);
        attr_name_end = p;      /* <foo bar ...> */
                                /*         ^     */
        if (attr_name_begin == attr_name_end)
          goto backout_tag;

        /* Establish bounds of attribute value. */
        SKIP_WS (p);

        if (NAME_CHAR_P (*p) || *p == '/' || *p == '<' || *p == '>')
          {
            /* Minimized attribute syntax allows `=' to be omitted.
               For example, <UL COMPACT> is a valid shorthand for <UL
               COMPACT="compact">.  Even if such attributes are not
               useful to Wget, we need to support them, so that the
               tags containing them can be parsed correctly. */
            attr_raw_value_begin = attr_value_begin = attr_name_begin;
            attr_raw_value_end = attr_value_end = attr_name_end;
          }
        else if (*p == '=')
          {
            ADVANCE (p);
            SKIP_WS (p);
            if (*p == '\"' || *p == '\'')
              {
                bool newline_seen = false;
                char quote_char = *p;
                attr_raw_value_begin = p;
                ADVANCE (p);
                attr_value_begin = p; /* <foo bar="baz"> */
                                      /*           ^     */
                while (*p != quote_char)
                  {
                    if (!newline_seen && *p == '\n')
                      {
                        /* If a newline is seen within the quotes, it
                           is most likely that someone forgot to close
                           the quote.  In that case, we back out to
                           the value beginning, and terminate the tag
                           at either `>' or the delimiter, whichever
                           comes first.  Such a tag terminated at `>'
                           is discarded.  */
                        p = attr_value_begin;
                        newline_seen = true;
                        continue;
                      }
                    else if (newline_seen && (*p == '<' || *p == '>'))
                      break;
                    ADVANCE (p);
                  }
                attr_value_end = p; /* <foo bar="baz"> */
                                    /*              ^  */
                if (*p == quote_char)
                  ADVANCE (p);
                else
                  goto look_for_tag;
                attr_raw_value_end = p; /* <foo bar="baz"> */
                                        /*               ^ */
                operation = AP_DECODE_ENTITIES;
                if (flags & MHT_TRIM_VALUES)
                  operation |= AP_TRIM_BLANKS;
              }
            else
              {
                attr_value_begin = p; /* <foo bar=baz> */
                                      /*          ^    */
                /* According to SGML, a name token should consist only
                   of alphanumerics, . and -.  However, this is often
                   violated by, for instance, `%' in `width=75%'.
                   We'll be liberal and allow just about anything as
                   an attribute value.  */
                while (!c_isspace (*p) && *p != '<' && *p != '>')
                  ADVANCE (p);
                attr_value_end = p; /* <foo bar=baz qux=quix> */
                                    /*             ^          */
                if (attr_value_begin == attr_value_end)
                  /* <foo bar=> */
                  /*          ^ */
                  goto backout_tag;
                attr_raw_value_begin = attr_value_begin;
                attr_raw_value_end = attr_value_end;
                operation = AP_DECODE_ENTITIES;
              }
          }
        else
          {
            /* We skipped the whitespace and found something that is
               neither `=' nor the beginning of the next attribute's
               name.  Back out.  */
            goto backout_tag;   /* <foo bar [... */
                                /*          ^    */
          }

        /* If we're not interested in the tag, don't bother with any
           of the attributes.  */
        if (uninteresting_tag)
          continue;

        /* If we aren't interested in the attribute, skip it.  We
           cannot do this test any sooner, because our text pointer
           needs to correctly advance over the attribute.  */
        if (!name_allowed (allowed_attributes, attr_name_begin, attr_name_end))
          continue;

        GROW_ARRAY (pairs, attr_pair_size, nattrs + 1, attr_pair_resized,
                    struct attr_pair);

        pairs[nattrs].name_pool_index = pool.tail;
        convert_and_copy (&pool, attr_name_begin, attr_name_end, AP_DOWNCASE);

        pairs[nattrs].value_pool_index = pool.tail;
        convert_and_copy (&pool, attr_value_begin, attr_value_end, operation);
        pairs[nattrs].value_raw_beginning = attr_raw_value_begin;
        pairs[nattrs].value_raw_size = (attr_raw_value_end
                                        - attr_raw_value_begin);
        ++nattrs;
      }

    if (!end_tag && tail && (tail->tagname_begin == tag_name_begin))
      {
        tail->contents_begin = p+1;
      }

    if (uninteresting_tag)
      {
        ADVANCE (p);
        goto look_for_tag;
      }

    /* By now, we have a valid tag with a name and zero or more
       attributes.  Fill in the data and call the mapper function.  */
    {
      int i;
      struct taginfo taginfo;
      struct tagstack_item *ts = NULL;

      taginfo.name      = pool.contents;
      taginfo.end_tag_p = end_tag;
      taginfo.nattrs    = nattrs;
      /* We fill in the char pointers only now, when pool can no
         longer get realloc'ed.  If we did that above, we could get
         hosed by reallocation.  Obviously, after this point, the pool
         may no longer be grown.  */
      for (i = 0; i < nattrs; i++)
        {
          pairs[i].name = pool.contents + pairs[i].name_pool_index;
          pairs[i].value = pool.contents + pairs[i].value_pool_index;
        }
      taginfo.attrs = pairs;
      taginfo.start_position = tag_start_position;
      taginfo.end_position   = p + 1;
      taginfo.contents_begin = NULL;
      taginfo.contents_end = NULL;

      if (end_tag)
        {
          ts = tagstack_find (tail, tag_name_begin, tag_name_end);
          if (ts)
            {
              if (ts->contents_begin)
                {
                  taginfo.contents_begin = ts->contents_begin;
                  taginfo.contents_end   = tag_start_position;
                }
              tagstack_pop (&head, &tail, ts);
            }
        }

      mapfun (&taginfo, maparg);
      if (*p != '<')
        ADVANCE (p);
    }
    goto look_for_tag;

  backout_tag:
#ifdef STANDALONE
    ++tag_backout_count;
#endif
    /* The tag wasn't really a tag.  Treat its contents as ordinary
       data characters. */
    p = tag_start_position + 1;
    goto look_for_tag;
  }

 finish:
  POOL_FREE (&pool);
  if (attr_pair_resized)
    xfree (pairs);
  /* pop any tag stack that's left */
  tagstack_pop (&head, &tail, head);
}

#undef ADVANCE
#undef SKIP_WS
#undef SKIP_NON_WS

#ifdef STANDALONE
static void
test_mapper (struct taginfo *taginfo, void *arg)
{
  int i;

  printf ("%s%s", taginfo->end_tag_p ? "/" : "", taginfo->name);
  for (i = 0; i < taginfo->nattrs; i++)
    printf (" %s=%s", taginfo->attrs[i].name, taginfo->attrs[i].value);
  putchar ('\n');
  ++*(int *)arg;
}

int main ()
{
  int size = 256;
  char *x = xmalloc (size);
  int length = 0;
  int read_count;
  int tag_counter = 0;

#ifdef ENABLE_NLS
  /* Set the current locale.  */
  setlocale (LC_ALL, "");
  /* Set the text message domain.  */
  bindtextdomain ("wget", LOCALEDIR);
  textdomain ("wget");
#endif /* ENABLE_NLS */

  while ((read_count = fread (x + length, 1, size - length, stdin)))
    {
      length += read_count;
      size <<= 1;
      x = xrealloc (x, size);
    }

  map_html_tags (x, length, test_mapper, &tag_counter, 0, NULL, NULL);
  printf ("TAGS: %d\n", tag_counter);
  printf ("Tag backouts:     %d\n", tag_backout_count);
  printf ("Comment backouts: %d\n", comment_backout_count);
  return 0;
}
#endif /* STANDALONE */
