/* @(#) HTML-page generation methods
 *
 * Copyright 2008-2011 Pavel V. Cherenkov (pcherenkov@gmail.com) (pcherenkov@gmail.com)
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

#ifndef UDPXY_MKPG_H_0115081656
#define UDPXY_MKPG_H_0115081656

#include <sys/types.h>
#include "ctx.h"

#ifdef __cplusplus
    extern "C" {
#endif

/* generate service's (HTML) status page
 *
 * @param ctx       server context
 * @param buf       destination buffer
 * @param len       destination buffer's length
 * @param options   options as a bitset of flags
 *
 * @return 0 if success, len gets updated with page text's size
 *         -1 in case of failure
 */

#define MSO_HTTP_HEADER      1      /* prepend page with HTTP header */
#define MSO_SKIP_CLIENTS     2      /* do not output client info     */
#define MSO_RESTART          4      /* use restart-page format       */


int
mk_status_page( const struct server_ctx* ctx,
                char* buf, size_t* len, int options );


#ifdef __cplusplus
}
#endif

#endif /* UDPXY_MKPG_H_0115081656 */

/* __EOF__ */

