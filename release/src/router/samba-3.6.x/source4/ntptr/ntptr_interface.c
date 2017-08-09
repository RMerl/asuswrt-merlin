/* 
   Unix SMB/CIFS implementation.

   NTPTR interface functions

   Copyright (C) Stefan (metze) Metzmacher 2005

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
#include "ntptr/ntptr.h"


/* PrintServer functions */
WERROR ntptr_OpenPrintServer(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			     struct spoolss_OpenPrinterEx *r,
			     const char *printer_name,
			     struct ntptr_GenericHandle **server)
{
	if (!ntptr->ops->OpenPrintServer) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->OpenPrintServer(ntptr, mem_ctx, r, printer_name, server);
}

WERROR ntptr_XcvDataPrintServer(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				struct spoolss_XcvData *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->XcvDataPrintServer) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->XcvDataPrintServer(server, mem_ctx, r);
}


/* PrintServer PrinterData functions */
WERROR ntptr_EnumPrintServerData(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				 struct spoolss_EnumPrinterData *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->EnumPrintServerData) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->EnumPrintServerData(server, mem_ctx, r);
}

WERROR ntptr_GetPrintServerData(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				struct spoolss_GetPrinterData *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->GetPrintServerData) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->GetPrintServerData(server, mem_ctx, r);
}

WERROR ntptr_SetPrintServerData(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				struct spoolss_SetPrinterData *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->SetPrintServerData) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->SetPrintServerData(server, mem_ctx, r);
}

WERROR ntptr_DeletePrintServerData(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				   struct spoolss_DeletePrinterData *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->DeletePrintServerData) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->DeletePrintServerData(server, mem_ctx, r);
}


/* PrintServer Form functions */
WERROR ntptr_EnumPrintServerForms(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				  struct spoolss_EnumForms *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->EnumPrintServerForms) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->EnumPrintServerForms(server, mem_ctx, r);
}

WERROR ntptr_AddPrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				struct spoolss_AddForm *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->AddPrintServerForm) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->AddPrintServerForm(server, mem_ctx, r);
}

WERROR ntptr_SetPrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				struct spoolss_SetForm *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->SetPrintServerForm) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->SetPrintServerForm(server, mem_ctx, r);
}

WERROR ntptr_DeletePrintServerForm(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				   struct spoolss_DeleteForm *r)
{
	if (server->type != NTPTR_HANDLE_SERVER) {
		return WERR_FOOBAR;
	}
	if (!server->ntptr->ops->DeletePrintServerForm) {
		return WERR_NOT_SUPPORTED;
	}
	return server->ntptr->ops->DeletePrintServerForm(server, mem_ctx, r);
}


/* PrintServer Driver functions */
WERROR ntptr_EnumPrinterDrivers(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				struct spoolss_EnumPrinterDrivers *r)
{
	if (!ntptr->ops->EnumPrinterDrivers) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->EnumPrinterDrivers(ntptr, mem_ctx, r);
}

WERROR ntptr_AddPrinterDriver(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			      struct spoolss_AddPrinterDriver *r)
{
	if (!ntptr->ops->AddPrinterDriver) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->AddPrinterDriver(ntptr, mem_ctx, r);
}

WERROR ntptr_DeletePrinterDriver(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				 struct spoolss_DeletePrinterDriver *r)
{
	if (!ntptr->ops->DeletePrinterDriver) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->DeletePrinterDriver(ntptr, mem_ctx, r);
}

WERROR ntptr_GetPrinterDriverDirectory(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				 struct spoolss_GetPrinterDriverDirectory *r)
{
	if (!ntptr->ops->GetPrinterDriverDirectory) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->GetPrinterDriverDirectory(ntptr, mem_ctx, r);
}


/* Port functions */
WERROR ntptr_EnumPorts(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
		       struct spoolss_EnumPorts *r)
{
	if (!ntptr->ops->EnumPorts) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->EnumPorts(ntptr, mem_ctx, r);
}

