/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   SMB transaction2 handling
   Copyright (C) Jeremy Allison 1994-1998

   Extensively modified by Andrew Tridgell, 1995

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
#include "trans2.h"

extern int DEBUGLEVEL;
extern int Protocol;
extern BOOL case_sensitive;
extern int Client;
extern int smb_read_error;
extern fstring local_machine;
extern int global_oplock_break;
extern uint32 global_client_caps;

/****************************************************************************
  Send the required number of replies back.
  We assume all fields other than the data fields are
  set correctly for the type of call.
  HACK ! Always assumes smb_setup field is zero.
****************************************************************************/
static int send_trans2_replies(char *outbuf, int bufsize, char *params, 
			       int paramsize, char *pdata, int datasize)
{
  /* As we are using a protocol > LANMAN1 then the max_send
     variable must have been set in the sessetupX call.
     This takes precedence over the max_xmit field in the
     global struct. These different max_xmit variables should
     be merged as this is now too confusing */

  extern int max_send;
  int data_to_send = datasize;
  int params_to_send = paramsize;
  int useable_space;
  char *pp = params;
  char *pd = pdata;
  int params_sent_thistime, data_sent_thistime, total_sent_thistime;
  int alignment_offset = 3;
  int data_alignment_offset = 0;

  /* Initially set the wcnt area to be 10 - this is true for all
     trans2 replies */
  set_message(outbuf,10,0,True);

  /* If there genuinely are no parameters or data to send just send
     the empty packet */
  if(params_to_send == 0 && data_to_send == 0)
  {
    send_smb(Client,outbuf);
    return 0;
  }

  /* When sending params and data ensure that both are nicely aligned */
  /* Only do this alignment when there is also data to send - else
     can cause NT redirector problems. */
  if (((params_to_send % 4) != 0) && (data_to_send != 0))
    data_alignment_offset = 4 - (params_to_send % 4);

  /* Space is bufsize minus Netbios over TCP header minus SMB header */
  /* The alignment_offset is to align the param bytes on an even byte
     boundary. NT 4.0 Beta needs this to work correctly. */
  useable_space = bufsize - ((smb_buf(outbuf)+
                    alignment_offset+data_alignment_offset) - 
                    outbuf);

  /* useable_space can never be more than max_send minus the
     alignment offset. */
  useable_space = MIN(useable_space, 
                      max_send - (alignment_offset+data_alignment_offset));


  while (params_to_send || data_to_send)
  {
    /* Calculate whether we will totally or partially fill this packet */
    total_sent_thistime = params_to_send + data_to_send + 
                            alignment_offset + data_alignment_offset;
    /* We can never send more than useable_space */
    /*
     * Note that 'useable_space' does not include the alignment offsets,
     * but we must include the alignment offsets in the calculation of
     * the length of the data we send over the wire, as the alignment offsets
     * are sent here. Fix from Marc_Jacobsen@hp.com.
     */
    total_sent_thistime = MIN(total_sent_thistime, useable_space +
                                alignment_offset + data_alignment_offset);

    set_message(outbuf, 10, total_sent_thistime, True);

    /* Set total params and data to be sent */
    SSVAL(outbuf,smb_tprcnt,paramsize);
    SSVAL(outbuf,smb_tdrcnt,datasize);

    /* Calculate how many parameters and data we can fit into
       this packet. Parameters get precedence */

    params_sent_thistime = MIN(params_to_send,useable_space);
    data_sent_thistime = useable_space - params_sent_thistime;
    data_sent_thistime = MIN(data_sent_thistime,data_to_send);

    SSVAL(outbuf,smb_prcnt, params_sent_thistime);

    /* smb_proff is the offset from the start of the SMB header to the
       parameter bytes, however the first 4 bytes of outbuf are
       the Netbios over TCP header. Thus use smb_base() to subtract
       them from the calculation */

    SSVAL(outbuf,smb_proff,((smb_buf(outbuf)+alignment_offset) - smb_base(outbuf)));

    if(params_sent_thistime == 0)
      SSVAL(outbuf,smb_prdisp,0);
    else
      /* Absolute displacement of param bytes sent in this packet */
      SSVAL(outbuf,smb_prdisp,pp - params);

    SSVAL(outbuf,smb_drcnt, data_sent_thistime);
    if(data_sent_thistime == 0)
    {
      SSVAL(outbuf,smb_droff,0);
      SSVAL(outbuf,smb_drdisp, 0);
    }
    else
    {
      /* The offset of the data bytes is the offset of the
         parameter bytes plus the number of parameters being sent this time */
      SSVAL(outbuf,smb_droff,((smb_buf(outbuf)+alignment_offset) - 
            smb_base(outbuf)) + params_sent_thistime + data_alignment_offset);
      SSVAL(outbuf,smb_drdisp, pd - pdata);
    }

    /* Copy the param bytes into the packet */
    if(params_sent_thistime)
      memcpy((smb_buf(outbuf)+alignment_offset),pp,params_sent_thistime);
    /* Copy in the data bytes */
    if(data_sent_thistime)
      memcpy(smb_buf(outbuf)+alignment_offset+params_sent_thistime+
             data_alignment_offset,pd,data_sent_thistime);

    DEBUG(9,("t2_rep: params_sent_thistime = %d, data_sent_thistime = %d, useable_space = %d\n",
          params_sent_thistime, data_sent_thistime, useable_space));
    DEBUG(9,("t2_rep: params_to_send = %d, data_to_send = %d, paramsize = %d, datasize = %d\n",
          params_to_send, data_to_send, paramsize, datasize));

    /* Send the packet */
    send_smb(Client,outbuf);

    pp += params_sent_thistime;
    pd += data_sent_thistime;

    params_to_send -= params_sent_thistime;
    data_to_send -= data_sent_thistime;

    /* Sanity check */
    if(params_to_send < 0 || data_to_send < 0)
    {
      DEBUG(0,("send_trans2_replies failed sanity check pts = %d, dts = %d\n!!!",
            params_to_send, data_to_send));
      return -1;
    }
  }

  return 0;
}


/****************************************************************************
  reply to a TRANSACT2_OPEN
****************************************************************************/
static int call_trans2open(connection_struct *conn, char *inbuf, char *outbuf, 
			   int bufsize,  
			   char **pparams, char **ppdata)
{
  char *params = *pparams;
  int16 open_mode = SVAL(params, 2);
  int16 open_attr = SVAL(params,6);
  BOOL oplock_request = (((SVAL(params,0)|(1<<1))>>1) | ((SVAL(params,0)|(1<<2))>>1));
#if 0
  BOOL return_additional_info = BITSETW(params,0);
  int16 open_sattr = SVAL(params, 4);
  time_t open_time = make_unix_date3(params+8);
#endif
  int16 open_ofun = SVAL(params,12);
  int32 open_size = IVAL(params,14);
  char *pname = &params[28];

  pstring fname;
  mode_t unixmode;
  SMB_OFF_T size=0;
  int fmode=0,mtime=0,rmode;
  SMB_INO_T inode = 0;
  SMB_STRUCT_STAT sbuf;
  int smb_action = 0;
  BOOL bad_path = False;
  files_struct *fsp;

  pstrcpy(fname,pname);

  DEBUG(3,("trans2open %s mode=%d attr=%d ofun=%d size=%d\n",
	   fname,open_mode, open_attr, open_ofun, open_size));

  /* XXXX we need to handle passed times, sattr and flags */

  unix_convert(fname,conn,0,&bad_path,NULL);
    
  fsp = file_new();
  if (!fsp)
    return(ERROR(ERRSRV,ERRnofids));

  if (!check_name(fname,conn))
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  unixmode = unix_mode(conn,open_attr | aARCH, fname);
      
  open_file_shared(fsp,conn,fname,open_mode,open_ofun,unixmode,
		   oplock_request, &rmode,&smb_action);
      
  if (!fsp->open)
  {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }
    file_free(fsp);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }

  if (sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
    close_file(fsp,False);
    return(UNIXERROR(ERRDOS,ERRnoaccess));
  }
    
  size = sbuf.st_size;
  fmode = dos_mode(conn,fname,&sbuf);
  mtime = sbuf.st_mtime;
  inode = sbuf.st_ino;
  if (fmode & aDIR) {
    close_file(fsp,False);
    return(ERROR(ERRDOS,ERRnoaccess));
  }

  /* Realloc the size of parameters and data we will return */
  params = *pparams = Realloc(*pparams, 28);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  memset((char *)params,'\0',28);
  SSVAL(params,0,fsp->fnum);
  SSVAL(params,2,fmode);
  put_dos_date2(params,4, mtime);
  SIVAL(params,8, (uint32)size);
  SSVAL(params,12,rmode);

  if (oplock_request && lp_fake_oplocks(SNUM(conn))) {
    smb_action |= EXTENDED_OPLOCK_GRANTED;
  }

  SSVAL(params,18,smb_action);
  /*
   * WARNING - this may need to be changed if SMB_INO_T <> 4 bytes.
   */
  SIVAL(params,20,inode);
 
  /* Send the required number of replies */
  send_trans2_replies(outbuf, bufsize, params, 28, *ppdata, 0);

  return -1;
}

