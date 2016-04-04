#include "buffer.h"
#include "etag.h"

#if defined HAVE_STDINT_H
# include <stdint.h>
#elif defined HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <string.h>

int etag_is_equal(buffer *etag, const char *line, int weak_ok) {
	enum {
		START = 0,
		CHECK,
		CHECK_QUOTED,
		SKIP,
		SKIP_QUOTED,
		TAIL
	} state = START;

	const char *current;
	const char *tok_start;
	const char *tok = NULL;
	int matched;

	if ('*' == line[0] && '\0' == line[1]) {
		return 1;
	}

	if (!etag || buffer_string_is_empty(etag)) return 0;
	tok_start = etag->ptr;

	if ('W' == tok_start[0]) {
		if (!weak_ok || '/' != tok_start[1]) return 0; /* bad etag */
		tok_start = tok_start + 2;
	}

	if ('"' != tok_start[0]) return 0; /* bad etag */
	/* we start comparing after the first '"' */
	++tok_start;

	for (current = line; *current; ++current) {
		switch (state) {
		case START:
			/* wait for etag to start; ignore whitespace and ',' */
			switch (*current) {
			case 'W':
				/* weak etag always starts with 'W/"' */
				if ('/' != *++current) return 0; /* bad etag list */
				if ('"' != *++current) return 0; /* bad etag list */
				if (!weak_ok) {
					state = SKIP;
				} else {
					state = CHECK;
					tok = tok_start;
				}
				break;
			case '"':
				/* strong etag starts with '"' */
				state = CHECK;
				tok = tok_start;
				break;
			case ' ':
			case ',':
			case '\t':
			case '\r':
			case '\n':
				break;
			default:
				return 0; /* bad etag list */
			}
			break;
		case CHECK:
			/* compare etags (after the beginning '"')
			 * quoted-pairs must match too (i.e. quoted in both strings):
			 * > (RFC 2616:) both validators MUST be identical in every way
			 */
			matched = *tok && *tok == *current;
			++tok;
			switch (*current) {
			case '\\':
				state = matched ? CHECK_QUOTED : SKIP_QUOTED;
				break;
			case '"':
				if (*tok)  {
					/* bad etag - string should end after '"' */
					return 0;
				}
				if (matched) {
					/* matching etag: strings were equal */
					return 1;
				}

				state = TAIL;
				break;
			default:
				if (!matched) {
					/* strings not matching, skip remainder of etag */
					state = SKIP;
				}
				break;
			}
			break;
		case CHECK_QUOTED:
			if (!*tok || *tok != *current) {
				/* strings not matching, skip remainder of etag */
				state = SKIP;
				break;
			}
			++tok;
			state = CHECK;
			break;
		case SKIP:
			/* wait for final (not quoted) '"' */
			switch (*current) {
			case '\\':
				state = SKIP_QUOTED;
				break;
			case '"':
				state = TAIL;
				break;
			}
			break;
		case SKIP_QUOTED:
			state = SKIP;
			break;
		case TAIL:
			/* search for ',', ignore white space */
			switch (*current) {
			case ',':
				state = START;
				break;
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				break;
			default:
				return 0; /* bad etag list */
			}
			break;
		}
	}
	/* no matching etag found */
	return 0;
}

int etag_create(buffer *etag, struct stat *st,etag_flags_t flags) {
	if (0 == flags) return 0;

	buffer_reset(etag);

	if (flags & ETAG_USE_INODE) {
		buffer_append_int(etag, st->st_ino);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}
	
	if (flags & ETAG_USE_SIZE) {
		buffer_append_int(etag, st->st_size);
		buffer_append_string_len(etag, CONST_STR_LEN("-"));
	}
	
	if (flags & ETAG_USE_MTIME) {
		buffer_append_int(etag, st->st_mtime);
	}

	return 0;
}

int etag_mutate(buffer *mut, buffer *etag) {
	size_t i, len;
	uint32_t h;

	len = buffer_string_length(etag);
	for (h=0, i=0; i < len; ++i) h = (h<<5)^(h>>27)^(etag->ptr[i]);

	buffer_reset(mut);
	buffer_copy_string_len(mut, CONST_STR_LEN("\""));
	buffer_append_int(mut, h);
	buffer_append_string_len(mut, CONST_STR_LEN("\""));

	return 0;
}
