/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _STUBS_H
#define _STUBS_H 1

#if defined(__UCLIBC__) \
 && (__UCLIBC_MAJOR__ == 0 \
 && (__UCLIBC_MINOR__ < 9 || (__UCLIBC_MINOR__ == 9 && __UCLIBC_SUBLEVEL__ < 33)))

#undef dn_comp
#define dn_comp(exp_dn, comp_dn, length, dnptrs, lastdnptr) \
	dn_encode(exp_dn, comp_dn, length)


#endif

extern int dn_encode(const char *src, uint8_t *dst, int length);
extern int fflags(int sock, int flags);

#endif
