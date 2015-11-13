/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright Copyright (C) 2015, AAA Internet Publishing, Inc. (DBA WTFast)
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
 

#include "rc.h"

void start_wtfast (void)
{
	char *argv[5];
	int argc;
	pid_t pid;

	/*
	 * gpnrd is dependant on wtfd, so start the
	 * latter first.  If either exists in /jffs
	 * (which is presistent across router reboots),
	 * execute that one rather than the copy in
	 * /usr/sbin.  This helps facilitate testing
	 * and deploying new builds. :-)
	 */

	if(getpid()!=1) {
		notify_rc("start_wtfast");
		return;
	}

	killall ("wtfslhd", SIGTERM);

	argv[0] = "wtfslhd";
	argv[1] = "-d";
	argc = 2;

	argv[argc] = NULL;
	_eval(argv, NULL, 0, &pid);

}


void stop_wtfast (void)
{
	killall ("wtfslhd", SIGTERM);
}
