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

 * 
 * NOTE:
 * 
 * These .Net APIs are a work in progress, currently being discussed and refined.
 * If you plan to build an application based on these APIs, you may wish to
 * statically link this code into your application, or otherwise distribute
 * the DLL so that it installs into the same folder as your application
 * (not into any common system area where it might interfere with other
 * applications using a future completed version of these APIs).
 * If you plan to do this, please be sure to inform us by sending email
 * to bonjour@apple.com to let us know.
 * You may want to discuss what you're doing on the Bonjour mailing
 * list to see if others in similar positions have any suggestions for you:
 * 
 * <http://lists.apple.com/bonjour-dev/>
 * 

    Change History (most recent first):

$Log: dnssd_NET.h,v $
Revision 1.9  2006/08/14 23:25:43  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.8  2005/02/10 22:35:33  cheshire
<rdar://problem/3727944> Update name

Revision 1.7  2004/12/16 19:56:12  cheshire
Update comments

Revision 1.6  2004/09/20 22:47:06  cheshire
Add cautionary comment

Revision 1.5  2004/09/16 18:16:27  shersche
Cleanup to parameter names
Submitted by: prepin@gmail.com

Revision 1.4  2004/09/13 19:35:57  shersche
<rdar://problem/3798941> Add Apple.DNSSD namespace to MC++ wrapper class
<rdar://problem/3798950> Change all instances of unsigned short to int
Bug #: 3798941, 3798950

Revision 1.3  2004/07/27 07:12:10  shersche
make TextRecord an instantiable class object

Revision 1.2  2004/07/19 07:48:34  shersche
fix bug in DNSService.Register when passing in NULL text record, add TextRecord APIs

Revision 1.1  2004/06/26 04:01:22  shersche
Initial revision


 */
    
#pragma once

#include <dns_sd.h>
#include <vcclr.h>
#include <memory>
#include <winsock2.h>

using namespace System;
using namespace System::Net;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;
using namespace System::Collections;


namespace Apple
{
	namespace DNSSD
	{
		public __gc class ServiceRef;

		public __value enum ServiceFlags : int
		{
			MoreComing			=	1,
			/* MoreComing indicates to a callback that at least one more result is
				* queued and will be delivered following immediately after this one.
				* Applications should not update their UI to display browse
				* results when the MoreComing flag is set, because this would
				* result in a great deal of ugly flickering on the screen.
				* Applications should instead wait until until MoreComing is not set,
				* and then update their UI.
				* When MoreComing is not set, that doesn't mean there will be no more
				* answers EVER, just that there are no more answers immediately
				* available right now at this instant. If more answers become available
				* in the future they will be delivered as usual.
				*/

			Add					=	2,
			Default				=	4,
			/* Flags for domain enumeration and browse/query reply callbacks.
				* "Default" applies only to enumeration and is only valid in
				* conjuction with "Add".  An enumeration callback with the "Add"
				* flag NOT set indicates a "Remove", i.e. the domain is no longer
				* valid.
				*/

			NoAutoRename		=	8,
			/* Flag for specifying renaming behavior on name conflict when registering
				* non-shared records. By default, name conflicts are automatically handled
				* by renaming the service.  NoAutoRename overrides this behavior - with this
				* flag set, name conflicts will result in a callback.  The NoAutorename flag
				* is only valid if a name is explicitly specified when registering a service
				* (i.e. the default name is not used.)
				*/

			Shared				=	16,
			Unique				=	32,
			/* Flag for registering individual records on a connected
				* DNSServiceRef.  Shared indicates that there may be multiple records
				* with this name on the network (e.g. PTR records).  Unique indicates that
	the
				* record's name is to be unique on the network (e.g. SRV records).
				*/

			BrowseDomains		=	64,
			RegistrationDomains	=	128,
			/* Flags for specifying domain enumeration type in DNSServiceEnumerateDomain
	s.
				* BrowseDomains enumerates domains recommended for browsing, RegistrationDo
	mains
				* enumerates domains recommended for registration.
				*/
		};


		public __value enum ErrorCode : int
		{
			NoError				=	0,
			Unknown				=	-65537,
			NoSuchName			=	-65538,
			NoMemory			=	-65539,
			BadParam			=	-65540,
			BadReference		=	-65541,
			BadState			=	-65542,
			BadFlags			=	-65543,
			Unsupported			=	-65544,
			AlreadyRegistered	=	-65547,
			NameConflict		=	-65548,
			Invalid				=	-65549,
			Incompatible		=	-65551,
			BadinterfaceIndex	=	-65552

			/*
				* mDNS Error codes are in the range
				* FFFE FF00 (-65792) to FFFE FFFF (-65537)
				*/
		};

		public __gc class DNSServiceException
		:
			public Exception
		{
		public:

			DNSServiceException
				(
				int err
				);

			DNSServiceException
				(	
				String				*	message,
				System::Exception	*	innerException
				);

			int err;
		};


