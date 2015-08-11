/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
 * @file doxygen.h
 * @brief PJSIP Doxygen's mainpage.
 */

/*////////////////////////////////////////////////////////////////////////// */
/*
	INTRODUCTION PAGE
 */

/**
  \n

  @mainpage PJSIP - Open Source SIP Stack

  \n
  \n
  @section intro_sec Introduction

  PJSIP is an Open Source SIP prototol stack, designed to be very small in 
  footprint, have high performance, and very flexible.

  @subsection hist_sec History

  PJSIP has been actively developed since 2003, but its history goes well
  beyond that. The author has been developing SIP stack since 1999 during
  RFC 2543 era, and after several experimentation with different approaches 
  in the programming (the first stack actually was in C++!), and also with
  the evolution of the SIP protocol itself, the current/third generation
  of PJSIP (the 0.2.9 version is the second generation) can be considered 
  as pretty stable in term of design, and should
  incorporate all design considerations (and implementation tricks!) that 
  have been learned over the years. Of course only time will tell if this 
  statement can still be held true in the future.




  \n
  \n
  @section pjsipgetting_started Getting Started

  PJSIP consists of multiple levels of APIs, which each of them layered on
  top of another. Because of this, new readers may find it a bit difficult
  to find the place to start.

  In general, I think perhaps I can recommend two approaches on using PJSIP.


  \n
  @subsection getting_started_high Using PJSUA API

  @ref PJSUA_LIB wraps together all SIP components and media into a high level
  API, suitable for creating typical SIP user agent applications. It 
  features easy to use API for:
  - multiple client registration (accounts),
  - high level SIP and media session (calls),
  - buddy list, presence and instant messaging,
  - powerful and very easy to use media manipulation,

  while maintaining some space for customization (custom SIP
  transport, custom SIP media, etc.) needed by some types of applications.
  @ref PJSUA_LIB is also aimed to be able to run on devices such as PDA
  or mobile phones, by carefully allowing application to set the appropriate
  threading strategy and memory limits (number of calls, media ports, etc.).

  However, @ref PJSUA_LIB may not be the most suitable API for some types
  of applications, since it is directed towards an easy to use API. For
  more more advanced use, you may better implement the application by using
  PJSIP + PJMEDIA directly, as described below.


  \n
  @subsection getting_started_pjsip_pjmedia Using PJSIP and PJMEDIA Directly

  For the ultimate flexibility and power, using PJSIP and PJMEDIA directly
  is the way to go. The drawback will be, of course, steeper learning curve. 

  However, the following links may provide some useful information:
  - <A HREF="/docs.htm">PJSIP Developer's Guide</A> PDF
    document is the ultimate guide to understand PJSIP design concept.
  - there are some samples in <A HREF="/cgi-bin/viewcvs.cgi/pjproject/trunk/pjsip-apps/src/samples/">
   <b>pjsip-apps/src/samples</b></A> directory.
  - @ref PJSUA_LIB source code may also be useful to see how high level
    API are implemented with PJSIP/PJMEDIA.
  - and finally, you can always <b>Use the Source</b>!



  \n
  \n
  @section this_doc About This Document

  This document contains the reference information about PJSIP. For
  more in-depth guide (and information in general), readers are 
  encouraged to read the <A HREF="/docs.htm">
  <b>PJSIP Developer's Guide</b></A> PDF document
  which can be downloaded from http://www.pjsip.org/docs.htm.

  \n
  @subsection doc_how_to_read How to Read This Document

  For main navigation, please go to <A HREF="modules.htm"><b>Modules</b></A>
  link on top of this page.

  This document was generated with <A HREF="http://www.doxygen.org">Doxygen</A>
  from PJSIP header files.


  \n
  \n
  @section pjsip_toc Documentation Contents

  Click on <A HREF="modules.htm"><b>Modules</b></A> link on top of this page 
  to get the detailed table of contents.

  The following are top level sections in the <A HREF="modules.htm">
  <b>Modules</b></A>, as laid out in the following diagram:

  \image html pjsip-arch.jpg "Static Library Layout"

  Enumerating the static libraries from the bottom:

  - <A HREF="/pjlib/docs/main.htm">PJLIB</A>, is the platform abstraction
    and framework library, on which all other libraries depend,

  - PJLIB-UTIL, provides auxiliary functions such as text scanning,
    XML, and STUN,

  - PJMEDIA is the multimedia framework,

  - PJMEDIA-CODEC is the placeholder for media codecs,

  - @ref PJSIP_CORE (<b>PJSIP-CORE</b>) is the very core of the PJSIP library, 
    and contains the SIP @ref PJSIP_ENDPT, which is the owner/manager for all
    SIP objects in the application, messaging elements, parsing, transport 
    management, module management, and stateless operations, and also
    contains:

  - The @ref PJSIP_TRANSACT module inside <b>PJSIP-CORE</b> provides 
    stateful operation, and is the base for higher layer features such as 
    dialogs,

  - The @ref PJSIP_UA module inside <b>PJSIP-CORE</b> manages dialogs, and supports dialog
    usages,

  - @ref PJSIP_SIMPLE (<b>PJSIP-SIMPLE</b>) provides the base SIP event framework 
    (which uses the common/base dialog framework) and implements presence 
    on top of it, and is also used by call transfer functions,

  - @ref PJSIP_HIGH_UA (<b>PJSIP-UA</b>) is the high level abstraction of INVITE sessions
    (using the common/base dialog framework). This library also provides
    SIP client registration and call transfer functionality,

  - and finally, @ref PJSUA_LIB (<b>PJSUA-LIB</b>) is the highest level of abstraction, 
    which wraps together all above functionalities into high level, easy to
    use API.
*/


