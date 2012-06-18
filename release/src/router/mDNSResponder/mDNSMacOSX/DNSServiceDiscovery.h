/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 */

/*!	@header		DNS Service Discovery (Deprecated Mach-based API)
 *
 * @discussion	This section describes the functions, callbacks, and data structures that 
 *				make up the DNS Service Discovery API.
 *
 *				The DNS Service Discovery API is part of Bonjour, Apple's implementation of 
 *				zero-configuration networking (ZEROCONF).
 *
 *				Bonjour allows you to register a network service, such as a 
 *				printer or file server, so that it can be found by name or browsed 
 *				for by service type and domain. Using Bonjour, applications can 
 *				discover what services are available on the network, along with 
 *				all necessary access information-such as name, IP address, and port 
 *				number-for a given service.
 *
 *				In effect, Bonjour combines the functions of a local DNS server 
 *				and AppleTalk. Bonjour allows applications to provide user-friendly printer 
 *				and server browsing, among other things, over standard IP networks. 
 *				This behavior is a result of combining protocols such as multicast and DNS 
 *				to add new functionality to the network (such as multicast DNS).
 *
 *				Bonjour gives applications easy access to services over local IP 
 *				networks without requiring the service or the application to support 
 *				an AppleTalk or a Netbeui stack, and without requiring a DNS server 
 *				for the local network.
 *
 *				Note that this API was deprecated in Mac OS X 10.3, and replaced
 *				by the portable cross-platform /usr/include/dns_sd.h API.
 */

#ifndef __DNS_SERVICE_DISCOVERY_H
#define __DNS_SERVICE_DISCOVERY_H

#include <mach/mach_types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/cdefs.h>

#include <netinet/in.h>

#include <AvailabilityMacros.h>

__BEGIN_DECLS

/* Opaque internal data type */
typedef struct _dns_service_discovery_t * dns_service_discovery_ref;

/* possible reply flags values */

enum {
    kDNSServiceDiscoveryNoFlags			= 0,
    kDNSServiceDiscoveryMoreRepliesImmediately	= 1 << 0,
};


/* possible error code values */
typedef enum
{
    kDNSServiceDiscoveryWaiting     = 1,
    kDNSServiceDiscoveryNoError     = 0,
      // mDNS Error codes are in the range
      // FFFE FF00 (-65792) to FFFE FFFF (-65537)
    kDNSServiceDiscoveryUnknownErr        = -65537,       // 0xFFFE FFFF
    kDNSServiceDiscoveryNoSuchNameErr     = -65538,
    kDNSServiceDiscoveryNoMemoryErr       = -65539,
    kDNSServiceDiscoveryBadParamErr       = -65540,
    kDNSServiceDiscoveryBadReferenceErr   = -65541,
    kDNSServiceDiscoveryBadStateErr       = -65542,
    kDNSServiceDiscoveryBadFlagsErr       = -65543,
    kDNSServiceDiscoveryUnsupportedErr    = -65544,
    kDNSServiceDiscoveryNotInitializedErr = -65545,
    kDNSServiceDiscoveryNoCache           = -65546,
    kDNSServiceDiscoveryAlreadyRegistered = -65547,
    kDNSServiceDiscoveryNameConflict      = -65548,
    kDNSServiceDiscoveryInvalid           = -65549,
    kDNSServiceDiscoveryMemFree           = -65792        // 0xFFFE FF00
} DNSServiceRegistrationReplyErrorType;

typedef uint32_t DNSRecordReference;


/*!
@function DNSServiceResolver_handleReply
 @discussion This function should be called with the Mach message sent
 to the port returned by the call to DNSServiceResolverResolve.
 The reply message will be interpreted and will result in a
 call to the specified callout function.
 @param replyMsg The Mach message.
 */
void DNSServiceDiscovery_handleReply(void *replyMsg);

/* Service Registration */

typedef void (*DNSServiceRegistrationReply) (
    DNSServiceRegistrationReplyErrorType 		errorCode,
    void										*context
);

