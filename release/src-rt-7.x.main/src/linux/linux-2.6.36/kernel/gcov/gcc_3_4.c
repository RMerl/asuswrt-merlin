/*
 *  This code provides functions to handle gcc's profiling data format
 *  introduced with gcc 3.4. Future versions of gcc may change the gcov
 *  format (as happened before), so all format-specific information needs
 *  to be kept modular and easily exchangeable.
 *
 *  This file is based on gcc-internal definitions. Functions and data
 *  structures are defined to be compatible with gcc counterparts.
 *  For a better understanding, refer to gcc source: gcc/gcov-io.h.
 *
 *    Copyright IBM Corp. 2009
 *    Author(s): Peter Oberparleiter <oberpar@linux.vnet.ibm.com>
 *
 *    Uses gcc-internal data definitions.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include "gcov.h"

/* Symbolic links to be created for each profiling data file. */
const struct gcov_link gcov_link[] = {
	{ OBJ_TREE, "gcno" },	/* Link to .gcno file in $(objtree). */
	{ 0, NULL},
};

/*
 * Determine whether a counter is active. Based on gcc magic. Doesn't change
 * at run-time.
 */
static int counter_active(struct gcov_info *info, unsigned int type)
{
	return (1 << type) & info->ctr_mask;
}

/* Determine number of active counters. Based on gcc magic. */
static unsigned int num_counter_active(struct gcov_info *info)
{
	unsigned int i;
	unsigned int result = 0;

	for (i = 0; i < GCOV_COUNTERS; i++) {
		if (counter_active(info, i))
			result++;
	}
	return result;
}

/**
 * gcov_info_reset - reset profiling data to zero
 * @info: profiling data set
 */
void gcov_info_reset(struct gcov_info *info)
{
	unsigned int active = num_counter_active(info);
	unsigned int i;

	for (i = 0; i < active; i++) {
		memset(info->counts[i].values, 0,
		       info->counts[i].num * sizeof(gcov_type));
	}
}

/**
 * gcov_info_is_compatible - check if profiling data can be added
 * @info1: first profiling data set
 * @info2: second profiling data set
 *
 * Returns non-zero if profiling data can be added, zero otherwise.
 */
int gcov_info_is_compatible(struct gcov_info *info1, struct gcov_info *info2)
{
	return (info1->stamp == info2->stamp);
}

/**
 * gcov_info_add - add up profiling data
 * @dest: profiling data set to which data is added
 * @source: profiling data set which is added
 *
 * Adds profiling counts of @source to @dest.
 */
void gcov_info_add(struct gcov_info *dest, struct gcov_info *source)
{
	unsigned int i;
	unsigned int j;

	for (i = 0; i < num_counter_active(dest); i++) {
		for (j = 0; j < dest->counts[i].num; j++) {
			dest->counts[i].values[j] +=
				source->counts[i].values[j];
		}
	}
}

/* Get size of function info entry. Based on gcc magic. */
static size_t get_fn_size(struct gcov_info *info)
{
	size_t size;

	size = sizeof(struct gcov_fn_info) + num_counter_active(info) *
	       sizeof(unsigned int);
	if (__alignof__(struct gcov_fn_info) > sizeof(unsigned int))
		size = ALIGN(size, __alignof__(struct gcov_fn_info));
	return size;
}

/* Get address of function info entry. Based on gcc magic. */
static struct gcov_fn_info *get_fn_info(struct gcov_info *info, unsigned int fn)
{
	return (struct gcov_fn_info *)
		((char *) info->functions + fn * get_fn_size(info));
}

/**
 * gcov_info_dup - duplicate profiling data set
 * @info: profiling data set to duplicate
 *
 * Return newly allocated duplicate on success, %NULL on error.
 */
struct gcov_info *gcov_info_dup(struct gcov_info *info)
{
	struct gcov_info *dup;
	unsigned int i;
	unsigned int active;

