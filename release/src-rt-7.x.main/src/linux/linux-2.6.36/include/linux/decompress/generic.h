#ifndef DECOMPRESS_GENERIC_H
#define DECOMPRESS_GENERIC_H

typedef int (*decompress_fn) (unsigned char *inbuf, int len,
			      int(*fill)(void*, unsigned int),
			      int(*flush)(void*, unsigned int),
			      unsigned char *outbuf,
			      int *posp,
			      void(*error)(char *x));



/* Utility routine to detect the decompression method */
decompress_fn decompress_method(const unsigned char *inbuf, int len,
				const char **name);

#endif
