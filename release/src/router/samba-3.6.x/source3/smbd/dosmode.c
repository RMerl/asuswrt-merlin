/* 
   Unix SMB/CIFS implementation.
   dos mode handling functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) James Peach 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "librpc/gen_ndr/ndr_xattr.h"
#include "../libcli/security/security.h"
#include "smbd/smbd.h"

static uint32_t filter_mode_by_protocol(uint32_t mode)
{
	if (get_Protocol() <= PROTOCOL_LANMAN2) {
		DEBUG(10,("filter_mode_by_protocol: "
			"filtering result 0x%x to 0x%x\n",
			(unsigned int)mode,
			(unsigned int)(mode & 0x3f) ));
		mode &= 0x3f;
	}
	return mode;
}

static int set_link_read_only_flag(const SMB_STRUCT_STAT *const sbuf)
{
#ifdef S_ISLNK
#if LINKS_READ_ONLY
	if (S_ISLNK(sbuf->st_mode) && S_ISDIR(sbuf->st_mode))
		return FILE_ATTRIBUTE_READONLY;
#endif
#endif
	return 0;
}

/****************************************************************************
 Change a dos mode to a unix mode.
    Base permission for files:
         if creating file and inheriting (i.e. parent_dir != NULL)
           apply read/write bits from parent directory.
         else   
           everybody gets read bit set
         dos readonly is represented in unix by removing everyone's write bit
         dos archive is represented in unix by the user's execute bit
         dos system is represented in unix by the group's execute bit
         dos hidden is represented in unix by the other's execute bit
         if !inheriting {
           Then apply create mask,
           then add force bits.
         }
    Base permission for directories:
         dos directory is represented in unix by unix's dir bit and the exec bit
         if !inheriting {
           Then apply create mask,
           then add force bits.
         }
****************************************************************************/

mode_t unix_mode(connection_struct *conn, int dosmode,
		 const struct smb_filename *smb_fname,
		 const char *inherit_from_dir)
{
	mode_t result = (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
	mode_t dir_mode = 0; /* Mode of the inherit_from directory if
			      * inheriting. */

	if (!lp_store_dos_attributes(SNUM(conn)) && IS_DOS_READONLY(dosmode)) {
		result &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	}

	if ((inherit_from_dir != NULL) && lp_inherit_perms(SNUM(conn))) {
		struct smb_filename *smb_fname_parent = NULL;
		NTSTATUS status;

		DEBUG(2, ("unix_mode(%s) inheriting from %s\n",
			  smb_fname_str_dbg(smb_fname),
			  inherit_from_dir));

		status = create_synthetic_smb_fname(talloc_tos(),
						    inherit_from_dir, NULL,
						    NULL, &smb_fname_parent);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1,("unix_mode(%s) failed, [dir %s]: %s\n",
				 smb_fname_str_dbg(smb_fname),
				 inherit_from_dir, nt_errstr(status)));
			return(0);
		}

		if (SMB_VFS_STAT(conn, smb_fname_parent) != 0) {
			DEBUG(4,("unix_mode(%s) failed, [dir %s]: %s\n",
				 smb_fname_str_dbg(smb_fname),
				 inherit_from_dir, strerror(errno)));
			TALLOC_FREE(smb_fname_parent);
			return(0);      /* *** shouldn't happen! *** */
		}

		/* Save for later - but explicitly remove setuid bit for safety. */
		dir_mode = smb_fname_parent->st.st_ex_mode & ~S_ISUID;
		DEBUG(2,("unix_mode(%s) inherit mode %o\n",
			 smb_fname_str_dbg(smb_fname), (int)dir_mode));
		/* Clear "result" */
		result = 0;
		TALLOC_FREE(smb_fname_parent);
	} 

	if (IS_DOS_DIR(dosmode)) {
		/* We never make directories read only for the owner as under DOS a user
		can always create a file in a read-only directory. */
		result |= (S_IFDIR | S_IWUSR);

		if (dir_mode) {
			/* Inherit mode of parent directory. */
			result |= dir_mode;
		} else {
			/* Provisionally add all 'x' bits */
			result |= (S_IXUSR | S_IXGRP | S_IXOTH);                 

			/* Apply directory mask */
			result &= lp_dir_mask(SNUM(conn));
			/* Add in force bits */
			result |= lp_force_dir_mode(SNUM(conn));
		}
	} else { 
		if (lp_map_archive(SNUM(conn)) && IS_DOS_ARCHIVE(dosmode))
			result |= S_IXUSR;

		if (lp_map_system(SNUM(conn)) && IS_DOS_SYSTEM(dosmode))
			result |= S_IXGRP;

		if (lp_map_hidden(SNUM(conn)) && IS_DOS_HIDDEN(dosmode))
			result |= S_IXOTH;  

		if (dir_mode) {
			/* Inherit 666 component of parent directory mode */
			result |= dir_mode & (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
		} else {
			/* Apply mode mask */
			result &= lp_create_mask(SNUM(conn));
			/* Add in force bits */
			result |= lp_force_create_mode(SNUM(conn));
		}
	}

	DEBUG(3,("unix_mode(%s) returning 0%o\n", smb_fname_str_dbg(smb_fname),
		 (int)result));
	return(result);
}

