/* 
   Unix SMB/CIFS implementation.
   rpc interface definitions

   Copyright (C) Andrew Tridgell 2003
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This is a public header file that is installed as part of Samba. 
 * If you remove any functions or change their signature, update 
 * the so version number. */

#ifndef __LIBNDR_H__
#define __LIBNDR_H__

#include <talloc.h>
#include <sys/time.h>
#if _SAMBA_BUILD_ == 4
#include "../lib/util/util.h" /* for discard_const */
#include "../lib/util/charset/charset.h"
#endif

/*
  this provides definitions for the libcli/rpc/ MSRPC library
*/


/*
  this is used by the token store/retrieve code
*/
struct ndr_token_list {
	struct ndr_token_list *next, *prev;
	const void *key;
	uint32_t value;
};

/* this is the base structure passed to routines that 
   parse MSRPC formatted data 

   note that in Samba4 we use separate routines and structures for
   MSRPC marshalling and unmarshalling. Also note that these routines
   are being kept deliberately very simple, and are not tied to a
   particular transport
*/
struct ndr_pull {
	uint32_t flags; /* LIBNDR_FLAG_* */
	uint8_t *data;
	uint32_t data_size;
	uint32_t offset;

	uint32_t relative_highest_offset;
	uint32_t relative_base_offset;
	uint32_t relative_rap_convert;
	struct ndr_token_list *relative_base_list;

	struct ndr_token_list *relative_list;
	struct ndr_token_list *array_size_list;
	struct ndr_token_list *array_length_list;
	struct ndr_token_list *switch_list;

	TALLOC_CTX *current_mem_ctx;

	/* this is used to ensure we generate unique reference IDs
	   between request and reply */
	uint32_t ptr_count;
};

/* structure passed to functions that generate NDR formatted data */
struct ndr_push {
	uint32_t flags; /* LIBNDR_FLAG_* */
	uint8_t *data;
	uint32_t alloc_size;
	uint32_t offset;

	uint32_t relative_base_offset;
	uint32_t relative_end_offset;
	struct ndr_token_list *relative_base_list;

	struct ndr_token_list *switch_list;
	struct ndr_token_list *relative_list;
	struct ndr_token_list *relative_begin_list;
	struct ndr_token_list *nbt_string_list;
	struct ndr_token_list *dns_string_list;
	struct ndr_token_list *full_ptr_list;

	/* this is used to ensure we generate unique reference IDs */
	uint32_t ptr_count;
};

/* structure passed to functions that print IDL structures */
struct ndr_print {
	uint32_t flags; /* LIBNDR_FLAG_* */
	uint32_t depth;
	struct ndr_token_list *switch_list;
	void (*print)(struct ndr_print *, const char *, ...) PRINTF_ATTRIBUTE(2,3);
	void *private_data;
	bool no_newline;
};

#define LIBNDR_FLAG_BIGENDIAN  (1<<0)
#define LIBNDR_FLAG_NOALIGN    (1<<1)

#define LIBNDR_FLAG_STR_ASCII		(1<<2)
#define LIBNDR_FLAG_STR_LEN4		(1<<3)
#define LIBNDR_FLAG_STR_SIZE4		(1<<4)
#define LIBNDR_FLAG_STR_NOTERM		(1<<5)
#define LIBNDR_FLAG_STR_NULLTERM	(1<<6)
#define LIBNDR_FLAG_STR_SIZE2		(1<<7)
#define LIBNDR_FLAG_STR_BYTESIZE	(1<<8)
#define LIBNDR_FLAG_STR_CONFORMANT	(1<<10)
#define LIBNDR_FLAG_STR_CHARLEN		(1<<11)
#define LIBNDR_FLAG_STR_UTF8		(1<<12)
#define LIBNDR_STRING_FLAGS		(0x7FFC)

/*
 * don't debug NDR_ERR_BUFSIZE failures,
 * as the available buffer might be incomplete.
 *
 * return NDR_ERR_INCOMPLETE_BUFFER instead.
 */
#define LIBNDR_FLAG_INCOMPLETE_BUFFER (1<<16)

/*
 * This lets ndr_pull_subcontext_end() return
 * NDR_ERR_UNREAD_BYTES.
 */
#define LIBNDR_FLAG_SUBCONTEXT_NO_UNREAD_BYTES (1<<17)

/* set if relative pointers should *not* be marshalled in reverse order */
#define LIBNDR_FLAG_NO_RELATIVE_REVERSE	(1<<18)

/* set if relative pointers are marshalled in reverse order */
#define LIBNDR_FLAG_RELATIVE_REVERSE	(1<<19)

#define LIBNDR_FLAG_REF_ALLOC    (1<<20)
#define LIBNDR_FLAG_REMAINING    (1<<21)
#define LIBNDR_FLAG_ALIGN2       (1<<22)
#define LIBNDR_FLAG_ALIGN4       (1<<23)
#define LIBNDR_FLAG_ALIGN8       (1<<24)