/****************************************************************************
  get a level dependent lanman2 dir entry.
****************************************************************************/
static BOOL get_lanman2_dir_entry(connection_struct *conn,
				 char *path_mask,int dirtype,int info_level,
				 int requires_resume_key,
				 BOOL dont_descend,char **ppdata, 
				 char *base_data, int space_remaining, 
				 BOOL *out_of_space, BOOL *got_exact_match,
				 int *last_name_off)
{
  char *dname;
  BOOL found = False;
  SMB_STRUCT_STAT sbuf;
  pstring mask;
  pstring pathreal;
  pstring fname;
  char *p, *pdata = *ppdata;
  uint32 reskey=0;
  int prev_dirpos=0;
  int mode=0;
  SMB_OFF_T size = 0;
  uint32 len;
  time_t mdate=0, adate=0, cdate=0;
  char *nameptr;
  BOOL was_8_3;
  int nt_extmode; /* Used for NT connections instead of mode */
  BOOL needslash = ( conn->dirpath[strlen(conn->dirpath) -1] != '/');

  *fname = 0;
  *out_of_space = False;
  *got_exact_match = False;

  if (!conn->dirptr)
    return(False);

  p = strrchr(path_mask,'/');
  if(p != NULL)
  {
    if(p[1] == '\0')
      pstrcpy(mask,"*.*");
    else
      pstrcpy(mask, p+1);
  }
  else
    pstrcpy(mask, path_mask);

  while (!found)
  {
    BOOL got_match;

    /* Needed if we run out of space */
    prev_dirpos = TellDir(conn->dirptr);
    dname = ReadDirName(conn->dirptr);

    /*
     * Due to bugs in NT client redirectors we are not using
     * resume keys any more - set them to zero.
     * Check out the related comments in findfirst/findnext.
     * JRA.
     */

    reskey = 0;

    DEBUG(8,("get_lanman2_dir_entry:readdir on dirptr 0x%lx now at offset %d\n",
      (long)conn->dirptr,TellDir(conn->dirptr)));
      
    if (!dname) 
      return(False);

    pstrcpy(fname,dname);      

    if(!(got_match = *got_exact_match = exact_match(fname, mask, case_sensitive)))
      got_match = mask_match(fname, mask, case_sensitive, True);

    if(!got_match && !is_8_3(fname, False)) {

      /*
       * It turns out that NT matches wildcards against
       * both long *and* short names. This may explain some
       * of the wildcard wierdness from old DOS clients
       * that some people have been seeing.... JRA.
       */

      pstring newname;
      pstrcpy( newname, fname);
      name_map_mangle( newname, True, False, SNUM(conn));
      if(!(got_match = *got_exact_match = exact_match(newname, mask, case_sensitive)))
        got_match = mask_match(newname, mask, case_sensitive, True);
    }

    if(got_match)
    {
      BOOL isdots = (strequal(fname,"..") || strequal(fname,"."));
      if (dont_descend && !isdots)
        continue;
	  
      pstrcpy(pathreal,conn->dirpath);
      if(needslash)
        pstrcat(pathreal,"/");
      pstrcat(pathreal,dname);
      if (dos_stat(pathreal,&sbuf) != 0) 
      {
        DEBUG(5,("get_lanman2_dir_entry:Couldn't stat [%s] (%s)\n",pathreal,strerror(errno)));
        continue;
      }

      mode = dos_mode(conn,pathreal,&sbuf);

      if (!dir_check_ftype(conn,mode,&sbuf,dirtype)) {
        DEBUG(5,("[%s] attribs didn't match %x\n",fname,dirtype));
        continue;
      }

      size = sbuf.st_size;
      mdate = sbuf.st_mtime;
      adate = sbuf.st_atime;
      cdate = get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn)));
      if(mode & aDIR)
        size = 0;

      DEBUG(5,("get_lanman2_dir_entry found %s fname=%s\n",pathreal,fname));
	  
      found = True;
    }
  }

  name_map_mangle(fname,False,True,SNUM(conn));

  p = pdata;
  nameptr = p;

  nt_extmode = mode ? mode : FILE_ATTRIBUTE_NORMAL;

  switch (info_level)
  {
    case 1:
      if(requires_resume_key) {
        SIVAL(p,0,reskey);
        p += 4;
      }
      put_dos_date2(p,l1_fdateCreation,cdate);
      put_dos_date2(p,l1_fdateLastAccess,adate);
      put_dos_date2(p,l1_fdateLastWrite,mdate);
      SIVAL(p,l1_cbFile,(uint32)size);
      SIVAL(p,l1_cbFileAlloc,SMB_ROUNDUP(size,1024));
      SSVAL(p,l1_attrFile,mode);
      SCVAL(p,l1_cchName,strlen(fname));
      pstrcpy(p + l1_achName, fname);
      nameptr = p + l1_achName;
      p += l1_achName + strlen(fname) + 1;
      break;

    case 2:
      /* info_level 2 */
      if(requires_resume_key) {
        SIVAL(p,0,reskey);
        p += 4;
      }
      put_dos_date2(p,l2_fdateCreation,cdate);
      put_dos_date2(p,l2_fdateLastAccess,adate);
      put_dos_date2(p,l2_fdateLastWrite,mdate);
      SIVAL(p,l2_cbFile,(uint32)size);
      SIVAL(p,l2_cbFileAlloc,SMB_ROUNDUP(size,1024));
      SSVAL(p,l2_attrFile,mode);
      SIVAL(p,l2_cbList,0); /* No extended attributes */
      SCVAL(p,l2_cchName,strlen(fname));
      pstrcpy(p + l2_achName, fname);
      nameptr = p + l2_achName;
      p += l2_achName + strlen(fname) + 1;
      break;

    case 3:
      SIVAL(p,0,reskey);
      put_dos_date2(p,4,cdate);
      put_dos_date2(p,8,adate);
      put_dos_date2(p,12,mdate);
      SIVAL(p,16,(uint32)size);
      SIVAL(p,20,SMB_ROUNDUP(size,1024));
      SSVAL(p,24,mode);
      SIVAL(p,26,4);
      CVAL(p,30) = strlen(fname);
      pstrcpy(p+31, fname);
      nameptr = p+31;
      p += 31 + strlen(fname) + 1;
      break;

    case 4:
      if(requires_resume_key) {
        SIVAL(p,0,reskey);
        p += 4;
      }
      SIVAL(p,0,33+strlen(fname)+1);
      put_dos_date2(p,4,cdate);
      put_dos_date2(p,8,adate);
      put_dos_date2(p,12,mdate);
      SIVAL(p,16,(uint32)size);
      SIVAL(p,20,SMB_ROUNDUP(size,1024));
      SSVAL(p,24,mode);
      CVAL(p,32) = strlen(fname);
      pstrcpy(p + 33, fname);
      nameptr = p+33;
      p += 33 + strlen(fname) + 1;
      break;

    case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
      was_8_3 = is_8_3(fname, True);
      len = 94+strlen(fname);
      len = (len + 3) & ~3;
      SIVAL(p,0,len); p += 4;
      SIVAL(p,0,reskey); p += 4;
      put_long_date(p,cdate); p += 8;
      put_long_date(p,adate); p += 8;
      put_long_date(p,mdate); p += 8;
      put_long_date(p,mdate); p += 8;
      SOFF_T(p,0,size);
      SOFF_T(p,8,size);
      p += 16;
      SIVAL(p,0,nt_extmode); p += 4;
      SIVAL(p,0,strlen(fname)); p += 4;
      SIVAL(p,0,0); p += 4;
      if (!was_8_3) {
        pstrcpy(p+2,fname);
        if (!name_map_mangle(p+2,True,True,SNUM(conn)))
          (p+2)[12] = 0;
      } else
        *(p+2) = 0;
      strupper(p+2);
      SSVAL(p,0,strlen(p+2));
      p += 2 + 24;
      /* nameptr = p;  */
      pstrcpy(p,fname); p += strlen(p);
      p = pdata + len;
      break;

    case SMB_FIND_FILE_DIRECTORY_INFO:
      len = 64+strlen(fname);
      len = (len + 3) & ~3;
      SIVAL(p,0,len); p += 4;
      SIVAL(p,0,reskey); p += 4;
      put_long_date(p,cdate); p += 8;
      put_long_date(p,adate); p += 8;
      put_long_date(p,mdate); p += 8;
      put_long_date(p,mdate); p += 8;
      SOFF_T(p,0,size);
      SOFF_T(p,8,size);
      p += 16;
      SIVAL(p,0,nt_extmode); p += 4;
      SIVAL(p,0,strlen(fname)); p += 4;
      pstrcpy(p,fname);
      p = pdata + len;
      break;
      
      
    case SMB_FIND_FILE_FULL_DIRECTORY_INFO:
      len = 68+strlen(fname);
      len = (len + 3) & ~3;
      SIVAL(p,0,len); p += 4;
      SIVAL(p,0,reskey); p += 4;
      put_long_date(p,cdate); p += 8;
      put_long_date(p,adate); p += 8;
      put_long_date(p,mdate); p += 8;
      put_long_date(p,mdate); p += 8;
      SOFF_T(p,0,size); 
      SOFF_T(p,8,size);
      p += 16;
      SIVAL(p,0,nt_extmode); p += 4;
      SIVAL(p,0,strlen(fname)); p += 4;
      SIVAL(p,0,0); p += 4;
      pstrcpy(p,fname);
      p = pdata + len;
      break;

    case SMB_FIND_FILE_NAMES_INFO:
      len = 12+strlen(fname);
      len = (len + 3) & ~3;
      SIVAL(p,0,len); p += 4;
      SIVAL(p,0,reskey); p += 4;
      SIVAL(p,0,strlen(fname)); p += 4;
      pstrcpy(p,fname);
      p = pdata + len;
      break;

    default:      
      return(False);
    }


  if (PTR_DIFF(p,pdata) > space_remaining) {
    /* Move the dirptr back to prev_dirpos */
    SeekDir(conn->dirptr, prev_dirpos);
    *out_of_space = True;
    DEBUG(9,("get_lanman2_dir_entry: out of space\n"));
    return False; /* Not finished - just out of space */
  }

  /* Setup the last_filename pointer, as an offset from base_data */
  *last_name_off = PTR_DIFF(nameptr,base_data);
  /* Advance the data pointer to the next slot */
  *ppdata = p;

  return(found);
}
  
/****************************************************************************
 Convert the directory masks formated for the wire.
****************************************************************************/

void mask_convert( char *mask)
{
  /*
   * We know mask is a pstring.
   */
  char *p = mask;
  while (*p) {
    if (*p == '<') {
      pstring expnd;
      if(p[1] != '"' && p[1] != '.') {
        pstrcpy( expnd, p+1 );
        *p++ = '*';
        *p = '.';
        safe_strcpy( p+1, expnd, sizeof(pstring) - (p - mask) - 2);
      } else
        *p = '*';
    }
    if (*p == '>') *p = '?';
    if (*p == '"') *p = '.';
    p++;
  }
}

/****************************************************************************
 Reply to a TRANS2_FINDFIRST.
****************************************************************************/

