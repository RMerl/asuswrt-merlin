/*
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB NT transaction handling
   Copyright (C) Jeremy Allison 1994-1998

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

#include "includes.h"
#include "nterr.h"

extern int DEBUGLEVEL;
extern int Protocol;
extern int Client;  
extern int smb_read_error;
extern int global_oplock_break;
extern BOOL case_sensitive;
extern BOOL case_preserve;
extern BOOL short_case_preserve;

static void remove_pending_change_notify_requests_by_mid(int mid);

static char *known_nt_pipes[] = {
  "\\LANMAN",
  "\\srvsvc",
  "\\samr",
  "\\wkssvc",
  "\\NETLOGON",
  "\\ntlsa",
  "\\ntsvcs",
  "\\lsass",
  "\\lsarpc",
  "\\winreg",
  NULL
};

/* Combinations of standard masks. */
#define STANDARD_RIGHTS_ALL_ACCESS (DELETE_ACCESS|READ_CONTROL_ACCESS|WRITE_DAC_ACCESS|WRITE_OWNER_ACCESS|SYNCHRONIZE_ACCESS) /* 0x001f0000 */
#define STANDARD_RIGHTS_EXECUTE_ACCESS (READ_CONTROL_ACCESS) /* 0x00020000 */
#define STANDARD_RIGHTS_READ_ACCESS (READ_CONTROL_ACCESS) /* 0x00200000 */
#define STANDARD_RIGHTS_REQUIRED_ACCESS (DELETE_ACCESS|READ_CONTROL_ACCESS|WRITE_DAC_ACCESS|WRITE_OWNER_ACCESS) /* 0x000f0000 */
#define STANDARD_RIGHTS_WRITE_ACCESS (READ_CONTROL_ACCESS) /* 0x00020000 */

/* Mapping of generic access rights for files to specific rights. */

#define FILE_GENERIC_ALL (STANDARD_RIGHTS_REQUIRED_ACCESS| SYNCHRONIZE_ACCESS|FILE_ALL_ATTRIBUTES)

#define FILE_GENERIC_READ (STANDARD_RIGHTS_READ_ACCESS|FILE_READ_DATA|FILE_READ_ATTRIBUTES|\
							FILE_READ_EA|SYNCHRONIZE_ACCESS)

#define FILE_GENERIC_WRITE (STANDARD_RIGHTS_WRITE_ACCESS|FILE_WRITE_DATA|FILE_WRITE_ATTRIBUTES|\
							FILE_WRITE_EA|FILE_APPEND_DATA|SYNCHRONIZE_ACCESS)

#define FILE_GENERIC_EXECUTE (STANDARD_RIGHTS_EXECUTE_ACCESS|FILE_READ_ATTRIBUTES|\
								FILE_EXECUTE|SYNCHRONIZE_ACCESS)

/* A type to describe the mapping of generic access rights to object
   specific access rights. */

typedef struct generic_mapping {
	uint32 generic_read;
	uint32 generic_write;
	uint32 generic_execute;
	uint32 generic_all;
} GENERIC_MAPPING;

/* Map generic permissions to file object specific permissions */
 
struct generic_mapping file_generic_mapping = {
    FILE_GENERIC_READ,
    FILE_GENERIC_WRITE,
    FILE_GENERIC_EXECUTE,
    FILE_GENERIC_ALL
};

/* Map generic access rights to object specific rights.  This technique is
   used to give meaning to assigning read, write, execute and all access to
   objects.  Each type of object has its own mapping of generic to object
   specific access rights. */

void se_map_generic(uint32 *access_mask, struct generic_mapping *mapping)
{
	uint32 old_mask = *access_mask;

	if (*access_mask & GENERIC_READ_ACCESS) {
		*access_mask &= ~GENERIC_READ_ACCESS;
		*access_mask |= mapping->generic_read;
	}

	if (*access_mask & GENERIC_WRITE_ACCESS) {
		*access_mask &= ~GENERIC_WRITE_ACCESS;
		*access_mask |= mapping->generic_write;
	}

	if (*access_mask & GENERIC_EXECUTE_ACCESS) {
		*access_mask &= ~GENERIC_EXECUTE_ACCESS;
		*access_mask |= mapping->generic_execute;
	}

	if (*access_mask & GENERIC_ALL_ACCESS) {
		*access_mask &= ~GENERIC_ALL_ACCESS;
		*access_mask |= mapping->generic_all;
	}

	if (old_mask != *access_mask) {
		DEBUG(10, ("se_map_generic(): mapped mask 0x%08x to 0x%08x\n",
			   old_mask, *access_mask));
	}
}

/****************************************************************************
 Send the required number of replies back.
 We assume all fields other than the data fields are
 set correctly for the type of call.
 HACK ! Always assumes smb_setup field is zero.
****************************************************************************/

static int send_nt_replies(char *inbuf, char *outbuf, int bufsize, uint32 nt_error, char *params,
                           int paramsize, char *pdata, int datasize)
{
  extern int max_send;
  int data_to_send = datasize;
  int params_to_send = paramsize;
  int useable_space;
  char *pp = params;
  char *pd = pdata;
  int params_sent_thistime, data_sent_thistime, total_sent_thistime;
  int alignment_offset = 3;
  int data_alignment_offset = 0;

  /*
   * Initially set the wcnt area to be 18 - this is true for all
   * transNT replies.
   */

  set_message(outbuf,18,0,True);

  if(nt_error != 0) {
    /* NT Error. */
    SSVAL(outbuf,smb_flg2, SVAL(outbuf,smb_flg2) | FLAGS2_32_BIT_ERROR_CODES);

    ERROR(0,nt_error);
  }

  /* 
   * If there genuinely are no parameters or data to send just send
   * the empty packet.
   */

  if(params_to_send == 0 && data_to_send == 0) {
    send_smb(Client,outbuf);
    return 0;
  }

  /*
   * When sending params and data ensure that both are nicely aligned.
   * Only do this alignment when there is also data to send - else
   * can cause NT redirector problems.
   */

  if (((params_to_send % 4) != 0) && (data_to_send != 0))
    data_alignment_offset = 4 - (params_to_send % 4);

  /* 
   * Space is bufsize minus Netbios over TCP header minus SMB header.
   * The alignment_offset is to align the param bytes on a four byte
   * boundary (2 bytes for data len, one byte pad). 
   * NT needs this to work correctly.
   */

  useable_space = bufsize - ((smb_buf(outbuf)+
                    alignment_offset+data_alignment_offset) -
                    outbuf);

  /*
   * useable_space can never be more than max_send minus the
   * alignment offset.
   */

  useable_space = MIN(useable_space,
                      max_send - (alignment_offset+data_alignment_offset));


  while (params_to_send || data_to_send) {

    /*
     * Calculate whether we will totally or partially fill this packet.
     */

    total_sent_thistime = params_to_send + data_to_send +
                            alignment_offset + data_alignment_offset;

    /* 
     * We can never send more than useable_space.
     */

    total_sent_thistime = MIN(total_sent_thistime, useable_space);

    set_message(outbuf, 18, total_sent_thistime, True);

    /*
     * Set total params and data to be sent.
     */

    SIVAL(outbuf,smb_ntr_TotalParameterCount,paramsize);
    SIVAL(outbuf,smb_ntr_TotalDataCount,datasize);

    /* 
     * Calculate how many parameters and data we can fit into
     * this packet. Parameters get precedence.
     */

    params_sent_thistime = MIN(params_to_send,useable_space);
    data_sent_thistime = useable_space - params_sent_thistime;
    data_sent_thistime = MIN(data_sent_thistime,data_to_send);

    SIVAL(outbuf,smb_ntr_ParameterCount,params_sent_thistime);

    if(params_sent_thistime == 0) {
      SIVAL(outbuf,smb_ntr_ParameterOffset,0);
      SIVAL(outbuf,smb_ntr_ParameterDisplacement,0);
    } else {
      /*
       * smb_ntr_ParameterOffset is the offset from the start of the SMB header to the
       * parameter bytes, however the first 4 bytes of outbuf are
       * the Netbios over TCP header. Thus use smb_base() to subtract
       * them from the calculation.
       */

      SIVAL(outbuf,smb_ntr_ParameterOffset,
            ((smb_buf(outbuf)+alignment_offset) - smb_base(outbuf)));
      /* 
       * Absolute displacement of param bytes sent in this packet.
       */

      SIVAL(outbuf,smb_ntr_ParameterDisplacement,pp - params);
    }

    /*
     * Deal with the data portion.
     */

    SIVAL(outbuf,smb_ntr_DataCount, data_sent_thistime);

    if(data_sent_thistime == 0) {
      SIVAL(outbuf,smb_ntr_DataOffset,0);
      SIVAL(outbuf,smb_ntr_DataDisplacement, 0);
    } else {
      /*
       * The offset of the data bytes is the offset of the
       * parameter bytes plus the number of parameters being sent this time.
       */

      SIVAL(outbuf,smb_ntr_DataOffset,((smb_buf(outbuf)+alignment_offset) -
            smb_base(outbuf)) + params_sent_thistime + data_alignment_offset);
      SIVAL(outbuf,smb_ntr_DataDisplacement, pd - pdata);
    }

    /* 
     * Copy the param bytes into the packet.
     */

    if(params_sent_thistime)
      memcpy((smb_buf(outbuf)+alignment_offset),pp,params_sent_thistime);

    /*
     * Copy in the data bytes
     */

    if(data_sent_thistime)
      memcpy(smb_buf(outbuf)+alignment_offset+params_sent_thistime+
             data_alignment_offset,pd,data_sent_thistime);
    
    DEBUG(9,("nt_rep: params_sent_thistime = %d, data_sent_thistime = %d, useable_space = %d\n",
          params_sent_thistime, data_sent_thistime, useable_space));
    DEBUG(9,("nt_rep: params_to_send = %d, data_to_send = %d, paramsize = %d, datasize = %d\n",
          params_to_send, data_to_send, paramsize, datasize));
    
    /* Send the packet */
    send_smb(Client,outbuf);
    
    pp += params_sent_thistime;
    pd += data_sent_thistime;
    
    params_to_send -= params_sent_thistime;
    data_to_send -= data_sent_thistime;

    /*
     * Sanity check
     */

    if(params_to_send < 0 || data_to_send < 0) {
      DEBUG(0,("send_nt_replies failed sanity check pts = %d, dts = %d\n!!!",
            params_to_send, data_to_send));
      return -1;
    }
  } 

  return 0;
}

/****************************************************************************
 (Hopefully) temporary call to fix bugs in NT5.0beta2. This OS sends unicode
 strings in NT calls AND DOESN'T SET THE UNICODE BIT !!!!!!!
****************************************************************************/

static void get_filename( char *fname, char *inbuf, int data_offset, int data_len, int fname_len)
{
  /*
   * We need various heuristics here to detect a unicode string... JRA.
   */

  DEBUG(10,("get_filename: data_offset = %d, data_len = %d, fname_len = %d\n",
           data_offset, data_len, fname_len ));

  if(data_len - fname_len > 1) {
    /*
     * NT 5.0 Beta 2 has kindly sent us a UNICODE string
     * without bothering to set the unicode bit. How kind.
     *
     * Firstly - ensure that the data offset is aligned
     * on a 2 byte boundary - add one if not.
     */
    fname_len = fname_len/2;
    if(data_offset & 1)
      data_offset++;
    pstrcpy(fname, dos_unistrn2((uint16 *)(inbuf+data_offset), fname_len));
  } else {
    StrnCpy(fname,inbuf+data_offset,fname_len);
    fname[fname_len] = '\0';
  }
}

/****************************************************************************
 Fix bugs in Win2000 final release. In trans calls this OS sends unicode
 strings AND DOESN'T SET THE UNICODE BIT !!!!!!!
****************************************************************************/

