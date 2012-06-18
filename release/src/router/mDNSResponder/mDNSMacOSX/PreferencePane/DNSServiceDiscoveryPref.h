/*
    File: DNSServiceDiscoveryPref.h

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

$Log: DNSServiceDiscoveryPref.h,v $
Revision 1.6  2006/08/14 23:15:47  cheshire
Tidy up Change History comment

Revision 1.5  2005/02/26 00:44:24  cheshire
Restore default reg domain if user deletes text and clicks "apply"

Revision 1.4  2005/02/25 02:29:28  cheshire
Show yellow dot for "update in progress"

Revision 1.3  2005/02/10 22:35:19  cheshire
<rdar://problem/3727944> Update name

Revision 1.2  2005/02/08 01:32:05  cheshire
Add trimCharactersFromDomain routine to strip leading and trailing
white space and punctuation from user-entered fields.

Revision 1.1  2005/02/05 01:59:19  cheshire
Add Preference Pane to facilitate testing of DDNS & wide-area features

*/

#import <Cocoa/Cocoa.h>
#import <PreferencePanes/PreferencePanes.h>
#import <CoreFoundation/CoreFoundation.h>
#import <SecurityInterface/SFAuthorizationView.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <dns_sd.h>

typedef struct MyDNSServiceState {
    DNSServiceRef       service;
    CFRunLoopSourceRef  source;
    CFSocketRef         socket;
} MyDNSServiceState;


@interface DNSServiceDiscoveryPref : NSPreferencePane 
{
    IBOutlet NSTextField         *hostName;
    IBOutlet NSTextField         *sharedSecretName;
    IBOutlet NSSecureTextField   *sharedSecretValue;
    IBOutlet NSComboBox          *browseDomainsComboBox;
    IBOutlet NSComboBox          *regDomainsComboBox;
    IBOutlet NSButton            *wideAreaCheckBox;
    IBOutlet NSButton            *hostNameSharedSecretButton;
    IBOutlet NSButton            *registrationSharedSecretButton;
    IBOutlet NSButton            *applyButton;
    IBOutlet NSButton            *revertButton;
    IBOutlet NSWindow            *sharedSecretWindow;
	IBOutlet NSWindow            *addBrowseDomainWindow;
    IBOutlet NSButton            *addBrowseDomainButton;
    IBOutlet NSButton            *removeBrowseDomainButton;	
    IBOutlet NSButton            *browseOKButton;
    IBOutlet NSButton            *browseCancelButton;
	IBOutlet NSButton            *secretOKButton;
    IBOutlet NSButton            *secretCancelButton;
    IBOutlet NSImageView         *statusImageView;
    IBOutlet NSTabView           *tabView;
	IBOutlet NSTableView         *browseDomainList;
	IBOutlet SFAuthorizationView *comboAuthButton;

    NSWindow       *mainWindow;
    NSString       *currentHostName;
    NSString       *currentRegDomain;
	NSArray        *currentBrowseDomainsArray;
	NSMutableArray *browseDomainsArray;
	NSMutableArray *defaultBrowseDomainsArray;
    NSString       *defaultRegDomain;

    NSString       *hostNameSharedSecretName;
    NSString       *hostNameSharedSecretValue;
    NSString       *regSharedSecretName;
    NSString       *regSharedSecretValue;
    BOOL           currentWideAreaState;
    BOOL           prefsNeedUpdating;
    BOOL           toolInstalled;
	BOOL           browseDomainListEnabled;
    BOOL           justStartedEditing;
	NSImage        *successImage;
	NSImage        *inprogressImage;
	NSImage        *failureImage;
    
    MyDNSServiceState regQuery;
    MyDNSServiceState browseQuery;
    NSMutableArray    *browseDataSource;
    NSMutableArray    *registrationDataSource;
}

- (IBAction)applyClicked:(id)sender;
- (IBAction)enableBrowseDomainClicked:(id)sender;
- (IBAction)addBrowseDomainClicked:(id)sender;
- (IBAction)removeBrowseDomainClicked:(id)sender;
- (IBAction)revertClicked:(id)sender;
- (IBAction)changeButtonPressed:(id)sender;
- (IBAction)closeMyCustomSheet:(id)sender;
- (IBAction)comboAction:(id)sender;
- (IBAction)wideAreaCheckBoxChanged:(id)sender;


- (NSMutableArray *)browseDataSource;
- (NSMutableArray *)registrationDataSource;
- (NSComboBox *)browseDomainsComboBox;
- (NSComboBox *)regDomainsComboBox;
- (NSString *)currentRegDomain;
- (NSMutableArray *)defaultBrowseDomainsArray;
- (NSArray *)currentBrowseDomainsArray;
- (NSString *)currentHostName;
- (NSString *)defaultRegDomain;
- (void)setDefaultRegDomain:(NSString *)domain;



- (void)enableApplyButton;
- (void)disableApplyButton;
- (void)applyCurrentState;
- (void)setBrowseDomainsComboBox;
- (void)setupInitialValues;
- (void)startDomainBrowsing;
- (void)toggleWideAreaBonjour:(BOOL)state;
- (void)updateApplyButtonState;
- (void)enableControls;
- (void)disableControls;
- (void)validateTextFields;
- (void)readPreferences;
- (void)savePreferences;
- (void)restorePreferences;
- (void)watchForPreferenceChanges;
- (void)updateStatusImageView;


- (NSString *)sharedSecretKeyName:(NSString * )domain;
- (NSString *)domainForHostName:(NSString *)hostNameString;
- (int)statusForHostName:(NSString * )domain;
- (NSData *)dataForDomainArray:(NSArray *)domainArray;
- (NSData *)dataForDomain:(NSString *)domainName isEnabled:(BOOL)enabled;
- (NSData *)dataForSharedSecret:(NSString *)secret domain:(NSString *)domainName key:(NSString *)keyName;
- (BOOL)domainAlreadyInList:(NSString *)domainString;
- (NSString *)trimCharactersFromDomain:(NSString *)domain;


// Delegate methods
- (void)authorizationViewDidAuthorize:(SFAuthorizationView *)view;
- (void)authorizationViewDidDeauthorize:(SFAuthorizationView *)view;
- (void)mainViewDidLoad;
- (int)numberOfItemsInComboBox:(NSComboBox *)aComboBox;
- (id)comboBox:(NSComboBox *)aComboBox objectValueForItemAtIndex:(int)index;
- (void)controlTextDidChange:(NSNotification *) notification;

@end
