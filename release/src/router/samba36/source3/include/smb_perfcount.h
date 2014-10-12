/*
   Unix SMB/CIFS implementation.
   Portable SMB Messaging statistics interfaces
   Copyright (C) Todd Stecher (2008)

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

#ifndef _SMB_PERFCOUNT_H_
#define _SMB_PERFCOUNT_H_

#define SMB_PERFCOUNTER_INTERFACE_VERSION	1

struct smb_perfcount_data{
	struct smb_perfcount_handlers *handlers;
	void *context;
};

struct smb_perfcount_handlers {
	void (*perfcount_start) (struct smb_perfcount_data *pcd);
	void (*perfcount_add) (struct smb_perfcount_data *pcd);
	void (*perfcount_set_op) (struct smb_perfcount_data *pcd, int op);
	void (*perfcount_set_subop) (struct smb_perfcount_data *pcd, int subop);
	void (*perfcount_set_ioctl) (struct smb_perfcount_data *pcd, int io_ctl);
	void (*perfcount_set_msglen_in) (struct smb_perfcount_data *pcd,
				         uint64_t in_bytes);
	void (*perfcount_set_msglen_out) (struct smb_perfcount_data *pcd,
				          uint64_t out_bytes);
	void (*perfcount_copy_context) (struct smb_perfcount_data *pcd,
				        struct smb_perfcount_data *new_pcd);
	void (*perfcount_defer_op) (struct smb_perfcount_data *pcd,
				    struct smb_perfcount_data *def_pcd);
	void (*perfcount_end) (struct smb_perfcount_data *pcd);
};

bool smb_perfcount_init(void);

NTSTATUS smb_register_perfcounter(int interface_version, const char *name,
			          const struct smb_perfcount_handlers *handlers);

void smb_init_perfcount_data(struct smb_perfcount_data *pcd);

#define SMB_PERFCOUNT_START(_pcd_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_start((_pcd_)); \
    } while (0)

#define SMB_PERFCOUNT_ADD(_pcd_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_add((_pcd_)); \
    } while (0)

#define SMB_PERFCOUNT_SET_OP(_pcd_,_op_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_set_op((_pcd_), (_op_)); \
    } while (0)

#define SMB_PERFCOUNT_SET_SUBOP(_pcd_,_subop_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_set_subop((_pcd_), (_subop_)); \
    } while (0)

#define SMB_PERFCOUNT_SET_IOCTL(_pcd_,_subop_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_set_ioctl((_pcd_), (_subop_)); \
    } while (0)

#define SMB_PERFCOUNT_SET_MSGLEN_IN(_pcd_,_in_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_set_msglen_in((_pcd_), (_in_));\
    } while (0)

#define SMB_PERFCOUNT_SET_MSGLEN_OUT(_pcd_,_out_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_set_msglen_out((_pcd_), (_out_));\
    } while (0)

#define SMB_PERFCOUNT_COPY_CONTEXT(_pcd_, _new_pcd_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_copy_context((_pcd_), (_new_pcd_)); \
    } while (0)

#define SMB_PERFCOUNT_DEFER_OP(_pcd_, _def_pcd_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_defer_op((_pcd_), (_def_pcd_)); \
    } while (0)

#define SMB_PERFCOUNT_END(_pcd_) \
    do {if((_pcd_) && (_pcd_)->handlers) \
	    (_pcd_)->handlers->perfcount_end((_pcd_));\
    } while (0)

#endif /* _SMB_PERFCOUNT_H_ */
