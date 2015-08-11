/* $Id: tonegen.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_TONEGEN_PORT_H__
#define __PJMEDIA_TONEGEN_PORT_H__

/**
 * @file tonegen.h
 * @brief Tone (sine, MF, DTMF) generator media port.
 */
#include <pjmedia/port.h>


/**
 * @defgroup PJMEDIA_MF_DTMF_TONE_GENERATOR Multi-frequency tone generator
 * @ingroup PJMEDIA_PORT
 * @brief Multi-frequency tone generator
 * @{
 *
 * This page describes tone generator media port. A tone generator can be
 * used to generate a single frequency sine wave or dual frequency tones
 * such as DTMF.
 *
 * The tone generator media port provides two functions to generate tones.
 * The function #pjmedia_tonegen_play() can be used to generate arbitrary
 * single or dual frequency tone, and #pjmedia_tonegen_play_digits() is
 * used to play digits such as DTMF. Each tone specified in the playback
 * function has individual on and off signal duration that must be
 * specified by application.
 *
 * In order to play digits such as DTMF, the tone generator is equipped
 * with digit map, which contain information about the frequencies of
 * the digits. The default digit map is DTMF (0-9,a-d,*,#), but application
 * may specifiy different digit map to the tone generator by calling
 * #pjmedia_tonegen_set_digit_map() function.
 */

PJ_BEGIN_DECL


/**
 * This structure describes individual MF digits to be played
 * with #pjmedia_tonegen_play().
 */
typedef struct pjmedia_tone_desc
{
    short   freq1;	    /**< First frequency.			    */
    short   freq2;	    /**< Optional second frequency.		    */
    short   on_msec;	    /**< Playback ON duration, in miliseconds.	    */
    short   off_msec;	    /**< Playback OFF duration, ini miliseconds.    */
    short   volume;	    /**< Volume (1-32767), or 0 for default, which
				 PJMEDIA_TONEGEN_VOLUME will be used.	    */
    short   flags;	    /**< Currently internal flags, must be 0	    */
} pjmedia_tone_desc;



/**
 * This structure describes individual MF digits to be played
 * with #pjmedia_tonegen_play_digits().
 */
typedef struct pjmedia_tone_digit
{
    char    digit;	    /**< The ASCI identification for the digit.	    */
    short   on_msec;	    /**< Playback ON duration, in miliseconds.	    */
    short   off_msec;	    /**< Playback OFF duration, ini miliseconds.    */
    short   volume;	    /**< Volume (1-32767), or 0 for default, which
				 PJMEDIA_TONEGEN_VOLUME will be used.	    */
} pjmedia_tone_digit;


/**
 * This structure describes the digit map which is used by the tone generator
 * to produce tones from an ASCII digits.
 * Digit map used by a particular tone generator can be retrieved/set with
 * #pjmedia_tonegen_get_digit_map() and #pjmedia_tonegen_set_digit_map().
 */
typedef struct pjmedia_tone_digit_map
{
    unsigned count;	    /**< Number of digits in the map.		*/

    struct {
	char    digit;	    /**< The ASCI identification for the digit.	*/
	short   freq1;	    /**< First frequency.			*/
	short   freq2;	    /**< Optional second frequency.		*/
    } digits[16];	    /**< Array of digits in the digit map.	*/
} pjmedia_tone_digit_map;


/**
 * Tone generator options.
 */
enum
{
    /**
     * Play the tones in loop, restarting playing the first tone after
     * the last tone has been played.
     */
    PJMEDIA_TONEGEN_LOOP    = 1,

    /**
     * Disable mutex protection to the tone generator.
     */
    PJMEDIA_TONEGEN_NO_LOCK = 2
};


