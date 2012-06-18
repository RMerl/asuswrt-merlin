/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher 2008

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
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "auth/credentials/credentials.h"
#include "param/secrets.h"
#include "param/param.h"

NTSTATUS wbsrv_setup_domains(struct wbsrv_service *service)
{
	const struct dom_sid *primary_sid;

	primary_sid = secrets_get_domain_sid(service,
					     service->task->event_ctx,
					     service->task->lp_ctx,
					     lp_workgroup(service->task->lp_ctx));
	if (!primary_sid) {
		return NT_STATUS_CANT_ACCESS_DOMAIN_INFO;
	}

	service->primary_sid = primary_sid;

	return NT_STATUS_OK;
}
