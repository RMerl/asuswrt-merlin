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
    
$Log: ThirdPage.cpp,v $
Revision 1.42  2009/07/07 22:04:55  herscher
<rdar://problem/4176343> LOC Impact: Need custom text when selecting the wrong printer driver

Revision 1.41  2009/06/18 18:05:50  herscher
<rdar://problem/4694554> Eliminate the first screen of Printer Wizard and maybe combine others ("I'm Feeling Lucky")

Revision 1.40  2009/05/29 20:43:36  herscher
<rdar://problem/6928136> Printer Wizard doesn't work correctly in Windows 7 64 bit

Revision 1.39  2009/05/27 06:25:49  herscher
<rdar://problem/4176334> Need error dialog when selecting bad INF file

Revision 1.38  2009/05/27 04:59:57  herscher
<rdar://problem/4517393> COMPATIBILITY WITH HP CLJ4700
<rdar://problem/6142138> Compatibility with Samsung print driver files

Revision 1.37  2007/06/08 06:30:26  herscher
<rdar://problem/5257700> Fix uninitialized pointers when detecting generic PCL and PS drivers

Revision 1.36  2007/06/06 20:39:10  cheshire
<rdar://problem/5254377> Printer Setup Wizard started crashing in Bonjour104A8, after update to Visual Studio 2005

Revision 1.35  2007/06/06 20:08:01  cheshire
<rdar://problem/4528853> mDNS: When auto-highlighting items in lists, scroll list so highlighted item is in the middle
AutoScroll model list as well as manufacturer list

Revision 1.34  2007/06/06 19:53:48  cheshire
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.33  2007/04/20 22:58:10  herscher
<rdar://problem/4826126> mDNS: Printer Wizard doesn't offer generic HP printers or generic PS support on Vista RC2

Revision 1.32  2007/04/13 23:42:20  herscher
<rdar://problem/4580061> mDNS: Printers added using Bonjour should be set as the default printer.

Revision 1.31  2007/04/13 21:38:46  herscher
<rdar://problem/4528853> mDNS: When auto-highlighting items in lists, scroll list so highlighted item is in the middle

Revision 1.30  2007/04/13 20:23:40  herscher
Fixed mistake in previous checkin that reverted license text for this file

Revision 1.29  2007/04/13 18:10:24  herscher
<rdar://problem/4496652> mDNS: Don't allow user to choose non-working driver

Revision 1.28  2006/08/14 23:24:09  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.27  2005/10/05 21:41:45  herscher
<rdar://problem/4190104> Use "application/octet-stream" to determine if CUPS shared queue supports raw

Revision 1.26  2005/07/11 20:17:15  shersche
<rdar://problem/4124524> UI fixes associated with CUPS printer workaround fix.

Revision 1.25  2005/07/07 17:53:20  shersche
Fix problems associated with the CUPS printer workaround fix.

Revision 1.24  2005/06/30 18:02:54  shersche
<rdar://problem/4124524> Workaround for Mac OS X Printer Sharing bug

Revision 1.23  2005/04/18 02:33:47  shersche
<rdar://problem/4091216> Default printer option cannot be deselected

Revision 1.22  2005/04/13 17:46:22  shersche
<rdar://problem/4082122> Generic PCL not selected when printers advertise multiple text records

Revision 1.21  2005/03/30 02:09:55  shersche
Auto-resize the column width to account for differing fonts and font sizes

Revision 1.20  2005/03/05 02:27:45  shersche
<rdar://problem/4030388> Generic drivers don't do color

Revision 1.19  2005/02/23 02:08:51  shersche
<rdar://problem/4012275> If we can't match the manufacturer, and select a generic printer, then show all the manufacturers in the manufacturer pane, not just "Generic".

Revision 1.18  2005/02/15 07:02:51  shersche
<rdar://problem/4003724> Display different UI text when generic printer drivers are selected

Revision 1.17  2005/02/08 21:45:06  shersche
<rdar://problem/3947490> Default to Generic PostScript or PCL if unable to match driver

Revision 1.16  2005/02/08 18:56:03  shersche
Fix generated IPP url so that it doesn't add "/printers" string

Revision 1.15  2005/02/01 01:44:07  shersche
Load ntprint.inf at startup.  This will cause the wizard to take a second or two longer to come up, but will eliminate the pause when auto-selecting the print drivers.

Revision 1.14  2005/01/25 08:55:54  shersche
<rdar://problem/3911084> Load icons at run-time from resource DLL
Bug #: 3911084

Revision 1.13  2005/01/06 08:15:45  shersche
Append queue name to end of LPR port name, correctly build port name when queue name is absent

Revision 1.12  2005/01/05 01:06:12  shersche
<rdar://problem/3841218> Strip the first substring off the product key if an initial match can't be found with the whole product key.
Bug #: 3841218

Revision 1.11  2004/12/29 18:53:38  shersche
<rdar://problem/3725106>
<rdar://problem/3737413> Added support for LPR and IPP protocols as well as support for obtaining multiple text records. Reorganized and simplified codebase.
Bug #: 3725106, 3737413

Revision 1.10  2004/10/11 22:55:34  shersche
<rdar://problem/3827624> Use the IP port number when deriving the printer port name.
Bug #: 3827624

Revision 1.9  2004/06/27 23:08:00  shersche
code cleanup, make sure EnumPrintDrivers returns non-zero value, ignore comments in inf files

Revision 1.8  2004/06/27 08:06:45  shersche
Parse [Strings] section of inf file

Revision 1.7  2004/06/26 04:00:05  shersche
fix warnings compiling in debug mode
Submitted by: herscher

Revision 1.6  2004/06/26 03:19:57  shersche
clean up warning messages

Submitted by: herscher

Revision 1.5  2004/06/25 05:06:02  shersche
Trim whitespace from key/value pairs when parsing inf files
Submitted by: herscher

Revision 1.4  2004/06/25 02:44:13  shersche
Tweaked code to handle Xerox Phaser printer identification
Submitted by: herscher

Revision 1.3  2004/06/25 02:27:58  shersche
Do a CListCtrl::FindItem() before calling CListCtrl::SetItemState().
Submitted by: herscher

