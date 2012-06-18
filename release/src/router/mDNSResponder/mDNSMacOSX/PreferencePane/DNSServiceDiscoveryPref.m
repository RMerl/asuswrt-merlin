/*
    File: DNSServiceDiscoveryPref.m

    Abstract: System Preference Pane for Dynamic DNS and Wide-Area DNS Service Discovery

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

$Log: DNSServiceDiscoveryPref.m,v $
Revision 1.16  2008/09/15 23:52:30  cheshire
<rdar://problem/6218902> mDNSResponder-177 fails to compile on Linux with .desc pseudo-op
Made __crashreporter_info__ symbol conditional, so we only use it for OS X build

Revision 1.15  2008/08/18 17:57:04  mcguire
<rdar://problem/6156209> build error

Revision 1.14  2008/07/18 17:39:14  cheshire
If NSInteger is not defined (indicated by lack of definition for NSINTEGER_DEFINED)
then #define "NSInteger" to be "int" like it used to be

Revision 1.13  2008/07/01 01:40:01  mcguire
<rdar://problem/5823010> 64-bit fixes

Revision 1.12  2008/05/08 00:46:38  cheshire
<rdar://problem/5919272> GetNextLabel insufficiently defensive
User shared copy of GetNextLabel in ClientCommon.c instead of having a local copy here

Revision 1.11  2007/11/30 23:42:09  cheshire
Fixed compile warning: declaration of 'index' shadows a global declaration

Revision 1.10  2007/09/18 19:09:02  cheshire
<rdar://problem/5489549> mDNSResponderHelper (and other binaries) missing SCCS version strings

Revision 1.9  2007/02/09 00:39:06  cheshire
Fix compile warnings

Revision 1.8  2006/08/14 23:15:47  cheshire
Tidy up Change History comment

Revision 1.7  2006/07/14 03:59:14  cheshire
Fix compile warnings: 'sortUsingFunction:context:' comparison function needs to return int

Revision 1.6  2005/02/26 00:44:24  cheshire
Restore default reg domain if user deletes text and clicks "apply"

Revision 1.5  2005/02/25 02:29:28  cheshire
Show yellow dot for "update in progress"

Revision 1.4  2005/02/16 00:18:33  cheshire
Bunch o' fixes

Revision 1.3  2005/02/10 22:35:20  cheshire
<rdar://problem/3727944> Update name

Revision 1.2  2005/02/08 01:32:05  cheshire
Add trimCharactersFromDomain routine to strip leading and trailing
white space and punctuation from user-entered fields.

Revision 1.1  2005/02/05 01:59:19  cheshire
Add Preference Pane to facilitate testing of DDNS & wide-area features

*/

#import "DNSServiceDiscoveryPref.h"
#import "ConfigurationAuthority.h"
#import "PrivilegedOperations.h"
#import <unistd.h>

#include "../../Clients/ClientCommon.h"

#ifndef NSINTEGER_DEFINED
#define NSInteger int
#endif

@implementation DNSServiceDiscoveryPref

static NSComparisonResult
MyArrayCompareFunction(id val1, id val2, void *context)
{
	(void)context; // Unused
    return CFStringCompare((CFStringRef)val1, (CFStringRef)val2, kCFCompareCaseInsensitive);
}


static NSComparisonResult
MyDomainArrayCompareFunction(id val1, id val2, void *context)
{
	(void)context; // Unused
	NSString *domain1 = [val1 objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
	NSString *domain2 = [val2 objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
    return CFStringCompare((CFStringRef)domain1, (CFStringRef)domain2, kCFCompareCaseInsensitive);
}


static void NetworkChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *context)
{
	(void)store; // Unused
	(void)changedKeys; // Unused
    DNSServiceDiscoveryPref * me = (DNSServiceDiscoveryPref *)context;
    assert(me != NULL);
    
    [me setupInitialValues];
}


static void ServiceDomainEnumReply( DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *replyDomain, void *context, DNSServiceFlags enumType)
{    
	(void)sdRef; // Unused
	(void)interfaceIndex; // Unused
	(void)errorCode; // Unused
    if (strcmp(replyDomain, "local.") == 0) return;  // local domain is not interesting

	DNSServiceDiscoveryPref * me = (DNSServiceDiscoveryPref *)context;
 	BOOL moreComing = (BOOL)(flags & kDNSServiceFlagsMoreComing);
    NSMutableArray * domainArray;
    NSMutableArray * defaultBrowseDomainsArray = nil;
    NSComboBox * domainComboBox;
    NSString * domainString;
    NSString * currentDomain = nil;
	char decodedDomainString[kDNSServiceMaxDomainName] = "\0";
    char nextLabel[256] = "\0";
    char * buffer = (char *)replyDomain;
    
	while (*buffer) {
        buffer = (char *)GetNextLabel(buffer, nextLabel);
        strcat(decodedDomainString, nextLabel);
        strcat(decodedDomainString, ".");
    }
    
    // Remove trailing dot from domain name.
    decodedDomainString[strlen(decodedDomainString)-1] = '\0';
    
    domainString = [[[NSString alloc] initWithUTF8String:(const char *)decodedDomainString] autorelease];

    if (enumType & kDNSServiceFlagsRegistrationDomains) {
        domainArray    = [me registrationDataSource];
        domainComboBox = [me regDomainsComboBox];
        currentDomain  = [me currentRegDomain];
    } else { 
        domainArray    = [me browseDataSource];
        domainComboBox = [me browseDomainsComboBox];
        defaultBrowseDomainsArray = [me defaultBrowseDomainsArray];
    }
    
	if (flags & kDNSServiceFlagsAdd) {
		[domainArray removeObject:domainString];  // How can I check if an object is in the array?
		[domainArray addObject:domainString];
        if ((flags & kDNSServiceFlagsDefault) && (enumType & kDNSServiceFlagsRegistrationDomains)) {
			[me setDefaultRegDomain:domainString];
			if ([[domainComboBox stringValue] length] == 0) [domainComboBox setStringValue:domainString];
		} else if ((flags & kDNSServiceFlagsDefault) && !(enumType & kDNSServiceFlagsRegistrationDomains)) {
			[defaultBrowseDomainsArray removeObject:domainString];
			[defaultBrowseDomainsArray addObject:domainString];
		}
	}
    
    if (moreComing == NO) {
        [domainArray sortUsingFunction:MyArrayCompareFunction context:nil];
        [domainComboBox reloadData];
    }    
}


static void
browseDomainReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *replyDomain, void *context)
{
    ServiceDomainEnumReply(sdRef, flags, interfaceIndex, errorCode, replyDomain, context, kDNSServiceFlagsBrowseDomains);
}


