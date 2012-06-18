/* -*- Mode: Java; tab-width: 4 -*-
 *
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

$Log: RegisterRecordListener.java,v $
Revision 1.2  2006/08/14 23:25:08  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.1  2006/06/20 23:00:12  rpantos
<rdar://problem/3839132> Java needs to implement DNSServiceRegisterRecord equivalent


 */


package	com.apple.dnssd;


/**	A listener that receives results from {@link DNSSDRecordRegistrar#registerRecord}. */

public interface RegisterRecordListener extends BaseListener
{
	/** Called when a record registration succeeds.<P> 

		@param	record
					A {@link DNSRecord}. 
		<P>
		@param	flags
					Currently ignored, reserved for future use.
		<P>
	*/
	void	recordRegistered( DNSRecord record, int flags);
}

