/* 
   Unix SMB/CIFS implementation.

   NTPTR structures and defines

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

/* modules can use the following to determine if the interface has changed */
#define NTPTR_INTERFACE_VERSION 0

struct ntptr_context;

enum ntptr_HandleType {
	NTPTR_HANDLE_SERVER,
	NTPTR_HANDLE_PRINTER,
	NTPTR_HANDLE_PORT,
	NTPTR_HANDLE_MONITOR
};

struct ntptr_GenericHandle {
	enum ntptr_HandleType type;
	struct ntptr_context *ntptr;
	const char *object_name;
	uint32_t access_mask;
	void *private_data;
};

struct spoolss_OpenPrinterEx;
struct spoolss_EnumPrinterData;
struct spoolss_DeletePrinterData;
struct spoolss_AddForm;
struct spoolss_GetForm;
struct spoolss_SetForm;
struct spoolss_DeleteForm;
struct spoolss_AddPrinterDriver;
struct spoolss_DeletePrinterDriver;
struct spoolss_GetPrinterDriverDirectory;
struct spoolss_AddPrinter;
struct spoolss_GetPrinter;
struct spoolss_SetPrinter;
struct spoolss_DeletePrinter;
struct spoolss_GetPrinterDriver;
struct spoolss_AddJob;
struct spoolss_EnumJobs;
struct spoolss_SetJob;
struct spoolss_GetJob;
struct spoolss_ScheduleJob;
struct spoolss_ReadPrinter;
struct spoolss_WritePrinter;
struct spoolss_StartDocPrinter;
struct spoolss_EndDocPrinter;
struct spoolss_StartPagePrinter;
struct spoolss_EndPagePrinter;
struct spoolss_GetPrinterData;
struct spoolss_SetPrinterData;
struct spoolss_EnumPrinterDrivers;
struct spoolss_EnumMonitors;
struct spoolss_EnumPrinters;
struct spoolss_EnumForms;
struct spoolss_EnumPorts;
struct spoolss_EnumPrintProcessors;
struct spoolss_XcvData;
struct spoolss_GetPrintProcessorDirectory;

/* the ntptr operations structure - contains function pointers to 
   the backend implementations of each operation */
struct ntptr_ops {
	const char *name;

	/* initial setup */
	NTSTATUS (*init_context)(struct ntptr_context *ntptr);

