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

extern int ej_get_leases_array(int eid, webs_t wp, int argc, char_t **argv);

#ifdef RTCONFIG_IPV6
#ifdef RTCONFIG_IGD2
extern int ej_ipv6_pinhole_array(int eid, webs_t wp, int argc, char_t **argv);
#endif
#endif

extern int ej_get_vserver_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_get_upnp_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_get_route_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_lan_ipv6_network_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_tcclass_dump_array(int eid, webs_t wp, int argc, char_t **argv);
int tcclass_dump(FILE *fp, webs_t wp);

extern int ej_iptmon(int eid, webs_t wp, int argc, char **argv);
extern int ej_ipt_bandwidth(int eid, webs_t wp, int argc, char **argv);
extern int ej_iptraffic(int eid, webs_t wp, int argc, char **argv);
extern void iptraffic_conntrack_init();
extern void ctvbuf(FILE *f);
