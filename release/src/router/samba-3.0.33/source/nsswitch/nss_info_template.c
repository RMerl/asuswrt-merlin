/* 
   Unix SMB/CIFS implementation.
   idMap nss template plugin

   Copyright (C) Gerald Carter             2006

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA  02111-1307, USA.   
*/

#include "includes.h"
#include "nss_info.h"

/************************************************************************
 ***********************************************************************/

static NTSTATUS nss_template_init( struct nss_domain_entry *e )
{
	return NT_STATUS_OK;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS nss_template_get_info( struct nss_domain_entry *e,
				       const DOM_SID *sid, 
				       TALLOC_CTX *ctx,
				       ADS_STRUCT *ads,
				       LDAPMessage *msg,
				       char **homedir,
				       char **shell, 
				       char **gecos,
				       gid_t *gid )
{     
	if ( !homedir || !shell || !gecos )
		return NT_STATUS_INVALID_PARAMETER;
	
	*homedir = talloc_strdup( ctx, lp_template_homedir() );
	*shell   = talloc_strdup( ctx, lp_template_shell() );
	*gecos   = NULL;

	if ( !*homedir || !*shell ) {
		return NT_STATUS_NO_MEMORY;
	}
	
	return NT_STATUS_OK;
}

/************************************************************************
 ***********************************************************************/

static NTSTATUS nss_template_close( void )
{
	return NT_STATUS_OK;
}


/************************************************************************
 ***********************************************************************/

static struct nss_info_methods nss_template_methods = {
	.init         = nss_template_init,
	.get_nss_info = nss_template_get_info,
	.close_fn     = nss_template_close
};
		
NTSTATUS nss_info_template_init( void )
{
	return smb_register_idmap_nss(SMB_NSS_INFO_INTERFACE_VERSION, 
				      "template", 
				      &nss_template_methods);	
}