/****************************************************************************
 Change a unix mode to a dos mode.
****************************************************************************/

static uint32 dos_mode_from_sbuf(connection_struct *conn,
				 const struct smb_filename *smb_fname)
{
	int result = 0;
	enum mapreadonly_options ro_opts = (enum mapreadonly_options)lp_map_readonly(SNUM(conn));

	if (ro_opts == MAP_READONLY_YES) {
		/* Original Samba method - map inverse of user "w" bit. */
		if ((smb_fname->st.st_ex_mode & S_IWUSR) == 0) {
			result |= FILE_ATTRIBUTE_READONLY;
		}
	} else if (ro_opts == MAP_READONLY_PERMISSIONS) {
		/* Check actual permissions for read-only. */
		if (!can_write_to_file(conn, smb_fname)) {
			result |= FILE_ATTRIBUTE_READONLY;
		}
	} /* Else never set the readonly bit. */

	if (MAP_ARCHIVE(conn) && ((smb_fname->st.st_ex_mode & S_IXUSR) != 0))
		result |= FILE_ATTRIBUTE_ARCHIVE;

	if (MAP_SYSTEM(conn) && ((smb_fname->st.st_ex_mode & S_IXGRP) != 0))
		result |= FILE_ATTRIBUTE_SYSTEM;

	if (MAP_HIDDEN(conn) && ((smb_fname->st.st_ex_mode & S_IXOTH) != 0))
		result |= FILE_ATTRIBUTE_HIDDEN;

	if (S_ISDIR(smb_fname->st.st_ex_mode))
		result = FILE_ATTRIBUTE_DIRECTORY | (result & FILE_ATTRIBUTE_READONLY);

	result |= set_link_read_only_flag(&smb_fname->st);

	DEBUG(8,("dos_mode_from_sbuf returning "));

	if (result & FILE_ATTRIBUTE_HIDDEN) DEBUG(8, ("h"));
	if (result & FILE_ATTRIBUTE_READONLY ) DEBUG(8, ("r"));
	if (result & FILE_ATTRIBUTE_SYSTEM) DEBUG(8, ("s"));
	if (result & FILE_ATTRIBUTE_DIRECTORY   ) DEBUG(8, ("d"));
	if (result & FILE_ATTRIBUTE_ARCHIVE  ) DEBUG(8, ("a"));

	DEBUG(8,("\n"));
	return result;
}

/****************************************************************************
 Get DOS attributes from an EA.
 This can also pull the create time into the stat struct inside smb_fname.
****************************************************************************/