static void
registrationDomainReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *replyDomain, void *context)
{
    ServiceDomainEnumReply(sdRef, flags, interfaceIndex, errorCode, replyDomain, context, kDNSServiceFlagsRegistrationDomains);
}



static void
MyDNSServiceCleanUp(MyDNSServiceState * query)
{
    /* Remove the CFRunLoopSource from the current run loop. */
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), query->source, kCFRunLoopCommonModes);
    CFRelease(query->source);
    
    /* Invalidate the CFSocket. */
    CFSocketInvalidate(query->socket);
    CFRelease(query->socket);
    
    /* Workaround that gives time to CFSocket's select thread so it can remove the socket from its FD set
    before we close the socket by calling DNSServiceRefDeallocate. <rdar://problem/3585273> */
    usleep(1000);
    
    /* Terminate the connection with the mDNSResponder daemon, which cancels the query. */
    DNSServiceRefDeallocate(query->service);
}



static void
MySocketReadCallback(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void * data, void * info)
{
    #pragma unused(s)
    #pragma unused(type)
    #pragma unused(address)
    #pragma unused(data)
    
    DNSServiceErrorType err;
 
    MyDNSServiceState * query = (MyDNSServiceState *)info;  // context passed in to CFSocketCreateWithNative().
    assert(query != NULL);
    
    /* Read a reply from the mDNSResponder.  */
    err= DNSServiceProcessResult(query->service);
    if (err != kDNSServiceErr_NoError) {
        fprintf(stderr, "DNSServiceProcessResult returned %d\n", err);
        
        /* Terminate the query operation and release the CFRunLoopSource and CFSocket. */
        MyDNSServiceCleanUp(query);
    }
}



static void
MyDNSServiceAddServiceToRunLoop(MyDNSServiceState * query)
{
    CFSocketNativeHandle sock;
    CFOptionFlags        sockFlags;
    CFSocketContext      context = { 0, query, NULL, NULL, NULL };  // Use MyDNSServiceState as context data.
    
    /* Access the underlying Unix domain socket to communicate with the mDNSResponder daemon. */
    sock = DNSServiceRefSockFD(query->service);
    assert(sock != -1);
    
    /* Create a CFSocket using the Unix domain socket. */
    query->socket = CFSocketCreateWithNative(NULL, sock, kCFSocketReadCallBack, MySocketReadCallback, &context);
    assert(query->socket != NULL);
    
    /* Prevent CFSocketInvalidate from closing DNSServiceRef's socket. */
    sockFlags = CFSocketGetSocketFlags(query->socket);
    CFSocketSetSocketFlags(query->socket, sockFlags & (~kCFSocketCloseOnInvalidate));
    
    /* Create a CFRunLoopSource from the CFSocket. */
    query->source = CFSocketCreateRunLoopSource(NULL, query->socket, 0);
    assert(query->source != NULL);

    /* Add the CFRunLoopSource to the current run loop. */
    CFRunLoopAddSource(CFRunLoopGetCurrent(), query->source, kCFRunLoopCommonModes);
}



-(void)updateStatusImageView
{
    int value = [self statusForHostName:currentHostName];
    if      (value == 0) [statusImageView setImage:successImage];
    else if (value >  0) [statusImageView setImage:inprogressImage];
    else                 [statusImageView setImage:failureImage];
}


- (void)watchForPreferenceChanges
{
	SCDynamicStoreContext context = { 0, self, NULL, NULL, NULL };
	SCDynamicStoreRef     store   = SCDynamicStoreCreate(NULL, CFSTR("watchForPreferenceChanges"), NetworkChanged, &context);
	CFMutableArrayRef     keys    = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFRunLoopSourceRef    rls;
	
	assert(store != NULL);
	assert(keys != NULL);
    
	CFArrayAppendValue(keys, SC_DYNDNS_STATE_KEY);
	CFArrayAppendValue(keys, SC_DYNDNS_SETUP_KEY);

	(void)SCDynamicStoreSetNotificationKeys(store, keys, NULL);

	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    assert(rls != NULL);
    
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopCommonModes);

    CFRelease(keys);
	CFRelease(store);
}