#define LIBNDR_ALIGN_FLAGS ( 0        | \
		LIBNDR_FLAG_NOALIGN   | \
		LIBNDR_FLAG_REMAINING | \
		LIBNDR_FLAG_ALIGN2    | \
		LIBNDR_FLAG_ALIGN4    | \
		LIBNDR_FLAG_ALIGN8    | \
		0)

#define LIBNDR_PRINT_ARRAY_HEX   (1<<25)
#define LIBNDR_PRINT_SET_VALUES  (1<<26)

/* used to force a section of IDL to be little-endian */
#define LIBNDR_FLAG_LITTLE_ENDIAN (1<<27)

/* used to check if alignment padding is zero */
#define LIBNDR_FLAG_PAD_CHECK     (1<<28)

#define LIBNDR_FLAG_NDR64         (1<<29)

/* set if an object uuid will be present */
#define LIBNDR_FLAG_OBJECT_PRESENT    (1<<30)

/* set to avoid recursion in ndr_size_*() calculation */
#define LIBNDR_FLAG_NO_NDR_SIZE		(1<<31)

/* useful macro for debugging */
#define NDR_PRINT_DEBUG(type, p) ndr_print_debug((ndr_print_fn_t)ndr_print_ ##type, #p, p)
#define NDR_PRINT_DEBUGC(dbgc_class, type, p) ndr_print_debugc(dbgc_class, (ndr_print_fn_t)ndr_print_ ##type, #p, p)
#define NDR_PRINT_UNION_DEBUG(type, level, p) ndr_print_union_debug((ndr_print_fn_t)ndr_print_ ##type, #p, level, p)
#define NDR_PRINT_FUNCTION_DEBUG(type, flags, p) ndr_print_function_debug((ndr_print_function_t)ndr_print_ ##type, #type, flags, p)
#define NDR_PRINT_BOTH_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_BOTH, p)
#define NDR_PRINT_OUT_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_OUT, p)
#define NDR_PRINT_IN_DEBUG(type, p) NDR_PRINT_FUNCTION_DEBUG(type, NDR_IN | NDR_SET_VALUES, p)

/* useful macro for debugging in strings */
#define NDR_PRINT_STRUCT_STRING(ctx, type, p) ndr_print_struct_string(ctx, (ndr_print_fn_t)ndr_print_ ##type, #p, p)
#define NDR_PRINT_UNION_STRING(ctx, type, level, p) ndr_print_union_string(ctx, (ndr_print_fn_t)ndr_print_ ##type, #p, level, p)
#define NDR_PRINT_FUNCTION_STRING(ctx, type, flags, p) ndr_print_function_string(ctx, (ndr_print_function_t)ndr_print_ ##type, #type, flags, p)
#define NDR_PRINT_BOTH_STRING(ctx, type, p) NDR_PRINT_FUNCTION_STRING(ctx, type, NDR_BOTH, p)
#define NDR_PRINT_OUT_STRING(ctx, type, p) NDR_PRINT_FUNCTION_STRING(ctx, type, NDR_OUT, p)
#define NDR_PRINT_IN_STRING(ctx, type, p) NDR_PRINT_FUNCTION_STRING(ctx, type, NDR_IN | NDR_SET_VALUES, p)

#define NDR_BE(ndr) (unlikely(((ndr)->flags & (LIBNDR_FLAG_BIGENDIAN|LIBNDR_FLAG_LITTLE_ENDIAN)) == LIBNDR_FLAG_BIGENDIAN))

enum ndr_err_code {
	NDR_ERR_SUCCESS = 0,
	NDR_ERR_ARRAY_SIZE,
	NDR_ERR_BAD_SWITCH,
	NDR_ERR_OFFSET,
	NDR_ERR_RELATIVE,
	NDR_ERR_CHARCNV,
	NDR_ERR_LENGTH,
	NDR_ERR_SUBCONTEXT,
	NDR_ERR_COMPRESSION,
	NDR_ERR_STRING,
	NDR_ERR_VALIDATE,
	NDR_ERR_BUFSIZE,
	NDR_ERR_ALLOC,
	NDR_ERR_RANGE,
	NDR_ERR_TOKEN,
	NDR_ERR_IPV4ADDRESS,
	NDR_ERR_IPV6ADDRESS,
	NDR_ERR_INVALID_POINTER,
	NDR_ERR_UNREAD_BYTES,
	NDR_ERR_NDR64,
	NDR_ERR_FLAGS,
	NDR_ERR_INCOMPLETE_BUFFER
};

#define NDR_ERR_CODE_IS_SUCCESS(x) (x == NDR_ERR_SUCCESS)

#define NDR_ERR_HAVE_NO_MEMORY(x) do { \
	if (NULL == (x)) { \
		return NDR_ERR_ALLOC; \
	} \
} while (0)

