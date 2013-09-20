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
 */
/*
 * This is the interface to a routine to notify the rc driver that it should
 * take some action.
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of ASUSTeK Inc..
 */

#ifndef NOTIFY_RC_H
#define NOTIFY_RC_H

#include <typedefs.h>


extern int notify_rc(const char *event_name);
extern int notify_rc_after_wait(const char *event_name);
extern int notify_rc_and_wait(const char *event_name);
extern int notify_rc_and_wait_1min(const char *event_name);
extern int notify_rc_and_wait_2min(const char *event_name);


#endif /* NOTIFY_RC_H */
