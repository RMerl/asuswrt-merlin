/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling spoolss subcontext buffer structures

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Tim Potter 2003
   Copyright (C) Guenther Deschner 2009

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


#include "includes.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "librpc/gen_ndr/ndr_security.h"
#if (_SAMBA_BUILD_ >= 4)
#include "param/param.h"
#endif

#define NDR_SPOOLSS_PUSH_ENUM_IN(fn) do { \
	if (!r->in.buffer && r->in.offered != 0) {\
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE,\
			"SPOOLSS Buffer: r->in.offered[%u] but there's no buffer",\
			(unsigned)r->in.offered);\
	} else if (r->in.buffer && r->in.buffer->length != r->in.offered) {\
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE,\
			"SPOOLSS Buffer: r->in.offered[%u] doesn't match length of r->in.buffer[%u]",\
			(unsigned)r->in.offered, (unsigned)r->in.buffer->length);\
	}\
	_r.in.level	= r->in.level;\
	_r.in.buffer	= r->in.buffer;\
	_r.in.offered	= r->in.offered;\
	NDR_CHECK(ndr_push__##fn(ndr, flags, &_r));\
} while(0)

#define NDR_SPOOLSS_PUSH_ENUM_OUT(fn) do { \
	struct ndr_push *_ndr_info;\
	_r.in.level	= r->in.level;\
	_r.in.buffer	= r->in.buffer;\
	_r.in.offered	= r->in.offered;\
	_r.out.info	= NULL;\
	_r.out.needed	= r->out.needed;\
	_r.out.count	= r->out.count;\
	_r.out.result	= r->out.result;\
	if (r->out.info && *r->out.info && !r->in.buffer) {\
		return ndr_push_error(ndr, NDR_ERR_BUFSIZE,\
			"SPOOLSS Buffer: *r->out.info but there's no r->in.buffer");\
	}\
	if (r->in.buffer) {\
		DATA_BLOB _data_blob_info;\
		_ndr_info = ndr_push_init_ctx(ndr);\
		NDR_ERR_HAVE_NO_MEMORY(_ndr_info);\
		_ndr_info->flags= ndr->flags;\
		if (r->out.info) {\
			struct ndr_push *_subndr_info;\
			struct __##fn __r;\
			__r.in.level	= r->in.level;\
			__r.in.count	= *r->out.count;\
			__r.out.info	= *r->out.info;\
			NDR_CHECK(ndr_push_subcontext_start(_ndr_info, &_subndr_info, 0, r->in.offered));\
			NDR_CHECK(ndr_push___##fn(_subndr_info, flags, &__r)); \
			NDR_CHECK(ndr_push_subcontext_end(_ndr_info, _subndr_info, 0, r->in.offered));\
		}\
		if (r->in.offered > _ndr_info->offset) {\
			uint32_t _padding_len = r->in.offered - _ndr_info->offset;\
			NDR_CHECK(ndr_push_zero(_ndr_info, _padding_len));\
		} else if (r->in.offered < _ndr_info->offset) {\
			return ndr_push_error(ndr, NDR_ERR_BUFSIZE,\
				"SPOOLSS Buffer: r->in.offered[%u] doesn't match length of out buffer[%u]!",\
				(unsigned)r->in.offered, (unsigned)_ndr_info->offset);\
		}\
		_data_blob_info = ndr_push_blob(_ndr_info);\
		_r.out.info	= &_data_blob_info;\
	}\
	NDR_CHECK(ndr_push__##fn(ndr, flags, &_r));\
} while(0)

#define NDR_SPOOLSS_PUSH_ENUM(fn,in,out) do { \
	struct _##fn _r;\
	if (flags & NDR_IN) {\
		in;\
		NDR_SPOOLSS_PUSH_ENUM_IN(fn);\
	}\
	if (flags & NDR_OUT) {\
		out;\
		NDR_SPOOLSS_PUSH_ENUM_OUT(fn);\
	}\
} while(0)

#define NDR_SPOOLSS_PULL_ENUM_IN(fn) do { \
	ZERO_STRUCT(r->out);\
	NDR_CHECK(ndr_pull__##fn(ndr, flags, &_r));\
	r->in.level	= _r.in.level;\
	r->in.buffer	= _r.in.buffer;\
	r->in.offered	= _r.in.offered;\
	r->out.needed	= _r.out.needed;\
	r->out.count	= _r.out.count;\
	if (!r->in.buffer && r->in.offered != 0) {\
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE,\
			"SPOOLSS Buffer: r->in.offered[%u] but there's no buffer",\
			(unsigned)r->in.offered);\
	} else if (r->in.buffer && r->in.buffer->length != r->in.offered) {\
		return ndr_pull_error(ndr, NDR_ERR_BUFSIZE,\
			"SPOOLSS Buffer: r->in.offered[%u] doesn't match length of r->in.buffer[%u]",\
			(unsigned)r->in.offered, (unsigned)r->in.buffer->length);\
	}\
	NDR_PULL_ALLOC(ndr, r->out.info);\
	ZERO_STRUCTP(r->out.info);\
} while(0)