		/*
		* class RecordRef
		*
		* This is a thin MC++ class facade on top of a DNSRecordRef
		*/
		public __gc class RecordRef
		{
		public:

			RecordRef()
			{
				m_impl = new RecordRefImpl;
				m_impl->m_ref = NULL;
			}

			~RecordRef()
			{
				delete m_impl;
			}

			__nogc class RecordRefImpl
			{
			public:

				DNSRecordRef m_ref;
			};

			RecordRefImpl * m_impl;
		};			


		/*
		* class ServiceRef
		*
		* This is a thin MC++ class facade on top of a DNSServiceRef
		*/
		public __gc class ServiceRef : public IDisposable
		{
		public:

			ServiceRef(Object * callback);

			~ServiceRef();

			/*
			* This does an underlying DNSServiceRefDeallocate().  After
			* calling Dispose, the ServiceRef is no longer usable.
			*/
			void
			Dispose();

			/*
			* Internal - Dispatch an EnumerateDomains callback
			*/
			void
			EnumerateDomainsDispatch
				(
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	replyDomain
				);

			/*
			* Internal - Dispatch a Register callback
			*/
			void
			RegisterDispatch
				(
				ServiceFlags	flags,
				ErrorCode		errorCode,
 				String		*	name,
				String		*	regtype,
				String		*	domain
				);

			/*
			* Internal - Dispatch a Browse callback
			*/
			void
			BrowseDispatch
				(
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	serviceName,
				String		*	regtype,
				String		*	replyDomain
				);

			/*
			* Internal - Dispatch a Resolve callback
			*/
			void
			ResolveDispatch
				(
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	fullname,
				String		*	hosttarget,
				int				port,
				Byte			txtRecord[]
				);
			
			/*
			* Internal - Dispatch a RegisterRecord callback
			*/
			void
			RegisterRecordDispatch
				(
				ServiceFlags	flags,
				ErrorCode		errorCode,
				RecordRef	*	record
				);

			/*
			* Internal - Dispatch a QueryRecord callback
			*/
			void
			QueryRecordDispatch
				(
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	fullname,
				int				rrtype,
				int				rrclass,
				Byte			rdata[],
				int				ttl
				);

			/*
			* Internal - A non managed class to wrap a DNSServiceRef
			*/
			__nogc class ServiceRefImpl
			{
			public:

				ServiceRefImpl
					(
					ServiceRef * outer
					);

				~ServiceRefImpl();

				/*
				* Sets up events for threaded operation
				*/
				void
				SetupEvents();

				/*
				* Main processing thread
				*/
				void
				ProcessingThread();

				/*
				* Calls DNSServiceRefDeallocate()
				*/
				void
				Dispose();

				/*
				* Called from dnssd.dll
				*/
				static void DNSSD_API
				EnumerateDomainsCallback
					(
					DNSServiceRef			sdRef,
					DNSServiceFlags			flags,
					uint32_t				interfaceIndex,
					DNSServiceErrorType		errorCode,
					const char			*	replyDomain,
					void				*	context
					);

				static void DNSSD_API
				RegisterCallback
					(
					DNSServiceRef			ref,
					DNSServiceFlags			flags,
					DNSServiceErrorType		errorCode,
 					const char			*	name,
					const char			*	regtype,
					const char			*	domain,
					void				*	context
					);

				static void DNSSD_API
				BrowseCallback
					(
					DNSServiceRef			sdRef,
   					DNSServiceFlags			flags,
					uint32_t				interfaceIndex,
					DNSServiceErrorType		errorCode,
					const char			*	serviceName,
					const char			*	regtype,
					const char			*	replyDomain,
					void				*	context
					);

				static void DNSSD_API
				ResolveCallback
					(
					DNSServiceRef			sdRef,
					DNSServiceFlags			flags,
					uint32_t				interfaceIndex,
					DNSServiceErrorType		errorCode,
					const char			*	fullname,
					const char			*	hosttarget,
					uint16_t				notAnIntPort,
					uint16_t				txtLen,
					const char			*	txtRecord,
					void				*	context
					);

				static void DNSSD_API
				RegisterRecordCallback
					( 
					DNSServiceRef		sdRef,
					DNSRecordRef		RecordRef,
					DNSServiceFlags		flags,
					DNSServiceErrorType	errorCode,
					void			*	context
					);

				static void DNSSD_API
				QueryRecordCallback
					(
					DNSServiceRef			DNSServiceRef,
					DNSServiceFlags			flags,
					uint32_t				interfaceIndex,
					DNSServiceErrorType		errorCode,
					const char			*	fullname,
					uint16_t				rrtype,
					uint16_t				rrclass,
					uint16_t				rdlen,
					const void			*	rdata,
					uint32_t				ttl,
					void				*	context
					);

				SOCKET				m_socket;
				HANDLE				m_socketEvent;
				HANDLE				m_stopEvent;
				DWORD				m_threadId;
				bool				m_disposed;
				DNSServiceRef		m_ref;
				gcroot<ServiceRef*> m_outer;
			};

			void
			StartThread();

			void
			ProcessingThread();

			bool				m_bDisposed;
			Object			*	m_callback;
			Thread			*	m_thread;
			ServiceRefImpl	*	m_impl;
		};
			
		/*********************************************************************************************
		*
		*   TXT Record Construction Functions
		*
		*********************************************************************************************/