-(int)statusForHostName:(NSString * )domain
{
	SCDynamicStoreRef store       = SCDynamicStoreCreate(NULL, CFSTR("statusForHostName"), NULL, NULL);
    NSString     *lowercaseDomain = [domain lowercaseString];
    int status = 1;
    
    assert(store != NULL);
        
    NSDictionary *dynamicDNS = (NSDictionary *)SCDynamicStoreCopyValue(store, SC_DYNDNS_STATE_KEY);
    if (dynamicDNS) {
        NSDictionary *hostNames = [dynamicDNS objectForKey:(NSString *)SC_DYNDNS_HOSTNAMES_KEY];
        NSDictionary *infoDict  = [hostNames objectForKey:lowercaseDomain];
        if (infoDict) status = [[infoDict objectForKey:(NSString*)SC_DYNDNS_STATUS_KEY] intValue];
        CFRelease(dynamicDNS);
	}
    CFRelease(store);

    return status;
}


- (void)startDomainBrowsing
{
    DNSServiceFlags flags;
    OSStatus err = noErr;
    
    flags = kDNSServiceFlagsRegistrationDomains;
    err = DNSServiceEnumerateDomains(&regQuery.service, flags, 0, registrationDomainReply, (void *)self);
    if (err == kDNSServiceErr_NoError) MyDNSServiceAddServiceToRunLoop(&regQuery);

    flags = kDNSServiceFlagsBrowseDomains;
    err = DNSServiceEnumerateDomains(&browseQuery.service, flags, 0, browseDomainReply, (void *)self);
    if (err == kDNSServiceErr_NoError) MyDNSServiceAddServiceToRunLoop(&browseQuery);
}


-(void)readPreferences
{
	NSDictionary *origDict;
    NSArray      *regDomainArray;
    NSArray      *hostArray;

    if (currentRegDomain)          [currentRegDomain release];
    if (currentBrowseDomainsArray) [currentBrowseDomainsArray release];
    if (currentHostName)           [currentHostName release];

	SCDynamicStoreRef store = SCDynamicStoreCreate(NULL, CFSTR("com.apple.preference.bonjour"), NULL, NULL);
	origDict = (NSDictionary *)SCDynamicStoreCopyValue(store, SC_DYNDNS_SETUP_KEY);

	regDomainArray = [origDict objectForKey:(NSString *)SC_DYNDNS_REGDOMAINS_KEY];
	if (regDomainArray && [regDomainArray count] > 0) {
		currentRegDomain = [[[regDomainArray objectAtIndex:0] objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY] copy];
		currentWideAreaState = [[[regDomainArray objectAtIndex:0] objectForKey:(NSString *)SC_DYNDNS_ENABLED_KEY] intValue];
    } else {
		currentRegDomain = [[NSString alloc] initWithString:@""];
		currentWideAreaState = NO;
	}

	currentBrowseDomainsArray = [[origDict objectForKey:(NSString *)SC_DYNDNS_BROWSEDOMAINS_KEY] retain];

    hostArray = [origDict objectForKey:(NSString *)SC_DYNDNS_HOSTNAMES_KEY];
	if (hostArray && [hostArray count] > 0) {
		currentHostName = [[[hostArray objectAtIndex:0] objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY] copy];
	} else {
		currentHostName = [[NSString alloc] initWithString:@""];
    }

    [origDict release];
    CFRelease(store);
}


- (void)tableViewSelectionDidChange:(NSNotification *)notification;
{
	[removeBrowseDomainButton setEnabled:[[notification object] numberOfSelectedRows]];
}


- (void)setBrowseDomainsComboBox;
{
	NSString * domain = nil;
	
	if ([defaultBrowseDomainsArray count] > 0) {
		NSEnumerator * arrayEnumerator = [defaultBrowseDomainsArray objectEnumerator];
		while ((domain = [arrayEnumerator nextObject]) != NULL) {
			if ([self domainAlreadyInList:domain] == NO) break;
		}
	}
	if (domain) [browseDomainsComboBox setStringValue:domain];
	else        [browseDomainsComboBox setStringValue:@""];
}


- (IBAction)addBrowseDomainClicked:(id)sender;
{
	[self setBrowseDomainsComboBox];

	[NSApp beginSheet:addBrowseDomainWindow modalForWindow:mainWindow modalDelegate:self
		didEndSelector:@selector(addBrowseDomainSheetDidEnd:returnCode:contextInfo:) contextInfo:sender];

	[browseDomainList deselectAll:sender];
	[self updateApplyButtonState];
}


- (IBAction)removeBrowseDomainClicked:(id)sender;
{
	(void)sender; // Unused
	int selectedBrowseDomain = [browseDomainList selectedRow];
	[browseDomainsArray removeObjectAtIndex:selectedBrowseDomain];
	[browseDomainList reloadData];
	[self updateApplyButtonState];
}


- (IBAction)enableBrowseDomainClicked:(id)sender;
{
	NSTableView *tableView = sender;
    NSMutableDictionary *browseDomainDict;
	int value;
	
	browseDomainDict = [[browseDomainsArray objectAtIndex:[tableView clickedRow]] mutableCopy];
	value = [[browseDomainDict objectForKey:(NSString *)SC_DYNDNS_ENABLED_KEY] intValue];
	[browseDomainDict setObject:[[[NSNumber alloc] initWithInt:(!value)] autorelease] forKey:(NSString *)SC_DYNDNS_ENABLED_KEY];
	[browseDomainsArray replaceObjectAtIndex:[tableView clickedRow] withObject:browseDomainDict];
	[tableView reloadData];
	[self updateApplyButtonState];
}



- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView;
{
	(void)tableView; // Unused
	int numberOfRows = 0;
		
	if (browseDomainsArray) {
		numberOfRows = [browseDomainsArray count];
	}
	return numberOfRows;
}