Revision 1.2  2004/06/23 18:09:23  shersche
Normalize tag names when parsing inf files.
Submitted by: herscher

Revision 1.1  2004/06/18 04:36:58  rpantos
First checked in
*/

#include "stdafx.h"
#include "PrinterSetupWizardApp.h"
#include "PrinterSetupWizardSheet.h"
#include "ThirdPage.h"
#include <dns_sd.h>
#include <tcpxcv.h>
#include <winspool.h>
#include <setupapi.h>

// local variable is initialize but not referenced
#pragma warning(disable:4189)

//
// This is the printer description file that is shipped
// with Windows XP and below
//
#define kNTPrintFile		L"inf\\ntprint.inf"

//
// Windows Vista ships with a set of prn*.inf files
//
#define kVistaPrintFiles	L"inf\\prn*.inf"

//
// These are pre-defined names for Generic manufacturer and model
//
#define kGenericManufacturer		L"Generic"
#define kGenericText				L"Generic / Text Only"
#define kGenericPostscript			L"Generic / Postscript"
#define kGenericPCL					L"Generic / PCL"
#define kPDLPostscriptKey			L"application/postscript"
#define kPDLPCLKey					L"application/vnd.hp-pcl"
#define kGenericPSColorDriver		L"HP Color LaserJet 4550 PS"
#define kGenericPSDriver			L"HP LaserJet 4050 Series PS"
#define kGenericPCLColorDriver		L"HP Color LaserJet 4550 PCL"
#define kGenericPCLDriver			L"HP LaserJet 4050 Series PCL"


// CThirdPage dialog

IMPLEMENT_DYNAMIC(CThirdPage, CPropertyPage)
CThirdPage::CThirdPage()
	: CPropertyPage(CThirdPage::IDD),
		m_manufacturerSelected( NULL ),
		m_modelSelected( NULL ),
		m_genericPostscript( NULL ),
		m_genericPCL( NULL ),
		m_initialized(false),
		m_printerImage( NULL )
{
	static const int	bufferSize	= 32768;
	TCHAR				windowsDirectory[bufferSize];
	CString				header;
	WIN32_FIND_DATA		findFileData;
	HANDLE				findHandle;
	CString				prnFiles;
	CString				ntPrint;
	OSStatus			err;
	BOOL				ok;

	m_psp.dwFlags &= ~(PSP_HASHELP);
	m_psp.dwFlags |= PSP_DEFAULT|PSP_USEHEADERTITLE|PSP_USEHEADERSUBTITLE;
	
	m_psp.pszHeaderTitle = MAKEINTRESOURCE(IDS_INSTALL_TITLE);
	m_psp.pszHeaderSubTitle = MAKEINTRESOURCE(IDS_INSTALL_SUBTITLE);

	//
	// load printers from ntprint.inf
	//
	ok = GetWindowsDirectory( windowsDirectory, bufferSize );
	err = translate_errno( ok, errno_compat(), kUnknownErr );
	require_noerr( err, exit );
 
	//
	// <rdar://problem/4826126>
	//
	// If there are no *prn.inf files, we'll assume that the information
	// is in ntprint.inf
	//
	prnFiles.Format( L"%s\\%s", windowsDirectory, kVistaPrintFiles );
	findHandle = FindFirstFile( prnFiles, &findFileData );
 
	if ( findHandle != INVALID_HANDLE_VALUE )
	{
		CString absolute;

		absolute.Format( L"%s\\inf\\%s", windowsDirectory, findFileData.cFileName );
		err = LoadPrintDriverDefsFromFile( m_manufacturers, absolute, false );
		require_noerr( err, exit );

		while ( FindNextFile( findHandle, &findFileData ) )
		{
			absolute.Format( L"%s\\inf\\%s", windowsDirectory, findFileData.cFileName );
			err = LoadPrintDriverDefsFromFile( m_manufacturers, absolute, false );
			require_noerr( err, exit );
		}

		FindClose( findHandle );
	}
	else
	{
		ntPrint.Format(L"%s\\%s", windowsDirectory, kNTPrintFile);
		err = LoadPrintDriverDefsFromFile( m_manufacturers, ntPrint, false );
		require_noerr(err, exit);
	}

	//
	// load printer drivers that have been installed on this machine
	//
	err = LoadPrintDriverDefs( m_manufacturers );
	require_noerr(err, exit);

	//
	// load our own special generic printer defs
	//
	err = LoadGenericPrintDriverDefs( m_manufacturers );
	require_noerr( err, exit );

exit:

	return;
}

CThirdPage::~CThirdPage()
{
	//
	// clean up all the printer manufacturers
	//
	while (m_manufacturers.size())
	{
		Manufacturers::iterator iter = m_manufacturers.begin();

		while (iter->second->models.size())
		{
			Models::iterator it = iter->second->models.begin();

			Model * model = *it;

			delete model;

			iter->second->models.erase(it);
		}

		delete iter->second;

		m_manufacturers.erase(iter);
	}
}

// ----------------------------------------------------
// SelectMatch
//
// SelectMatch will do all the UI work associated with
// selected a manufacturer and model of printer.  It also
// makes sure the printer object is update with the
// latest settings
//
// ----------------------------------------------------
void
CThirdPage::SelectMatch(Printer * printer, Service * service, Manufacturer * manufacturer, Model * model)
{
	LVFINDINFO	info;
	int			nIndex;

	check( printer != NULL );
	check( manufacturer != NULL );
	check( model != NULL );

	//
	// select the manufacturer
	//
	info.flags	= LVFI_STRING;
	info.psz	= manufacturer->name;

	nIndex = m_manufacturerListCtrl.FindItem(&info);
	
	if (nIndex != -1)
	{
		m_manufacturerListCtrl.SetItemState(nIndex, LVIS_SELECTED, LVIS_SELECTED);
		//
		//<rdar://problem/4528853> mDNS: When auto-highlighting items in lists, scroll list so highlighted item is in the middle
		//
		AutoScroll(m_manufacturerListCtrl, nIndex);
	}

	//
	// select the model
	//
	info.flags	= LVFI_STRING;
	info.psz	= model->displayName;

	nIndex = m_modelListCtrl.FindItem(&info);

	if (nIndex != -1)
	{
		m_modelListCtrl.SetItemState(nIndex, LVIS_SELECTED, LVIS_SELECTED);
		AutoScroll( m_modelListCtrl, nIndex );

		m_modelListCtrl.SetFocus();
	}

	CopyPrinterSettings( printer, service, manufacturer, model );
}