		/*
		* A typical calling sequence for TXT record construction is something like:
		*
		* DNSService.TextRecord tr = new DNSService.TextRecord(1024);
		* tr.SetValue();
		* tr.SetValue();
		* tr.SetValue();
		* ...
		* DNSServiceRegister( ... tr.GetLength(), tr.GetBytes() ... );
		*/


		/* TextRecord
		*
		* Opaque internal data type.
		* Note: Represents a DNS-SD TXT record.
		*/


		/* TextRecord::TextRecord()
		*
		* Creates a new empty TextRecord .
		*
		*/

		public __gc class TextRecord
		{
		public:

			TextRecord()
			{
				m_impl = new TextRecordImpl();
				TXTRecordCreate(&m_impl->m_ref, 0, NULL);
			}

			~TextRecord()
			{
				TXTRecordDeallocate(&m_impl->m_ref);
				delete m_impl;
			}

			__nogc class TextRecordImpl
			{
			public:

				TXTRecordRef m_ref;
			};

			TextRecordImpl * m_impl;


			/* SetValue()
			*
			* Adds a key (optionally with value) to a TextRecord. If the "key" already
			* exists in the TextRecord, then the current value will be replaced with
			* the new value.
			* Keys may exist in four states with respect to a given TXT record:
			*  - Absent (key does not appear at all)
			*  - Present with no value ("key" appears alone)
			*  - Present with empty value ("key=" appears in TXT record)
			*  - Present with non-empty value ("key=value" appears in TXT record)
			* For more details refer to "Data Syntax for DNS-SD TXT Records" in
			* <http://files.dns-sd.org/draft-cheshire-dnsext-dns-sd.txt>
			*
			* key:             A null-terminated string which only contains printable ASCII
			*                  values (0x20-0x7E), excluding '=' (0x3D). Keys should be
			*                  14 characters or less (not counting the terminating null).
			*
			* value:           Any binary value. For values that represent
			*                  textual data, UTF-8 is STRONGLY recommended.
			*                  For values that represent textual data, valueSize
			*                  should NOT include the terminating null (if any)
			*                  at the end of the string.
			*                  If NULL, then "key" will be added with no value.
			*                  If non-NULL but valueSize is zero, then "key=" will be
			*                  added with empty value.
			*
			* exceptions:      Throws kDNSServiceErr_Invalid if the "key" string contains
			*                  illegal characters.
			*                  Throws kDNSServiceErr_NoMemory if adding this key would
			*                  exceed the available storage.
			*/

			void
			SetValue
				(
				String	*	key,
				Byte		value[]  /* may be NULL */
				);


			/* RemoveValue()
			*
			* Removes a key from a TextRecord.  The "key" must be an
			* ASCII string which exists in the TextRecord.
			*
			* key:             A key name which exists in the TextRecord.
			*
			* exceptions:      Throws kDNSServiceErr_NoSuchKey if the "key" does not
			*                  exist in the TextRecord.
			*
			*/

			void
			RemoveValue
				(
				String	*	key
				);


			/* GetLength()
			*
			* Allows you to determine the length of the raw bytes within a TextRecord.
			*
			* return value :     Returns the size of the raw bytes inside a TextRecord
			*                  which you can pass directly to DNSServiceRegister() or
			*                  to DNSServiceUpdateRecord().
			*                  Returns 0 if the TextRecord is empty.
			*
			*/

			int
			GetLength
				(
				);


			/* GetBytes()
			*
			* Allows you to retrieve a pointer to the raw bytes within a TextRecord.
			*
			* return value:    Returns a pointer to the bytes inside the TextRecord
			*                  which you can pass directly to DNSServiceRegister() or
			*                  to DNSServiceUpdateRecord().
			*
			*/

			Byte
			GetBytes
				(
				) __gc[];


			/*********************************************************************************************
			*
			*   TXT Record Parsing Functions
			*
			*********************************************************************************************/

			/*
			* A typical calling sequence for TXT record parsing is something like:
			*
			* Receive TXT record data in DNSServiceResolve() callback
			* if (TXTRecordContainsKey(txtLen, txtRecord, "key")) then do something
			* val1ptr = DNSService.TextService.GetValue(txtRecord, "key1", &len1);
			* val2ptr = DNSService.TextService.GetValue(txtRecord, "key2", &len2);
			* ...
			* return;
			*
			*/

			/* ContainsKey()
			*
			* Allows you to determine if a given TXT Record contains a specified key.
			*
			* txtRecord:       Pointer to the received TXT Record bytes.
			*
			* key:             A null-terminated ASCII string containing the key name.
			*
			* return value:    Returns 1 if the TXT Record contains the specified key.
			*                  Otherwise, it returns 0.
			*
			*/

			static public bool
			ContainsKey
				(
				Byte		txtRecord[],
				String	*	key
				);


			/* GetValueBytes()
			*
			* Allows you to retrieve the value for a given key from a TXT Record.
			*
			* txtRecord:       Pointer to the received TXT Record bytes.
			*
			* key:             A null-terminated ASCII string containing the key name.
			*
			* return value:    Returns NULL if the key does not exist in this TXT record,
			*                  or exists with no value (to differentiate between
			*                  these two cases use ContainsKey()).
			*                  Returns byte array 
			*                  if the key exists with empty or non-empty value.
			*                  For empty value, length of byte array will be zero.
			*                  For non-empty value, it will be the length of value data.
			*/

			static public Byte
			GetValueBytes
				(
				Byte		txtRecord[],
				String	*	key
				) __gc[];


			/* GetCount()
			*
			* Returns the number of keys stored in the TXT Record.  The count
			* can be used with TXTRecordGetItemAtIndex() to iterate through the keys.
			*
			* txtRecord:       Pointer to the received TXT Record bytes.
			*
			* return value:    Returns the total number of keys in the TXT Record.
			*
			*/

			static public int
			GetCount
				(
				Byte	txtRecord[]
				);


			/* GetItemAtIndex()
			*
			* Allows you to retrieve a key name and value pointer, given an index into
			* a TXT Record.  Legal index values range from zero to TXTRecordGetCount()-1.
			* It's also possible to iterate through keys in a TXT record by simply
			* calling TXTRecordGetItemAtIndex() repeatedly, beginning with index zero
			* and increasing until TXTRecordGetItemAtIndex() returns kDNSServiceErr_Invalid.
			*
			* On return:
			* For keys with no value, *value is set to NULL and *valueLen is zero.
			* For keys with empty value, *value is non-NULL and *valueLen is zero.
			* For keys with non-empty value, *value is non-NULL and *valueLen is non-zero.
			*
			* txtRecord:       Pointer to the received TXT Record bytes.
			*
			* index:           An index into the TXT Record.
			*
			* key:             A string buffer used to store the key name.
			*                  On return, the buffer contains a string
			*                  giving the key name. DNS-SD TXT keys are usually
			*                  14 characters or less. 
			*
			* return value:    Record bytes that holds the value data.
			*
			* exceptions:      Throws kDNSServiceErr_Invalid if index is greater than
			*                  GetCount()-1.
			*/

			static public Byte
			GetItemAtIndex
				(
				Byte				txtRecord[],
				int					index,
				[Out] String	**	key
				) __gc[];
		};