static int call_trans2findfirst(connection_struct *conn,
				char *inbuf, char *outbuf, int bufsize,  
				char **pparams, char **ppdata)
{
  /* We must be careful here that we don't return more than the
     allowed number of data bytes. If this means returning fewer than
     maxentries then so be it. We assume that the redirector has
     enough room for the fixed number of parameter bytes it has
     requested. */
  uint32 max_data_bytes = SVAL(inbuf, smb_mdrcnt);
  char *params = *pparams;
  char *pdata = *ppdata;
  int dirtype = SVAL(params,0);
  int maxentries = SVAL(params,2);
  BOOL close_after_first = BITSETW(params+4,0);
  BOOL close_if_end = BITSETW(params+4,1);
  BOOL requires_resume_key = BITSETW(params+4,2);
  int info_level = SVAL(params,6);
  pstring directory;
  pstring mask;
  char *p, *wcard;
  int last_name_off=0;
  int dptr_num = -1;
  int numentries = 0;
  int i;
  BOOL finished = False;
  BOOL dont_descend = False;
  BOOL out_of_space = False;
  int space_remaining;
  BOOL bad_path = False;

  *directory = *mask = 0;

  DEBUG(3,("call_trans2findfirst: dirtype = %d, maxentries = %d, close_after_first=%d, close_if_end = %d requires_resume_key = %d level = %d, max_data_bytes = %d\n",
	   dirtype, maxentries, close_after_first, close_if_end, requires_resume_key,
	   info_level, max_data_bytes));
  
  switch (info_level) 
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case SMB_FIND_FILE_DIRECTORY_INFO:
    case SMB_FIND_FILE_FULL_DIRECTORY_INFO:
    case SMB_FIND_FILE_NAMES_INFO:
    case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
      break;
    default:
      return(ERROR(ERRDOS,ERRunknownlevel));
    }

  pstrcpy(directory, params + 12); /* Complete directory path with 
				     wildcard mask appended */

  DEBUG(5,("path=%s\n",directory));

  unix_convert(directory,conn,0,&bad_path,NULL);
  if(!check_name(directory,conn)) {
    if((errno == ENOENT) && bad_path)
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbadpath;
    }

#if 0
    /* Ugly - NT specific hack - maybe not needed ? (JRA) */
    if((errno == ENOTDIR) && (Protocol >= PROTOCOL_NT1) && 
       (get_remote_arch() == RA_WINNT))
    {
      unix_ERR_class = ERRDOS;
      unix_ERR_code = ERRbaddirectory;
    }
#endif 

    return(UNIXERROR(ERRDOS,ERRbadpath));
  }

  p = strrchr(directory,'/');
  if(p == NULL) {
    pstrcpy(mask,directory);
    pstrcpy(directory,"./");
  } else {
    pstrcpy(mask,p+1);
    *p = 0;
  }

  DEBUG(5,("dir=%s, mask = %s\n",directory, mask));

  pdata = *ppdata = Realloc(*ppdata, max_data_bytes + 1024);
  if(!*ppdata)
    return(ERROR(ERRDOS,ERRnomem));
  memset((char *)pdata,'\0',max_data_bytes + 1024);

  /* Realloc the params space */
  params = *pparams = Realloc(*pparams, 10);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  dptr_num = dptr_create(conn,directory, False, True ,SVAL(inbuf,smb_pid));
  if (dptr_num < 0)
    return(UNIXERROR(ERRDOS,ERRbadfile));

  /* Convert the formatted mask. */
  mask_convert(mask);

  /* Save the wildcard match and attribs we are using on this directory - 
     needed as lanman2 assumes these are being saved between calls */

  if(!(wcard = strdup(mask))) {
    dptr_close(&dptr_num);
    return(ERROR(ERRDOS,ERRnomem));
  }

  dptr_set_wcard(dptr_num, wcard);
  dptr_set_attr(dptr_num, dirtype);

  DEBUG(4,("dptr_num is %d, wcard = %s, attr = %d\n",dptr_num, wcard, dirtype));

  /* We don't need to check for VOL here as this is returned by 
     a different TRANS2 call. */
  
  DEBUG(8,("dirpath=<%s> dontdescend=<%s>\n",
	   conn->dirpath,lp_dontdescend(SNUM(conn))));
  if (in_list(conn->dirpath,lp_dontdescend(SNUM(conn)),case_sensitive))
    dont_descend = True;
    
  p = pdata;
  space_remaining = max_data_bytes;
  out_of_space = False;

  for (i=0;(i<maxentries) && !finished && !out_of_space;i++)
  {
    BOOL got_exact_match = False;

    /* this is a heuristic to avoid seeking the dirptr except when 
       absolutely necessary. It allows for a filename of about 40 chars */
    if (space_remaining < DIRLEN_GUESS && numentries > 0)
    {
      out_of_space = True;
      finished = False;
    }
    else
    {
      finished = !get_lanman2_dir_entry(conn,mask,dirtype,info_level,
                   requires_resume_key,dont_descend,
                   &p,pdata,space_remaining, &out_of_space, &got_exact_match,
                   &last_name_off);
    }

    if (finished && out_of_space)
      finished = False;

    if (!finished && !out_of_space)
      numentries++;

    /*
     * As an optimisation if we know we aren't looking
     * for a wildcard name (ie. the name matches the wildcard exactly)
     * then we can finish on any (first) match.
     * This speeds up large directory searches. JRA.
     */

    if(got_exact_match)
      finished = True;

    space_remaining = max_data_bytes - PTR_DIFF(p,pdata);
  }
  
  /* Check if we can close the dirptr */
  if(close_after_first || (finished && close_if_end))
  {
    DEBUG(5,("call_trans2findfirst - (2) closing dptr_num %d\n", dptr_num));
    dptr_close(&dptr_num);
  }

  /* 
   * If there are no matching entries we must return ERRDOS/ERRbadfile - 
   * from observation of NT.
   */

  if(numentries == 0)
  {
    dptr_close(&dptr_num);
    return(ERROR(ERRDOS,ERRbadfile));
  }

  /* At this point pdata points to numentries directory entries. */

  /* Set up the return parameter block */
  SSVAL(params,0,dptr_num);
  SSVAL(params,2,numentries);
  SSVAL(params,4,finished);
  SSVAL(params,6,0); /* Never an EA error */
  SSVAL(params,8,last_name_off);

  send_trans2_replies( outbuf, bufsize, params, 10, pdata, PTR_DIFF(p,pdata));

  if ((! *directory) && dptr_path(dptr_num))
    slprintf(directory,sizeof(directory)-1, "(%s)",dptr_path(dptr_num));

  DEBUG( 4, ( "%s mask=%s directory=%s dirtype=%d numentries=%d\n",
	    smb_fn_name(CVAL(inbuf,smb_com)), 
	    mask, directory, dirtype, numentries ) );

  /* 
   * Force a name mangle here to ensure that the
   * mask as an 8.3 name is top of the mangled cache.
   * The reasons for this are subtle. Don't remove
   * this code unless you know what you are doing
   * (see PR#13758). JRA.
   */

  if(!is_8_3( mask, False))
    name_map_mangle(mask, True, True, SNUM(conn));

  return(-1);
}


/****************************************************************************
  reply to a TRANS2_FINDNEXT
****************************************************************************/
static int call_trans2findnext(connection_struct *conn, 
			       char *inbuf, char *outbuf, 
			       int length, int bufsize,
			       char **pparams, char **ppdata)
{
  /* We must be careful here that we don't return more than the
     allowed number of data bytes. If this means returning fewer than
     maxentries then so be it. We assume that the redirector has
     enough room for the fixed number of parameter bytes it has
     requested. */
  int max_data_bytes = SVAL(inbuf, smb_mdrcnt);
  char *params = *pparams;
  char *pdata = *ppdata;
  int dptr_num = SVAL(params,0);
  int maxentries = SVAL(params,2);
  uint16 info_level = SVAL(params,4);
  uint32 resume_key = IVAL(params,6);
  BOOL close_after_request = BITSETW(params+10,0);
  BOOL close_if_end = BITSETW(params+10,1);
  BOOL requires_resume_key = BITSETW(params+10,2);
  BOOL continue_bit = BITSETW(params+10,3);
  pstring resume_name;
  pstring mask;
  pstring directory;
  char *p;
  uint16 dirtype;
  int numentries = 0;
  int i, last_name_off=0;
  BOOL finished = False;
  BOOL dont_descend = False;
  BOOL out_of_space = False;
  int space_remaining;

  *mask = *directory = *resume_name = 0;

  pstrcpy( resume_name, params+12);

  DEBUG(3,("call_trans2findnext: dirhandle = %d, max_data_bytes = %d, maxentries = %d, \
close_after_request=%d, close_if_end = %d requires_resume_key = %d \
resume_key = %d resume name = %s continue=%d level = %d\n",
	   dptr_num, max_data_bytes, maxentries, close_after_request, close_if_end, 
	   requires_resume_key, resume_key, resume_name, continue_bit, info_level));

  switch (info_level) 
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case SMB_FIND_FILE_DIRECTORY_INFO:
    case SMB_FIND_FILE_FULL_DIRECTORY_INFO:
    case SMB_FIND_FILE_NAMES_INFO:
    case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
      break;
    default:
      return(ERROR(ERRDOS,ERRunknownlevel));
    }

  pdata = *ppdata = Realloc( *ppdata, max_data_bytes + 1024);
  if(!*ppdata)
    return(ERROR(ERRDOS,ERRnomem));
  memset((char *)pdata,'\0',max_data_bytes + 1024);

  /* Realloc the params space */
  params = *pparams = Realloc(*pparams, 6*SIZEOFWORD);
  if(!params)
    return(ERROR(ERRDOS,ERRnomem));

  /* Check that the dptr is valid */
  if(!(conn->dirptr = dptr_fetch_lanman2(dptr_num)))
    return(ERROR(ERRDOS,ERRnofiles));

  string_set(&conn->dirpath,dptr_path(dptr_num));

  /* Get the wildcard mask from the dptr */
  if((p = dptr_wcard(dptr_num))== NULL) {
    DEBUG(2,("dptr_num %d has no wildcard\n", dptr_num));
    return (ERROR(ERRDOS,ERRnofiles));
  }
  pstrcpy(mask, p);
  pstrcpy(directory,conn->dirpath);

  /* Get the attr mask from the dptr */
  dirtype = dptr_attr(dptr_num);

  DEBUG(3,("dptr_num is %d, mask = %s, attr = %x, dirptr=(0x%lX,%d)\n",
	   dptr_num, mask, dirtype, 
	   (long)conn->dirptr,
	   TellDir(conn->dirptr)));

  /* We don't need to check for VOL here as this is returned by 
     a different TRANS2 call. */

  DEBUG(8,("dirpath=<%s> dontdescend=<%s>\n",conn->dirpath,lp_dontdescend(SNUM(conn))));
  if (in_list(conn->dirpath,lp_dontdescend(SNUM(conn)),case_sensitive))
    dont_descend = True;
    
  p = pdata;
  space_remaining = max_data_bytes;
  out_of_space = False;

  /* 
   * Seek to the correct position. We no longer use the resume key but
   * depend on the last file name instead.
   */
  if(requires_resume_key && *resume_name && !continue_bit)
  {
    /*
     * Fix for NT redirector problem triggered by resume key indexes
     * changing between directory scans. We now return a resume key of 0
     * and instead look for the filename to continue from (also given
     * to us by NT/95/smbfs/smbclient). If no other scans have been done between the
     * findfirst/findnext (as is usual) then the directory pointer
     * should already be at the correct place. Check this by scanning
     * backwards looking for an exact (ie. case sensitive) filename match. 
     * If we get to the beginning of the directory and haven't found it then scan
     * forwards again looking for a match. JRA.
     */

    int current_pos, start_pos;
    char *dname = NULL;
    void *dirptr = conn->dirptr;
    start_pos = TellDir(dirptr);
    for(current_pos = start_pos; current_pos >= 0; current_pos--)
    {
      DEBUG(7,("call_trans2findnext: seeking to pos %d\n", current_pos));

      SeekDir(dirptr, current_pos);
      dname = ReadDirName(dirptr);

      /*
       * Remember, name_map_mangle is called by
       * get_lanman2_dir_entry(), so the resume name
       * could be mangled. Ensure we do the same
       * here.
       */

      if(dname != NULL)
        name_map_mangle( dname, False, True, SNUM(conn));

      if(dname && strcsequal( resume_name, dname))
      {
        SeekDir(dirptr, current_pos+1);
        DEBUG(7,("call_trans2findnext: got match at pos %d\n", current_pos+1 ));
        break;
      }
    }

    /*
     * Scan forward from start if not found going backwards.
     */

    if(current_pos < 0)
    {
      DEBUG(7,("call_trans2findnext: notfound: seeking to pos %d\n", start_pos));
      SeekDir(dirptr, start_pos);
      for(current_pos = start_pos; (dname = ReadDirName(dirptr)) != NULL; SeekDir(dirptr,++current_pos))
      {
        /*
         * Remember, name_map_mangle is called by
         * get_lanman2_dir_entry(), so the resume name
         * could be mangled. Ensure we do the same
         * here.
         */

        if(dname != NULL)
          name_map_mangle( dname, False, True, SNUM(conn));

        if(dname && strcsequal( resume_name, dname))
        {
          SeekDir(dirptr, current_pos+1);
          DEBUG(7,("call_trans2findnext: got match at pos %d\n", current_pos+1 ));
          break;
        }
      } /* end for */
    } /* end if current_pos */
  } /* end if requires_resume_key && !continue_bit */

  for (i=0;(i<(int)maxentries) && !finished && !out_of_space ;i++)
  {
    BOOL got_exact_match = False;

    /* this is a heuristic to avoid seeking the dirptr except when 
       absolutely necessary. It allows for a filename of about 40 chars */
    if (space_remaining < DIRLEN_GUESS && numentries > 0)
    {
      out_of_space = True;
      finished = False;
    }
    else
    {
      finished = !get_lanman2_dir_entry(conn,mask,dirtype,info_level,
                   requires_resume_key,dont_descend,
                   &p,pdata,space_remaining, &out_of_space, &got_exact_match,
                   &last_name_off);
    }

    if (finished && out_of_space)
      finished = False;

    if (!finished && !out_of_space)
      numentries++;

    /*
     * As an optimisation if we know we aren't looking
     * for a wildcard name (ie. the name matches the wildcard exactly)
     * then we can finish on any (first) match.
     * This speeds up large directory searches. JRA.
     */

    if(got_exact_match)
      finished = True;

    space_remaining = max_data_bytes - PTR_DIFF(p,pdata);
  }
  
  /* Check if we can close the dirptr */
  if(close_after_request || (finished && close_if_end))
  {
    DEBUG(5,("call_trans2findnext: closing dptr_num = %d\n", dptr_num));
    dptr_close(&dptr_num); /* This frees up the saved mask */
  }


  /* Set up the return parameter block */
  SSVAL(params,0,numentries);
  SSVAL(params,2,finished);
  SSVAL(params,4,0); /* Never an EA error */
  SSVAL(params,6,last_name_off);

  send_trans2_replies( outbuf, bufsize, params, 8, pdata, PTR_DIFF(p,pdata));

  if ((! *directory) && dptr_path(dptr_num))
    slprintf(directory,sizeof(directory)-1, "(%s)",dptr_path(dptr_num));

  DEBUG( 3, ( "%s mask=%s directory=%s dirtype=%d numentries=%d\n",
	    smb_fn_name(CVAL(inbuf,smb_com)), 
	    mask, directory, dirtype, numentries ) );

  return(-1);
}