void
CThirdPage::SelectMatch(Manufacturers & manufacturers, Printer * printer, Service * service, Manufacturer * manufacturer, Model * model)
{
	PopulateUI( manufacturers );

	SelectMatch( printer, service, manufacturer, model );
}

// --------------------------------------------------------
// CopyPrinterSettings
//
// This function makes sure that the printer object has the
// latest settings from the manufacturer and model objects
// --------------------------------------------------------

void
CThirdPage::CopyPrinterSettings( Printer * printer, Service * service, Manufacturer * manufacturer, Model * model )
{
	printer->manufacturer		=	manufacturer->name;
	printer->displayModelName	=	model->displayName;
	printer->modelName			=	model->name;
	printer->driverInstalled	=	model->driverInstalled;
	printer->infFileName		=	model->infFileName;

	if ( service->type == kPDLServiceType )
	{
		printer->portName.Format(L"IP_%s.%d", static_cast<LPCTSTR>(service->hostname), service->portNumber);
		service->protocol = L"Raw";
	}
	else if ( service->type == kLPRServiceType )
	{
		Queue * q = service->queues.front();
		check( q );

		if ( q->name.GetLength() > 0 )
		{
			printer->portName.Format(L"LPR_%s.%d.%s", static_cast<LPCTSTR>(service->hostname), service->portNumber, static_cast<LPCTSTR>(q->name) );
		}
		else
		{
			printer->portName.Format(L"LPR_%s.%d", static_cast<LPCTSTR>(service->hostname), service->portNumber);
		}

		service->protocol = L"LPR";
	}
	else if ( service->type == kIPPServiceType )
	{
		Queue * q = service->queues.front();
		check( q );

		if ( q->name.GetLength() > 0 )
		{
			printer->portName.Format(L"http://%s:%d/%s", static_cast<LPCTSTR>(service->hostname), service->portNumber, static_cast<LPCTSTR>(q->name) );
		}
		else
		{
			printer->portName.Format(L"http://%s:%d/", static_cast<LPCTSTR>(service->hostname), service->portNumber );
		}

		service->protocol = L"IPP";
	}
}

// --------------------------------------------------------
// DefaultPrinterExists
//
// Checks to see if a default printer has been configured
// on this machine
// --------------------------------------------------------
BOOL
CThirdPage::DefaultPrinterExists()
{
	CPrintDialog dlg(FALSE);
	
	dlg.m_pd.Flags |= PD_RETURNDEFAULT;

	return dlg.GetDefaults();
}

// --------------------------------------------------------
// AutoScroll
//
// Ensure selected item is in middle of list
// --------------------------------------------------------
void
CThirdPage::AutoScroll( CListCtrl & list, int nIndex )
{
	//
	//<rdar://problem/4528853> mDNS: When auto-highlighting items in lists, scroll list so highlighted item is in the middle
	//

	int		top;
	int		count;

	list.EnsureVisible( nIndex, FALSE );
	
	top		= list.GetTopIndex();
	count	= list.GetCountPerPage();

	if ( ( nIndex == top ) || ( ( nIndex + 1 ) == ( top + count ) ) )
	{
		CRect	rect;
		int		rows;
		
		rows = ( count / 2 );

		if ( nIndex == top )
		{
			list.GetItemRect(0, rect, LVIR_BOUNDS);
			list.Scroll( CPoint( 0, rows * rect.Height() * -1 ) );
		}
		else
		{
			list.GetItemRect(0, rect, LVIR_BOUNDS);
			list.Scroll( CPoint( 0, rows * rect.Height() ) );
		}
	}
}

// ------------------------------------------------------
// LoadPrintDriverDefsFromFile
//
// The only potentially opaque thing about this function is the
// checkForDuplicateModels flag.  The problem here is that ntprint.inf
// doesn't contain duplicate models, and it has hundreds of models
// listed.  You wouldn't check for duplicates there.  But oftentimes,
// loading different windows print driver files contain multiple
// entries for the same printer.  You don't want the UI to display
// the same printer multiple times, so in that case, you would ask
// this function to check for multiple models.