	/* Duplicate gcov_info. */
	active = num_counter_active(info);
	dup = kzalloc(sizeof(struct gcov_info) +
		      sizeof(struct gcov_ctr_info) * active, GFP_KERNEL);
	if (!dup)
		return NULL;
	dup->version		= info->version;
	dup->stamp		= info->stamp;
	dup->n_functions	= info->n_functions;
	dup->ctr_mask		= info->ctr_mask;
	/* Duplicate filename. */
	dup->filename		= kstrdup(info->filename, GFP_KERNEL);
	if (!dup->filename)
		goto err_free;
	/* Duplicate table of functions. */
	dup->functions = kmemdup(info->functions, info->n_functions *
				 get_fn_size(info), GFP_KERNEL);
	if (!dup->functions)
		goto err_free;
	/* Duplicate counter arrays. */
	for (i = 0; i < active ; i++) {
		struct gcov_ctr_info *ctr = &info->counts[i];
		size_t size = ctr->num * sizeof(gcov_type);

		dup->counts[i].num = ctr->num;
		dup->counts[i].merge = ctr->merge;
		dup->counts[i].values = vmalloc(size);
		if (!dup->counts[i].values)
			goto err_free;
		memcpy(dup->counts[i].values, ctr->values, size);
	}
	return dup;

err_free:
	gcov_info_free(dup);
	return NULL;
}

/**
 * gcov_info_free - release memory for profiling data set duplicate
 * @info: profiling data set duplicate to free
 */
void gcov_info_free(struct gcov_info *info)
{
	unsigned int active = num_counter_active(info);
	unsigned int i;

	for (i = 0; i < active ; i++)
		vfree(info->counts[i].values);
	kfree(info->functions);
	kfree(info->filename);
	kfree(info);
}

/**
 * struct type_info - iterator helper array
 * @ctr_type: counter type
 * @offset: index of the first value of the current function for this type
 *
 * This array is needed to convert the in-memory data format into the in-file
 * data format:
 *
 * In-memory:
 *   for each counter type
 *     for each function
 *       values
 *
 * In-file:
 *   for each function
 *     for each counter type
 *       values
 *
 * See gcc source gcc/gcov-io.h for more information on data organization.
 */
struct type_info {
	int ctr_type;
	unsigned int offset;
};

/**
 * struct gcov_iterator - specifies current file position in logical records
 * @info: associated profiling data
 * @record: record type
 * @function: function number
 * @type: counter type
 * @count: index into values array
 * @num_types: number of counter types
 * @type_info: helper array to get values-array offset for current function
 */
struct gcov_iterator {
	struct gcov_info *info;

	int record;
	unsigned int function;
	unsigned int type;
	unsigned int count;

	int num_types;
	struct type_info type_info[0];
};

static struct gcov_fn_info *get_func(struct gcov_iterator *iter)
{
	return get_fn_info(iter->info, iter->function);
}

static struct type_info *get_type(struct gcov_iterator *iter)
{
	return &iter->type_info[iter->type];
}

/**
 * gcov_iter_new - allocate and initialize profiling data iterator
 * @info: profiling data set to be iterated
 *
 * Return file iterator on success, %NULL otherwise.
 */
struct gcov_iterator *gcov_iter_new(struct gcov_info *info)
{
	struct gcov_iterator *iter;

	iter = kzalloc(sizeof(struct gcov_iterator) +
		       num_counter_active(info) * sizeof(struct type_info),
		       GFP_KERNEL);
	if (iter)
		iter->info = info;

	return iter;
}

/**
 * gcov_iter_free - release memory for iterator
 * @iter: file iterator to free
 */
void gcov_iter_free(struct gcov_iterator *iter)
{
	kfree(iter);
}

/**
 * gcov_iter_get_info - return profiling data set for given file iterator
 * @iter: file iterator
 */
struct gcov_info *gcov_iter_get_info(struct gcov_iterator *iter)
{
	return iter->info;
}

/**
 * gcov_iter_start - reset file iterator to starting position
 * @iter: file iterator
 */
void gcov_iter_start(struct gcov_iterator *iter)
{
	int i;

	iter->record = 0;
	iter->function = 0;
	iter->type = 0;
	iter->count = 0;
	iter->num_types = 0;
	for (i = 0; i < GCOV_COUNTERS; i++) {
		if (counter_active(iter->info, i)) {
			iter->type_info[iter->num_types].ctr_type = i;
			iter->type_info[iter->num_types++].offset = 0;
		}
	}
}

/* Mapping of logical record number to actual file content. */
#define RECORD_FILE_MAGIC	0
#define RECORD_GCOV_VERSION	1
#define RECORD_TIME_STAMP	2
#define RECORD_FUNCTION_TAG	3
#define RECORD_FUNCTON_TAG_LEN	4
#define RECORD_FUNCTION_IDENT	5
#define RECORD_FUNCTION_CHECK	6
#define RECORD_COUNT_TAG	7
#define RECORD_COUNT_LEN	8
#define RECORD_COUNT		9

