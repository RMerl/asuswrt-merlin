/* $Id: RootViewController.h 3552 2011-05-05 05:50:48Z nanang $ */
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
#import "TestViewController.h"

@interface RootViewController : UITableViewController {
    NSMutableArray *titles;
    NSMutableArray *menus;
    
    TestViewController *testView;
}

@property (nonatomic, retain) NSMutableArray *titles;
@property (nonatomic, retain) NSMutableArray *menus;
@property (nonatomic, retain) TestViewController *testView;

@end