static void get_filename_transact( char *fname, char *inbuf, int data_offset, int data_len, int fname_len)
{
  /*
   * We need various heuristics here to detect a unicode string... JRA.
   */

  DEBUG(10,("get_filename_transact: data_offset = %d, data_len = %d, fname_len = %d\n",
           data_offset, data_len, fname_len ));

  /*
   * Win2K sends a unicode filename plus one extra alingment byte.
   * WinNT4.x send an ascii string with multiple garbage bytes on
   * the end here.
   */

  if((data_len - fname_len == 1) || (inbuf[data_offset] == '\0')) {
    /*
     * Ensure that the data offset is aligned
     * on a 2 byte boundary - add one if not.
     */
    fname_len = fname_len/2;
    if(data_offset & 1)
      data_offset++;
    pstrcpy(fname, dos_unistrn2((uint16 *)(inbuf+data_offset), fname_len));
  } else {
    StrnCpy(fname,inbuf+data_offset,fname_len);
    fname[fname_len] = '\0';
  }
}

/****************************************************************************
 Save case statics.
****************************************************************************/

static BOOL saved_case_sensitive;
static BOOL saved_case_preserve;
static BOOL saved_short_case_preserve;

/****************************************************************************
 Save case semantics.
****************************************************************************/

static void set_posix_case_semantics(uint32 file_attributes)
{
  if(!(file_attributes & FILE_FLAG_POSIX_SEMANTICS))
    return;

  saved_case_sensitive = case_sensitive;
  saved_case_preserve = case_preserve;
  saved_short_case_preserve = short_case_preserve;

  /* Set to POSIX. */
  case_sensitive = True;
  case_preserve = True;
  short_case_preserve = True;
}

/****************************************************************************
 Restore case semantics.
****************************************************************************/

static void restore_case_semantics(uint32 file_attributes)
{
  if(!(file_attributes & FILE_FLAG_POSIX_SEMANTICS))
    return;

  case_sensitive = saved_case_sensitive;
  case_preserve = saved_case_preserve;
  short_case_preserve = saved_short_case_preserve;
}

/****************************************************************************
 Utility function to map create disposition.
****************************************************************************/

static int map_create_disposition( uint32 create_disposition)
{
  int ret;

  switch( create_disposition ) {
  case FILE_CREATE:
    /* create if not exist, fail if exist */
    ret = (FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_FAIL);
    break;
  case FILE_SUPERSEDE:
  case FILE_OVERWRITE_IF:
    /* create if not exist, trunc if exist */
    ret = (FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_TRUNCATE);
    break;
  case FILE_OPEN:
    /* fail if not exist, open if exists */
    ret = (FILE_FAIL_IF_NOT_EXIST|FILE_EXISTS_OPEN);
    break;
  case FILE_OPEN_IF:
    /* create if not exist, open if exists */
    ret = (FILE_CREATE_IF_NOT_EXIST|FILE_EXISTS_OPEN);
    break;
  case FILE_OVERWRITE:
    /* fail if not exist, truncate if exists */
    ret = (FILE_FAIL_IF_NOT_EXIST|FILE_EXISTS_TRUNCATE);
    break;
  default:
    DEBUG(0,("map_create_disposition: Incorrect value for create_disposition = %d\n",
             create_disposition ));
    return -1;
  }

  DEBUG(10,("map_create_disposition: Mapped create_disposition %lx to %x\n",
        (unsigned long)create_disposition, ret ));

  return ret;
}

/****************************************************************************
 Utility function to map share modes.
****************************************************************************/

static int map_share_mode( BOOL *pstat_open_only, char *fname, uint32 create_options,
							uint32 desired_access, uint32 share_access, uint32 file_attributes)
{
  int smb_open_mode = -1;

  DEBUG(6, ("map_share_mode(%s, create_options=0x%x, "
    "desired_access=0x%x, share_access=0x%x, "
    "file_attributes=0x%x\n",
    fname, create_options, desired_access,
    share_access, file_attributes));

  /*
   * Convert GENERIC bits to specific bits.
   */

  se_map_generic(&desired_access, &file_generic_mapping);

  *pstat_open_only = False;

  /* this is just so the next switch works sane */
  if (desired_access & FILE_EXECUTE)
    desired_access |= FILE_READ_DATA;

  switch( desired_access & (FILE_READ_DATA|FILE_WRITE_DATA|FILE_APPEND_DATA) ) {
  case FILE_READ_DATA:
    smb_open_mode = DOS_OPEN_RDONLY;
    break;
  case FILE_WRITE_DATA:
  case FILE_APPEND_DATA:
  case FILE_WRITE_DATA|FILE_APPEND_DATA:
    smb_open_mode = DOS_OPEN_WRONLY;
    break;
  case FILE_READ_DATA|FILE_WRITE_DATA:
  case FILE_READ_DATA|FILE_WRITE_DATA|FILE_APPEND_DATA:
  case FILE_READ_DATA|FILE_APPEND_DATA:
    smb_open_mode = DOS_OPEN_RDWR;
    break;
  }

  /*
   * NB. For DELETE_ACCESS we should really check the
   * directory permissions, as that is what controls
   * delete, and for WRITE_DAC_ACCESS we should really
   * check the ownership, as that is what controls the
   * chmod. Note that this is *NOT* a security hole (this
   * note is for you, Andrew) as we are not *allowing*
   * the access at this point, the actual unlink or
   * chown or chmod call would do this. We are just helping
   * clients out by telling them if they have a hope
   * of any of this succeeding. POSIX acls may still
   * deny the real call. JRA.
   */

  if (smb_open_mode == -1) {
	if(desired_access == WRITE_DAC_ACCESS || desired_access == READ_CONTROL_ACCESS)
		*pstat_open_only = True;

    if(desired_access & (DELETE_ACCESS|WRITE_DAC_ACCESS|WRITE_OWNER_ACCESS|SYNCHRONIZE_ACCESS|
                              FILE_EXECUTE|FILE_READ_ATTRIBUTES|
                              FILE_READ_EA|FILE_WRITE_EA|SYSTEM_SECURITY_ACCESS|
                              FILE_WRITE_ATTRIBUTES|READ_CONTROL_ACCESS)) {
      smb_open_mode = DOS_OPEN_RDONLY;
    } else if (desired_access == 0) {
      /*
       * JRA - NT seems to sometimes send desired_access as zero. play it safe
       * and map to a stat open.
       */
      smb_open_mode = DOS_OPEN_RDONLY;
    } else {
      DEBUG(0,("map_share_mode: Incorrect value 0x%lx for desired_access to file %s\n",
        (unsigned long)desired_access, fname));
      return -1;
    }
  }

  /*
   * Set the special bit that means allow share delete.
   * This is held outside the normal share mode bits at 1<<15.
   * JRA.
   */

  if(share_access & FILE_SHARE_DELETE) {
    smb_open_mode |= ALLOW_SHARE_DELETE;
    DEBUG(10,("map_share_mode: FILE_SHARE_DELETE requested. open_mode = 0x%x\n", smb_open_mode));
  }

  /*
   * We need to store the intent to open for Delete. This
   * is what determines if a delete on close flag can be set.
   * This is the wrong way (and place) to store this, but for 2.x this
   * is the only practical way. JRA.
   */

  if(desired_access & DELETE_ACCESS) {
    DEBUG(10,("map_share_mode: DELETE_ACCESS requested. open_mode = 0x%x\n", smb_open_mode));
  }

  if (create_options & FILE_DELETE_ON_CLOSE) {
    /* Implicit delete access is *NOT* requested... */
    smb_open_mode |= DELETE_ON_CLOSE_FLAG;
    DEBUG(10,("map_share_mode: FILE_DELETE_ON_CLOSE requested. open_mode = 0x%x\n", smb_open_mode));
  }

  /* Add in the requested share mode. */
  switch( share_access & (FILE_SHARE_READ|FILE_SHARE_WRITE)) {
  case FILE_SHARE_READ:
    smb_open_mode |= SET_DENY_MODE(DENY_WRITE);
    break;
  case FILE_SHARE_WRITE:
    smb_open_mode |= SET_DENY_MODE(DENY_READ);
    break;
  case (FILE_SHARE_READ|FILE_SHARE_WRITE):
    smb_open_mode |= SET_DENY_MODE(DENY_NONE);
    break;
  case FILE_SHARE_NONE:
    smb_open_mode |= SET_DENY_MODE(DENY_ALL);
    break;
  }

  /*
   * Handle an O_SYNC request.
   */

  if(file_attributes & FILE_FLAG_WRITE_THROUGH)
    smb_open_mode |= FILE_SYNC_OPENMODE;

  DEBUG(10,("map_share_mode: Mapped desired access 0x%lx, share access 0x%lx, file attributes 0x%lx \
to open_mode 0x%x\n", (unsigned long)desired_access, (unsigned long)share_access,
                    (unsigned long)file_attributes, smb_open_mode ));
 
  return smb_open_mode;
}

/*
 * This is a *disgusting* hack.
 * This is *so* bad that even I'm embarrassed (and I
 * have no shame). Here's the deal :
 * Until we get the correct SPOOLSS code into smbd
 * then when we're running with NT SMB support then
 * NT makes this call with a level of zero, and then
 * immediately follows it with an open request to
 * the \\SRVSVC pipe. If we allow that open to
 * succeed then NT barfs when it cannot open the
 * \\SPOOLSS pipe immediately after and continually
 * whines saying "Printer name is invalid" forever
 * after. If we cause *JUST THIS NEXT OPEN* of \\SRVSVC
 * to fail, then NT downgrades to using the downlevel code
 * and everything works as well as before. I hate
 * myself for adding this code.... JRA.
 *
 * The HACK_FAIL_TIME define allows only a 2
 * second window for this to occur, just in
 * case...
 */

static BOOL fail_next_srvsvc = False;
static time_t fail_time;
#define HACK_FAIL_TIME 2 /* In seconds. */

void fail_next_srvsvc_open(void)
{
  /* Check client is WinNT proper; Win2K doesn't like Jeremy's hack - matty */
  if (get_remote_arch() != RA_WINNT)
    return;

  fail_next_srvsvc = True;
  fail_time = time(NULL);
  DEBUG(10,("fail_next_srvsvc_open: setting up timeout close of \\srvsvc pipe for print fix.\n"));
}

/*
 * HACK alert.... see above - JRA.
 */

BOOL should_fail_next_srvsvc_open(const char *pipename)
{

  DEBUG(10,("should_fail_next_srvsvc_open: fail = %d, pipe = %s\n",
    (int)fail_next_srvsvc, pipename));

  if(fail_next_srvsvc && (time(NULL) > fail_time + HACK_FAIL_TIME)) {
    fail_next_srvsvc = False;
    fail_time = (time_t)0;
    DEBUG(10,("should_fail_next_srvsvc_open: End of timeout close of \\srvsvc pipe for print fix.\n"));
  }

  if(fail_next_srvsvc && strequal(pipename, "srvsvc")) {
    fail_next_srvsvc = False;
    DEBUG(10,("should_fail_next_srvsvc_open: Deliberately failing open of \\srvsvc pipe for print fix.\n"));
    return True;
  }
  return False;
}


/****************************************************************************
 Reply to an NT create and X call on a pipe.
****************************************************************************/
static int nt_open_pipe(char *fname, connection_struct *conn,
			char *inbuf, char *outbuf, int *ppnum)
{
	pipes_struct *p = NULL;

	uint16 vuid = SVAL(inbuf, smb_uid);
	int i;

	DEBUG(4,("nt_open_pipe: Opening pipe %s.\n", fname));
    
	/* See if it is one we want to handle. */
	for( i = 0; known_nt_pipes[i]; i++ )
		if( strequal(fname,known_nt_pipes[i]))
			break;
    
	if ( known_nt_pipes[i] == NULL )
		return(ERROR(ERRSRV,ERRaccess));
    
	/* Strip \\ off the name. */
	fname++;
    
	if(should_fail_next_srvsvc_open(fname))
		return (ERROR(ERRSRV,ERRaccess));

	DEBUG(3,("nt_open_pipe: Known pipe %s opening.\n", fname));

	p = open_rpc_pipe_p(fname, conn, vuid);
	if (!p)
		return(ERROR(ERRSRV,ERRnofids));

	*ppnum = p->pnum;

	return 0;
}