		public __abstract __gc class DNSService
		{
		public:

			/*********************************************************************************************
			*
			* Domain Enumeration
			*
			*********************************************************************************************/

			/* DNSServiceEnumerateDomains()
			*
			* Asynchronously enumerate domains available for browsing and registration.
			* Currently, the only domain returned is "local.", but other domains will be returned in future.
			*
			* The enumeration MUST be cancelled via DNSServiceRefDeallocate() when no more domains
			* are to be found.
			*
			*
			* EnumerateDomainsReply Delegate
			*
			* This Delegate is invoked upon a reply from an EnumerateDomains call.
			*
			* sdRef:           The DNSServiceRef initialized by DNSServiceEnumerateDomains().
			*
			* flags:           Possible values are:
			*                  MoreComing
			*                  Add
			*                  Default
			*
			* interfaceIndex:  Specifies the interface on which the domain exists.  (The index for a given
			*                  interface is determined via the if_nametoindex() family of calls.)
			*
			* errorCode:       Will be NoError (0) on success, otherwise indicates
			*                  the failure that occurred (other parameters are undefined if errorCode is nonzero).
			*
			* replyDomain:     The name of the domain.
			*
			*/

			__delegate void
			EnumerateDomainsReply
				(
				ServiceRef	*	sdRef,
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	replyDomain
				);

			/* DNSServiceEnumerateDomains() Parameters:
			*
			*
			* flags:           Possible values are:
			*                  BrowseDomains to enumerate domains recommended for browsing.
			*                  RegistrationDomains to enumerate domains recommended
			*                  for registration.
			*
			* interfaceIndex:  If non-zero, specifies the interface on which to look for domains.
			*                  (the index for a given interface is determined via the if_nametoindex()
			*                  family of calls.)  Most applications will pass 0 to enumerate domains on
			*                  all interfaces.
			*
			* callback:        The delegate to be called when a domain is found or the call asynchronously
			*                  fails.
			*
			*
			* return value:    Returns initialize ServiceRef on succeses (any subsequent, asynchronous
			*                  errors are delivered to the delegate), otherwise throws an exception indicating
			*                  the error that occurred (the callback is not invoked and the ServiceRef
			*                  is not initialized.)
			*/

			static public ServiceRef*
			EnumerateDomains
				(
				int							flags,
				int							interfaceIndex,
				EnumerateDomainsReply	*	callback
				);

			/*********************************************************************************************
			*
			*  Service Registration
			*
			*********************************************************************************************/

			/* Register a service that is discovered via Browse() and Resolve() calls.
			* 
			* RegisterReply() Callback Parameters:
			*
			* sdRef:           The ServiceRef initialized by Register().
			*
			* flags:           Currently unused, reserved for future use.
			*
			* errorCode:       Will be NoError on success, otherwise will
			*                  indicate the failure that occurred (including name conflicts, if the
			*                  NoAutoRename flag was passed to the
			*                  callout.)  Other parameters are undefined if errorCode is nonzero.
			*
			* name:            The service name registered (if the application did not specify a name in
			*                  DNSServiceRegister(), this indicates what name was automatically chosen).
			*
			* regtype:         The type of service registered, as it was passed to the callout.
			*
			* domain:          The domain on which the service was registered (if the application did not
			*                  specify a domain in Register(), this indicates the default domain
			*                  on which the service was registered).
			*
			*/

			__delegate void
			RegisterReply
				(
				ServiceRef	*	sdRef,
				ServiceFlags	flags,
				ErrorCode		errorCode,
				String		*	name,
				String		*	regtype,
				String		*	domain
				);

			/* Register()  Parameters:
			*
			* flags:           Indicates the renaming behavior on name conflict (most applications
			*                  will pass 0).  See flag definitions above for details.
			*
			* interfaceIndex:  If non-zero, specifies the interface on which to register the service
			*                  (the index for a given interface is determined via the if_nametoindex()
			*                  family of calls.)  Most applications will pass 0 to register on all
			*                  available interfaces.  Pass -1 to register a service only on the local
			*                  machine (service will not be visible to remote hosts.)
			*
			* name:            If non-NULL, specifies the service name to be registered.
			*                  Most applications will not specify a name, in which case the
			*                  computer name is used (this name is communicated to the client via
			*                  the callback).
			*
			* regtype:         The service type followed by the protocol, separated by a dot
			*                  (e.g. "_ftp._tcp").  The transport protocol must be "_tcp" or "_udp".
			*                  New service types should be registered at htp://www.dns-sd.org/ServiceTypes.html.
			*
			* domain:          If non-NULL, specifies the domain on which to advertise the service.
			*                  Most applications will not specify a domain, instead automatically
			*                  registering in the default domain(s).
			*
			* host:            If non-NULL, specifies the SRV target host name.  Most applications
			*                  will not specify a host, instead automatically using the machine's
			*                  default host name(s).  Note that specifying a non-NULL host does NOT
			*                  create an address record for that host - the application is responsible
			*                  for ensuring that the appropriate address record exists, or creating it
			*                  via DNSServiceRegisterRecord().
			*
			* port:            The port, in host byte order, on which the service accepts connections.
			*                  Pass 0 for a "placeholder" service (i.e. a service that will not be discovered
			*                  by browsing, but will cause a name conflict if another client tries to
			*                  register that same name).  Most clients will not use placeholder services.
			*
			* txtRecord:       The txt record rdata.  May be NULL.  Note that a non-NULL txtRecord
			*                  MUST be a properly formatted DNS TXT record, i.e. <length byte> <data>
			*                  <length byte> <data> ...
			*
			* callback:        The delegate to be called when the registration completes or asynchronously
			*                  fails.  The client MAY pass NULL for the callback -  The client will NOT be notified
			*                  of the default values picked on its behalf, and the client will NOT be notified of any
			*                  asynchronous errors (e.g. out of memory errors, etc.) that may prevent the registration
			*                  of the service.  The client may NOT pass the NoAutoRename flag if the callback is NULL.
			*                  The client may still deregister the service at any time via DNSServiceRefDeallocate().
			*
			* return value:    Returns initialize ServiceRef (any subsequent, asynchronous
			*                  errors are delivered to the callback), otherwise throws an exception indicating
			*                  the error that occurred (the callback is never invoked and the DNSServiceRef
			*                  is not initialized.)
			*
			*/
			static public ServiceRef*
			Register
				(
				int					flags,
				int					interfaceIndex,
				String			*	name,
				String			*	regtype,
				String			*	domain,
				String			*	host,
				int					port,
				Byte				txtRecord[],
				RegisterReply	*	callback
				);

			/* AddRecord()
			*
			* Add a record to a registered service.  The name of the record will be the same as the
			* registered service's name.
			* The record can later be updated or deregistered by passing the RecordRef initialized
			* by this function to UpdateRecord() or RemoveRecord().
			*
			*
			* Parameters;
			*
			* sdRef:           A ServiceRef initialized by Register().
			*
			* RecordRef:       A pointer to an uninitialized RecordRef.  Upon succesfull completion of this
			*                  call, this ref may be passed to UpdateRecord() or RemoveRecord().
			*                  If the above ServiceRef is disposed, RecordRef is also
			*                  invalidated and may not be used further.
			*
			* flags:           Currently ignored, reserved for future use.
			*
			* rrtype:          The type of the record (e.g. TXT, SRV, etc), as defined in nameser.h.
			*
			* rdata:           The raw rdata to be contained in the added resource record.
			*
			* ttl:             The time to live of the resource record, in seconds.
			*
			* return value:    Returns initialized RecordRef, otherwise throws
			*                  an exception indicating the error that occurred (the RecordRef is not initialized).
			*/

			static public RecordRef*
			AddRecord
				(
				ServiceRef	*	sref,
				int				flags,
				int				rrtype,
				Byte			rdata[],
				int				ttl
				);

			/* UpdateRecord
			*
			* Update a registered resource record.  The record must either be:
			*   - The primary txt record of a service registered via Register()
			*   - A record added to a registered service via AddRecord()
			*   - An individual record registered by RegisterRecord()
			*
			*
			* Parameters:
			*
			* sdRef:           A ServiceRef that was initialized by Register()
			*                  or CreateConnection().
			*
			* RecordRef:       A RecordRef initialized by AddRecord, or NULL to update the
			*                  service's primary txt record.
			*
			* flags:           Currently ignored, reserved for future use.
			*
			* rdata:           The new rdata to be contained in the updated resource record.
			*
			* ttl:             The time to live of the updated resource record, in seconds.
			*
			* return value:    No return value on success, otherwise throws an exception
			*                  indicating the error that occurred.
			*/
			static public void
			UpdateRecord
				(
				ServiceRef	*	sref,
				RecordRef	*	record,
				int				flags,
				Byte			rdata[],
				int				ttl
				);

			/* RemoveRecord
			*
			* Remove a record previously added to a service record set via AddRecord(), or deregister
			* an record registered individually via RegisterRecord().
			*
			* Parameters:
			*
			* sdRef:           A ServiceRef initialized by Register() (if the
			*                  record being removed was registered via AddRecord()) or by
			*                  CreateConnection() (if the record being removed was registered via
			*                  RegisterRecord()).
			*
			* recordRef:       A RecordRef initialized by a successful call to AddRecord()
			*                  or RegisterRecord().
			*
			* flags:           Currently ignored, reserved for future use.
			*
			* return value:    Nothing on success, otherwise throws an
			*                  exception indicating the error that occurred.
			*/

			static public void
			RemoveRecord
							(
							ServiceRef	*	sref,
							RecordRef	*	record,	
							int				flags
							);

			/*********************************************************************************************
			*
			*  Service Discovery
			*
			*********************************************************************************************/

			/* Browse for instances of a service.
			*
			*
			* BrowseReply() Parameters:
			*
			* sdRef:           The DNSServiceRef initialized by Browse().
			*
			* flags:           Possible values are MoreComing and Add.
			*                  See flag definitions for details.
			*
			* interfaceIndex:  The interface on which the service is advertised.  This index should
			*                  be passed to Resolve() when resolving the service.
			*
			* errorCode:       Will be NoError (0) on success, otherwise will
			*                  indicate the failure that occurred.  Other parameters are undefined if
			*                  the errorCode is nonzero.
			*
			* serviceName:     The service name discovered.
			*
			* regtype:         The service type, as passed in to Browse().
			* 
			* domain:          The domain on which the service was discovered (if the application did not
			*                  specify a domain in Browse(), this indicates the domain on which the
			*                  service was discovered.)
			*
			*/

			__delegate void
			BrowseReply
				(
				ServiceRef	*	sdRef,
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	name,
				String		*	type,
				String		*	domain
				);

			/* DNSServiceBrowse() Parameters:
			*
			* sdRef:           A pointer to an uninitialized ServiceRef.  Call ServiceRef.Dispose()
			*                  to terminate the browse.
			*
			* flags:           Currently ignored, reserved for future use.
			*
			* interfaceIndex:  If non-zero, specifies the interface on which to browse for services
			*                  (the index for a given interface is determined via the if_nametoindex()
			*                  family of calls.)  Most applications will pass 0 to browse on all available
			*                  interfaces.  Pass -1 to only browse for services provided on the local host.
			*
			* regtype:         The service type being browsed for followed by the protocol, separated by a
			*                  dot (e.g. "_ftp._tcp").  The transport protocol must be "_tcp" or "_udp".
			*
			* domain:          If non-NULL, specifies the domain on which to browse for services.
			*                  Most applications will not specify a domain, instead browsing on the
			*                  default domain(s).
			*
			* callback:        The delegate to be called when an instance of the service being browsed for
			*                  is found, or if the call asynchronously fails.
			*
			* return value:    Returns initialized ServiceRef on succeses (any subsequent, asynchronous
			*                  errors are delivered to the callback), otherwise throws an exception indicating
			*                  the error that occurred (the callback is not invoked and the ServiceRef
			*                  is not initialized.)
			*/

			static public ServiceRef*
			Browse
				(
				int				flags,
				int				interfaceIndex,
				String		*	regtype,
				String		*	domain,
				BrowseReply	*	callback
				);

			/* ResolveReply() Parameters:
			*
			* Resolve a service name discovered via Browse() to a target host name, port number, and
			* txt record.
			*
			* Note: Applications should NOT use Resolve() solely for txt record monitoring - use
			* QueryRecord() instead, as it is more efficient for this task.
			*
			* Note: When the desired results have been returned, the client MUST terminate the resolve by calling
			* ServiceRef.Dispose().
			*
			* Note: Resolve() behaves correctly for typical services that have a single SRV record and
			* a single TXT record (the TXT record may be empty.)  To resolve non-standard services with multiple
			* SRV or TXT records, QueryRecord() should be used.
			*
			* ResolveReply Callback Parameters:
			*
			* sdRef:           The DNSServiceRef initialized by Resolve().
			*
			* flags:           Currently unused, reserved for future use.
			*
			* interfaceIndex:  The interface on which the service was resolved.
			*
			* errorCode:       Will be NoError (0) on success, otherwise will
			*                  indicate the failure that occurred.  Other parameters are undefined if
			*                  the errorCode is nonzero.
			*
			* fullname:        The full service domain name, in the form <servicename>.<protocol>.<domain>.
			*                  (Any literal dots (".") are escaped with a backslash ("\."), and literal
			*                  backslashes are escaped with a second backslash ("\\"), e.g. a web server
			*                  named "Dr. Pepper" would have the fullname  "Dr\.\032Pepper._http._tcp.local.").
			*                  This is the appropriate format to pass to standard system DNS APIs such as
			*                  res_query(), or to the special-purpose functions included in this API that
			*                  take fullname parameters.
			*
			* hosttarget:      The target hostname of the machine providing the service.  This name can
			*                  be passed to functions like gethostbyname() to identify the host's IP address.
			*
			* port:            The port, in host byte order, on which connections are accepted for this service.
			*
			* txtRecord:       The service's primary txt record, in standard txt record format.
			*
			*/

			__delegate void
			ResolveReply
				(	
				ServiceRef	*	sdRef,  
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,
				String		*	fullName,
				String		*	hostName,
				int				port,
				Byte			txtRecord[]
				);

			/* Resolve() Parameters
			*
			* flags:           Currently ignored, reserved for future use.
			*
			* interfaceIndex:  The interface on which to resolve the service.  The client should
			*                  pass the interface on which the servicename was discovered, i.e.
			*                  the interfaceIndex passed to the DNSServiceBrowseReply callback,
			*                  or 0 to resolve the named service on all available interfaces.
			*
			* name:            The servicename to be resolved.
			*
			* regtype:         The service type being resolved followed by the protocol, separated by a
			*                  dot (e.g. "_ftp._tcp").  The transport protocol must be "_tcp" or "_udp".
			*
			* domain:          The domain on which the service is registered, i.e. the domain passed
			*                  to the DNSServiceBrowseReply callback.
			*
			* callback:        The delegate to be called when a result is found, or if the call
			*                  asynchronously fails.
			*
			*
			* return value:    Returns initialized ServiceRef on succeses (any subsequent, asynchronous
			*                  errors are delivered to the callback), otherwise throws an exception indicating
			*                  the error that occurred (the callback is never invoked and the DNSServiceRef
			*                  is not initialized.)
			*/

			static public ServiceRef*
			Resolve
				(
				int					flags,
				int					interfaceIndex,
				String			*	name,
				String			*	regtype,
				String			*	domain,
				ResolveReply	*	callback
				);

			/*********************************************************************************************
			*
			*  Special Purpose Calls (most applications will not use these)
			*
			*********************************************************************************************/

			/* CreateConnection/RegisterRecord
			*
			* Register an individual resource record on a connected ServiceRef.
			*
			* Note that name conflicts occurring for records registered via this call must be handled
			* by the client in the callback.
			*
			*
			* RecordReply() parameters:
			*
			* sdRef:           The connected ServiceRef initialized by
			*                  CreateConnection().
			*
			* RecordRef:       The RecordRef initialized by RegisterRecord().  If the above
			*                  ServiceRef.Dispose is called, this RecordRef is
			*                  invalidated, and may not be used further.
			*
			* flags:           Currently unused, reserved for future use.
			*
			* errorCode:       Will be NoError on success, otherwise will
			*                  indicate the failure that occurred (including name conflicts.)
			*                  Other parameters are undefined if errorCode is nonzero.
			*
			*/

			__delegate void
			RegisterRecordReply
				(
				ServiceRef	*	sdRef,
				ServiceFlags	flags,
				ErrorCode		errorCode,
				RecordRef	*	record
				);

			/* CreateConnection()
			*
			* Create a connection to the daemon allowing efficient registration of
			* multiple individual records.
			*
			*
			* Parameters:
			*
			* callback:        The delegate to be called when a result is found, or if the call
			*                  asynchronously fails (e.g. because of a name conflict.)
			*
			* return value:    Returns initialize ServiceRef on success, otherwise throws
			*                  an exception indicating the specific failure that occurred (in which
			*                  case the ServiceRef is not initialized).
			*/

			static public ServiceRef*
			CreateConnection
				(
				RegisterRecordReply * callback
				);


			/* RegisterRecord() Parameters:
			*
			* sdRef:           A ServiceRef initialized by CreateConnection().
			*
			* RecordRef:       A pointer to an uninitialized RecordRef.  Upon succesfull completion of this
			*                  call, this ref may be passed to UpdateRecord() or RemoveRecord().
			*                  (To deregister ALL records registered on a single connected ServiceRef
			*                  and deallocate each of their corresponding RecordRefs, call
			*                  ServiceRef.Dispose()).
			*
			* flags:           Possible values are Shared or Unique
			*                  (see flag type definitions for details).
			*
			* interfaceIndex:  If non-zero, specifies the interface on which to register the record
			*                  (the index for a given interface is determined via the if_nametoindex()
			*                  family of calls.)  Passing 0 causes the record to be registered on all interfaces.
			*                  Passing -1 causes the record to only be visible on the local host.
			*
			* fullname:        The full domain name of the resource record.
			*
			* rrtype:          The numerical type of the resource record (e.g. PTR, SRV, etc), as defined
			*                  in nameser.h.
			*
			* rrclass:         The class of the resource record, as defined in nameser.h (usually 1 for the
			*                  Internet class).
			*
			* rdata:           A pointer to the raw rdata, as it is to appear in the DNS record.
			*
			* ttl:             The time to live of the resource record, in seconds.
			*
			*
			* return value:    Returns initialize RecordRef on succeses (any subsequent, asynchronous
			*                  errors are delivered to the callback), otherwise throws an exception indicating
			*                  the error that occurred (the callback is never invoked and the RecordRef is
			*                  not initialized.)
			*/
			static public RecordRef*
			RegisterRecord
				(
				ServiceRef			*	sdRef,
				ServiceFlags			flags,
				int						interfaceIndex,
				String				*	fullname,
				int						rrtype,
				int						rrclass,
				Byte					rdata[],
				int						ttl
				);


			/* DNSServiceQueryRecord
			*
			* Query for an arbitrary DNS record.
			*
			*
			* QueryRecordReply() Delegate Parameters:
			*
			* sdRef:           The ServiceRef initialized by QueryRecord().
			*
			* flags:           Possible values are MoreComing and
			*                  Add.  The Add flag is NOT set for PTR records
			*                  with a ttl of 0, i.e. "Remove" events.
			*
			* interfaceIndex:  The interface on which the query was resolved (the index for a given
			*                  interface is determined via the if_nametoindex() family of calls).
			*
			* errorCode:       Will be kDNSServiceErr_NoError on success, otherwise will
			*                  indicate the failure that occurred.  Other parameters are undefined if
			*                  errorCode is nonzero.
			*
			* fullname:        The resource record's full domain name.
			*
			* rrtype:          The resource record's type (e.g. PTR, SRV, etc) as defined in nameser.h.
			*
			* rrclass:         The class of the resource record, as defined in nameser.h (usually 1).
			*
			* rdata:           The raw rdata of the resource record.
			*
			* ttl:             The resource record's time to live, in seconds.
			*
			*/

			__delegate void
			QueryRecordReply
				(
				ServiceRef	*	sdRef,
				ServiceFlags	flags,
				int				interfaceIndex,
				ErrorCode		errorCode,	
				String		*	fullName,
				int				rrtype,
				int				rrclass,
				Byte			rdata[],
				int				ttl
				);

			/* QueryRecord() Parameters:
			*
			* flags:           Pass LongLivedQuery to create a "long-lived" unicast
			*                  query in a non-local domain.  Without setting this flag, unicast queries
			*                  will be one-shot - that is, only answers available at the time of the call
			*                  will be returned.  By setting this flag, answers (including Add and Remove
			*                  events) that become available after the initial call is made will generate
			*                  callbacks.  This flag has no effect on link-local multicast queries.
			*
			* interfaceIndex:  If non-zero, specifies the interface on which to issue the query
			*                  (the index for a given interface is determined via the if_nametoindex()
			*                  family of calls.)  Passing 0 causes the name to be queried for on all
			*                  interfaces.  Passing -1 causes the name to be queried for only on the
			*                  local host.
			*
			* fullname:        The full domain name of the resource record to be queried for.
			*
			* rrtype:          The numerical type of the resource record to be queried for (e.g. PTR, SRV, etc)
			*                  as defined in nameser.h.
			*
			* rrclass:         The class of the resource record, as defined in nameser.h
			*                  (usually 1 for the Internet class).
			*
			* callback:        The delegate to be called when a result is found, or if the call
			*                  asynchronously fails.
			*
			*
			* return value:    Returns initialized ServiceRef on succeses (any subsequent, asynchronous
			*                  errors are delivered to the callback), otherwise throws an exception indicating
			*                  the error that occurred (the callback is never invoked and the ServiceRef
			*                  is not initialized.)
			*/

			static public ServiceRef*
			QueryRecord
				(
				ServiceFlags			flags,
				int						interfaceIndex,
				String				*	fullname,
				int						rrtype,
				int						rrclass,
				QueryRecordReply	*	callback
				);

			/* ReconfirmRecord
			*
			* Instruct the daemon to verify the validity of a resource record that appears to
			* be out of date (e.g. because tcp connection to a service's target failed.)
			* Causes the record to be flushed from the daemon's cache (as well as all other
			* daemons' caches on the network) if the record is determined to be invalid.
			*
			* Parameters:
			*
			* flags:           Currently unused, reserved for future use.
			*
			* fullname:        The resource record's full domain name.
			*
			* rrtype:          The resource record's type (e.g. PTR, SRV, etc) as defined in nameser.h.
			*
			* rrclass:         The class of the resource record, as defined in nameser.h (usually 1).
			*
			* rdata:           The raw rdata of the resource record.
			*
			*/
			static public void
			ReconfirmRecord
				(
				ServiceFlags	flags,
				int				interfaceIndex,
				String		*	fullname,
				int				rrtype,
				int				rrclass,
				Byte			rdata[]
				);
		};
	}
}
