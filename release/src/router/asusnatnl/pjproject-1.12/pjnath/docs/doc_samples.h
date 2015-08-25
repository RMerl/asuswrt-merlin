/* $Id: doc_samples.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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


/**
@addtogroup samples_page

Several samples that are included in the PJSIP distributions. The screenshots
below were taken on a Windows machine, but the library is very portable and
it is known to run on platforms such as Linux, MacOS X, Windows Mobile,
Symbian, and so on.

  - @ref ice_demo_sample\n
    This sample demonstrates how to use \ref PJNATH_ICE_STREAM_TRANSPORT
    <b>without</b> using signaling protocol such as <b>SIP</b>. It provides 
    interactive user interface to create and manage the ICE sessions as well
    as to exchange SDP with another ice_demo instance.\n\n
    \image html ice_demo.jpg "ice_demo on WinXP"

  - @ref turn_client_sample\n
    This sample demonstrates how to use \ref PJNATH_TURN_SOCK
    and also \ref PJNATH_STUN_SOCK. It provides interactive 
    user interface to manage allocation, permissions, and
    channel bindings.\n\n
    \image html pjturn_client.jpg "pjturn_client on WinXP"

  - TURN server sample\n
    This is a simple sample TURN server application, which
    we mainly use for testing (as back then there is no TURN
    server available).\n
    The source code for this application are in <tt><b>pjnath/src/pjturn-srv</b></tt>
    directory.

 */


/**
\page turn_client_sample pjturn-client, a sample TURN client

This is a simple, interactive TURN client application, with the 
following features:
 - DNS SRV resolution
 - TCP connection to TURN server
 - Optional fingerprint

This file is pjnath/src/pjturn-client/client_main.c.

Screenshot on WinXP: \image html pjturn_client.jpg "pjturn_client on WinXP"

\includelineno client_main.c.
*/


/**
\page ice_demo_sample ice_demo, an interactive ICE endpoint

This sample demonstrates how to use \ref PJNATH_ICE_STREAM_TRANSPORT
<b>without</b> using signaling protocol such as SIP. It provides
interactive user interface to create and manage the ICE sessions as well
as to exchange SDP with another ice_demo instance.

Features of the demo application:
 - supports host, STUN, and TURN candidates
 - disabling of host candidates
 - DNS SRV resolution for STUN and TURN servers
 - TCP connection to TURN server
 - Optional use of fingerprint for TURN
 - prints and parse SDP containing ICE infos
 - exchange SDP with copy/paste

This file is pjsip-apps/src/samples/icedemo.c

Screenshot on WinXP: \image html ice_demo.jpg "ice_demo on WinXP"

\includelineno icedemo.c.
*/