/****************************************************************************
  reply to a TRANS2_QFSINFO (query filesystem info)
****************************************************************************/

static int call_trans2qfsinfo(connection_struct *conn, 
			      char *inbuf, char *outbuf, 
			      int length, int bufsize,
			      char **pparams, char **ppdata)
{
  int max_data_bytes = SVAL(inbuf, smb_mdrcnt);
  char *pdata = *ppdata;
  char *params = *pparams;
  uint16 info_level = SVAL(params,0);
  int data_len;
  SMB_STRUCT_STAT st;
  char *vname = volume_label(SNUM(conn));
  int snum = SNUM(conn);
  char *fstype = lp_fstype(SNUM(conn));

  DEBUG(3,("call_trans2qfsinfo: level = %d\n", info_level));

  if(dos_stat(".",&st)!=0) {
    DEBUG(2,("call_trans2qfsinfo: stat of . failed (%s)\n", strerror(errno)));
    return (ERROR(ERRSRV,ERRinvdevice));
  }

  pdata = *ppdata = Realloc(*ppdata, max_data_bytes + 1024);
  memset((char *)pdata,'\0',max_data_bytes + 1024);

  switch (info_level) 
  {
    case 1:
    {
      SMB_BIG_UINT dfree,dsize,bsize;
      data_len = 18;
      sys_disk_free(".",False,&bsize,&dfree,&dsize);	
      SIVAL(pdata,l1_idFileSystem,st.st_dev);
      SIVAL(pdata,l1_cSectorUnit,bsize/512);
      SIVAL(pdata,l1_cUnit,dsize);
      SIVAL(pdata,l1_cUnitAvail,dfree);
      SSVAL(pdata,l1_cbSector,512);
      DEBUG(5,("call_trans2qfsinfo : bsize=%u, id=%x, cSectorUnit=%u, cUnit=%u, cUnitAvail=%u, cbSector=%d\n",
		 (unsigned int)bsize, (unsigned int)st.st_dev, ((unsigned int)bsize)/512, (unsigned int)dsize,
         (unsigned int)dfree, 512));
      break;
    }
    case 2:
    { 
      /* Return volume name */
      int volname_len = MIN(strlen(vname),11);
      data_len = l2_vol_szVolLabel + volname_len + 1;
      /* 
       * Add volume serial number - hash of a combination of
       * the called hostname and the service name.
       */
      SIVAL(pdata,0,str_checksum(lp_servicename(snum)) ^ (str_checksum(local_machine)<<16) );
      SCVAL(pdata,l2_vol_cch,volname_len);
      StrnCpy(pdata+l2_vol_szVolLabel,vname,volname_len);
      DEBUG(5,("call_trans2qfsinfo : time = %x, namelen = %d, name = %s\n",
	       (unsigned)st.st_ctime, volname_len,
	       pdata+l2_vol_szVolLabel));
      break;
    }
    case SMB_QUERY_FS_ATTRIBUTE_INFO:
    {
      int fstype_len;
      SIVAL(pdata,0,FILE_CASE_PRESERVED_NAMES|FILE_CASE_SENSITIVE_SEARCH|
			FILE_DEVICE_IS_MOUNTED|
            (lp_nt_acl_support() ? FILE_PERSISTENT_ACLS : 0)); /* FS ATTRIBUTES */
#if 0 /* Old code. JRA. */
      SIVAL(pdata,0,0x4006); /* FS ATTRIBUTES == long filenames supported? */
#endif /* Old code. */

      SIVAL(pdata,4,128); /* Max filename component length */
      fstype_len = dos_PutUniCode(pdata+12,unix_to_dos(fstype,False),sizeof(pstring), False);
      SIVAL(pdata,8,fstype_len);
      data_len = 12 + fstype_len;
      SSVAL(outbuf,smb_flg2,SVAL(outbuf,smb_flg2)|FLAGS2_UNICODE_STRINGS);
      break;
    }
    case SMB_QUERY_FS_LABEL_INFO:
      data_len = 4 + strlen(vname);
      SIVAL(pdata,0,strlen(vname));
      pstrcpy(pdata+4,vname);      
      break;
    case SMB_QUERY_FS_VOLUME_INFO:      

      /* 
       * Add volume serial number - hash of a combination of
       * the called hostname and the service name.
       */
      SIVAL(pdata,8,str_checksum(lp_servicename(snum)) ^ 
	    (str_checksum(local_machine)<<16));

      /* NT4 always serves this up as unicode but expects it to be
       * delivered as ascii! (tridge && JRA)
       */
      if ((get_remote_arch() != RA_WIN2K) && (global_client_caps & CAP_NT_SMBS)) {
	      data_len = 18 + strlen(vname);
	      SIVAL(pdata,12,strlen(vname));
	      pstrcpy(pdata+18,vname);      
      } else {
	      data_len = 18 + 2*strlen(vname);
	      SIVAL(pdata,12,strlen(vname)*2);
	      dos_PutUniCode(pdata+18,unix_to_dos(vname,False),sizeof(pstring), False);
		  SSVAL(outbuf,smb_flg2,SVAL(outbuf,smb_flg2)|FLAGS2_UNICODE_STRINGS);
      }

      DEBUG(5,("call_trans2qfsinfo : SMB_QUERY_FS_VOLUME_INFO namelen = %d, vol = %s\n", 
	       (int)strlen(vname),vname));
      break;
    case SMB_QUERY_FS_SIZE_INFO:
    {
      SMB_BIG_UINT dfree,dsize,bsize;
      data_len = 24;
      sys_disk_free(".",False,&bsize,&dfree,&dsize);	
      SBIG_UINT(pdata,0,dsize);
      SBIG_UINT(pdata,8,dfree);
      SIVAL(pdata,16,bsize/512);
      SIVAL(pdata,20,512);
      break;
    }
    case SMB_QUERY_FS_DEVICE_INFO:
      data_len = 8;
      SIVAL(pdata,0,0); /* dev type */
      SIVAL(pdata,4,0); /* characteristics */
      break;
    case SMB_MAC_QUERY_FS_INFO:
	    /*
	     * Thursby MAC extension... ONLY on NTFS filesystems
	     * once we do streams then we don't need this
	     */
	    if (strequal(lp_fstype(SNUM(conn)),"NTFS")) {
		    data_len = 88;
		    SIVAL(pdata,84,0x100); /* Don't support mac... */
		    break;
	    }
	    /* drop through */
    default:
      return(ERROR(ERRDOS,ERRunknownlevel));
  }


  send_trans2_replies( outbuf, bufsize, params, 0, pdata, data_len);

  DEBUG( 4, ( "%s info_level = %d\n",
            smb_fn_name(CVAL(inbuf,smb_com)), info_level) );

  return -1;
}

