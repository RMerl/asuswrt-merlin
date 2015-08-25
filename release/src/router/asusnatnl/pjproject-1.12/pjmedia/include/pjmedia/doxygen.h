/* $Id: doxygen.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_DOXYGEN_H__
#define __PJMEDIA_DOXYGEN_H__

/**
 * @file doxygen.h
 * @brief Doxygen's mainpage.
 */


/*////////////////////////////////////////////////////////////////////////// */
/*
	INTRODUCTION PAGE
 */

/**
 * @mainpage PJMEDIA
 *
 * \n
 * @section intro2 Introduction to PJMEDIA
 *
 * PJMEDIA is a fully featured media stack, distributed under Open Source/GPL
 * terms, and featuring small footprint and good extensibility and excellent
 * portability. Here are some brief overview of PJMEDIA benefits.
 *
 * @subsection benefit Benefits
 * @subsubsection full_feature Many Features
 * PJMEDIA has many features, and rather than to list them all here, please
 * see the <A HREF="modules.htm"><b>Modules</b></A> page for more info.
 *
 * Video is planned to arrive at version 2.
 *
 * @subsubsection portable Excellent Portability
 * It's been ported to all desktop systems and many mobile platforms including
 * Symbian, Windows Mobile, iPhone, and Android. Thanks to its zero thread
 * design, users have been able to run PJMEDIA on deeply embedded platforms,
 * even without operating systems (those typically found in DSP platforms).
 * Except the echo suppressor, all other PJMEDIA components have fixed point
 * implementation, which makes it ideal for embedded systems which lack FPU.
 * PJMEDIA also has tiny footprint, as explained below
 *
 * @subsubsection footprint Tiny Footprint
 * Lets not talk about less meaningful and potentially misleading term such as
 * core footprint, and instead here is the footprint of all components
 * typically used to build a full streaming media:
 *
 * \verbatim
Category        Component       text    data    bss     dec     filename
-------------------------------------------------------------------------------
Core            Error subsystem 135     0       0       135     errno.o
Core            Endpoint        4210    4       4       4218    endpoint.o
Core            Port framework  652     0       0       652     port.o
Core            Codec framework 6257    0       0       6257    codec.o
Codec           Alaw/ulaw conv. 1060    16      0       1076    alaw_ulaw.o
Codec           G.711           3298    128     96      3522    g711.o
Codec           PLC             883     24      0       907     plc_common.o
Codec           PLC             7130    0       0       7130    wsola.o
Session         Stream          12222   0       1920    14142   stream.o
Transport       RTCP            3732    0       0       3732    rtcp.o
Transport       RTP             2568    0       0       2568    rtp.o
Transport       UDP             6612    96      0       6708    transport_udp.o
Transport       Jitter buffer   6473    0       0       6473    jbuf.o
-------------------------------------------------------------------------------
TOTAL                          55,232   268    2,020    57,520

 \endverbatim
 * The 56KB are for media streaming components, complete with codec, RTP, and
 * RTCP. The footprint above was done for PJSIP version 1.8.2 on a Linux x86
 * machine, using footprintopimization as explained in PJSIP FAQ. Numbers are
 * in bytes.
 *
 * @subsubsection quality Good Quality
 * PJMEDIA supports wideband, ultra-wideband, and beyond, as well as multiple
 * audio channels. The jitter buffer has been proven to work on lower
 * bandwidth links such as 3G, and to some extent, Edge and GPRS. We've grown
 * our own algorithm to compensate for packet losses and clock drifts in audio
 * transmission, as well as feature to use codec's built in PLC if available.
 *
 * @subsubsection hw Hardware Support
 * PJMEDIA supports hardware, firmware, or other built-in feature that comes
 * with the device. These are crucial for mobile devices to allow the best
 * use of the very limited CPU and battery power of the device. Among other
 * things, device's on-board codec and echo cancellation may be used if
 * available.
 *
 * @subsubsection extensible Extensible
 * Despite its tiny footprint, PJMEDIA uses a flexible port concept, which is
 * adapted from filter based concept found in other media framework. It is not
 * as flexible as those found in Direct Show or gstreamer (and that would be
 * unnecessary since it serves different purpose), but it's flexible enough
 * to allow components to be assembled one after another to achieve certain
 * task, and easy creation of such components by application and interconnect
 * them to the rest of the framework.
 *
 * @subsubsection doc (Fairly Okay) Documentation
 * We understand that any documentation can always be improved, but we put
 * a lot of efforts in creating and maintaining our documentation, because
 * we know it matters.
 *
 * \n
 * @subsection org1 Organization
 *
 * At the top-most level, PJMEDIA library suite contains the following
 * libraries.
 *
 * @subsubsection libpjmedia PJMEDIA
 * This contains all main media components. Please see the
 * <A HREF="modules.htm"><b>Modules</b></A> page for complete list of
 * components that PJMEDIA provides.
 *
 * @subsubsection libpjmediacodec PJMEDIA Codec
 * PJMEDIA-CODEC is a static library containing various codec implementations,
 * wrapped into PJMEDIA codec framework. The static library is designed as
 * such so that only codecs that are explicitly initialized are linked with 
 * the application, therefore keeping the application size in control.
 *
 * Please see @ref PJMEDIA_CODEC for more info.
 *
 * @subsubsection libpjmediaaudiodev PJMEDIA Audio Device
 * PJMEDIA-Audiodev is audio device framework and abstraction library. Please
 * see @ref audio_device_api for more info.
 *
 * \n
 * @section pjmedia_concepts PJMEDIA Key Concepts
 * Below are some key concepts in PJMEDIA:
 *  - @ref PJMEDIA_PORT
 *  - @ref PJMEDIA_PORT_CLOCK
 *  - @ref PJMEDIA_TRANSPORT
 *  - @ref PJMEDIA_SESSION
 */