static bool get_ea_dos_attribute(connection_struct *conn,
				 struct smb_filename *smb_fname,
				 uint32 *pattr)
{
	struct xattr_DOSATTRIB dosattrib;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	ssize_t sizeret;
	fstring attrstr;
	uint32_t dosattr;

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return False;
	}

	/* Don't reset pattr to zero as we may already have filename-based attributes we
	   need to preserve. */

	sizeret = SMB_VFS_GETXATTR(conn, smb_fname->base_name,
				   SAMBA_XATTR_DOS_ATTRIB, attrstr,
				   sizeof(attrstr));
	if (sizeret == -1) {
		if (errno == ENOSYS
#if defined(ENOTSUP)
			|| errno == ENOTSUP) {
#else
				) {
#endif
			DEBUG(1,("get_ea_dos_attribute: Cannot get attribute "
				 "from EA on file %s: Error = %s\n",
				 smb_fname_str_dbg(smb_fname),
				 strerror(errno)));
			set_store_dos_attributes(SNUM(conn), False);
		}
		return False;
	}

	blob.data = (uint8_t *)attrstr;
	blob.length = sizeret;

	ndr_err = ndr_pull_struct_blob(&blob, talloc_tos(), &dosattrib,
			(ndr_pull_flags_fn_t)ndr_pull_xattr_DOSATTRIB);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(1,("get_ea_dos_attribute: bad ndr decode "
			 "from EA on file %s: Error = %s\n",
			 smb_fname_str_dbg(smb_fname),
			 ndr_errstr(ndr_err)));
		return false;
	}

	DEBUG(10,("get_ea_dos_attribute: %s attr = %s\n",
		  smb_fname_str_dbg(smb_fname), dosattrib.attrib_hex));

	switch (dosattrib.version) {
		case 0xFFFF:
			dosattr = dosattrib.info.compatinfoFFFF.attrib;
			break;
		case 1:
			dosattr = dosattrib.info.info1.attrib;
			if (!null_nttime(dosattrib.info.info1.create_time)) {
				struct timespec create_time =
					nt_time_to_unix_timespec(
						&dosattrib.info.info1.create_time);

				update_stat_ex_create_time(&smb_fname->st,
							create_time);

				DEBUG(10,("get_ea_dos_attribute: file %s case 1 "
					"set btime %s\n",
					smb_fname_str_dbg(smb_fname),
					time_to_asc(convert_timespec_to_time_t(
						create_time)) ));
			}
			break;
		case 2:
			dosattr = dosattrib.info.oldinfo2.attrib;
			/* Don't know what flags to check for this case. */
			break;
		case 3:
			dosattr = dosattrib.info.info3.attrib;
			if ((dosattrib.info.info3.valid_flags & XATTR_DOSINFO_CREATE_TIME) &&
					!null_nttime(dosattrib.info.info3.create_time)) {
				struct timespec create_time =
					nt_time_to_unix_timespec(
						&dosattrib.info.info3.create_time);

				update_stat_ex_create_time(&smb_fname->st,
							create_time);

				DEBUG(10,("get_ea_dos_attribute: file %s case 3 "
					"set btime %s\n",
					smb_fname_str_dbg(smb_fname),
					time_to_asc(convert_timespec_to_time_t(
						create_time)) ));
			}
			break;
		default:
			DEBUG(1,("get_ea_dos_attribute: Badly formed DOSATTRIB on "
				 "file %s - %s\n", smb_fname_str_dbg(smb_fname),
				 attrstr));
	                return false;
	}

	if (S_ISDIR(smb_fname->st.st_ex_mode)) {
		dosattr |= FILE_ATTRIBUTE_DIRECTORY;
	}
	/* FILE_ATTRIBUTE_SPARSE is valid on get but not on set. */
	*pattr = (uint32)(dosattr & (SAMBA_ATTRIBUTES_MASK|FILE_ATTRIBUTE_SPARSE));

	DEBUG(8,("get_ea_dos_attribute returning (0x%x)", dosattr));

	if (dosattr & FILE_ATTRIBUTE_HIDDEN) DEBUG(8, ("h"));
	if (dosattr & FILE_ATTRIBUTE_READONLY ) DEBUG(8, ("r"));
	if (dosattr & FILE_ATTRIBUTE_SYSTEM) DEBUG(8, ("s"));
	if (dosattr & FILE_ATTRIBUTE_DIRECTORY   ) DEBUG(8, ("d"));
	if (dosattr & FILE_ATTRIBUTE_ARCHIVE  ) DEBUG(8, ("a"));

	DEBUG(8,("\n"));

	return True;
}

/****************************************************************************
 Set DOS attributes in an EA.
 Also sets the create time.
****************************************************************************/

static bool set_ea_dos_attribute(connection_struct *conn,
				 struct smb_filename *smb_fname,
				 uint32 dosmode)
{
	struct xattr_DOSATTRIB dosattrib;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return False;
	}

	ZERO_STRUCT(dosattrib);
	ZERO_STRUCT(blob);

	dosattrib.version = 3;
	dosattrib.info.info3.valid_flags = XATTR_DOSINFO_ATTRIB|
					XATTR_DOSINFO_CREATE_TIME;
	dosattrib.info.info3.attrib = dosmode;
	unix_timespec_to_nt_time(&dosattrib.info.info3.create_time,
				smb_fname->st.st_ex_btime);

	DEBUG(10,("set_ea_dos_attributes: set attribute 0x%x, btime = %s on file %s\n",
		(unsigned int)dosmode,
		time_to_asc(convert_timespec_to_time_t(smb_fname->st.st_ex_btime)),
		smb_fname_str_dbg(smb_fname) ));

	ndr_err = ndr_push_struct_blob(
			&blob, talloc_tos(), &dosattrib,
			(ndr_push_flags_fn_t)ndr_push_xattr_DOSATTRIB);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(5, ("create_acl_blob: ndr_push_xattr_DOSATTRIB failed: %s\n",
			ndr_errstr(ndr_err)));
		return false;
	}

	if (blob.data == NULL || blob.length == 0) {
		return false;
	}

	if (SMB_VFS_SETXATTR(conn, smb_fname->base_name,
			     SAMBA_XATTR_DOS_ATTRIB, blob.data, blob.length,
			     0) == -1) {
		bool ret = false;
		files_struct *fsp = NULL;

		if((errno != EPERM) && (errno != EACCES)) {
			if (errno == ENOSYS
#if defined(ENOTSUP)
				|| errno == ENOTSUP) {
#else
				) {
#endif
				DEBUG(1,("set_ea_dos_attributes: Cannot set "
					 "attribute EA on file %s: Error = %s\n",
					 smb_fname_str_dbg(smb_fname),
					 strerror(errno) ));
				set_store_dos_attributes(SNUM(conn), False);
			}
			return false;
		}

		/* We want DOS semantics, ie allow non owner with write permission to change the
			bits on a file. Just like file_ntimes below.
		*/

		/* Check if we have write access. */
		if(!CAN_WRITE(conn) || !lp_dos_filemode(SNUM(conn)))
			return false;

		/*
		 * We need to open the file with write access whilst
		 * still in our current user context. This ensures we
		 * are not violating security in doing the setxattr.
		 */

		if (!NT_STATUS_IS_OK(open_file_fchmod(conn, smb_fname,
						      &fsp)))
			return false;
		become_root();
		if (SMB_VFS_FSETXATTR(fsp,
				     SAMBA_XATTR_DOS_ATTRIB, blob.data,
				     blob.length, 0) == 0) {
			ret = true;
		}
		unbecome_root();
		close_file(NULL, fsp, NORMAL_CLOSE);
		return ret;
	}
	DEBUG(10,("set_ea_dos_attribute: set EA 0x%x on file %s\n",
		(unsigned int)dosmode,
		smb_fname_str_dbg(smb_fname)));
	return true;
}

