/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2004 Ferenc Havasi <havasi@inf.u-szeged.hu>,
 *                    University of Szeged, Hungary
 *
 * For licensing information, see the file 'LICENCE' in this directory
 * in the jffs2 directory.
 */

#include "compr.h"
#include <string.h>
#include <stdlib.h>
#include <linux/jffs2.h>

#define FAVOUR_LZO_PERCENT 80

extern int page_size;

/* LIST IMPLEMENTATION (from linux/list.h) */

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void __list_add(struct list_head *new,
		struct list_head *prev,
		struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = (void *) 0;
	entry->prev = (void *) 0;
}

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each_entry(pos, head, member)                          \
	for (pos = list_entry((head)->next, typeof(*pos), member);      \
			&pos->member != (head);                                    \
			pos = list_entry(pos->member.next, typeof(*pos), member))


/* Available compressors are on this list */
static LIST_HEAD(jffs2_compressor_list);

/* Actual compression mode */
static int jffs2_compression_mode = JFFS2_COMPR_MODE_PRIORITY;

void jffs2_set_compression_mode(int mode)
{
	jffs2_compression_mode = mode;
}

int jffs2_get_compression_mode(void)
{
	return jffs2_compression_mode;
}

/* Statistics for blocks stored without compression */
static uint32_t none_stat_compr_blocks=0,none_stat_decompr_blocks=0,none_stat_compr_size=0;

/* Compression test stuffs */

static int jffs2_compression_check = 0;

static unsigned char *jffs2_compression_check_buf = NULL;

void jffs2_compression_check_set(int yesno)
{
	jffs2_compression_check = yesno;
}

int jffs2_compression_check_get(void)
{
	return jffs2_compression_check;
}

static int jffs2_error_cnt = 0;

int jffs2_compression_check_errorcnt_get(void)
{
	return jffs2_error_cnt;
}

#define JFFS2_BUFFER_FILL 0x55

/* Called before compression (if compression_check is setted) to prepare
   the buffer for buffer overflow test */
static void jffs2_decompression_test_prepare(unsigned char *buf, int size)
{
	memset(buf,JFFS2_BUFFER_FILL,size+1);
}

/* Called after compression (if compression_check is setted) to test the result */
static void jffs2_decompression_test(struct jffs2_compressor *compr,
		unsigned char *data_in, unsigned char *output_buf,
		uint32_t cdatalen, uint32_t datalen, uint32_t buf_size)
{
	uint32_t i;

	/* buffer overflow test */
	for (i=buf_size;i>cdatalen;i--) {
		if (output_buf[i]!=JFFS2_BUFFER_FILL) {
			fprintf(stderr,"COMPR_ERROR: buffer overflow at %s. "
					"(bs=%d csize=%d b[%d]=%d)\n", compr->name,
					buf_size, cdatalen, i, (int)(output_buf[i]));
			jffs2_error_cnt++;
			return;
		}
	}
	/* allocing temporary buffer for decompression */
	if (!jffs2_compression_check_buf) {
		jffs2_compression_check_buf = malloc(page_size);
		if (!jffs2_compression_check_buf) {
			fprintf(stderr,"No memory for buffer allocation. Compression check disabled.\n");
			jffs2_compression_check = 0;
			return;
		}
	}
	/* decompressing */
	if (!compr->decompress) {
		fprintf(stderr,"JFFS2 compression check: there is no decompress function at %s.\n", compr->name);
		jffs2_error_cnt++;
		return;
	}
	if (compr->decompress(output_buf,jffs2_compression_check_buf,cdatalen,datalen)) {
		fprintf(stderr,"JFFS2 compression check: decompression failed at %s.\n", compr->name);
		jffs2_error_cnt++;
	}
	/* validate decompression */
	else {
		for (i=0;i<datalen;i++) {
			if (data_in[i]!=jffs2_compression_check_buf[i]) {
				fprintf(stderr,"JFFS2 compression check: data mismatch at %s (pos %d).\n", compr->name, i);
				jffs2_error_cnt++;
				break;
			}
		}
	}
}

/*
 * Return 1 to use this compression
 */
