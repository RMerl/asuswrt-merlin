#ifndef _DPRINTF_H
#define _DPRINTF_H

extern void show_message(char *fmt, ...);

extern int options;
#ifndef OPT_DEBUG
#  define OPT_DEBUG       0x0001
#endif

#ifdef DEBUG
#  define dprintf2(f, fmt, ...) if( options & OPT_DEBUG ) \
{ \
  show_message("%s,%d: " fmt, __FILE__, __LINE__ , ##__VA_ARGS__); \
}
#  define dprintf(x)		dprintf2 x
#else
#  define dprintf(x)
#endif

#endif
