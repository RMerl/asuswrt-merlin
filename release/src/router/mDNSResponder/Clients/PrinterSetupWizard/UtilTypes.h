/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 1997-2004 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):
    
$Log: UtilTypes.h,v $
Revision 1.18  2009/05/29 20:43:37  herscher
<rdar://problem/6928136> Printer Wizard doesn't work correctly in Windows 7 64 bit

Revision 1.17  2009/05/27 04:59:57  herscher
<rdar://problem/4517393> COMPATIBILITY WITH HP CLJ4700
<rdar://problem/6142138> Compatibility with Samsung print driver files

Revision 1.16  2007/04/20 22:58:10  herscher
<rdar://problem/4826126> mDNS: Printer Wizard doesn't offer generic HP printers or generic PS support on Vista RC2

Revision 1.15  2006/08/14 23:24:09  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.14  2005/06/30 18:02:54  shersche
<rdar://problem/4124524> Workaround for Mac OS X Printer Sharing bug

Revision 1.13  2005/04/13 17:46:22  shersche
<rdar://problem/4082122> Generic PCL not selected when printers advertise multiple text records

Revision 1.12  2005/03/16 03:12:28  shersche
<rdar://problem/4050504> Generic PCL driver isn't selected correctly on Win2K

Revision 1.11  2005/03/05 02:27:46  shersche
<rdar://problem/4030388> Generic drivers don't do color

Revision 1.10  2005/02/08 21:45:06  shersche
<rdar://problem/3947490> Default to Generic PostScript or PCL if unable to match driver

Revision 1.9  2005/02/01 01:16:12  shersche
Change window owner from CSecondPage to CPrinterSetupWizardSheet

Revision 1.8  2005/01/06 08:18:26  shersche
Add protocol field to service, add EmptyQueues() function to service

Revision 1.7  2005/01/04 21:07:29  shersche
add description member to service object.  this member corresponds to the 'ty' key in a printer text record

Revision 1.6  2004/12/30 01:24:02  shersche
<rdar://problem/3906182> Remove references to description key
Bug #: 3906182

Revision 1.5  2004/12/29 18:53:38  shersche
<rdar://problem/3725106>
<rdar://problem/3737413> Added support for LPR and IPP protocols as well as support for obtaining multiple text records. Reorganized and simplified codebase.
Bug #: 3725106, 3737413

Revision 1.4  2004/09/13 21:22:44  shersche
<rdar://problem/3796483> Add moreComing argument to OnAddPrinter and OnRemovePrinter callbacks
Bug #: 3796483

Revision 1.3  2004/06/26 23:27:12  shersche
support for installing multiple printers of the same name

Revision 1.2  2004/06/25 02:25:59  shersche
Remove item field from manufacturer and model structures
Submitted by: herscher

Revision 1.1  2004/06/18 04:36:58  rpantos
First checked in


*/

#pragma once

#include <dns_sd.h>
#include <string>
#include <list>
#include <DebugServices.h>

class CPrinterSetupWizardSheet;

#define	kDefaultPriority	50
#define kDefaultQTotal		1

namespace PrinterSetupWizard
{
	struct Printer;
	struct Service;
	struct Queue;
	struct Manufacturer;
	struct Model;

	typedef std::list<Queue*>	Queues;
	typedef std::list<Printer*>	Printers;
	typedef std::list<Service*>	Services;
	typedef std::list<Model*>	Models;

	struct Printer
	{
		Printer();

		~Printer();

		Service*
		LookupService
			(
			const std::string	&	type
			);

		CPrinterSetupWizardSheet	*	window;
		HTREEITEM		item;

		//
		// These are from the browse reply
		//
		std::string		name;
		CString			displayName;
		CString			actualName;

		//
		// These keep track of the different services associated with this printer.
		// the services are ordered according to preference.
		//
		Services		services;

		//
		// these are derived from the printer matching code
		//
		// if driverInstalled is false, then infFileName should
		// have an absolute path to the printers inf file.  this
		// is used to install the printer from printui.dll
		//
		// if driverInstalled is true, then model is the name
		// of the driver to use in AddPrinter
		// 
		bool			driverInstalled;
		CString			infFileName;
		CString			manufacturer;
		CString			displayModelName;
		CString			modelName;
		CString			portName;
		bool			deflt;

		// This let's us know that this printer was discovered via OSX Printer Sharing.
		// We use this knowledge to workaround a problem with OS X Printer sharing.

		bool			isSharedFromOSX;
		
		//
		// state
		//
		unsigned		resolving;
		bool			installed;
	};


	struct Service
	{
		Service();

		~Service();

		Queue*
		SelectedQueue();

		void
		EmptyQueues();

		Printer		*	printer;
		uint32_t		ifi;
		std::string		type;
		std::string		domain;

		//
		// these are from the resolve
		//
		DNSServiceRef	serviceRef;
		CString			hostname;
		unsigned short	portNumber;
		CString			protocol;
		unsigned short	qtotal;

		//
		// There will usually one be one of these, however
		// this will handle printers that have multiple
		// queues.  These are ordered according to preference.
		//
		Queues			queues;

		//
		// Reference count
		//
		unsigned		refs;
	};


	struct Queue
	{
		Queue();

		~Queue();

		CString		name;
		uint32_t	priority;
		CString		pdl;
		CString		usb_MFG;
		CString		usb_MDL;
		CString		description;
		CString		location;
		CString		product;
	};


	struct Manufacturer
	{
		CString		name;
		Models		models;

		Model*
		find( const CString & name );
	};


	struct Model
	{
		bool		driverInstalled;
		CString		infFileName;
		CString		displayName;
		CString		name;
	};


	inline
	Printer::Printer()
	:
		isSharedFromOSX( false )
	{
	}

	inline
	Printer::~Printer()
	{
		while ( services.size() > 0 )
		{
			Service * service = services.front();
			services.pop_front();
			delete service;
		}
	}

	inline Service*
	Printer::LookupService
				(
				const std::string	&	type
				)
	{
		Services::iterator it;

		for ( it = services.begin(); it != services.end(); it++ )
		{
			Service * service = *it;

			if ( strcmp(service->type.c_str(), type.c_str()) == 0 )
			{
				return service;
			}
		}

		return NULL;
	}

	inline
	Service::Service()
	:
		qtotal(kDefaultQTotal)
	{
	}

	inline
	Service::~Service()
	{
		check( serviceRef == NULL );

		EmptyQueues();
	}

	inline Queue*
	Service::SelectedQueue()
	{
		return queues.front();
	}

	inline void
	Service::EmptyQueues()
	{
		while ( queues.size() > 0 )
		{
			Queue * q = queues.front();
			queues.pop_front();
			delete q;
		}
	}

	inline
	Queue::Queue()
	:
		priority(kDefaultPriority)
	{
	}

	inline
	Queue::~Queue()
	{
	}

	inline Model*
	Manufacturer::find( const CString & name )
	{
		Models::iterator it;

		for ( it = models.begin(); it != models.end(); it++ )
		{
			Model * model = *it;

			if ( model->name == name )
			{
				return model;
			}
		}

		return NULL;
	}
}