OSStatus
CThirdPage::LoadPrintDriverDefsFromFile(Manufacturers & manufacturers, const CString & filename, bool checkForDuplicateModels )
{
	HINF			handle	= INVALID_HANDLE_VALUE;
	const TCHAR *	section = TEXT( "Manufacturer" );
	LONG			sectionCount;
	TCHAR			line[ 1000 ];
	CString			klass;
	INFCONTEXT		manufacturerContext;
	BOOL			ok;
	OSStatus		err		= 0;
	
	// Make sure we can open the file
	handle = SetupOpenInfFile( filename, NULL, INF_STYLE_WIN4, NULL );
	translate_errno( handle != INVALID_HANDLE_VALUE, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	// Make sure it's a printer file
	ok = SetupGetLineText( NULL, handle, TEXT( "Version" ), TEXT( "Class" ), line, sizeof( line ), NULL );
	translate_errno( ok, GetLastError(), kUnknownErr );
	require_noerr( err, exit );
	klass = line;
	require_action( klass == TEXT( "Printer" ), exit, err = kUnknownErr );

	sectionCount = SetupGetLineCount( handle, section );
	translate_errno( sectionCount != -1, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	memset( &manufacturerContext, 0, sizeof( manufacturerContext ) );
			
	for ( LONG i = 0; i < sectionCount; i++ )
	{
		Manufacturers::iterator	iter;
		Manufacturer	*	manufacturer;
		CString				manufacturerName;
		CString				temp;
		CStringList			modelSectionNameDecl;
		CString				modelSectionName;
		CString				baseModelName;
		CString				model;
		INFCONTEXT			modelContext;
		LONG				modelCount;
		POSITION			p;

		if ( i == 0 )
		{
			ok = SetupFindFirstLine( handle, section, NULL, &manufacturerContext );
			err = translate_errno( ok, GetLastError(), kUnknownErr );
			require_noerr( err, exit );
		}
		else
		{
			ok = SetupFindNextLine( &manufacturerContext, &manufacturerContext );
			err = translate_errno( ok, GetLastError(), kUnknownErr );
			require_noerr( err, exit );
		}

		ok = SetupGetStringField( &manufacturerContext, 0, line, sizeof( line ), NULL );
		err = translate_errno( ok, GetLastError(), kUnknownErr );
		require_noerr( err, exit );
		manufacturerName = line;

		ok = SetupGetLineText( &manufacturerContext, handle, NULL, NULL, line, sizeof( line ), NULL );
		err = translate_errno( ok, GetLastError(), kUnknownErr );
		require_noerr( err, exit );

		// Try to find some model section name that has entries. Explanation of int file structure
		// can be found at:
		//
		// <http://msdn.microsoft.com/en-us/library/ms794359.aspx>
		Split( line, ',', modelSectionNameDecl );

		p					= modelSectionNameDecl.GetHeadPosition();
		modelSectionName	= modelSectionNameDecl.GetNext( p );
		modelCount			= SetupGetLineCount( handle, modelSectionName );
		baseModelName		= modelSectionName;
		
		while ( modelCount <= 0 && p )
		{
			CString targetOSVersion;

			targetOSVersion		= modelSectionNameDecl.GetNext( p );
			modelSectionName	= baseModelName + TEXT( "." ) + targetOSVersion;
			modelCount			= SetupGetLineCount( handle, modelSectionName );
		}

		if ( modelCount > 0 )
		{
			manufacturerName = NormalizeManufacturerName( manufacturerName );

			iter = manufacturers.find( manufacturerName );

			if ( iter != manufacturers.end() )
			{
				manufacturer = iter->second;
				require_action( manufacturer, exit, err = kUnknownErr );
			}
			else
			{
				try
				{
					manufacturer = new Manufacturer;
				}
				catch (...)
				{
					manufacturer = NULL;
				}

				require_action( manufacturer, exit, err = kNoMemoryErr );

				manufacturer->name					= manufacturerName;
				manufacturers[ manufacturerName ]	= manufacturer;
			}

			memset( &modelContext, 0, sizeof( modelContext ) );

			for ( LONG j = 0; j < modelCount; j++ )
			{
				CString modelName;
				Model * model;

				if ( j == 0 )
				{
					ok = SetupFindFirstLine( handle, modelSectionName, NULL, &modelContext );
					err = translate_errno( ok, GetLastError(), kUnknownErr );
					require_noerr( err, exit );
				}
				else
				{
					SetupFindNextLine( &modelContext, &modelContext );
					err = translate_errno( ok, GetLastError(), kUnknownErr );
					require_noerr( err, exit );
				}

				ok = SetupGetStringField( &modelContext, 0, line, sizeof( line ), NULL );
				err = translate_errno( ok, GetLastError(), kUnknownErr );
				require_noerr( err, exit );

				modelName = line;

				if (checkForDuplicateModels == true)
				{
					if ( MatchModel( manufacturer, ConvertToModelName( modelName ) ) != NULL )
					{
						continue;
					}
				}

				//
				// Stock Vista printer inf files embed guids in the model
				// declarations for Epson printers. Let's ignore those.
				//
				if ( modelName.Find( TEXT( "{" ), 0 ) != -1 )
				{
					continue;
				}

				try
				{
					model = new Model;
				}
				catch (...)
				{
					model = NULL;
				}

				require_action( model, exit, err = kNoMemoryErr );

				model->infFileName		=	filename;
				model->displayName		=	modelName;
				model->name				=	modelName;
				model->driverInstalled	=	false;

				manufacturer->models.push_back(model);
			}
		}
	}

exit:

	if ( handle != INVALID_HANDLE_VALUE )
	{
		SetupCloseInfFile( handle );
		handle = NULL;
	}

	return err;
}


// -------------------------------------------------------
// LoadPrintDriverDefs
//
// This function is responsible for loading the print driver
// definitions of all print drivers that have been installed
// on this machine.
// -------------------------------------------------------
OSStatus
CThirdPage::LoadPrintDriverDefs( Manufacturers & manufacturers )
{
	BYTE	*	buffer			=	NULL;
	DWORD		bytesReceived	=	0;
	DWORD		numPrinters		=	0;
	OSStatus	err				=	0;
	BOOL		ok;

	//
	// like a lot of win32 calls, we call this first to get the
	// size of the buffer we need.
	//
	EnumPrinterDrivers(NULL, L"all", 6, NULL, 0, &bytesReceived, &numPrinters);

	if (bytesReceived > 0)
	{
		try
		{
			buffer = new BYTE[bytesReceived];
		}
		catch (...)
		{
			buffer = NULL;
		}
	
		require_action( buffer, exit, err = kNoMemoryErr );
		
		//
		// this call gets the real info
		//
		ok = EnumPrinterDrivers(NULL, L"all", 6, buffer, bytesReceived, &bytesReceived, &numPrinters);
		err = translate_errno( ok, errno_compat(), kUnknownErr );
		require_noerr( err, exit );
	
		DRIVER_INFO_6 * info = (DRIVER_INFO_6*) buffer;
	
		for (DWORD i = 0; i < numPrinters; i++)
		{
			Manufacturer	*	manufacturer;
			Model			*	model;
			CString				name;
	
			//
			// skip over anything that doesn't have a manufacturer field.  This
			// fixes a bug that I noticed that occurred after I installed
			// ProComm.  This program add a print driver with no manufacturer
			// that screwed up this wizard.
			//
			if (info[i].pszMfgName == NULL)
			{
				continue;
			}
	
			//
			// look for manufacturer
			//
			Manufacturers::iterator iter;
	
			//
			// save the name
			//
			name = NormalizeManufacturerName( info[i].pszMfgName );
	
			iter = manufacturers.find(name);
	
			if (iter != manufacturers.end())
			{
				manufacturer = iter->second;
			}
			else
			{
				try
				{
					manufacturer = new Manufacturer;
				}
				catch (...)
				{
					manufacturer = NULL;
				}
	
				require_action( manufacturer, exit, err = kNoMemoryErr );
	
				manufacturer->name	=	name;
	
				manufacturers[name]	=	manufacturer;
			}
	
			//
			// now look to see if we have already seen this guy.  this could
			// happen if we have already installed printers that are described
			// in ntprint.inf.  the extant drivers will show up in EnumPrinterDrivers
			// but we have already loaded their info
			//
			//
			if ( MatchModel( manufacturer, ConvertToModelName( info[i].pName ) ) == NULL )
			{
				try
				{
					model = new Model;
				}
				catch (...)
				{
					model = NULL;
				}
	
				require_action( model, exit, err = kNoMemoryErr );
	
				model->displayName		=	info[i].pName;
				model->name				=	info[i].pName;
				model->driverInstalled	=	true;
	
				manufacturer->models.push_back(model);
			}
		}
	}

exit:

	if (buffer != NULL)
	{
		delete [] buffer;
	}

	return err;
}

// -------------------------------------------------------
// LoadGenericPrintDriverDefs
//
// This function is responsible for loading polymorphic
// generic print drivers defs.  The UI will read
// something like "Generic / Postscript" and we can map
// that to any print driver we want.
// -------------------------------------------------------
OSStatus
CThirdPage::LoadGenericPrintDriverDefs( Manufacturers & manufacturers )
{
	Manufacturer		*	manufacturer;
	Model				*	model;
	Manufacturers::iterator	iter;
	CString					psDriverName;
	CString					pclDriverName;
	OSStatus				err	= 0;

	// <rdar://problem/4030388> Generic drivers don't do color

	// First try and find our generic driver names

	iter = m_manufacturers.find(L"HP");
	require_action( iter != m_manufacturers.end(), exit, err = kUnknownErr );
	manufacturer = iter->second;

	// Look for Postscript

	model = manufacturer->find( kGenericPSColorDriver );

	if ( !model )
	{
		model = manufacturer->find( kGenericPSDriver );
	}

	if ( model )
	{
		psDriverName = model->name;
	}

	// Look for PCL
	
	model = manufacturer->find( kGenericPCLColorDriver );

	if ( !model )
	{
		model = manufacturer->find( kGenericPCLDriver );
	}

	if ( model )
	{
		pclDriverName = model->name;
	}

	// If we found either a generic PS driver, or a generic PCL driver,
	// then add them to the list

	if ( psDriverName.GetLength() || pclDriverName.GetLength() )
	{
		// Try and find generic manufacturer if there is one

		iter = manufacturers.find(L"Generic");
		
		if (iter != manufacturers.end())
		{
			manufacturer = iter->second;
		}
		else
		{
			try
			{
				manufacturer = new Manufacturer;
			}
			catch (...)
			{
				manufacturer = NULL;
			}
		
			require_action( manufacturer, exit, err = kNoMemoryErr );
		
			manufacturer->name					=	"Generic";
			manufacturers[manufacturer->name]	=	manufacturer;
		}

		if ( psDriverName.GetLength() > 0 )
		{
			try
			{
				m_genericPostscript = new Model;
			}
			catch (...)
			{
				m_genericPostscript = NULL;
			}
			
			require_action( m_genericPostscript, exit, err = kNoMemoryErr );

			m_genericPostscript->displayName		=	kGenericPostscript;
			m_genericPostscript->name				=	psDriverName;
			m_genericPostscript->driverInstalled	=	false;

			manufacturer->models.push_back( m_genericPostscript );
		}

		if ( pclDriverName.GetLength() > 0 )
		{
			try
			{
				m_genericPCL = new Model;
			}
			catch (...)
			{
				m_genericPCL = NULL;
			}
			
			require_action( m_genericPCL, exit, err = kNoMemoryErr );

			m_genericPCL->displayName		=	kGenericPCL;
			m_genericPCL->name				=	pclDriverName;
			m_genericPCL->driverInstalled	=	false;

			manufacturer->models.push_back( m_genericPCL );
		}
	}

exit:

	return err;
}

// ------------------------------------------------------
// ConvertToManufacturerName
//
// This function is responsible for tweaking the
// name so that subsequent string operations won't fail because
// of capitalizations/different names for the same manufacturer
// (i.e.  Hewlett-Packard/HP/Hewlett Packard)
//
CString
CThirdPage::ConvertToManufacturerName( const CString & name )
{
	//
	// first we're going to convert all the characters to lower
	// case
	//
	CString lower = name;
	lower.MakeLower();

	//
	// now we're going to check to see if the string says "hewlett-packard",
	// because sometimes they refer to themselves as "hewlett-packard", and
	// sometimes they refer to themselves as "hp".
	//
	if ( lower == L"hewlett-packard")
	{
		lower = "hp";
	}

	//
	// tweak for Xerox Phaser, which doesn't announce itself
	// as a xerox
	//
	else if ( lower.Find( L"phaser", 0 ) != -1 )
	{
		lower = "xerox";
	}

	return lower;
}

// ------------------------------------------------------
// ConvertToModelName
//
// This function is responsible for ensuring that subsequent
// string operations don't fail because of differing capitalization
// schemes and the like
// ------------------------------------------------------

CString
CThirdPage::ConvertToModelName( const CString & name )
{
	//
	// convert it to lowercase
	//
	CString lower = name;
	lower.MakeLower();

	return lower;
}

// ------------------------------------------------------
// NormalizeManufacturerName
//
// This function is responsible for tweaking the manufacturer
// name so that there are no aliases for vendors
//
CString
CThirdPage::NormalizeManufacturerName( const CString & name )
{
	CString normalized = name;

	//
	// now we're going to check to see if the string says "hewlett-packard",
	// because sometimes they refer to themselves as "hewlett-packard", and
	// sometimes they refer to themselves as "hp".
	//
	if ( normalized == L"Hewlett-Packard")
	{
		normalized = "HP";
	}

	return normalized;
}

// -------------------------------------------------------
// MatchPrinter
//
// This function is responsible for matching a printer
// to a list of manufacturers and models.  It calls
// MatchManufacturer and MatchModel in turn.
//

OSStatus CThirdPage::MatchPrinter(Manufacturers & manufacturers, Printer * printer, Service * service, bool useCUPSWorkaround)
{
	CString					normalizedProductName;
	Manufacturer		*	manufacturer		=	NULL;
	Manufacturer		*	genericManufacturer	=	NULL;
	Model				*	model				=	NULL;
	Model				*	genericModel		=	NULL;
	bool					found				=	false;
	CString					text;
	OSStatus				err					=	kNoErr;

	check( printer );
	check( service );

	Queue * q = service->SelectedQueue();

	check( q );

	//
	// first look to see if we have a usb_MFG descriptor
	//
	if ( q->usb_MFG.GetLength() > 0)
	{
		manufacturer = MatchManufacturer( manufacturers, ConvertToManufacturerName ( q->usb_MFG ) );
	}

	if ( manufacturer == NULL )
	{
		q->product.Remove('(');
		q->product.Remove(')');

		manufacturer = MatchManufacturer( manufacturers, ConvertToManufacturerName ( q->product ) );
	}
	
	//
	// if we found the manufacturer, then start looking for the model
	//
	if ( manufacturer != NULL )
	{
		if ( q->usb_MDL.GetLength() > 0 )
		{
			model = MatchModel ( manufacturer, ConvertToModelName ( q->usb_MDL ) );
		}

		if ( ( model == NULL ) && ( q->product.GetLength() > 0 ) )
		{
			q->product.Remove('(');
			q->product.Remove(')');

			model = MatchModel ( manufacturer, ConvertToModelName ( q->product ) );
		}

		if ( model != NULL )
		{
			// <rdar://problem/4124524> Offer Generic printers if printer advertises Postscript or PCL.  Workaround
			// bug in OS X CUPS printer sharing by selecting Generic driver instead of matched printer.
 
			bool hasGenericDriver = false;

			if ( MatchGeneric( manufacturers, printer, service, &genericManufacturer, &genericModel ) )
			{
				hasGenericDriver = true;
			}

			// <rdar://problem/4190104> Use "application/octet-stream" to determine if CUPS
			// shared queue supports raw

			if ( q->pdl.Find( L"application/octet-stream" ) != -1 )
			{
				useCUPSWorkaround = false;
			}

			if ( useCUPSWorkaround && printer->isSharedFromOSX && hasGenericDriver )
			{
				//
				// <rdar://problem/4496652> mDNS: Don't allow user to choose non-working driver
				//
				Manufacturers genericManufacturers;

				LoadGenericPrintDriverDefs( genericManufacturers );

				SelectMatch( genericManufacturers, printer, service, genericManufacturer, genericModel );
			}
			else
			{
				SelectMatch(manufacturers, printer, service, manufacturer, model);
			}

			found = true;
		}
	}

	//
	// display a message to the user based on whether we could match
	// this printer
	//
	if (found)
	{
		text.LoadString(IDS_PRINTER_MATCH_GOOD);
		err = kNoErr;
	}
	else if ( MatchGeneric( manufacturers, printer, service, &genericManufacturer, &genericModel ) )
	{
		if ( printer->isSharedFromOSX )
		{
			//
			// <rdar://problem/4496652> mDNS: Don't allow user to choose non-working driver
			//
			Manufacturers genericManufacturers;

			LoadGenericPrintDriverDefs( genericManufacturers );

			SelectMatch( genericManufacturers, printer, service, genericManufacturer, genericModel );
			
			text.LoadString(IDS_PRINTER_MATCH_GOOD);
		}
		else
		{
			SelectMatch( manufacturers, printer, service, genericManufacturer, genericModel );
			text.LoadString(IDS_PRINTER_MATCH_MAYBE);
		}

		err = kNoErr;
	}
	else
	{
		text.LoadString(IDS_PRINTER_MATCH_BAD);

		//
		// if there was any crud in this list from before, get rid of it now
		//
		m_modelListCtrl.DeleteAllItems();
		
		//
		// select the manufacturer if we found one
		//
		if (manufacturer != NULL)
		{
			LVFINDINFO	info;
			int			nIndex;

			//
			// select the manufacturer
			//
			info.flags	= LVFI_STRING;
			info.psz	= manufacturer->name;

			nIndex = m_manufacturerListCtrl.FindItem(&info);
	
			if (nIndex != -1)
			{
				m_manufacturerListCtrl.SetItemState(nIndex, LVIS_SELECTED, LVIS_SELECTED);

				//
				//<rdar://problem/4528853> mDNS: When auto-highlighting items in lists, scroll list so highlighted item is in the middle
				//
				AutoScroll(m_manufacturerListCtrl, nIndex);
			}
		}

		err = kUnknownErr;
	}

	m_printerSelectionText.SetWindowText(text);

	return err;
}

// ------------------------------------------------------
// MatchManufacturer
//
// This function is responsible for finding a manufacturer
// object from a string name.  It does a CString::Find, which
// is like strstr, so it doesn't have to do an exact match
//
// If it can't find a match, NULL is returned
// ------------------------------------------------------

Manufacturer*
CThirdPage::MatchManufacturer( Manufacturers & manufacturers, const CString & name)
{
	Manufacturers::iterator iter;

	for (iter = manufacturers.begin(); iter != manufacturers.end(); iter++)
	{
		//
		// we're going to convert all the manufacturer names to lower case,
		// so we match the name passed in.
		//
		CString lower = iter->second->name;
		lower.MakeLower();

		//
		// now try and find the lowered string in the name passed in.
		//
		if (name.Find(lower) != -1)
		{
			return iter->second;
		}
	}

	return NULL;
}

// -------------------------------------------------------
// MatchModel
//
// This function is responsible for matching a model from
// a name.  It does a CString::Find(), which works like strstr,
// so it doesn't rely on doing an exact string match.
//

Model*
CThirdPage::MatchModel(Manufacturer * manufacturer, const CString & name)
{
	Models::iterator iter;

	iter = manufacturer->models.begin();

	for (iter = manufacturer->models.begin(); iter != manufacturer->models.end(); iter++)
	{
		Model * model = *iter;

		//
		// convert the model name to lower case
		//
		CString lowered = model->name;
		lowered.MakeLower();

		if (lowered.Find( name ) != -1)
		{
			return model;
		}

		//
		// <rdar://problem/3841218>
		// try removing the first substring and search again
		//

		if ( name.Find(' ') != -1 )
		{
			CString altered = name;
			altered.Delete( 0, altered.Find(' ') + 1 );

			if ( lowered.Find( altered ) != -1 )
			{
				return model;
			}
		}
	}

	return NULL;
}

// -------------------------------------------------------
// MatchGeneric
//
// This function will attempt to find a generic printer
// driver for a printer that we weren't able to match
// specifically
//
BOOL
CThirdPage::MatchGeneric( Manufacturers & manufacturers, Printer * printer, Service * service, Manufacturer ** manufacturer, Model ** model )
{
	CString	pdl;
	BOOL	ok = FALSE;

	DEBUG_UNUSED( printer );

	check( service );

	Queue * q = service->SelectedQueue();

	check( q );

	Manufacturers::iterator iter = manufacturers.find( kGenericManufacturer );
	require_action_quiet( iter != manufacturers.end(), exit, ok = FALSE );

	*manufacturer = iter->second;

	pdl = q->pdl;
	pdl.MakeLower();

	if ( m_genericPCL && ( pdl.Find( kPDLPCLKey ) != -1 ) )
	{
		*model	= m_genericPCL;
		ok		= TRUE;
	}
	else if ( m_genericPostscript && ( pdl.Find( kPDLPostscriptKey ) != -1 ) )
	{
		*model	= m_genericPostscript;
		ok		= TRUE;
	}

exit:

	return ok;
}

// -----------------------------------------------------------
// OnInitPage
//
// This function is responsible for doing initialization that
// only occurs once during a run of the wizard
//

OSStatus CThirdPage::OnInitPage()
{
	CString		header;
	CString		ntPrint;
	OSStatus	err = kNoErr;

	// Load printer icon
	check( m_printerImage == NULL );
	
	m_printerImage = (CStatic*) GetDlgItem( 1 );	// 1 == IDR_MANIFEST
	check( m_printerImage );

	if ( m_printerImage != NULL )
	{
		m_printerImage->SetIcon( LoadIcon( GetNonLocalizedResources(), MAKEINTRESOURCE( IDI_PRINTER ) ) );
	}

	//
	// The CTreeCtrl widget automatically sends a selection changed
	// message which initially we want to ignore, because the user
	// hasn't selected anything
	//
	// this flag gets reset in the message handler.  Every subsequent
	// message gets handled.
	//

	//
	// we have to make sure that we only do this once.  Typically,
	// we would do this in something like OnInitDialog, but we don't
	// have this in Wizards, because the window is a PropertySheet.
	// We're considered fully initialized when we receive the first
	// selection notice
	//
	header.LoadString(IDS_MANUFACTURER_HEADING);
	m_manufacturerListCtrl.InsertColumn(0, header, LVCFMT_LEFT, -1 );
	m_manufacturerSelected = NULL;

	header.LoadString(IDS_MODEL_HEADING);
	m_modelListCtrl.InsertColumn(0, header, LVCFMT_LEFT, -1 );
	m_modelSelected = NULL;

	return (err);
}

void CThirdPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PRINTER_MANUFACTURER, m_manufacturerListCtrl);
	DDX_Control(pDX, IDC_PRINTER_MODEL, m_modelListCtrl);
	DDX_Control(pDX, IDC_PRINTER_NAME, m_printerName);
	DDX_Control(pDX, IDC_DEFAULT_PRINTER, m_defaultPrinterCtrl);
	DDX_Control(pDX, IDC_PRINTER_SELECTION_TEXT, m_printerSelectionText);

}