static int jffs2_is_best_compression(struct jffs2_compressor *this,
		struct jffs2_compressor *best, uint32_t size, uint32_t bestsize)
{
	switch (jffs2_compression_mode) {
	case JFFS2_COMPR_MODE_SIZE:
		if (bestsize > size)
			return 1;
		return 0;
	case JFFS2_COMPR_MODE_FAVOURLZO:
		if ((this->compr == JFFS2_COMPR_LZO) && (bestsize > size))
			return 1;
		if ((best->compr != JFFS2_COMPR_LZO) && (bestsize > size))
			return 1;
		if ((this->compr == JFFS2_COMPR_LZO) && (bestsize > (size * FAVOUR_LZO_PERCENT / 100)))
			return 1;
		if ((bestsize * FAVOUR_LZO_PERCENT / 100) > size)
			return 1;

		return 0;
	}
	/* Shouldn't happen */
	return 0;
}

/* jffs2_compress:
 * @data: Pointer to uncompressed data
 * @cdata: Pointer to returned pointer to buffer for compressed data
 * @datalen: On entry, holds the amount of data available for compression.
 *	On exit, expected to hold the amount of data actually compressed.
 * @cdatalen: On entry, holds the amount of space available for compressed
 *	data. On exit, expected to hold the actual size of the compressed
 *	data.
 *
 * Returns: Lower byte to be stored with data indicating compression type used.
 * Zero is used to show that the data could not be compressed - the
 * compressed version was actually larger than the original.
 * Upper byte will be used later. (soon)
 *
 * If the cdata buffer isn't large enough to hold all the uncompressed data,
 * jffs2_compress should compress as much as will fit, and should set
 * *datalen accordingly to show the amount of data which were compressed.
 */
uint16_t jffs2_compress( unsigned char *data_in, unsigned char **cpage_out,
		uint32_t *datalen, uint32_t *cdatalen)
{
	int ret = JFFS2_COMPR_NONE;
	int compr_ret;
	struct jffs2_compressor *this, *best=NULL;
	unsigned char *output_buf = NULL, *tmp_buf;
	uint32_t orig_slen, orig_dlen;
	uint32_t best_slen=0, best_dlen=0;

	switch (jffs2_compression_mode) {
		case JFFS2_COMPR_MODE_NONE:
			break;
		case JFFS2_COMPR_MODE_PRIORITY:
			orig_slen = *datalen;
			orig_dlen = *cdatalen;
			output_buf = malloc(orig_dlen+jffs2_compression_check);
			if (!output_buf) {
				fprintf(stderr,"mkfs.jffs2: No memory for compressor allocation. Compression failed.\n");
				goto out;
			}
			list_for_each_entry(this, &jffs2_compressor_list, list) {
				/* Skip decompress-only backwards-compatibility and disabled modules */
				if ((!this->compress)||(this->disabled))
					continue;

				this->usecount++;

				if (jffs2_compression_check) /*preparing output buffer for testing buffer overflow */
					jffs2_decompression_test_prepare(output_buf, orig_dlen);

				*datalen  = orig_slen;
				*cdatalen = orig_dlen;
				compr_ret = this->compress(data_in, output_buf, datalen, cdatalen);
				this->usecount--;
				if (!compr_ret) {
					ret = this->compr;
					this->stat_compr_blocks++;
					this->stat_compr_orig_size += *datalen;
					this->stat_compr_new_size  += *cdatalen;
					if (jffs2_compression_check)
						jffs2_decompression_test(this, data_in, output_buf, *cdatalen, *datalen, orig_dlen);
					break;
				}
			}
			if (ret == JFFS2_COMPR_NONE) free(output_buf);
			break;
		case JFFS2_COMPR_MODE_FAVOURLZO:
		case JFFS2_COMPR_MODE_SIZE:
			orig_slen = *datalen;
			orig_dlen = *cdatalen;
			list_for_each_entry(this, &jffs2_compressor_list, list) {
				uint32_t needed_buf_size;

				if (jffs2_compression_mode == JFFS2_COMPR_MODE_FAVOURLZO)
					needed_buf_size = orig_slen + jffs2_compression_check;
				else
					needed_buf_size = orig_dlen + jffs2_compression_check;

				/* Skip decompress-only backwards-compatibility and disabled modules */
				if ((!this->compress)||(this->disabled))
					continue;
				/* Allocating memory for output buffer if necessary */
				if ((this->compr_buf_size < needed_buf_size) && (this->compr_buf)) {
					free(this->compr_buf);
					this->compr_buf_size=0;
					this->compr_buf=NULL;
				}
				if (!this->compr_buf) {
					tmp_buf = malloc(needed_buf_size);
					if (!tmp_buf) {
						fprintf(stderr,"mkfs.jffs2: No memory for compressor allocation. (%d bytes)\n",orig_dlen);
						continue;
					}
					else {
						this->compr_buf = tmp_buf;
						this->compr_buf_size = orig_dlen;
					}
				}
				this->usecount++;
				if (jffs2_compression_check) /*preparing output buffer for testing buffer overflow */
					jffs2_decompression_test_prepare(this->compr_buf,this->compr_buf_size);
				*datalen  = orig_slen;
				*cdatalen = orig_dlen;
				compr_ret = this->compress(data_in, this->compr_buf, datalen, cdatalen);
				this->usecount--;
				if (!compr_ret) {
					if (jffs2_compression_check)
						jffs2_decompression_test(this, data_in, this->compr_buf, *cdatalen, *datalen, this->compr_buf_size);
					if (((!best_dlen) || jffs2_is_best_compression(this, best, *cdatalen, best_dlen))
								&& (*cdatalen < *datalen)) {
						best_dlen = *cdatalen;
						best_slen = *datalen;
						best = this;
					}
				}
			}
			if (best_dlen) {
				*cdatalen = best_dlen;
				*datalen  = best_slen;
				output_buf = best->compr_buf;
				best->compr_buf = NULL;
				best->compr_buf_size = 0;
				best->stat_compr_blocks++;
				best->stat_compr_orig_size += best_slen;
				best->stat_compr_new_size  += best_dlen;
				ret = best->compr;
			}
			break;
		default:
			fprintf(stderr,"mkfs.jffs2: unknown compression mode.\n");
	}
out:
	if (ret == JFFS2_COMPR_NONE) {
		*cpage_out = data_in;
		*datalen = *cdatalen;
		none_stat_compr_blocks++;
		none_stat_compr_size += *datalen;
	}
	else {
		*cpage_out = output_buf;
	}
	return ret;
}


