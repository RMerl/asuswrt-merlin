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

struct neighbour {
    struct neighbour *next;
    /* This is -1 when unknown, so don't make it unsigned */
    int hello_seqno;
    unsigned char address[16];
    unsigned short reach;
    unsigned short txcost;
    struct timeval hello_time;
    struct timeval ihu_time;
    unsigned short hello_interval; /* in centiseconds */
    unsigned short ihu_interval;   /* in centiseconds */
    struct interface *ifp;
};

extern struct neighbour *neighs;

#define FOR_ALL_NEIGHBOURS(_neigh) \
    for(_neigh = neighs; _neigh; _neigh = _neigh->next)

int neighbour_valid(struct neighbour *neigh);
void flush_neighbour(struct neighbour *neigh);
struct neighbour *find_neighbour(const unsigned char *address,
                                 struct interface *ifp);
int update_neighbour(struct neighbour *neigh, int hello, int hello_interval);
unsigned check_neighbours(void);
unsigned neighbour_txcost(struct neighbour *neigh);
unsigned neighbour_rxcost(struct neighbour *neigh);
unsigned neighbour_cost(struct neighbour *neigh);