enum ndr_compression_alg {
	NDR_COMPRESSION_MSZIP	= 2,
	NDR_COMPRESSION_XPRESS	= 3
};

/*
  flags passed to control parse flow
  These are deliberately in a different range to the NDR_IN/NDR_OUT
  flags to catch mixups
*/
#define NDR_SCALARS    0x100
#define NDR_BUFFERS    0x200

/*
  flags passed to ndr_print_*() and ndr pull/push for functions
  These are deliberately in a different range to the NDR_SCALARS/NDR_BUFFERS
  flags to catch mixups
*/
#define NDR_IN         0x10
#define NDR_OUT        0x20
#define NDR_BOTH       0x30
#define NDR_SET_VALUES 0x40


#define NDR_PULL_CHECK_FLAGS(ndr, ndr_flags) do { \
	if ((ndr_flags) & ~(NDR_SCALARS|NDR_BUFFERS)) { \
		return ndr_pull_error(ndr, NDR_ERR_FLAGS, "Invalid pull struct ndr_flags 0x%x", ndr_flags); \
	} \
} while (0)

#define NDR_PUSH_CHECK_FLAGS(ndr, ndr_flags) do { \
	if ((ndr_flags) & ~(NDR_SCALARS|NDR_BUFFERS)) \
		return ndr_push_error(ndr, NDR_ERR_FLAGS, "Invalid push struct ndr_flags 0x%x", ndr_flags); \
} while (0)

#define NDR_PULL_CHECK_FN_FLAGS(ndr, flags) do { \
	if ((flags) & ~(NDR_BOTH|NDR_SET_VALUES)) { \
		return ndr_pull_error(ndr, NDR_ERR_FLAGS, "Invalid fn pull flags 0x%x", flags); \
	} \
} while (0)

#define NDR_PUSH_CHECK_FN_FLAGS(ndr, flags) do { \
	if ((flags) & ~(NDR_BOTH|NDR_SET_VALUES)) \
		return ndr_push_error(ndr, NDR_ERR_FLAGS, "Invalid fn push flags 0x%x", flags); \
} while (0)

#define NDR_PULL_NEED_BYTES(ndr, n) do { \
	if (unlikely((n) > ndr->data_size || ndr->offset + (n) > ndr->data_size)) { \
		if (ndr->flags & LIBNDR_FLAG_INCOMPLETE_BUFFER) { \
			uint32_t _available = ndr->data_size - ndr->offset; \
			uint32_t _missing = n - _available; \
			ndr->relative_highest_offset = _missing; \
		} \
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, "Pull bytes %u (%s)", (unsigned)n, __location__); \
	} \
} while(0)

#define NDR_ALIGN(ndr, n) ndr_align_size(ndr->offset, n)

#define NDR_ROUND(size, n) (((size)+((n)-1)) & ~((n)-1))

#define NDR_PULL_ALIGN(ndr, n) do { \
	if (unlikely(!(ndr->flags & LIBNDR_FLAG_NOALIGN))) {	\
		if (unlikely(ndr->flags & LIBNDR_FLAG_PAD_CHECK)) {	\
			ndr_check_padding(ndr, n); \
		} \
		ndr->offset = (ndr->offset + (n-1)) & ~(n-1); \
	} \
	if (unlikely(ndr->offset > ndr->data_size)) {			\
		if (ndr->flags & LIBNDR_FLAG_INCOMPLETE_BUFFER) { \
			uint32_t _missing = ndr->offset - ndr->data_size; \
			ndr->relative_highest_offset = _missing; \
		} \
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE, "Pull align %u", (unsigned)n); \
	} \
} while(0)

#define NDR_PUSH_NEED_BYTES(ndr, n) NDR_CHECK(ndr_push_expand(ndr, n))

#define NDR_PUSH_ALIGN(ndr, n) do { \
	if (likely(!(ndr->flags & LIBNDR_FLAG_NOALIGN))) {	\
		uint32_t _pad = ((ndr->offset + (n-1)) & ~(n-1)) - ndr->offset; \
		while (_pad--) NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, 0)); \
	} \
} while(0)

/* these are used to make the error checking on each element in libndr
   less tedious, hopefully making the code more readable */
#define NDR_CHECK(call) do { \
	enum ndr_err_code _status; \
	_status = call; \
	if (unlikely(!NDR_ERR_CODE_IS_SUCCESS(_status))) {	\
		return _status; \
	} \
} while (0)

/* if the call fails then free the ndr pointer */
#define NDR_CHECK_FREE(call) do { \
	enum ndr_err_code _status; \
	_status = call; \
	if (unlikely(!NDR_ERR_CODE_IS_SUCCESS(_status))) {	\
		talloc_free(ndr);		 \
		return _status; \
	} \
} while (0)

#define NDR_PULL_GET_MEM_CTX(ndr) (ndr->current_mem_ctx)

