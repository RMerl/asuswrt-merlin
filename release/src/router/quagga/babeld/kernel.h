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

#include <netinet/in.h>
#include "babel_main.h"
#include "if.h"

#define KERNEL_INFINITY 0xFFFF

struct kernel_route {
    unsigned char prefix[16];
    int plen;
    int metric;
    unsigned int ifindex;
    int proto;
    unsigned char gw[16];
};

#define ROUTE_FLUSH 0
#define ROUTE_ADD 1
#define ROUTE_MODIFY 2

extern int export_table, import_table;

int kernel_interface_operational(struct interface *interface);
int kernel_interface_mtu(struct interface *interface);
int kernel_interface_wireless(struct interface *interface);
int kernel_route(int operation, const unsigned char *dest, unsigned short plen,
                 const unsigned char *gate, int ifindex, unsigned int metric,
                 const unsigned char *newgate, int newifindex,
                 unsigned int newmetric);
int if_eui64(char *ifname, int ifindex, unsigned char *eui);
int gettime(struct timeval *tv);
int read_random_bytes(void *buf, size_t len);
