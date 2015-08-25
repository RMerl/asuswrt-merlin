/* $Id: RootViewController.m 3552 2011-05-05 05:50:48Z nanang $ */
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
#import "RootViewController.h"
#import "gui.h"
#import "systest.h"

/* Sleep interval duration, change to shorter duration for better
 * interaction but make sure there is enough time for the other
 * thread to do what it's supposed to do
 */
#define SLEEP_INTERVAL 0.5

@implementation RootViewController
@synthesize titles;
@synthesize menus;
@synthesize testView;

RootViewController	*view;

bool			systest_initialized;
bool			thread_quit;
gui_menu		*gmenu;
int			section;
int			row;
const char		*ctitle;
const char		*cmsg; 
enum gui_flag		cflag;

pj_status_t gui_init(gui_menu *menu)
{
    PJ_UNUSED_ARG(menu);
    return PJ_SUCCESS;
}

/* Run GUI main loop */
pj_status_t gui_start(gui_menu *menu)
{
    view.titles = [NSMutableArray arrayWithCapacity:menu->submenu_cnt];
    view.menus = [NSMutableArray arrayWithCapacity:menu->submenu_cnt];
    NSMutableArray *smenu;
    for (int i = 0; i < menu->submenu_cnt; i++) {
	NSString *str = [NSString stringWithFormat:@"%s" , menu->submenus[i]->title];
	[view.titles addObject: str];
	smenu = [NSMutableArray arrayWithCapacity:menu->submenus[i]->submenu_cnt];
	/* We do not need the last two menus of the "Tests" menu (NULL and "Exit"),
	 * so subtract by 2
	 */
	for (int j = 0; j < menu->submenus[i]->submenu_cnt - (i==0? 2: 0); j++) {
	    str = [NSString stringWithFormat:@"%s" , menu->submenus[i]->submenus[j]->title];
	    [smenu addObject:str];
	}
	[view.menus addObject:smenu];
    }
    gmenu = menu;
    
    return PJ_SUCCESS;
}

/* Signal GUI mainloop to stop */
void gui_destroy(void)
{
}

/* AUX: display messagebox */
enum gui_key gui_msgbox(const char *title, const char *message, enum gui_flag flag)
{
    ctitle = title;
    cmsg = message;
    cflag = flag;
    [view performSelectorOnMainThread:@selector(showMsg) withObject:nil waitUntilDone:YES];
    
    view.testView.key = 0;
    while(view.testView.key == 0) {
	/* Let the main thread do its job (refresh the view) while we wait for 
	 * user interaction (button click)
	 */
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    }
    
    if (view.testView.key == 1)
	return KEY_OK;
    else
	return (flag == WITH_YESNO? KEY_NO: KEY_CANCEL);
}

/* AUX: sleep */
void gui_sleep(unsigned sec)
{
    [NSThread sleepForTimeInterval:sec];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    view = self;
    
    /* Get a writable path for output files */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    [documentsDirectory getCString:doc_path maxLength:PATH_LENGTH encoding:NSASCIIStringEncoding];
    strncat(doc_path, "/", PATH_LENGTH);

    /* Get path for our test resources (the wav files) */
    NSString *resPath = [[NSBundle mainBundle] resourcePath];
    [resPath getCString:res_path maxLength:PATH_LENGTH encoding:NSASCIIStringEncoding];
    strncat(res_path, "/", PATH_LENGTH);
    
    systest_initialized = false;
    thread_quit = false;
    [NSThread detachNewThreadSelector:@selector(startTest) toTarget:self withObject:nil];
    /* Let our new thread initialize */
    while (!systest_initialized) {
	[NSThread sleepForTimeInterval:SLEEP_INTERVAL];
    }

    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

/*
 - (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
 }
 */
/*
 - (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
 }
 */
/*
 - (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
 }
 */
/*
 - (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
 }
 */

/*
 // Override to allow orientations other than the default portrait orientation.
 - (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations.
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
 }
 */

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
    // Release anything that can be recreated in viewDidLoad or on demand.
    // e.g. self.myOutlet = nil;
    self.titles = nil;
    self.menus = nil;
    
    thread_quit = true;
    [NSThread sleepForTimeInterval:SLEEP_INTERVAL];
}


