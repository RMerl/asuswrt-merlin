/* Declarations for wide characters */
/* This file must be included last because the redefinition of wchar_t may
   cause conflicts when system include files were included after it. */

#ifdef HAVE_WIDECHAR

# include <wchar.h>
# include <wctype.h>

#else /* !HAVE_WIDECHAR */

# include <ctype.h>
  /* Fallback for types */
# define wchar_t char
# define wint_t int
# define WEOF EOF
  /* Fallback for input operations */
# define fgetwc fgetc
# define getwc getc
# define getwchar getchar
# define fgetws fgets
  /* Fallback for output operations */
# define fputwc fputc
# define putwc putc
# define putwchar putchar
# define fputws fputs
  /* Fallback for character classification */
# define iswgraph isgraph
# define iswprint isprint
# define iswspace isspace
  /* Fallback for string functions */
# define wcschr strchr
# define wcsdup strdup
# define wcslen strlen

# define wcwidth(c) 1

#endif /* HAVE_WIDECHAR */