- (void)tabView:(NSTabView *)xtabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem;
{
	(void)xtabView; // Unused
	(void)tabViewItem; // Unused
	[browseDomainList deselectAll:self];
	[mainWindow makeFirstResponder:nil];
}
 

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row;
{
	(void)tableView; // Unused
	NSDictionary *browseDomainDict;
	id           value = nil;
		
	if (browseDomainsArray) {
		browseDomainDict = [browseDomainsArray objectAtIndex:row];
		if (browseDomainDict) {
			if ([[tableColumn identifier] isEqualTo:(NSString *)SC_DYNDNS_ENABLED_KEY]) {
				value = [browseDomainDict objectForKey:(NSString *)SC_DYNDNS_ENABLED_KEY];
			} else if ([[tableColumn identifier] isEqualTo:(NSString *)SC_DYNDNS_DOMAIN_KEY]) {
				value = [browseDomainDict objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
			}
		}
	}
	return value;
}


- (void)setupInitialValues
{    
    [self readPreferences];
    
    if (currentHostName) {
		[hostName setStringValue:currentHostName];
		[self updateStatusImageView];
	}
	
	if (browseDomainsArray) {
		[browseDomainsArray release];
		browseDomainsArray = nil;
	}
	
	if (currentBrowseDomainsArray) {
		browseDomainsArray = [currentBrowseDomainsArray mutableCopy];
		if (browseDomainsArray) {
			[browseDomainsArray sortUsingFunction:MyDomainArrayCompareFunction context:nil];
			if ([browseDomainsArray isEqualToArray:currentBrowseDomainsArray] == NO) {
				OSStatus err = WriteBrowseDomain((CFDataRef)[self dataForDomainArray:browseDomainsArray]);
				if (err != noErr) NSLog(@"WriteBrowseDomain returned %d\n", err);
				[currentBrowseDomainsArray release];
				currentBrowseDomainsArray = [browseDomainsArray copy];
			}
		}
	} else {
		browseDomainsArray = nil;
	}
	[browseDomainList reloadData];
	
    if (currentRegDomain && ([currentRegDomain length] > 0)) {
        [regDomainsComboBox setStringValue:currentRegDomain];
        [registrationDataSource removeObject:currentRegDomain];
        [registrationDataSource addObject:currentRegDomain];
        [registrationDataSource sortUsingFunction:MyArrayCompareFunction context:nil];
        [regDomainsComboBox reloadData];
    }
    
    if (currentWideAreaState) {
        [self toggleWideAreaBonjour:YES];
    } else {
        [self toggleWideAreaBonjour:NO];
    }

    if (hostNameSharedSecretValue) {
        [hostNameSharedSecretValue release];
        hostNameSharedSecretValue = nil;
    }
    
    if (regSharedSecretValue) {
        [regSharedSecretValue release];
        regSharedSecretValue = nil;
    }
    
    [self updateApplyButtonState];
    [mainWindow makeFirstResponder:nil];
	[browseDomainList deselectAll:self];
	[removeBrowseDomainButton setEnabled:NO];
}



- (void)awakeFromNib
{        
    OSStatus err;
    
    prefsNeedUpdating         = NO;
    toolInstalled             = NO;
	browseDomainListEnabled   = NO;
	defaultRegDomain          = nil;
    currentRegDomain          = nil;
	currentBrowseDomainsArray = nil;
    currentHostName           = nil;
    hostNameSharedSecretValue = nil;
    regSharedSecretValue      = nil;
	browseDomainsArray        = nil;
    justStartedEditing        = YES;
    currentWideAreaState      = NO;
	NSString *successPath     = [[NSBundle bundleForClass:[self class]] pathForResource:@"success"    ofType:@"tiff"];
	NSString *inprogressPath  = [[NSBundle bundleForClass:[self class]] pathForResource:@"inprogress" ofType:@"tiff"];
	NSString *failurePath     = [[NSBundle bundleForClass:[self class]] pathForResource:@"failure"    ofType:@"tiff"];

    registrationDataSource    = [[NSMutableArray alloc] init];
    browseDataSource          = [[NSMutableArray alloc] init];
	defaultBrowseDomainsArray = [[NSMutableArray alloc] init];
	successImage              = [[NSImage alloc] initWithContentsOfFile:successPath];
	inprogressImage           = [[NSImage alloc] initWithContentsOfFile:inprogressPath];
	failureImage              = [[NSImage alloc] initWithContentsOfFile:failurePath];

    [tabView selectFirstTabViewItem:self];
    [self setupInitialValues];
    [self startDomainBrowsing];
    [self watchForPreferenceChanges];
	
    InitConfigAuthority();
    err = EnsureToolInstalled();
    if (err == noErr) toolInstalled = YES;
    else { long int tmp = err; fprintf(stderr, "EnsureToolInstalled returned %ld\n", tmp); }
    
}


- (IBAction)closeMyCustomSheet:(id)sender
{
    BOOL result = [sender isEqualTo:browseOKButton] || [sender isEqualTo:secretOKButton];

    if (result) [NSApp endSheet:[sender window] returnCode:NSOKButton];
    else        [NSApp endSheet:[sender window] returnCode:NSCancelButton];
}


- (void)sharedSecretSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    NSButton * button = (NSButton *)contextInfo;
    [sheet orderOut:self];
    [self enableControls];
    
    if (returnCode == NSOKButton) {
        if ([button isEqualTo:hostNameSharedSecretButton]) {
            hostNameSharedSecretName = [[NSString alloc] initWithString:[sharedSecretName stringValue]];
            hostNameSharedSecretValue = [[NSString alloc] initWithString:[sharedSecretValue stringValue]];
        } else {
            regSharedSecretName = [[NSString alloc] initWithString:[sharedSecretName stringValue]];
            regSharedSecretValue = [[NSString alloc] initWithString:[sharedSecretValue stringValue]];
        }
        [self updateApplyButtonState];
    }
    [sharedSecretValue setStringValue:@""];
}


