#include <stdint.h>
#include <stddef.h>

typedef struct md5_ctx {
	uint32_t A;
	uint32_t B;
	uint32_t C;
	uint32_t D;
	uint64_t total;
	uint32_t buflen;
	char buffer[128];
} md5_ctx_t;

void md5_begin(md5_ctx_t *ctx);
void md5_hash(const void *data, size_t length, md5_ctx_t *ctx);
void md5_end(void *resbuf, md5_ctx_t *ctx);