/****************************************************************************
 Change a unix mode to a dos mode for an ms dfs link.
****************************************************************************/

uint32 dos_mode_msdfs(connection_struct *conn,
		      const struct smb_filename *smb_fname)
{
	uint32 result = 0;

	DEBUG(8,("dos_mode_msdfs: %s\n", smb_fname_str_dbg(smb_fname)));

	if (!VALID_STAT(smb_fname->st)) {
		return 0;
	}

	/* First do any modifications that depend on the path name. */
	/* hide files with a name starting with a . */
	if (lp_hide_dot_files(SNUM(conn))) {
		const char *p = strrchr_m(smb_fname->base_name, '/');
		if (p) {
			p++;
		} else {
			p = smb_fname->base_name;
		}

		/* Only . and .. are not hidden. */
		if (p[0] == '.' && !((p[1] == '\0') ||
				(p[1] == '.' && p[2] == '\0'))) {
			result |= FILE_ATTRIBUTE_HIDDEN;
		}
	}

	result |= dos_mode_from_sbuf(conn, smb_fname);

	/* Optimization : Only call is_hidden_path if it's not already
	   hidden. */
	if (!(result & FILE_ATTRIBUTE_HIDDEN) &&
	    IS_HIDDEN_PATH(conn, smb_fname->base_name)) {
		result |= FILE_ATTRIBUTE_HIDDEN;
	}

	if (result == 0) {
		result = FILE_ATTRIBUTE_NORMAL;
	}

	result = filter_mode_by_protocol(result);

	/*
	 * Add in that it is a reparse point
	 */
	result |= FILE_ATTRIBUTE_REPARSE_POINT;

	DEBUG(8,("dos_mode_msdfs returning "));

	if (result & FILE_ATTRIBUTE_HIDDEN) DEBUG(8, ("h"));
	if (result & FILE_ATTRIBUTE_READONLY ) DEBUG(8, ("r"));
	if (result & FILE_ATTRIBUTE_SYSTEM) DEBUG(8, ("s"));
	if (result & FILE_ATTRIBUTE_DIRECTORY   ) DEBUG(8, ("d"));
	if (result & FILE_ATTRIBUTE_ARCHIVE  ) DEBUG(8, ("a"));
	if (result & FILE_ATTRIBUTE_SPARSE ) DEBUG(8, ("[sparse]"));

	DEBUG(8,("\n"));

	return(result);
}

#ifdef HAVE_STAT_DOS_FLAGS
/****************************************************************************
 Convert dos attributes (FILE_ATTRIBUTE_*) to dos stat flags (UF_*)
****************************************************************************/

int dos_attributes_to_stat_dos_flags(uint32_t dosmode)
{
	uint32_t dos_stat_flags = 0;

	if (dosmode & FILE_ATTRIBUTE_ARCHIVE)
		dos_stat_flags |= UF_DOS_ARCHIVE;
	if (dosmode & FILE_ATTRIBUTE_HIDDEN)
		dos_stat_flags |= UF_DOS_HIDDEN;
	if (dosmode & FILE_ATTRIBUTE_READONLY)
		dos_stat_flags |= UF_DOS_RO;
	if (dosmode & FILE_ATTRIBUTE_SYSTEM)
		dos_stat_flags |= UF_DOS_SYSTEM;
	if (dosmode & FILE_ATTRIBUTE_NONINDEXED)
		dos_stat_flags |= UF_DOS_NOINDEX;

	return dos_stat_flags;
}

/****************************************************************************
 Gets DOS attributes, accessed via st_ex_flags in the stat struct.
****************************************************************************/