/*!
@function DNSServiceRegistrationCreate
    @discussion Register a named service with DNS Service Discovery
    @param name The name of this service instance (e.g. "Steve's Printer")
    @param regtype The service type (e.g. "_printer._tcp." -- see
        RFC 2782 (DNS SRV) and <http://www.iana.org/assignments/port-numbers>)
    @param domain The domain in which to register the service (e.g. "apple.com.")
    @param port The local port on which this service is being offered (in network byte order)
    @param txtRecord Optional protocol-specific additional information
    @param callBack The DNSServiceRegistrationReply function to be called
    @param context A user specified context which will be passed to the callout function.
    @result A dns_registration_t
*/
dns_service_discovery_ref DNSServiceRegistrationCreate
(
    const char 		*name,
    const char 		*regtype,
    const char 		*domain,
    uint16_t		port,
    const char 		*txtRecord,
    DNSServiceRegistrationReply callBack,
    void		*context
) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/***************************************************************************/
/*   DNS Domain Enumeration   */

typedef enum
{
    DNSServiceDomainEnumerationReplyAddDomain,			// Domain found
    DNSServiceDomainEnumerationReplyAddDomainDefault,		// Domain found (and should be selected by default)
    DNSServiceDomainEnumerationReplyRemoveDomain,			// Domain has been removed from network
} DNSServiceDomainEnumerationReplyResultType;

typedef enum
{
    DNSServiceDiscoverReplyFlagsFinished,
    DNSServiceDiscoverReplyFlagsMoreComing,
} DNSServiceDiscoveryReplyFlags;

typedef void (*DNSServiceDomainEnumerationReply) (
    DNSServiceDomainEnumerationReplyResultType 			resultType,		// One of DNSServiceDomainEnumerationReplyResultType
    const char  						*replyDomain,
    DNSServiceDiscoveryReplyFlags 		flags,			// DNS Service Discovery reply flags information
    void								*context		
);

/*!
    @function DNSServiceDomainEnumerationCreate
    @discussion Asynchronously create a DNS Domain Enumerator
    @param registrationDomains A boolean indicating whether you are looking
        for recommended registration domains
        (e.g. equivalent to the AppleTalk zone list in the AppleTalk Control Panel)
        or recommended browsing domains
        (e.g. equivalent to the AppleTalk zone list in the Chooser).
    @param callBack The function to be called when domains are found or removed
    @param context A user specified context which will be passed to the callout function.
    @result A dns_registration_t
*/
dns_service_discovery_ref DNSServiceDomainEnumerationCreate
(
    int 		registrationDomains,
    DNSServiceDomainEnumerationReply	callBack,
    void		*context
) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/***************************************************************************/
/*   DNS Service Browser   */

typedef enum
{
    DNSServiceBrowserReplyAddInstance,	// Instance of service found
    DNSServiceBrowserReplyRemoveInstance	// Instance has been removed from network
} DNSServiceBrowserReplyResultType;

typedef void (*DNSServiceBrowserReply) (
    DNSServiceBrowserReplyResultType 			resultType,		// One of DNSServiceBrowserReplyResultType
    const char  	*replyName,
    const char  	*replyType,
    const char  	*replyDomain,
    DNSServiceDiscoveryReplyFlags 				flags,			// DNS Service Discovery reply flags information
    void			*context
);

/*!
    @function DNSServiceBrowserCreate
    @discussion Asynchronously create a DNS Service browser
    @param regtype The type of service
    @param domain The domain in which to find the service
    @param callBack The function to be called when service instances are found or removed
    @param context A user specified context which will be passed to the callout function.
    @result A dns_registration_t
*/
dns_service_discovery_ref DNSServiceBrowserCreate
(
    const char 		*regtype,
    const char 		*domain,
    DNSServiceBrowserReply	callBack,
    void		*context
) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/***************************************************************************/
/* Resolver requests */

typedef void (*DNSServiceResolverReply) (
    struct sockaddr 	*interface,		// Needed for scoped addresses like link-local
    struct sockaddr 	*address,
    const char 			*txtRecord,
    DNSServiceDiscoveryReplyFlags 				flags,			// DNS Service Discovery reply flags information
    void				*context
);