/****************************************************************************
  reply to a TRANS2_SETFSINFO (set filesystem info)
****************************************************************************/
static int call_trans2setfsinfo(connection_struct *conn,
				char *inbuf, char *outbuf, int length, 
				int bufsize,
				char **pparams, char **ppdata)
{
  /* Just say yes we did it - there is nothing that
     can be set here so it doesn't matter. */
  int outsize;
  DEBUG(3,("call_trans2setfsinfo\n"));

  if (!CAN_WRITE(conn))
    return(ERROR(ERRSRV,ERRaccess));

  outsize = set_message(outbuf,10,0,True);

  return outsize;
}

/****************************************************************************
  Reply to a TRANS2_QFILEPATHINFO or TRANSACT2_QFILEINFO (query file info by
  file name or file id).
****************************************************************************/

static int call_trans2qfilepathinfo(connection_struct *conn,
				    char *inbuf, char *outbuf, int length, 
				    int bufsize,
				    char **pparams,char **ppdata,
				    int total_data)
{
  int max_data_bytes = SVAL(inbuf, smb_mdrcnt);
  char *params = *pparams;
  char *pdata = *ppdata;
  uint16 tran_call = SVAL(inbuf, smb_setup0);
  uint16 info_level;
  int mode=0;
  SMB_OFF_T size=0;
  unsigned int data_size;
  SMB_STRUCT_STAT sbuf;
  pstring fname1;
  char *fname;
  char *p;
  int l;
  SMB_OFF_T pos = 0;
  BOOL bad_path = False;
  BOOL delete_pending = False;

  if (tran_call == TRANSACT2_QFILEINFO) {
    files_struct *fsp = file_fsp(params,0);
    info_level = SVAL(params,2);

    DEBUG(3,("call_trans2qfilepathinfo: TRANSACT2_QFILEINFO: level = %d\n", info_level));

    if(fsp && fsp->open && (fsp->is_directory || fsp->stat_open)) {
      /*
       * This is actually a QFILEINFO on a directory
       * handle (returned from an NT SMB). NT5.0 seems
       * to do this call. JRA.
       */
      fname = fsp->fsp_name;
      unix_convert(fname,conn,0,&bad_path,&sbuf);
      if (!check_name(fname,conn) || (!VALID_STAT(sbuf) && dos_stat(fname,&sbuf))) {
        DEBUG(3,("fileinfo of %s failed (%s)\n",fname,strerror(errno)));
        if((errno == ENOENT) && bad_path)
        {
          unix_ERR_class = ERRDOS;
          unix_ERR_code = ERRbadpath;
        }
        return(UNIXERROR(ERRDOS,ERRbadpath));
      }

      delete_pending = fsp->directory_delete_on_close;

    } else {
      /*
       * Original code - this is an open file.
       */
      CHECK_FSP(fsp,conn);
      CHECK_ERROR(fsp);

      fname = fsp->fsp_name;
      if (sys_fstat(fsp->fd_ptr->fd,&sbuf) != 0) {
        DEBUG(3,("fstat of fnum %d failed (%s)\n",fsp->fnum, strerror(errno)));
        return(UNIXERROR(ERRDOS,ERRbadfid));
      }
      if((pos = sys_lseek(fsp->fd_ptr->fd,0,SEEK_CUR)) == -1)
        return(UNIXERROR(ERRDOS,ERRnoaccess));

      delete_pending = fsp->fd_ptr->delete_on_close;
    }
  } else {
    /* qpathinfo */
    info_level = SVAL(params,0);

    DEBUG(3,("call_trans2qfilepathinfo: TRANSACT2_QPATHINFO: level = %d\n", info_level));

    fname = &fname1[0];
    pstrcpy(fname,&params[6]);
    unix_convert(fname,conn,0,&bad_path,&sbuf);
    if (!check_name(fname,conn) || (!VALID_STAT(sbuf) && dos_stat(fname,&sbuf))) {
      DEBUG(3,("fileinfo of %s failed (%s)\n",fname,strerror(errno)));
      if((errno == ENOENT) && bad_path)
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,ERRbadpath));
    }
  }


  DEBUG(3,("call_trans2qfilepathinfo %s level=%d call=%d total_data=%d\n",
	   fname,info_level,tran_call,total_data));

  p = strrchr(fname,'/'); 
  if (!p) 
    p = fname;
  else
    p++;
  l = strlen(p);  
  mode = dos_mode(conn,fname,&sbuf);
  size = sbuf.st_size;
  if (mode & aDIR) size = 0;

  /* from now on we only want the part after the / */
  fname = p;
  
  params = *pparams = Realloc(*pparams,2);
  memset((char *)params,'\0',2);
  data_size = max_data_bytes + 1024;
  pdata = *ppdata = Realloc(*ppdata, data_size); 

#if 0
  if (total_data > 0 && IVAL(pdata,0) == total_data) {
    /* uggh, EAs for OS2 */
    DEBUG(4,("Rejecting EA request with total_data=%d\n",total_data));
    return(ERROR(ERRDOS,ERROR_EAS_NOT_SUPPORTED));
  }
#endif

  memset((char *)pdata,'\0',data_size);

  switch (info_level) 
    {
    case SMB_INFO_STANDARD:
    case SMB_INFO_QUERY_EA_SIZE:
      data_size = (info_level==1?22:26);
      put_dos_date2(pdata,l1_fdateCreation,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
      put_dos_date2(pdata,l1_fdateLastAccess,sbuf.st_atime);
      put_dos_date2(pdata,l1_fdateLastWrite,sbuf.st_mtime); /* write time */
      SIVAL(pdata,l1_cbFile,(uint32)size);
      SIVAL(pdata,l1_cbFileAlloc,SMB_ROUNDUP(size,1024));
      SSVAL(pdata,l1_attrFile,mode);
      SIVAL(pdata,l1_attrFile+2,4); /* this is what OS2 does */
      break;

    case SMB_INFO_QUERY_EAS_FROM_LIST:
      data_size = 24;
      put_dos_date2(pdata,0,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
      put_dos_date2(pdata,4,sbuf.st_atime);
      put_dos_date2(pdata,8,sbuf.st_mtime);
      SIVAL(pdata,12,(uint32)size);
      SIVAL(pdata,16,SMB_ROUNDUP(size,1024));
      SIVAL(pdata,20,mode);
      break;

    case SMB_INFO_QUERY_ALL_EAS:
      data_size = 4;
      SIVAL(pdata,0,data_size);
      break;

    case 6:
      return(ERROR(ERRDOS,ERRbadfunc)); /* os/2 needs this */      

    case SMB_QUERY_FILE_BASIC_INFO:
      data_size = 36; /* w95 returns 40 bytes not 36 - why ?. */
      put_long_date(pdata,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
      put_long_date(pdata+8,sbuf.st_atime);
      put_long_date(pdata+16,sbuf.st_mtime); /* write time */
      put_long_date(pdata+24,sbuf.st_mtime); /* change time */
      SIVAL(pdata,32,mode);

      DEBUG(5,("SMB_QFBI - "));
      {
        time_t create_time = get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn)));
        DEBUG(5,("create: %s ", ctime(&create_time)));
      }
      DEBUG(5,("access: %s ", ctime(&sbuf.st_atime)));
      DEBUG(5,("write: %s ", ctime(&sbuf.st_mtime)));
      DEBUG(5,("change: %s ", ctime(&sbuf.st_mtime)));
      DEBUG(5,("mode: %x\n", mode));

      break;

    case SMB_QUERY_FILE_STANDARD_INFO:
      data_size = 22;
      SOFF_T(pdata,0,size);
      SOFF_T(pdata,8,size);
      SIVAL(pdata,16,sbuf.st_nlink);
      CVAL(pdata,20) = 0;
      CVAL(pdata,21) = (mode&aDIR)?1:0;
      break;

    case SMB_QUERY_FILE_EA_INFO:
      data_size = 4;
      break;

    /* Get the 8.3 name - used if NT SMB was negotiated. */
    case SMB_QUERY_FILE_ALT_NAME_INFO:
      {
        pstring short_name;
        pstrcpy(short_name,p);
        /* Mangle if not already 8.3 */
        if(!is_8_3(short_name, True))
        {
          if(!name_map_mangle(short_name,True,True,SNUM(conn)))
            *short_name = '\0';
        }
        strupper(short_name);
        l = strlen(short_name);
        dos_PutUniCode(pdata + 4, unix_to_dos(short_name,False),sizeof(pstring), False);
        data_size = 4 + (2*l);
        SIVAL(pdata,0,2*l);
      }
      break;

    case SMB_QUERY_FILE_NAME_INFO:
      /*
       * The first part of this code is essential
       * to get security descriptors to work on mapped
       * drives. Don't ask how I discovered this unless
       * you like hearing about me suffering.... :-). JRA.
       */
      if(strequal(".", fname) && (global_client_caps & CAP_UNICODE)) {
        l = l*2;
        SSVAL(outbuf,smb_flg2,SVAL(outbuf,smb_flg2)|FLAGS2_UNICODE_STRINGS);
        dos_PutUniCode(pdata + 4, unix_to_dos("\\",False),sizeof(pstring), False);
      } else {
        pstrcpy(pdata+4,fname);
      }
      data_size = 4 + l;
      SIVAL(pdata,0,l);
      break;

    case SMB_QUERY_FILE_ALLOCATION_INFO:
    case SMB_QUERY_FILE_END_OF_FILEINFO:
      data_size = 8;
      SOFF_T(pdata,0,size);
      break;

    case SMB_QUERY_FILE_ALL_INFO:
      put_long_date(pdata,get_create_time(&sbuf,lp_fake_dir_create_times(SNUM(conn))));
      put_long_date(pdata+8,sbuf.st_atime);
      put_long_date(pdata+16,sbuf.st_mtime); /* write time */
      put_long_date(pdata+24,sbuf.st_mtime); /* change time */
      SIVAL(pdata,32,mode);
      pdata += 40;
      SOFF_T(pdata,0,size);
      SOFF_T(pdata,8,size);
      SIVAL(pdata,16,sbuf.st_nlink);
      CVAL(pdata,20) = delete_pending;
      CVAL(pdata,21) = (mode&aDIR)?1:0;
      pdata += 24;
      SINO_T(pdata,0,(SMB_INO_T)sbuf.st_ino); 
      pdata += 8; /* index number */
      pdata += 4; /* EA info */
      if (mode & aRONLY)
        SIVAL(pdata,0,0xA9);
      else
        SIVAL(pdata,0,0xd01BF);
      pdata += 4;
      SOFF_T(pdata,0,pos); /* current offset */
      pdata += 8;
      SIVAL(pdata,0,mode); /* is this the right sort of mode info? */
      pdata += 4;
      pdata += 4; /* alignment */
      SIVAL(pdata,0,l);
      pstrcpy(pdata+4,fname);
      pdata += 4 + l;
      data_size = PTR_DIFF(pdata,(*ppdata));
      break;

#if 0
      /* NT4 server just returns "invalid query" to this - if we try to answer 
	 it then NTws gets a BSOD! (tridge) */
    case SMB_QUERY_FILE_STREAM_INFO:
      data_size = 24 + l;
      SIVAL(pdata,0,pos);
      SIVAL(pdata,4,size);
      SIVAL(pdata,12,size);
      SIVAL(pdata,20,l);	
      pstrcpy(pdata+24,fname);
      break;
#endif

    default:
      return(ERROR(ERRDOS,ERRunknownlevel));
    }

  send_trans2_replies( outbuf, bufsize, params, 2, *ppdata, data_size);

  return(-1);
}