static bool get_stat_dos_flags(connection_struct *conn,
			       const struct smb_filename *smb_fname,
			       uint32_t *dosmode)
{
	SMB_ASSERT(VALID_STAT(smb_fname->st));
	SMB_ASSERT(dosmode);

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return false;
	}

	DEBUG(5, ("Getting stat dos attributes for %s.\n",
		  smb_fname_str_dbg(smb_fname)));

	if (smb_fname->st.st_ex_flags & UF_DOS_ARCHIVE)
		*dosmode |= FILE_ATTRIBUTE_ARCHIVE;
	if (smb_fname->st.st_ex_flags & UF_DOS_HIDDEN)
		*dosmode |= FILE_ATTRIBUTE_HIDDEN;
	if (smb_fname->st.st_ex_flags & UF_DOS_RO)
		*dosmode |= FILE_ATTRIBUTE_READONLY;
	if (smb_fname->st.st_ex_flags & UF_DOS_SYSTEM)
		*dosmode |= FILE_ATTRIBUTE_SYSTEM;
	if (smb_fname->st.st_ex_flags & UF_DOS_NOINDEX)
		*dosmode |= FILE_ATTRIBUTE_NONINDEXED;
	if (smb_fname->st.st_ex_flags & FILE_ATTRIBUTE_SPARSE)
		*dosmode |= FILE_ATTRIBUTE_SPARSE;
	if (S_ISDIR(smb_fname->st.st_ex_mode))
		*dosmode |= FILE_ATTRIBUTE_DIRECTORY;

	*dosmode |= set_link_read_only_flag(&smb_fname->st);

	return true;
}

/****************************************************************************
 Sets DOS attributes, stored in st_ex_flags of the inode.
****************************************************************************/

static bool set_stat_dos_flags(connection_struct *conn,
			       const struct smb_filename *smb_fname,
			       uint32_t dosmode,
			       bool *attributes_changed)
{
	uint32_t new_flags = 0;
	int error = 0;

	SMB_ASSERT(VALID_STAT(smb_fname->st));
	SMB_ASSERT(attributes_changed);

	*attributes_changed = false;

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return false;
	}

	DEBUG(5, ("Setting stat dos attributes for %s.\n",
		  smb_fname_str_dbg(smb_fname)));

	new_flags = (smb_fname->st.st_ex_flags & ~UF_DOS_FLAGS) |
		     dos_attributes_to_stat_dos_flags(dosmode);

	/* Return early if no flags changed. */
	if (new_flags == smb_fname->st.st_ex_flags)
		return true;

	DEBUG(5, ("Setting stat dos attributes=0x%x, prev=0x%x\n", new_flags,
		  smb_fname->st.st_ex_flags));

	/* Set new flags with chflags. */
	error = SMB_VFS_CHFLAGS(conn, smb_fname->base_name, new_flags);
	if (error) {
		DEBUG(0, ("Failed setting new stat dos attributes (0x%x) on "
			  "file %s! errno=%d\n", new_flags,
			  smb_fname_str_dbg(smb_fname), errno));
		return false;
	}

	*attributes_changed = true;
	return true;
}
#endif /* HAVE_STAT_DOS_FLAGS */

/****************************************************************************
 Change a unix mode to a dos mode.
 May also read the create timespec into the stat struct in smb_fname
 if "store dos attributes" is true.
****************************************************************************/

uint32 dos_mode(connection_struct *conn, struct smb_filename *smb_fname)
{
	uint32 result = 0;
	bool offline, used_stat_dos_flags = false;

	DEBUG(8,("dos_mode: %s\n", smb_fname_str_dbg(smb_fname)));

	if (!VALID_STAT(smb_fname->st)) {
		return 0;
	}

	/* First do any modifications that depend on the path name. */
	/* hide files with a name starting with a . */
	if (lp_hide_dot_files(SNUM(conn))) {
		const char *p = strrchr_m(smb_fname->base_name,'/');
		if (p) {
			p++;
		} else {
			p = smb_fname->base_name;
		}

		/* Only . and .. are not hidden. */
		if (p[0] == '.' && !((p[1] == '\0') ||
				(p[1] == '.' && p[2] == '\0'))) {
			result |= FILE_ATTRIBUTE_HIDDEN;
		}
	}

#ifdef HAVE_STAT_DOS_FLAGS
	used_stat_dos_flags = get_stat_dos_flags(conn, smb_fname, &result);
#endif
	if (!used_stat_dos_flags) {
		/* Get the DOS attributes from an EA by preference. */
		if (!get_ea_dos_attribute(conn, smb_fname, &result)) {
			result |= dos_mode_from_sbuf(conn, smb_fname);
		}
	}

	offline = SMB_VFS_IS_OFFLINE(conn, smb_fname, &smb_fname->st);
	if (S_ISREG(smb_fname->st.st_ex_mode) && offline) {
		result |= FILE_ATTRIBUTE_OFFLINE;
	}

	/* Optimization : Only call is_hidden_path if it's not already
	   hidden. */
	if (!(result & FILE_ATTRIBUTE_HIDDEN) &&
	    IS_HIDDEN_PATH(conn, smb_fname->base_name)) {
		result |= FILE_ATTRIBUTE_HIDDEN;
	}

	if (result == 0) {
		result = FILE_ATTRIBUTE_NORMAL;
	}

	result = filter_mode_by_protocol(result);

	DEBUG(8,("dos_mode returning "));

	if (result & FILE_ATTRIBUTE_HIDDEN) DEBUG(8, ("h"));
	if (result & FILE_ATTRIBUTE_READONLY ) DEBUG(8, ("r"));
	if (result & FILE_ATTRIBUTE_SYSTEM) DEBUG(8, ("s"));
	if (result & FILE_ATTRIBUTE_DIRECTORY   ) DEBUG(8, ("d"));
	if (result & FILE_ATTRIBUTE_ARCHIVE  ) DEBUG(8, ("a"));
	if (result & FILE_ATTRIBUTE_SPARSE ) DEBUG(8, ("[sparse]"));

	DEBUG(8,("\n"));

	return(result);
}

