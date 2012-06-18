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
#ifndef VSFTP_NETSTR_H
#define VSFTP_NETSTR_H

struct mystr;

/* str_netfd_alloc()
 * PURPOSE
 * Read a string from a network socket into a string buffer object. The string
 * is delimited by a specified string terminator character.
 * If any network related errors occur trying to read the string, this call
 * will exit the program.
 * This method avoids reading one character at a time from the network.
 * PARAMETERS
 * p_str        - the destination string object
 * fd           - the file descriptor of the remote network socket
 * term         - the character which will terminate the string. This character
 *                is included in the returned string.
 * p_readbuf    - pointer to a scratch buffer into which to read from the
 *                network. This buffer must be at least "maxlen" characters!
 * maxlen       - maximum length of string to return. If this limit is passed,
 *                an empty string will be returned.
 */
void str_netfd_alloc(struct mystr* p_str, int fd, char term,
                     char* p_readbuf, unsigned int maxlen);

/* str_netfd_read()
 * PURPOSE
 * Fills contents of a string buffer object from a (typically network) file
 * descriptor.
 * PARAMETERS
 * p_str        - the string object to be filled
 * fd           - the file descriptor to read from
 * len          - the number of bytes to read
 * RETURNS
 * Number read on success, -1 on failure. The read is considered a failure
 * unless the full requested byte count is read.
 */
int str_netfd_read(struct mystr* p_str, int fd, unsigned int len);

/* str_netfd_write()
 * PURPOSE
 * Write the contents of a string buffer object out to a (typically network)
 * file descriptor.
 * PARAMETERS
 * p_str        - the string object to send
 * fd           - the file descriptor to write to
 * RETURNS
 * Number written on success, -1 on failure. The write is considered a failure
 * unless the full string buffer object is written.
 */
int str_netfd_write(const struct mystr* p_str, int fd);

#endif /* VSFTP_NETSTR_H */

