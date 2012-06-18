/*
    File: ddnswriteconfig.m

    Abstract: Setuid root tool invoked by Preference Pane to perform
    privileged accesses to system configuration preferences and the system keychain.
    Invoked by PrivilegedOperations.c.

    Copyright: (c) Copyright 2005 Apple Computer, Inc. All rights reserved.

    Disclaimer: IMPORTANT: This Apple software is supplied to you by Apple Computer, Inc.
    ("Apple") in consideration of your agreement to the following terms, and your
    use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these terms.  If you do not agree with these terms,
    please do not use, install, modify or redistribute this Apple software.

    In consideration of your agreement to abide by the following terms, and subject
    to these terms, Apple grants you a personal, non-exclusive license, under Apple's
    copyrights in this original Apple software (the "Apple Software"), to use,
    reproduce, modify and redistribute the Apple Software, with or without
    modifications, in source and/or binary forms; provided that if you redistribute
    the Apple Software in its entirety and without modifications, you must retain
    this notice and the following text and disclaimers in all such redistributions of
    the Apple Software.  Neither the name, trademarks, service marks or logos of
    Apple Computer, Inc. may be used to endorse or promote products derived from the
    Apple Software without specific prior written permission from Apple.  Except as
    expressly stated in this notice, no other rights or licenses, express or implied,
    are granted by Apple herein, including but not limited to any patent rights that
    may be infringed by your derivative works or by other works in which the Apple
    Software may be incorporated.

    The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
    WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
    COMBINATION WITH YOUR PRODUCTS.

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
    OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
    (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Change History (most recent first):

$Log: ddnswriteconfig.m,v $
Revision 1.13  2008/11/04 20:08:44  cheshire
Use constant kDNSServiceMaxDomainName instead of literal value "1005"

Revision 1.12  2008/09/15 23:52:30  cheshire
<rdar://problem/6218902> mDNSResponder-177 fails to compile on Linux with .desc pseudo-op
Made __crashreporter_info__ symbol conditional, so we only use it for OS X build

Revision 1.11  2008/06/26 17:34:18  mkrochma
<rdar://problem/6030630> Pref pane destroying shared "system.preferences" authorization right

Revision 1.10  2007/11/30 23:43:04  cheshire
Fixed compile warning: declaration of 'access' shadows a global declaration

Revision 1.9  2007/09/18 19:09:02  cheshire
<rdar://problem/5489549> mDNSResponderHelper (and other binaries) missing SCCS version strings

Revision 1.8  2007/07/20 23:41:03  mkrochma
<rdar://problem/5348663> null deref in ddnswriteconfig

Revision 1.7  2007/03/07 00:49:00  cheshire
<rdar://problem/4618207> Security: ddnswriteconfig does not verify that authorization blob is of correct size

Revision 1.6  2007/02/09 00:39:06  cheshire
Fix compile warnings

Revision 1.5  2006/08/14 23:15:47  cheshire
Tidy up Change History comment

Revision 1.4  2005/06/04 04:47:47  cheshire
<rdar://problem/4138070> ddnswriteconfig (Bonjour PreferencePane) vulnerability
Remove self-installing capability of ddnswriteconfig

Revision 1.3  2005/02/16 00:17:35  cheshire
Don't create empty arrays -- CFArrayGetValueAtIndex(array,0) returns an essentially random (non-null)
result for empty arrays, which can lead to code crashing if it's not sufficiently defensive.

Revision 1.2  2005/02/10 22:35:20  cheshire
<rdar://problem/3727944> Update name

Revision 1.1  2005/02/05 01:59:19  cheshire
Add Preference Pane to facilitate testing of DDNS & wide-area features

*/


#import "PrivilegedOperations.h"
#import "ConfigurationRights.h"

#import <stdio.h>
#import <stdint.h>
#import <stdlib.h>
#import <unistd.h>
#import <fcntl.h>
#import <errno.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/mman.h>
#import <mach-o/dyld.h>
#import <dns_sd.h>
#import <AssertMacros.h>
#import <Security/Security.h>
#import <CoreServices/CoreServices.h>
#import <CoreFoundation/CoreFoundation.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <Foundation/Foundation.h>


static AuthorizationRef	gAuthRef = 0;

