/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: Firewall.cpp,v $
Revision 1.6  2009/04/24 04:55:26  herscher
<rdar://problem/3496833> Advertise SMB file sharing via Bonjour

Revision 1.5  2009/03/30 20:39:29  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows
<rdar://problem/5712486> Put in extra defensive checks to prevent NULL pointer dereferencing crash
<rdar://problem/5187308> Move build train to Visual Studio 2005

Revision 1.4  2006/08/14 23:26:07  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.3  2005/09/29 06:33:54  herscher
<rdar://problem/4278931> Fix compilation error when using latest Microsoft Platform SDK.

Revision 1.2  2004/09/15 09:39:53  shersche
Retry the method INetFwPolicy::get_CurrentProfile on error

Revision 1.1  2004/09/13 07:32:31  shersche
Wrapper for Windows Firewall API code


*/

// <rdar://problem/4278931> Doesn't compile correctly with latest Platform SDK

#if !defined(_WIN32_DCOM)
#	define _WIN32_DCOM 
#endif


#include "Firewall.h"
#include <windows.h>
#include <crtdbg.h>
#include <netfw.h>
#include <objbase.h>
#include <oleauto.h>


static const int kMaxTries			= 30;
static const int kRetrySleepPeriod	= 1 * 1000; // 1 second


static OSStatus
mDNSFirewallInitialize(OUT INetFwProfile ** fwProfile)
{
	INetFwMgr		*	fwMgr		= NULL;
	INetFwPolicy	*	fwPolicy	= NULL;
	int					numRetries	= 0;
	HRESULT				err			= kNoErr;
    
	_ASSERT(fwProfile != NULL);

    *fwProfile = NULL;

	// Use COM to get a reference to the firewall settings manager.  This
	// call will fail on anything other than XP SP2

	err = CoCreateInstance( __uuidof(NetFwMgr), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwMgr), (void**)&fwMgr );
	require(SUCCEEDED(err) && ( fwMgr != NULL ), exit);

	// Use the reference to get the local firewall policy

	err = fwMgr->get_LocalPolicy(&fwPolicy);
	require(SUCCEEDED(err) && ( fwPolicy != NULL ), exit);

	// Use the reference to get the extant profile. Empirical evidence
	// suggests that there is the potential for a race condition when a system
	// service whose startup type is automatic calls this method.
	// This is true even when the service declares itself to be dependent
	// on the firewall service. Re-trying the method will succeed within
	// a few seconds.

	do
	{
    	err = fwPolicy->get_CurrentProfile(fwProfile);

		if (err)
		{
			Sleep(kRetrySleepPeriod);
		}
	}
	while (err && (numRetries++ < kMaxTries));

	require(SUCCEEDED(err), exit);

	err = kNoErr;

exit:

	// Release temporary COM objects

    if (fwPolicy != NULL)
    {
        fwPolicy->Release();
    }

    if (fwMgr != NULL)
    {
        fwMgr->Release();
    }

    return err;
}


static void
mDNSFirewallCleanup
			(
			IN INetFwProfile	*	fwProfile
			)
{
	// Call Release on the COM reference.

    if (fwProfile != NULL)
    {
        fwProfile->Release();
    }
}


static OSStatus
mDNSFirewallAppIsEnabled
			(
			IN INetFwProfile	*	fwProfile,
			IN const wchar_t	*	fwProcessImageFileName,
			OUT BOOL			*	fwAppEnabled    
			)
{
	BSTR							fwBstrProcessImageFileName = NULL;
	VARIANT_BOOL					fwEnabled;
	INetFwAuthorizedApplication	*	fwApp	= NULL;
	INetFwAuthorizedApplications*	fwApps	= NULL;
	OSStatus						err		= kNoErr;
    
	_ASSERT(fwProfile != NULL);
	_ASSERT(fwProcessImageFileName != NULL);
	_ASSERT(fwAppEnabled != NULL);

    *fwAppEnabled = FALSE;

	// Get the list of authorized applications

	err = fwProfile->get_AuthorizedApplications(&fwApps);
	require(SUCCEEDED(err) && ( fwApps != NULL ), exit);

    fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
	require_action( ( fwProcessImageFileName != NULL ) && ( SysStringLen(fwBstrProcessImageFileName) > 0 ), exit, err = kNoMemoryErr);

	// Look for us

    err = fwApps->Item(fwBstrProcessImageFileName, &fwApp);
	
    if (SUCCEEDED(err) && ( fwApp != NULL ) )
    {
        // It's listed, but is it enabled?

		err = fwApp->get_Enabled(&fwEnabled);
		require(SUCCEEDED(err), exit);

        if (fwEnabled != VARIANT_FALSE)
        {
			// Yes, it's enabled

            *fwAppEnabled = TRUE;
		}
	}

	err = kNoErr;

exit:

	// Deallocate the BSTR

	if ( fwBstrProcessImageFileName != NULL )
	{
		SysFreeString(fwBstrProcessImageFileName);
	}

	// Release the COM objects

    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return err;
}


