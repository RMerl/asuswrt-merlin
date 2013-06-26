/* 
 * TEST implementation of an Shadow Copy module
 *
 * Copyright (C) Stefan Metzmacher	2003
 * Copyright (C) Jeremy Allison 2009.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "ntioctl.h"
#include "smbd/proto.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

/* USE THIS MODULE ONLY FOR TESTING!!!! */

/*    
    For this share
    Z:\

    the ShadowCopies are in this directories

    Z:\@GMT-2003.08.05-12.00.00\
    Z:\@GMT-2003.08.05-12.01.00\
    Z:\@GMT-2003.08.05-12.02.00\

    e.g.
    
    Z:\testfile.txt
    Z:\@GMT-2003.08.05-12.02.00\testfile.txt

    or:

    Z:\testdir\testfile.txt
    Z:\@GMT-2003.08.05-12.02.00\testdir\testfile.txt


    Note: Files must differ to be displayed via Windows Explorer!
	  Directories are always displayed...    
*/

static int test_get_shadow_copy_data(vfs_handle_struct *handle,
				    files_struct *fsp,
				    struct shadow_copy_data *shadow_copy_data,
				    bool labels)
{
	uint32 num = 3;
	uint32 i;
	
	shadow_copy_data->num_volumes = num;
	
	if (labels) {	
		if (num) {
			shadow_copy_data->labels = TALLOC_ZERO_ARRAY(shadow_copy_data,SHADOW_COPY_LABEL,num);
		} else {
			shadow_copy_data->labels = NULL;
		}
		for (i=0;i<num;i++) {
			snprintf(shadow_copy_data->labels[i], sizeof(SHADOW_COPY_LABEL), "@GMT-2003.08.05-12.%02u.00",i);
		}
	} else {
		shadow_copy_data->labels = NULL;
	}

	return 0;
}

/* VFS operations structure */

static struct vfs_fn_pointers vfs_test_shadow_copy_fns = {
	.get_shadow_copy_data = test_get_shadow_copy_data
};

NTSTATUS vfs_shadow_copy_test_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"shadow_copy_test",
				&vfs_test_shadow_copy_fns);
}
