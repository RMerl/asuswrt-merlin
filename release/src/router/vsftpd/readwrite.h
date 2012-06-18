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
#ifndef VSF_READWRITE_H
#define VSF_READWRITE_H

struct vsf_session;
struct mystr;

enum EVSFRWTarget
{
  kVSFRWControl = 1,
  kVSFRWData
};

int ftp_write_str(const struct vsf_session* p_sess, const struct mystr* p_str,
                  enum EVSFRWTarget target);
int ftp_read_data(const struct vsf_session* p_sess, char* p_buf,
                  unsigned int len);
int ftp_write_data(const struct vsf_session* p_sess, const char* p_buf,
                   unsigned int len);
void ftp_getline(const struct vsf_session* p_sess, struct mystr* p_str,
                 char* p_buf);

#endif /* VSF_READWRITE_H */