#define NDR_SPOOLSS_PULL_ENUM_OUT(fn) do { \
	_r.in.level	= r->in.level;\
	_r.in.buffer	= r->in.buffer;\
	_r.in.offered	= r->in.offered;\
	_r.out.needed	= r->out.needed;\
	_r.out.count	= r->out.count;\
	NDR_CHECK(ndr_pull__##fn(ndr, flags, &_r));\
	if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {\
		NDR_PULL_ALLOC(ndr, r->out.info);\
	}\
	*r->out.info = NULL;\
	r->out.needed	= _r.out.needed;\
	r->out.count	= _r.out.count;\
	r->out.result	= _r.out.result;\
	if (_r.out.info) {\
		struct ndr_pull *_ndr_info;\
		NDR_PULL_ALLOC(ndr, *r->out.info);\
		_ndr_info = ndr_pull_init_blob(_r.out.info, *r->out.info);\
		NDR_ERR_HAVE_NO_MEMORY(_ndr_info);\
		_ndr_info->flags= ndr->flags;\
		if (r->in.offered != _ndr_info->data_size) {\
			return ndr_pull_error(ndr, NDR_ERR_BUFSIZE,\
				"SPOOLSS Buffer: offered[%u] doesn't match length of buffer[%u]",\
				(unsigned)r->in.offered, (unsigned)_ndr_info->data_size);\
		}\
		if (*r->out.needed <= _ndr_info->data_size) {\
			struct __##fn __r;\
			__r.in.level	= r->in.level;\
			__r.in.count	= *r->out.count;\
			__r.out.info	= NULL;\
			NDR_CHECK(ndr_pull___##fn(_ndr_info, flags, &__r));\
			*r->out.info	= __r.out.info;\
		}\
	}\
} while(0)

#define NDR_SPOOLSS_PULL_ENUM(fn,in,out) do { \
	struct _##fn _r;\
	if (flags & NDR_IN) {\
		out;\
		NDR_SPOOLSS_PULL_ENUM_IN(fn);\
		in;\
	}\
	if (flags & NDR_OUT) {\
		out;\
		NDR_SPOOLSS_PULL_ENUM_OUT(fn);\
	}\
} while(0)

#define _NDR_CHECK_UINT32(call) do {\
	enum ndr_err_code _ndr_err; \
        _ndr_err = call; \
	if (!NDR_ERR_CODE_IS_SUCCESS(_ndr_err)) { \
		return 0; \
	}\
} while (0)

/* TODO: set _ndr_info->flags correct */
#define NDR_SPOOLSS_SIZE_ENUM_LEVEL(fn) do { \
	struct __##fn __r;\
	DATA_BLOB _data_blob_info;\
	struct ndr_push *_ndr_info = ndr_push_init_ctx(mem_ctx);\
	if (!_ndr_info) return 0;\
	_ndr_info->flags|=LIBNDR_FLAG_NO_NDR_SIZE;\
	__r.in.level	= level;\
	__r.in.count	= count;\
	__r.out.info	= info;\
	_NDR_CHECK_UINT32(ndr_push___##fn(_ndr_info, NDR_OUT, &__r)); \
	_data_blob_info = ndr_push_blob(_ndr_info);\
	return _data_blob_info.length;\
} while(0)

/* TODO: set _ndr_info->flags correct */
#define NDR_SPOOLSS_SIZE_ENUM(fn) do { \
	struct __##fn __r;\
	DATA_BLOB _data_blob_info;\
	struct ndr_push *_ndr_info = ndr_push_init_ctx(mem_ctx);\
	if (!_ndr_info) return 0;\
	_ndr_info->flags|=LIBNDR_FLAG_NO_NDR_SIZE;\
	__r.in.count	= count;\
	__r.out.info	= info;\
	_NDR_CHECK_UINT32(ndr_push___##fn(_ndr_info, NDR_OUT, &__r)); \
	_data_blob_info = ndr_push_blob(_ndr_info);\
	return _data_blob_info.length;\
} while(0)


