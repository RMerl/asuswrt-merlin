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
    
$Log: RegNames.h,v $
Revision 1.5  2009/03/30 21:47:35  herscher
Fix file corruption during previous checkin

Revision 1.3  2006/08/14 23:25:20  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2005/10/05 18:05:28  herscher
<rdar://problem/4192011> Save Wide-Area preferences in a different spot in the registry so they don't get removed when doing an update install.

Revision 1.1  2005/03/03 02:31:37  shersche
Consolidates all registry key names and can safely be included in any component that needs it


*/

//----------------------------------------------------------------------------------------
//	Registry Constants
//----------------------------------------------------------------------------------------

#if defined(UNICODE)

#	define kServiceParametersSoftware			L"SOFTWARE"
#	define kServiceParametersAppleComputer		L"Apple Computer, Inc."
#	define kServiceParametersBonjour			L"Bonjour"
#	define kServiceParametersNode				L"SOFTWARE\\Apple Inc.\\Bonjour"
#	define kServiceName							L"Bonjour Service"
#	define kServiceDynDNSBrowseDomains			L"BrowseDomains"
#	define kServiceDynDNSHostNames				L"HostNames"
#	define kServiceDynDNSRegistrationDomains	L"RegistrationDomains"
#	define kServiceDynDNSDomains				L"Domains"	// value is comma separated list of domains
#	define kServiceDynDNSEnabled				L"Enabled"
#	define kServiceDynDNSStatus					L"Status"
#	define kServiceManageLLRouting				L"ManageLLRouting"
#	define kServiceCacheEntryCount				L"CacheEntryCount"
#	define kServiceManageFirewall				L"ManageFirewall"

# else

#	define kServiceParametersSoftware			"SOFTWARE"
#	define kServiceParametersAppleComputer		"Apple Computer, Inc."
#	define kServiceParametersBonjour			"Bonjour"
#	define kServiceParametersNode				"SOFTWARE\\Apple Inc.\\Bonjour"
#	define kServiceName							"Bonjour Service"
#	define kServiceDynDNSBrowseDomains			"BrowseDomains"
#	define kServiceDynDNSHostNames				"HostNames"
#	define kServiceDynDNSRegistrationDomains	"RegistrationDomains"
#	define kServiceDynDNSDomains				"Domains"	// value is comma separated list of domains
#	define kServiceDynDNSEnabled				"Enabled"
#	define kServiceDynDNSStatus					"Status"
#	define kServiceManageLLRouting				"ManageLLRouting"
#	define kServiceCacheEntryCount				"CacheEntryCount"
#	define kServiceManageFirewall				"ManageFirewall"

#endif