// ----------------------------------------------------------
// OnSetActive
//
// This function is called by MFC after the window has been
// activated.
//

BOOL
CThirdPage::OnSetActive()
{
	CPrinterSetupWizardSheet	*	psheet;
	Printer						*	printer;
	Service						*	service;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );
   
	psheet->SetWizardButtons( PSWIZB_BACK );

	printer = psheet->GetSelectedPrinter();
	require_quiet( printer, exit );

	service = printer->services.front();
	require_quiet( service, exit );

	//
	// call OnInitPage once
	//
	if (!m_initialized)
	{
		OnInitPage();
		m_initialized = true;
	}

	//
	// <rdar://problem/4580061> mDNS: Printers added using Bonjour should be set as the default printer.
	//
	if ( DefaultPrinterExists() )
	{
		m_defaultPrinterCtrl.SetCheck( BST_UNCHECKED );
		printer->deflt = false;
	}
	else
	{
		m_defaultPrinterCtrl.SetCheck( BST_CHECKED );
		printer->deflt = true;
	}

	//
	// update the UI with the printer name
	//
	m_printerName.SetWindowText(printer->displayName);

	//
	// populate the list controls with the manufacturers and models
	// from ntprint.inf
	//
	PopulateUI( m_manufacturers );

	//
	// and try and match the printer
	//

	if ( psheet->GetLastPage() == psheet->GetPage(0) )
	{
		MatchPrinter( m_manufacturers, printer, service, true );

		if ( ( m_manufacturerSelected != NULL ) && ( m_modelSelected != NULL  ) )
		{
			GetParent()->PostMessage(PSM_SETCURSEL, 2 );
		}
	}
	else
	{
		SelectMatch(printer, service, m_manufacturerSelected, m_modelSelected);
	}