int jffs2_register_compressor(struct jffs2_compressor *comp)
{
	struct jffs2_compressor *this;

	if (!comp->name) {
		fprintf(stderr,"NULL compressor name at registering JFFS2 compressor. Failed.\n");
		return -1;
	}
	comp->compr_buf_size=0;
	comp->compr_buf=NULL;
	comp->usecount=0;
	comp->stat_compr_orig_size=0;
	comp->stat_compr_new_size=0;
	comp->stat_compr_blocks=0;
	comp->stat_decompr_blocks=0;

	list_for_each_entry(this, &jffs2_compressor_list, list) {
		if (this->priority < comp->priority) {
			list_add(&comp->list, this->list.prev);
			goto out;
		}
	}
	list_add_tail(&comp->list, &jffs2_compressor_list);
out:
	return 0;
}

int jffs2_unregister_compressor(struct jffs2_compressor *comp)
{

	if (comp->usecount) {
		fprintf(stderr,"mkfs.jffs2: Compressor modul is in use. Unregister failed.\n");
		return -1;
	}
	list_del(&comp->list);

	return 0;
}

#define JFFS2_STAT_BUF_SIZE 16000

char *jffs2_list_compressors(void)
{
	struct jffs2_compressor *this;
	char *buf, *act_buf;

	act_buf = buf = malloc(JFFS2_STAT_BUF_SIZE);
	list_for_each_entry(this, &jffs2_compressor_list, list) {
		act_buf += sprintf(act_buf, "%10s priority:%d ", this->name, this->priority);
		if ((this->disabled)||(!this->compress))
			act_buf += sprintf(act_buf,"disabled");
		else
			act_buf += sprintf(act_buf,"enabled");
		act_buf += sprintf(act_buf,"\n");
	}
	return buf;
}