/*******************************************************************
 chmod a file - but preserve some bits.
 If "store dos attributes" is also set it will store the create time
 from the stat struct in smb_fname (in NTTIME format) in the EA
 attribute also.
********************************************************************/

int file_set_dosmode(connection_struct *conn, struct smb_filename *smb_fname,
		     uint32 dosmode, const char *parent_dir, bool newfile)
{
	int mask=0;
	mode_t tmp;
	mode_t unixmode;
	int ret = -1, lret = -1;
	uint32_t old_mode;
	struct timespec new_create_timespec;

	/* We only allow READONLY|HIDDEN|SYSTEM|DIRECTORY|ARCHIVE here. */
	dosmode &= (SAMBA_ATTRIBUTES_MASK | FILE_ATTRIBUTE_OFFLINE);

	DEBUG(10,("file_set_dosmode: setting dos mode 0x%x on file %s\n",
		  dosmode, smb_fname_str_dbg(smb_fname)));

	unixmode = smb_fname->st.st_ex_mode;

	get_acl_group_bits(conn, smb_fname->base_name,
			   &smb_fname->st.st_ex_mode);

	if (S_ISDIR(smb_fname->st.st_ex_mode))
		dosmode |= FILE_ATTRIBUTE_DIRECTORY;
	else
		dosmode &= ~FILE_ATTRIBUTE_DIRECTORY;

	new_create_timespec = smb_fname->st.st_ex_btime;

	old_mode = dos_mode(conn, smb_fname);

	if (dosmode & FILE_ATTRIBUTE_OFFLINE) {
		if (!(old_mode & FILE_ATTRIBUTE_OFFLINE)) {
			lret = SMB_VFS_SET_OFFLINE(conn, smb_fname);
			if (lret == -1) {
				DEBUG(0, ("set_dos_mode: client has asked to "
					  "set FILE_ATTRIBUTE_OFFLINE to "
					  "%s/%s but there was an error while "
					  "setting it or it is not "
					  "supported.\n", parent_dir,
					  smb_fname_str_dbg(smb_fname)));
			}
		}
	}

	dosmode  &= ~FILE_ATTRIBUTE_OFFLINE;
	old_mode &= ~FILE_ATTRIBUTE_OFFLINE;

	smb_fname->st.st_ex_btime = new_create_timespec;

#ifdef HAVE_STAT_DOS_FLAGS
	{
		bool attributes_changed;

		if (set_stat_dos_flags(conn, smb_fname, dosmode,
				       &attributes_changed))
		{
			if (!newfile && attributes_changed) {
				notify_fname(conn, NOTIFY_ACTION_MODIFIED,
				    FILE_NOTIFY_CHANGE_ATTRIBUTES,
				    smb_fname->base_name);
			}
			smb_fname->st.st_ex_mode = unixmode;
			return 0;
		}
	}
#endif
	/* Store the DOS attributes in an EA by preference. */
	if (set_ea_dos_attribute(conn, smb_fname, dosmode)) {
		if (!newfile) {
			notify_fname(conn, NOTIFY_ACTION_MODIFIED,
				     FILE_NOTIFY_CHANGE_ATTRIBUTES,
				     smb_fname->base_name);
		}
		smb_fname->st.st_ex_mode = unixmode;
		return 0;
	}

	unixmode = unix_mode(conn, dosmode, smb_fname, parent_dir);

	/* preserve the s bits */
	mask |= (S_ISUID | S_ISGID);

	/* preserve the t bit */
#ifdef S_ISVTX
	mask |= S_ISVTX;
#endif

	/* possibly preserve the x bits */
	if (!MAP_ARCHIVE(conn))
		mask |= S_IXUSR;
	if (!MAP_SYSTEM(conn))
		mask |= S_IXGRP;
	if (!MAP_HIDDEN(conn))
		mask |= S_IXOTH;

	unixmode |= (smb_fname->st.st_ex_mode & mask);

	/* if we previously had any r bits set then leave them alone */
	if ((tmp = smb_fname->st.st_ex_mode & (S_IRUSR|S_IRGRP|S_IROTH))) {
		unixmode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
		unixmode |= tmp;
	}

	/* if we previously had any w bits set then leave them alone 
		whilst adding in the new w bits, if the new mode is not rdonly */
	if (!IS_DOS_READONLY(dosmode)) {
		unixmode |= (smb_fname->st.st_ex_mode & (S_IWUSR|S_IWGRP|S_IWOTH));
	}

	/*
	 * From the chmod 2 man page:
	 *
	 * "If the calling process is not privileged, and the group of the file
	 * does not match the effective group ID of the process or one of its
	 * supplementary group IDs, the S_ISGID bit will be turned off, but
	 * this will not cause an error to be returned."
	 *
	 * Simply refuse to do the chmod in this case.
	 */

	if (S_ISDIR(smb_fname->st.st_ex_mode) && (unixmode & S_ISGID) &&
			geteuid() != sec_initial_uid() &&
			!current_user_in_group(conn, smb_fname->st.st_ex_gid)) {
		DEBUG(3,("file_set_dosmode: setgid bit cannot be "
			"set for directory %s\n",
			smb_fname_str_dbg(smb_fname)));
		errno = EPERM;
		return -1;
	}

	ret = SMB_VFS_CHMOD(conn, smb_fname->base_name, unixmode);
	if (ret == 0) {
		if(!newfile || (lret != -1)) {
			notify_fname(conn, NOTIFY_ACTION_MODIFIED,
				     FILE_NOTIFY_CHANGE_ATTRIBUTES,
				     smb_fname->base_name);
		}
		smb_fname->st.st_ex_mode = unixmode;
		return 0;
	}

	if((errno != EPERM) && (errno != EACCES))
		return -1;

	if(!lp_dos_filemode(SNUM(conn)))
		return -1;

	/* We want DOS semantics, ie allow non owner with write permission to change the
		bits on a file. Just like file_ntimes below.
	*/

	/* Check if we have write access. */
	if (CAN_WRITE(conn)) {
		/*
		 * We need to open the file with write access whilst
		 * still in our current user context. This ensures we
		 * are not violating security in doing the fchmod.
		 */
		files_struct *fsp;
		if (!NT_STATUS_IS_OK(open_file_fchmod(conn, smb_fname,
				     &fsp)))
			return -1;
		become_root();
		ret = SMB_VFS_FCHMOD(fsp, unixmode);
		unbecome_root();
		close_file(NULL, fsp, NORMAL_CLOSE);
		if (!newfile) {
			notify_fname(conn, NOTIFY_ACTION_MODIFIED,
				     FILE_NOTIFY_CHANGE_ATTRIBUTES,
				     smb_fname->base_name);
		}
		if (ret == 0) {
			smb_fname->st.st_ex_mode = unixmode;
		}
	}

	return( ret );
}