exit:

	return CPropertyPage::OnSetActive();
}

BOOL
CThirdPage::OnKillActive()
{
	CPrinterSetupWizardSheet * psheet;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );
   
	psheet->SetLastPage(this);

exit:

	return CPropertyPage::OnKillActive();
}

// -------------------------------------------------------
// PopulateUI
//
// This function is called to populate the list of manufacturers
//
OSStatus
CThirdPage::PopulateUI(Manufacturers & manufacturers)
{
	Manufacturers::iterator iter;
	
	m_manufacturerListCtrl.DeleteAllItems();

	for (iter = manufacturers.begin(); iter != manufacturers.end(); iter++)
	{
		int nIndex;

		Manufacturer * manufacturer = iter->second;

		nIndex = m_manufacturerListCtrl.InsertItem(0, manufacturer->name);

		m_manufacturerListCtrl.SetItemData(nIndex, (DWORD_PTR) manufacturer);

		m_manufacturerListCtrl.SetColumnWidth( 0, LVSCW_AUTOSIZE_USEHEADER );
	}

	return 0;
}

BEGIN_MESSAGE_MAP(CThirdPage, CPropertyPage)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_PRINTER_MANUFACTURER, OnLvnItemchangedManufacturer)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_PRINTER_MODEL, OnLvnItemchangedPrinterModel)
	ON_BN_CLICKED(IDC_DEFAULT_PRINTER, OnBnClickedDefaultPrinter)
	ON_BN_CLICKED(IDC_HAVE_DISK, OnBnClickedHaveDisk)
