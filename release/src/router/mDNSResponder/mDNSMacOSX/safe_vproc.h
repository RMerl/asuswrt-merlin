/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

$Log: safe_vproc.h,v $
Revision 1.1  2009/02/06 03:06:49  mcguire
<rdar://problem/5858533> Adopt vproc_transaction API in mDNSResponder

 */

#ifndef __SAFE_VPROC_H
#define __SAFE_VPROC_H

extern void safe_vproc_transaction_begin(void);
extern void safe_vproc_transaction_end(void);

#endif /* __SAFE_VPROC_H */
