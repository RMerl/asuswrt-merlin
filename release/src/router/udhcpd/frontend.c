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
#include <string.h>

extern int udhcpd_main(int argc, char *argv[]);
extern int udhcpc_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	int ret = 0;
	char *base = strrchr(argv[0], '/');
	
	if (strstr(base ? (base + 1) : argv[0], "dhcpd"))
		ret = udhcpd_main(argc, argv);
	else ret = udhcpc_main(argc, argv);
	
	return ret;
}