- (BOOL)domainAlreadyInList:(NSString *)domainString
{
	if (browseDomainsArray) {
		NSDictionary *domainDict;
		NSString     *domainName;
		NSEnumerator *arrayEnumerator = [browseDomainsArray objectEnumerator];
		while ((domainDict = [arrayEnumerator nextObject]) != NULL) {
			domainName = [domainDict objectForKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
			if ([domainString caseInsensitiveCompare:domainName] == NSOrderedSame) return YES;
		}
	}
	return NO;
}


- (NSString *)trimCharactersFromDomain:(NSString *)domain
{
	NSMutableCharacterSet * trimSet = [[[NSCharacterSet whitespaceCharacterSet] mutableCopy] autorelease];
	[trimSet formUnionWithCharacterSet:[NSCharacterSet punctuationCharacterSet]];
	return [domain stringByTrimmingCharactersInSet:trimSet];	
}


- (void)addBrowseDomainSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	(void)contextInfo; // Unused
    [sheet orderOut:self];
    [self enableControls];
    
    if (returnCode == NSOKButton) {
		NSString * newBrowseDomainString = [self trimCharactersFromDomain:[browseDomainsComboBox stringValue]];
		NSMutableDictionary *newBrowseDomainDict;
		
		if (browseDomainsArray == nil) browseDomainsArray = [[NSMutableArray alloc] initWithCapacity:0];
		if ([self domainAlreadyInList:newBrowseDomainString] == NO) {
			newBrowseDomainDict = [[[NSMutableDictionary alloc] initWithCapacity:2] autorelease];

			[newBrowseDomainDict setObject:newBrowseDomainString forKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
			[newBrowseDomainDict setObject:[[[NSNumber alloc] initWithBool:YES] autorelease] forKey:(NSString *)SC_DYNDNS_ENABLED_KEY];
			
			[browseDomainsArray addObject:newBrowseDomainDict];
			[browseDomainsArray sortUsingFunction:MyDomainArrayCompareFunction context:nil];
			[browseDomainList reloadData];
			[self updateApplyButtonState];
		}
    }
}


-(void)validateTextFields
{
    [hostName validateEditing];
    [browseDomainsComboBox validateEditing];
    [regDomainsComboBox validateEditing];    
}


- (IBAction)changeButtonPressed:(id)sender
{
    NSString * keyName;
    
    [self disableControls];
    [self validateTextFields];
    [mainWindow makeFirstResponder:nil];
	[browseDomainList deselectAll:sender];

    if ([sender isEqualTo:hostNameSharedSecretButton]) {		
        if (hostNameSharedSecretValue) {
			[sharedSecretValue setStringValue:hostNameSharedSecretValue];
        } else if ((keyName = [self sharedSecretKeyName:[hostName stringValue]]) != NULL) {
			[sharedSecretName setStringValue:keyName];
            [sharedSecretValue setStringValue:@"****************"];
		} else {
			[sharedSecretName setStringValue:[hostName stringValue]];
            [sharedSecretValue setStringValue:@""];
        }

    } else {        
        if (regSharedSecretValue) {
			[sharedSecretValue setStringValue:regSharedSecretValue];
        } else if ((keyName = [self sharedSecretKeyName:[regDomainsComboBox stringValue]]) != NULL) {
			[sharedSecretName setStringValue:keyName];
            [sharedSecretValue setStringValue:@"****************"];
		} else {
			[sharedSecretName setStringValue:[regDomainsComboBox stringValue]];
            [sharedSecretValue setStringValue:@""];
        }
    }
    
    [sharedSecretWindow resignFirstResponder];

    if ([[sharedSecretName stringValue] length] > 0) [sharedSecretWindow makeFirstResponder:sharedSecretValue];
    else                                             [sharedSecretWindow makeFirstResponder:sharedSecretName];
    
    [NSApp beginSheet:sharedSecretWindow modalForWindow:mainWindow modalDelegate:self
            didEndSelector:@selector(sharedSecretSheetDidEnd:returnCode:contextInfo:) contextInfo:sender];
}


- (IBAction)wideAreaCheckBoxChanged:(id)sender
{    
    [self toggleWideAreaBonjour:[sender state]];
    [self updateApplyButtonState];
    [mainWindow makeFirstResponder:nil];
}


- (void)updateApplyButtonState
{
    NSString *hostNameString  = [hostName stringValue];
    NSString *regDomainString = [regDomainsComboBox stringValue];
    
    NSComparisonResult hostNameResult  = [hostNameString    compare:currentHostName];
    NSComparisonResult regDomainResult = [regDomainString  compare:currentRegDomain];

    if ((currentHostName && (hostNameResult != NSOrderedSame)) ||
        (currentRegDomain && (regDomainResult != NSOrderedSame) && ([wideAreaCheckBox state])) ||
        (currentHostName == nil && ([hostNameString length]) > 0) ||
        (currentRegDomain == nil && ([regDomainString length]) > 0) ||
        (currentWideAreaState  != [wideAreaCheckBox state]) ||
        (hostNameSharedSecretValue != nil) ||
        (regSharedSecretValue != nil) ||
		(browseDomainsArray && [browseDomainsArray isEqualToArray:currentBrowseDomainsArray] == NO))
    {
        [self enableApplyButton];
    } else {
        [self disableApplyButton];
    }
}



- (void)controlTextDidChange:(NSNotification *)notification;
{
	(void)notification; // Unused
    [self updateApplyButtonState];
}



- (IBAction)comboAction:(id)sender;
{
	(void)sender; // Unused
    [self updateApplyButtonState];
}


- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)ind
{
    NSString *domain = nil;
    if      ([aComboBox isEqualTo:browseDomainsComboBox]) domain = [browseDataSource objectAtIndex:ind];
    else if ([aComboBox isEqualTo:regDomainsComboBox])    domain = [registrationDataSource objectAtIndex:ind];
    return domain;
}



- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox
{
    int count = 0;
    if      ([aComboBox isEqualTo:browseDomainsComboBox]) count = [browseDataSource count];
    else if ([aComboBox isEqualTo:regDomainsComboBox])    count = [registrationDataSource count];
    return count;
}


- (NSMutableArray *)browseDataSource
{
    return browseDataSource;
}


- (NSMutableArray *)registrationDataSource
{
    return registrationDataSource;
}


- (NSComboBox *)browseDomainsComboBox
{
    return browseDomainsComboBox;
}


- (NSComboBox *)regDomainsComboBox
{
    return regDomainsComboBox;
}


- (NSString *)currentRegDomain
{
    return currentRegDomain;
}


- (NSMutableArray *)defaultBrowseDomainsArray
{
    return defaultBrowseDomainsArray;
}


- (NSArray *)currentBrowseDomainsArray
{
    return currentBrowseDomainsArray;
}


- (NSString *)currentHostName
{
    return currentHostName;
}


- (NSString *)defaultRegDomain
{
	return defaultRegDomain;
}


- (void)setDefaultRegDomain:(NSString *)domain
{
	[defaultRegDomain release];
	defaultRegDomain = domain;
	[defaultRegDomain retain];
}


- (void)didSelect
{
    [super didSelect];
    mainWindow = [[self mainView] window];
}


- (void)mainViewDidLoad
{	
    [comboAuthButton setString:"system.preferences"];
    [comboAuthButton setDelegate:self];
    [comboAuthButton updateStatus:nil];
    [comboAuthButton setAutoupdate:YES];
}



- (IBAction)applyClicked:(id)sender
{
	(void)sender; // Unused
    [self applyCurrentState];
}


- (void)applyCurrentState
{
    [self validateTextFields];

    if (toolInstalled == YES) {
        [self savePreferences];
        [self disableApplyButton];
        [mainWindow makeFirstResponder:nil];
    }
}


- (void)enableApplyButton
{
    [applyButton setEnabled:YES];
    [revertButton setEnabled:YES];
    prefsNeedUpdating = YES;
}


- (void)disableApplyButton
{
    [applyButton setEnabled:NO];
    [revertButton setEnabled:NO];
    prefsNeedUpdating = NO;
}


- (void)toggleWideAreaBonjour:(BOOL)state
{
	[wideAreaCheckBox setState:state];
	[regDomainsComboBox setEnabled:state];
	[registrationSharedSecretButton setEnabled:state];
}


- (IBAction)revertClicked:(id)sender;
{
    [self restorePreferences];
	[browseDomainList deselectAll:sender];
    [mainWindow makeFirstResponder:nil];
}


- (void)restorePreferences
{
    [self setupInitialValues];
}


- (void)savePanelWillClose:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	(void)sheet; // Unused
    DNSServiceDiscoveryPref * me = (DNSServiceDiscoveryPref *)contextInfo;
    
    if (returnCode == NSAlertDefaultReturn) {
        [me applyCurrentState];
    } else if (returnCode == NSAlertAlternateReturn ) {
        [me restorePreferences];
    }
    
    [me enableControls];
    [me replyToShouldUnselect:(returnCode != NSAlertOtherReturn)];
}


-(SecKeychainItemRef)copyKeychainItemforDomain:(NSString *)domain
{
    const char * serviceName = [domain UTF8String];
    UInt32 type              = 'ddns';
	UInt32 typeLength        = sizeof(type);

	SecKeychainAttribute attrs[] = { { kSecServiceItemAttr, strlen(serviceName),   (char *)serviceName },
                                     { kSecTypeItemAttr,             typeLength, (UInt32 *)&type       } };
    
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
    SecKeychainSearchRef searchRef;
    SecKeychainItemRef itemRef = NULL;
    OSStatus err;
    
    err = SecKeychainSearchCreateFromAttributes(NULL, kSecGenericPasswordItemClass, &attributes, &searchRef);
	if (err == noErr) {
		err = SecKeychainSearchCopyNext(searchRef, &itemRef);
		if (err != noErr) itemRef = NULL;
	}
	return itemRef;
}


-(NSString *)sharedSecretKeyName:(NSString * )domain
{
	SecKeychainItemRef itemRef = NULL;
	NSString *keyName = nil;
	OSStatus err;
	    
	err = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	assert(err == noErr);

	itemRef = [self copyKeychainItemforDomain:[domain lowercaseString]];
    if (itemRef) {
        UInt32 tags[1];
        SecKeychainAttributeInfo attrInfo;
        SecKeychainAttributeList *attrList = NULL;
        SecKeychainAttribute attribute;
		unsigned int i;
		
        tags[0] = kSecAccountItemAttr;
        attrInfo.count = 1;
        attrInfo.tag = tags;
        attrInfo.format = NULL;
					
        err = SecKeychainItemCopyAttributesAndData(itemRef,  &attrInfo, NULL, &attrList, NULL, NULL);
        if (err == noErr) {
            for (i = 0; i < attrList->count; i++) {
                attribute = attrList->attr[i];
                if (attribute.tag == kSecAccountItemAttr) {
                    keyName = [[NSString alloc] initWithBytes:attribute.data length:attribute.length encoding:NSUTF8StringEncoding];
                    break;
                }
            }
            if (attrList) (void)SecKeychainItemFreeAttributesAndData(attrList, NULL);
        }
		CFRelease(itemRef);
	}
    return keyName;
}