/****************************************************************************
 Reply to an NT create and X call.
****************************************************************************/

int reply_ntcreate_and_X(connection_struct *conn,
			 char *inbuf,char *outbuf,int length,int bufsize)
{  
	pstring fname;
	uint32 flags = IVAL(inbuf,smb_ntcreate_Flags);
	uint32 desired_access = IVAL(inbuf,smb_ntcreate_DesiredAccess);
	uint32 file_attributes = IVAL(inbuf,smb_ntcreate_FileAttributes);
	uint32 share_access = IVAL(inbuf,smb_ntcreate_ShareAccess);
	uint32 create_disposition = IVAL(inbuf,smb_ntcreate_CreateDisposition);
	uint32 create_options = IVAL(inbuf,smb_ntcreate_CreateOptions);
	uint32 fname_len = MIN(((uint32)SVAL(inbuf,smb_ntcreate_NameLength)),
			       ((uint32)sizeof(fname)-1));
	uint16 root_dir_fid = (uint16)IVAL(inbuf,smb_ntcreate_RootDirectoryFid);
	int smb_ofun;
	int smb_open_mode;
	int smb_attr = (file_attributes & SAMBA_ATTRIBUTES_MASK);
	/* Breakout the oplock request bits so we can set the
	   reply bits separately. */
	int oplock_request = 0;
	mode_t unixmode;
	int pnum = -1;
	int fmode=0,rmode=0;
	SMB_OFF_T file_len = 0;
	SMB_STRUCT_STAT sbuf;
	int smb_action = 0;
	BOOL bad_path = False;
	files_struct *fsp=NULL;
	char *p = NULL;
	BOOL stat_open_only = False;

	/* 
	 * We need to construct the open_and_X ofun value from the
	 * NT values, as that's what our code is structured to accept.
	 */    
	
	if((smb_ofun = map_create_disposition( create_disposition )) == -1)
		return(ERROR(ERRDOS,ERRbadaccess));

	/*
	 * Get the file name.
	 */

    if(root_dir_fid != 0) {
      /*
       * This filename is relative to a directory fid.
       */
      files_struct *dir_fsp = file_fsp(inbuf,smb_ntcreate_RootDirectoryFid);
      size_t dir_name_len;

      if(!dir_fsp)
        return(ERROR(ERRDOS,ERRbadfid));

      if(!dir_fsp->is_directory) {
        /* 
         * Check to see if this is a mac fork of some kind.
         */

        get_filename(&fname[0], inbuf, smb_buf(inbuf)-inbuf, 
                   smb_buflen(inbuf),fname_len);

        if( fname[0] == ':') {
          SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
          return(ERROR(0, 0xc0000000|NT_STATUS_OBJECT_PATH_NOT_FOUND));
        }
        return(ERROR(ERRDOS,ERRbadfid));
      }

      /*
       * Copy in the base directory name.
       */

      pstrcpy( fname, dir_fsp->fsp_name );
      dir_name_len = strlen(fname);

      /*
       * Ensure it ends in a '\'.
       */

      if(fname[dir_name_len-1] != '\\' && fname[dir_name_len-1] != '/') {
        pstrcat(fname, "\\");
        dir_name_len++;
      }

      /*
       * This next calculation can refuse a correct filename if we're dealing
       * with the Win2k unicode bug, but that would be rare. JRA.
       */

      if(fname_len + dir_name_len >= sizeof(pstring))
        return(ERROR(ERRSRV,ERRfilespecs));

      get_filename(&fname[dir_name_len], inbuf, smb_buf(inbuf)-inbuf, 
                   smb_buflen(inbuf),fname_len);

    } else {
      
      get_filename(fname, inbuf, smb_buf(inbuf)-inbuf, 
                   smb_buflen(inbuf),fname_len);
    }
	
	/* If it's an IPC, use the pipe handler. */

	if (IS_IPC(conn) && lp_nt_pipe_support()) {

		int ret = nt_open_pipe(fname, conn, inbuf, outbuf, &pnum);
		if(ret != 0)
			return ret;

		/*
		 * Deal with pipe return.
		 */  

		set_message(outbuf,34,0,True);
	
		p = outbuf + smb_vwv2;
		p++;
		SSVAL(p,0,pnum);
		p += 2;
		SIVAL(p,0,FILE_WAS_OPENED);
		p += 4;
		p += 32;
		SIVAL(p,0,FILE_ATTRIBUTE_NORMAL); /* File Attributes. */
		p += 20;
		/* File type. */
		SSVAL(p,0,FILE_TYPE_MESSAGE_MODE_PIPE);
		/* Device state. */
		SSVAL(p,2, 0x5FF); /* ? */

		DEBUG(5,("reply_ntcreate_and_X: open pipe = %s\n", fname));

		return chain_reply(inbuf,outbuf,length,bufsize);
	}

	/*
	 * Now contruct the smb_open_mode value from the filename, 
	 * desired access and the share access.
	 */
	
	if((smb_open_mode = map_share_mode(&stat_open_only, fname, create_options, desired_access, 
					   share_access, 
					   file_attributes)) == -1)
		return(ERROR(ERRDOS,ERRbadaccess));

	oplock_request = (flags & REQUEST_OPLOCK) ? EXCLUSIVE_OPLOCK : 0;
	oplock_request |= (flags & REQUEST_BATCH_OPLOCK) ? BATCH_OPLOCK : 0;

	/*
	 * Ordinary file or directory.
	 */
		
	/*
	 * Check if POSIX semantics are wanted.
	 */
		
	set_posix_case_semantics(file_attributes);
		
	unix_convert(fname,conn,0,&bad_path,NULL);
		
	fsp = file_new();
	if (!fsp) {
		restore_case_semantics(file_attributes);
		return(ERROR(ERRSRV,ERRnofids));
	}
		
	if (!check_name(fname,conn)) { 
		if((errno == ENOENT) && bad_path) {
			unix_ERR_class = ERRDOS;
			unix_ERR_code = ERRbadpath;
		}
		file_free(fsp);
		
		restore_case_semantics(file_attributes);
		
		return(UNIXERROR(ERRDOS,ERRnoaccess));
	} 
		
	unixmode = unix_mode(conn,smb_attr | aARCH, fname);
    
	/* 
	 * If it's a request for a directory open, deal with it separately.
	 */

	if(create_options & FILE_DIRECTORY_FILE) {
		oplock_request = 0;
		
		open_directory(fsp, conn, fname, smb_ofun, 
			       unixmode, &smb_action);
			
		restore_case_semantics(file_attributes);

		if(!fsp->open) {
			file_free(fsp);
			return(UNIXERROR(ERRDOS,ERRnoaccess));
		}
	} else {
		/*
		 * Ordinary file case.
		 */

		/* NB. We have a potential bug here. If we
		 * cause an oplock break to ourselves, then we
		 * could end up processing filename related
		 * SMB requests whilst we await the oplock
		 * break response. As we may have changed the
		 * filename case semantics to be POSIX-like,
		 * this could mean a filename request could
		 * fail when it should succeed. This is a rare
		 * condition, but eventually we must arrange
		 * to restore the correct case semantics
		 * before issuing an oplock break request to
		 * our client. JRA.  */

		open_file_shared(fsp,conn,fname,smb_open_mode,
				 smb_ofun,unixmode, oplock_request,&rmode,&smb_action);

		if (!fsp->open) { 
			/* We cheat here. There are two cases we
			 * care about. One is a directory rename,
			 * where the NT client will attempt to
			 * open the source directory for
			 * DELETE access. Note that when the
			 * NT client does this it does *not*
			 * set the directory bit in the
			 * request packet. This is translated
			 * into a read/write open
			 * request. POSIX states that any open
			 * for write request on a directory
			 * will generate an EISDIR error, so
			 * we can catch this here and open a
			 * pseudo handle that is flagged as a
			 * directory. The second is an open
			 * for a permissions read only, which
			 * we handle in the open_file_stat case. JRA.
			 */

			if(errno == EISDIR) {

				/*
				 * Fail the open if it was explicitly a non-directory file.
				 */

				if (create_options & FILE_NON_DIRECTORY_FILE) {
					file_free(fsp);
					restore_case_semantics(file_attributes);
					SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
					return(ERROR(0, 0xc0000000|NT_STATUS_FILE_IS_A_DIRECTORY));
				}
	
				oplock_request = 0;
				open_directory(fsp, conn, fname, smb_ofun, unixmode, &smb_action);
				
				if(!fsp->open) {
					file_free(fsp);
					restore_case_semantics(file_attributes);
					return(UNIXERROR(ERRDOS,ERRnoaccess));
				}
#ifdef EROFS
			} else if (((errno == EACCES) || (errno == EROFS)) && stat_open_only) {
#else /* !EROFS */
			} else if (errno == EACCES && stat_open_only) {
#endif
				/*
				 * We couldn't open normally and all we want
				 * are the permissions. Try and do a stat open.
				 */

				oplock_request = 0;

				open_file_stat(fsp,conn,fname,smb_open_mode,&sbuf,&smb_action);

				if(!fsp->open) {
					file_free(fsp);
					restore_case_semantics(file_attributes);
					return(UNIXERROR(ERRDOS,ERRnoaccess));
				}

			} else {

				if((errno == ENOENT) && bad_path) {
					unix_ERR_class = ERRDOS;
					unix_ERR_code = ERRbadpath;
				}
				
				file_free(fsp);
				
				restore_case_semantics(file_attributes);
				
				return(UNIXERROR(ERRDOS,ERRnoaccess));
			}
		} 
	}
		
	if(fsp->is_directory) {
		if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
			close_file(fsp,True);
			restore_case_semantics(file_attributes);
			return(ERROR(ERRDOS,ERRnoaccess));
		}
	} else {
		if (!fsp->stat_open && sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
			close_file(fsp,False);
			restore_case_semantics(file_attributes);
			return(ERROR(ERRDOS,ERRnoaccess));
		} 
	}
		
	restore_case_semantics(file_attributes);
		
	file_len = sbuf.st_size;
	fmode = dos_mode(conn,fname,&sbuf);
	if(fmode == 0)
		fmode = FILE_ATTRIBUTE_NORMAL;
	if (!fsp->is_directory && (fmode & aDIR)) {
		close_file(fsp,False);
		return(ERROR(ERRDOS,ERRnoaccess));
	} 
	
	/* 
	 * If the caller set the extended oplock request bit
	 * and we granted one (by whatever means) - set the
	 * correct bit for extended oplock reply.
	 */
	
	if (oplock_request && lp_fake_oplocks(SNUM(conn)))
		smb_action |= EXTENDED_OPLOCK_GRANTED;
	
	if(oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
		smb_action |= EXTENDED_OPLOCK_GRANTED;

	set_message(outbuf,34,0,True);
	
	p = outbuf + smb_vwv2;
	
	/*
	 * Currently as we don't support level II oplocks we just report
	 * exclusive & batch here.
	 */

    if (smb_action & EXTENDED_OPLOCK_GRANTED)	
	  	SCVAL(p,0, BATCH_OPLOCK_RETURN);
	else if (LEVEL_II_OPLOCK_TYPE(fsp->oplock_type))
        SCVAL(p,0, LEVEL_II_OPLOCK_RETURN);
	else
		SCVAL(p,0,NO_OPLOCK_RETURN);
	
	p++;
	SSVAL(p,0,fsp->fnum);
	p += 2;
	SIVAL(p,0,smb_action);
	p += 4;
	
	/* Create time. */  
	put_long_date(p,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
	p += 8;
	put_long_date(p,sbuf.st_atime); /* access time */
	p += 8;
	put_long_date(p,sbuf.st_mtime); /* write time */
	p += 8;
	put_long_date(p,sbuf.st_mtime); /* change time */
	p += 8;
	SIVAL(p,0,fmode); /* File Attributes. */
	p += 4;
	SOFF_T(p, 0, file_len);
	p += 8;
	SOFF_T(p,0,file_len);
	p += 12;
	SCVAL(p,0,fsp->is_directory ? 1 : 0);
	
	DEBUG(5,("reply_ntcreate_and_X: fnum = %d, open name = %s\n", fsp->fnum, fsp->fsp_name));

	return chain_reply(inbuf,outbuf,length,bufsize);
}

/****************************************************************************
 Reply to a NT_TRANSACT_CREATE call (needs to process SD's).
****************************************************************************/

static int call_nt_transact_create(connection_struct *conn,
					char *inbuf, char *outbuf, int length, 
					int bufsize, char **ppsetup, char **ppparams, 
					char **ppdata)
{
  pstring fname;
  char *params = *ppparams;
  int total_parameter_count = (int)IVAL(inbuf, smb_nt_TotalParameterCount);
  /* Breakout the oplock request bits so we can set the
     reply bits separately. */
  int oplock_request = 0;
  mode_t unixmode;
  int pnum = -1;
  int fmode=0,rmode=0;
  SMB_OFF_T file_len = 0;
  SMB_STRUCT_STAT sbuf;
  int smb_action = 0;
  BOOL bad_path = False;
  files_struct *fsp = NULL;
  char *p = NULL;
  BOOL stat_open_only = False;
  uint32 flags;
  uint32 desired_access;
  uint32 file_attributes;
  uint32 share_access;
  uint32 create_disposition;
  uint32 create_options;
  uint32 fname_len;
  uint16 root_dir_fid;
  int smb_ofun;
  int smb_open_mode;
  int smb_attr;

  DEBUG(5,("call_nt_transact_create\n"));

  /*
   * Ensure minimum number of parameters sent.
   */

  if(total_parameter_count < 54) {
    DEBUG(0,("call_nt_transact_create - insufficient parameters (%u)\n", (unsigned int)total_parameter_count));
    return(ERROR(ERRDOS,ERRbadaccess));
  }

  flags = IVAL(params,0);
  desired_access = IVAL(params,8);
  file_attributes = IVAL(params,20);
  share_access = IVAL(params,24);
  create_disposition = IVAL(params,28);
  create_options = IVAL(params,32);
  fname_len = MIN(((uint32)IVAL(params,44)),((uint32)sizeof(fname)-1));
  root_dir_fid = (uint16)IVAL(params,4);
  smb_attr = (file_attributes & SAMBA_ATTRIBUTES_MASK);

  /* 
   * We need to construct the open_and_X ofun value from the
   * NT values, as that's what our code is structured to accept.
   */    

  if((smb_ofun = map_create_disposition( create_disposition )) == -1)
    return(ERROR(ERRDOS,ERRbadmem));

  /*
   * Get the file name.
   */

  if(root_dir_fid != 0) {
    /*
     * This filename is relative to a directory fid.
     */

    files_struct *dir_fsp = file_fsp(params,4);
    size_t dir_name_len;

    if(!dir_fsp)
        return(ERROR(ERRDOS,ERRbadfid));

    if(!dir_fsp->is_directory) {
      /*
       * Check to see if this is a mac fork of some kind.
       */

      get_filename_transact(&fname[0], params, 53,
                            total_parameter_count - 53 - fname_len, fname_len);

      if( fname[0] == ':') {
          SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
          return(ERROR(0, 0xc0000000|NT_STATUS_OBJECT_PATH_NOT_FOUND));
      }

      return(ERROR(ERRDOS,ERRbadfid));
    }

    /*
     * Copy in the base directory name.
     */

    pstrcpy( fname, dir_fsp->fsp_name );
    dir_name_len = strlen(fname);

    /*
     * Ensure it ends in a '\'.
     */

    if((fname[dir_name_len-1] != '\\') && (fname[dir_name_len-1] != '/')) {
      pstrcat(fname, "\\");
      dir_name_len++;
    }

    /*
     * This next calculation can refuse a correct filename if we're dealing
     * with the Win2k unicode bug, but that would be rare. JRA.
     */

    if(fname_len + dir_name_len >= sizeof(pstring))
      return(ERROR(ERRSRV,ERRfilespecs));

    get_filename_transact(&fname[dir_name_len], params, 53,
                 total_parameter_count - 53 - fname_len, fname_len);

  } else {
    get_filename_transact(&fname[0], params, 53,
                 total_parameter_count - 53 - fname_len, fname_len);
  }

  /* If it's an IPC, use the pipe handler. */
  if (IS_IPC(conn)) {
    int ret = nt_open_pipe(fname, conn, inbuf, outbuf, &pnum);
    if(ret != 0)
      return ret;
    smb_action = FILE_WAS_OPENED;
  } else {

    /*
     * Now contruct the smb_open_mode value from the desired access
     * and the share access.
     */

    if((smb_open_mode = map_share_mode( &stat_open_only, fname, create_options, desired_access,
                                        share_access, file_attributes)) == -1)
      return(ERROR(ERRDOS,ERRbadaccess));

    oplock_request = (flags & REQUEST_OPLOCK) ? EXCLUSIVE_OPLOCK : 0;
    oplock_request |= (flags & REQUEST_BATCH_OPLOCK) ? BATCH_OPLOCK : 0;

    /*
     * Check if POSIX semantics are wanted.
     */

    set_posix_case_semantics(file_attributes);

    unix_convert(fname,conn,0,&bad_path,NULL);
    
    fsp = file_new();
    if (!fsp) {
	    restore_case_semantics(file_attributes);
	    return(ERROR(ERRSRV,ERRnofids));
    }

    if (!check_name(fname,conn)) { 
      if((errno == ENOENT) && bad_path) {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      file_free(fsp);

      restore_case_semantics(file_attributes);

      return(UNIXERROR(ERRDOS,ERRnoaccess));
    } 
  
    unixmode = unix_mode(conn,smb_attr | aARCH, fname);
    
    /*
     * If it's a request for a directory open, deal with it separately.
     */

    if(create_options & FILE_DIRECTORY_FILE) {

      oplock_request = 0;

      /*
       * We will get a create directory here if the Win32
       * app specified a security descriptor in the 
       * CreateDirectory() call.
       */

      open_directory(fsp, conn, fname, smb_ofun, unixmode, &smb_action);

      if(!fsp->open) {
        file_free(fsp);
        restore_case_semantics(file_attributes);
        return(UNIXERROR(ERRDOS,ERRnoaccess));
      }

      if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
        close_file(fsp,True);
        restore_case_semantics(file_attributes);
        return(ERROR(ERRDOS,ERRnoaccess));
      }

    } else {

      /*
       * Ordinary file case.
       */

      open_file_shared(fsp,conn,fname,smb_open_mode,smb_ofun,unixmode,
                       oplock_request,&rmode,&smb_action);

      if (!fsp->open) { 

		if(errno == EISDIR) {

			/*
			 * Fail the open if it was explicitly a non-directory file.
			 */

			if (create_options & FILE_NON_DIRECTORY_FILE) {
				file_free(fsp);
				restore_case_semantics(file_attributes);
				SSVAL(outbuf, smb_flg2, FLAGS2_32_BIT_ERROR_CODES);
				return(ERROR(0, 0xc0000000|NT_STATUS_FILE_IS_A_DIRECTORY));
			}
	
			oplock_request = 0;
			open_directory(fsp, conn, fname, smb_ofun, unixmode, &smb_action);
				
			if(!fsp->open) {
				file_free(fsp);
				restore_case_semantics(file_attributes);
				return(UNIXERROR(ERRDOS,ERRnoaccess));
			}
#ifdef EROFS
		} else if (((errno == EACCES) || (errno == EROFS)) && stat_open_only) {
#else /* !EROFS */
		} else if (errno == EACCES && stat_open_only) {
#endif

			/*
			 * We couldn't open normally and all we want
			 * are the permissions. Try and do a stat open.
			 */

			oplock_request = 0;

			open_file_stat(fsp,conn,fname,smb_open_mode,&sbuf,&smb_action);

			if(!fsp->open) {
				file_free(fsp);
				restore_case_semantics(file_attributes);
				return(UNIXERROR(ERRDOS,ERRnoaccess));
			}
		} else {

			if((errno == ENOENT) && bad_path) {
				unix_ERR_class = ERRDOS;
				unix_ERR_code = ERRbadpath;
			}
			file_free(fsp);

			restore_case_semantics(file_attributes);

			return(UNIXERROR(ERRDOS,ERRnoaccess));
		}
      } 
  
      if(fsp->is_directory) {
          if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
              close_file(fsp,True);
              restore_case_semantics(file_attributes);
              return(ERROR(ERRDOS,ERRnoaccess));
          }
      } else {
          if (!fsp->stat_open && sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
              close_file(fsp,False);
              restore_case_semantics(file_attributes);
              return(ERROR(ERRDOS,ERRnoaccess));
          } 
      }
 
      file_len = sbuf.st_size;
      fmode = dos_mode(conn,fname,&sbuf);
      if(fmode == 0)
        fmode = FILE_ATTRIBUTE_NORMAL;

      if (fmode & aDIR) {
        close_file(fsp,False);
        restore_case_semantics(file_attributes);
        return(ERROR(ERRDOS,ERRnoaccess));
      } 

      /* 
       * If the caller set the extended oplock request bit
       * and we granted one (by whatever means) - set the
       * correct bit for extended oplock reply.
       */
    
      if (oplock_request && lp_fake_oplocks(SNUM(conn)))
        smb_action |= EXTENDED_OPLOCK_GRANTED;
  
      if(oplock_request && EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type))
        smb_action |= EXTENDED_OPLOCK_GRANTED;
    }
  }

  restore_case_semantics(file_attributes);

  /* Realloc the size of parameters and data we will return */
  params = *ppparams = Realloc(*ppparams, 69);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  memset((char *)params,'\0',69);

  p = params;
  if (smb_action & EXTENDED_OPLOCK_GRANTED)	
  	SCVAL(p,0, BATCH_OPLOCK_RETURN);
  else if (LEVEL_II_OPLOCK_TYPE(fsp->oplock_type))
    SCVAL(p,0, LEVEL_II_OPLOCK_RETURN);
  else
	SCVAL(p,0,NO_OPLOCK_RETURN);
	
  p += 2;
  if (IS_IPC(conn)) {
	  SSVAL(p,0,pnum);
  } else {
	  SSVAL(p,0,fsp->fnum);
  }
  p += 2;
  SIVAL(p,0,smb_action);
  p += 8;

  if (IS_IPC(conn)) {
    /*
     * Deal with pipe return.
     */  
    p += 32;
    SIVAL(p,0,FILE_ATTRIBUTE_NORMAL); /* File Attributes. */
    p += 20;
    /* File type. */
    SSVAL(p,0,FILE_TYPE_MESSAGE_MODE_PIPE);
    /* Device state. */
    SSVAL(p,2, 0x5FF); /* ? */
  } else {
    /*
     * Deal with file return.
     */
    /* Create time. */
    put_long_date(p,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
    p += 8;
    put_long_date(p,sbuf.st_atime); /* access time */
    p += 8;
    put_long_date(p,sbuf.st_mtime); /* write time */
    p += 8;
    put_long_date(p,sbuf.st_mtime); /* change time */
    p += 8;
    SIVAL(p,0,fmode); /* File Attributes. */
    p += 4;
    SOFF_T(p,0,file_len);
    p += 8;
    SOFF_T(p,0,file_len);
  }

  DEBUG(5,("call_nt_transact_create: open name = %s\n", fname));

  /* Send the required number of replies */
  send_nt_replies(inbuf, outbuf, bufsize, 0, params, 69, *ppdata, 0);

  return -1;
}

