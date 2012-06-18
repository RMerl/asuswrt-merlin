/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
    
$Log: VPCDetect.cpp,v $
Revision 1.3  2006/08/14 23:25:20  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2006/02/26 19:31:05  herscher
<rdar://problem/4455038> Bonjour For Windows takes 90 seconds to start. This was caused by a bad interaction between the VirtualPC check, and the removal of the WMI dependency.  The problem was fixed by: 1) checking to see if WMI is running before trying to talk to it.  2) Retrying the VirtualPC check every 10 seconds upon failure, stopping after 10 unsuccessful tries.

Revision 1.1  2005/11/27 20:21:16  herscher
<rdar://problem/4210580> Workaround Virtual PC bug that incorrectly modifies incoming mDNS packets

*/

#define _WIN32_DCOM
#include "VPCDetect.h"
#include "DebugServices.h"
#include <comdef.h>
#include <Wbemidl.h>

# pragma comment(lib, "wbemuuid.lib")

static BOOL g_doneCheck = FALSE;
static BOOL g_isVPC		= FALSE;


mStatus
IsVPCRunning( BOOL * inVirtualPC )
{
	IWbemLocator			*	pLoc 		= 0;
	IWbemServices			*	pSvc 		= 0;
    IEnumWbemClassObject	*	pEnumerator = NULL;
	bool						coInit 		= false;
	HRESULT						hres;
	SC_HANDLE					scm			= NULL;
	SC_HANDLE					service		= NULL;
	SERVICE_STATUS				status;
	mStatus						err;
	BOOL						ok          = TRUE;

	// Initialize flag

	*inVirtualPC = FALSE;

	// Find out if WMI is running

	scm = OpenSCManager( 0, 0, SC_MANAGER_CONNECT );
	err = translate_errno( scm, (OSStatus) GetLastError(), kOpenErr );
	require_noerr( err, exit );

	service = OpenService( scm, TEXT( "winmgmt" ), SERVICE_QUERY_STATUS );
	err = translate_errno( service, (OSStatus) GetLastError(), kNotFoundErr );
	require_noerr( err, exit );
	
	ok = QueryServiceStatus( service, &status );
	err = translate_errno( ok, (OSStatus) GetLastError(), kAuthenticationErr );
	require_noerr( err, exit );
	require_action( status.dwCurrentState == SERVICE_RUNNING, exit, err = kUnknownErr );
	
    // Initialize COM.

	hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );
	coInit = true;

	// Initialize Security

	hres =  CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL );
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );

                      
    // Obtain the initial locator to Windows Management on a particular host computer.

    hres = CoCreateInstance( CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc );
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );
 
    // Connect to the root\cimv2 namespace with the
    // current user and obtain pointer pSvc
    // to make IWbemServices calls.

	hres = pLoc->ConnectServer( _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &pSvc );
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );
    
    // Set the IWbemServices proxy so that impersonation
    // of the user (client) occurs.

	hres = CoSetProxyBlanket( pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE );
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );

    // Use the IWbemServices pointer to make requests of WMI. 
    // Make requests here:

	hres = pSvc->ExecQuery( bstr_t("WQL"), bstr_t("SELECT * from Win32_BaseBoard"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    
	require_action( SUCCEEDED( hres ), exit, err = kUnknownErr );

	do
	{
		IWbemClassObject* pInstance = NULL;
		ULONG dwCount = NULL;

		hres = pEnumerator->Next( WBEM_INFINITE, 1, &pInstance, &dwCount);

		if ( pInstance )
		{
			VARIANT v;
			BSTR strClassProp = SysAllocString(L"Manufacturer");
			HRESULT hr;

			hr = pInstance->Get(strClassProp, 0, &v, 0, 0);
			SysFreeString(strClassProp);

			// check the HRESULT to see if the action succeeded.

			if (SUCCEEDED(hr) && (V_VT(&v) == VT_BSTR))
			{
				wchar_t * wstring = wcslwr( V_BSTR( &v ) );

				if (wcscmp( wstring, L"microsoft corporation" ) == 0 )
				{
					*inVirtualPC = TRUE;
				}
			}
		
			VariantClear(&v);
		}
	} while (hres == WBEM_S_NO_ERROR);
         
exit:
 
	if ( pSvc != NULL )
	{
    	pSvc->Release();
	}

	if ( pLoc != NULL )
	{
    	pLoc->Release();     
	}

	if ( coInit )
	{
    	CoUninitialize();
	}

	if ( service )
	{
		CloseServiceHandle( service );
	}

	if ( scm )
	{
		CloseServiceHandle( scm );
	}

	if ( *inVirtualPC )
	{
		dlog( kDebugLevelTrace, "Virtual PC detected" );
	}

	return err;
}