/*
  spoolss_EnumPrinters
*/
enum ndr_err_code ndr_push_spoolss_EnumPrinters(struct ndr_push *ndr, int flags, const struct spoolss_EnumPrinters *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumPrinters,{
		_r.in.flags	= r->in.flags;
		_r.in.server	= r->in.server;
	},{
		_r.in.flags	= r->in.flags;
		_r.in.server	= r->in.server;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPrinters(struct ndr_pull *ndr, int flags, struct spoolss_EnumPrinters *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumPrinters,{
		r->in.flags	= _r.in.flags;
		r->in.server	= _r.in.server;
	},{
		_r.in.flags	= r->in.flags;
		_r.in.server	= r->in.server;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPrinters_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_PrinterInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumPrinters);
}

/*
  spoolss_EnumJobs
*/
enum ndr_err_code ndr_push_spoolss_EnumJobs(struct ndr_push *ndr, int flags, const struct spoolss_EnumJobs *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumJobs,{
		_r.in.handle	= r->in.handle;
		_r.in.firstjob	= r->in.firstjob;
		_r.in.numjobs	= r->in.numjobs;
	},{
		_r.in.handle	= r->in.handle;
		_r.in.firstjob	= r->in.firstjob;
		_r.in.numjobs	= r->in.numjobs;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumJobs(struct ndr_pull *ndr, int flags, struct spoolss_EnumJobs *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumJobs,{
		r->in.handle	= _r.in.handle;
		r->in.firstjob	= _r.in.firstjob;
		r->in.numjobs	= _r.in.numjobs;
	},{
		_r.in.handle	= r->in.handle;
		_r.in.firstjob	= r->in.firstjob;
		_r.in.numjobs	= r->in.numjobs;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumJobs_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_JobInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumJobs);
}

/*
  spoolss_EnumPrinterDrivers
*/
enum ndr_err_code ndr_push_spoolss_EnumPrinterDrivers(struct ndr_push *ndr, int flags, const struct spoolss_EnumPrinterDrivers *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumPrinterDrivers,{
		_r.in.server		= r->in.server;
		_r.in.environment	= r->in.environment;
	},{
		_r.in.server		= r->in.server;
		_r.in.environment	= r->in.environment;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPrinterDrivers(struct ndr_pull *ndr, int flags, struct spoolss_EnumPrinterDrivers *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumPrinterDrivers,{
		r->in.server		= _r.in.server;
		r->in.environment	= _r.in.environment;
	},{
		_r.in.server		= r->in.server;
		_r.in.environment	= r->in.environment;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPrinterDrivers_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_DriverInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumPrinterDrivers);
}

/*
  spoolss_EnumForms
*/
enum ndr_err_code ndr_push_spoolss_EnumForms(struct ndr_push *ndr, int flags, const struct spoolss_EnumForms *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumForms,{
		_r.in.handle	= r->in.handle;
	},{
		_r.in.handle	= r->in.handle;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumForms(struct ndr_pull *ndr, int flags, struct spoolss_EnumForms *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumForms,{
		r->in.handle	= _r.in.handle;
	},{
		_r.in.handle	= r->in.handle;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumForms_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_FormInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumForms);
}

/*
  spoolss_EnumPorts
*/
enum ndr_err_code ndr_push_spoolss_EnumPorts(struct ndr_push *ndr, int flags, const struct spoolss_EnumPorts *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumPorts,{
		_r.in.servername= r->in.servername;
	},{
		_r.in.servername= r->in.servername;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPorts(struct ndr_pull *ndr, int flags, struct spoolss_EnumPorts *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumPorts,{
		r->in.servername= _r.in.servername;
	},{
		_r.in.servername= r->in.servername;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPorts_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_PortInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumPorts);
}

/*
  spoolss_EnumMonitors
*/
enum ndr_err_code ndr_push_spoolss_EnumMonitors(struct ndr_push *ndr, int flags, const struct spoolss_EnumMonitors *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumMonitors,{
		_r.in.servername= r->in.servername;
	},{
		_r.in.servername= r->in.servername;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumMonitors(struct ndr_pull *ndr, int flags, struct spoolss_EnumMonitors *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumMonitors,{
		r->in.servername= _r.in.servername;
	},{
		_r.in.servername= r->in.servername;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumMonitors_info(TALLOC_CTX *mem_ctx, uint32_t level, uint32_t count, union spoolss_MonitorInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumMonitors);
}

/*
  spoolss_EnumPrintProcessors
*/
enum ndr_err_code ndr_push_spoolss_EnumPrintProcessors(struct ndr_push *ndr, int flags, const struct spoolss_EnumPrintProcessors *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumPrintProcessors,{
		_r.in.servername	= r->in.servername;
		_r.in.environment	= r->in.environment;
	},{
		_r.in.servername	= r->in.servername;
		_r.in.environment	= r->in.environment;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPrintProcessors(struct ndr_pull *ndr, int flags, struct spoolss_EnumPrintProcessors *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumPrintProcessors,{
		r->in.servername	= _r.in.servername;
		r->in.environment	= _r.in.environment;
	},{
		_r.in.servername	= r->in.servername;
		_r.in.environment	= r->in.environment;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPrintProcessors_info(TALLOC_CTX *mem_ctx, 
						   uint32_t level, uint32_t count, union spoolss_PrintProcessorInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcessors);
}

/*
  spoolss_EnumPrintProcessors
*/
enum ndr_err_code ndr_push_spoolss_EnumPrintProcDataTypes(struct ndr_push *ndr, int flags, const struct spoolss_EnumPrintProcDataTypes *r)
{
	NDR_SPOOLSS_PUSH_ENUM(spoolss_EnumPrintProcDataTypes,{
		_r.in.servername		= r->in.servername;
		_r.in.print_processor_name	= r->in.print_processor_name;
	},{
		_r.in.servername		= r->in.servername;
		_r.in.print_processor_name	= r->in.print_processor_name;
	});
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPrintProcDataTypes(struct ndr_pull *ndr, int flags, struct spoolss_EnumPrintProcDataTypes *r)
{
	NDR_SPOOLSS_PULL_ENUM(spoolss_EnumPrintProcDataTypes,{
		r->in.servername		= _r.in.servername;
		r->in.print_processor_name	= _r.in.print_processor_name;
	},{
		_r.in.servername		= r->in.servername;
		_r.in.print_processor_name	= r->in.print_processor_name;
	});
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPrintProcDataTypes_info(TALLOC_CTX *mem_ctx, 
						      uint32_t level, uint32_t count, union spoolss_PrintProcDataTypesInfo *info)
{
	NDR_SPOOLSS_SIZE_ENUM_LEVEL(spoolss_EnumPrintProcDataTypes);
}

/*
  spoolss_EnumPrinterDataEx
*/

enum ndr_err_code ndr_push_spoolss_EnumPrinterDataEx(struct ndr_push *ndr, int flags, const struct spoolss_EnumPrinterDataEx *r)
{
	struct _spoolss_EnumPrinterDataEx _r;
	if (flags & NDR_IN) {
		_r.in.handle	= r->in.handle;
		_r.in.key_name	= r->in.key_name;
		_r.in.offered	= r->in.offered;
		NDR_CHECK(ndr_push__spoolss_EnumPrinterDataEx(ndr, flags, &_r));
	}
	if (flags & NDR_OUT) {
		struct ndr_push *_ndr_info;
		_r.in.handle	= r->in.handle;
		_r.in.key_name	= r->in.key_name;
		_r.in.offered	= r->in.offered;
		_r.out.count	= r->out.count;
		_r.out.needed	= r->out.needed;
		_r.out.result	= r->out.result;
		_r.out.info 	= data_blob(NULL, 0);
		if (r->in.offered >= *r->out.needed) {
			struct ndr_push *_subndr_info;
			struct __spoolss_EnumPrinterDataEx __r;
			_ndr_info = ndr_push_init_ctx(ndr);
			NDR_ERR_HAVE_NO_MEMORY(_ndr_info);
			_ndr_info->flags= ndr->flags;
			__r.in.count	= *r->out.count;
			__r.out.info	= *r->out.info;
			NDR_CHECK(ndr_push_subcontext_start(_ndr_info, &_subndr_info, 0, r->in.offered));
			NDR_CHECK(ndr_push___spoolss_EnumPrinterDataEx(_subndr_info, flags, &__r));
			NDR_CHECK(ndr_push_subcontext_end(_ndr_info, _subndr_info, 0, r->in.offered));
			if (r->in.offered > _ndr_info->offset) {
				uint32_t _padding_len = r->in.offered - _ndr_info->offset;
				NDR_CHECK(ndr_push_zero(_ndr_info, _padding_len));
			}
			_r.out.info = ndr_push_blob(_ndr_info);
		}
		NDR_CHECK(ndr_push__spoolss_EnumPrinterDataEx(ndr, flags, &_r));
	}
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_spoolss_EnumPrinterDataEx(struct ndr_pull *ndr, int flags, struct spoolss_EnumPrinterDataEx *r)
{
	struct _spoolss_EnumPrinterDataEx _r;
	if (flags & NDR_IN) {
		_r.in.handle	= r->in.handle;
		_r.in.key_name	= r->in.key_name;
		ZERO_STRUCT(r->out);
		NDR_CHECK(ndr_pull__spoolss_EnumPrinterDataEx(ndr, flags, &_r));
		r->in.handle	= _r.in.handle;
		r->in.key_name	= _r.in.key_name;
		r->in.offered	= _r.in.offered;
		r->out.needed	= _r.out.needed;
		r->out.count	= _r.out.count;
		NDR_PULL_ALLOC(ndr, r->out.info);
		ZERO_STRUCTP(r->out.info);
	}
	if (flags & NDR_OUT) {
		_r.in.handle	= r->in.handle;
		_r.in.key_name	= r->in.key_name;
		_r.in.offered	= r->in.offered;
		_r.out.count	= r->out.count;
		_r.out.needed	= r->out.needed;
		NDR_CHECK(ndr_pull__spoolss_EnumPrinterDataEx(ndr, flags, &_r));
		if (ndr->flags & LIBNDR_FLAG_REF_ALLOC) {
			NDR_PULL_ALLOC(ndr, r->out.info);
		}
		*r->out.info	= NULL;
		r->out.needed	= _r.out.needed;
		r->out.count	= _r.out.count;
		r->out.result	= _r.out.result;
		if (_r.out.info.length) {
			struct ndr_pull *_ndr_info;
			NDR_PULL_ALLOC(ndr, *r->out.info);
			_ndr_info = ndr_pull_init_blob(&_r.out.info, *r->out.info);
			NDR_ERR_HAVE_NO_MEMORY(_ndr_info);
			_ndr_info->flags= ndr->flags;
			if (r->in.offered != _ndr_info->data_size) {
				return ndr_pull_error(ndr, NDR_ERR_BUFSIZE,
					"SPOOLSS Buffer: offered[%u] doesn't match length of buffer[%u]",
					(unsigned)r->in.offered, (unsigned)_ndr_info->data_size);
			}
			if (*r->out.needed <= _ndr_info->data_size) {
				struct __spoolss_EnumPrinterDataEx __r;
				__r.in.count	= *r->out.count;
				__r.out.info	= NULL;
				NDR_CHECK(ndr_pull___spoolss_EnumPrinterDataEx(_ndr_info, flags, &__r));
				*r->out.info	= __r.out.info;
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

uint32_t ndr_size_spoolss_EnumPrinterDataEx_info(TALLOC_CTX *mem_ctx,
						 uint32_t count, struct spoolss_PrinterEnumValues *info)
{
	NDR_SPOOLSS_SIZE_ENUM(spoolss_EnumPrinterDataEx);
}

uint32_t _ndr_size_spoolss_DeviceMode(struct spoolss_DeviceMode *devmode, uint32_t flags)
{
	if (!devmode) return 0;
	return ndr_size_spoolss_DeviceMode(devmode, flags);
}

_PUBLIC_ size_t ndr_size_spoolss_StringArray(const struct spoolss_StringArray *r, int flags)
{
	if (!r) {
		return 4;
	}

	return ndr_size_struct(r, flags, (ndr_push_flags_fn_t)ndr_push_spoolss_StringArray);
}

/* hand marshall as pidl cannot (yet) generate a relative pointer to a fixed array of
 * structs */

_PUBLIC_ enum ndr_err_code ndr_push_spoolss_DriverInfo101(struct ndr_push *ndr, int ndr_flags, const struct spoolss_DriverInfo101 *r)
{
	uint32_t cntr_file_info_1;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 8));
		NDR_CHECK(ndr_push_spoolss_DriverOSVersion(ndr, NDR_SCALARS, r->version));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->driver_name));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->architecture));
			ndr->flags = _flags_save_string;
		}
		NDR_CHECK(ndr_push_relative_ptr1(ndr, r->file_info));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->file_count));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->monitor_name));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->default_datatype));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string_array = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->previous_names));
			ndr->flags = _flags_save_string_array;
		}
		NDR_CHECK(ndr_push_NTTIME(ndr, NDR_SCALARS, r->driver_date));
		NDR_CHECK(ndr_push_hyper(ndr, NDR_SCALARS, r->driver_version));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->manufacturer_name));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->manufacturer_url));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->hardware_id));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->provider));
			ndr->flags = _flags_save_string;
		}
		NDR_CHECK(ndr_push_trailer_align(ndr, 8));
	}
	if (ndr_flags & NDR_BUFFERS) {
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->driver_name) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->driver_name));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->driver_name));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->driver_name));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->architecture) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->architecture));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->architecture));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->architecture));
			}
			ndr->flags = _flags_save_string;
		}
		if (r->file_info) {
			NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->file_info));
