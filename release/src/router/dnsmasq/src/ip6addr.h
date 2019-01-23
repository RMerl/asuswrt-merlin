/* dnsmasq is Copyright (c) 2000-2017 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#define IN6_IS_ADDR_ULA(a) \
        ((((__const uint32_t *) (a))[0] & htonl (0xff000000))                 \
         == htonl (0xfd000000))

#define IN6_IS_ADDR_ULA_ZERO(a) \
        (((__const uint32_t *) (a))[0] == htonl (0xfd000000)                        \
         && ((__const uint32_t *) (a))[1] == 0                                \
         && ((__const uint32_t *) (a))[2] == 0                                \
         && ((__const uint32_t *) (a))[3] == 0)

#define IN6_IS_ADDR_LINK_LOCAL_ZERO(a) \
        (((__const uint32_t *) (a))[0] == htonl (0xfe800000)                  \
         && ((__const uint32_t *) (a))[1] == 0                                \
         && ((__const uint32_t *) (a))[2] == 0                                \
         && ((__const uint32_t *) (a))[3] == 0)