/*!
@function DNSServiceResolverResolve
    @discussion Resolved a named instance of a service to its address, port, and
        (optionally) other demultiplexing information contained in the TXT record.
    @param name The name of the service instance
    @param regtype The type of service
    @param domain The domain in which to find the service
    @param callBack The DNSServiceResolverReply function to be called when the specified
        address has been resolved.
    @param context A user specified context which will be passed to the callout function.
    @result A dns_registration_t
*/

dns_service_discovery_ref DNSServiceResolverResolve
(
    const char 		*name,
    const char 		*regtype,
    const char 		*domain,
    DNSServiceResolverReply callBack,
    void		*context
) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/***************************************************************************/
/* Mach port accessor and deallocation */

/*!
    @function DNSServiceDiscoveryMachPort
    @discussion Returns the mach port for a dns_service_discovery_ref
    @param registration A dns_service_discovery_ref as returned from DNSServiceRegistrationCreate
    @result A mach reply port which will be sent messages as appropriate.
        These messages should be passed to the DNSServiceDiscovery_handleReply
        function.  A NULL value indicates that no address was
        specified or some other error occurred which prevented the
        resolution from being started.
*/
mach_port_t DNSServiceDiscoveryMachPort(dns_service_discovery_ref dnsServiceDiscovery) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/*!
    @function DNSServiceDiscoveryDeallocate
    @discussion Deallocates the DNS Service Discovery type / closes the connection to the server
    @param dnsServiceDiscovery A dns_service_discovery_ref as returned from a creation or enumeration call
    @result void
*/
void DNSServiceDiscoveryDeallocate(dns_service_discovery_ref dnsServiceDiscovery) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/***************************************************************************/
/* Registration updating */


/*!
    @function DNSServiceRegistrationAddRecord
    @discussion Request that the mDNS Responder add the DNS Record of a specific type
    @param dnsServiceDiscovery A dns_service_discovery_ref as returned from a DNSServiceRegistrationCreate call
    @param rrtype A standard DNS Resource Record Type, from http://www.iana.org/assignments/dns-parameters
    @param rdlen Length of the data
    @param rdata Opaque binary Resource Record data, up to 64 kB.
    @param ttl time to live for the added record.
    @result DNSRecordReference An opaque reference that can be passed to the update and remove record calls.  If an error occurs, this value will be zero or negative
*/
DNSRecordReference DNSServiceRegistrationAddRecord(dns_service_discovery_ref dnsServiceDiscovery, uint16_t rrtype, uint16_t rdlen, const char *rdata, uint32_t ttl) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/*!
    @function DNSServiceRegistrationUpdateRecord
    @discussion Request that the mDNS Responder add the DNS Record of a specific type
    @param dnsServiceDiscovery A dns_service_discovery_ref as returned from a DNSServiceRegistrationCreate call
    @param dnsRecordReference A dnsRecordReference as returned from a DNSServiceRegistrationAddRecord call
    @param rdlen Length of the data
    @param rdata Opaque binary Resource Record data, up to 64 kB.
    @param ttl time to live for the updated record.
    @result DNSServiceRegistrationReplyErrorType If an error occurs, this value will be non zero
*/
DNSServiceRegistrationReplyErrorType DNSServiceRegistrationUpdateRecord(dns_service_discovery_ref ref, DNSRecordReference reference, uint16_t rdlen, const char *rdata, uint32_t ttl) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;

/*!
    @function DNSServiceRegistrationRemoveRecord
    @discussion Request that the mDNS Responder remove the DNS Record(s) of a specific type
    @param dnsServiceDiscovery A dns_service_discovery_ref as returned from a DNSServiceRegistrationCreate call
    @param dnsRecordReference A dnsRecordReference as returned from a DNSServiceRegistrationAddRecord call
    @result DNSServiceRegistrationReplyErrorType If an error occurs, this value will be non zero
*/
DNSServiceRegistrationReplyErrorType DNSServiceRegistrationRemoveRecord(dns_service_discovery_ref ref, DNSRecordReference reference) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER_BUT_DEPRECATED_IN_MAC_OS_X_VERSION_10_3;


__END_DECLS

#endif	/* __DNS_SERVICE_DISCOVERY_H */
