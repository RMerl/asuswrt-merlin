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

#define REQUEST_TIMEOUT 65000
#define RESEND_MAX 3

#define RESEND_REQUEST 1
#define RESEND_UPDATE 2

struct resend {
    unsigned char kind;
    unsigned char max;
    unsigned short delay;
    struct timeval time;
    unsigned char prefix[16];
    unsigned char plen;
    unsigned short seqno;
    unsigned char id[8];
    struct interface *ifp;
    struct resend *next;
};

extern struct timeval resend_time;

struct resend *find_request(const unsigned char *prefix, unsigned char plen,
                            struct resend **previous_return);
void flush_resends(struct neighbour *neigh);
int record_resend(int kind, const unsigned char *prefix, unsigned char plen,
                   unsigned short seqno, const unsigned char *id,
                   struct interface *ifp, int delay);
int unsatisfied_request(const unsigned char *prefix, unsigned char plen,
                        unsigned short seqno, const unsigned char *id);
int request_redundant(struct interface *ifp,
                      const unsigned char *prefix, unsigned char plen,
                      unsigned short seqno, const unsigned char *id);
int satisfy_request(const unsigned char *prefix, unsigned char plen,
                    unsigned short seqno, const unsigned char *id,
                    struct interface *ifp);

void expire_resend(void);
void recompute_resend_time(void);
void do_resend(void);
