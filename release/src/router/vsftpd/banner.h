/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef VSF_BANNER_H
#define VSF_BANNER_H

struct vsf_session;
struct mystr;

/* vsf_banner_dir_changed()
 * PURPOSE
 * This function, when called, will check if the current directory has just
 * been entered for the first time in this session. If so, and message file
 * support is on, a message file is looked for (default .message), and output
 * to the FTP control connection with the FTP code prefix specified by
 * "ftpcode".
 * PARAMETERS
 * p_sess         - the current FTP session object
 * ftpcode        - the FTP code to show with the message
 */
void vsf_banner_dir_changed(struct vsf_session* p_sess, int ftpcode);

/* vsf_banner_write()
 * PURPOSE
 * This function, when called, will write the specified string as a multiline
 * FTP banner, using the specified FTP response code.
 * PARAMETERS
 * p_sess         - the current FTP session object
 * p_str          - the string to write
 * ftpcode        - the FTP code to show with the message
 */
void vsf_banner_write(struct vsf_session* p_sess, struct mystr* p_str,
                      int ftpcode);

#endif /* VSF_BANNER_H */