/**
  @page page_pjmedia_samples PJMEDIA and PJMEDIA-CODEC Examples

  @section pjmedia_samples_sec PJMEDIA and PJMEDIA-CODEC Examples

  Please find below some PJMEDIA related examples that may help in giving
  some more info:

  - @ref page_pjmedia_samples_level_c\n
    This is a good place to start learning about @ref PJMEDIA_PORT,
    as it shows that @ref PJMEDIA_PORT are only "passive" objects
    with <tt>get_frame()</tt> and <tt>put_frame()</tt> interface, and
    someone has to call these to retrieve/store media frames.

  - @ref page_pjmedia_samples_playfile_c\n
    This example shows that when application connects a media port (in this
    case a @ref PJMEDIA_FILE_PLAY) to @ref PJMED_SND_PORT, media will flow
    automatically since the @ref PJMED_SND_PORT provides @ref PJMEDIA_PORT_CLOCK.

  - @ref page_pjmedia_samples_recfile_c\n
    Demonstrates how to capture audio from microphone to WAV file.

  - @ref page_pjmedia_samples_playsine_c\n
    Demonstrates how to create a custom @ref PJMEDIA_PORT (in this
    case a sine wave generator) and integrate it to PJMEDIA.

  - @ref page_pjmedia_samples_confsample_c\n
    This demonstrates how to use the @ref PJMEDIA_CONF. The sample program can 
    open multiple WAV files, and instruct the conference bridge to mix the
    signal before playing it to the sound device.

  - @ref page_pjmedia_samples_confbench_c\n
    I use this to benchmark/optimize the conference bridge algorithm, but
    readers may find the source useful.

  - @ref page_pjmedia_samples_resampleplay_c\n
    Demonstrates how to use @ref PJMEDIA_RESAMPLE_PORT to change the
    sampling rate of a media port (in this case, a @ref PJMEDIA_FILE_PLAY).

  - @ref page_pjmedia_samples_sndtest_c\n
    This program performs some tests to the sound device to get some
    quality parameters (such as sound jitter and clock drifts).\n
    Screenshots on WinXP: \image html sndtest.jpg "sndtest screenshot on WinXP"

  - @ref page_pjmedia_samples_streamutil_c\n
    This example mainly demonstrates how to stream media (in this case a
    @ref PJMEDIA_FILE_PLAY) to remote peer using RTP.

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

  - @ref page_pjmedia_samples_tonegen_c\n
    This is a simple program to generate a tone and write the samples to
    a raw PCM file. The main purpose of this file is to analyze the
    quality of the tones/sine wave generated by PJMEDIA tone/sine wave
    generator.

  - @ref page_pjmedia_samples_aectest_c\n
    Play a file to speaker, run AEC, and record the microphone input
    to see if echo is coming.
 */

/**
 * \page page_pjmedia_samples_siprtp_c Samples: Using SIP and Custom RTP/RTCP to Monitor Quality
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

#endif /* __PJMEDIA_DOXYGEN_H__ */