/**
 @page page_pjsip_samples PJSIP Samples

 I wish I could write more samples, but for now here are some samples or
 working applications that are available from the source tree:

  - @ref page_pjsip_sample_sipstateless_c\n
    This is about the simplest SIP application with PJSIP, all it does is
    respond all incoming requests with 501 (Not Implemented) response
    statelessly.

  - @ref page_pjsip_sample_simple_ua_c\n
    This is a very simple SIP User Agent application that only use PJSIP
    (without PJSIP-UA). It's able to make and receive call, and play
    media to the sound device.
    
  - @ref page_pjsip_sample_simple_pjsuaua_c\n
    Very simple SIP User Agent with registration, call, and media, using
    PJSUA-API, all in under 200 lines of code.

  - @ref page_pjsip_samples_pjsua\n
    This is the reference implementation for PJSIP and PJMEDIA.
    PJSUA is a console based application, designed to be simple enough
    to be readble, but powerful enough to demonstrate all features
    available in PJSIP and PJMEDIA.\n
    Screenshot on WinXP: \image html pjsua.jpg "pjsua on WinXP"

  - @ref page_pjmedia_samples_siprtp_c\n
    This is a useful program (integrated with PJSIP) to actively measure 
    the network quality/impairment parameters by making one or more SIP 
    calls (or receiving one or more SIP calls) and display the network
    impairment of each stream direction at the end of the call.
    The program is able to measure network quality parameters such as
    jitter, packet lost/reorder/duplicate, round trip time, etc.\n
    Note that the remote peer MUST support RTCP so that network quality
    of each direction can be calculated. Using siprtp for both endpoints
    is recommended.\n
    Screenshots on WinXP: \image html siprtp.jpg "siprtp screenshot on WinXP"

  - @ref page_pjsip_perf_c\n
    This sample contains a complete implementation of a SIP performance
    measurement tool. Unlike other tool such SIPp, pjsip-perf is geared
    more towards finding the performance of an endpoint by flooding the
    endpoint with some requests and time the completion of the requests.\n
    Screenshots on Linux console: \image html pjsip-perf.jpg "pjsip-perf screenshot on Linux"
 */

/**
 * \page page_pjsip_samples_pjsua PJSUA
 *
 * This is the reference implementation for PJSIP and PJMEDIA.
 * PJSUA is a console based application, designed to be simple enough
 * to be readble, but powerful enough to demonstrate all features
 * available in PJSIP and PJMEDIA.
 *
 * This file is pjsip-apps/src/pjsua/pjsua_app.c
 *
 * Screenshot on WinXP: \image html pjsua.jpg "pjsua on WinXP"
 *
 * \includelineno pjsua_app.c
 */

/**
 * \page page_pjsip_sample_simple_ua_c Samples: Simple UA
 *
 * This is a very simple SIP User Agent application that only use PJSIP
 * (without PJSIP-UA). It's able to make and receive call, and play
 * media to the sound device.
 *
 * \includelineno simpleua.c
 */

/**
 * \page page_pjsip_sample_simple_pjsuaua_c Samples: Simple PJSUA
 *
 * Very simple SIP User Agent with registration, call, and media, all
 * in under 200 lines of code.
 *
 * \includelineno simple_pjsua.c
 */

/**
 * \page page_pjsip_sample_sipstateless_c Samples: Stateless SIP Endpoint
 *
 * This is about the simplest SIP application with PJSIP, all it does is
 * respond all incoming requests with 501 (Not Implemented) response
 * statelessly.
 *
 * \includelineno sipstateless.c
 */

/**
 * \page page_pjmedia_samples_siprtp_c Samples: siprtp - SIP with RTCP Quality Monitoring
 *
 * This source is an example to demonstrate using SIP and RTP/RTCP framework
 * to measure the network quality/impairment from the SIP call. This
 * program can be used to make calls or to receive calls from other
 * SIP endpoint (or other siprtp program), and to display the media
 * quality statistics at the end of the call.
 *
 * Note that the remote peer must support RTCP.
 *
 * The layout of the program has been designed so that custom reporting
 * can be generated instead of plain human readable text.
 *
 * The source code of the file is pjsip-apps/src/samples/siprtp.c
 *
 * Screenshots on WinXP: \image html siprtp.jpg
 *
 * \includelineno siprtp.c
 */

/**
 * \page page_pjsip_perf_c Samples: pjsip-perf - SIP Performance Benchmarking Tool
 *
 * This sample contains a complete implementation of a SIP performance
 * measurement tool. Unlike other tool such SIPp, pjsip-perf is geared
 * more towards finding the performance of an endpoint by flooding the
 * endpoint with some requests and time the completion of the requests.
 *
 * The source code of the file is pjsip-apps/src/samples/pjsip-perf.c
 *
 * Screenshots on Linux console: \image html pjsip-perf.jpg
 *
 * \includelineno pjsip-perf.c
 */