#pragma mark Table view methods


- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return [titles count];
}


// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [[menus objectAtIndex:section] count];
}


- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    // The header for the section is the region name -- get this from the region at the section index.
    return [titles objectAtIndex:section];
}

// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
    }
    
    // Configure the cell.
    cell.textLabel.text = [[menus objectAtIndex:indexPath.section] objectAtIndex:indexPath.row];
    
    return cell;
}

- (void)startTest {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    if (systest_init() != PJ_SUCCESS) {
	[pool release];
	return;
    }
    
    systest_run();
    
    systest_initialized = 1;
    while(!thread_quit) {
	section = -1;
	while (section == -1) {
	    [NSThread sleepForTimeInterval:SLEEP_INTERVAL];
	}
	(*gmenu->submenus[section]->submenus[row]->handler)();
	cmsg = NULL;
	[view performSelectorOnMainThread:@selector(showMsg) withObject:nil waitUntilDone:YES];
    }
    
    systest_deinit();
    [pool release];
}

- (void)showMsg {
    if (cmsg == NULL) {
	self.testView.testDesc.text = [self.testView.testDesc.text stringByAppendingString: @"Finished"];
	[self.testView.testDesc scrollRangeToVisible:NSMakeRange([self.testView.testDesc.text length] - 1, 1)];
	[self.testView.button1 setHidden:true];
	[self.testView.button2 setHidden:true];
	return;
    }
    self.testView.title = [NSString stringWithFormat:@"%s", ctitle];
    self.testView.testDesc.text = [self.testView.testDesc.text stringByAppendingString: [NSString stringWithFormat:@"%s\n\n", cmsg]];
    [self.testView.testDesc scrollRangeToVisible:NSMakeRange([self.testView.testDesc.text length] - 1, 1)];
    
    [self.testView.button1 setHidden:false];
    [self.testView.button2 setHidden:false];
    if (cflag == WITH_YESNO) {
	[self.testView.button1 setTitle:@"Yes" forState:UIControlStateNormal];
	[self.testView.button2 setTitle:@"No" forState:UIControlStateNormal];
    } else if (cflag == WITH_OK) {
	[self.testView.button1 setTitle:@"OK" forState:UIControlStateNormal];
	[self.testView.button2 setHidden:true];
    } else if (cflag == WITH_OKCANCEL) {
	[self.testView.button1 setTitle:@"OK" forState:UIControlStateNormal];
	[self.testView.button2 setTitle:@"Cancel" forState:UIControlStateNormal];
    }
}


// Override to support row selection in the table view.
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    
    // Navigation logic may go here -- for example, create and push another view controller.
    // AnotherViewController *anotherViewController = [[AnotherViewController alloc] initWithNibName:@"AnotherView" bundle:nil];
    // [self.navigationController pushViewController:anotherViewController animated:YES];
    // [anotherViewController release];
    
    if (self.testView == nil) {
	self.testView = [[TestViewController alloc] initWithNibName:@"TestViewController" bundle:[NSBundle mainBundle]];
    }
    
    [self.navigationController pushViewController:self.testView animated:YES];
    self.testView.title = [[menus objectAtIndex:indexPath.section] objectAtIndex:indexPath.row];
    self.testView.testDesc.text = @"";
    section = indexPath.section;
    row = indexPath.row;
}


/*
 // Override to support conditional editing of the table view.
 - (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the specified item to be editable.
    return YES;
 }
 */


/*
 // Override to support editing the table view.
 - (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
 
    if (editingStyle == UITableViewCellEditingStyleDelete) {
	// Delete the row from the data source.
	[tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
    }   
    else if (editingStyle == UITableViewCellEditingStyleInsert) {
	// Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view.
    }   
 }
 */


/*
 // Override to support rearranging the table view.
 - (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath {
 }
 */


/*
 // Override to support conditional rearranging of the table view.
 - (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the item to be re-orderable.
    return YES;
 }
 */


- (void)dealloc {
    [super dealloc];
}


@end