#define NDR_PULL_SET_MEM_CTX(ndr, mem_ctx, flgs) do {\
	if ( !(flgs) || (ndr->flags & flgs) ) {\
		if (!(mem_ctx)) {\
			return ndr_pull_error(ndr, NDR_ERR_ALLOC, "NDR_PULL_SET_MEM_CTX(NULL): %s\n", __location__); \
		}\
		ndr->current_mem_ctx = discard_const(mem_ctx);\
	}\
} while(0)

#define _NDR_PULL_FIX_CURRENT_MEM_CTX(ndr) do {\
	if (!ndr->current_mem_ctx) {\
		ndr->current_mem_ctx = talloc_new(ndr);\
		if (!ndr->current_mem_ctx) {\
			return ndr_pull_error(ndr, NDR_ERR_ALLOC, "_NDR_PULL_FIX_CURRENT_MEM_CTX() failed: %s\n", __location__); \
		}\
	}\
} while(0)

#define NDR_PULL_ALLOC(ndr, s) do { \
	_NDR_PULL_FIX_CURRENT_MEM_CTX(ndr);\
	(s) = talloc_ptrtype(ndr->current_mem_ctx, (s)); \
	if (unlikely(!(s))) return ndr_pull_error(ndr, NDR_ERR_ALLOC, "Alloc %s failed: %s\n", # s, __location__); \
} while (0)

#define NDR_PULL_ALLOC_N(ndr, s, n) do { \
	_NDR_PULL_FIX_CURRENT_MEM_CTX(ndr);\
	(s) = talloc_array_ptrtype(ndr->current_mem_ctx, (s), n); \
	if (unlikely(!(s))) return ndr_pull_error(ndr, NDR_ERR_ALLOC, "Alloc %u * %s failed: %s\n", (unsigned)n, # s, __location__); \
} while (0)


#define NDR_PUSH_ALLOC_SIZE(ndr, s, size) do { \
       (s) = talloc_array(ndr, uint8_t, size); \
       if (unlikely(!(s))) return ndr_push_error(ndr, NDR_ERR_ALLOC, "push alloc %u failed: %s\n", (unsigned)size, __location__); \
} while (0)

#define NDR_PUSH_ALLOC(ndr, s) do { \
       (s) = talloc_ptrtype(ndr, (s)); \
       if (unlikely(!(s))) return ndr_push_error(ndr, NDR_ERR_ALLOC, "push alloc %s failed: %s\n", # s, __location__); \
} while (0)

/* these are used when generic fn pointers are needed for ndr push/pull fns */
typedef enum ndr_err_code (*ndr_push_flags_fn_t)(struct ndr_push *, int ndr_flags, const void *);
typedef enum ndr_err_code (*ndr_pull_flags_fn_t)(struct ndr_pull *, int ndr_flags, void *);
typedef void (*ndr_print_fn_t)(struct ndr_print *, const char *, const void *);
typedef void (*ndr_print_function_t)(struct ndr_print *, const char *, int, const void *);

#include "../libcli/util/error.h"
#include "librpc/gen_ndr/misc.h"

extern const struct ndr_syntax_id ndr_transfer_syntax;
extern const struct ndr_syntax_id ndr64_transfer_syntax;
extern const struct ndr_syntax_id null_ndr_syntax_id;

struct ndr_interface_call_pipe {
	const char *name;
	const char *chunk_struct_name;
	size_t chunk_struct_size;
	ndr_push_flags_fn_t ndr_push;
	ndr_pull_flags_fn_t ndr_pull;
	ndr_print_fn_t ndr_print;
};

struct ndr_interface_call_pipes {
	uint32_t num_pipes;
	const struct ndr_interface_call_pipe *pipes;
};

struct ndr_interface_call {
	const char *name;
	size_t struct_size;
	ndr_push_flags_fn_t ndr_push;
	ndr_pull_flags_fn_t ndr_pull;
	ndr_print_function_t ndr_print;
	struct ndr_interface_call_pipes in_pipes;
	struct ndr_interface_call_pipes out_pipes;
};

struct ndr_interface_string_array {
	uint32_t count;
	const char * const *names;
};

struct ndr_interface_table {
	const char *name;
	struct ndr_syntax_id syntax_id;
	const char *helpstring;
	uint32_t num_calls;
	const struct ndr_interface_call *calls;
	const struct ndr_interface_string_array *endpoints;
	const struct ndr_interface_string_array *authservices;
};

struct ndr_interface_list {
	struct ndr_interface_list *prev, *next;
	const struct ndr_interface_table *table;
};

/*********************************************************************
 Map an NT error code from a NDR error code.
*********************************************************************/
NTSTATUS ndr_map_error2ntstatus(enum ndr_err_code ndr_err);
const char *ndr_map_error2string(enum ndr_err_code ndr_err);
#define ndr_errstr ndr_map_error2string