/****************************************************************************
  reply to a TRANS2_SETFILEINFO (set file info by fileid)
****************************************************************************/
static int call_trans2setfilepathinfo(connection_struct *conn,
				      char *inbuf, char *outbuf, int length, 
				      int bufsize, char **pparams, 
				      char **ppdata, int total_data)
{
  char *params = *pparams;
  char *pdata = *ppdata;
  uint16 tran_call = SVAL(inbuf, smb_setup0);
  uint16 info_level;
  int mode=0;
  SMB_OFF_T size=0;
  struct utimbuf tvs;
  SMB_STRUCT_STAT st;
  pstring fname1;
  char *fname;
  int fd = -1;
  BOOL bad_path = False;
  files_struct *fsp = NULL;

  if (!CAN_WRITE(conn))
    return(ERROR(ERRSRV,ERRaccess));

  if (tran_call == TRANSACT2_SETFILEINFO) {
    fsp = file_fsp(params,0);
    info_level = SVAL(params,2);    

    if(fsp && fsp->open && (fsp->is_directory || fsp->stat_open)) {
      /*
       * This is actually a SETFILEINFO on a directory
       * handle (returned from an NT SMB). NT5.0 seems
       * to do this call. JRA.
       */
      fname = fsp->fsp_name;
      unix_convert(fname,conn,0,&bad_path,&st);
      if (!check_name(fname,conn) || (!VALID_STAT(st) && dos_stat(fname,&st))) {
        DEBUG(3,("fileinfo of %s failed (%s)\n",fname,strerror(errno)));
        if((errno == ENOENT) && bad_path)
        {
          unix_ERR_class = ERRDOS;
          unix_ERR_code = ERRbadpath;
        }
        return(UNIXERROR(ERRDOS,ERRbadpath));
      }
    } else {
      /*
       * Original code - this is an open file.
       */
      CHECK_FSP(fsp,conn);
      CHECK_ERROR(fsp);

      fname = fsp->fsp_name;
      fd = fsp->fd_ptr->fd;

      if (sys_fstat(fd,&st) != 0) {
        DEBUG(3,("fstat of fnum %d failed (%s)\n",fsp->fnum, strerror(errno)));
        return(UNIXERROR(ERRDOS,ERRbadfid));
      }
    }
  } else {
    /* set path info */
    info_level = SVAL(params,0);    
    fname = fname1;
    pstrcpy(fname,&params[6]);
    unix_convert(fname,conn,0,&bad_path,&st);
    if(!check_name(fname, conn))
    {
      if((errno == ENOENT) && bad_path)
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,ERRbadpath));
    }
 
    if(!VALID_STAT(st) && dos_stat(fname,&st)!=0) {
      DEBUG(3,("stat of %s failed (%s)\n", fname, strerror(errno)));
      if((errno == ENOENT) && bad_path)
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,ERRbadpath));
    }    
  }

  DEBUG(3,("call_trans2setfilepathinfo(%d) %s info_level=%d totdata=%d\n",
	   tran_call,fname,info_level,total_data));

  /* Realloc the parameter and data sizes */
  params = *pparams = Realloc(*pparams,2);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  SSVAL(params,0,0);

  size = st.st_size;
  tvs.modtime = st.st_mtime;
  tvs.actime = st.st_atime;
  mode = dos_mode(conn,fname,&st);

#if 0
  if (total_data > 0 && IVAL(pdata,0) == total_data) {
    /* uggh, EAs for OS2 */
    DEBUG(4,("Rejecting EA request with total_data=%d\n",total_data));
    return(ERROR(ERRDOS,ERROR_EAS_NOT_SUPPORTED));
  }
