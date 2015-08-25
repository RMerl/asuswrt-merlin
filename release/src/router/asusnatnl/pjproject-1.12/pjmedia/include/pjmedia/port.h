/* $Id: port.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_PORT_H__
#define __PJMEDIA_PORT_H__

/**
 * @file port.h
 * @brief Port interface declaration
 */
#include <pjmedia/types.h>
#include <pj/assert.h>
#include <pj/os.h>


/**
  @addtogroup PJMEDIA_PORT Media Ports Framework
  @{

  @section media_port_intro Media Port Concepts
  
  @subsection The Media Port
  A media port (represented with pjmedia_port "class") provides a generic
  and extensible framework for implementing media elements. Media element
  itself could be a media source, sink, or processing element. A media
  port interface basically has the following properties:
  - media port information (pjmedia_port_info) to describe the
  media port properties (sampling rate, number of channels, etc.),
  - optional pointer to function to acquire frames from the port (the
    <tt>get_frame() </tt> interface), which will be called by
    #pjmedia_port_get_frame() public API, and
  - optional pointer to function to store frames to the port (the
    <tt>put_frame()</tt> interface) which will be called by
    #pjmedia_port_put_frame() public API.

  The <tt>get_frame()</tt> and <tt>put_frame()</tt> interface of course
  would only need to be implemented if the media port emits and/or takes
  media frames respectively.
  
  Media ports are passive "objects". By default, there is no worker thread
  to run the media flow. Applications (or other PJMEDIA
  components, as explained in @ref PJMEDIA_PORT_CLOCK) must actively call
  #pjmedia_port_get_frame() or #pjmedia_port_put_frame() from/to the media
  port in order to retrieve/store media frames.
  
  Some media ports (such as @ref PJMEDIA_CONF and @ref PJMEDIA_RESAMPLE_PORT)
  may be interconnected with (or encapsulate) other port, to perform the
  combined task of the ports, while some
  others represent the ultimate source/sink termination for the media. 
  Interconnection means the upstream media port will call <tt>get_frame()</tt>
  and <tt>put_frame()</tt> to its downstream media port. For this to happen,
  the media ports need to have the same format, where format is defined as
  combination of sample format, clock rate, channel count, bits per sample,
  and samples per frame for audio media.


  @subsection port_clock_ex1 Example: Manual Resampling

  For example, suppose application wants to convert the sampling rate
  of one WAV file to another. In this case, application would create and
  arrange media ports connection as follows:

    \image html sample-manual-resampling.jpg

  Application would setup the media ports using the following pseudo-
  code:

  \code
  
      pjmedia_port *player, *resample, *writer;
      pj_status_t status;
  
      // Create the file player port.
      status = pjmedia_wav_player_port_create(pool, 
  					      "Input.WAV",	    // file name
  					      20,		    // ptime.
  					      PJMEDIA_FILE_NO_LOOP, // flags
  					      0,		    // buffer size
  					      NULL,		    // user data.
  					      &player );
      PJ_ASSERT_RETURN(status==PJ_SUCCESS, PJ_SUCCESS);
  
      // Create the resample port with specifying the target sampling rate, 
      // and with the file port as the source. This will effectively
      // connect the resample port with the player port.
      status = pjmedia_resample_port_create( pool, player, 8000, 
  					     0, &resample);
      PJ_ASSERT_RETURN(status==PJ_SUCCESS, PJ_SUCCESS);
  
      // Create the file writer, specifying the resample port's configuration
      // as the WAV parameters.
      status pjmedia_wav_writer_port_create(pool, 
  					    "Output.WAV",  // file name.
  					    resample->info.clock_rate,
  					    resample->info.channel_count,
  					    resample->info.samples_per_frame,
  					    resample->info.bits_per_sample,
  					    0,		// flags
  					    0,		// buffer size
  					    NULL,	// user data.
  					    &writer);
  
  \endcode

  
  After the ports have been set up, application can perform the conversion
  process by running this loop:
 
  \code
  
  	pj_int16_t samplebuf[MAX_FRAME];
  	
  	while (1) {
  	    pjmedia_frame frame;
  	    pj_status_t status;
  
  	    frame.buf = samplebuf;
  	    frame.size = sizeof(samplebuf);
  
  	    // Get the frame from resample port.
  	    status = pjmedia_port_get_frame(resample, &frame);
  	    if (status != PJ_SUCCESS || frame.type == PJMEDIA_FRAME_TYPE_NONE) {
  		// End-of-file, end the conversion.
  		break;
  	    }
  
  	    // Put the frame to write port.
  	    status = pjmedia_port_put_frame(writer, &frame);
  	    if (status != PJ_SUCCESS) {
  		// Error in writing the file.
  		break;
  	    }
  	}
  
  \endcode
 
  For the sake of completeness, after the resampling process is done, 
  application would need to destroy the ports:
  
  \code
	// Note: by default, destroying resample port will destroy the
	//	 the downstream port too.
  	pjmedia_port_destroy(resample);
  	pjmedia_port_destroy(writer);
  \endcode
 
 
  The above steps are okay for our simple purpose of changing file's sampling
  rate. But for other purposes, the process of reading and writing frames
  need to be done in timely manner (for example, sending RTP packets to
  remote stream). And more over, as the application's scope goes bigger,
  the same pattern of manually reading/writing frames comes up more and more often,
  thus perhaps it would be better if PJMEDIA provides mechanism to 
  automate this process.
  
  And indeed PJMEDIA does provide such mechanism, which is described in 
  @ref PJMEDIA_PORT_CLOCK section.


  @subsection media_port_autom Automating Media Flow

  PJMEDIA provides few mechanisms to make media flows automatically
  among media ports. This concept is described in @ref PJMEDIA_PORT_CLOCK 
  section.
*/