WERROR ntptr_OpenPort(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
		      struct spoolss_OpenPrinterEx *r,
		      const char *port_name,
		      struct ntptr_GenericHandle **port)
{
	if (!ntptr->ops->OpenPort) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->OpenPort(ntptr, mem_ctx, r, port_name, port);
}

WERROR ntptr_XcvDataPort(struct ntptr_GenericHandle *port, TALLOC_CTX *mem_ctx,
			 struct spoolss_XcvData *r)
{
	if (port->type != NTPTR_HANDLE_PORT) {
		return WERR_FOOBAR;
	}
	if (!port->ntptr->ops->XcvDataPort) {
		return WERR_NOT_SUPPORTED;
	}
	return port->ntptr->ops->XcvDataPort(port, mem_ctx, r);
}

/* Monitor functions */
WERROR ntptr_EnumMonitors(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			  struct spoolss_EnumMonitors *r)
{
	if (!ntptr->ops->EnumMonitors) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->EnumMonitors(ntptr, mem_ctx, r);
}

WERROR ntptr_OpenMonitor(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			 struct spoolss_OpenPrinterEx *r,
			 const char *monitor_name,
			 struct ntptr_GenericHandle **monitor)
{
	if (!ntptr->ops->OpenMonitor) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->OpenMonitor(ntptr, mem_ctx, r, monitor_name, monitor);
}

WERROR ntptr_XcvDataMonitor(struct ntptr_GenericHandle *monitor, TALLOC_CTX *mem_ctx,
			    struct spoolss_XcvData *r)
{
	if (monitor->type != NTPTR_HANDLE_MONITOR) {
		return WERR_FOOBAR;
	}
	if (!monitor->ntptr->ops->XcvDataMonitor) {
		return WERR_NOT_SUPPORTED;
	}
	return monitor->ntptr->ops->XcvDataMonitor(monitor, mem_ctx, r);
}


/* PrintProcessor functions */
WERROR ntptr_EnumPrintProcessors(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				 struct spoolss_EnumPrintProcessors *r)
{
	if (!ntptr->ops->EnumPrintProcessors) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->EnumPrintProcessors(ntptr, mem_ctx, r);
}

WERROR ntptr_GetPrintProcessorDirectory(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
					struct spoolss_GetPrintProcessorDirectory *r)
{
	if (!ntptr->ops->GetPrintProcessorDirectory) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->GetPrintProcessorDirectory(ntptr, mem_ctx, r);
}


/* Printer functions */
WERROR ntptr_EnumPrinters(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			  struct spoolss_EnumPrinters *r)
{
	if (!ntptr->ops->EnumPrinters) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->EnumPrinters(ntptr, mem_ctx, r);
}

WERROR ntptr_OpenPrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			 struct spoolss_OpenPrinterEx *r,
			 const char *printer_name,
			 struct ntptr_GenericHandle **printer)
{
	if (!ntptr->ops->OpenPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->OpenPrinter(ntptr, mem_ctx, r, printer_name, printer);
}

WERROR ntptr_AddPrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			struct spoolss_AddPrinter *r,
			struct ntptr_GenericHandle **printer)
{
	if (!ntptr->ops->AddPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->AddPrinter(ntptr, mem_ctx, r, printer);
}

WERROR ntptr_GetPrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			struct spoolss_GetPrinter *r)
{
	if (!ntptr->ops->GetPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->GetPrinter(ntptr, mem_ctx, r);
}

WERROR ntptr_SetPrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			struct spoolss_SetPrinter *r)
{
	if (!ntptr->ops->SetPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->SetPrinter(ntptr, mem_ctx, r);
}

WERROR ntptr_DeletePrinter(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			   struct spoolss_DeletePrinter *r)
{
	if (!ntptr->ops->DeletePrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->DeletePrinter(ntptr, mem_ctx, r);
}

WERROR ntptr_XcvDataPrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_XcvData *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->XcvDataPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->XcvDataPrinter(printer, mem_ctx, r);
}


/* Printer Driver functions */
WERROR ntptr_GetPrinterDriver(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			      struct spoolss_GetPrinterDriver *r)
{
	if (!ntptr->ops->GetPrinterDriver) {
		return WERR_NOT_SUPPORTED;
	}
	return ntptr->ops->GetPrinterDriver(ntptr, mem_ctx, r);
}


/* Printer PrinterData functions */
WERROR ntptr_EnumPrinterData(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			     struct spoolss_EnumPrinterData *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->EnumPrinterData) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->EnumPrinterData(printer, mem_ctx, r);
}

