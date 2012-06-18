/*
 * zebra string function
 *
 * these functions are just very basic wrappers around exiting ones and
 * do not offer the protection that might be expected against buffer
 * overruns etc
 */

#include <zebra.h>

#include "str.h"

#ifndef HAVE_SNPRINTF
/*
 * snprint() is a real basic wrapper around the standard sprintf()
 * without any bounds checking
 */
int
snprintf(char *str, size_t size, const char *format, ...)
{
  va_list args;

  va_start (args, format);

  return vsprintf (str, format, args);
}
#endif

#ifndef HAVE_STRLCPY
/*
 * strlcpy is a safer version of strncpy(), checking the total
 * size of the buffer
 */
size_t
strlcpy(char *dst, const char *src, size_t size)
{
  strncpy(dst, src, size);

  return (strlen(dst));
}
#endif

#ifndef HAVE_STRLCAT
/*
 * strlcat is a safer version of strncat(), checking the total
 * size of the buffer
 */
size_t
strlcat(char *dst, const char *src, size_t size)
{
  /* strncpy(dst, src, size - strlen(dst)); */

  /* I've just added below code only for workable under Linux.  So
     need rewrite -- Kunihiro. */
  if (strlen (dst) + strlen (src) >= size)
    return -1;

  strcat (dst, src);

  return (strlen(dst));
}
#endif
