
struct globals {
	pid_t helper_pid;
	unsigned timeout;
	unsigned opts;
	char *user;
	char *pass;
	FILE *fp0; // initial stdin
	char *opt_charset;
	char *content_type;
};

#define G (*ptr_to_globals)
#define timeout         (G.timeout  )
#define opts            (G.opts     )
//#define user            (G.user     )
//#define pass            (G.pass     )
//#define fp0             (G.fp0      )
//#define opt_charset     (G.opt_charset)
//#define content_type    (G.content_type)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	G.opt_charset = (char *)CONFIG_FEATURE_MIME_CHARSET; \
	G.content_type = (char *)"text/plain"; \
} while (0)

//char FAST_FUNC *parse_url(char *url, char **user, char **pass);

void FAST_FUNC launch_helper(const char **argv);
void FAST_FUNC get_cred_or_die(int fd);

const FAST_FUNC char *command(const char *fmt, const char *param);

void FAST_FUNC encode_base64(char *fname, const char *text, const char *eol);
void FAST_FUNC decode_base64(FILE *src_stream, FILE *dst_stream);
