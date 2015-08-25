/* $Id: ipjsuaAppDelegate.m 3550 2011-05-05 05:33:27Z nanang $ */
/* 
 * Copyright (C) 2010-2011 Teluu Inc. (http://www.teluu.com)
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
#import <pjlib.h>
#import <pjsua.h>
#import "ipjsuaAppDelegate.h"

extern pj_log_func *log_cb;

@implementation ipjsuaAppDelegate
@synthesize window;
@synthesize tabBarController;
@synthesize mainView;
@synthesize cfgView;

/* Sleep interval duration */
#define SLEEP_INTERVAL	    0.5
/* Determine whether we should print the messages in the debugger
 * console as well
 */
#define DEBUGGER_PRINT	    1
/* Whether we should show pj log messages in the text area */
#define SHOW_LOG	    1
#define PATH_LENGTH	    PJ_MAXPATH
#define KEEP_ALIVE_INTERVAL 600

extern pj_bool_t app_restart;

char argv_buf[PATH_LENGTH];
char *argv[] = {"", "--config-file", argv_buf};

ipjsuaAppDelegate	*app;

bool			 app_running;
bool			 thread_quit;
NSMutableString		*mstr;
pj_thread_desc		 a_thread_desc;
pj_thread_t		*a_thread;
pjsua_call_id		 ccall_id;

pj_status_t app_init(int argc, char *argv[]);
pj_status_t app_main(void);
pj_status_t app_destroy(void);
void keepAliveFunction(int timeout);

void showMsg(const char *format, ...)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    va_list arg;
    
    va_start(arg, format);
    NSString *str = [[NSString alloc] initWithFormat:[NSString stringWithFormat:@"%s", format] arguments: arg];
#if DEBUGGER_PRINT
    NSLog(@"%@", str);
#endif
    va_end(arg);
    
    [mstr appendString:str];
    [pool release];
}

char * getInput(char *s, int n, FILE *stream)
{
    if (stream != stdin) {
	return fgets(s, n, stream);
    }
    
    app.mainView.hasInput = false;
    [app.mainView.textField setEnabled: true];
    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
    [mstr setString:@""];
    
    while (!thread_quit && !app.mainView.hasInput) {
	int ctr = 0;
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
	if (ctr == 4) {
	    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
	    [mstr setString:@""];
	    ctr = 0;
	}
	ctr++;
    }
    
    [app.mainView.text getCString:s maxLength:n encoding:NSASCIIStringEncoding];
    [app.mainView.textField setEnabled: false];    
    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:app.mainView.text waitUntilDone:NO];
    
    return s;
}

void showLog(int level, const char *data, int len)
{
    showMsg("%s", data);
}

pj_bool_t showNotification(pjsua_call_id call_id)
{
#ifdef __IPHONE_4_0
    ccall_id = call_id;

    // Create a new notification
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    UILocalNotification* alert = [[[UILocalNotification alloc] init] autorelease];
    if (alert)
    {
	alert.repeatInterval = 0;
	alert.alertBody = @"Incoming call received...";
	alert.alertAction = @"Answer";
	
	[[UIApplication sharedApplication] presentLocalNotificationNow:alert];
    }
    
    [pool release];
    
    return PJ_FALSE;
#else
    return PJ_TRUE;
#endif
}

- (void)answer_call {
    if (!pj_thread_is_registered())
    {
	pj_thread_register("ipjsua", a_thread_desc, &a_thread);
    }
    pjsua_call_answer(ccall_id, 200, NULL, NULL);
}

#ifdef __IPHONE_4_0
- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification {
    [app performSelectorOnMainThread:@selector(answer_call) withObject:nil waitUntilDone:YES];
}

- (void)keepAlive {
    if (!pj_thread_is_registered())
    {
	pj_thread_register("ipjsua", a_thread_desc, &a_thread);
    }    
    keepAliveFunction(KEEP_ALIVE_INTERVAL);
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    [app performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];
    [application setKeepAliveTimeout:KEEP_ALIVE_INTERVAL handler: ^{
	[app performSelectorOnMainThread:@selector(keepAlive) withObject:nil waitUntilDone:YES];
    }];
}

#endif

- (void)start_app {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    /* Wait until the view is ready */
    while (self.mainView == nil) {
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    }    

    [NSThread setThreadPriority:1.0];
    mstr = [NSMutableString stringWithCapacity:4196];
#if SHOW_LOG
    pj_log_set_log_func(&showLog);
    log_cb = &showLog;
#endif
    
    do {
	app_restart = PJ_FALSE;
	if (app_init(3, argv) != PJ_SUCCESS) {
	    NSString *str = @"Failed to initialize pjsua\n";
	    [app performSelectorOnMainThread:@selector(displayMsg:) withObject:str waitUntilDone:YES];
	} else {
	    app_running = true;
	    app_main();
	    
	    app_destroy();
	    /* This is on purpose */
	    app_destroy();
	}

	[app performSelectorOnMainThread:@selector(displayMsg:) withObject:mstr waitUntilDone:YES];
	[mstr setString:@""];
    } while (app_restart);
    
    [pool release];
}

- (void)displayMsg:(NSString *)str {
    self.mainView.textView.text = [self.mainView.textView.text stringByAppendingString:str];
    [self.mainView.textView scrollRangeToVisible:NSMakeRange([self.mainView.textView.text length] - 1, 1)];
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
    /* If there is no config file in the document dir, copy the default config file into the directory */ 
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *cfgPath = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"/config.cfg"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:cfgPath]) {
	NSString *resPath = [[NSBundle mainBundle] pathForResource:@"config" ofType:@"cfg"];
	NSString *cfg = [NSString stringWithContentsOfFile:resPath encoding:NSASCIIStringEncoding error:NULL];
	[cfg writeToFile:cfgPath atomically:NO encoding:NSASCIIStringEncoding error:NULL];
    }
    [cfgPath getCString:argv[2] maxLength:PATH_LENGTH encoding:NSASCIIStringEncoding];    
    
    // Add the tab bar controller's current view as a subview of the window
    [window addSubview:tabBarController.view];
    [window makeKeyAndVisible];

    app = self;
    app_running = false;
    thread_quit = false;
    /* Start pjsua thread */
    [NSThread detachNewThreadSelector:@selector(start_app) toTarget:self withObject:nil];
}

/*
// Optional UITabBarControllerDelegate method
- (void)tabBarController:(UITabBarController *)tabBarController didSelectViewController:(UIViewController *)viewController {
}
*/

/*
// Optional UITabBarControllerDelegate method
- (void)tabBarController:(UITabBarController *)tabBarController didEndCustomizingViewControllers:(NSArray *)viewControllers changed:(BOOL)changed {
}
*/


- (void)dealloc {
    thread_quit = true;
    [NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    
    [tabBarController release];
    [window release];
    [super dealloc];
}

@end