static OSStatus
WriteArrayToDynDNS(CFStringRef arrayKey, CFArrayRef domainArray)
{
    SCPreferencesRef	    store;
	OSStatus				err = noErr;
	CFDictionaryRef			origDict;
	CFMutableDictionaryRef	dict = NULL;
	Boolean					result;
	CFStringRef				scKey = CFSTR("/System/Network/DynamicDNS");
	

	// Add domain to the array member ("arrayKey") of the DynamicDNS dictionary
	// Will replace duplicate, at head of list
	// At this point, we only support a single-item list
	store = SCPreferencesCreate(NULL, CFSTR("com.apple.preference.bonjour"), NULL);
	require_action(store != NULL, SysConfigErr, err=paramErr;);
	require_action(true == SCPreferencesLock( store, true), LockFailed, err=coreFoundationUnknownErr;);

	origDict = SCPreferencesPathGetValue(store, scKey);
	if (origDict) {
		dict = CFDictionaryCreateMutableCopy(NULL, 0, origDict);
	}
    
	if (!dict) {
		dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	require_action( dict != NULL, NoDict, err=memFullErr;);

	if (CFArrayGetCount(domainArray) > 0) {
		CFDictionarySetValue(dict, arrayKey, domainArray);
	} else {
		CFDictionaryRemoveValue(dict, arrayKey);
	}
	
	result = SCPreferencesPathSetValue(store, scKey, dict);
	require_action(result, SCError, err=kernelPrivilegeErr;);

	result = SCPreferencesCommitChanges(store);
	require_action(result, SCError, err=kernelPrivilegeErr;);
	result = SCPreferencesApplyChanges(store);
	require_action(result, SCError, err=kernelPrivilegeErr;);

SCError:
	CFRelease(dict);
NoDict:
	SCPreferencesUnlock(store);
LockFailed:
	CFRelease(store);
SysConfigErr:
	return err;
}


static int
readTaggedBlock(int fd, u_int32_t *pTag, u_int32_t *pLen, char **ppBuff)
// Read tag, block len and block data from stream and return. Dealloc *ppBuff via free().
{
	ssize_t		num, len;
	u_int32_t	tag;
	int			result = 0;

	num = read(fd, &tag, sizeof tag);
	require_action(num == sizeof tag, GetTagFailed, result = -1;);
	num = read(fd, &len, sizeof len);
	require_action(num == sizeof len, GetLenFailed, result = -1;);

	*ppBuff = (char*) malloc( len);
	require_action(*ppBuff != NULL, AllocFailed, result = -1;);

	num = read(fd, *ppBuff, len);
	if (num == len) {
		*pTag = tag;
		*pLen = len;
	} else {
		free(*ppBuff);
		result = -1;
	}

AllocFailed:
GetLenFailed:
GetTagFailed:
	return result;
}



static int
SetAuthInfo( int fd)
{
	int				result = 0;
	u_int32_t		tag, len;
	char			*p;

	result = readTaggedBlock( fd, &tag, &len, &p);
	require( result == 0, ReadParamsFailed);
	require( len == sizeof(AuthorizationExternalForm), ReadParamsFailed);
	require( len == kAuthorizationExternalFormLength, ReadParamsFailed);

	if (gAuthRef != 0) {
		(void) AuthorizationFree(gAuthRef, kAuthorizationFlagDefaults);
		gAuthRef = 0;
	}

	result = AuthorizationCreateFromExternalForm((AuthorizationExternalForm*) p, &gAuthRef);

	free( p);
ReadParamsFailed:
	return result;
}


static int
HandleWriteDomain(int fd, int domainType)
{
	CFArrayRef      domainArray;
	CFDataRef       domainData;
	int				result = 0;
	u_int32_t		tag, len;
	char			*p;

	AuthorizationItem	scAuth = { UPDATE_SC_RIGHT, 0, NULL, 0 };
	AuthorizationRights	authSet = { 1, &scAuth };

	if (noErr != (result = AuthorizationCopyRights(gAuthRef, &authSet, NULL, (AuthorizationFlags)0, NULL)))
		return result;

	result = readTaggedBlock(fd, &tag, &len, &p);
	require(result == 0, ReadParamsFailed);

	domainData = CFDataCreate(NULL, (UInt8 *)p, len);
	domainArray = (CFArrayRef)[NSUnarchiver unarchiveObjectWithData:(NSData *)domainData];
	
    if (domainType) {
        result = WriteArrayToDynDNS(SC_DYNDNS_REGDOMAINS_KEY, domainArray);
    } else {
        result = WriteArrayToDynDNS(SC_DYNDNS_BROWSEDOMAINS_KEY, domainArray);
    }

ReadParamsFailed:
	return result;
}


static int
HandleWriteHostname(int fd)
{
	CFArrayRef      domainArray;
	CFDataRef       domainData;
	int				result = 0;
	u_int32_t		tag, len;
	char			*p;

	AuthorizationItem	scAuth = { UPDATE_SC_RIGHT, 0, NULL, 0 };
	AuthorizationRights	authSet = { 1, &scAuth };

	if (noErr != (result = AuthorizationCopyRights(gAuthRef, &authSet, NULL, (AuthorizationFlags) 0, NULL)))
		return result;

	result = readTaggedBlock(fd, &tag, &len, &p);
	require(result == 0, ReadParamsFailed);

	domainData = CFDataCreate(NULL, (const UInt8 *)p, len);
	domainArray = (CFArrayRef)[NSUnarchiver unarchiveObjectWithData:(NSData *)domainData];
	result = WriteArrayToDynDNS(SC_DYNDNS_HOSTNAMES_KEY, domainArray);
	
ReadParamsFailed:
	return result;
}


static SecAccessRef
MyMakeUidAccess(uid_t uid)
{
	// make the "uid/gid" ACL subject
	// this is a CSSM_LIST_ELEMENT chain
	CSSM_ACL_PROCESS_SUBJECT_SELECTOR selector = {
		CSSM_ACL_PROCESS_SELECTOR_CURRENT_VERSION,	// selector version
		CSSM_ACL_MATCH_UID,	// set mask: match uids (only)
		uid,				// uid to match
		0					// gid (not matched here)
	};
	CSSM_LIST_ELEMENT subject2 = { NULL, 0, 0, {{0,0,0}} };
	subject2.Element.Word.Data = (UInt8 *)&selector;
	subject2.Element.Word.Length = sizeof(selector);
	CSSM_LIST_ELEMENT subject1 = { &subject2, CSSM_ACL_SUBJECT_TYPE_PROCESS, CSSM_LIST_ELEMENT_WORDID, {{0,0,0}} };


	// rights granted (replace with individual list if desired)
	CSSM_ACL_AUTHORIZATION_TAG rights[] = {
		CSSM_ACL_AUTHORIZATION_ANY	// everything
	};
	// owner component (right to change ACL)
	CSSM_ACL_OWNER_PROTOTYPE owner = {
		// TypedSubject
		{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 },
		// Delegate
		false
	};
	// ACL entries (any number, just one here)
	CSSM_ACL_ENTRY_INFO acls =
		{
		// CSSM_ACL_ENTRY_PROTOTYPE
			{
			{ CSSM_LIST_TYPE_UNKNOWN, &subject1, &subject2 }, // TypedSubject
			false,	// Delegate
			{ sizeof(rights) / sizeof(rights[0]), rights }, // Authorization rights for this entry
			{ { 0, 0 }, { 0, 0 } }, // CSSM_ACL_VALIDITY_PERIOD
			"" // CSSM_STRING EntryTag
			},
		// CSSM_ACL_HANDLE
		0
		};

	SecAccessRef a = NULL;
	(void) SecAccessCreateFromOwnerAndACL(&owner, 1, &acls, &a);
	return a;
}


static OSStatus
MyAddDynamicDNSPassword(SecKeychainRef keychain, SecAccessRef a, UInt32 serviceNameLength, const char *serviceName,
    UInt32 accountNameLength, const char *accountName, UInt32 passwordLength, const void *passwordData)
{
	char * description       = DYNDNS_KEYCHAIN_DESCRIPTION;
	UInt32 descriptionLength = strlen(DYNDNS_KEYCHAIN_DESCRIPTION);
	UInt32 type              = 'ddns';
	UInt32 creator           = 'ddns';
	UInt32 typeLength        = sizeof(type);
	UInt32 creatorLength     = sizeof(creator);
    OSStatus err;
	
	// set up attribute vector (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = { { kSecLabelItemAttr,       serviceNameLength,   (char *)serviceName },
                                     { kSecAccountItemAttr,     accountNameLength,   (char *)accountName },
                                     { kSecServiceItemAttr,     serviceNameLength,   (char *)serviceName },
                                     { kSecDescriptionItemAttr, descriptionLength,   (char *)description },
                                     { kSecTypeItemAttr,               typeLength, (UInt32 *)&type       },
                                     { kSecCreatorItemAttr,         creatorLength, (UInt32 *)&creator    } };
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };

	err = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass, &attributes, passwordLength, passwordData, keychain, a, NULL);
    return err;
}