/****************************************************************************
 Reply to a NT CANCEL request.
****************************************************************************/
int reply_ntcancel(connection_struct *conn,
		   char *inbuf,char *outbuf,int length,int bufsize)
{
	/*
	 * Go through and cancel any pending change notifies.
	 */
	
	int mid = SVAL(inbuf,smb_mid);
	remove_pending_change_notify_requests_by_mid(mid);
	remove_pending_lock_requests_by_mid(mid);
	
	DEBUG(3,("reply_ntcancel: cancel called on mid = %d.\n", mid));

	return(-1);
}

/****************************************************************************
 Reply to an unsolicited SMBNTtranss - just ignore it!
****************************************************************************/
int reply_nttranss(connection_struct *conn,
		   char *inbuf,char *outbuf,int length,int bufsize)
{
	DEBUG(4,("Ignoring nttranss of length %d\n",length));
	return(-1);
}

/****************************************************************************
 Reply to an NT transact rename command.
****************************************************************************/

static int call_nt_transact_rename(connection_struct *conn,
				   char *inbuf, char *outbuf, int length, 
                                   int bufsize,
                                   char **ppsetup, char **ppparams, char **ppdata)
{
  char *params = *ppparams;
  pstring new_name;
  files_struct *fsp = file_fsp(params, 0);
  BOOL replace_if_exists = (SVAL(params,2) & RENAME_REPLACE_IF_EXISTS) ? True : False;
  uint32 fname_len = MIN((((uint32)IVAL(inbuf,smb_nt_TotalParameterCount)-4)),
                         ((uint32)sizeof(new_name)-1));
  int outsize = 0;

  CHECK_FSP(fsp, conn);
  StrnCpy(new_name,params+4,fname_len);
  new_name[fname_len] = '\0';

  outsize = rename_internals(conn, inbuf, outbuf, fsp->fsp_name,
                             new_name, replace_if_exists);
  if(outsize == 0) {
    /*
     * Rename was successful.
     */
    send_nt_replies(inbuf, outbuf, bufsize, 0, NULL, 0, NULL, 0);

    DEBUG(3,("nt transact rename from = %s, to = %s succeeded.\n", 
          fsp->fsp_name, new_name));

    outsize = -1;
  }

  return(outsize);
}
   