#endif

  switch (info_level)
  {
    case SMB_INFO_STANDARD:
    case SMB_INFO_QUERY_EA_SIZE:
    {
      /* access time */
      tvs.actime = make_unix_date2(pdata+l1_fdateLastAccess);

      /* write time */
      tvs.modtime = make_unix_date2(pdata+l1_fdateLastWrite);

      mode = SVAL(pdata,l1_attrFile);
      size = IVAL(pdata,l1_cbFile);
      break;
    }

    /* XXXX um, i don't think this is right.
       it's also not in the cifs6.txt spec.
     */
    case SMB_INFO_QUERY_EAS_FROM_LIST:
      tvs.actime = make_unix_date2(pdata+8);
      tvs.modtime = make_unix_date2(pdata+12);
      size = IVAL(pdata,16);
      mode = IVAL(pdata,24);
      break;

    /* XXXX nor this.  not in cifs6.txt, either. */
    case SMB_INFO_QUERY_ALL_EAS:
      tvs.actime = make_unix_date2(pdata+8);
      tvs.modtime = make_unix_date2(pdata+12);
      size = IVAL(pdata,16);
      mode = IVAL(pdata,24);
      break;

    case SMB_SET_FILE_BASIC_INFO:
    {
      /* Patch to do this correctly from Paul Eggert <eggert@twinsun.com>. */
      time_t write_time;
      time_t changed_time;

      /* Ignore create time at offset pdata. */

      /* access time */
      tvs.actime = interpret_long_date(pdata+8);

      write_time = interpret_long_date(pdata+16);
      changed_time = interpret_long_date(pdata+24);

      tvs.modtime = MIN(write_time, changed_time);

      /* Prefer a defined time to an undefined one. */
      if (tvs.modtime == (time_t)0 || tvs.modtime == (time_t)-1)
       tvs.modtime = (write_time == (time_t)0 || write_time == (time_t)-1
                      ? changed_time
                      : write_time);

#if 0 /* Needs more testing... */
      /* Test from Luke to prevent Win95 from
         setting incorrect values here.
       */
      if (tvs.actime < tvs.modtime)
        return(ERROR(ERRDOS,ERRnoaccess));
#endif /* Needs more testing... */

      /* attributes */
      mode = IVAL(pdata,32);
      break;
    }

    /*
     * NT seems to use this call with a size of zero
     * to mean truncate the file. JRA.
     */

    case SMB_SET_FILE_ALLOCATION_INFO:
    {
      SMB_OFF_T newsize = IVAL(pdata,0);
#ifdef LARGE_SMB_OFF_T
      newsize |= (((SMB_OFF_T)IVAL(pdata,4)) << 32);
#else /* LARGE_SMB_OFF_T */
      if (IVAL(pdata,4) != 0)	/* more than 32 bits? */
         return(ERROR(ERRDOS,ERRunknownlevel));
#endif /* LARGE_SMB_OFF_T */
      DEBUG(10,("call_trans2setfilepathinfo: Set file allocation info for file %s to %.0f\n", fname, (double)newsize ));
      if(newsize == 0)
        size = 0;
      break;
    }

    case SMB_SET_FILE_END_OF_FILE_INFO:
    {
      size = IVAL(pdata,0);
#ifdef LARGE_SMB_OFF_T
      size |= (((SMB_OFF_T)IVAL(pdata,4)) << 32);
#else /* LARGE_SMB_OFF_T */
      if (IVAL(pdata,4) != 0)	/* more than 32 bits? */
         return(ERROR(ERRDOS,ERRunknownlevel));
#endif /* LARGE_SMB_OFF_T */
      DEBUG(10,("call_trans2setfilepathinfo: Set end of file info for file %s to %.0f\n", fname, (double)size ));
      break;
    }

    case SMB_SET_FILE_DISPOSITION_INFO: /* Set delete on close for open file. */
    {
      if ((tran_call == TRANSACT2_SETFILEINFO) && (fsp != NULL))
      {
        BOOL delete_on_close = (CVAL(pdata,0) ? True : False);

        if(fsp->is_directory)
        {
          fsp->directory_delete_on_close = delete_on_close;
          DEBUG(10, ("call_trans2setfilepathinfo: %s delete on close flag for fnum = %d, directory %s\n",
                delete_on_close ? "Added" : "Removed", fsp->fnum, fsp->fsp_name ));

        }
	else if(fsp->stat_open)
	{
          DEBUG(10, ("call_trans2setfilepathinfo: %s delete on close flag for fnum = %d, stat open %s\n",
                delete_on_close ? "Added" : "Removed", fsp->fnum, fsp->fsp_name ));
	}
        else
        {

          /*
           * We can only set the delete on close flag if
           * the share mode contained ALLOW_SHARE_DELETE
           */

          if(lp_share_modes(SNUM(conn)))
          {
            if(!GET_ALLOW_SHARE_DELETE(fsp->share_mode))
              return(ERROR(ERRDOS,ERRnoaccess));

            /*
             * If the flag has been set then
             * modify the share mode entry for all files we have open
             * on this device and inode to tell other smbds we have 
             * changed the delete on close flag.
             */

            if(delete_on_close && !GET_DELETE_ON_CLOSE_FLAG(fsp->share_mode))
            {
              int token;
              int i;
              files_struct *iterate_fsp;
              SMB_DEV_T dev = fsp->fd_ptr->dev;
              SMB_INO_T inode = fsp->fd_ptr->inode;
              int num_share_modes;
              share_mode_entry *current_shares = NULL;

              if(lock_share_entry(fsp->conn, dev, inode, &token) == False)
                return(ERROR(ERRDOS,ERRnoaccess));

              /*
               * Before we allow this we need to ensure that all current opens
               * on the file have the GET_ALLOW_SHARE_DELETE flag set. If they
               * do not then we deny this (as we are essentially deleting the
               * file at this point.
               */

              num_share_modes = get_share_modes(conn, token, dev, inode, &current_shares);
              for(i = 0; i < num_share_modes; i++)
              {
                if(!GET_ALLOW_SHARE_DELETE(current_shares[i].share_mode))
                {
                  DEBUG(5,("call_trans2setfilepathinfo: refusing to set delete on close flag for fnum = %d, \
file %s as a share exists that was not opened with FILE_DELETE access.\n",
                        fsp->fnum, fsp->fsp_name ));
                  /*
                   * Release the lock.
                   */

                  unlock_share_entry(fsp->conn, dev, inode, token);

                  /*
                   * current_shares was malloced by get_share_modes - free it here.
                   */

                  free((char *)current_shares);

                  /*
                   * Even though share violation would be more appropriate here,
                   * return ERRnoaccess as that's what NT does.
                   */

                  return(ERROR(ERRDOS,ERRnoaccess));
                }
              }

              /*
               * current_shares was malloced by get_share_modes - free it here.
               */

              free((char *)current_shares);

              DEBUG(10,("call_trans2setfilepathinfo: %s delete on close flag for fnum = %d, file %s\n",
                   delete_on_close ? "Adding" : "Removing", fsp->fnum, fsp->fsp_name ));

              /*
               * Go through all files we have open on the same device and
               * inode (hanging off the same hash bucket) and set the DELETE_ON_CLOSE_FLAG.
               * Other smbd's that have this file open will have to fend for themselves. We
               * take care of this (rare) case in close_file(). See the comment there.
               */

              for(iterate_fsp = file_find_di_first(dev, inode); iterate_fsp;
                                    iterate_fsp = file_find_di_next(iterate_fsp))
              {
                int new_share_mode = (delete_on_close ? 
                                      (iterate_fsp->share_mode | DELETE_ON_CLOSE_FLAG) :
                                      (iterate_fsp->share_mode & ~DELETE_ON_CLOSE_FLAG) );

                DEBUG(10,("call_trans2setfilepathinfo: Changing share mode for fnum %d, file %s \
dev = %x, inode = %.0f from %x to %x\n", 
                      iterate_fsp->fnum, iterate_fsp->fsp_name, (unsigned int)dev, 
                      (double)inode, iterate_fsp->share_mode, new_share_mode ));

                if(modify_share_mode(token, iterate_fsp, new_share_mode, iterate_fsp->oplock_type)==False)
                  DEBUG(0,("call_trans2setfilepathinfo: failed to change delete on close for fnum %d, \
dev = %x, inode = %.0f\n", iterate_fsp->fnum, (unsigned int)dev, (double)inode));
              }

              /*
               * Set the delete on close flag in the reference
               * counted struct. Delete when the last reference
               * goes away.
               */
             fsp->fd_ptr->delete_on_close = delete_on_close;

             unlock_share_entry(fsp->conn, dev, inode, token);

             DEBUG(10, ("call_trans2setfilepathinfo: %s delete on close flag for fnum = %d, file %s\n",
                   delete_on_close ? "Added" : "Removed", fsp->fnum, fsp->fsp_name ));

            } /* end if(delete_on_close && !GET_DELETE_ON_CLOSE_FLAG(fsp->share_mode)) */
          } /* end if lp_share_modes() */
        } /* end if is_directory. */
      } else
        return(ERROR(ERRDOS,ERRunknownlevel));
      break;
    }

    default:
    {
      return(ERROR(ERRDOS,ERRunknownlevel));
    }
  }

  /* get some defaults (no modifications) if any info is zero or -1. */
  if (tvs.actime == (time_t)0 || tvs.actime == (time_t)-1)
    tvs.actime = st.st_atime;

  if (tvs.modtime == (time_t)0 || tvs.modtime == (time_t)-1)
    tvs.modtime = st.st_mtime;

  DEBUG(6,("actime: %s " , ctime(&tvs.actime)));
  DEBUG(6,("modtime: %s ", ctime(&tvs.modtime)));
  DEBUG(6,("size: %.0f ", (double)size));
  DEBUG(6,("mode: %x\n"  , mode));

  if(!((info_level == SMB_SET_FILE_END_OF_FILE_INFO) ||
     (info_level == SMB_SET_FILE_ALLOCATION_INFO))) {
    /*
     * Only do this test if we are not explicitly
     * changing the size of a file.
     */
    if (!size)
      size = st.st_size;
  }

  /* Try and set the times, size and mode of this file -
     if they are different from the current values
   */
  if (st.st_mtime != tvs.modtime || st.st_atime != tvs.actime) {
    if(fsp != NULL) {
      /*
       * This was a setfileinfo on an open file.
       * NT does this a lot. It's actually pointless
       * setting the time here, as it will be overwritten
       * on the next write, so we save the request
       * away and will set it on file code. JRA.
       */

       if (tvs.modtime != (time_t)0 && tvs.modtime != (time_t)-1) {
         DEBUG(10,("call_trans2setfilepathinfo: setting pending modtime to %s\n",
            ctime(&tvs.modtime) ));
         fsp->pending_modtime = tvs.modtime;
       }

    } else {

      DEBUG(10,("call_trans2setfilepathinfo: setting utimes to modified values.\n"));

      if(file_utime(conn, fname, &tvs)!=0)
        return(UNIXERROR(ERRDOS,ERRnoaccess));
    }
  }

  /* check the mode isn't different, before changing it */
  if ((mode != 0) && (mode != dos_mode(conn, fname, &st))) {

    DEBUG(10,("call_trans2setfilepathinfo: file %s : setting dos mode %x\n",
          fname, mode ));

    if(file_chmod(conn, fname, mode, NULL)) {
      DEBUG(2,("chmod of %s failed (%s)\n", fname, strerror(errno)));
      return(UNIXERROR(ERRDOS,ERRnoaccess));
    }
  }

  if(size != st.st_size) {

    DEBUG(10,("call_trans2setfilepathinfo: file %s : setting new size to %.0f\n",
          fname, (double)size ));

    if (fd == -1) {
      fd = dos_open(fname,O_RDWR,0);
      if (fd == -1)
        return(UNIXERROR(ERRDOS,ERRbadpath));
      set_filelen(fd, size);
      close(fd);
    } else {
      set_filelen(fd, size);
    }

    if(fsp)
      set_filelen_write_cache(fsp, size);
  }

  SSVAL(params,0,0);

  send_trans2_replies(outbuf, bufsize, params, 2, *ppdata, 0);
  
  return(-1);
}

/****************************************************************************
  reply to a TRANS2_MKDIR (make directory with extended attributes).
****************************************************************************/
static int call_trans2mkdir(connection_struct *conn,
			    char *inbuf, char *outbuf, int length, int bufsize,
			    char **pparams, char **ppdata)
{
  char *params = *pparams;
  pstring directory;
  int ret = -1;
  BOOL bad_path = False;

  if (!CAN_WRITE(conn))
    return(ERROR(ERRSRV,ERRaccess));

  pstrcpy(directory, &params[4]);

  DEBUG(3,("call_trans2mkdir : name = %s\n", directory));

  unix_convert(directory,conn,0,&bad_path,NULL);
  if (check_name(directory,conn))
    ret = dos_mkdir(directory,unix_mode(conn,aDIR,directory));
  
  if(ret < 0)
    {
      DEBUG(5,("call_trans2mkdir error (%s)\n", strerror(errno)));
      if((errno == ENOENT) && bad_path)
      {
        unix_ERR_class = ERRDOS;
        unix_ERR_code = ERRbadpath;
      }
      return(UNIXERROR(ERRDOS,ERRnoaccess));
    }

  /* Realloc the parameter and data sizes */
  params = *pparams = Realloc(*pparams,2);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  SSVAL(params,0,0);

  send_trans2_replies(outbuf, bufsize, params, 2, *ppdata, 0);
  
  return(-1);
}

/****************************************************************************
  reply to a TRANS2_FINDNOTIFYFIRST (start monitoring a directory for changes)
  We don't actually do this - we just send a null response.
****************************************************************************/
static int call_trans2findnotifyfirst(connection_struct *conn,
				      char *inbuf, char *outbuf, 
				      int length, int bufsize,
				      char **pparams, char **ppdata)
{
  static uint16 fnf_handle = 257;
  char *params = *pparams;
  uint16 info_level = SVAL(params,4);

  DEBUG(3,("call_trans2findnotifyfirst - info_level %d\n", info_level));

  switch (info_level) 
    {
    case 1:
    case 2:
      break;
    default:
      return(ERROR(ERRDOS,ERRunknownlevel));
    }

  /* Realloc the parameter and data sizes */
  params = *pparams = Realloc(*pparams,6);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  SSVAL(params,0,fnf_handle);
  SSVAL(params,2,0); /* No changes */
  SSVAL(params,4,0); /* No EA errors */

  fnf_handle++;

  if(fnf_handle == 0)
    fnf_handle = 257;

  send_trans2_replies(outbuf, bufsize, params, 6, *ppdata, 0);
  
  return(-1);
}

/****************************************************************************
  reply to a TRANS2_FINDNOTIFYNEXT (continue monitoring a directory for 
  changes). Currently this does nothing.
****************************************************************************/
static int call_trans2findnotifynext(connection_struct *conn,
				     char *inbuf, char *outbuf, 
				     int length, int bufsize,
				     char **pparams, char **ppdata)
{
  char *params = *pparams;

  DEBUG(3,("call_trans2findnotifynext\n"));

  /* Realloc the parameter and data sizes */
  params = *pparams = Realloc(*pparams,4);
  if(params == NULL)
    return(ERROR(ERRDOS,ERRnomem));

  SSVAL(params,0,0); /* No changes */
  SSVAL(params,2,0); /* No EA errors */

  send_trans2_replies(outbuf, bufsize, params, 4, *ppdata, 0);
  
  return(-1);
}

/****************************************************************************
  reply to a SMBfindclose (stop trans2 directory search)
****************************************************************************/
int reply_findclose(connection_struct *conn,
		    char *inbuf,char *outbuf,int length,int bufsize)
{
	int outsize = 0;
	int dptr_num=SVALS(inbuf,smb_vwv0);

	DEBUG(3,("reply_findclose, dptr_num = %d\n", dptr_num));

	dptr_close(&dptr_num);

	outsize = set_message(outbuf,0,0,True);

	DEBUG(3,("SMBfindclose dptr_num = %d\n", dptr_num));

	return(outsize);
}

