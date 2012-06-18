/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Rémi Turboult <r3mi@users.sourceforge.net>
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////


#include "upnp.h"
#include <stdio.h>
#include <stdlib.h>
#if UPNP_HAVE_TOOLS
#	include "upnptools.h"
#endif


int
main (int argc, char* argv[])
{
	int rc;

	/*
	 * Check library version (and formats)
	 */
	printf ("\n");
	
	printf ("UPNP_VERSION_STRING = \"%s\"\n", UPNP_VERSION_STRING);
	printf ("UPNP_VERSION_MAJOR  = %d\n",	  UPNP_VERSION_MAJOR);
	printf ("UPNP_VERSION_MINOR  = %d\n",	  UPNP_VERSION_MINOR);
	printf ("UPNP_VERSION_PATCH  = %d\n",	  UPNP_VERSION_PATCH);
	printf ("UPNP_VERSION        = %d\n",	  UPNP_VERSION);
	
	
	/*
	 * Check library optional features
	 */
	printf ("\n");
	
#if UPNP_HAVE_DEBUG
	printf ("UPNP_HAVE_DEBUG \t= yes\n");
#else
	printf ("UPNP_HAVE_DEBUG \t= no\n");
#endif
	
#if UPNP_HAVE_CLIENT
	printf ("UPNP_HAVE_CLIENT\t= yes\n");
#else
	printf ("UPNP_HAVE_CLIENT\t= no\n");
#endif
	
#if UPNP_HAVE_DEVICE
	printf ("UPNP_HAVE_DEVICE\t= yes\n");
#else
	printf ("UPNP_HAVE_DEVICE\t= no\n");
#endif
	
#if UPNP_HAVE_WEBSERVER
	printf ("UPNP_HAVE_WEBSERVER\t= yes\n");
#else
	printf ("UPNP_HAVE_WEBSERVER\t= no\n");
#endif

#if UPNP_HAVE_TOOLS
	printf ("UPNP_HAVE_TOOLS \t= yes\n");
#else
	printf ("UPNP_HAVE_TOOLS \t= no\n");
#endif

	
	/*
	 * Test library initialisation
	 */
	printf ("\n");
	printf ("Intializing UPnP ... \n");
	rc = UpnpInit (NULL, 0);
	if ( UPNP_E_SUCCESS == rc ) {
		const char* ip_address = UpnpGetServerIpAddress();
		unsigned short port    = UpnpGetServerPort();
		
		printf ("UPnP Initialized OK ip=%s, port=%d\n", 
			(ip_address ? ip_address : "UNKNOWN"), port);
	} else {
		printf ("** ERROR UpnpInit(): %d", rc);
#if UPNP_HAVE_TOOLS
		printf (" %s", UpnpGetErrorMessage (rc));
#endif
		printf ("\n");
	}
	
	(void) UpnpFinish();
	printf ("\n");
	
	exit (0);
}




