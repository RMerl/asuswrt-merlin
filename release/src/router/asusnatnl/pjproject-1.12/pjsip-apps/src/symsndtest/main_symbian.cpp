/* $Id: main_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include <e32std.h>
#include <e32base.h>
#include <e32std.h>
#include <e32cons.h>
#include <stdlib.h>


//  Global Variables
CConsoleBase* console;

// Needed by APS
TPtrC APP_UID = _L("A000000E");

int app_main();


////////////////////////////////////////////////////////////////////////////

LOCAL_C void DoStartL()
{
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    app_main();

    CActiveScheduler::Install(NULL);
    CleanupStack::Pop(scheduler);
    delete scheduler;
}


////////////////////////////////////////////////////////////////////////////

// E32Main()
GLDEF_C TInt E32Main()
{
    // Mark heap usage
    __UHEAP_MARK;

    // Create cleanup stack
    CTrapCleanup* cleanup = CTrapCleanup::New();

    // Create output console
    TRAPD(createError, console = Console::NewL(_L("Console"), TSize(KConsFullScreen,KConsFullScreen)));
    if (createError)
        return createError;

    TRAPD(startError, DoStartL());

    //console->Printf(_L("[press any key to close]\n"));
    //console->Getch();

    delete console;
    delete cleanup;

    CloseSTDLIB();

    // Mark end of heap usage, detect memory leaks
    __UHEAP_MARKEND;
    return KErrNone;
}