WERROR ntptr_GetPrinterData(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_GetPrinterData *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->GetPrinterData) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->GetPrinterData(printer, mem_ctx, r);
}

WERROR ntptr_SetPrinterData(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_SetPrinterData *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->SetPrinterData) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->SetPrinterData(printer, mem_ctx, r);
}

WERROR ntptr_DeletePrinterData(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			       struct spoolss_DeletePrinterData *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->DeletePrinterData) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->DeletePrinterData(printer, mem_ctx, r);
}


/* Printer Form functions */
WERROR ntptr_EnumPrinterForms(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			      struct spoolss_EnumForms *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->EnumPrinterForms) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->EnumPrinterForms(printer, mem_ctx, r);
}

WERROR ntptr_AddPrinterForm(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_AddForm *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->AddPrinterForm) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->AddPrinterForm(printer, mem_ctx, r);
}

WERROR ntptr_GetPrinterForm(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_GetForm *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->GetPrinterForm) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->GetPrinterForm(printer, mem_ctx, r);
}

WERROR ntptr_SetPrinterForm(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_SetForm *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->SetPrinterForm) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->SetPrinterForm(printer, mem_ctx, r);
}

WERROR ntptr_DeletePrinterForm(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			       struct spoolss_DeleteForm *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->DeletePrinterForm) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->DeletePrinterForm(printer, mem_ctx, r);
}


/* Printer Job functions */
WERROR ntptr_EnumJobs(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
		      struct spoolss_EnumJobs *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->EnumJobs) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->EnumJobs(printer, mem_ctx, r);
}

WERROR ntptr_AddJob(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
		    struct spoolss_AddJob *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->AddJob) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->AddJob(printer, mem_ctx, r);
}

WERROR ntptr_ScheduleJob(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			 struct spoolss_ScheduleJob *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->ScheduleJob) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->ScheduleJob(printer, mem_ctx, r);
}

WERROR ntptr_GetJob(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
		    struct spoolss_GetJob *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->GetJob) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->GetJob(printer, mem_ctx, r);
}

WERROR ntptr_SetJob(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
		    struct spoolss_SetJob *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->SetJob) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->SetJob(printer, mem_ctx, r);
}


/* Printer Printing functions */
WERROR ntptr_StartDocPrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			     struct spoolss_StartDocPrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->StartDocPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->StartDocPrinter(printer, mem_ctx, r);
}

WERROR ntptr_EndDocPrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			   struct spoolss_EndDocPrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->EndDocPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->EndDocPrinter(printer, mem_ctx, r);
}

WERROR ntptr_StartPagePrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			      struct spoolss_StartPagePrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->StartPagePrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->StartPagePrinter(printer, mem_ctx, r);
}

WERROR ntptr_EndPagePrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			    struct spoolss_EndPagePrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->EndPagePrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->EndPagePrinter(printer, mem_ctx, r);
}

WERROR ntptr_WritePrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			  struct spoolss_WritePrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->WritePrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->WritePrinter(printer, mem_ctx, r);
}

WERROR ntptr_ReadPrinter(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			 struct spoolss_ReadPrinter *r)
{
	if (printer->type != NTPTR_HANDLE_PRINTER) {
		return WERR_FOOBAR;
	}
	if (!printer->ntptr->ops->ReadPrinter) {
		return WERR_NOT_SUPPORTED;
	}
	return printer->ntptr->ops->ReadPrinter(printer, mem_ctx, r);
}