static int
SetKeychainEntry(int fd)
// Create a new entry in system keychain, or replace existing
{
	CFDataRef           secretData;
	CFDictionaryRef     secretDictionary;
	CFStringRef         keyNameString;
	CFStringRef         domainString;
	CFStringRef         secretString;
	SecKeychainItemRef	item = NULL;
	int					result = 0;
	u_int32_t			tag, len;
	char				*p;
	char                keyname[kDNSServiceMaxDomainName];
	char                domain[kDNSServiceMaxDomainName];
	char                secret[kDNSServiceMaxDomainName];

	AuthorizationItem	kcAuth = { EDIT_SYS_KEYCHAIN_RIGHT, 0, NULL, 0 };
	AuthorizationRights	authSet = { 1, &kcAuth };

	if (noErr != (result = AuthorizationCopyRights(gAuthRef, &authSet, NULL, (AuthorizationFlags)0, NULL)))
		return result;

	result = readTaggedBlock(fd, &tag, &len, &p);
	require_noerr(result, ReadParamsFailed);

	secretData = CFDataCreate(NULL, (UInt8 *)p, len);
	secretDictionary = (CFDictionaryRef)[NSUnarchiver unarchiveObjectWithData:(NSData *)secretData];

	keyNameString = (CFStringRef)CFDictionaryGetValue(secretDictionary, SC_DYNDNS_KEYNAME_KEY);
	assert(keyNameString != NULL);
	
	domainString  = (CFStringRef)CFDictionaryGetValue(secretDictionary, SC_DYNDNS_DOMAIN_KEY);
	assert(domainString != NULL);
	
	secretString  = (CFStringRef)CFDictionaryGetValue(secretDictionary, SC_DYNDNS_SECRET_KEY);
	assert(secretString != NULL);
			
	CFStringGetCString(keyNameString, keyname, kDNSServiceMaxDomainName, kCFStringEncodingUTF8);
	CFStringGetCString(domainString,   domain, kDNSServiceMaxDomainName, kCFStringEncodingUTF8);
	CFStringGetCString(secretString,   secret, kDNSServiceMaxDomainName, kCFStringEncodingUTF8);

	result = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (result == noErr) {
		result = SecKeychainFindGenericPassword(NULL, strlen(domain), domain, 0, NULL, 0, NULL, &item);
		if (result == noErr) {
			result = SecKeychainItemDelete(item);
			if (result != noErr) fprintf(stderr, "SecKeychainItemDelete returned %d\n", result);
		}
			 
		result = MyAddDynamicDNSPassword(NULL, MyMakeUidAccess(0), strlen(domain), domain, strlen(keyname)+1, keyname, strlen(secret)+1, secret);
		if (result != noErr) fprintf(stderr, "MyAddDynamicDNSPassword returned %d\n", result);
		if (item) CFRelease(item);
	}

ReadParamsFailed:
	return result;
}


