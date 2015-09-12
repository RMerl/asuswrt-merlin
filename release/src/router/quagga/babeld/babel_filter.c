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

#include "babel_filter.h"
#include "vty.h"
#include "filter.h"
#include "log.h"
#include "plist.h"
#include "distribute.h"
#include "util.h"

int
babel_filter(int output, const unsigned char *prefix, unsigned short plen,
             unsigned int ifindex)
{
    struct interface *ifp = if_lookup_by_index(ifindex);
    babel_interface_nfo *babel_ifp = ifp ? babel_get_if_nfo(ifp) : NULL;
    struct prefix p;
    struct distribute *dist;
    struct access_list *alist;
    struct prefix_list *plist;
    int filter = output ? BABEL_FILTER_OUT : BABEL_FILTER_IN;
    int distribute = output ? DISTRIBUTE_OUT : DISTRIBUTE_IN;

    p.family = v4mapped(prefix) ? AF_INET : AF_INET6;
    p.prefixlen = v4mapped(prefix) ? plen - 96 : plen;
    if (p.family == AF_INET)
        uchar_to_inaddr(&p.u.prefix4, prefix);
    else
        uchar_to_in6addr(&p.u.prefix6, prefix);

    if (babel_ifp != NULL && babel_ifp->list[filter]) {
        if (access_list_apply (babel_ifp->list[filter], &p)
            == FILTER_DENY) {
            debugf(BABEL_DEBUG_FILTER,
                   "%s/%d filtered by distribute in",
                   p.family == AF_INET ?
                   inet_ntoa(p.u.prefix4) :
                   inet6_ntoa (p.u.prefix6),
                   p.prefixlen);
            return INFINITY;
	}
    }
    if (babel_ifp != NULL && babel_ifp->prefix[filter]) {
        if (prefix_list_apply (babel_ifp->prefix[filter], &p)
            == PREFIX_DENY) {
            debugf(BABEL_DEBUG_FILTER, "%s/%d filtered by distribute in",
                        p.family == AF_INET ?
                        inet_ntoa(p.u.prefix4) :
                        inet6_ntoa (p.u.prefix6),
                        p.prefixlen);
            return INFINITY;
	}
    }

    /* All interface filter check. */
    dist = distribute_lookup (NULL);
    if (dist) {
        if (dist->list[distribute]) {
            alist = access_list_lookup (AFI_IP6, dist->list[distribute]);

            if (alist) {
                if (access_list_apply (alist, &p) == FILTER_DENY) {
                    debugf(BABEL_DEBUG_FILTER, "%s/%d filtered by distribute in",
                                p.family == AF_INET ?
                                inet_ntoa(p.u.prefix4) :
                                inet6_ntoa (p.u.prefix6),
                                p.prefixlen);
                    return INFINITY;
		}
	    }
	}
        if (dist->prefix[distribute]) {
            plist = prefix_list_lookup (AFI_IP6, dist->prefix[distribute]);
            if (plist) {
                if (prefix_list_apply (plist, &p) == PREFIX_DENY) {
                    debugf(BABEL_DEBUG_FILTER, "%s/%d filtered by distribute in",
                                p.family == AF_INET ?
                                inet_ntoa(p.u.prefix4) :
                                inet6_ntoa (p.u.prefix6),
                                p.prefixlen);
                    return INFINITY;
		}
	    }
	}
    }
    return 0;
}