/* FIXME: Use represent_as instead */
struct dom_sid;
enum ndr_err_code ndr_push_dom_sid2(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid);
enum ndr_err_code ndr_pull_dom_sid2(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid);
void ndr_print_dom_sid2(struct ndr_print *ndr, const char *name, const struct dom_sid *sid);
enum ndr_err_code ndr_push_dom_sid28(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid);
enum ndr_err_code ndr_pull_dom_sid28(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid);
void ndr_print_dom_sid28(struct ndr_print *ndr, const char *name, const struct dom_sid *sid);
size_t ndr_size_dom_sid28(const struct dom_sid *sid, int flags);
enum ndr_err_code ndr_push_dom_sid0(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid);
enum ndr_err_code ndr_pull_dom_sid0(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid);
void ndr_print_dom_sid0(struct ndr_print *ndr, const char *name, const struct dom_sid *sid);
size_t ndr_size_dom_sid0(const struct dom_sid *sid, int flags);
void ndr_print_GUID(struct ndr_print *ndr, const char *name, const struct GUID *guid);
bool ndr_syntax_id_equal(const struct ndr_syntax_id *i1, const struct ndr_syntax_id *i2); 
char *ndr_syntax_id_to_string(TALLOC_CTX *mem_ctx, const struct ndr_syntax_id *id);
bool ndr_syntax_id_from_string(const char *s, struct ndr_syntax_id *id);
enum ndr_err_code ndr_push_struct_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, const void *p, ndr_push_flags_fn_t fn);
enum ndr_err_code ndr_push_union_blob(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p, uint32_t level, ndr_push_flags_fn_t fn);
size_t ndr_size_struct(const void *p, int flags, ndr_push_flags_fn_t push);
size_t ndr_size_union(const void *p, int flags, uint32_t level, ndr_push_flags_fn_t push);
uint32_t ndr_push_get_relative_base_offset(struct ndr_push *ndr);
void ndr_push_restore_relative_base_offset(struct ndr_push *ndr, uint32_t offset);
enum ndr_err_code ndr_push_setup_relative_base_offset1(struct ndr_push *ndr, const void *p, uint32_t offset);
enum ndr_err_code ndr_push_setup_relative_base_offset2(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_relative_ptr1(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_short_relative_ptr1(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_relative_ptr2_start(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_relative_ptr2_end(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_short_relative_ptr2(struct ndr_push *ndr, const void *p);
uint32_t ndr_pull_get_relative_base_offset(struct ndr_pull *ndr);
void ndr_pull_restore_relative_base_offset(struct ndr_pull *ndr, uint32_t offset);
enum ndr_err_code ndr_pull_setup_relative_base_offset1(struct ndr_pull *ndr, const void *p, uint32_t offset);
enum ndr_err_code ndr_pull_setup_relative_base_offset2(struct ndr_pull *ndr, const void *p);
enum ndr_err_code ndr_pull_relative_ptr1(struct ndr_pull *ndr, const void *p, uint32_t rel_offset);
enum ndr_err_code ndr_pull_relative_ptr2(struct ndr_pull *ndr, const void *p);
enum ndr_err_code ndr_pull_relative_ptr_short(struct ndr_pull *ndr, uint16_t *v);
size_t ndr_align_size(uint32_t offset, size_t n);
struct ndr_pull *ndr_pull_init_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx);
enum ndr_err_code ndr_pull_append(struct ndr_pull *ndr, DATA_BLOB *blob);
enum ndr_err_code ndr_pull_pop(struct ndr_pull *ndr);
enum ndr_err_code ndr_pull_advance(struct ndr_pull *ndr, uint32_t size);
struct ndr_push *ndr_push_init_ctx(TALLOC_CTX *mem_ctx);
DATA_BLOB ndr_push_blob(struct ndr_push *ndr);
enum ndr_err_code ndr_push_expand(struct ndr_push *ndr, uint32_t extra_size);
void ndr_print_debug_helper(struct ndr_print *ndr, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ndr_print_debugc_helper(struct ndr_print *ndr, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ndr_print_printf_helper(struct ndr_print *ndr, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ndr_print_string_helper(struct ndr_print *ndr, const char *format, ...) PRINTF_ATTRIBUTE(2,3);
void ndr_print_debug(ndr_print_fn_t fn, const char *name, void *ptr);
void ndr_print_debugc(int dbgc_class, ndr_print_fn_t fn, const char *name, void *ptr);
void ndr_print_union_debug(ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr);
void ndr_print_function_debug(ndr_print_function_t fn, const char *name, int flags, void *ptr);
char *ndr_print_struct_string(TALLOC_CTX *mem_ctx, ndr_print_fn_t fn, const char *name, void *ptr);
char *ndr_print_union_string(TALLOC_CTX *mem_ctx, ndr_print_fn_t fn, const char *name, uint32_t level, void *ptr);
char *ndr_print_function_string(TALLOC_CTX *mem_ctx,
				ndr_print_function_t fn, const char *name, 
				int flags, void *ptr);
void ndr_set_flags(uint32_t *pflags, uint32_t new_flags);
enum ndr_err_code ndr_pull_error(struct ndr_pull *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...) PRINTF_ATTRIBUTE(3,4);
enum ndr_err_code ndr_push_error(struct ndr_push *ndr,
				 enum ndr_err_code ndr_err,
				 const char *format, ...)  PRINTF_ATTRIBUTE(3,4);
enum ndr_err_code ndr_pull_subcontext_start(struct ndr_pull *ndr,
				   struct ndr_pull **_subndr,
				   size_t header_size,
				   ssize_t size_is);
enum ndr_err_code ndr_pull_subcontext_end(struct ndr_pull *ndr,
				 struct ndr_pull *subndr,
				 size_t header_size,
				 ssize_t size_is);
enum ndr_err_code ndr_push_subcontext_start(struct ndr_push *ndr,
				   struct ndr_push **_subndr,
				   size_t header_size,
				   ssize_t size_is);
enum ndr_err_code ndr_push_subcontext_end(struct ndr_push *ndr,
				 struct ndr_push *subndr,
				 size_t header_size,
				 ssize_t size_is);
enum ndr_err_code ndr_token_store(TALLOC_CTX *mem_ctx,
			 struct ndr_token_list **list, 
			 const void *key, 
			 uint32_t value);
enum ndr_err_code ndr_token_retrieve_cmp_fn(struct ndr_token_list **list, const void *key, uint32_t *v, int(*_cmp_fn)(const void*,const void*), bool _remove_tok);
enum ndr_err_code ndr_token_retrieve(struct ndr_token_list **list, const void *key, uint32_t *v);
uint32_t ndr_token_peek(struct ndr_token_list **list, const void *key);
enum ndr_err_code ndr_pull_array_size(struct ndr_pull *ndr, const void *p);
uint32_t ndr_get_array_size(struct ndr_pull *ndr, const void *p);
enum ndr_err_code ndr_check_array_size(struct ndr_pull *ndr, void *p, uint32_t size);
enum ndr_err_code ndr_pull_array_length(struct ndr_pull *ndr, const void *p);
uint32_t ndr_get_array_length(struct ndr_pull *ndr, const void *p);
enum ndr_err_code ndr_check_array_length(struct ndr_pull *ndr, void *p, uint32_t length);
enum ndr_err_code ndr_push_pipe_chunk_trailer(struct ndr_push *ndr, int ndr_flags, uint32_t count);
enum ndr_err_code ndr_check_pipe_chunk_trailer(struct ndr_pull *ndr, int ndr_flags, uint32_t count);
enum ndr_err_code ndr_push_set_switch_value(struct ndr_push *ndr, const void *p, uint32_t val);
enum ndr_err_code ndr_pull_set_switch_value(struct ndr_pull *ndr, const void *p, uint32_t val);
enum ndr_err_code ndr_print_set_switch_value(struct ndr_print *ndr, const void *p, uint32_t val);
uint32_t ndr_push_get_switch_value(struct ndr_push *ndr, const void *p);
uint32_t ndr_pull_get_switch_value(struct ndr_pull *ndr, const void *p);
uint32_t ndr_print_get_switch_value(struct ndr_print *ndr, const void *p);
enum ndr_err_code ndr_pull_struct_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p, ndr_pull_flags_fn_t fn);
enum ndr_err_code ndr_pull_struct_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p, ndr_pull_flags_fn_t fn);
enum ndr_err_code ndr_pull_union_blob(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p, uint32_t level, ndr_pull_flags_fn_t fn);
enum ndr_err_code ndr_pull_union_blob_all(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, void *p, uint32_t level, ndr_pull_flags_fn_t fn);

/* from libndr_basic.h */
#define NDR_SCALAR_PROTO(name, type) \
enum ndr_err_code ndr_push_ ## name(struct ndr_push *ndr, int ndr_flags, type v); \
enum ndr_err_code ndr_pull_ ## name(struct ndr_pull *ndr, int ndr_flags, type *v); \
void ndr_print_ ## name(struct ndr_print *ndr, const char *var_name, type v); 

#define NDR_BUFFER_PROTO(name, type) \
enum ndr_err_code ndr_push_ ## name(struct ndr_push *ndr, int ndr_flags, const type *v); \
enum ndr_err_code ndr_pull_ ## name(struct ndr_pull *ndr, int ndr_flags, type *v); \
void ndr_print_ ## name(struct ndr_print *ndr, const char *var_name, const type *v); 

NDR_SCALAR_PROTO(uint8, uint8_t)
NDR_SCALAR_PROTO(int8, int8_t)
NDR_SCALAR_PROTO(uint16, uint16_t)
NDR_SCALAR_PROTO(int16, int16_t)
NDR_SCALAR_PROTO(uint1632, uint16_t)
NDR_SCALAR_PROTO(uint32, uint32_t)
NDR_SCALAR_PROTO(uint3264, uint32_t)
NDR_SCALAR_PROTO(int32, int32_t)
NDR_SCALAR_PROTO(int3264, int32_t)
NDR_SCALAR_PROTO(udlong, uint64_t)
NDR_SCALAR_PROTO(udlongr, uint64_t)
NDR_SCALAR_PROTO(dlong, int64_t)
NDR_SCALAR_PROTO(hyper, uint64_t)
NDR_SCALAR_PROTO(pointer, void *)
NDR_SCALAR_PROTO(time_t, time_t)
NDR_SCALAR_PROTO(uid_t, uid_t)
NDR_SCALAR_PROTO(gid_t, gid_t)
NDR_SCALAR_PROTO(NTSTATUS, NTSTATUS)
NDR_SCALAR_PROTO(WERROR, WERROR)
NDR_SCALAR_PROTO(NTTIME, NTTIME)
NDR_SCALAR_PROTO(NTTIME_1sec, NTTIME)
NDR_SCALAR_PROTO(NTTIME_hyper, NTTIME)
NDR_SCALAR_PROTO(DATA_BLOB, DATA_BLOB)
NDR_SCALAR_PROTO(ipv4address, const char *)
NDR_SCALAR_PROTO(ipv6address, const char *)
NDR_SCALAR_PROTO(string, const char *)
NDR_SCALAR_PROTO(double, double)

enum ndr_err_code ndr_pull_policy_handle(struct ndr_pull *ndr, int ndr_flags, struct policy_handle *r);
enum ndr_err_code ndr_push_policy_handle(struct ndr_push *ndr, int ndr_flags, const struct policy_handle *r);
void ndr_print_policy_handle(struct ndr_print *ndr, const char *name, const struct policy_handle *r);
bool policy_handle_empty(const struct policy_handle *h);
bool is_valid_policy_hnd(const struct policy_handle *hnd);
bool policy_handle_equal(const struct policy_handle *hnd1,
			 const struct policy_handle *hnd2);

void ndr_check_padding(struct ndr_pull *ndr, size_t n);
enum ndr_err_code ndr_pull_generic_ptr(struct ndr_pull *ndr, uint32_t *v);
enum ndr_err_code ndr_pull_ref_ptr(struct ndr_pull *ndr, uint32_t *v);
enum ndr_err_code ndr_pull_bytes(struct ndr_pull *ndr, uint8_t *data, uint32_t n);
enum ndr_err_code ndr_pull_array_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *data, uint32_t n);
enum ndr_err_code ndr_push_align(struct ndr_push *ndr, size_t size);
enum ndr_err_code ndr_pull_align(struct ndr_pull *ndr, size_t size);
enum ndr_err_code ndr_push_union_align(struct ndr_push *ndr, size_t size);
enum ndr_err_code ndr_pull_union_align(struct ndr_pull *ndr, size_t size);
enum ndr_err_code ndr_push_trailer_align(struct ndr_push *ndr, size_t size);
enum ndr_err_code ndr_pull_trailer_align(struct ndr_pull *ndr, size_t size);
enum ndr_err_code ndr_push_bytes(struct ndr_push *ndr, const uint8_t *data, uint32_t n);
enum ndr_err_code ndr_push_zero(struct ndr_push *ndr, uint32_t n);
enum ndr_err_code ndr_push_array_uint8(struct ndr_push *ndr, int ndr_flags, const uint8_t *data, uint32_t n);
enum ndr_err_code ndr_push_unique_ptr(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_full_ptr(struct ndr_push *ndr, const void *p);
enum ndr_err_code ndr_push_ref_ptr(struct ndr_push *ndr);
void ndr_print_struct(struct ndr_print *ndr, const char *name, const char *type);
void ndr_print_null(struct ndr_print *ndr);
void ndr_print_enum(struct ndr_print *ndr, const char *name, const char *type, const char *val, uint32_t value);
void ndr_print_bitmap_flag(struct ndr_print *ndr, size_t size, const char *flag_name, uint32_t flag, uint32_t value);
void ndr_print_bitmap_flag(struct ndr_print *ndr, size_t size, const char *flag_name, uint32_t flag, uint32_t value);
void ndr_print_ptr(struct ndr_print *ndr, const char *name, const void *p);
void ndr_print_union(struct ndr_print *ndr, const char *name, int level, const char *type);
void ndr_print_bad_level(struct ndr_print *ndr, const char *name, uint16_t level);
void ndr_print_array_uint8(struct ndr_print *ndr, const char *name, const uint8_t *data, uint32_t count);
uint32_t ndr_size_DATA_BLOB(int ret, const DATA_BLOB *data, int flags);

/* strings */
uint32_t ndr_charset_length(const void *var, charset_t chset);
size_t ndr_string_array_size(struct ndr_push *ndr, const char *s);
uint32_t ndr_size_string(int ret, const char * const* string, int flags);
enum ndr_err_code ndr_pull_string_array(struct ndr_pull *ndr, int ndr_flags, const char ***_a);
enum ndr_err_code ndr_push_string_array(struct ndr_push *ndr, int ndr_flags, const char **a);
void ndr_print_string_array(struct ndr_print *ndr, const char *name, const char **a);
size_t ndr_size_string_array(const char **a, uint32_t count, int flags);
uint32_t ndr_string_length(const void *_var, uint32_t element_size);
enum ndr_err_code ndr_check_string_terminator(struct ndr_pull *ndr, uint32_t count, uint32_t element_size);
enum ndr_err_code ndr_pull_charset(struct ndr_pull *ndr, int ndr_flags, const char **var, uint32_t length, uint8_t byte_mul, charset_t chset);
enum ndr_err_code ndr_pull_charset_to_null(struct ndr_pull *ndr, int ndr_flags, const char **var, uint32_t length, uint8_t byte_mul, charset_t chset);
enum ndr_err_code ndr_push_charset(struct ndr_push *ndr, int ndr_flags, const char *var, uint32_t length, uint8_t byte_mul, charset_t chset);

/* GUIDs */
bool GUID_equal(const struct GUID *u1, const struct GUID *u2);
NTSTATUS GUID_to_ndr_blob(const struct GUID *guid, TALLOC_CTX *mem_ctx, DATA_BLOB *b);
NTSTATUS GUID_from_ndr_blob(const DATA_BLOB *b, struct GUID *guid);
NTSTATUS GUID_from_data_blob(const DATA_BLOB *s, struct GUID *guid);
NTSTATUS GUID_from_string(const char *s, struct GUID *guid);
NTSTATUS NS_GUID_from_string(const char *s, struct GUID *guid);
struct GUID GUID_zero(void);
bool GUID_all_zero(const struct GUID *u);
int GUID_compare(const struct GUID *u1, const struct GUID *u2);
char *GUID_string(TALLOC_CTX *mem_ctx, const struct GUID *guid);
char *GUID_string2(TALLOC_CTX *mem_ctx, const struct GUID *guid);
char *GUID_hexstring(TALLOC_CTX *mem_ctx, const struct GUID *guid);
char *NS_GUID_string(TALLOC_CTX *mem_ctx, const struct GUID *guid);
struct GUID GUID_random(void);

_PUBLIC_ enum ndr_err_code ndr_pull_enum_uint8(struct ndr_pull *ndr, int ndr_flags, uint8_t *v);
_PUBLIC_ enum ndr_err_code ndr_pull_enum_uint16(struct ndr_pull *ndr, int ndr_flags, uint16_t *v);
_PUBLIC_ enum ndr_err_code ndr_pull_enum_uint32(struct ndr_pull *ndr, int ndr_flags, uint32_t *v);
_PUBLIC_ enum ndr_err_code ndr_pull_enum_uint1632(struct ndr_pull *ndr, int ndr_flags, uint16_t *v);
_PUBLIC_ enum ndr_err_code ndr_push_enum_uint8(struct ndr_push *ndr, int ndr_flags, uint8_t v);
_PUBLIC_ enum ndr_err_code ndr_push_enum_uint16(struct ndr_push *ndr, int ndr_flags, uint16_t v);
_PUBLIC_ enum ndr_err_code ndr_push_enum_uint32(struct ndr_push *ndr, int ndr_flags, uint32_t v);
_PUBLIC_ enum ndr_err_code ndr_push_enum_uint1632(struct ndr_push *ndr, int ndr_flags, uint16_t v);

_PUBLIC_ void ndr_print_bool(struct ndr_print *ndr, const char *name, const bool b);
_PUBLIC_ void ndr_print_disabled(struct ndr_print *ndr, const char *name, int flags, void *r);

#ifndef VERBOSE_ERROR
#define ndr_print_bool(...) do {} while (0)
#define ndr_print_struct(...) do {} while (0)
#define ndr_print_null(...) do {} while (0)
#define ndr_print_enum(...) do {} while (0)
#define ndr_print_bitmap_flag(...) do {} while (0)
#define ndr_print_ptr(...) do {} while (0)
#define ndr_print_union(...) do {} while (0)
#define ndr_print_bad_level(...) do {} while (0)
#define ndr_print_array_uint8(...) do {} while (0)
#define ndr_print_string_array(...) do {} while (0)
#define ndr_print_string_array(...) do {} while (0)
#define ndr_print_NTSTATUS(...) do {} while (0)
#define ndr_print_WERROR(...) do {} while (0)
#endif

#endif /* __LIBNDR_H__ */