	/* PrintServer functions */
	WERROR (*OpenPrintServer)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				  struct spoolss_OpenPrinterEx *r,
				  const char *printer_name,
				  struct ntptr_GenericHandle **server);
	WERROR (*XcvDataPrintServer)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_XcvData *r);

	/* PrintServer PrinterData functions */
	WERROR (*EnumPrintServerData)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_EnumPrinterData *r);
	WERROR (*GetPrintServerData)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_GetPrinterData *r);
	WERROR (*SetPrintServerData)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_SetPrinterData *r);
	WERROR (*DeletePrintServerData)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
					struct spoolss_DeletePrinterData *r);

	/* PrintServer Form functions */
	WERROR (*EnumPrintServerForms)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				       struct spoolss_EnumForms *r);
	WERROR (*AddPrintServerForm)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_AddForm *r);
	WERROR (*SetPrintServerForm)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
				     struct spoolss_SetForm *r);
	WERROR (*DeletePrintServerForm)(struct ntptr_GenericHandle *server, TALLOC_CTX *mem_ctx,
					struct spoolss_DeleteForm *r);

	/* PrintServer Driver functions */
	WERROR (*EnumPrinterDrivers)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				     struct spoolss_EnumPrinterDrivers *r);
	WERROR (*AddPrinterDriver)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				   struct spoolss_AddPrinterDriver *r);
	WERROR (*DeletePrinterDriver)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				      struct spoolss_DeletePrinterDriver *r);
	WERROR (*GetPrinterDriverDirectory)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
					    struct spoolss_GetPrinterDriverDirectory *r);

	/* Port functions */
	WERROR (*EnumPorts)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			    struct spoolss_EnumPorts *r);
	WERROR (*OpenPort)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			   struct spoolss_OpenPrinterEx *r,
			   const char *port_name,
			   struct ntptr_GenericHandle **port);
	WERROR (*XcvDataPort)(struct ntptr_GenericHandle *port, TALLOC_CTX *mem_ctx,
			      struct spoolss_XcvData *r);
	
	/* Monitor functions */
	WERROR (*EnumMonitors)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			       struct spoolss_EnumMonitors *r);
	WERROR (*OpenMonitor)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			      struct spoolss_OpenPrinterEx *r,
			      const char *monitor_name,
			      struct ntptr_GenericHandle **monitor);
	WERROR (*XcvDataMonitor)(struct ntptr_GenericHandle *monitor, TALLOC_CTX *mem_ctx,
				 struct spoolss_XcvData *r);

	/* PrintProcessor functions */
	WERROR (*EnumPrintProcessors)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				      struct spoolss_EnumPrintProcessors *r);
	WERROR (*GetPrintProcessorDirectory)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
					     struct spoolss_GetPrintProcessorDirectory *r);

	/* Printer functions */
	WERROR (*EnumPrinters)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			       struct spoolss_EnumPrinters *r);
	WERROR (*OpenPrinter)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			      struct spoolss_OpenPrinterEx *r,
			      const char *printer_name,
			      struct ntptr_GenericHandle **printer);
	WERROR (*AddPrinter)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			     struct spoolss_AddPrinter *r,
			     struct ntptr_GenericHandle **printer);
	WERROR (*GetPrinter)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			     struct spoolss_GetPrinter *r);
	WERROR (*SetPrinter)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
			     struct spoolss_SetPrinter *r);
	WERROR (*DeletePrinter)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				struct spoolss_DeletePrinter *r);
	WERROR (*XcvDataPrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_XcvData *r);

	/* Printer Driver functions */
	WERROR (*GetPrinterDriver)(struct ntptr_context *ntptr, TALLOC_CTX *mem_ctx,
				   struct spoolss_GetPrinterDriver *r);

	/* Printer PrinterData functions */
	WERROR (*EnumPrinterData)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				  struct spoolss_EnumPrinterData *r);
	WERROR (*GetPrinterData)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_GetPrinterData *r);
	WERROR (*SetPrinterData)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_SetPrinterData *r);
	WERROR (*DeletePrinterData)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				    struct spoolss_DeletePrinterData *r);

	/* Printer Form functions */
	WERROR (*EnumPrinterForms)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				   struct spoolss_EnumForms *r);
	WERROR (*AddPrinterForm)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_AddForm *r);
	WERROR (*GetPrinterForm)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_GetForm *r);
	WERROR (*SetPrinterForm)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_SetForm *r);
	WERROR (*DeletePrinterForm)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				    struct spoolss_DeleteForm *r);

	/* Printer Job functions */
	WERROR (*EnumJobs)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			   struct spoolss_EnumJobs *r);
	WERROR (*AddJob)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			 struct spoolss_AddJob *r);
	WERROR (*ScheduleJob)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			      struct spoolss_ScheduleJob *r);
	WERROR (*GetJob)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			 struct spoolss_GetJob *r);
	WERROR (*SetJob)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			 struct spoolss_SetJob *r);

	/* Printer Printing functions */
	WERROR (*StartDocPrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				  struct spoolss_StartDocPrinter *r);
	WERROR (*EndDocPrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				struct spoolss_EndDocPrinter *r);
	WERROR (*StartPagePrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				   struct spoolss_StartPagePrinter *r);
	WERROR (*EndPagePrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
				 struct spoolss_EndPagePrinter *r);
	WERROR (*WritePrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			       struct spoolss_WritePrinter *r);
	WERROR (*ReadPrinter)(struct ntptr_GenericHandle *printer, TALLOC_CTX *mem_ctx,
			      struct spoolss_ReadPrinter *r);
};

struct ntptr_context {
	const struct ntptr_ops *ops;
	void *private_data;
	struct tevent_context *ev_ctx;
	struct loadparm_context *lp_ctx;
};

/* this structure is used by backends to determine the size of some critical types */
struct ntptr_critical_sizes {
	int interface_version;
	int sizeof_ntptr_critical_sizes;
	int sizeof_ntptr_context;
	int sizeof_ntptr_ops;
};

struct loadparm_context;

#include "ntptr/ntptr_proto.h"