NTSTATUS file_set_sparse(connection_struct *conn,
			 files_struct *fsp,
			 bool sparse)
{
	uint32_t old_dosmode;
	uint32_t new_dosmode;
	NTSTATUS status;

	if (!CAN_WRITE(conn)) {
		DEBUG(9,("file_set_sparse: fname[%s] set[%u] "
			"on readonly share[%s]\n",
			smb_fname_str_dbg(fsp->fsp_name),
			sparse,
			lp_servicename(SNUM(conn))));
		return NT_STATUS_MEDIA_WRITE_PROTECTED;
	}

	if (!(fsp->access_mask & FILE_WRITE_DATA) &&
			!(fsp->access_mask & FILE_WRITE_ATTRIBUTES)) {
		DEBUG(9,("file_set_sparse: fname[%s] set[%u] "
			"access_mask[0x%08X] - access denied\n",
			smb_fname_str_dbg(fsp->fsp_name),
			sparse,
			fsp->access_mask));
		return NT_STATUS_ACCESS_DENIED;
	}

	DEBUG(10,("file_set_sparse: setting sparse bit %u on file %s\n",
		  sparse, smb_fname_str_dbg(fsp->fsp_name)));

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return NT_STATUS_INVALID_DEVICE_REQUEST;
	}

	status = vfs_stat_fsp(fsp);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	old_dosmode = dos_mode(conn, fsp->fsp_name);

	if (sparse && !(old_dosmode & FILE_ATTRIBUTE_SPARSE)) {
		new_dosmode = old_dosmode | FILE_ATTRIBUTE_SPARSE;
	} else if (!sparse && (old_dosmode & FILE_ATTRIBUTE_SPARSE)) {
		new_dosmode = old_dosmode & ~FILE_ATTRIBUTE_SPARSE;
	} else {
		return NT_STATUS_OK;
	}

	/* Store the DOS attributes in an EA. */
	if (!set_ea_dos_attribute(conn, fsp->fsp_name,
				  new_dosmode)) {
		if (errno == 0) {
			errno = EIO;
		}
		return map_nt_error_from_unix(errno);
	}

	notify_fname(conn, NOTIFY_ACTION_MODIFIED,
		     FILE_NOTIFY_CHANGE_ATTRIBUTES,
		     fsp->fsp_name->base_name);

	fsp->is_sparse = sparse;

	return NT_STATUS_OK;
}