char *jffs2_stats(void)
{
	struct jffs2_compressor *this;
	char *buf, *act_buf;

	act_buf = buf = malloc(JFFS2_STAT_BUF_SIZE);

	act_buf += sprintf(act_buf,"Compression mode: ");
	switch (jffs2_compression_mode) {
		case JFFS2_COMPR_MODE_NONE:
			act_buf += sprintf(act_buf,"none");
			break;
		case JFFS2_COMPR_MODE_PRIORITY:
			act_buf += sprintf(act_buf,"priority");
			break;
		case JFFS2_COMPR_MODE_SIZE:
			act_buf += sprintf(act_buf,"size");
			break;
		case JFFS2_COMPR_MODE_FAVOURLZO:
			act_buf += sprintf(act_buf, "favourlzo");
			break;
		default:
			act_buf += sprintf(act_buf, "unknown");
			break;
	}
	act_buf += sprintf(act_buf,"\nCompressors:\n");
	act_buf += sprintf(act_buf,"%10s             ","none");
	act_buf += sprintf(act_buf,"compr: %d blocks (%d)  decompr: %d blocks\n", none_stat_compr_blocks,
			none_stat_compr_size, none_stat_decompr_blocks);
	list_for_each_entry(this, &jffs2_compressor_list, list) {
		act_buf += sprintf(act_buf,"%10s (prio:%d) ",this->name,this->priority);
		if ((this->disabled)||(!this->compress))
			act_buf += sprintf(act_buf,"- ");
		else
			act_buf += sprintf(act_buf,"+ ");
		act_buf += sprintf(act_buf,"compr: %d blocks (%d/%d)  decompr: %d blocks ", this->stat_compr_blocks,
				this->stat_compr_new_size, this->stat_compr_orig_size,
				this->stat_decompr_blocks);
		act_buf += sprintf(act_buf,"\n");
	}
	return buf;
}

int jffs2_set_compression_mode_name(const char *name)
{
	if (!strcmp("none",name)) {
		jffs2_compression_mode = JFFS2_COMPR_MODE_NONE;
		return 0;
	}
	if (!strcmp("priority",name)) {
		jffs2_compression_mode = JFFS2_COMPR_MODE_PRIORITY;
		return 0;
	}
	if (!strcmp("size",name)) {
		jffs2_compression_mode = JFFS2_COMPR_MODE_SIZE;
		return 0;
	}
	if (!strcmp("favourlzo", name)) {
		jffs2_compression_mode = JFFS2_COMPR_MODE_FAVOURLZO;
		return 0;
	}

	return 1;
}

static int jffs2_compressor_Xable(const char *name, int disabled)
{
	struct jffs2_compressor *this;
	list_for_each_entry(this, &jffs2_compressor_list, list) {
		if (!strcmp(this->name, name)) {
			this->disabled = disabled;
			return 0;
		}
	}
	return 1;
}

int jffs2_enable_compressor_name(const char *name)
{
	return jffs2_compressor_Xable(name, 0);
}

int jffs2_disable_compressor_name(const char *name)
{
	return jffs2_compressor_Xable(name, 1);
}

int jffs2_set_compressor_priority(const char *name, int priority)
{
	struct jffs2_compressor *this,*comp;
	list_for_each_entry(this, &jffs2_compressor_list, list) {
		if (!strcmp(this->name, name)) {
			this->priority = priority;
			comp = this;
			goto reinsert;
		}
	}
	fprintf(stderr,"mkfs.jffs2: compressor %s not found.\n",name);
	return 1;
reinsert:
	/* list is sorted in the order of priority, so if
	   we change it we have to reinsert it into the
	   good place */
	list_del(&comp->list);
	list_for_each_entry(this, &jffs2_compressor_list, list) {
		if (this->priority < comp->priority) {
			list_add(&comp->list, this->list.prev);
			return 0;
		}
	}
	list_add_tail(&comp->list, &jffs2_compressor_list);
	return 0;
}


int jffs2_compressors_init(void)
{
#ifdef CONFIG_JFFS2_ZLIB
	jffs2_zlib_init();
#endif
#ifdef CONFIG_JFFS2_RTIME
	jffs2_rtime_init();
#endif
#ifdef CONFIG_JFFS2_LZO
	jffs2_lzo_init();
#endif
	return 0;
}

int jffs2_compressors_exit(void)
{
#ifdef CONFIG_JFFS2_RTIME
	jffs2_rtime_exit();
#endif
#ifdef CONFIG_JFFS2_ZLIB
	jffs2_zlib_exit();
#endif
#ifdef CONFIG_JFFS2_LZO
	jffs2_lzo_exit();
#endif
	return 0;
}