/**
 * gcov_iter_next - advance file iterator to next logical record
 * @iter: file iterator
 *
 * Return zero if new position is valid, non-zero if iterator has reached end.
 */
int gcov_iter_next(struct gcov_iterator *iter)
{
	switch (iter->record) {
	case RECORD_FILE_MAGIC:
	case RECORD_GCOV_VERSION:
	case RECORD_FUNCTION_TAG:
	case RECORD_FUNCTON_TAG_LEN:
	case RECORD_FUNCTION_IDENT:
	case RECORD_COUNT_TAG:
		/* Advance to next record */
		iter->record++;
		break;
	case RECORD_COUNT:
		/* Advance to next count */
		iter->count++;
		/* fall through */
	case RECORD_COUNT_LEN:
		if (iter->count < get_func(iter)->n_ctrs[iter->type]) {
			iter->record = 9;
			break;
		}
		/* Advance to next counter type */
		get_type(iter)->offset += iter->count;
		iter->count = 0;
		iter->type++;
		/* fall through */
	case RECORD_FUNCTION_CHECK:
		if (iter->type < iter->num_types) {
			iter->record = 7;
			break;
		}
		/* Advance to next function */
		iter->type = 0;
		iter->function++;
		/* fall through */
	case RECORD_TIME_STAMP:
		if (iter->function < iter->info->n_functions)
			iter->record = 3;
		else
			iter->record = -1;
		break;
	}
	/* Check for EOF. */
	if (iter->record == -1)
		return -EINVAL;
	else
		return 0;
}

/**
 * seq_write_gcov_u32 - write 32 bit number in gcov format to seq_file
 * @seq: seq_file handle
 * @v: value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file.
 */
static int seq_write_gcov_u32(struct seq_file *seq, u32 v)
{
	return seq_write(seq, &v, sizeof(v));
}

/**
 * seq_write_gcov_u64 - write 64 bit number in gcov format to seq_file
 * @seq: seq_file handle
 * @v: value to be stored
 *
 * Number format defined by gcc: numbers are recorded in the 32 bit
 * unsigned binary form of the endianness of the machine generating the
 * file. 64 bit numbers are stored as two 32 bit numbers, the low part
 * first.
 */
static int seq_write_gcov_u64(struct seq_file *seq, u64 v)
{
	u32 data[2];

	data[0] = (v & 0xffffffffUL);
	data[1] = (v >> 32);
	return seq_write(seq, data, sizeof(data));
}

/**
 * gcov_iter_write - write data for current pos to seq_file
 * @iter: file iterator
 * @seq: seq_file handle
 *
 * Return zero on success, non-zero otherwise.
 */
int gcov_iter_write(struct gcov_iterator *iter, struct seq_file *seq)
{
	int rc = -EINVAL;

	switch (iter->record) {
	case RECORD_FILE_MAGIC:
		rc = seq_write_gcov_u32(seq, GCOV_DATA_MAGIC);
		break;
	case RECORD_GCOV_VERSION:
		rc = seq_write_gcov_u32(seq, iter->info->version);
		break;
	case RECORD_TIME_STAMP:
		rc = seq_write_gcov_u32(seq, iter->info->stamp);
		break;
	case RECORD_FUNCTION_TAG:
		rc = seq_write_gcov_u32(seq, GCOV_TAG_FUNCTION);
		break;
	case RECORD_FUNCTON_TAG_LEN:
		rc = seq_write_gcov_u32(seq, 2);
		break;
	case RECORD_FUNCTION_IDENT:
		rc = seq_write_gcov_u32(seq, get_func(iter)->ident);
		break;
	case RECORD_FUNCTION_CHECK:
		rc = seq_write_gcov_u32(seq, get_func(iter)->checksum);
		break;
	case RECORD_COUNT_TAG:
		rc = seq_write_gcov_u32(seq,
			GCOV_TAG_FOR_COUNTER(get_type(iter)->ctr_type));
		break;
	case RECORD_COUNT_LEN:
		rc = seq_write_gcov_u32(seq,
				get_func(iter)->n_ctrs[iter->type] * 2);
		break;
	case RECORD_COUNT:
		rc = seq_write_gcov_u64(seq,
			iter->info->counts[iter->type].
				values[iter->count + get_type(iter)->offset]);
		break;
	}
	return rc;
}
