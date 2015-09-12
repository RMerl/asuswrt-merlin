/*  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
Copyright (c) 2007, 2008 by Juliusz Chroboczek
Copyright 2011 by Matthieu Boutier and Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

struct xroute {
    unsigned char prefix[16];
    unsigned char plen;
    unsigned short metric;
    unsigned int ifindex;
    int proto;
};

struct xroute *find_xroute(const unsigned char *prefix, unsigned char plen);
void flush_xroute(struct xroute *xroute);
int babel_ipv4_route_add (struct zapi_ipv4 *api, struct prefix_ipv4 *prefix,
                          unsigned int ifindex, struct in_addr *nexthop);
int babel_ipv4_route_delete (struct zapi_ipv4 *api, struct prefix_ipv4 *prefix,
                             unsigned int ifindex);
int babel_ipv6_route_add (struct zapi_ipv6 *api, struct prefix_ipv6 *prefix,
                          unsigned int ifindex, struct in6_addr *nexthop);
int babel_ipv6_route_delete (struct zapi_ipv6 *api, struct prefix_ipv6 *prefix,
                             unsigned int ifindex);
int xroutes_estimate(void);
void for_all_xroutes(void (*f)(struct xroute*, void*), void *closure);