/****************************************************************************
 This is the structure to keep the information needed to
 determine if a directory has changed.
*****************************************************************************/

typedef struct {
  time_t modify_time; /* Info from the directory we're monitoring. */ 
  time_t status_time; /* Info from the directory we're monitoring. */
  time_t total_time; /* Total time of all directory entries - don't care if it wraps. */
  unsigned int num_entries; /* Zero or the number of files in the directory. */
} change_hash_data;

/****************************************************************************
 This is the structure to queue to implement NT change
 notify. It consists of smb_size bytes stored from the
 transact command (to keep the mid, tid etc around).
 Plus the fid to examine and the time to check next.
*****************************************************************************/

typedef struct {
  ubi_slNode msg_next;
  files_struct *fsp;
  connection_struct *conn;
  uint32 flags;
  time_t next_check_time;
  change_hash_data change_data;
  char request_buf[smb_size];
} change_notify_buf;

static ubi_slList change_notify_queue = { NULL, (ubi_slNodePtr)&change_notify_queue, 0};

/****************************************************************************
 Setup the common parts of the return packet and send it.
*****************************************************************************/

static void change_notify_reply_packet(char *inbuf, int error_class, uint32 error_code)
{
  extern int Client;
  char outbuf[smb_size+38];

  memset(outbuf, '\0', sizeof(outbuf));
  construct_reply_common(inbuf, outbuf);

  /*
   * If we're returning a 'too much in the directory changed' we need to
   * set this is an NT error status flags. If we don't then the (probably
   * untested) code in the NT redirector has a bug in that it doesn't re-issue
   * the change notify.... Ah - I *love* it when I get so deeply into this I
   * can even determine how MS failed to test stuff and why.... :-). JRA.
   */

  if(error_class == 0) /* NT Error. */
    SSVAL(outbuf,smb_flg2, SVAL(outbuf,smb_flg2) | FLAGS2_32_BIT_ERROR_CODES);

  ERROR(error_class,error_code);

  /*
   * Seems NT needs a transact command with an error code
   * in it. This is a longer packet than a simple error.
   */
  set_message(outbuf,18,0,False);

  send_smb(Client,outbuf);
}

/****************************************************************************
 Create the hash we will use to determine if the contents changed.
*****************************************************************************/

static BOOL create_directory_notify_hash( change_notify_buf *cnbp, change_hash_data *change_data)
{
  SMB_STRUCT_STAT st;
  files_struct *fsp = cnbp->fsp;

  memset((char *)change_data, '\0', sizeof(change_data));

  /* 
   * Store the current timestamp on the directory we are monitoring.
   */

  if(dos_stat(fsp->fsp_name, &st) < 0) {
    DEBUG(0,("create_directory_notify_hash: Unable to stat name = %s. \
Error was %s\n", fsp->fsp_name, strerror(errno) ));
    return False;
  }
 
  change_data->modify_time = st.st_mtime;
  change_data->status_time = st.st_ctime;

  /*
   * If we are to watch for changes that are only stored
   * in inodes of files, not in the directory inode, we must
   * scan the directory and produce a unique identifier with
   * which we can determine if anything changed. We use the
   * modify and change times from all the files in the
   * directory, added together (ignoring wrapping if it's
   * larger than the max time_t value).
   */

  if(cnbp->flags & (FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE)) {
    pstring full_name;
    char *p;
    char *fname;
    size_t remaining_len;
    size_t fullname_len;
    void *dp = OpenDir(cnbp->conn, fsp->fsp_name, True);

    if(dp == NULL) {
      DEBUG(0,("create_directory_notify_hash: Unable to open directory = %s. \
Error was %s\n", fsp->fsp_name, strerror(errno) ));
      return False;
    }

    change_data->num_entries = 0;

    pstrcpy(full_name, fsp->fsp_name);
    pstrcat(full_name, "/");

    fullname_len = strlen(full_name);
    remaining_len = sizeof(full_name) - fullname_len - 1;
    p = &full_name[fullname_len];

    while ((fname = ReadDirName(dp))) {
      if(strequal(fname, ".") || strequal(fname, ".."))
        continue;

      change_data->num_entries++;
      safe_strcpy( p, fname, remaining_len);

      memset(&st, '\0', sizeof(st));

      /*
       * Do the stat - but ignore errors.
       */

      if(dos_stat(full_name, &st) < 0) {
        DEBUG(5,("create_directory_notify_hash: Unable to stat content file = %s. \
Error was %s\n", fsp->fsp_name, strerror(errno) ));
      }
      change_data->total_time += (st.st_mtime + st.st_ctime);
    }

    CloseDir(dp);
  }

  return True;
}

/****************************************************************************
 Delete entries by fnum from the change notify pending queue.
*****************************************************************************/

void remove_pending_change_notify_requests_by_fid(files_struct *fsp)
{
  change_notify_buf *cnbp = (change_notify_buf *)ubi_slFirst( &change_notify_queue );
  change_notify_buf *prev = NULL;

  while(cnbp != NULL) {
    if(cnbp->fsp->fnum == fsp->fnum) {
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      continue;
    }

    prev = cnbp;
    cnbp = (change_notify_buf *)ubi_slNext(cnbp);
  }
}

/****************************************************************************
 Delete entries by mid from the change notify pending queue. Always send reply.
*****************************************************************************/

static void remove_pending_change_notify_requests_by_mid(int mid)
{
  change_notify_buf *cnbp = (change_notify_buf *)ubi_slFirst( &change_notify_queue );
  change_notify_buf *prev = NULL;

  while(cnbp != NULL) {
    if(SVAL(cnbp->request_buf,smb_mid) == mid) {
      change_notify_reply_packet(cnbp->request_buf,0,0xC0000000 |NT_STATUS_CANCELLED);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      continue;
    }

    prev = cnbp;
    cnbp = (change_notify_buf *)ubi_slNext(cnbp);
  }
}

/****************************************************************************
 Delete entries by filename and cnum from the change notify pending queue.
 Always send reply.
*****************************************************************************/

void remove_pending_change_notify_requests_by_filename(files_struct *fsp)
{
  change_notify_buf *cnbp = (change_notify_buf *)ubi_slFirst( &change_notify_queue );
  change_notify_buf *prev = NULL;

  while(cnbp != NULL) {
    /*
     * We know it refers to the same directory if the connection number and
     * the filename are identical.
     */
    if((cnbp->fsp->conn == fsp->conn) && strequal(cnbp->fsp->fsp_name,fsp->fsp_name)) {
      change_notify_reply_packet(cnbp->request_buf,0,0xC0000000 |NT_STATUS_CANCELLED);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      continue;
    }

    prev = cnbp;
    cnbp = (change_notify_buf *)ubi_slNext(cnbp);
  }
}

/****************************************************************************
 Process the change notify queue. Note that this is only called as root.
 Returns True if there are still outstanding change notify requests on the
 queue.
*****************************************************************************/

BOOL process_pending_change_notify_queue(time_t t)
{
  change_notify_buf *cnbp = (change_notify_buf *)ubi_slFirst( &change_notify_queue );
  change_notify_buf *prev = NULL;

  if(cnbp == NULL)
    return False;

  if(cnbp->next_check_time >= t)
    return True;

  /*
   * It's time to check. Go through the queue and see if
   * the timestamps changed.
   */

  while((cnbp != NULL) && (cnbp->next_check_time <= t)) {
    change_hash_data change_data;
    connection_struct *conn = cnbp->conn;
    uint16 vuid = (lp_security() == SEC_SHARE) ? UID_FIELD_INVALID : 
                  SVAL(cnbp->request_buf,smb_uid);

	ZERO_STRUCT(change_data);

    /*
     * Ensure we don't have any old chain_fsp values
     * sitting around....
     */
    chain_size = 0;
    file_chain_reset();

    if(!become_user(conn,vuid)) {
      DEBUG(0,("process_pending_change_notify_queue: Unable to become user vuid=%d.\n",
            vuid ));
      /*
       * Remove the entry and return an error to the client.
       */
      change_notify_reply_packet(cnbp->request_buf,ERRSRV,ERRaccess);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      continue;
    }

    if(!become_service(conn,True)) {
	    DEBUG(0,("process_pending_change_notify_queue: Unable to become service Error was %s.\n", strerror(errno) ));
      /*
       * Remove the entry and return an error to the client.
       */
      change_notify_reply_packet(cnbp->request_buf,ERRSRV,ERRaccess);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      unbecome_user();
      continue;
    }

    if(!create_directory_notify_hash( cnbp, &change_data)) {
      DEBUG(0,("process_pending_change_notify_queue: Unable to create change data for \
directory %s\n", cnbp->fsp->fsp_name ));
      /*
       * Remove the entry and return an error to the client.
       */
      change_notify_reply_packet(cnbp->request_buf,ERRSRV,ERRaccess);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      unbecome_user();
      continue;
    }

    if(memcmp( (char *)&cnbp->change_data, (char *)&change_data, sizeof(change_data))) {
      /*
       * Remove the entry and return a change notify to the client.
       */
      DEBUG(5,("process_pending_change_notify_queue: directory name = %s changed.\n",
            cnbp->fsp->fsp_name ));
      change_notify_reply_packet(cnbp->request_buf,0,NT_STATUS_NOTIFY_ENUM_DIR);
      free((char *)ubi_slRemNext( &change_notify_queue, prev));
      cnbp = (change_notify_buf *)(prev ? ubi_slNext(prev) : ubi_slFirst(&change_notify_queue));
      unbecome_user();
      continue;
    }

    unbecome_user();

    /*
     * Move to the next in the list.
     */
    prev = cnbp;
    cnbp = (change_notify_buf *)ubi_slNext(cnbp);
  }

  return (cnbp != NULL);
}

/****************************************************************************
 Return true if there are pending change notifies.
****************************************************************************/

BOOL change_notifies_pending(void)
{
  change_notify_buf *cnbp = (change_notify_buf *)ubi_slFirst( &change_notify_queue );
  return (cnbp != NULL);
}

/****************************************************************************
 Reply to a notify change - queue the request and 
 don't allow a directory to be opened.
****************************************************************************/