/****************************************************************************
  reply to a SMBfindnclose (stop FINDNOTIFYFIRST directory search)
****************************************************************************/
int reply_findnclose(connection_struct *conn, 
		     char *inbuf,char *outbuf,int length,int bufsize)
{
	int outsize = 0;
	int dptr_num= -1;
	
	dptr_num = SVAL(inbuf,smb_vwv0);

	DEBUG(3,("reply_findnclose, dptr_num = %d\n", dptr_num));

	/* We never give out valid handles for a 
	   findnotifyfirst - so any dptr_num is ok here. 
	   Just ignore it. */

	outsize = set_message(outbuf,0,0,True);

	DEBUG(3,("SMB_findnclose dptr_num = %d\n", dptr_num));

	return(outsize);
}


/****************************************************************************
  reply to a SMBtranss2 - just ignore it!
****************************************************************************/
int reply_transs2(connection_struct *conn,
		  char *inbuf,char *outbuf,int length,int bufsize)
{
	DEBUG(4,("Ignoring transs2 of length %d\n",length));
	return(-1);
}

/****************************************************************************
  reply to a SMBtrans2
****************************************************************************/
int reply_trans2(connection_struct *conn,
		 char *inbuf,char *outbuf,int length,int bufsize)
{
	int outsize = 0;
	unsigned int total_params = SVAL(inbuf, smb_tpscnt);
	unsigned int total_data =SVAL(inbuf, smb_tdscnt);
#if 0
	unsigned int max_param_reply = SVAL(inbuf, smb_mprcnt);
	unsigned int max_data_reply = SVAL(inbuf, smb_mdrcnt);
	unsigned int max_setup_fields = SVAL(inbuf, smb_msrcnt);
	BOOL close_tid = BITSETW(inbuf+smb_flags,0);
	BOOL no_final_response = BITSETW(inbuf+smb_flags,1);
	int32 timeout = IVALS(inbuf,smb_timeout);
#endif
	unsigned int suwcnt = SVAL(inbuf, smb_suwcnt);
	unsigned int tran_call = SVAL(inbuf, smb_setup0);
	char *params = NULL, *data = NULL;
	unsigned int num_params, num_params_sofar, num_data, num_data_sofar;

	if(global_oplock_break && (tran_call == TRANSACT2_OPEN)) {
		/* Queue this open message as we are the process of an
		 * oplock break.  */

		DEBUG(2,("reply_trans2: queueing message trans2open due to being "));
		DEBUGADD(2,( "in oplock break state.\n"));

		push_oplock_pending_smb_message(inbuf, length);
		return -1;
	}
	
	outsize = set_message(outbuf,0,0,True);

	/* All trans2 messages we handle have smb_sucnt == 1 - ensure this
	   is so as a sanity check */
	if (suwcnt != 1) {
		DEBUG(2,("Invalid smb_sucnt in trans2 call(%u)\n",suwcnt));
		DEBUG(2,("Transaction is %d\n",tran_call));
		ERROR(ERRDOS,ERRinvalidparam);
	}
    
	/* Allocate the space for the maximum needed parameters and data */
	if (total_params > 0)
		params = (char *)malloc(total_params);
	if (total_data > 0)
		data = (char *)malloc(total_data);
  
	if ((total_params && !params)  || (total_data && !data)) {
		DEBUG(2,("Out of memory in reply_trans2\n"));
		SAFE_FREE(params);
		SAFE_FREE(data); 
		return ERROR(ERRDOS,ERRnomem);
	}

	/* Copy the param and data bytes sent with this request into
	   the params buffer */
	num_params = num_params_sofar = SVAL(inbuf,smb_pscnt);
	num_data = num_data_sofar = SVAL(inbuf, smb_dscnt);

	if (num_params > total_params || num_data > total_data)
		exit_server("invalid params in reply_trans2");

	if(params) {
		unsigned int psoff = SVAL(inbuf, smb_psoff);
		if ((psoff + num_params < psoff) || (psoff + num_params < num_params))
			goto bad_param;
		if (smb_base(inbuf) + psoff + num_params > inbuf + length)
			goto bad_param;
		memcpy( params, smb_base(inbuf) + psoff, num_params);
	}
	if(data) {
		unsigned int dsoff = SVAL(inbuf, smb_dsoff);
		if ((dsoff + num_data < dsoff) || (dsoff + num_data < num_data))
			goto bad_param;
		if (smb_base(inbuf) + dsoff + num_data > inbuf + length)
			goto bad_param;
		memcpy( data, smb_base(inbuf) + dsoff, num_data);
	}

	if(num_data_sofar < total_data || num_params_sofar < total_params)  {
		/* We need to send an interim response then receive the rest
		   of the parameter/data bytes */
		outsize = set_message(outbuf,0,0,True);
		if (!send_smb(Client,outbuf))
			exit_server("reply_trans2: send_smb failed.");

		while (num_data_sofar < total_data || 
		       num_params_sofar < total_params) {
			BOOL ret;
			unsigned int param_disp;
			unsigned int param_off;
			unsigned int data_disp;
			unsigned int data_off;

			ret = receive_next_smb(inbuf,bufsize,SMB_SECONDARY_WAIT);
			
			if ((ret && 
			     (CVAL(inbuf, smb_com) != SMBtranss2)) || !ret) {
				outsize = set_message(outbuf,0,0,True);
				if(ret)
					DEBUG(0,("reply_trans2: Invalid secondary trans2 packet\n"));
				else
					DEBUG(0,("reply_trans2: %s in getting secondary trans2 response.\n",
						 (smb_read_error == READ_ERROR) ? "error" : "timeout" ));
				goto bad_param;
			}
      
			/* Revise total_params and total_data in case
                           they have changed downwards */
			if (SVAL(inbuf, smb_tpscnt) < total_params)
				total_params = SVAL(inbuf, smb_tpscnt);
			if (SVAL(inbuf, smb_tdscnt) < total_data)
				total_data = SVAL(inbuf, smb_tdscnt);

			num_params = SVAL(inbuf,smb_spscnt);
			param_off = SVAL(inbuf, smb_spsoff);
			param_disp = SVAL(inbuf, smb_spsdisp);
			num_params_sofar += num_params;

			num_data = SVAL(inbuf, smb_sdscnt);
			data_off = SVAL(inbuf, smb_sdsoff);
			data_disp = SVAL(inbuf, smb_sdsdisp);
			num_data_sofar += num_data;

			if (num_params_sofar > total_params || num_data_sofar > total_data)
				goto bad_param;
			
			if (num_params) {
				if (param_disp + num_params >= total_params)
					goto bad_param;
				if ((param_disp + num_params < param_disp) ||
						(param_disp + num_params < num_params))
					goto bad_param;
				if (smb_base(inbuf) + param_off + num_params >= inbuf + bufsize)
					goto bad_param;
				if (params + param_disp < params)
					goto bad_param;

				memcpy( &params[param_disp], smb_base(inbuf) + param_off, num_params);
			}
			if (num_data) {
				if (data_disp + num_data >= total_data)
					goto bad_param;
				if ((data_disp + num_data < data_disp) ||
						(data_disp + num_data < num_data))
					goto bad_param;
				if (smb_base(inbuf) + data_off + num_data >= inbuf + bufsize)
					goto bad_param;
				if (data + data_disp < data)
					goto bad_param;

				memcpy( &data[data_disp], smb_base(inbuf) + data_off, num_data);
			}
		}
	}
	
	if (Protocol >= PROTOCOL_NT1) {
		uint16 flg2 = SVAL(outbuf,smb_flg2);
		SSVAL(outbuf,smb_flg2,flg2 | 0x40); /* IS_LONG_NAME */
	}

	/* Now we must call the relevant TRANS2 function */
	switch(tran_call)  {
	case TRANSACT2_OPEN:
		outsize = call_trans2open(conn, 
					  inbuf, outbuf, bufsize, 
					  &params, &data);
		break;

	case TRANSACT2_FINDFIRST:
		outsize = call_trans2findfirst(conn, inbuf, outbuf, 
					       bufsize, &params, &data);
		break;

	case TRANSACT2_FINDNEXT:
		outsize = call_trans2findnext(conn, inbuf, outbuf, 
					      length, bufsize, 
					      &params, &data);
		break;

	case TRANSACT2_QFSINFO:
	    outsize = call_trans2qfsinfo(conn, inbuf, outbuf, 
					 length, bufsize, &params, 
					 &data);
	    break;

	case TRANSACT2_SETFSINFO:
		outsize = call_trans2setfsinfo(conn, inbuf, outbuf, 
					       length, bufsize, 
					       &params, &data);
		break;

	case TRANSACT2_QPATHINFO:
	case TRANSACT2_QFILEINFO:
		outsize = call_trans2qfilepathinfo(conn, inbuf, outbuf, 
						   length, bufsize, 
						   &params, &data, total_data);
		break;
	case TRANSACT2_SETPATHINFO:
	case TRANSACT2_SETFILEINFO:
		outsize = call_trans2setfilepathinfo(conn, inbuf, outbuf, 
						     length, bufsize, 
						     &params, &data, 
						     total_data);
		break;

	case TRANSACT2_FINDNOTIFYFIRST:
		outsize = call_trans2findnotifyfirst(conn, inbuf, outbuf, 
						     length, bufsize, 
						     &params, &data);
		break;

	case TRANSACT2_FINDNOTIFYNEXT:
		outsize = call_trans2findnotifynext(conn, inbuf, outbuf, 
						    length, bufsize, 
						    &params, &data);
		break;
	case TRANSACT2_MKDIR:
		outsize = call_trans2mkdir(conn, inbuf, outbuf, length, 
					   bufsize, &params, &data);
		break;
	default:
		/* Error in request */
		DEBUG(2,("Unknown request %d in trans2 call\n", tran_call));
		if(params)
			free(params);
		if(data)
			free(data);
		return (ERROR(ERRSRV,ERRerror));
	}
	
	/* As we do not know how many data packets will need to be
	   returned here the various call_trans2xxxx calls
	   must send their own. Thus a call_trans2xxx routine only
	   returns a value other than -1 when it wants to send
	   an error packet. 
	*/
	
	if(params)
		free(params);
	if(data)
		free(data);
	return outsize; /* If a correct response was needed the
			   call_trans2xxx calls have already sent
			   it. If outsize != -1 then it is returning */

  bad_param:

	SAFE_FREE(params);
	SAFE_FREE(data);
	return (ERROR(ERRDOS,ERRinvalidparam));
}