#if 0
			NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, r->file_count));
#endif
			for (cntr_file_info_1 = 0; cntr_file_info_1 < r->file_count; cntr_file_info_1++) {
				NDR_CHECK(ndr_push_spoolss_DriverFileInfo(ndr, NDR_SCALARS, &r->file_info[cntr_file_info_1]));
			}
			for (cntr_file_info_1 = 0; cntr_file_info_1 < r->file_count; cntr_file_info_1++) {
				NDR_CHECK(ndr_push_spoolss_DriverFileInfo(ndr, NDR_BUFFERS, &r->file_info[cntr_file_info_1]));
			}
			NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->file_info));
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->monitor_name) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->monitor_name));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->monitor_name));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->monitor_name));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->default_datatype) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->default_datatype));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->default_datatype));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->default_datatype));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string_array = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->previous_names) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->previous_names));
				NDR_CHECK(ndr_push_string_array(ndr, NDR_SCALARS, r->previous_names));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->previous_names));
			}
			ndr->flags = _flags_save_string_array;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->manufacturer_name) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->manufacturer_name));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->manufacturer_name));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->manufacturer_name));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->manufacturer_url) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->manufacturer_url));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->manufacturer_url));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->manufacturer_url));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->hardware_id) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->hardware_id));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->hardware_id));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->hardware_id));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->provider) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->provider));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->provider));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->provider));
			}
			ndr->flags = _flags_save_string;
		}
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_spoolss_DriverInfo101(struct ndr_pull *ndr, int ndr_flags, struct spoolss_DriverInfo101 *r)
{
	uint32_t _ptr_driver_name;
	TALLOC_CTX *_mem_save_driver_name_0;
	uint32_t _ptr_architecture;
	TALLOC_CTX *_mem_save_architecture_0;
	uint32_t _ptr_file_info;
	uint32_t cntr_file_info_1;
	TALLOC_CTX *_mem_save_file_info_0;
	TALLOC_CTX *_mem_save_file_info_1;
	uint32_t _ptr_monitor_name;
	TALLOC_CTX *_mem_save_monitor_name_0;
	uint32_t _ptr_default_datatype;
	TALLOC_CTX *_mem_save_default_datatype_0;
	uint32_t _ptr_previous_names;
	TALLOC_CTX *_mem_save_previous_names_0;
	uint32_t _ptr_manufacturer_name;
	TALLOC_CTX *_mem_save_manufacturer_name_0;
	uint32_t _ptr_manufacturer_url;
	TALLOC_CTX *_mem_save_manufacturer_url_0;
	uint32_t _ptr_hardware_id;
	TALLOC_CTX *_mem_save_hardware_id_0;
	uint32_t _ptr_provider;
	TALLOC_CTX *_mem_save_provider_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 8));
		NDR_CHECK(ndr_pull_spoolss_DriverOSVersion(ndr, NDR_SCALARS, &r->version));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_driver_name));
			if (_ptr_driver_name) {
				NDR_PULL_ALLOC(ndr, r->driver_name);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->driver_name, _ptr_driver_name));
			} else {
				r->driver_name = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_architecture));
			if (_ptr_architecture) {
				NDR_PULL_ALLOC(ndr, r->architecture);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->architecture, _ptr_architecture));
			} else {
				r->architecture = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_file_info));
		if (_ptr_file_info) {
			NDR_PULL_ALLOC(ndr, r->file_info);
			NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->file_info, _ptr_file_info));
		} else {
			r->file_info = NULL;
		}
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->file_count));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_monitor_name));
			if (_ptr_monitor_name) {
				NDR_PULL_ALLOC(ndr, r->monitor_name);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->monitor_name, _ptr_monitor_name));
			} else {
				r->monitor_name = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_default_datatype));
			if (_ptr_default_datatype) {
				NDR_PULL_ALLOC(ndr, r->default_datatype);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->default_datatype, _ptr_default_datatype));
			} else {
				r->default_datatype = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string_array = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_previous_names));
			if (_ptr_previous_names) {
				NDR_PULL_ALLOC(ndr, r->previous_names);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->previous_names, _ptr_previous_names));
			} else {
				r->previous_names = NULL;
			}
			ndr->flags = _flags_save_string_array;
		}
		NDR_CHECK(ndr_pull_NTTIME(ndr, NDR_SCALARS, &r->driver_date));
		NDR_CHECK(ndr_pull_hyper(ndr, NDR_SCALARS, &r->driver_version));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_manufacturer_name));
			if (_ptr_manufacturer_name) {
				NDR_PULL_ALLOC(ndr, r->manufacturer_name);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->manufacturer_name, _ptr_manufacturer_name));
			} else {
				r->manufacturer_name = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_manufacturer_url));
			if (_ptr_manufacturer_url) {
				NDR_PULL_ALLOC(ndr, r->manufacturer_url);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->manufacturer_url, _ptr_manufacturer_url));
			} else {
				r->manufacturer_url = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_hardware_id));
			if (_ptr_hardware_id) {
				NDR_PULL_ALLOC(ndr, r->hardware_id);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->hardware_id, _ptr_hardware_id));
			} else {
				r->hardware_id = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_provider));
			if (_ptr_provider) {
				NDR_PULL_ALLOC(ndr, r->provider);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->provider, _ptr_provider));
			} else {
				r->provider = NULL;
			}
			ndr->flags = _flags_save_string;
		}
		NDR_CHECK(ndr_pull_trailer_align(ndr, 8));
	}
	if (ndr_flags & NDR_BUFFERS) {
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->driver_name) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->driver_name));
				_mem_save_driver_name_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->driver_name, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->driver_name));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_driver_name_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->architecture) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->architecture));
				_mem_save_architecture_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->architecture, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->architecture));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_architecture_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		if (r->file_info) {
			uint32_t _relative_save_offset;
			_relative_save_offset = ndr->offset;
			NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->file_info));
			_mem_save_file_info_0 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->file_info, 0);
