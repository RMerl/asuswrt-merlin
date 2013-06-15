/* 
   Unix SMB/Netbios implementation.
   Version 2.0

   Printer security permission manipulation.

   Copyright (C) Tim Potter 2000
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* This program can get or set NT printer security permissions from the 
   command line.  Usage: psec getsec|setsec printername.  You must have
   write access to the ntdrivers.tdb file to set permissions and read
   access to get permissions.

   For this program to compile using the supplied Makefile.psec, Samba
   must be configured with the --srcdir option

   For getsec, output like the following is sent to standard output:

       S-1-5-21-1067277791-1719175008-3000797951-500

       1 9 0x10000000 S-1-5-21-1067277791-1719175008-3000797951-501
       1 2 0x10000000 S-1-5-21-1067277791-1719175008-3000797951-501
       0 9 0x10000000 S-1-5-21-1067277791-1719175008-3000797951-500
       0 2 0x10000000 S-1-5-21-1067277791-1719175008-3000797951-500
       0 9 0x10000000 S-1-5-21-1067277791-1719175008-3000797951-513
       0 2 0x00020000 S-1-5-21-1067277791-1719175008-3000797951-513
       0 2 0xe0000000 S-1-1-0

   The first two lines describe the owner user and owner group of the printer.
   If either of these lines are blank then the respective owner property is 
   not set.  The remaining lines list the printer permissions or ACE entries, 
   one per line.  Each column describes a different property of the ACE:

       Column    Description
       -------------------------------------------------------------------
         1       ACE type (allow/deny etc) defined in rpc_secdes.h
         2       ACE flags defined in rpc_secdes.h
         3       ACE mask - printer ACE masks are defined in rpc_spoolss.h
         4       SID the ACE applies to

   The above example describes the following permissions in order:
 
       - The guest user has No Access to the printer
       - The domain administrator has Full Access
       - Domain Users can Manage Documents
       - Everyone has Print access

   The setsec command takes the output format but sets the security descriptor
   appropriately. */

#include "includes.h"

TDB_CONTEXT *tdb;

/* ACE type conversions */

char *ace_type_to_str(uint ace_type)
{
	static fstring temp;

	switch(ace_type) {
	case SEC_ACE_TYPE_ACCESS_DENIED:
		return "DENY";
	case SEC_ACE_TYPE_ACCESS_ALLOWED:
		return "ALLOW";
	}

	slprintf(temp, sizeof(temp) - 1, "0x%02x", ace_type);
	return temp;
}

uint str_to_ace_type(char *ace_type)
{
	if (strcmp(ace_type, "ALLOWED") == 0) 
		return SEC_ACE_TYPE_ACCESS_ALLOWED;

	if (strcmp(ace_type, "DENIED") == 0)
		return SEC_ACE_TYPE_ACCESS_DENIED;

	return -1;
}		

/* ACE mask (permission) conversions */

char *ace_mask_to_str(uint32 ace_mask)
{
	static fstring temp;

	switch (ace_mask) {
	case PRINTER_ACE_FULL_CONTROL:
		return "Full Control";
	case PRINTER_ACE_MANAGE_DOCUMENTS:
		return "Manage Documents";
	case PRINTER_ACE_PRINT:
		return "Print";
	}

	slprintf(temp, sizeof(temp) - 1, "0x%08x", ace_mask);
	return temp;
}

uint32 str_to_ace_mask(char *ace_mask)
{
	if (strcmp(ace_mask, "Full Control") == 0) 
		return PRINTER_ACE_FULL_CONTROL;

	if (strcmp(ace_mask, "Manage Documents") == 0)
		return PRINTER_ACE_MANAGE_DOCUMENTS;

	if (strcmp(ace_mask, "Print") == 0)
		return PRINTER_ACE_PRINT;

	return -1;
}

/* ACE conversions */

char *ace_to_str(SEC_ACE *ace)
{
	static pstring temp;
	fstring sidstr;

	sid_to_string(sidstr, &ace->sid);

	slprintf(temp, sizeof(temp) - 1, "%s %d %s %s", 
		 ace_type_to_str(ace->type), ace->flags,
		 ace_mask_to_str(ace->info.mask), sidstr);

	return temp;
}

