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
#ifndef VSF_SSL_H
#define VSF_SSL_H

struct vsf_session;
struct mystr;

void ssl_getline(const struct vsf_session* p_sess, struct mystr* p_str,
                 char end_char, char* p_buf, unsigned int buflen);
int ssl_read(void* p_ssl, char* p_buf, unsigned int len);
int ssl_write(void* p_ssl, const char* p_buf, unsigned int len);
int ssl_write_str(void* p_ssl, const struct mystr* p_str);
void ssl_init(struct vsf_session* p_sess);
int ssl_accept(struct vsf_session* p_sess, int fd);
void ssl_data_close(struct vsf_session* p_sess);
void ssl_comm_channel_init(struct vsf_session* p_sess);
void handle_auth(struct vsf_session* p_sess);
void handle_pbsz(struct vsf_session* p_sess);
void handle_prot(struct vsf_session* p_sess);

#endif /* VSF_SSL_H */