#if 0
			NDR_CHECK(ndr_pull_array_size(ndr, &r->file_info));
#else
			NDR_CHECK(ndr_token_store(ndr, &ndr->array_size_list, &r->file_info, r->file_count));
#endif
			NDR_PULL_ALLOC_N(ndr, r->file_info, ndr_get_array_size(ndr, &r->file_info));
			_mem_save_file_info_1 = NDR_PULL_GET_MEM_CTX(ndr);
			NDR_PULL_SET_MEM_CTX(ndr, r->file_info, 0);
			for (cntr_file_info_1 = 0; cntr_file_info_1 < r->file_count; cntr_file_info_1++) {
				NDR_CHECK(ndr_pull_spoolss_DriverFileInfo(ndr, NDR_SCALARS, &r->file_info[cntr_file_info_1]));
			}
			for (cntr_file_info_1 = 0; cntr_file_info_1 < r->file_count; cntr_file_info_1++) {
				NDR_CHECK(ndr_pull_spoolss_DriverFileInfo(ndr, NDR_BUFFERS, &r->file_info[cntr_file_info_1]));
			}
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_file_info_1, 0);
			NDR_PULL_SET_MEM_CTX(ndr, _mem_save_file_info_0, 0);
			if (ndr->offset > ndr->relative_highest_offset) {
				ndr->relative_highest_offset = ndr->offset;
			}
			ndr->offset = _relative_save_offset;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->monitor_name) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->monitor_name));
				_mem_save_monitor_name_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->monitor_name, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->monitor_name));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_monitor_name_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->default_datatype) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->default_datatype));
				_mem_save_default_datatype_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->default_datatype, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->default_datatype));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_default_datatype_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string_array = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->previous_names) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->previous_names));
				_mem_save_previous_names_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->previous_names, 0);
				NDR_CHECK(ndr_pull_string_array(ndr, NDR_SCALARS, &r->previous_names));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_previous_names_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string_array;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->manufacturer_name) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->manufacturer_name));
				_mem_save_manufacturer_name_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->manufacturer_name, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->manufacturer_name));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_manufacturer_name_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->manufacturer_url) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->manufacturer_url));
				_mem_save_manufacturer_url_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->manufacturer_url, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->manufacturer_url));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_manufacturer_url_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->hardware_id) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->hardware_id));
				_mem_save_hardware_id_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->hardware_id, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->hardware_id));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_hardware_id_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->provider) {
				uint32_t _relative_save_offset;
				_relative_save_offset = ndr->offset;
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->provider));
				_mem_save_provider_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->provider, 0);
				NDR_CHECK(ndr_pull_string(ndr, NDR_SCALARS, &r->provider));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_provider_0, 0);
				if (ndr->offset > ndr->relative_highest_offset) {
					ndr->relative_highest_offset = ndr->offset;
				}
				ndr->offset = _relative_save_offset;
			}
			ndr->flags = _flags_save_string;
		}
		if (r->file_info) {
			NDR_CHECK(ndr_check_array_size(ndr, (void*)&r->file_info, r->file_count));
		}
	}
	return NDR_ERR_SUCCESS;
}

