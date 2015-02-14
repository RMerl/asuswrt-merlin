/* 
* Debug.c
*
*Copyright (C) 2010 Beceem Communications, Inc.
*
*This program is free software: you can redistribute it and/or modify 
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation. 
*
*This program is distributed in the hope that it will be useful,but 
*WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
*See the GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with this program. If not, write to the Free Software Foundation, Inc.,
*51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/


#include<headers.h>

char *buff_dump_base[]={"DEC", "HEX",  "OCT", "BIN"	};

static UINT current_debug_level=BCM_SCREAM;

int bcm_print_buffer( UINT debug_level, const char *function_name, 
				  char *file_name, int line_number, unsigned char *buffer, int bufferlen, unsigned int base)
{
	if(debug_level>=current_debug_level)
	{
		int i=0;
		printk("\n%s:%s:%d:Buffer dump of size 0x%x in the %s:\n", file_name, function_name, line_number, bufferlen, buff_dump_base[1]);
		for(;i<bufferlen;i++)
		{
			if(i && !(i%16) )
				printk("\n");
			switch(base)
			{
				case BCM_BASE_TYPE_DEC:
					printk("%03d ", buffer[i]);
					break;
				case BCM_BASE_TYPE_OCT:
					printk("%0x03o ", buffer[i]);
					break;
				case BCM_BASE_TYPE_BIN:
					printk("%02x ", buffer[i]);
					break;
				case BCM_BASE_TYPE_HEX:
				default:
					printk("%02X ", buffer[i]);
					break;
			}
		}
		printk("\n");
	}
	return 0;
} 