static int call_nt_transact_notify_change(connection_struct *conn,
                                          char *inbuf, char *outbuf, int length,
                                          int bufsize, 
                                          char **ppsetup, 
                                          char **ppparams, char **ppdata)
{
  char *setup = *ppsetup;
  files_struct *fsp;
  change_notify_buf *cnbp;

  fsp = file_fsp(setup,4);

  DEBUG(3,("call_nt_transact_notify_change\n"));

  if(!fsp)
    return(ERROR(ERRDOS,ERRbadfid));

  if((!fsp->open) || (!fsp->is_directory) || (conn != fsp->conn))
    return(ERROR(ERRDOS,ERRbadfid));

  /*
   * Now queue an entry on the notify change stack. We timestamp
   * the entry we are adding so that we know when to scan next.
   * We only need to save smb_size bytes from this incoming packet
   * as we will always by returning a 'read the directory yourself'
   * error.
   */

  if((cnbp = (change_notify_buf *)malloc(sizeof(change_notify_buf))) == NULL) {
    DEBUG(0,("call_nt_transact_notify_change: malloc fail !\n" ));
    return -1;
  }

  memset((char *)cnbp, '\0', sizeof(change_notify_buf));

  memcpy(cnbp->request_buf, inbuf, smb_size);
  cnbp->fsp = fsp;
  cnbp->conn = conn;
  cnbp->next_check_time = time(NULL) + lp_change_notify_timeout();
  cnbp->flags = IVAL(setup, 0);

  if(!create_directory_notify_hash( cnbp, &cnbp->change_data )) {
    free((char *)cnbp);
    return(UNIXERROR(ERRDOS,ERRbadfid));
  }

  /*
   * Adding to the tail enables us to check only
   * the head when scanning for change, as this entry
   * is forced to have the first timeout expiration.
   */

  ubi_slAddTail(&change_notify_queue, cnbp);

  DEBUG(3,("call_nt_transact_notify_change: notify change called on directory \
name = %s\n", fsp->fsp_name ));

  return -1;
}

/****************************************************************************
 Map unix perms to NT.
****************************************************************************/

static SEC_ACCESS map_unix_perms( int *pacl_type, mode_t perm, int r_mask, int w_mask, int x_mask, BOOL is_directory)
{
	SEC_ACCESS sa;
	uint32 nt_mask = 0;

	*pacl_type = SEC_ACE_TYPE_ACCESS_ALLOWED;

	if((perm & (r_mask|w_mask|x_mask)) == (r_mask|w_mask|x_mask)) {
		nt_mask = UNIX_ACCESS_RWX;
	} else if((perm & (r_mask|w_mask|x_mask)) == 0) {
		nt_mask = UNIX_ACCESS_NONE;
	} else {
		nt_mask |= (perm & r_mask) ? UNIX_ACCESS_R : 0;
		if(is_directory)
			nt_mask |= (perm & w_mask) ? UNIX_ACCESS_W : 0;
		else
			nt_mask |= (perm & w_mask) ? UNIX_ACCESS_W : 0;
		nt_mask |= (perm & x_mask) ? UNIX_ACCESS_X : 0;
	}
	init_sec_access(&sa,nt_mask);
	return sa;
}

/****************************************************************************
 Function to create owner and group SIDs from a SMB_STRUCT_STAT.
****************************************************************************/

static void create_file_sids(SMB_STRUCT_STAT *psbuf, DOM_SID *powner_sid, DOM_SID *pgroup_sid)
{
  extern DOM_SID global_sam_sid;

  sid_copy(powner_sid, &global_sam_sid);
  sid_copy(pgroup_sid, &global_sam_sid);
  sid_append_rid(powner_sid, pdb_uid_to_user_rid(psbuf->st_uid));
  sid_append_rid(pgroup_sid, pdb_gid_to_group_rid(psbuf->st_gid));
}

/****************************************************************************
 Reply to query a security descriptor from an fsp. If it succeeds it allocates
 the space for the return elements and returns True.
****************************************************************************/

static size_t get_nt_acl(files_struct *fsp, SEC_DESC **ppdesc)
{
  extern DOM_SID global_sid_World;
  SMB_STRUCT_STAT sbuf;
  SEC_ACE ace_list[6];
  DOM_SID owner_sid;
  DOM_SID group_sid;
  size_t sec_desc_size;
  SEC_ACL *psa = NULL;
  SEC_ACCESS owner_access;
  int owner_acl_type;
  SEC_ACCESS group_access;
  int grp_acl_type;
  SEC_ACCESS other_access;
  int other_acl_type;
  int num_acls = 0;
 
  *ppdesc = NULL;

  if(!lp_nt_acl_support()) {
    sid_copy( &owner_sid, &global_sid_World);
    sid_copy( &group_sid, &global_sid_World);
  } else {

    if(fsp->is_directory || fsp->fd_ptr == NULL) {
      if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
        return 0;
      }
    } else {
      if(sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
        return 0;
      }
    }

    /*
     * Get the owner, group and world SIDs.
     */

    create_file_sids(&sbuf, &owner_sid, &group_sid);

    /*
     * Create the generic 3 element UNIX acl.
     */

    owner_access = map_unix_perms(&owner_acl_type, sbuf.st_mode,
							S_IRUSR, S_IWUSR, S_IXUSR, fsp->is_directory);
    group_access = map_unix_perms(&grp_acl_type, sbuf.st_mode,
							S_IRGRP, S_IWGRP, S_IXGRP, fsp->is_directory);
    other_access = map_unix_perms(&other_acl_type, sbuf.st_mode,
							S_IROTH, S_IWOTH, S_IXOTH, fsp->is_directory);

    if(owner_access.mask)
      init_sec_ace(&ace_list[num_acls++], &owner_sid, owner_acl_type,
                   owner_access, 0);

    if(group_access.mask)
      init_sec_ace(&ace_list[num_acls++], &group_sid, grp_acl_type,
                   group_access, 0);

    if(other_access.mask)
      init_sec_ace(&ace_list[num_acls++], &global_sid_World, other_acl_type,
                   other_access, 0);

    if(fsp->is_directory) {
      /*
       * For directory ACLs we also add in the inherited permissions
       * ACE entries. These are the permissions a file would get when
       * being created in the directory.
       */
      mode_t mode = unix_mode( fsp->conn, FILE_ATTRIBUTE_ARCHIVE, fsp->fsp_name);

      owner_access = map_unix_perms(&owner_acl_type, mode,
                            S_IRUSR, S_IWUSR, S_IXUSR, fsp->is_directory);
      group_access = map_unix_perms(&grp_acl_type, mode,
                            S_IRGRP, S_IWGRP, S_IXGRP, fsp->is_directory);
      other_access = map_unix_perms(&other_acl_type, mode,
                            S_IROTH, S_IWOTH, S_IXOTH, fsp->is_directory);

      if(owner_access.mask)
        init_sec_ace(&ace_list[num_acls++], &owner_sid, owner_acl_type,
                     owner_access, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_INHERIT_ONLY);

      if(group_access.mask)
        init_sec_ace(&ace_list[num_acls++], &group_sid, grp_acl_type,
                     group_access, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_INHERIT_ONLY);

      if(other_access.mask)
        init_sec_ace(&ace_list[num_acls++], &global_sid_World, other_acl_type,
                     other_access, SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_INHERIT_ONLY);
    }

    if(num_acls)
      if((psa = make_sec_acl( 3, num_acls, ace_list)) == NULL) {
        DEBUG(0,("get_nt_acl: Unable to malloc space for acl.\n"));
        return 0;
      }
  }

  *ppdesc = make_standard_sec_desc( &owner_sid, &group_sid, psa, &sec_desc_size);

  if(!*ppdesc) {
    DEBUG(0,("get_nt_acl: Unable to malloc space for security descriptor.\n"));
    sec_desc_size = 0;
  }

  free_sec_acl(&psa);

  return sec_desc_size;
}

/****************************************************************************
 Reply to query a security descriptor - currently this is not implemented (it
 is planned to be though). Right now it just returns the same thing NT would
 when queried on a FAT filesystem. JRA.
****************************************************************************/

