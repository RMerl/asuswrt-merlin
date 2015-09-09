#include <stdbool.h>

#define	VPN_UPLOAD_DONE				0
#define	VPN_UPLOAD_NEED_CA_CERT		1
#define	VPN_UPLOAD_NEED_CERT		2
#define	VPN_UPLOAD_NEED_KEY			4
#define	VPN_UPLOAD_NEED_STATIC		8
#define VPN_UPLOAD_NEED_CRL			16
#define VPN_UPLOAD_NEED_EXTRA		32

#define MAX_PARMS 16

#define OPTION_PARM_SIZE 256
#define OPTION_LINE_SIZE 256

#define BUF_SIZE_MAX 1000000

#define BOOL_CAST(x) ((x) ? (true) : (false))

#define INLINE_FILE_TAG "[[INLINE]]"

/* size of an array */
#define SIZE(x) (sizeof(x)/sizeof(x[0]))

/* clear an object */
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define streq(x, y) (!strcmp((x), (y)))
#define likely(x)       __builtin_expect((x),1)

struct buffer
{
	int capacity;	//Size in bytes of memory allocated by malloc().
	int offset;	//Offset in bytes of the actual content within the allocated memory.
	int len;	//Length in bytes of the actual content within the allocated memory.
	uint8_t *data;	//Pointer to the allocated memory.
};


struct in_src {
# define IS_TYPE_FP 1
# define IS_TYPE_BUF 2
	int type;
	union {
		FILE *fp;
		struct buffer *multiline;
	} u;
};


static inline bool
buf_defined (const struct buffer *buf)
{
	return buf->data != NULL;
}

static inline bool
buf_valid (const struct buffer *buf)
{
	return likely (buf->data != NULL) && likely (buf->len >= 0);
}

static inline uint8_t *
buf_bptr (const struct buffer *buf)
{
	if (buf_valid (buf))
		return buf->data + buf->offset;
	else
		return NULL;
}

static int
buf_len (const struct buffer *buf)
{
	if (buf_valid (buf))
		return buf->len;
	else
		return 0;
}

static inline uint8_t *
buf_bend (const struct buffer *buf)
{
	return buf_bptr (buf) + buf_len (buf);
}

static inline bool
buf_size_valid (const size_t size)
{
	return likely (size < BUF_SIZE_MAX);
}

static inline char *
buf_str (const struct buffer *buf)
{
	return (char *)buf_bptr(buf);
}

static inline int
buf_forward_capacity (const struct buffer *buf)
{
	if (buf_valid (buf))
	{
		int ret = buf->capacity - (buf->offset + buf->len);
		if (ret < 0)
			ret = 0;
		return ret;
	}
	else
		return 0;
}

static inline bool
buf_advance (struct buffer *buf, int size)
{
	if (!buf_valid (buf) || size < 0 || buf->len < size)
		return false;
	buf->offset += size;
	buf->len -= size;
	return true;
}

static inline int
buf_read_u8 (struct buffer *buf)
{
	int ret;
	if (buf_len (buf) < 1)
		return -1;
	ret = *buf_bptr(buf);
		buf_advance (buf, 1);
	return ret;
}

bool buf_parse (struct buffer *buf, const int delim, char *line, const int size);
void buf_size_error (const size_t size);
void buf_clear (struct buffer *buf);
void free_buf (struct buffer *buf);
struct buffer alloc_buf (size_t size);
bool buf_printf (struct buffer *buf, const char *format, ...);
char *string_alloc (const char *str);
int parse_line (const char *line, char *p[], const int n, const int line_num);
int read_config_file (const char *file, int unit);
void reset_client_setting(int unit);
void parse_openvpn_status(int unit);
