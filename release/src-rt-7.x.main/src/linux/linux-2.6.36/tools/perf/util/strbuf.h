#ifndef __PERF_STRBUF_H
#define __PERF_STRBUF_H


#include <assert.h>

extern char strbuf_slopbuf[];
struct strbuf {
	size_t alloc;
	size_t len;
	char *buf;
};

#define STRBUF_INIT  { 0, 0, strbuf_slopbuf }

/*----- strbuf life cycle -----*/
extern void strbuf_init(struct strbuf *buf, ssize_t hint);
extern void strbuf_release(struct strbuf *);
extern char *strbuf_detach(struct strbuf *, size_t *);

/*----- strbuf size related -----*/
static inline ssize_t strbuf_avail(const struct strbuf *sb) {
	return sb->alloc ? sb->alloc - sb->len - 1 : 0;
}

extern void strbuf_grow(struct strbuf *, size_t);

static inline void strbuf_setlen(struct strbuf *sb, size_t len) {
	if (!sb->alloc)
		strbuf_grow(sb, 0);
	assert(len < sb->alloc);
	sb->len = len;
	sb->buf[len] = '\0';
}

/*----- add data in your buffer -----*/
static inline void strbuf_addch(struct strbuf *sb, int c) {
	strbuf_grow(sb, 1);
	sb->buf[sb->len++] = c;
	sb->buf[sb->len] = '\0';
}

extern void strbuf_remove(struct strbuf *, size_t pos, size_t len);

extern void strbuf_add(struct strbuf *, const void *, size_t);
static inline void strbuf_addstr(struct strbuf *sb, const char *s) {
	strbuf_add(sb, s, strlen(s));
}

__attribute__((format(printf,2,3)))
extern void strbuf_addf(struct strbuf *sb, const char *fmt, ...);

extern ssize_t strbuf_read(struct strbuf *, int fd, ssize_t hint);

#endif /* __PERF_STRBUF_H */
