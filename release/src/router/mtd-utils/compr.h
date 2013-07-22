/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2004 Ferenc Havasi <havasi@inf.u-szeged.hu>,
 *                    University of Szeged, Hungary
 *
 * For licensing information, see the file 'LICENCE' in the
 * jffs2 directory.
 */

#ifndef __JFFS2_COMPR_H__
#define __JFFS2_COMPR_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "linux/jffs2.h"

#define CONFIG_JFFS2_ZLIB
#define CONFIG_JFFS2_RTIME
#define CONFIG_JFFS2_LZO

#define JFFS2_RUBINMIPS_PRIORITY 10
#define JFFS2_DYNRUBIN_PRIORITY  20
#define JFFS2_RTIME_PRIORITY     50
#define JFFS2_ZLIB_PRIORITY      60
#define JFFS2_LZO_PRIORITY       80

#define JFFS2_COMPR_MODE_NONE       0
#define JFFS2_COMPR_MODE_PRIORITY   1
#define JFFS2_COMPR_MODE_SIZE       2
#define JFFS2_COMPR_MODE_FAVOURLZO  3

#define kmalloc(a,b)                malloc(a)
#define kfree(a)                    free(a)
#ifndef GFP_KERNEL
#define GFP_KERNEL                  0
#endif

#define vmalloc(a)                  malloc(a)
#define vfree(a)                    free(a)

#define printk(...)                 fprintf(stderr,__VA_ARGS__)

#define KERN_EMERG
#define KERN_ALERT
#define KERN_CRIT
#define KERN_ERR
#define KERN_WARNING
#define KERN_NOTICE
#define KERN_INFO
#define KERN_DEBUG

struct list_head {
	struct list_head *next, *prev;
};

void jffs2_set_compression_mode(int mode);
int jffs2_get_compression_mode(void);
int jffs2_set_compression_mode_name(const char *mode_name);

int jffs2_enable_compressor_name(const char *name);
int jffs2_disable_compressor_name(const char *name);

int jffs2_set_compressor_priority(const char *name, int priority);

struct jffs2_compressor {
	struct list_head list;
	int priority;             /* used by prirority comr. mode */
	const char *name;
	char compr;               /* JFFS2_COMPR_XXX */
	int (*compress)(unsigned char *data_in, unsigned char *cpage_out,
			uint32_t *srclen, uint32_t *destlen);
	int (*decompress)(unsigned char *cdata_in, unsigned char *data_out,
			uint32_t cdatalen, uint32_t datalen);
	int usecount;
	int disabled;             /* if seted the compressor won't compress */
	unsigned char *compr_buf; /* used by size compr. mode */
	uint32_t compr_buf_size;  /* used by size compr. mode */
	uint32_t stat_compr_orig_size;
	uint32_t stat_compr_new_size;
	uint32_t stat_compr_blocks;
	uint32_t stat_decompr_blocks;
};

int jffs2_register_compressor(struct jffs2_compressor *comp);
int jffs2_unregister_compressor(struct jffs2_compressor *comp);

int jffs2_compressors_init(void);
int jffs2_compressors_exit(void);

uint16_t jffs2_compress(unsigned char *data_in, unsigned char **cpage_out,
		uint32_t *datalen, uint32_t *cdatalen);

/* If it is setted, a decompress will be called after every compress */
void jffs2_compression_check_set(int yesno);
int jffs2_compression_check_get(void);
int jffs2_compression_check_errorcnt_get(void);

char *jffs2_list_compressors(void);
char *jffs2_stats(void);

/* Compressor modules */

/* These functions will be called by jffs2_compressors_init/exit */
#ifdef CONFIG_JFFS2_ZLIB
int jffs2_zlib_init(void);
void jffs2_zlib_exit(void);
#endif
#ifdef CONFIG_JFFS2_RTIME
int jffs2_rtime_init(void);
void jffs2_rtime_exit(void);
#endif
#ifdef CONFIG_JFFS2_LZO
int jffs2_lzo_init(void);
void jffs2_lzo_exit(void);
#endif

#endif /* __JFFS2_COMPR_H__ */
