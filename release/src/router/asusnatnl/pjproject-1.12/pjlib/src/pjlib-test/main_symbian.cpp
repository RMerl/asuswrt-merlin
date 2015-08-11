//Auto-generated file. Please do not modify.
//#include <e32cmn.h>

//#pragma data_seg(".SYMBIAN")
//__EMULATOR_IMAGE_HEADER2 (0x1000007a,0x00000000,0x00000000,EPriorityForeground,0x00000000u,0x00000000u,0x00000000,0x00000000,0x00000000,0)
//#pragma data_seg()

#include "test.h"
#include <stdlib.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/unicode.h>
#include <stdio.h>

#include <e32std.h>

#if 0
int main()
{
    int err = 0;
    int exp = 0;

    err = test_main();
    //err = test_main();

    if (err)
	return err;
    return exp;
    //return 0;
}

#else
#include <pj/os.h>

#include <e32base.h>
#include <e32std.h>
#include <e32cons.h>            // Console



//  Global Variables

LOCAL_D CConsoleBase* console;  // write all messages to this


class MyScheduler : public CActiveScheduler
{
public:
    MyScheduler()
    {}

    void Error(TInt aError) const;
};

void MyScheduler::Error(TInt aError) const
{
    PJ_UNUSED_ARG(aError);
}

LOCAL_C void DoStartL()
    {
    // Create active scheduler (to run active objects)
    CActiveScheduler* scheduler = new (ELeave) MyScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    test_main();

    CActiveScheduler::Install(NULL);
    CleanupStack::Pop(scheduler);
    delete scheduler;
    }

#define WRITE_TO_DEBUG_CONSOLE

#ifdef WRITE_TO_DEBUG_CONSOLE
#include<e32debug.h>
#endif

//  Global Functions
static void log_writer(int level, const char *buf, int len)
{
    static wchar_t buf16[PJ_LOG_MAX_SIZE];

    PJ_UNUSED_ARG(level);
    
    pj_ansi_to_unicode(buf, len, buf16, PJ_ARRAY_SIZE(buf16));
    buf16[len] = 0;
    buf16[len+1] = 0;
    
    TPtrC16 aBuf((const TUint16*)buf16, (TInt)len);
    console->Write(aBuf);
    
#ifdef WRITE_TO_DEBUG_CONSOLE
    RDebug::Print(aBuf);
#endif
}


GLDEF_C TInt E32Main()
    {
    // Create cleanup stack
    __UHEAP_MARK;
    CTrapCleanup* cleanup = CTrapCleanup::New();

    // Create output console
    TRAPD(createError, console = Console::NewL(_L("Console"), TSize(KConsFullScreen,KConsFullScreen)));
    if (createError)
        return createError;

    pj_log_set_log_func(&log_writer);

    // Run application code inside TRAP harness, wait keypress when terminated
    TRAPD(mainError, DoStartL());
    if (mainError)
        console->Printf(_L(" failed, leave code = %d"), mainError);
    
    console->Printf(_L(" [press any key]\n"));
    console->Getch();
    
    delete console;
    delete cleanup;
    
    CloseSTDLIB(); 
    
    __UHEAP_MARKEND;
    
    return KErrNone;
    }

#endif	/* if 0 */

