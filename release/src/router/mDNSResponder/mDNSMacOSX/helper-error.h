/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

$Log: helper-error.h,v $
Revision 1.8  2007/11/07 00:22:30  jgraessley
Bug #: <rdar://problem/5573573> mDNSResponder doesn't build without IPSec
Reviewed by: Stuart Cheshire

Revision 1.7  2007/09/12 00:42:47  mcguire
<rdar://problem/5468236> BTMM: Need to clean up security associations

Revision 1.6  2007/09/04 22:32:58  mcguire
<rdar://problem/5453633> BTMM: BTMM overwrites /etc/racoon/remote/anonymous.conf

Revision 1.5  2007/08/29 21:42:12  mcguire
<rdar://problem/5431192> BTMM: Duplicate Private DNS names are being added to DynamicStore

Revision 1.4  2007/08/23 21:15:49  cheshire
Added $Log header

Revision 1.3  2007/08/23 21:04:44  cheshire
Tidied up alignment of error message list

Revision 1.2  2007/08/18 01:02:03  mcguire
<rdar://problem/5415593> No Bonjour services are getting registered at boot

Revision 1.1  2007/08/08 22:34:58  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root

 */

ERROR(kmDNSHelperCommunicationFailed,             "Mach communication failed")
ERROR(kmDNSHelperNotAuthorized,                   "Not authorized")
ERROR(kmDNSHelperCreationFailed,                  "Object creation failed")
ERROR(kmDNSHelperInvalidPList,                    "Invalid property list")
ERROR(kmDNSHelperDynamicStoreFailed,              "Could not create dynamic store session")
ERROR(kmDNSHelperDynamicStoreSetFailed,           "Could not set dynamic store configuration")
ERROR(kmDNSHelperInvalidNameKey,                  "Invalid name key")
ERROR(kmDNSHelperInvalidConfigKey,                "Invalid configuration key")
ERROR(kmDNSHelperTypeError,                       "Object was not of expected type")
ERROR(kmDNSHelperPreferencesFailed,               "Could not create preferences session")
ERROR(kmDNSHelperPreferencesLockFailed,           "Could not lock preferences")
ERROR(kmDNSHelperPreferencesSetFailed,            "Could not update preferences")
ERROR(kmDNSHelperKeychainCopyDefaultFailed,       "Could not copy keychain default")
ERROR(kmDNSHelperKeychainSearchCreationFailed,    "Could not create keychain search")
ERROR(kmDNSHelperPListWriteFailed,                "Could not write property list to stream")
ERROR(kmDNSHelperResultTooLarge,                  "Result too large")
ERROR(kmDNSHelperInterfaceCreationFailed,         "Could not create auto-tunnel interface")
ERROR(kmDNSHelperInterfaceDeletionFailed,         "Could not delete auto-tunnel interface")
ERROR(kmDNSHelperInvalidInterfaceState,           "Invalid interface state requested")
ERROR(kmDNSHelperInvalidServerState,              "Invalid server state requested")
ERROR(kmDNSHelperRacoonConfigCreationFailed,      "Could not create racoon configuration file")
ERROR(kmDNSHelperRacoonStartFailed,               "Could not start racoon")
ERROR(kmDNSHelperRacoonNotificationFailed,        "Could not notify racoon")
ERROR(kmDNSHelperInvalidTunnelSetKeysOperation,   "Invalid tunnel setkey operation requested")
ERROR(kmDNSHelperInvalidNetworkAddress,           "Invalid network address")
ERROR(kmDNSHelperRouteAdditionFailed,             "Could not add route")
ERROR(kmDNSHelperRouteDeletionFailed,             "Could not remove route")
ERROR(kmDNSHelperRoutingSocketCreationFailed,     "Could not create routing socket")
ERROR(kmDNSHelperDatagramSocketCreationFailed,    "Could not create datagram socket")
ERROR(kmDNSHelperIPsecPolicyCreationFailed,       "Could not create IPsec policy")
ERROR(kmDNSHelperIPsecPolicySetFailed,            "Could not set IPsec policy")
ERROR(kmDNSHelperIPsecRemoveSAFailed,             "Could not remove IPsec SA")
ERROR(kmDNSHelperIPsecPolicySocketCreationFailed, "Could not create IPsec policy socket")
ERROR(kmDNSHelperIPsecDisabled,                   "IPSec support was not compiled in to the helper")