int	main( int argc, char **argv)
/* argv[0] is the exec path; argv[1] is a fd for input data; argv[2]... are operation codes.
   The tool supports the following operations:
	V		-- exit with status PRIV_OP_TOOL_VERS
	A		-- read AuthInfo from input pipe
	Wd		-- write registration domain to dynamic store
	Wb		-- write browse domain to dynamic store
	Wh		-- write hostname to dynamic store
	Wk		-- write keychain entry for given account name
*/
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	int	commFD = -1, iArg, result = 0;

	if ( 0 != seteuid( 0))
		return -1;

	if ( argc == 3 && 0 == strcmp( argv[2], "V"))
		return PRIV_OP_TOOL_VERS;

	if ( argc > 1)
	{
		commFD = strtol( argv[1], NULL, 0);
		lseek( commFD, 0, SEEK_SET);
	}
	for ( iArg = 2; iArg < argc && result == 0; iArg++)
	{
		if ( 0 == strcmp( "A", argv[ iArg]))	// get auth info
		{
			result = SetAuthInfo( commFD);
		}
		else if ( 0 == strcmp( "Wd", argv[ iArg]))	// Write registration domain
		{
			result = HandleWriteDomain( commFD, 1);
		}
        else if ( 0 == strcmp( "Wb", argv[ iArg]))	// Write browse domain
		{
			result = HandleWriteDomain( commFD, 0);
		}
		else if ( 0 == strcmp( "Wh", argv[ iArg]))	// Write hostname
		{
			result = HandleWriteHostname( commFD);
		}
		else if ( 0 == strcmp( "Wk", argv[ iArg]))	// Write keychain entry
		{
			result = SetKeychainEntry( commFD);
		}
	}
	[pool release];
	return result;
}

// Note: The C preprocessor stringify operator ('#') makes a string from its argument, without macro expansion
// e.g. If "version" is #define'd to be "4", then STRINGIFY_AWE(version) will return the string "version", not "4"
// To expand "version" to its value before making the string, use STRINGIFY(version) instead
#define STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s) #s
#define STRINGIFY(s) STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s)

// NOT static -- otherwise the compiler may optimize it out
// The "@(#) " pattern is a special prefix the "what" command looks for
const char VersionString_SCCS[] = "@(#) ddnswriteconfig " STRINGIFY(mDNSResponderVersion) " (" __DATE__ " " __TIME__ ")";

#if _BUILDING_XCODE_PROJECT_
// If the process crashes, then this string will be magically included in the automatically-generated crash log
const char *__crashreporter_info__ = VersionString_SCCS + 5;
asm(".desc ___crashreporter_info__, 0x10");
#endif