-(NSString *)domainForHostName:(NSString *)hostNameString
{
    NSString * domainName = nil;
    char text[64];
    char * ptr = NULL;
    
    ptr = (char *)[hostNameString UTF8String];
    if (ptr) {
        ptr = (char *)GetNextLabel(ptr, text);
        domainName = [[NSString alloc] initWithUTF8String:(const char *)ptr];             
    }
    return ([domainName autorelease]);
}


- (NSData *)dataForDomain:(NSString *)domainName isEnabled:(BOOL)enabled
{
	NSMutableArray      *domainsArray; 
	NSMutableDictionary *domainDict = nil;
	
	if (domainName && [domainName length] > 0) {
		domainDict= [[[NSMutableDictionary alloc] initWithCapacity:2] autorelease];
		[domainDict setObject:domainName forKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
		[domainDict setObject:[[[NSNumber alloc] initWithBool:enabled] autorelease] forKey:(NSString *)SC_DYNDNS_ENABLED_KEY];
	}
	domainsArray = [[[NSMutableArray alloc] initWithCapacity:1] autorelease];
	if (domainDict) [domainsArray addObject:domainDict];
	return [NSArchiver archivedDataWithRootObject:domainsArray];
}


- (NSData *)dataForDomainArray:(NSArray *)domainArray
{
	return [NSArchiver archivedDataWithRootObject:domainArray];
}


- (NSData *)dataForSharedSecret:(NSString *)secret domain:(NSString *)domainName key:(NSString *)keyName
{
	NSMutableDictionary *sharedSecretDict = [[[NSMutableDictionary alloc] initWithCapacity:3] autorelease];
	[sharedSecretDict setObject:secret forKey:(NSString *)SC_DYNDNS_SECRET_KEY];
	[sharedSecretDict setObject:[domainName lowercaseString] forKey:(NSString *)SC_DYNDNS_DOMAIN_KEY];
	[sharedSecretDict setObject:keyName forKey:(NSString *)SC_DYNDNS_KEYNAME_KEY];
	return [NSArchiver archivedDataWithRootObject:sharedSecretDict];
}


-(void)savePreferences
{
    NSString      *hostNameString               = [hostName stringValue];
    NSString      *browseDomainString           = [browseDomainsComboBox stringValue];
    NSString      *regDomainString              = [regDomainsComboBox stringValue];
    NSString      *tempHostNameSharedSecretName = hostNameSharedSecretName;
    NSString      *tempRegSharedSecretName      = regSharedSecretName;
	NSData        *browseDomainData             = nil;
    BOOL          regSecretWasSet               = NO;
    BOOL          hostSecretWasSet              = NO;
    OSStatus      err                           = noErr;

	hostNameString                = [self trimCharactersFromDomain:hostNameString];
	browseDomainString            = [self trimCharactersFromDomain:browseDomainString];
	regDomainString               = [self trimCharactersFromDomain:regDomainString];
	tempHostNameSharedSecretName  = [self trimCharactersFromDomain:tempHostNameSharedSecretName];
	tempRegSharedSecretName       = [self trimCharactersFromDomain:tempRegSharedSecretName];
	
	[hostName setStringValue:hostNameString];
	[regDomainsComboBox setStringValue:regDomainString];
    
    // Convert Shared Secret account names to lowercase.
    tempHostNameSharedSecretName = [tempHostNameSharedSecretName lowercaseString];
    tempRegSharedSecretName      = [tempRegSharedSecretName lowercaseString];
    
    // Save hostname shared secret.
    if ([hostNameSharedSecretName length] > 0 && ([hostNameSharedSecretValue length] > 0)) {
		SetKeyForDomain((CFDataRef)[self dataForSharedSecret:hostNameSharedSecretValue domain:hostNameString key:tempHostNameSharedSecretName]);
        [hostNameSharedSecretValue release];
        hostNameSharedSecretValue = nil;
        hostSecretWasSet = YES;
    }
    
    // Save registration domain shared secret.
    if (([regSharedSecretName length] > 0) && ([regSharedSecretValue length] > 0)) {
		SetKeyForDomain((CFDataRef)[self dataForSharedSecret:regSharedSecretValue domain:regDomainString key:tempRegSharedSecretName]);
        [regSharedSecretValue release];
        regSharedSecretValue = nil;
        regSecretWasSet = YES;
    }

    // Save hostname.
    if ((currentHostName == NULL) || [currentHostName compare:hostNameString] != NSOrderedSame) {
		err = WriteHostname((CFDataRef)[self dataForDomain:hostNameString isEnabled:YES]);
		if (err != noErr) NSLog(@"WriteHostname returned %d\n", err);
        currentHostName = [hostNameString copy];
    } else if (hostSecretWasSet) {
		WriteHostname((CFDataRef)[self dataForDomain:@"" isEnabled:NO]);
		usleep(200000);  // Temporary hack
        if ([currentHostName length] > 0) WriteHostname((CFDataRef)[self dataForDomain:(NSString *)currentHostName isEnabled:YES]);
    }
    
    // Save browse domain.
	if (browseDomainsArray && [browseDomainsArray isEqualToArray:currentBrowseDomainsArray] == NO) {
		browseDomainData = [self dataForDomainArray:browseDomainsArray];
		err = WriteBrowseDomain((CFDataRef)browseDomainData);
		if (err != noErr) NSLog(@"WriteBrowseDomain returned %d\n", err);
		currentBrowseDomainsArray = [browseDomainsArray copy];
    }
	
    // Save registration domain.
    if ((currentRegDomain == NULL) || ([currentRegDomain compare:regDomainString] != NSOrderedSame) || (currentWideAreaState != [wideAreaCheckBox state])) {

		err = WriteRegistrationDomain((CFDataRef)[self dataForDomain:regDomainString isEnabled:[wideAreaCheckBox state]]);
		if (err != noErr) NSLog(@"WriteRegistrationDomain returned %d\n", err);
        
		if (currentRegDomain) CFRelease(currentRegDomain);
        currentRegDomain = [regDomainString copy];

        if ([currentRegDomain length] > 0) {
			currentWideAreaState = [wideAreaCheckBox state];
            [registrationDataSource removeObject:regDomainString];
            [registrationDataSource addObject:currentRegDomain];
            [registrationDataSource sortUsingFunction:MyArrayCompareFunction context:nil];
            [regDomainsComboBox reloadData];
        } else {
			currentWideAreaState = NO;
			[self toggleWideAreaBonjour:NO];
            if (defaultRegDomain != nil) [regDomainsComboBox setStringValue:defaultRegDomain];
		}
    } else if (regSecretWasSet) {
        WriteRegistrationDomain((CFDataRef)[self dataForDomain:@"" isEnabled:NO]);
		usleep(200000);  // Temporary hack
        if ([currentRegDomain length] > 0) WriteRegistrationDomain((CFDataRef)[self dataForDomain:currentRegDomain isEnabled:currentWideAreaState]);
    }
}   


- (NSPreferencePaneUnselectReply)shouldUnselect
{
#if 1
    if (prefsNeedUpdating == YES) {
    
        [self disableControls];
        
        NSBeginAlertSheet(
                    @"Apply Configuration Changes?",
                    @"Apply",
                    @"Don't Apply",
                    @"Cancel",
                    mainWindow,
                    self,
                    @selector( savePanelWillClose:returnCode:contextInfo: ),
                    NULL,
                    (void *) self, // sender,
                    @"" );
        return NSUnselectLater;
    }
#endif
    
    return NSUnselectNow;
}


-(void)disableControls
{
    [hostName setEnabled:NO];
    [hostNameSharedSecretButton setEnabled:NO];
    [browseDomainsComboBox setEnabled:NO];
    [applyButton setEnabled:NO];
    [revertButton setEnabled:NO];
    [wideAreaCheckBox setEnabled:NO];
    [regDomainsComboBox setEnabled:NO];
    [registrationSharedSecretButton setEnabled:NO];
    [statusImageView setEnabled:NO];
	
	browseDomainListEnabled = NO;
	[browseDomainList deselectAll:self];
	[browseDomainList setEnabled:NO];
	
	[addBrowseDomainButton setEnabled:NO];
	[removeBrowseDomainButton setEnabled:NO];
}


- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row;
{
	(void)row; // Unused
	(void)tableView; // Unused
	return browseDomainListEnabled;
}


-(void)enableControls
{
    [hostName setEnabled:YES];
    [hostNameSharedSecretButton setEnabled:YES];
    [browseDomainsComboBox setEnabled:YES];
    [wideAreaCheckBox setEnabled:YES];
    [registrationSharedSecretButton setEnabled:YES];
    [self toggleWideAreaBonjour:[wideAreaCheckBox state]];
    [statusImageView setEnabled:YES];
	[addBrowseDomainButton setEnabled:YES];

	[browseDomainList setEnabled:YES];
	[browseDomainList deselectAll:self];
	browseDomainListEnabled = YES;

	[removeBrowseDomainButton setEnabled:[browseDomainList numberOfSelectedRows]];
	[applyButton setEnabled:prefsNeedUpdating];
	[revertButton setEnabled:prefsNeedUpdating];
}


- (void)authorizationViewDidAuthorize:(SFAuthorizationView *)view
{
	(void)view; // Unused
    [self enableControls];
}


- (void)authorizationViewDidDeauthorize:(SFAuthorizationView *)view
{    
	(void)view; // Unused
    [self disableControls];
}

@end


// Note: The C preprocessor stringify operator ('#') makes a string from its argument, without macro expansion
// e.g. If "version" is #define'd to be "4", then STRINGIFY_AWE(version) will return the string "version", not "4"
// To expand "version" to its value before making the string, use STRINGIFY(version) instead
#define STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s) #s
#define STRINGIFY(s) STRINGIFY_ARGUMENT_WITHOUT_EXPANSION(s)

// NOT static -- otherwise the compiler may optimize it out
// The "@(#) " pattern is a special prefix the "what" command looks for
const char VersionString_SCCS[] = "@(#) Bonjour Preference Pane " STRINGIFY(mDNSResponderVersion) " (" __DATE__ " " __TIME__ ")";

#if _BUILDING_XCODE_PROJECT_
// If the process crashes, then this string will be magically included in the automatically-generated crash log
const char *__crashreporter_info__ = VersionString_SCCS + 5;
asm(".desc ___crashreporter_info__, 0x10");
#endif