void ndr_print_spoolss_Field(struct ndr_print *ndr, const char *name, const union spoolss_Field *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	ndr_print_union(ndr, name, level, "spoolss_Field");
	switch (level) {
		case PRINTER_NOTIFY_TYPE:
			ndr_print_spoolss_PrintNotifyField(ndr, "field", r->field);
		break;

		case JOB_NOTIFY_TYPE:
			ndr_print_spoolss_JobNotifyField(ndr, "field", r->field);
		break;

		default:
			ndr_print_uint16(ndr, "field", r->field);
		break;

	}
}

_PUBLIC_ size_t ndr_size_spoolss_PrinterData(const union spoolss_PrinterData *r, uint32_t level, int flags)
{
	if (!r) {
		return 0;
	}
	return ndr_size_union(r, flags, level, (ndr_push_flags_fn_t)ndr_push_spoolss_PrinterData);
}

void ndr_print_spoolss_security_descriptor(struct ndr_print *ndr, const char *name, const struct security_descriptor *r)
{
	ndr_print_security_descriptor(ndr, name, r);
}

enum ndr_err_code ndr_pull_spoolss_security_descriptor(struct ndr_pull *ndr, int ndr_flags, struct security_descriptor *r)
{
	uint32_t _flags_save_STRUCT = ndr->flags;
	ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NO_RELATIVE_REVERSE);
	NDR_CHECK(ndr_pull_security_descriptor(ndr, ndr_flags, r));
	ndr->flags = _flags_save_STRUCT;
	return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_push_spoolss_security_descriptor(struct ndr_push *ndr, int ndr_flags, const struct security_descriptor *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_LITTLE_ENDIAN|LIBNDR_FLAG_NO_RELATIVE_REVERSE);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 5));
			NDR_CHECK(ndr_push_security_descriptor_revision(ndr, NDR_SCALARS, r->revision));
			NDR_CHECK(ndr_push_security_descriptor_type(ndr, NDR_SCALARS, r->type));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->owner_sid));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->group_sid));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->sacl));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->dacl));
			NDR_CHECK(ndr_push_trailer_align(ndr, 5));
		}
		if (ndr_flags & NDR_BUFFERS) {
			if (r->sacl) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->sacl));
				NDR_CHECK(ndr_push_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->sacl));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->sacl));
			}
			if (r->dacl) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->dacl));
				NDR_CHECK(ndr_push_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->dacl));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->dacl));
			}
			if (r->owner_sid) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->owner_sid));
				NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, r->owner_sid));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->owner_sid));
			}
			if (r->group_sid) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->group_sid));
				NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, r->group_sid));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->group_sid));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_push_spoolss_PrinterInfo2(struct ndr_push *ndr, int ndr_flags, const struct spoolss_PrinterInfo2 *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 5));
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->servername));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->printername));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->sharename));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->portname));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->drivername));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->comment));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->location));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_spoolss_DeviceMode = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_ALIGN4);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->devmode));
			ndr->flags = _flags_save_spoolss_DeviceMode;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->sepfile));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->printprocessor));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->datatype));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->parameters));
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_spoolss_security_descriptor = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_ALIGN4);
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->secdesc));
			ndr->flags = _flags_save_spoolss_security_descriptor;
		}
		NDR_CHECK(ndr_push_spoolss_PrinterAttributes(ndr, NDR_SCALARS, r->attributes));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->priority));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->defaultpriority));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->starttime));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->untiltime));
		NDR_CHECK(ndr_push_spoolss_PrinterStatus(ndr, NDR_SCALARS, r->status));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->cjobs));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->averageppm));
		NDR_CHECK(ndr_push_trailer_align(ndr, 5));
	}
	if (ndr_flags & NDR_BUFFERS) {
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->servername) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->servername));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->servername));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->servername));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->printername) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->printername));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->printername));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->printername));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->sharename) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->sharename));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->sharename));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->sharename));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->portname) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->portname));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->portname));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->portname));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->drivername) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->drivername));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->drivername));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->drivername));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->comment) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->comment));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->comment));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->comment));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->location) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->location));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->location));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->location));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->sepfile) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->sepfile));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->sepfile));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->sepfile));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->printprocessor) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->printprocessor));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->printprocessor));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->printprocessor));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->datatype) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->datatype));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->datatype));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->datatype));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_string = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NULLTERM);
			if (r->parameters) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->parameters));
				NDR_CHECK(ndr_push_string(ndr, NDR_SCALARS, r->parameters));
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->parameters));
			}
			ndr->flags = _flags_save_string;
		}
		{
			uint32_t _flags_save_spoolss_DeviceMode = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_ALIGN4);
			if (r->devmode) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->devmode));
				{
					struct ndr_push *_ndr_devmode;
					NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_devmode, 0, -1));
					NDR_CHECK(ndr_push_spoolss_DeviceMode(_ndr_devmode, NDR_SCALARS, r->devmode));
					NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_devmode, 0, -1));
				}
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->devmode));
			}
			ndr->flags = _flags_save_spoolss_DeviceMode;
		}
		{
			uint32_t _flags_save_spoolss_security_descriptor = ndr->flags;
			ndr_set_flags(&ndr->flags, LIBNDR_FLAG_ALIGN4);
			if (r->secdesc) {
				NDR_CHECK(ndr_push_relative_ptr2_start(ndr, r->secdesc));
				{
					struct ndr_push *_ndr_secdesc;
					NDR_CHECK(ndr_push_subcontext_start(ndr, &_ndr_secdesc, 0, -1));
					NDR_CHECK(ndr_push_spoolss_security_descriptor(_ndr_secdesc, NDR_SCALARS|NDR_BUFFERS, r->secdesc));
					NDR_CHECK(ndr_push_subcontext_end(ndr, _ndr_secdesc, 0, -1));
				}
				NDR_CHECK(ndr_push_relative_ptr2_end(ndr, r->secdesc));
			}
			ndr->flags = _flags_save_spoolss_security_descriptor;
		}
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ void ndr_print_spoolss_Time(struct ndr_print *ndr, const char *name, const struct spoolss_Time *r)
{
	struct tm tm;
	time_t t;
	char *str;

	tm.tm_sec	= r->second;
	tm.tm_min	= r->minute;
	tm.tm_hour	= r->hour;
	tm.tm_mday	= r->day;
	tm.tm_mon	= r->month - 1;
	tm.tm_year	= r->year - 1900;
	tm.tm_wday	= r->day_of_week;
	tm.tm_yday	= 0;
	tm.tm_isdst	= -1;

	t = mktime(&tm);

	str = timestring(ndr, t);

	ndr_print_struct(ndr, name, "spoolss_Time");
	ndr->depth++;
	ndr_print_string(ndr, "", str);
	ndr->depth--;
	talloc_free(str);
}

_PUBLIC_ uint32_t ndr_spoolss_PrinterEnumValues_align(enum winreg_Type type)
{
	switch(type) {
	case REG_NONE:
		return 0;
	case REG_SZ:
		return LIBNDR_FLAG_ALIGN2;
	case REG_EXPAND_SZ:
		return LIBNDR_FLAG_ALIGN2;
	case REG_BINARY:
		return 0;
	case REG_DWORD:
		return LIBNDR_FLAG_ALIGN4;
	case REG_DWORD_BIG_ENDIAN:
		return LIBNDR_FLAG_ALIGN4;
	case REG_LINK:
		return 0;
	case REG_MULTI_SZ:
		return LIBNDR_FLAG_ALIGN2;
	case REG_RESOURCE_LIST:
		return LIBNDR_FLAG_ALIGN2;
	case REG_FULL_RESOURCE_DESCRIPTOR:
		return LIBNDR_FLAG_ALIGN4;
	case REG_RESOURCE_REQUIREMENTS_LIST:
		return LIBNDR_FLAG_ALIGN2;
	case REG_QWORD:
		return LIBNDR_FLAG_ALIGN8;
	}

	return 0;
}
