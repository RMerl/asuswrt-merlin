/* 
   Unix SMB/CIFS implementation.
   Utility functions for Samba
   Copyright (C) Jelmer Vernooij 2008
    
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

#ifndef __SAMBA_COM_H__
#define __SAMBA_COM_H__

#include "librpc/gen_ndr/misc.h"

struct com_context;
struct tevent_context;

struct com_context 
{
	struct dcom_client_context *dcom;
	struct tevent_context *event_ctx;
        struct com_extension {
                uint32_t id;
                void *data;
                struct com_extension *prev, *next;
        } *extensions;
	struct loadparm_context *lp_ctx;
};

struct IUnknown *com_class_by_clsid(struct com_context *ctx, const struct GUID *clsid);
NTSTATUS com_register_running_class(struct GUID *clsid, const char *progid, struct IUnknown *p);

struct dcom_interface_p *dcom_get_local_iface_p(struct GUID *ipid);

WERROR com_init_ctx(struct com_context **ctx, struct tevent_context *event_ctx);
WERROR com_create_object(struct com_context *ctx, struct GUID *clsid, int num_ifaces, struct GUID *iid, struct IUnknown **ip, WERROR *results);
WERROR com_get_class_object(struct com_context *ctx, struct GUID *clsid, struct GUID *iid, struct IUnknown **ip);
NTSTATUS com_init(void);

typedef struct IUnknown *(*get_class_object_function) (const struct GUID *clsid);

#endif /* __SAMBA_COM_H__ */