/**
 * Create an instance of tone generator with the specified parameters.
 * When the tone generator is first created, it will be loaded with the
 * default digit map.
 *
 * @param pool		    Pool to allocate memory for the port structure.
 * @param clock_rate	    Sampling rate.
 * @param channel_count	    Number of channels. Currently only mono and stereo
 *			    are supported.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample. This version of PJMEDIA
 *			    only supports 16bit per sample.
 * @param options	    Option flags. Application may specify 
 *			    PJMEDIA_TONEGEN_LOOP to play the tone in a loop.
 * @param p_port	    Pointer to receive the port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate
 *			    error code.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_create(pj_pool_t *pool,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    unsigned options,
					    pjmedia_port **p_port);


/**
 * Create an instance of tone generator with the specified parameters.
 * When the tone generator is first created, it will be loaded with the
 * default digit map.
 *
 * @param pool		    Pool to allocate memory for the port structure.
 * @param name		    Optional name for the tone generator.
 * @param clock_rate	    Sampling rate.
 * @param channel_count	    Number of channels. Currently only mono and stereo
 *			    are supported.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample. This version of PJMEDIA
 *			    only supports 16bit per sample.
 * @param options	    Option flags. Application may specify 
 *			    PJMEDIA_TONEGEN_LOOP to play the tone in a loop.
 * @param p_port	    Pointer to receive the port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate
 *			    error code.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_create2(pj_pool_t *pool,
					     const pj_str_t *name,
					     unsigned clock_rate,
					     unsigned channel_count,
					     unsigned samples_per_frame,
					     unsigned bits_per_sample,
					     unsigned options,
					     pjmedia_port **p_port);


/**
 * Check if the tone generator is still busy producing some tones.
 *
 * @param tonegen	    The tone generator instance.
 *
 * @return		    Non-zero if busy.
 */
PJ_DECL(pj_bool_t) pjmedia_tonegen_is_busy(pjmedia_port *tonegen);


/**
 * Instruct the tone generator to stop current processing.
 *
 * @param tonegen	    The tone generator instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_stop(pjmedia_port *tonegen);


/**
 * Rewind the playback. This will start the playback to the first
 * tone in the playback list.
 *
 * @param tonegen	    The tone generator instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_rewind(pjmedia_port *tonegen);


/**
 * Instruct the tone generator to play single or dual frequency tones 
 * with the specified duration. The new tones will be appended to currently
 * playing tones, unless #pjmedia_tonegen_stop() is called before calling
 * this function. The playback will begin as soon as  the first get_frame()
 * is called to the generator.
 *
 * @param tonegen	    The tone generator instance.
 * @param count		    The number of tones in the array.
 * @param tones		    Array of tones to be played.
 * @param options	    Option flags. Application may specify 
 *			    PJMEDIA_TONEGEN_LOOP to play the tone in a loop.
 *
 * @return		    PJ_SUCCESS on success, or PJ_ETOOMANY if
 *			    there are too many digits in the queue.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_play(pjmedia_port *tonegen,
					  unsigned count,
					  const pjmedia_tone_desc tones[],
					  unsigned options);

/**
 * Instruct the tone generator to play multiple MF digits with each of
 * the digits having individual ON/OFF duration. Each of the digit in the
 * digit array must have the corresponding descriptor in the digit map.
 * The new tones will be appended to currently playing tones, unless 
 * #pjmedia_tonegen_stop() is called before calling this function. 
 * The playback will begin as soon as the first get_frame() is called 
 * to the generator.
 *
 * @param tonegen	    The tone generator instance.
 * @param count		    Number of digits in the array.
 * @param digits	    Array of MF digits.
 * @param options	    Option flags. Application may specify 
 *			    PJMEDIA_TONEGEN_LOOP to play the tone in a loop.
 *
 * @return		    PJ_SUCCESS on success, or PJ_ETOOMANY if
 *			    there are too many digits in the queue, or
 *			    PJMEDIA_RTP_EINDTMF if invalid digit is
 *			    specified.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_play_digits(pjmedia_port *tonegen,
						 unsigned count,
						 const pjmedia_tone_digit digits[],
						 unsigned options);


/**
 * Get the digit-map currently used by this tone generator.
 *
 * @param tonegen	    The tone generator instance.
 * @param m		    On output, it will be filled with the pointer to
 *			    the digitmap currently used by the tone generator.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_get_digit_map(pjmedia_port *tonegen,
						   const pjmedia_tone_digit_map **m);


/**
 * Set digit map to be used by the tone generator.
 *
 * @param tonegen	    The tone generator instance.
 * @param m		    Digitmap to be used by the tone generator.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_tonegen_set_digit_map(pjmedia_port *tonegen,
						   pjmedia_tone_digit_map *m);


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_TONEGEN_PORT_H__ */

