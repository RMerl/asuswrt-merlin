/* $Id: FirstViewController.m 3550 2011-05-05 05:33:27Z nanang $ */
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
#import "FirstViewController.h"
#import "ipjsuaAppDelegate.h"


@implementation FirstViewController
@synthesize textField;
@synthesize textView;
@synthesize button1;
@synthesize text;
@synthesize hasInput;

- (BOOL)textFieldShouldReturn:(UITextField *)theTextField {
    // When the user presses return, take focus away from the text field so that the keyboard is dismissed.
    if (theTextField == textField) {
	[self.textField resignFirstResponder];
	self.hasInput = true;
        self.text = [textField.text stringByAppendingString:@"\n"];
	textField.text = @"";
    }
    return YES;
}


- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    // Dismiss the keyboard when the view outside the text field is touched.
    [textField resignFirstResponder];
    [super touchesBegan:touches withEvent:event];
}

/*
// The designated initializer. Override to perform setup that is required before the view is loaded.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    if (self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]) {
        // Custom initialization
    }
    return self;
}
*/

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView {
}
*/

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad {
    [super viewDidLoad];
    
    [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
    
    ipjsuaAppDelegate *appd = (ipjsuaAppDelegate *)[[UIApplication sharedApplication] delegate];
    appd.mainView = self;
    textField.delegate = self;
    [self.textView setFont:[UIFont fontWithName:@"Courier New" size:8.9]];
    [self.textField setEnabled: false];
    [button1 addTarget:self action:@selector(button1Pressed:) forControlEvents:(UIControlEvents)UIControlEventTouchDown];
}

- (void)button1Pressed:(id)sender {
    /* Clear the text view */
    self.textView.text = @"";
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationLandscapeRight);
}

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
    // Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}


- (void)dealloc {
    [super dealloc];
}

@end
