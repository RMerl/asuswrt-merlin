/*
    File: PrivilegedOperations.c

    Abstract: Interface to "ddnswriteconfig" setuid root tool.

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

$Log: PrivilegedOperations.c,v $
Revision 1.9  2008/06/26 17:34:18  mkrochma
<rdar://problem/6030630> Pref pane destroying shared "system.preferences" authorization right

Revision 1.8  2007/11/30 23:42:33  cheshire
Fixed compile warning: declaration of 'status' shadows a previous local

Revision 1.7  2007/02/09 00:39:06  cheshire
Fix compile warnings

Revision 1.6  2006/08/14 23:15:47  cheshire
Tidy up Change History comment

Revision 1.5  2006/06/10 02:07:11  mkrochma
Whoa.  Make sure code compiles before checking it in.

Revision 1.4  2006/05/27 02:32:38  mkrochma
Wait for installer script to exit before returning result

Revision 1.3  2005/06/04 04:50:00  cheshire
<rdar://problem/4138070> ddnswriteconfig (Bonjour PreferencePane) vulnerability
Use installtool instead of requiring ddnswriteconfig to self-install

Revision 1.2  2005/02/10 22:35:20  cheshire
<rdar://problem/3727944> Update name

Revision 1.1  2005/02/05 01:59:19  cheshire
Add Preference Pane to facilitate testing of DDNS & wide-area features

*/

#include "PrivilegedOperations.h"
#include "ConfigurationAuthority.h"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <AssertMacros.h>
#include <Security/Security.h>

Boolean	gToolApproved = false;

static pid_t	execTool(const char *args[])
// fork/exec and return new pid
{
	pid_t	child;
	
	child = vfork();
	if (child == 0)
	{
		execv(args[0], (char *const *)args);
printf("exec of %s failed; errno = %d\n", args[0], errno);
		_exit(-1);		// exec failed
	}
	else
		return child;
}

OSStatus EnsureToolInstalled(void)
// Make sure that the tool is installed in the right place, with the right privs, and the right version.
{
	CFURLRef		bundleURL;
	pid_t			toolPID;
	int				status;
	OSStatus		err = noErr;
	const char		*args[] = { kToolPath, "0", "V", NULL };
	char			toolSourcePath[PATH_MAX] = {};
	char			toolInstallerPath[PATH_MAX] = {};

	if (gToolApproved) 
		return noErr;

	// Check version of installed tool
	toolPID = execTool(args);
	if (toolPID > 0)
	{
		waitpid(toolPID, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == PRIV_OP_TOOL_VERS)
			return noErr;
	}

	// Locate our in-bundle copy of privop tool
	bundleURL = CFBundleCopyBundleURL(CFBundleGetBundleWithIdentifier(CFSTR("com.apple.preference.bonjour")) );
	if (bundleURL != NULL)
	{
		CFURLGetFileSystemRepresentation(bundleURL, false, (UInt8*) toolSourcePath, sizeof toolSourcePath);
		if (strlcat(toolSourcePath,    "/Contents/Resources/" kToolName,      sizeof toolSourcePath   ) >= sizeof toolSourcePath   ) return(-1);
		CFURLGetFileSystemRepresentation(bundleURL, false, (UInt8*) toolInstallerPath, sizeof toolInstallerPath);
		if (strlcat(toolInstallerPath, "/Contents/Resources/" kToolInstaller, sizeof toolInstallerPath) >= sizeof toolInstallerPath) return(-1);
	}
	else
		return coreFoundationUnknownErr;
	
	// Obtain authorization and run in-bundle copy as root to install it
	{
		AuthorizationItem		aewpRight = { kAuthorizationRightExecute, strlen(toolInstallerPath), toolInstallerPath, 0 };
		AuthorizationItemSet	rights = { 1, &aewpRight };
		AuthorizationRef		authRef;
		
		err = AuthorizationCreate(&rights, (AuthorizationEnvironment*) NULL,
					kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights | 
					kAuthorizationFlagPreAuthorize, &authRef);
		if (err == noErr)
		{
			char *installerargs[] = { toolSourcePath, NULL };
			err = AuthorizationExecuteWithPrivileges(authRef, toolInstallerPath, 0, installerargs, (FILE**) NULL);
			if (err == noErr) {
				int pid = wait(&status);
				if (pid > 0 && WIFEXITED(status)) {
					err = WEXITSTATUS(status);
					if (err == noErr) {
						gToolApproved = true;
					}
				} else {
					err = -1;
				}
			}
			(void) AuthorizationFree(authRef, kAuthorizationFlagDefaults);
		}
	}

	return err;
}


static OSStatus	ExecWithCmdAndParam(const char *subCmd, CFDataRef paramData)
// Execute our privop tool with the supplied subCmd and parameter
{
	OSStatus				err = noErr;
	int						commFD, dataLen;
	u_int32_t				len;
	pid_t					child;
	char					fileNum[16];
	UInt8					*buff;
	const char				*args[] = { kToolPath, NULL, "A", NULL, NULL };
	AuthorizationExternalForm	authExt;

	err = ExternalizeAuthority(&authExt);
	require_noerr(err, AuthFailed);

	dataLen = CFDataGetLength(paramData);
	buff = (UInt8*) malloc(dataLen * sizeof(UInt8));
	require_action(buff != NULL, AllocBuffFailed, err=memFullErr;);
	{
		CFRange	all = { 0, dataLen };
		CFDataGetBytes(paramData, all, buff);
	}

	commFD = fileno(tmpfile());
	sprintf(fileNum, "%d", commFD);
	args[1] = fileNum;
	args[3] = subCmd;

	// write authority to pipe
	len = 0;	// tag, unused
	write(commFD, &len, sizeof len);
	len = sizeof authExt;	// len
	write(commFD, &len, sizeof len);
	write(commFD, &authExt, len);

	// write parameter to pipe
	len = 0;	// tag, unused
	write(commFD, &len, sizeof len);
	len = dataLen;	// len
	write(commFD, &len, sizeof len);
	write(commFD, buff, len);

	child = execTool(args);
	if (child > 0) {
		int	status;
		waitpid(child, &status, 0);
		if (WIFEXITED(status))
			err = WEXITSTATUS(status);
		//fprintf(stderr, "child exited; status = %d (%ld)\n", status, err);
	}

	close(commFD);

	free(buff);
AllocBuffFailed:
AuthFailed:
	return err;
}

OSStatus
WriteBrowseDomain(CFDataRef domainArrayData)
{
	if (!CurrentlyAuthorized())
		return authFailErr;
	return ExecWithCmdAndParam("Wb", domainArrayData);
}

OSStatus
WriteRegistrationDomain(CFDataRef domainArrayData)
{
	if (!CurrentlyAuthorized())
		return authFailErr;
	return ExecWithCmdAndParam("Wd", domainArrayData);
}

OSStatus
WriteHostname(CFDataRef domainArrayData)
{
	if (!CurrentlyAuthorized())
		return authFailErr;
	return ExecWithCmdAndParam("Wh", domainArrayData);
}

OSStatus
SetKeyForDomain(CFDataRef secretData)
{
	if (!CurrentlyAuthorized())
		return authFailErr;
	return ExecWithCmdAndParam("Wk", secretData);
}
