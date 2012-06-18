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

$Log: DomainListener.java,v $
Revision 1.3  2006/08/14 23:25:08  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.2  2004/04/30 21:48:27  rpantos
Change line endings for CVS.

Revision 1.1  2004/04/30 16:29:35  rpantos
First checked in.

 */


package	com.apple.dnssd;


/**
	A listener that receives results from {@link DNSSD#enumerateDomains}.
*/

public interface DomainListener extends BaseListener
{
	/** Called to report discovered domains.<P> 

		@param	domainEnum
					The active domain enumerator.
		@param	flags
					Possible values are: DNSSD.MORE_COMING, DNSSD.DEFAULT
		<P>
		@param	ifIndex
					Specifies the interface on which the domain exists.  (The index for a given 
					interface is determined via the if_nametoindex() family of calls.)  
		<P>
		@param	domain
					The name of the domain.
	*/
	void	domainFound( DNSSDService domainEnum, int flags, int ifIndex, String domain);

	/** Called to report that a domain has disappeared.<P> 

		@param	domainEnum
					The active domain enumerator.
		@param	flags
					Possible values are: DNSSD.MORE_COMING, DNSSD.DEFAULT
		<P>
		@param	ifIndex
					Specifies the interface on which the domain exists.  (The index for a given 
					interface is determined via the if_nametoindex() family of calls.)  
		<P>
		@param	domain
					The name of the domain.
	*/
	void	domainLost( DNSSDService domainEnum, int flags, int ifIndex, String domain);
}

