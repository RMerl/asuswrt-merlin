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
/*
    Copyright 2001, ASUSTeK Inc.
    All Rights Reserved.
    
    This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
    the contents of this file may not be disclosed to third parties, copied or
    duplicated in any form, in whole or in part, without the prior written
    permission of ASUSTeK Inc..
*/
/*
 * This module creates an array of name, value pairs
 * and supports updating the nvram space. 
 *
 * This module requires the following support routines
 *
 *      malloc, free, strcmp, strncmp, strcpy, strtol, strchr, printf and sprintf
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <bcmnvram.h>
#include <sys/mman.h>

#define MAX_LINE_SIZE 512
#define MAX_FILE_NAME 64

/*
 * NOTE : The mutex must be initialized in the OS previous to the point at
 *	   which multiple entry points to the nvram code are enabled 
 *
 */
#define MAX_NVRAM_SIZE 4096
#define EPI_VERSION_STR "2.4.5"

/*
 * Set the value of an NVRAM variable
 * @param	name	name of variable to set
 * @param	value	value of variable
 * @return	0 on success and errno on failure
 */
int
nvram_set_x(const char *sid, const char *name, const char *value)
{	    
     return (nvram_set(name, value));
}


/*
 * Get the value of an NVRAM variable
 * @param	name	name of variable to get
 * @return	value of variable or NULL if undefined
 */
char*
nvram_get_x(const char *sid, const char *name)
{	
   return (nvram_safe_get(name));
}


/*
 * Get the value of an NVRAM variable
 * @param	name	name of variable to get
 * @return	value of variable or NULL if undefined
 */
char*
nvram_get_f(const char *file, const char *field)
{ 
    return (nvram_safe_get(field));
}

/*
 * Set the value of an NVRAM variable from file
 * @param	name	name of variable to get
 * @return	value of variable or NULL if undefined
 */
int 
nvram_set_f(const char *file, const char *field, const char *value)
{
    return (nvram_set(field, value));
}

/*
 * Get the value of an NVRAM variable list 
 * @param	name	name of variable to get
 *	      index   index of the list, start from 1
 * @return	value of variable or NULL if undefined
 */

char *nvram_get_list_x(const char *sid, const char *name, int index)
{   
    char new_name[MAX_LINE_SIZE];
    
    sprintf(new_name, "%s%d", name, index);
    
    return (nvram_get_f(sid, new_name));	 
}

/*
 * Add the value into the end an NVRAM variable list 
 * @param	name	name of variable to get
 * @return	0 on success and errno on failure
 */
int nvram_add_lists_x(const char *sid, const char *name, const char *value, int count)
{    	
    char name1[32], name2[32];
  
    strcpy(name1, name);
  
  if (name[0]!='\0')
    {	
	sprintf(name2, "%s%d", name1, count);
    	nvram_set(name2, value);
    }	
    return (0);		
}


/*
 * Delete the value from an NVRAM variable list 
 * @param	name	name of variable list
 *	      index   index of variable list
 * @return	0 on success and errno on failure
 */
int nvram_del_lists_x(const char *sid, const char *name, int *delMap)
{
//    FILE *fp;
    char names[32], oname[32], nname[32], *oval, *nval;
    int oi, ni, di;
    
    strcpy(names, name);
    
    if (names[0]!='\0')
    {	
	oi=0;
	ni=0;
	di=0;
	while (1)
	{
		sprintf(oname, "%s%d", names, oi);		
		sprintf(nname, "%s%d", names, ni);

		oval = nvram_get(oname);
		nval = nvram_get(nname);

		if (oval==NULL) break;

		printf("d: %d %d %d %d\n", oi, ni, di, delMap[di]);
		if (delMap[di]!=-1&&delMap[di]==oi)
		{
			oi++;
			di++;
		}
		else
		{
			nvram_set(nname, oval);
			ni++;
			oi++;
		}	
	}
    }			
    return (0);	
}

