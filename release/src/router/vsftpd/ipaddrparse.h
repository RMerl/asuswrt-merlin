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
#ifndef VSF_IPADDRPARSE_H
#define VSF_IPADDRPARSE_H

struct mystr;

/* Effectively doing the same sort of job as inet_pton. Since inet_pton does
 * a non-trivial amount of parsing, we'll do it ourselves for maximum security
 * and safety.
 */

const unsigned char* vsf_sysutil_parse_ipv6(const struct mystr* p_str);

const unsigned char* vsf_sysutil_parse_ipv4(const struct mystr* p_str);

const unsigned char* vsf_sysutil_parse_uchar_string_sep(
  const struct mystr* p_str, char sep, unsigned char* p_items,
  unsigned int items);

#endif /* VSF_IPADDRPARSE_H */