/*******************************************************************
 Wrapper around the VFS ntimes that possibly allows DOS semantics rather
 than POSIX.
*******************************************************************/

int file_ntimes(connection_struct *conn, const struct smb_filename *smb_fname,
		struct smb_file_time *ft)
{
	int ret = -1;

	errno = 0;

	DEBUG(6, ("file_ntime: actime: %s",
		  time_to_asc(convert_timespec_to_time_t(ft->atime))));
	DEBUG(6, ("file_ntime: modtime: %s",
		  time_to_asc(convert_timespec_to_time_t(ft->mtime))));
	DEBUG(6, ("file_ntime: ctime: %s",
		  time_to_asc(convert_timespec_to_time_t(ft->ctime))));
	DEBUG(6, ("file_ntime: createtime: %s",
		  time_to_asc(convert_timespec_to_time_t(ft->create_time))));

	/* Don't update the time on read-only shares */
	/* We need this as set_filetime (which can be called on
	   close and other paths) can end up calling this function
	   without the NEED_WRITE protection. Found by : 
	   Leo Weppelman <leo@wau.mis.ah.nl>
	*/

	if (!CAN_WRITE(conn)) {
		return 0;
	}

	if(SMB_VFS_NTIMES(conn, smb_fname, ft) == 0) {
		return 0;
	}

	if((errno != EPERM) && (errno != EACCES)) {
		return -1;
	}

	if(!lp_dos_filetimes(SNUM(conn))) {
		return -1;
	}

	/* We have permission (given by the Samba admin) to
	   break POSIX semantics and allow a user to change
	   the time on a file they don't own but can write to
	   (as DOS does).
	 */

	/* Check if we have write access. */
	if (can_write_to_file(conn, smb_fname)) {
		/* We are allowed to become root and change the filetime. */
		become_root();
		ret = SMB_VFS_NTIMES(conn, smb_fname, ft);
		unbecome_root();
	}

	return ret;
}

/******************************************************************
 Force a "sticky" write time on a pathname. This will always be
 returned on all future write time queries and set on close.
******************************************************************/

bool set_sticky_write_time_path(struct file_id fileid, struct timespec mtime)
{
	if (null_timespec(mtime)) {
		return true;
	}

	if (!set_sticky_write_time(fileid, mtime)) {
		return false;
	}

	return true;
}

/******************************************************************
 Force a "sticky" write time on an fsp. This will always be
 returned on all future write time queries and set on close.
******************************************************************/

bool set_sticky_write_time_fsp(struct files_struct *fsp, struct timespec mtime)
{
	if (null_timespec(mtime)) {
		return true;
	}

	fsp->write_time_forced = true;
	TALLOC_FREE(fsp->update_write_time_event);

	return set_sticky_write_time_path(fsp->file_id, mtime);
}

/******************************************************************
 Set a create time EA.
******************************************************************/

NTSTATUS set_create_timespec_ea(connection_struct *conn,
				const struct smb_filename *psmb_fname,
				struct timespec create_time)
{
	NTSTATUS status;
	struct smb_filename *smb_fname = NULL;
	uint32_t dosmode;
	int ret;

	if (!lp_store_dos_attributes(SNUM(conn))) {
		return NT_STATUS_OK;
	}

	status = create_synthetic_smb_fname(talloc_tos(),
				psmb_fname->base_name,
				NULL, &psmb_fname->st,
				&smb_fname);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	dosmode = dos_mode(conn, smb_fname);

	smb_fname->st.st_ex_btime = create_time;

	ret = file_set_dosmode(conn, smb_fname, dosmode, NULL, false);
	if (ret == -1) {
		map_nt_error_from_unix(errno);
	}

	DEBUG(10,("set_create_timespec_ea: wrote create time EA for file %s\n",
		smb_fname_str_dbg(smb_fname)));

	return NT_STATUS_OK;
}

/******************************************************************
 Return a create time.
******************************************************************/

struct timespec get_create_timespec(connection_struct *conn,
				struct files_struct *fsp,
				const struct smb_filename *smb_fname)
{
	return smb_fname->st.st_ex_btime;
}

/******************************************************************
 Return a change time (may look at EA in future).
******************************************************************/

struct timespec get_change_timespec(connection_struct *conn,
				struct files_struct *fsp,
				const struct smb_filename *smb_fname)
{
	return smb_fname->st.st_ex_mtime;
}