static OSStatus
mDNSFirewallAddApp
			(
            IN INetFwProfile	*	fwProfile,
            IN const wchar_t	*	fwProcessImageFileName,
            IN const wchar_t	*	fwName
            )
{
	BOOL							fwAppEnabled;
	BSTR							fwBstrName = NULL;
	BSTR							fwBstrProcessImageFileName = NULL;
	INetFwAuthorizedApplication	*	fwApp = NULL;
	INetFwAuthorizedApplications*	fwApps = NULL;
	OSStatus						err = S_OK;
    
	_ASSERT(fwProfile != NULL);
    _ASSERT(fwProcessImageFileName != NULL);
    _ASSERT(fwName != NULL);

    // First check to see if the application is already authorized.
	err = mDNSFirewallAppIsEnabled( fwProfile, fwProcessImageFileName, &fwAppEnabled );
	require_noerr(err, exit);

	// Only add the application if it isn't enabled

	if (!fwAppEnabled)
	{
		// Get the list of authorized applications

        err = fwProfile->get_AuthorizedApplications(&fwApps);
		require(SUCCEEDED(err) && ( fwApps != NULL ), exit);

        // Create an instance of an authorized application.

		err = CoCreateInstance( __uuidof(NetFwAuthorizedApplication), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwAuthorizedApplication), (void**)&fwApp );
		require(SUCCEEDED(err) && ( fwApp != NULL ), exit);

        fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
		require_action(( fwProcessImageFileName != NULL ) && ( SysStringLen(fwBstrProcessImageFileName) > 0 ), exit, err = kNoMemoryErr);

		// Set the executable file name

		err = fwApp->put_ProcessImageFileName(fwBstrProcessImageFileName);
		require(SUCCEEDED(err), exit);

		fwBstrName = SysAllocString(fwName);
		require_action( ( fwBstrName != NULL ) && ( SysStringLen(fwBstrName) > 0 ), exit, err = kNoMemoryErr);

		// Set the friendly name

        err = fwApp->put_Name(fwBstrName);
		require(SUCCEEDED(err), exit);

		// Now add the application

        err = fwApps->Add(fwApp);
		require(SUCCEEDED(err), exit);
	}

	err = kNoErr;

exit:

	// Deallocate the BSTR objects

	if ( fwBstrName != NULL )
	{
		SysFreeString(fwBstrName);
	}

	if ( fwBstrProcessImageFileName != NULL )
	{
		SysFreeString(fwBstrProcessImageFileName);
	}

    // Release the COM objects

    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return err;
}


static OSStatus
mDNSFirewallIsFileAndPrintSharingEnabled
	(
	IN INetFwProfile	* fwProfile,
	OUT BOOL			* fwServiceEnabled
	)
{
    VARIANT_BOOL fwEnabled;
    INetFwService* fwService = NULL;
    INetFwServices* fwServices = NULL;
	OSStatus err = S_OK;

    _ASSERT(fwProfile != NULL);
    _ASSERT(fwServiceEnabled != NULL);

    *fwServiceEnabled = FALSE;

    // Retrieve the globally open ports collection.
    err = fwProfile->get_Services(&fwServices);
	require( SUCCEEDED( err ), exit );

    // Attempt to retrieve the globally open port.
    err = fwServices->Item(NET_FW_SERVICE_FILE_AND_PRINT, &fwService);
	require( SUCCEEDED( err ), exit );
	
	// Find out if the globally open port is enabled.
    err = fwService->get_Enabled(&fwEnabled);
	require( SUCCEEDED( err ), exit );
	if (fwEnabled != VARIANT_FALSE)
	{
		*fwServiceEnabled = TRUE;
	}

exit:

    // Release the globally open port.
    if (fwService != NULL)
    {
        fwService->Release();
    }

    // Release the globally open ports collection.
    if (fwServices != NULL)
    {
        fwServices->Release();
    }

    return err;
}


OSStatus
mDNSAddToFirewall
		(
		LPWSTR	executable,
		LPWSTR	name
		)
{
	INetFwProfile	*	fwProfile	= NULL;
	HRESULT				comInit		= E_FAIL;
	OSStatus			err			= kNoErr;

	// Initialize COM.

	comInit = CoInitializeEx( 0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	// Ignore this case. RPC_E_CHANGED_MODE means that COM has already been
	// initialized with a different mode.

	if (comInit != RPC_E_CHANGED_MODE)
	{
		err = comInit;
		require(SUCCEEDED(err), exit);
	}

	// Connect to the firewall

	err = mDNSFirewallInitialize(&fwProfile);
	require( SUCCEEDED( err ) && ( fwProfile != NULL ), exit);

	// Add us to the list of exempt programs

	err = mDNSFirewallAddApp( fwProfile, executable, name );
	require_noerr(err, exit);

exit:

	// Disconnect from the firewall

	if ( fwProfile != NULL )
	{
		mDNSFirewallCleanup(fwProfile);
	}

	// De-initialize COM

	if (SUCCEEDED(comInit))
    {
        CoUninitialize();
    }

	return err;
}


BOOL
mDNSIsFileAndPrintSharingEnabled()
{
	INetFwProfile	*	fwProfile					= NULL;
	HRESULT				comInit						= E_FAIL;
	BOOL				enabled						= FALSE;
	OSStatus			err							= kNoErr;

	// Initialize COM.

	comInit = CoInitializeEx( 0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

	// Ignore this case. RPC_E_CHANGED_MODE means that COM has already been
	// initialized with a different mode.

	if (comInit != RPC_E_CHANGED_MODE)
	{
		err = comInit;
		require(SUCCEEDED(err), exit);
	}

	// Connect to the firewall

	err = mDNSFirewallInitialize(&fwProfile);
	require( SUCCEEDED( err ) && ( fwProfile != NULL ), exit);

	err = mDNSFirewallIsFileAndPrintSharingEnabled( fwProfile, &enabled );
	require_noerr( err, exit );

exit:

	// Disconnect from the firewall

	if ( fwProfile != NULL )
	{
		mDNSFirewallCleanup(fwProfile);
	}

	// De-initialize COM

	if (SUCCEEDED(comInit))
    {
        CoUninitialize();
    }

	return enabled;
}
