/* @(#) interface to memory-buffer printf API 
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com)
 *
 *  This file is part of udpxy.
 *
 *  udpxy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  udpxy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with udpxy.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PRBUF_H_12272007bsl_
#define PRBUF_H_12272007bsl_

typedef struct printbuf* prbuf_t;

#ifdef __cplusplus
    extern "C" {
#endif


/* create/open print buffer
 *
 * @param pb    - address of the new print buffer
 * @param buf   - (pre-allocated) memory buffer
 *                to associate new printbuffer with
 * @param n     - size (in bytes) of the memory buffer
 *
 * @return 0 if success: contents of pb are updated with
 *         the address on new printbuffer; -1 otherwise
 */
int prbuf_open( prbuf_t* pb, void* buf, size_t n );


/* close print buffer
 *
 * @param pb    - print buffer to close
 *
 * @return 0 if success, -1 otherwise
 */
int prbuf_close( prbuf_t pb );


/* write a formatted string into the print buffer,
 * beginning at the current position (each write
 * advances the position appropriately)
 *
 * @param   - destination print buffer
 * @param   - printf-style format string
 *
 * @return 0 if end of buffer has been reached,
 *         -1 in case of error,
 *         n > 0, where n is the number of characters written
 *         into the buffer (not including trailing zero character)
 */
int prbuf_printf( prbuf_t pb, const char* format, ... );


/* @ return length of the text inside the buffer
 *   NOT including the termination zero character
 *
 */
size_t prbuf_len( prbuf_t pb );


/* set current position to the beginning of the print buffer
 *
 * @param pb    - destination print buffer
 */
void prbuf_rewind( prbuf_t pb );

#ifdef __cplusplus
}
#endif

#endif /* PRBUF_H_12272007bsl_ */

/* __EOF__ */

