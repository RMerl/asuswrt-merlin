/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):

$Log: helper-server.h,v $
Revision 1.5  2009/02/06 03:06:49  mcguire
<rdar://problem/5858533> Adopt vproc_transaction API in mDNSResponder

Revision 1.4  2009/01/28 03:17:19  mcguire
<rdar://problem/5858535> helper: Adopt vproc_transaction API

Revision 1.3  2007/09/07 22:44:03  mcguire
<rdar://problem/5448420> Move CFUserNotification code to mDNSResponderHelper

Revision 1.2  2007/08/23 21:39:01  cheshire
Made code layout style consistent with existing project style; added $Log header

Revision 1.1  2007/08/08 22:34:58  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root
 */

#ifndef H_HELPER_SERVER_H
#define H_HELPER_SERVER_H

extern void helplog(int, const char *, ...);
extern void pause_idle_timer(void);
extern void unpause_idle_timer(void);
extern void update_idle_timer(void);
extern uid_t mDNSResponderUID;
extern uid_t mDNSResponderGID;
extern CFRunLoopRef gRunLoop;
#define debug(...) debug_(__func__, __VA_ARGS__)
extern void debug_(const char *func, const char *fmt, ...);

#endif /* H_HELPER_SERVER_H */
