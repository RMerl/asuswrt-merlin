/*
    File: ConfigurationAuthority.c

    Abstract: Interface to system security framework that manages access
    to protected resources like system configuration preferences.

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

$Log: ConfigurationAuthority.c,v $
Revision 1.3  2008/06/26 17:34:18  mkrochma
<rdar://problem/6030630> Pref pane destroying shared "system.preferences" authorization right

Revision 1.2  2005/08/07 22:48:05  mkrochma
<rdar://problem/4204003> Bonjour Pref Pane returns -927 when "system.preferences" is not shared

Revision 1.1  2005/02/05 02:28:22  cheshire
Add Preference Pane to facilitate testing of DDNS & wide-area features

*/

#include "ConfigurationAuthority.h"
#include "ConfigurationRights.h"

#include <AssertMacros.h>


static AuthorizationRef	gAuthRef = 0;

static AuthorizationItem	gAuthorizations[] = {	{ UPDATE_SC_RIGHT, 0, NULL, 0 }, 
													{ EDIT_SYS_KEYCHAIN_RIGHT, 0, NULL, 0 }};
static AuthorizationRights	gAuthSet = { sizeof gAuthorizations / sizeof gAuthorizations[0], gAuthorizations };

static CFDictionaryRef	CreateRightsDict( CFStringRef prompt)
/* Create a CFDictionary decribing an auth right. See /etc/authorization for examples. */
/* Specifies that the right requires admin authentication, which persists for 5 minutes. */
{
	CFMutableDictionaryRef	dict = NULL, tmpDict;
	CFMutableArrayRef		mechanisms;
	CFNumberRef				timeout;
	int						val;
	
	tmpDict = CFDictionaryCreateMutable( (CFAllocatorRef) NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	require( tmpDict != NULL, MakeDictFailed);

	CFDictionaryAddValue(tmpDict, CFSTR("class"), CFSTR("user"));
	CFDictionaryAddValue(tmpDict, CFSTR("comment"), prompt);
	CFDictionaryAddValue(tmpDict, CFSTR("group"), CFSTR("admin"));

	mechanisms = CFArrayCreateMutable((CFAllocatorRef) NULL, 1, &kCFTypeArrayCallBacks);
	require( mechanisms != NULL, MakeArrayFailed);
	CFArrayAppendValue( mechanisms, CFSTR("builtin:authenticate"));
	CFDictionaryAddValue( tmpDict, CFSTR("mechanisms"), mechanisms);

	val = 300;	// seconds
	timeout = CFNumberCreate((CFAllocatorRef) NULL, kCFNumberIntType, &val);
	require( timeout != NULL, MakeIntFailed);
	CFDictionaryAddValue( tmpDict, CFSTR("timeout"), timeout);
	CFDictionaryAddValue( tmpDict, CFSTR("shared"), kCFBooleanTrue);

	dict = tmpDict;
	tmpDict = NULL;

	CFRelease( timeout);
MakeIntFailed:
	CFRelease( mechanisms);
MakeArrayFailed:
	if ( tmpDict)
		CFRelease( tmpDict);
MakeDictFailed:
	return dict;
}

OSStatus InitConfigAuthority(void)
/* Initialize the authorization record-keeping */
{
	OSStatus        err;
	CFDictionaryRef dict;
	CFStringRef		rightInfo;

	err = AuthorizationCreate((AuthorizationRights*) NULL, (AuthorizationEnvironment*) NULL,
								(AuthorizationFlags) 0, &gAuthRef);
	require_noerr( err, NewAuthFailed);

	err = AuthorizationRightGet( UPDATE_SC_RIGHT, (CFDictionaryRef*) NULL);
	if( err == errAuthorizationDenied)
	{
		rightInfo = CFCopyLocalizedString(CFSTR("Authentication required to set Dynamic DNS preferences."), 
						CFSTR("Describes operation that requires user authorization"));
		require_action( rightInfo != NULL, GetStrFailed, err=coreFoundationUnknownErr;);
		dict = CreateRightsDict(rightInfo);
		require_action( dict != NULL, GetStrFailed, err=coreFoundationUnknownErr;);

		err = AuthorizationRightSet(gAuthRef, UPDATE_SC_RIGHT, dict, (CFStringRef) NULL, 
									(CFBundleRef) NULL, (CFStringRef) NULL);
		CFRelease(rightInfo);
		CFRelease(dict);
	}
	require_noerr( err, AuthSetFailed);

	err = AuthorizationRightGet( EDIT_SYS_KEYCHAIN_RIGHT, (CFDictionaryRef*) NULL);
	if( err == errAuthorizationDenied)
	{
		rightInfo = CFCopyLocalizedString( CFSTR("Authentication required to edit System Keychain."), 
						CFSTR("Describes operation that requires user authorization"));
		require_action( rightInfo != NULL, GetStrFailed, err=coreFoundationUnknownErr;);
		dict = CreateRightsDict( rightInfo);
		require_action( dict != NULL, GetStrFailed, err=coreFoundationUnknownErr;);

		err = AuthorizationRightSet(gAuthRef, EDIT_SYS_KEYCHAIN_RIGHT, dict, (CFStringRef) NULL, 
									(CFBundleRef) NULL, (CFStringRef) NULL);
		CFRelease( rightInfo);
		CFRelease( dict);
	}
	require_noerr( err, AuthSetFailed);

AuthSetFailed:
GetStrFailed:
NewAuthFailed:
	return err;
}

OSStatus	AttemptAcquireAuthority( Boolean allowUI)
/* Try to get permission for privileged ops, either implicitly or by asking the user for */
/* authority to perform operations (if necessary) */
{
	AuthorizationFlags	allowFlag = allowUI ? kAuthorizationFlagInteractionAllowed : 0;
	OSStatus			err;

	err = AuthorizationCopyRights( gAuthRef, &gAuthSet, (AuthorizationEnvironment*) NULL,
									kAuthorizationFlagExtendRights | kAuthorizationFlagPreAuthorize |
									allowFlag,
									(AuthorizationRights**) NULL);
	return err;
}

OSStatus ReleaseAuthority(void)
/* Discard authority to perform operations */
{
	(void) AuthorizationFree( gAuthRef, kAuthorizationFlagDefaults);
	gAuthRef = 0;
	return AuthorizationCreate( (AuthorizationRights*) NULL, (AuthorizationEnvironment*) NULL,
								(AuthorizationFlags) 0, &gAuthRef);
}

Boolean	CurrentlyAuthorized(void)
{
	OSStatus err = AttemptAcquireAuthority(true);
	return err == noErr;
}


OSStatus ExternalizeAuthority(AuthorizationExternalForm *pAuth)
/* Package up current authorizations for transfer to another process */
{
	return AuthorizationMakeExternalForm(gAuthRef, pAuth);
}