static int call_nt_transact_query_security_desc(connection_struct *conn,
                                                char *inbuf, char *outbuf, 
                                                int length, int bufsize, 
                                                char **ppsetup, char **ppparams, char **ppdata)
{
  uint32 max_data_count = IVAL(inbuf,smb_nt_MaxDataCount);
  char *params = *ppparams;
  char *data = *ppdata;
  prs_struct pd;
  SEC_DESC *psd;
  size_t sec_desc_size;

  files_struct *fsp = file_fsp(params,0);

  if(!fsp)
    return(ERROR(ERRDOS,ERRbadfid));

  DEBUG(3,("call_nt_transact_query_security_desc: file = %s\n", fsp->fsp_name ));

  params = *ppparams = Realloc(*ppparams, 4);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  /*
   * Get the permissions to return.
   */

  if((sec_desc_size = get_nt_acl(fsp, &psd)) == 0)
    return(UNIXERROR(ERRDOS,ERRnoaccess));

  DEBUG(3,("call_nt_transact_query_security_desc: sec_desc_size = %d.\n",(int)sec_desc_size));

  SIVAL(params,0,(uint32)sec_desc_size);

  if(max_data_count < sec_desc_size) {

    free_sec_desc(&psd);

    send_nt_replies(inbuf, outbuf, bufsize, 0xC0000000|NT_STATUS_BUFFER_TOO_SMALL,
                    params, 4, *ppdata, 0);
    return -1;
  }

  /*
   * Allocate the data we will point this at.
   */

  data = *ppdata = Realloc(*ppdata, sec_desc_size);
  if(data == NULL) {
    free_sec_desc(&psd);
    return(ERROR(ERRDOS,ERRnomem));
  }

  memset(data, '\0', sec_desc_size);

  /*
   * Init the parse struct we will marshall into.
   */

  prs_init(&pd, 0, 4, MARSHALL);

  /*
   * Setup the prs_struct to point at the memory we just
   * allocated.
   */
	
  prs_give_memory( &pd, data, (uint32)sec_desc_size, False);

  /*
   * Finally, linearize into the outgoing buffer.
   */

  if(!sec_io_desc( "sd data", &psd, &pd, 1)) {
    free_sec_desc(&psd);
    DEBUG(0,("call_nt_transact_query_security_desc: Error in marshalling \
security descriptor.\n"));
    /*
     * Return access denied for want of a better error message..
     */ 
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  /*
   * Now we can delete the security descriptor.
   */

  free_sec_desc(&psd);

  send_nt_replies(inbuf, outbuf, bufsize, 0, params, 4, data, (int)sec_desc_size);
  return -1;
}

/****************************************************************************
 Validate a SID.
****************************************************************************/

static BOOL validate_unix_sid( DOM_SID *psid, uint32 *prid, DOM_SID *sd_sid)
{
  extern DOM_SID global_sam_sid;
  DOM_SID sid;

  if(!sd_sid) {
    DEBUG(5,("validate_unix_sid: sid missing.\n"));
    return False;
  }

  sid_copy(psid, sd_sid);
  sid_copy(&sid, sd_sid);

  if(!sid_split_rid(&sid, prid)) {
    DEBUG(5,("validate_unix_sid: cannot get RID from sid.\n"));
    return False;
  }

  if(!sid_equal( &sid, &global_sam_sid)) {
    DEBUG(5,("validate_unix_sid: sid is not ours.\n"));
    return False;
  }

  return True;
}

/****************************************************************************
 Map NT perms to UNIX.
****************************************************************************/

#define FILE_SPECIFIC_READ_BITS (FILE_READ_DATA|FILE_READ_EA|FILE_READ_ATTRIBUTES)
#define FILE_SPECIFIC_WRITE_BITS (FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES)
#define FILE_SPECIFIC_EXECUTE_BITS (FILE_EXECUTE)

static mode_t map_nt_perms( SEC_ACCESS sec_access, int type)
{
  mode_t mode = 0;

  switch(type) {
  case S_IRUSR:
    if(sec_access.mask & GENERIC_ALL_ACCESS)
      mode = S_IRUSR|S_IWUSR|S_IXUSR;
    else {
      mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IRUSR : 0;
      mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWUSR : 0;
      mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXUSR : 0;
    }
    break;
  case S_IRGRP:
    if(sec_access.mask & GENERIC_ALL_ACCESS)
      mode = S_IRGRP|S_IWGRP|S_IXGRP;
    else {
      mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IRGRP : 0;
      mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWGRP : 0;
      mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXGRP : 0;
    }
    break;
  case S_IROTH:
    if(sec_access.mask & GENERIC_ALL_ACCESS)
      mode = S_IROTH|S_IWOTH|S_IXOTH;
    else {
      mode |= (sec_access.mask & (GENERIC_READ_ACCESS|FILE_SPECIFIC_READ_BITS)) ? S_IROTH : 0;
      mode |= (sec_access.mask & (GENERIC_WRITE_ACCESS|FILE_SPECIFIC_WRITE_BITS)) ? S_IWOTH : 0;
      mode |= (sec_access.mask & (GENERIC_EXECUTE_ACCESS|FILE_SPECIFIC_EXECUTE_BITS)) ? S_IXOTH : 0;
    }
    break;
  }

  return mode;
}

/****************************************************************************
 Unpack a SEC_DESC into a owner, group and set of UNIX permissions.
****************************************************************************/

static BOOL unpack_nt_permissions(SMB_STRUCT_STAT *psbuf, uid_t *puser, gid_t *pgrp, mode_t *pmode,
                                  uint32 security_info_sent, SEC_DESC *psd, BOOL is_directory)
{
  extern DOM_SID global_sid_World;
  DOM_SID owner_sid;
  DOM_SID grp_sid;
  DOM_SID file_owner_sid;
  DOM_SID file_grp_sid;
  uint32 owner_rid;
  uint32 grp_rid;
  SEC_ACL *dacl = psd->dacl;
  BOOL all_aces_are_inherit_only = (is_directory ? True : False);
  int i;

  *pmode = 0;
  *puser = (uid_t)-1;
  *pgrp = (gid_t)-1;

  if(security_info_sent == 0) {
    DEBUG(0,("unpack_nt_permissions: no security info sent !\n"));
    return False;
  }

  /*
   * Windows 2000 sends the owner and group SIDs as the logged in
   * user, not the connected user. But it still sends the file
   * owner SIDs on an ACL set. So we need to check for the file
   * owner and group SIDs as well as the owner SIDs. JRA.
   */

  create_file_sids(psbuf, &file_owner_sid, &file_grp_sid);

  /*
   * Validate the owner and group SID's.
   */

  memset(&owner_sid, '\0', sizeof(owner_sid));
  memset(&grp_sid, '\0', sizeof(grp_sid));

  DEBUG(5,("unpack_nt_permissions: validating owner_sid.\n"));

  /*
   * Don't immediately fail if the owner sid cannot be validated.
   * This may be a group chown only set.
   */

  if(!validate_unix_sid( &owner_sid, &owner_rid, psd->owner_sid))
    DEBUG(3,("unpack_nt_permissions: unable to validate owner sid.\n"));
  else if(security_info_sent & OWNER_SECURITY_INFORMATION)
    *puser = pdb_user_rid_to_uid(owner_rid);

  /*
   * Don't immediately fail if the group sid cannot be validated.
   * This may be an owner chown only set.
   */

  if(!validate_unix_sid( &grp_sid, &grp_rid, psd->grp_sid))
    DEBUG(3,("unpack_nt_permissions: unable to validate group sid.\n"));
  else if(security_info_sent & GROUP_SECURITY_INFORMATION)
    *pgrp = pdb_user_rid_to_gid(grp_rid);

  /*
   * If no DACL then this is a chown only security descriptor.
   */

  if(!(security_info_sent & DACL_SECURITY_INFORMATION) || !dacl) {
    *pmode = 0;
    return True;
  }

  /*
   * Now go through the DACL and ensure that
   * any owner/group sids match.
   */

  for(i = 0; i < dacl->num_aces; i++) {
    DOM_SID ace_sid;
    SEC_ACE *psa = &dacl->ace_list[i];

    if((psa->type != SEC_ACE_TYPE_ACCESS_ALLOWED) &&
       (psa->type != SEC_ACE_TYPE_ACCESS_DENIED)) {
      DEBUG(3,("unpack_nt_permissions: unable to set anything but an ALLOW or DENY ACE.\n"));
      return False;
    }

    /*
     * Ignore or remove bits we don't care about on a directory ACE.
     */

    if(is_directory) {
      if(psa->flags & SEC_ACE_FLAG_INHERIT_ONLY) {
        DEBUG(3,("unpack_nt_permissions: ignoring inherit only ACE.\n"));
        continue;
      }

      /*
       * At least one of the ACE entries wasn't inherit only.
       * Flag this so we know the returned mode is valid.
       */

      all_aces_are_inherit_only = False;
    }

    /*
     * Windows 2000 sets these flags even on *file* ACE's. This is wrong
     * but we can ignore them for now. Revisit this when we go to POSIX
     * ACLs on directories.
     */

    psa->flags &= ~(SEC_ACE_FLAG_OBJECT_INHERIT|SEC_ACE_FLAG_CONTAINER_INHERIT);

    if(psa->flags != 0) {
      DEBUG(1,("unpack_nt_permissions: unable to set ACE flags (%x).\n", 
            (unsigned int)psa->flags));
      return False;
    }

    /*
     * The security mask may be UNIX_ACCESS_NONE which should map into
     * no permissions (we overload the WRITE_OWNER bit for this) or it
     * should be one of the ALL/EXECUTE/READ/WRITE bits. Arrange for this
     * to be so. Any other bits override the UNIX_ACCESS_NONE bit.
     */

    psa->info.mask &= (GENERIC_ALL_ACCESS|GENERIC_EXECUTE_ACCESS|GENERIC_WRITE_ACCESS|
                     GENERIC_READ_ACCESS|UNIX_ACCESS_NONE|FILE_ALL_ATTRIBUTES);

    if(psa->info.mask != UNIX_ACCESS_NONE)
      psa->info.mask &= ~UNIX_ACCESS_NONE;

    sid_copy(&ace_sid, &psa->sid);

    if(sid_equal(&ace_sid, &file_owner_sid)) {
      /*
       * Map the desired permissions into owner perms.
       */

      if(psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED)
        *pmode |= map_nt_perms( psa->info, S_IRUSR);
      else
        *pmode &= ~(map_nt_perms( psa->info, S_IRUSR));

    } else if( sid_equal(&ace_sid, &file_grp_sid)) {
      /*
       * Map the desired permissions into group perms.
       */

      if(psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED)
        *pmode |= map_nt_perms( psa->info, S_IRGRP);
      else
        *pmode &= ~(map_nt_perms( psa->info, S_IRGRP));

    } else if( sid_equal(&ace_sid, &global_sid_World)) {
      /*
       * Map the desired permissions into other perms.
       */

      if(psa->type == SEC_ACE_TYPE_ACCESS_ALLOWED)
        *pmode |= map_nt_perms( psa->info, S_IROTH);
      else
        *pmode &= ~(map_nt_perms( psa->info, S_IROTH));

    } else {
      DEBUG(0,("unpack_nt_permissions: unknown SID used in ACL.\n"));
      return False;
    }
  }

  if (is_directory && all_aces_are_inherit_only) {
    /*
     * Windows 2000 is doing one of these weird 'inherit acl'
     * traverses to conserve NTFS ACL resources. Just pretend
     * there was no DACL sent. JRA.
     */

    DEBUG(10,("unpack_nt_permissions: Win2k inherit acl traverse. Ignoring DACL.\n"));
    free_sec_acl(&psd->dacl);
  }

  return True;
}

/****************************************************************************
 Reply to set a security descriptor. Map to UNIX perms.
****************************************************************************/

static int call_nt_transact_set_security_desc(connection_struct *conn,
									char *inbuf, char *outbuf, int length,
									int bufsize, char **ppsetup, 
									char **ppparams, char **ppdata)
{
  uint32 total_parameter_count = IVAL(inbuf, smb_nts_TotalParameterCount);
  char *params= *ppparams;
  char *data = *ppdata;
  prs_struct pd;
  SEC_DESC *psd = NULL;
  uint32 total_data_count = (uint32)IVAL(inbuf, smb_nts_TotalDataCount);
  uid_t user = (uid_t)-1;
  gid_t grp = (gid_t)-1;
  mode_t perms = 0;
  SMB_STRUCT_STAT sbuf;
  files_struct *fsp = NULL;
  uint32 security_info_sent = 0;
  BOOL got_dacl = False;

  if(!lp_nt_acl_support())
    return(UNIXERROR(ERRDOS,ERRnoaccess));

  if(total_parameter_count < 8)
    return(ERROR(ERRDOS,ERRbadfunc));

  if((fsp = file_fsp(params,0)) == NULL)
    return(ERROR(ERRDOS,ERRbadfid));

  security_info_sent = IVAL(params,4);

  DEBUG(3,("call_nt_transact_set_security_desc: file = %s, sent 0x%x\n", fsp->fsp_name,
       (unsigned int)security_info_sent ));

  /*
   * Init the parse struct we will unmarshall from.
   */

  prs_init(&pd, 0, 4, UNMARSHALL);

  /*
   * Setup the prs_struct to point at the memory we just
   * allocated.
   */
	
  prs_give_memory( &pd, data, total_data_count, False);

  /*
   * Finally, unmarshall from the data buffer.
   */

  if(!sec_io_desc( "sd data", &psd, &pd, 1)) {
    free_sec_desc(&psd);
    DEBUG(0,("call_nt_transact_set_security_desc: Error in unmarshalling \
security descriptor.\n"));
    /*
     * Return access denied for want of a better error message..
     */ 
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  /*
   * Get the current state of the file.
   */

  if(fsp->is_directory) {
    if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
      free_sec_desc(&psd);
      return(UNIXERROR(ERRDOS,ERRnoaccess));
    }
  } else {

    int ret;

    if(fsp->fd_ptr == NULL)
      ret = dos_stat(fsp->fsp_name, &sbuf);
    else
      ret = sys_fstat(fsp->fd_ptr->fd,&sbuf);

    if(ret != 0) {
      free_sec_desc(&psd);
      return(UNIXERROR(ERRDOS,ERRnoaccess));
    }
  }

  /*
   * Unpack the user/group/world id's and permissions.
   */

  if(!unpack_nt_permissions( &sbuf, &user, &grp, &perms, security_info_sent, psd, fsp->is_directory)) {
    free_sec_desc(&psd);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  if (psd->dacl != NULL)
    got_dacl = True;

  free_sec_desc(&psd);

  /*
   * Do we need to chown ?
   */

  if((user != (uid_t)-1 || grp != (uid_t)-1) && (sbuf.st_uid != user || sbuf.st_gid != grp)) {

    DEBUG(3,("call_nt_transact_set_security_desc: chown %s. uid = %u, gid = %u.\n",
          fsp->fsp_name, (unsigned int)user, (unsigned int)grp ));

    if(dos_chown( fsp->fsp_name, user, grp) == -1) {
      DEBUG(3,("call_nt_transact_set_security_desc: chown %s, %u, %u failed. Error = %s.\n",
            fsp->fsp_name, (unsigned int)user, (unsigned int)grp, strerror(errno) ));
      return(UNIXERROR(ERRDOS,ERRnoaccess));
    }

    /*
     * Recheck the current state of the file, which may have changed.
     * (suid/sgid bits, for instance)
     */

    if(fsp->is_directory) {
      if(dos_stat(fsp->fsp_name, &sbuf) != 0) {
        return(UNIXERROR(ERRDOS,ERRnoaccess));
      }
    } else {

      int ret;
    
      if(fsp->fd_ptr == NULL)
        ret = dos_stat(fsp->fsp_name, &sbuf);
      else
        ret = sys_fstat(fsp->fd_ptr->fd,&sbuf);
  
      if(ret != 0)
        return(UNIXERROR(ERRDOS,ERRnoaccess));
    }
  }

  /*
   * Only change security if we got a DACL.
   */

  if((security_info_sent & DACL_SECURITY_INFORMATION) && got_dacl) {

    /*
     * Check to see if we need to change anything.
     * Enforce limits on modified bits *only*. Don't enforce masks
	 * on bits not changed by the user.
     */

    if(fsp->is_directory) {

      perms &= (lp_dir_security_mask(SNUM(conn)) | sbuf.st_mode);
      perms |= (lp_force_dir_security_mode(SNUM(conn)) & ( perms ^ sbuf.st_mode ));

    } else {

      perms &= (lp_security_mask(SNUM(conn)) | sbuf.st_mode); 
      perms |= (lp_force_security_mode(SNUM(conn)) & ( perms ^ sbuf.st_mode ));

    }

    /*
     * Preserve special bits.
     */

    perms |= (sbuf.st_mode & ~0777);

    /*
     * Do we need to chmod ?
     */

    if(sbuf.st_mode != perms) {

      DEBUG(3,("call_nt_transact_set_security_desc: chmod %s. perms = 0%o.\n",
            fsp->fsp_name, (unsigned int)perms ));

      if(dos_chmod( fsp->fsp_name, perms) == -1) {
        DEBUG(3,("call_nt_transact_set_security_desc: chmod %s, 0%o failed. Error = %s.\n",
              fsp->fsp_name, (unsigned int)perms, strerror(errno) ));
        return(UNIXERROR(ERRDOS,ERRnoaccess));
      }
    }
  }

  send_nt_replies(inbuf, outbuf, bufsize, 0, NULL, 0, NULL, 0);
  return -1;
}
   
/****************************************************************************
 Reply to IOCTL - not implemented - no plans.
****************************************************************************/
static int call_nt_transact_ioctl(connection_struct *conn,
				  char *inbuf, char *outbuf, int length,
                                  int bufsize, 
                                  char **ppsetup, char **ppparams, char **ppdata)
{
  static BOOL logged_message = False;

  if(!logged_message) {
    DEBUG(3,("call_nt_transact_ioctl: Currently not implemented.\n"));
    logged_message = True; /* Only print this once... */
  }
  return(ERROR(ERRSRV,ERRnosupport));
}
   
/****************************************************************************
 Reply to a SMBNTtrans.
****************************************************************************/
int reply_nttrans(connection_struct *conn,
		  char *inbuf,char *outbuf,int length,int bufsize)
{
  int  outsize = 0;
#if 0 /* Not used. */
  uint16 max_setup_count = CVAL(inbuf, smb_nt_MaxSetupCount);
  uint32 max_parameter_count = IVAL(inbuf, smb_nt_MaxParameterCount);
  uint32 max_data_count = IVAL(inbuf,smb_nt_MaxDataCount);
#endif /* Not used. */
  uint32 total_parameter_count = IVAL(inbuf, smb_nt_TotalParameterCount);
  uint32 total_data_count = IVAL(inbuf, smb_nt_TotalDataCount);
  uint32 parameter_count = IVAL(inbuf,smb_nt_ParameterCount);
  uint32 parameter_offset = IVAL(inbuf,smb_nt_ParameterOffset);
  uint32 data_count = IVAL(inbuf,smb_nt_DataCount);
  uint32 data_offset = IVAL(inbuf,smb_nt_DataOffset);
  uint16 setup_count = 2*CVAL(inbuf,smb_nt_SetupCount); /* setup count is in *words* */
  uint16 function_code = SVAL( inbuf, smb_nt_Function);
  char *params = NULL, *data = NULL, *setup = NULL;
  uint32 num_params_sofar, num_data_sofar;

  if(global_oplock_break && (function_code == NT_TRANSACT_CREATE)) {
    /*
     * Queue this open message as we are the process of an oplock break.
     */

    DEBUG(2,("reply_nttrans: queueing message NT_TRANSACT_CREATE \
due to being in oplock break state.\n" ));

    push_oplock_pending_smb_message( inbuf, length);
    return -1;
  }

  outsize = set_message(outbuf,0,0,True);

  /* 
   * All nttrans messages we handle have smb_wct == 19 + setup_count.
   * Ensure this is so as a sanity check.
   */

  if(CVAL(inbuf, smb_wct) != 19 + (setup_count/2)) {
    DEBUG(2,("Invalid smb_wct %d in nttrans call (should be %d)\n",
          CVAL(inbuf, smb_wct), 19 + (setup_count/2)));
    return(ERROR(ERRSRV,ERRerror));
  }
    
  /* Allocate the space for the setup, the maximum needed parameters and data */

  if(setup_count > 0)
    setup = (char *)malloc(setup_count);
  if (total_parameter_count > 0)
    params = (char *)malloc(total_parameter_count);
  if (total_data_count > 0)
    data = (char *)malloc(total_data_count);

  if ((total_parameter_count && !params)  || (total_data_count && !data) ||
      (setup_count && !setup)) {
    SAFE_FREE(setup);
    SAFE_FREE(params);
    SAFE_FREE(data);
    DEBUG(0,("reply_nttrans : Out of memory\n"));
    return ERROR(ERRDOS,ERRnomem);
  }

  /* Copy the param and data bytes sent with this request into
     the params buffer */
  num_params_sofar = parameter_count;
  num_data_sofar = data_count;

  if (parameter_count > total_parameter_count || data_count > total_data_count)
    goto bad_param;

  if(setup) {
    DEBUG(10,("reply_nttrans: setup_count = %d\n", setup_count));
    if ((smb_nt_SetupStart + setup_count < smb_nt_SetupStart) ||
	(smb_nt_SetupStart + setup_count < setup_count))
      goto bad_param;
    if (smb_nt_SetupStart + setup_count > length)
      goto bad_param;
    
    memcpy( setup, &inbuf[smb_nt_SetupStart], setup_count);
  }
  if(params) {
    DEBUG(10,("reply_nttrans: parameter_count = %d\n", parameter_count));
    if ((parameter_offset + parameter_count < parameter_offset) ||
	(parameter_offset + parameter_count < parameter_count))
      goto bad_param;
    if (smb_base(inbuf) + parameter_offset + parameter_count > inbuf + length)
      goto bad_param;
    
    memcpy( params, smb_base(inbuf) + parameter_offset, parameter_count);
  }
  if(data) {
    DEBUG(10,("reply_nttrans: data_count = %d\n",data_count));
    if ((data_offset + data_count < data_offset) || (data_offset + data_count < data_count))
      goto bad_param;
    if (smb_base(inbuf) + data_offset + data_count > inbuf + length)
      goto bad_param;
    
    memcpy( data, smb_base(inbuf) + data_offset, data_count);

  }

  if(num_data_sofar < total_data_count || num_params_sofar < total_parameter_count) {
    /* We need to send an interim response then receive the rest
       of the parameter/data bytes */
    outsize = set_message(outbuf,0,0,True);
    if (!send_smb(Client,outbuf))
      exit_server("reply_nttrans: send_smb failed.");

    while( num_data_sofar < total_data_count || num_params_sofar < total_parameter_count) {
      BOOL ret;
      uint32 parameter_displacement;
      uint32 data_displacement;
      
      ret = receive_next_smb(inbuf,bufsize,SMB_SECONDARY_WAIT);
      
      if((ret && (CVAL(inbuf, smb_com) != SMBnttranss)) || !ret) {
	outsize = set_message(outbuf,0,0,True);
	if(ret) {
	  DEBUG(0,("reply_nttrans: Invalid secondary nttrans packet\n"));
	} else {
	  DEBUG(0,("reply_nttrans: %s in getting secondary nttrans response.\n",
		   (smb_read_error == READ_ERROR) ? "error" : "timeout" ));
	}
	goto bad_param;
      }
      
      /* Revise total_params and total_data in case they have changed downwards */
      if (IVAL(inbuf, smb_nts_TotalParameterCount) < total_parameter_count)
	total_parameter_count = IVAL(inbuf, smb_nts_TotalParameterCount);
      if (IVAL(inbuf, smb_nts_TotalDataCount) < total_data_count)
	total_data_count = IVAL(inbuf, smb_nts_TotalDataCount);
      
      parameter_count = IVAL(inbuf,smb_nts_ParameterCount);
      parameter_offset = IVAL(inbuf, smb_nts_ParameterOffset);
      parameter_displacement = IVAL(inbuf, smb_nts_ParameterDisplacement);
      num_params_sofar += parameter_count;
      
      data_count = IVAL(inbuf, smb_nts_DataCount);
      data_displacement = IVAL(inbuf, smb_nts_DataDisplacement);
      data_offset = IVAL(inbuf, smb_nts_DataOffset);
      num_data_sofar += data_count;

      if (num_params_sofar > total_parameter_count || num_data_sofar > total_data_count) {
	DEBUG(0,("reply_nttrans2: data overflow in secondary nttrans packet"));
	goto bad_param;
      }

      if (parameter_count) {
	if (parameter_displacement + parameter_count >= total_parameter_count)
	  goto bad_param;
	if ((parameter_displacement + parameter_count < parameter_displacement) ||
	    (parameter_displacement + parameter_count < parameter_count))
	  goto bad_param;
	if (smb_base(inbuf) + parameter_offset + parameter_count >= inbuf + bufsize)
	  goto bad_param;
	if (params + parameter_displacement < params)
	  goto bad_param;
	
	memcpy( &params[parameter_displacement], smb_base(inbuf) + parameter_offset, parameter_count);
      }
      
      if (data_count) {
	if (data_displacement + data_count >= total_data_count)
	  goto bad_param;
	if ((data_displacement + data_count < data_displacement) ||
	    (data_displacement + data_count < data_count))
	  goto bad_param;
	if (smb_base(inbuf) + data_offset + data_count >= inbuf + bufsize)
	  goto bad_param;
	if (data + data_displacement < data)
	  goto bad_param;
	
	memcpy( &data[data_displacement], smb_base(inbuf)+ data_offset, data_count);
      }
    }
  }

  if (Protocol >= PROTOCOL_NT1) {
    uint16 flg2 = SVAL(outbuf,smb_flg2);
    SSVAL(outbuf,smb_flg2,flg2 | 0x40); /* IS_LONG_NAME */
  }

  /* Now we must call the relevant NT_TRANS function */
  switch(function_code) {
    case NT_TRANSACT_CREATE:
      outsize = call_nt_transact_create(conn, inbuf, outbuf, length, bufsize, 
                                        &setup, &params, &data);
      break;
    case NT_TRANSACT_IOCTL:
      outsize = call_nt_transact_ioctl(conn, 
				       inbuf, outbuf, length, bufsize, 
                                       &setup, &params, &data);
      break;
    case NT_TRANSACT_SET_SECURITY_DESC:
      outsize = call_nt_transact_set_security_desc(conn, inbuf, outbuf, 
						   length, bufsize, 
                                                   &setup, &params, &data);
      break;
    case NT_TRANSACT_NOTIFY_CHANGE:
      outsize = call_nt_transact_notify_change(conn, inbuf, outbuf, 
					       length, bufsize, 
                                               &setup, &params, &data);
      break;
    case NT_TRANSACT_RENAME:
      outsize = call_nt_transact_rename(conn, inbuf, outbuf, length, 
					bufsize, 
                                        &setup, &params, &data);
      break;

    case NT_TRANSACT_QUERY_SECURITY_DESC:
      outsize = call_nt_transact_query_security_desc(conn, inbuf, outbuf, 
						     length, bufsize, 
                                                     &setup, &params, &data);
      break;
  default:
	  /* Error in request */
	  DEBUG(0,("reply_nttrans: Unknown request %d in nttrans call\n", function_code));
	  if(setup)
		  free(setup);
	  if(params)
		  free(params);
	  if(data)
		  free(data);
	  return (ERROR(ERRSRV,ERRerror));
  }

  /* As we do not know how many data packets will need to be
     returned here the various call_nt_transact_xxxx calls
     must send their own. Thus a call_nt_transact_xxxx routine only
     returns a value other than -1 when it wants to send
     an error packet. 
  */

  if(setup)
    free(setup);
  if(params)
    free(params);
  if(data)
    free(data);
  return outsize; /* If a correct response was needed the call_nt_transact_xxxx 
		     calls have already sent it. If outsize != -1 then it is
		     returning an error packet. */
 bad_param:

  SAFE_FREE(params);
  SAFE_FREE(data);
  SAFE_FREE(setup);
  return ERROR(ERRDOS,ERRinvalidparam);
}