void str_to_ace(SEC_ACE *ace, char *ace_str)
{
	SEC_ACCESS sa;
	DOM_SID sid;
	uint32 mask;
	uint8 type, flags;

	init_sec_access(&sa, mask);
	init_sec_ace(ace, &sid, type, sa, flags);
}

/* Get a printer security descriptor */

int psec_getsec(char *printer)
{
	SEC_DESC_BUF *secdesc_ctr = NULL;
	TALLOC_CTX *mem_ctx = NULL;
	fstring keystr, sidstr, tdb_path;
	prs_struct ps;
	int result = 0, i;

	ZERO_STRUCT(ps);

	/* Open tdb for reading */

	slprintf(tdb_path, sizeof(tdb_path) - 1, "%s/ntdrivers.tdb", 
		 lp_lockdir());

	tdb = tdb_open(tdb_path, 0, 0, O_RDONLY, 0600);

	if (!tdb) {
		printf("psec: failed to open nt drivers database: %s\n",
		       sys_errlist[errno]);
		return 1;
	}

	/* Get security blob from tdb */

	slprintf(keystr, sizeof(keystr) - 1, "SECDESC/%s", printer);

	mem_ctx = talloc_init();

	if (!mem_ctx) {
		printf("memory allocation error\n");
		result = 1;
		goto done;
	}

	if (tdb_prs_fetch(tdb, keystr, &ps, mem_ctx) != 0) {
		printf("error fetching descriptor for printer %s\n",
		       printer);
		/* cannot do a prs_mem_free() when tdb_prs_fetch fails */
		/* as the prs structure has not been initialized */
		tdb_close(tdb);
		talloc_destroy(mem_ctx);
		return 1;
	}

	/* Unpack into security descriptor buffer */

	if (!sec_io_desc_buf("nt_printing_getsec", &secdesc_ctr, &ps, 1)) {
		printf("error unpacking sec_desc_buf\n");
		result = 1;
		goto done;
	}

	/* Print owner and group sid */

	if (secdesc_ctr->sec->owner_sid) {
		sid_to_string(sidstr, secdesc_ctr->sec->owner_sid);
	} else {
		fstrcpy(sidstr, "");
	}

	printf("%s\n", sidstr);

	if (secdesc_ctr->sec->grp_sid) {
		sid_to_string(sidstr, secdesc_ctr->sec->grp_sid);
	} else {
		fstrcpy(sidstr, "");
	}

	printf("%s\n", sidstr);

	/* Print aces */

	if (!secdesc_ctr->sec->dacl) {
		result = 0;
		goto done;
	}

	for (i = 0; i < secdesc_ctr->sec->dacl->num_aces; i++) {
		SEC_ACE *ace = &secdesc_ctr->sec->dacl->ace[i];

		sid_to_string(sidstr, &ace->sid);

		printf("%d %d 0x%08x %s\n", ace->type, ace->flags,
		       ace->info.mask, sidstr);
	}

 done:
	if (tdb) tdb_close(tdb);
	if (mem_ctx) talloc_destroy(mem_ctx);
	if (secdesc_ctr) free_sec_desc_buf(&secdesc_ctr);
	prs_mem_free(&ps);

	return result;
}

/* Set a printer security descriptor */