PJ_BEGIN_DECL


/**
 * Port operation setting.
 */
typedef enum pjmedia_port_op
{
    /** 
     * No change to the port TX or RX settings.
     */
    PJMEDIA_PORT_NO_CHANGE,

    /**
     * TX or RX is disabled from the port. It means get_frame() or
     * put_frame() WILL NOT be called for this port.
     */
    PJMEDIA_PORT_DISABLE,

    /**
     * TX or RX is muted, which means that get_frame() or put_frame()
     * will still be called, but the audio frame is discarded.
     */
    PJMEDIA_PORT_MUTE,

    /**
     * Enable TX and RX to/from this port.
     */
    PJMEDIA_PORT_ENABLE

} pjmedia_port_op;


/**
 * Port info.
 */
typedef struct pjmedia_port_info
{
    pj_str_t	    name;		/**< Port name.			    */
    pj_uint32_t	    signature;		/**< Port signature.		    */
    pjmedia_type    type;		/**< Media type.		    */
    pj_bool_t	    has_info;		/**< Has info?			    */
    pj_bool_t	    need_info;		/**< Need info on connect?	    */
    unsigned	    pt;			/**< Payload type (can be dynamic). */
    pjmedia_format  format;		/**< Format.			    */
    pj_str_t	    encoding_name;	/**< Encoding name.		    */
    unsigned	    clock_rate;		/**< Sampling rate.		    */
    unsigned	    channel_count;	/**< Number of channels.	    */
    unsigned	    bits_per_sample;	/**< Bits/sample		    */
    unsigned	    samples_per_frame;	/**< No of samples per frame.	    */
    unsigned	    bytes_per_frame;	/**< No of bytes per frame.	    */
} pjmedia_port_info;


/**
 * Port interface.
 */
typedef struct pjmedia_port
{
    pjmedia_port_info	 info;		    /**< Port information.  */

    /** Port data can be used by the port creator to attach arbitrary
     *  value to be associated with the port.
     */
    struct port_data {
	void		*pdata;		    /**< Pointer data.	    */
	long		 ldata;		    /**< Long data.	    */
    } port_data;

    /**
     * Sink interface. 
     * This should only be called by #pjmedia_port_put_frame().
     */
    pj_status_t (*put_frame)(struct pjmedia_port *this_port, 
			     const pjmedia_frame *frame);

    /**
     * Source interface. 
     * This should only be called by #pjmedia_port_get_frame().
     */
    pj_status_t (*get_frame)(struct pjmedia_port *this_port, 
			     pjmedia_frame *frame);

    /**
     * Called to destroy this port.
     */
    pj_status_t (*on_destroy)(struct pjmedia_port *this_port);

} pjmedia_port;


/**
 * This is an auxiliary function to initialize port info for
 * ports which deal with PCM audio.
 *
 * @param info		    The port info to be initialized.
 * @param name		    Port name.
 * @param signature	    Port signature.
 * @param clock_rate	    Port's clock rate.
 * @param channel_count	    Number of channels.
 * @param bits_per_sample   Bits per sample.
 * @param samples_per_frame Number of samples per frame.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_port_info_init( pjmedia_port_info *info,
					     const pj_str_t *name,
					     unsigned signature,
					     unsigned clock_rate,
					     unsigned channel_count,
					     unsigned bits_per_sample,
					     unsigned samples_per_frame);


/**
 * Get a frame from the port (and subsequent downstream ports).
 *
 * @param port	    The media port.
 * @param frame	    Frame to store samples.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_port_get_frame( pjmedia_port *port,
					     pjmedia_frame *frame );

/**
 * Put a frame to the port (and subsequent downstream ports).
 *
 * @param port	    The media port.
 * @param frame	    Frame to the put to the port.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_port_put_frame( pjmedia_port *port,
					     const pjmedia_frame *frame );


/**
 * Destroy port (and subsequent downstream ports)
 *
 * @param port	    The media port.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_port_destroy( pjmedia_port *port );



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_PORT_H__ */

