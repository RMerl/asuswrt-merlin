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
#ifndef VSF_SECBUF_H
#define VSF_SECBUF_H

/* vsf_secbuf_alloc()
 * PURPOSE
 * Allocate a "secure buffer". A secure buffer is one which will attempt to
 * catch out of bounds accesses by crashing the program (rather than
 * corrupting memory). It works by using UNIX memory protection. It isn't
 * foolproof.
 * PARAMETERS
 * p_ptr        - pointer to a pointer which is to contain the secure buffer.
 *                Any previous buffer pointed to is freed.
 * size         - size in bytes required for the secure buffer.
 */
void vsf_secbuf_alloc(char** p_ptr, unsigned int size);

/* vsf_secbuf_free()
 * PURPOSE
 * Frees a "secure buffer".
 * PARAMETERS
 * p_ptr        - pointer to a pointer containing the buffer to be freed. The
 *                buffer pointer is nullified by this call.
 */
void vsf_secbuf_free(char** p_ptr);

#endif /* VSF_SECBUF_H */