int psec_setsec(char *printer)
{
	DOM_SID user_sid, group_sid;
	SEC_ACE *ace_list = NULL;
	SEC_ACL *dacl = NULL;
	SEC_DESC *sd;
	SEC_DESC_BUF *sdb = NULL;
	int result = 0, num_aces = 0;
	fstring line, keystr, tdb_path;
	size_t size;
	prs_struct ps;
	TALLOC_CTX *mem_ctx = NULL;
	BOOL has_user_sid = False, has_group_sid = False;

	ZERO_STRUCT(ps);

	/* Open tdb for reading */

	slprintf(tdb_path, sizeof(tdb_path) - 1, "%s/ntdrivers.tdb", 
		 lp_lockdir());

	tdb = tdb_open(tdb_path, 0, 0, O_RDWR, 0600);

	if (!tdb) {
		printf("psec: failed to open nt drivers database: %s\n",
		       sys_errlist[errno]);
		result = 1;
		goto done;
	}

	/* Read owner and group sid */

	fgets(line, sizeof(fstring), stdin);
	if (line[0] != '\n') {
		string_to_sid(&user_sid, line);
		has_user_sid = True;
	}

	fgets(line, sizeof(fstring), stdin);
	if (line[0] != '\n') {
		string_to_sid(&group_sid, line);
		has_group_sid = True;
	}

	/* Read ACEs from standard input for discretionary ACL */

	while(fgets(line, sizeof(fstring), stdin)) {
		int ace_type, ace_flags;
		uint32 ace_mask;
		fstring sidstr;
		DOM_SID sid;
		SEC_ACCESS sa;

		if (sscanf(line, "%d %d 0x%x %s", &ace_type, &ace_flags, 
			   &ace_mask, sidstr) != 4) {
			continue;
		}

		string_to_sid(&sid, sidstr);
		
		ace_list = Realloc(ace_list, sizeof(SEC_ACE) * 
				   (num_aces + 1));
		
		init_sec_access(&sa, ace_mask);
		init_sec_ace(&ace_list[num_aces], &sid, ace_type, sa, 
			     ace_flags);

		num_aces++;
	}

	dacl = make_sec_acl(ACL_REVISION, num_aces, ace_list);
	free(ace_list);

	/* Create security descriptor */

	sd = make_sec_desc(SEC_DESC_REVISION,
			   has_user_sid ? &user_sid : NULL, 
			   has_group_sid ? &group_sid : NULL,
			   NULL, /* System ACL */
			   dacl, /* Discretionary ACL */
			   &size);

	free_sec_acl(&dacl);

	sdb = make_sec_desc_buf(size, sd);

	free_sec_desc(&sd);

	/* Write security descriptor to tdb */

	mem_ctx = talloc_init();

	if (!mem_ctx) {
		printf("memory allocation error\n");
		result = 1;
		goto done;
	}

	prs_init(&ps, (uint32)sec_desc_size(sdb->sec) + 
		 sizeof(SEC_DESC_BUF), 4, mem_ctx, MARSHALL);

	if (!sec_io_desc_buf("nt_printing_setsec", &sdb, &ps, 1)) {
		printf("sec_io_desc_buf failed\n");
		goto done;
	}

	slprintf(keystr, sizeof(keystr) - 1, "SECDESC/%s", printer);

	if (!tdb_prs_store(tdb, keystr, &ps)==0) {
		printf("Failed to store secdesc for %s\n", printer);
		goto done;
	}

 done:
	if (tdb) tdb_close(tdb);
	if (sdb) free_sec_desc_buf(&sdb);
	if (mem_ctx) talloc_destroy(mem_ctx);
	prs_mem_free(&ps);

	return result;
}

/* Help */

void usage(void)
{
	printf("Usage: psec getsec|setsec printername\n");
}

/* Main function */

int main(int argc, char **argv)
{
	pstring servicesf = CONFIGFILE;

	/* Argument check */

	if (argc == 1) {
		usage();
		return 1;
	}

	/* Load smb.conf file */

	charset_initialise();

	if (!lp_load(servicesf,False,False,True)) {
		fprintf(stderr, "Couldn't load confiuration file %s\n",
			servicesf);
		return 1;
	}

	/* Do commands */

	if (strcmp(argv[1], "setsec") == 0) {

		if (argc != 3) {
			usage();
			return 1;
		}

		return psec_setsec(argv[2]);
	}

	if (strcmp(argv[1], "getsec") == 0) {

		if (argc != 3) {
			usage();
			return 1;
		}

		return psec_getsec(argv[2]);
	}

	/* An unknown command */

	printf("psec: unknown command %s\n", argv[1]);
	return 1;
}