END_MESSAGE_MAP()

// CThirdPage message handlers
void CThirdPage::OnLvnItemchangedManufacturer(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	POSITION p = m_manufacturerListCtrl.GetFirstSelectedItemPosition();
	int nSelected = m_manufacturerListCtrl.GetNextSelectedItem(p);

	if (nSelected != -1)
	{
		m_manufacturerSelected = (Manufacturer*) m_manufacturerListCtrl.GetItemData(nSelected);

		m_modelListCtrl.SetRedraw(FALSE);
		
		m_modelListCtrl.DeleteAllItems();
		m_modelSelected = NULL;

		Models::iterator iter;

		for (iter = m_manufacturerSelected->models.begin(); iter != m_manufacturerSelected->models.end(); iter++)
		{
			Model * model = *iter;

			int nItem = m_modelListCtrl.InsertItem( 0, model->displayName );

			m_modelListCtrl.SetItemData(nItem, (DWORD_PTR) model);

			m_modelListCtrl.SetColumnWidth( 0, LVSCW_AUTOSIZE_USEHEADER );
		}

		m_modelListCtrl.SetRedraw(TRUE);
	}

	*pResult = 0;
}

void CThirdPage::OnLvnItemchangedPrinterModel(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW					pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	
	CPrinterSetupWizardSheet	*	psheet;
	Printer						*	printer;
	Service						*	service;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );

	printer = psheet->GetSelectedPrinter();
	require_quiet( printer, exit );

	service = printer->services.front();
	require_quiet( service, exit );

	check ( m_manufacturerSelected );

	POSITION p = m_modelListCtrl.GetFirstSelectedItemPosition();
	int nSelected = m_modelListCtrl.GetNextSelectedItem(p);

	if (nSelected != -1)
	{
		m_modelSelected = (Model*) m_modelListCtrl.GetItemData(nSelected);

		CopyPrinterSettings( printer, service, m_manufacturerSelected, m_modelSelected );

		psheet->SetWizardButtons(PSWIZB_BACK|PSWIZB_NEXT);
	}
	else
	{
		psheet->SetWizardButtons(PSWIZB_BACK);
	}

exit:

	*pResult = 0;
}

void CThirdPage::OnBnClickedDefaultPrinter()
{
	CPrinterSetupWizardSheet	*	psheet;
	Printer						*	printer;

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );

	printer = psheet->GetSelectedPrinter();
	require_quiet( printer, exit );

	printer->deflt = ( m_defaultPrinterCtrl.GetCheck() == BST_CHECKED ) ? true : false;

exit:

	return;
}

void CThirdPage::OnBnClickedHaveDisk()
{
	CPrinterSetupWizardSheet	*	psheet;
	Printer						*	printer;
	Service						*	service;
	Manufacturers					manufacturers;

	CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY|OFN_FILEMUSTEXIST, L"Setup Information (*.inf)|*.inf||", this);

	psheet = reinterpret_cast<CPrinterSetupWizardSheet*>(GetParent());
	require_quiet( psheet, exit );

	printer = psheet->GetSelectedPrinter();
	require_quiet( printer, exit );
	
	service = printer->services.front();
	require_quiet( service, exit );

	for ( ;; )
	{
		if ( dlg.DoModal() == IDOK )
		{
			CString filename = dlg.GetPathName();

			LoadPrintDriverDefsFromFile( manufacturers, filename, true );
   
			// Sanity check

			if ( manufacturers.size() > 0 )
			{
				PopulateUI( manufacturers );

				if ( MatchPrinter( manufacturers, printer, service, false ) != kNoErr )
				{
					CString errorMessage;
					CString errorCaption;
					
					errorMessage.LoadString( IDS_NO_MATCH_INF_FILE );
					errorCaption.LoadString( IDS_NO_MATCH_INF_FILE_CAPTION );

					MessageBox( errorMessage, errorCaption, MB_OK );
				}

				break;
			}
			else
			{
				CString errorMessage;
				CString errorCaption;

				errorMessage.LoadString( IDS_BAD_INF_FILE );
				errorCaption.LoadString( IDS_BAD_INF_FILE_CAPTION );

				MessageBox( errorMessage, errorCaption, MB_OK );
			}
		}
		else
		{
			break;
		}
	}

exit:

	return;
}


void
CThirdPage::Split( const CString & string, TCHAR ch, CStringList & components )
{
	CString	temp;
	int		n;

	temp = string;
	
	while ( ( n = temp.Find( ch ) ) != -1 )
	{
		components.AddTail( temp.Left( n ) );
		temp = temp.Right( temp.GetLength() - ( n + 1 ) );
	}

	components.AddTail( temp );
}
