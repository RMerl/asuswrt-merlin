/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))
#ifdef WITH_NISPLUS_HOME
#ifdef BROKEN_NISPLUS_INCLUDE_FILES
/*
 * The following lines are needed due to buggy include files
 * in Solaris 2.6 which define GROUP in both /usr/include/sys/acl.h and
 * also in /usr/include/rpcsvc/nis.h. The definitions conflict. JRA.
 * Also GROUP_OBJ is defined as 0x4 in /usr/include/sys/acl.h and as
 * an enum in /usr/include/rpcsvc/nis.h.
 */

#if defined(GROUP)
#undef GROUP
#endif

#if defined(GROUP_OBJ)
#undef GROUP_OBJ
#endif

#endif /* BROKEN_NISPLUS_INCLUDE_FILES */

#include <rpcsvc/nis.h>

#else /* !WITH_NISPLUS_HOME */

#include "rpcsvc/ypclnt.h"

#endif /* WITH_NISPLUS_HOME */
#endif /* HAVE_NETGROUP && WITH_AUTOMOUNT */

#ifdef WITH_SSL
#include <ssl.h>
#undef Realloc  /* SSLeay defines this and samba has a function of this name */
extern SSL  *ssl;
extern int  sslFd;
#endif  /* WITH_SSL */

extern int DEBUGLEVEL;

int Protocol = PROTOCOL_COREPLUS;

/* a default finfo structure to ensure all fields are sensible */
file_info def_finfo = {-1,0,0,0,0,0,0,""};

/* the client file descriptor */
extern int Client;

/* this is used by the chaining code */
int chain_size = 0;

int trans_num = 0;

/*
   case handling on filenames 
*/
int case_default = CASE_LOWER;

/* the following control case operations - they are put here so the
   client can link easily */
BOOL case_sensitive;
BOOL case_preserve;
BOOL use_mangled_map = False;
BOOL short_case_preserve;
BOOL case_mangle;

fstring remote_machine="";
fstring local_machine="";
fstring remote_arch="UNKNOWN";
static enum remote_arch_types ra_type = RA_UNKNOWN;
fstring remote_proto="UNKNOWN";
pstring user_socket_options=DEFAULT_SOCKET_OPTIONS;   

pstring sesssetup_user="";
pstring samlogon_user="";

BOOL sam_logon_in_ssb = False;

pstring global_myname = "";
fstring global_myworkgroup = "";
char **my_netbios_names;

static char *filename_dos(char *path,char *buf);



/****************************************************************************
  find a suitable temporary directory. The result should be copied immediately
  as it may be overwritten by a subsequent call
  ****************************************************************************/
char *tmpdir(void)
{
  char *p;
  if ((p = getenv("TMPDIR"))) {
    return p;
  }
  return "/tmp";
}

/****************************************************************************
determine whether we are in the specified group
****************************************************************************/

BOOL in_group(gid_t group, gid_t current_gid, int ngroups, gid_t *groups)
{
	int i;

	if (group == current_gid) return(True);

	for (i=0;i<ngroups;i++)
		if (group == groups[i])
			return(True);

	return(False);
}


/****************************************************************************
like atoi but gets the value up to the separater character
****************************************************************************/
char *Atoic(char *p, int *n, char *c)
{
	if (!isdigit((int)*p))
	{
		DEBUG(5, ("Atoic: malformed number\n"));
		return NULL;
	}

	(*n) = atoi(p);

	while ((*p) && isdigit((int)*p))
	{
		p++;
	}

	if (strchr(c, *p) == NULL)
	{
		DEBUG(5, ("Atoic: no separator characters (%s) not found\n", c));
		return NULL;
	}

	return p;
}

/*************************************************************************
 reads a list of numbers
 *************************************************************************/
char *get_numlist(char *p, uint32 **num, int *count)
{
	int val;

	if (num == NULL || count == NULL)
	{
		return NULL;
	}

	(*count) = 0;
	(*num  ) = NULL;

	while ((p = Atoic(p, &val, ":,")) != NULL && (*p) != ':')
	{
		(*num) = Realloc((*num), ((*count)+1) * sizeof(uint32));
		if ((*num) == NULL)
		{
			return NULL;
		}
		(*num)[(*count)] = val;
		(*count)++;
		p++;
	}

	return p;
}

#define TRUNCATE_NETBIOS_NAME 1

/*******************************************************************
 convert, possibly using a stupid microsoft-ism which has destroyed
 the transport independence of netbios (for CIFS vendors that usually
 use the Win95-type methods, not for NT to NT communication, which uses
 DCE/RPC and therefore full-length unicode strings...) a dns name into
 a netbios name.

 the netbios name (NOT necessarily null-terminated) is truncated to 15
 characters.

 ******************************************************************/
char *dns_to_netbios_name(char *dns_name)
{
	static char netbios_name[16];
	int i;
	StrnCpy(netbios_name, dns_name, 15);
	netbios_name[15] = 0;
	
#ifdef TRUNCATE_NETBIOS_NAME
	/* ok.  this is because of a stupid microsoft-ism.  if the called host
	   name contains a '.', microsoft clients expect you to truncate the
	   netbios name up to and including the '.'  this even applies, by
	   mistake, to workgroup (domain) names, which is _really_ daft.
	 */
	for (i = 15; i >= 0; i--)
	{
		if (netbios_name[i] == '.')
		{
			netbios_name[i] = 0;
			break;
		}
	}
#endif /* TRUNCATE_NETBIOS_NAME */

	return netbios_name;
}


/****************************************************************************
interpret the weird netbios "name". Return the name type
****************************************************************************/
static int name_interpret(char *in,char *out)
{
  int ret;
  int len = (*in++) / 2;

  *out=0;

  if (len > 30 || len<1) return(0);

  while (len--)
    {
      if (in[0] < 'A' || in[0] > 'P' || in[1] < 'A' || in[1] > 'P') {
	*out = 0;
	return(0);
      }
      *out = ((in[0]-'A')<<4) + (in[1]-'A');
      in += 2;
      out++;
    }
  *out = 0;
  ret = out[-1];

#ifdef NETBIOS_SCOPE
  /* Handle any scope names */
  while(*in) 
    {
      *out++ = '.'; /* Scope names are separated by periods */
      len = *(unsigned char *)in++;
      StrnCpy(out, in, len);
      out += len;
      *out=0;
      in += len;
    }
#endif
  return(ret);
}

/****************************************************************************
mangle a name into netbios format

  Note:  <Out> must be (33 + strlen(scope) + 2) bytes long, at minimum.
****************************************************************************/
int name_mangle( char *In, char *Out, char name_type )
  {
  int   i;
  int   c;
  int   len;
  char  buf[20];
  char *p = Out;
  extern pstring global_scope;

  /* Safely copy the input string, In, into buf[]. */
  (void)memset( buf, 0, 20 );
  if (strcmp(In,"*") == 0)
    buf[0] = '*';
  else
    (void)slprintf( buf, sizeof(buf) - 1, "%-15.15s%c", In, name_type );

  /* Place the length of the first field into the output buffer. */
  p[0] = 32;
  p++;

  /* Now convert the name to the rfc1001/1002 format. */
  for( i = 0; i < 16; i++ )
    {
    c = toupper( buf[i] );
    p[i*2]     = ( (c >> 4) & 0x000F ) + 'A';
    p[(i*2)+1] = (c & 0x000F) + 'A';
    }
  p += 32;
  p[0] = '\0';

  /* Add the scope string. */
  for( i = 0, len = 0; NULL != global_scope; i++, len++ )
    {
    switch( global_scope[i] )
      {
      case '\0':
        p[0]     = len;
        if( len > 0 )
          p[len+1] = 0;
        return( name_len(Out) );
      case '.':
        p[0] = len;
        p   += (len + 1);
        len  = -1;
        break;
      default:
        p[len+1] = global_scope[i];
        break;
      }
    }

  return( name_len(Out) );
  } /* name_mangle */

/*******************************************************************
  check if a file exists
********************************************************************/
BOOL file_exist(char *fname,SMB_STRUCT_STAT *sbuf)
{
  SMB_STRUCT_STAT st;
  if (!sbuf) sbuf = &st;
  
  if (sys_stat(fname,sbuf) != 0) 
    return(False);

  return(S_ISREG(sbuf->st_mode));
}

/*******************************************************************
check a files mod time
********************************************************************/
time_t file_modtime(char *fname)
{
  SMB_STRUCT_STAT st;
  
  if (sys_stat(fname,&st) != 0) 
    return(0);

  return(st.st_mtime);
}

/*******************************************************************
  check if a directory exists
********************************************************************/
BOOL directory_exist(char *dname,SMB_STRUCT_STAT *st)
{
  SMB_STRUCT_STAT st2;
  BOOL ret;

  if (!st) st = &st2;

  if (sys_stat(dname,st) != 0) 
    return(False);

  ret = S_ISDIR(st->st_mode);
  if(!ret)
    errno = ENOTDIR;
  return ret;
}

/*******************************************************************
returns the size in bytes of the named file
********************************************************************/
SMB_OFF_T get_file_size(char *file_name)
{
  SMB_STRUCT_STAT buf;
  buf.st_size = 0;
  if(sys_stat(file_name,&buf) != 0)
    return (SMB_OFF_T)-1;
  return(buf.st_size);
}

/*******************************************************************
return a string representing an attribute for a file
********************************************************************/
char *attrib_string(uint16 mode)
{
  static fstring attrstr;

  attrstr[0] = 0;

  if (mode & aVOLID) fstrcat(attrstr,"V");
  if (mode & aDIR) fstrcat(attrstr,"D");
  if (mode & aARCH) fstrcat(attrstr,"A");
  if (mode & aHIDDEN) fstrcat(attrstr,"H");
  if (mode & aSYSTEM) fstrcat(attrstr,"S");
  if (mode & aRONLY) fstrcat(attrstr,"R");	  

  return(attrstr);
}

/*******************************************************************
  show a smb message structure
********************************************************************/
void show_msg(char *buf)
{
	int i;
	int bcc=0;

	if (DEBUGLEVEL < 5) return;

	DEBUG(5,("size=%d\nsmb_com=0x%x\nsmb_rcls=%d\nsmb_reh=%d\nsmb_err=%d\nsmb_flg=%d\nsmb_flg2=%d\n",
			smb_len(buf),
			(int)CVAL(buf,smb_com),
			(int)CVAL(buf,smb_rcls),
			(int)CVAL(buf,smb_reh),
			(int)SVAL(buf,smb_err),
			(int)CVAL(buf,smb_flg),
			(int)SVAL(buf,smb_flg2)));
	DEBUG(5,("smb_tid=%d\nsmb_pid=%d\nsmb_uid=%d\nsmb_mid=%d\nsmt_wct=%d\n",
			(int)SVAL(buf,smb_tid),
			(int)SVAL(buf,smb_pid),
			(int)SVAL(buf,smb_uid),
			(int)SVAL(buf,smb_mid),
			(int)CVAL(buf,smb_wct)));

	for (i=0;i<(int)CVAL(buf,smb_wct);i++)
	{
		DEBUG(5,("smb_vwv[%d]=%d (0x%X)\n",i,
			SVAL(buf,smb_vwv+2*i),SVAL(buf,smb_vwv+2*i)));
	}

	bcc = (int)SVAL(buf,smb_vwv+2*(CVAL(buf,smb_wct)));

	DEBUG(5,("smb_bcc=%d\n",bcc));

	if (DEBUGLEVEL < 10) return;

	if (DEBUGLEVEL < 50)
	{
		bcc = MIN(bcc, 512);
	}

	dump_data(10, smb_buf(buf), bcc);
}

/*******************************************************************
  set the length and marker of an smb packet
********************************************************************/
void smb_setlen(char *buf,int len)
{
  _smb_setlen(buf,len);

  CVAL(buf,4) = 0xFF;
  CVAL(buf,5) = 'S';
  CVAL(buf,6) = 'M';
  CVAL(buf,7) = 'B';
}

/*******************************************************************
  setup the word count and byte count for a smb message
********************************************************************/
int set_message(char *buf,int num_words,int num_bytes,BOOL zero)
{
  if (zero)
    memset(buf + smb_size,'\0',num_words*2 + num_bytes);
  CVAL(buf,smb_wct) = num_words;
  SSVAL(buf,smb_vwv + num_words*SIZEOFWORD,num_bytes);  
  smb_setlen(buf,smb_size + num_words*2 + num_bytes - 4);
  return (smb_size + num_words*2 + num_bytes);
}

/*******************************************************************
reduce a file name, removing .. elements.
********************************************************************/
void dos_clean_name(char *s)
{
  char *p=NULL;

  DEBUG(3,("dos_clean_name [%s]\n",s));

  /* remove any double slashes */
  all_string_sub(s, "\\\\", "\\", 0);

  while ((p = strstr(s,"\\..\\")) != NULL)
    {
      pstring s1;

      *p = 0;
      pstrcpy(s1,p+3);

      if ((p=strrchr(s,'\\')) != NULL)
	*p = 0;
      else
	*s = 0;
      pstrcat(s,s1);
    }  

  trim_string(s,NULL,"\\..");

  all_string_sub(s, "\\.\\", "\\", 0);
}

/*******************************************************************
reduce a file name, removing .. elements. 
********************************************************************/
void unix_clean_name(char *s)
{
  char *p=NULL;

  DEBUG(3,("unix_clean_name [%s]\n",s));

  /* remove any double slashes */
  all_string_sub(s, "//","/", 0);

  /* Remove leading ./ characters */
  if(strncmp(s, "./", 2) == 0) {
    trim_string(s, "./", NULL);
    if(*s == 0)
      pstrcpy(s,"./");
  }

  while ((p = strstr(s,"/../")) != NULL)
    {
      pstring s1;

      *p = 0;
      pstrcpy(s1,p+3);

      if ((p=strrchr(s,'/')) != NULL)
	*p = 0;
      else
	*s = 0;
      pstrcat(s,s1);
    }  

  trim_string(s,NULL,"/..");
}

/*******************************************************************
reduce a file name, removing .. elements and checking that 
it is below dir in the heirachy. This uses dos_GetWd() and so must be run
on the system that has the referenced file system.

widelinks are allowed if widelinks is true
********************************************************************/

BOOL reduce_name(char *s,char *dir,BOOL widelinks)
{
#ifndef REDUCE_PATHS
  return True;
#else
  pstring dir2;
  pstring wd;
  pstring base_name;
  pstring newname;
  char *p=NULL;
  BOOL relative = (*s != '/');

  *dir2 = *wd = *base_name = *newname = 0;

  if (widelinks)
  {
    unix_clean_name(s);
    /* can't have a leading .. */
    if (strncmp(s,"..",2) == 0 && (s[2]==0 || s[2]=='/'))
    {
      DEBUG(3,("Illegal file name? (%s)\n",s));
      return(False);
    }

    if (strlen(s) == 0)
      pstrcpy(s,"./");

    return(True);
  }
  
  DEBUG(3,("reduce_name [%s] [%s]\n",s,dir));

  /* remove any double slashes */
  all_string_sub(s,"//","/",0);

  pstrcpy(base_name,s);
  p = strrchr(base_name,'/');

  if (!p)
    return(True);

  if (!dos_GetWd(wd))
  {
    DEBUG(0,("couldn't getwd for %s %s\n",s,dir));
    return(False);
  }

  if (dos_ChDir(dir) != 0)
  {
    DEBUG(0,("couldn't chdir to %s\n",dir));
    return(False);
  }

  if (!dos_GetWd(dir2))
  {
    DEBUG(0,("couldn't getwd for %s\n",dir));
    dos_ChDir(wd);
    return(False);
  }

  if (p && (p != base_name))
  {
    *p = 0;
    if (strcmp(p+1,".")==0)
      p[1]=0;
    if (strcmp(p+1,"..")==0)
      *p = '/';
  }

  if (dos_ChDir(base_name) != 0)
  {
    dos_ChDir(wd);
    DEBUG(3,("couldn't chdir for %s %s basename=%s\n",s,dir,base_name));
    return(False);
  }

  if (!dos_GetWd(newname))
  {
    dos_ChDir(wd);
    DEBUG(2,("couldn't get wd for %s %s\n",s,dir2));
    return(False);
  }

  if (p && (p != base_name))
  {
    pstrcat(newname,"/");
    pstrcat(newname,p+1);
  }

  {
    size_t l = strlen(dir2);    
    if (dir2[l-1] == '/')
      l--;

    if (strncmp(newname,dir2,l) != 0)
    {
      dos_ChDir(wd);
      DEBUG(2,("Bad access attempt? s=%s dir=%s newname=%s l=%d\n",s,dir2,newname,(int)l));
      return(False);
    }

    if (relative)
    {
      if (newname[l] == '/')
        pstrcpy(s,newname + l + 1);
      else
        pstrcpy(s,newname+l);
    }
    else
      pstrcpy(s,newname);
  }

  dos_ChDir(wd);

  if (strlen(s) == 0)
    pstrcpy(s,"./");

  DEBUG(3,("reduced to %s\n",s));
  return(True);
#endif
}

/****************************************************************************
expand some *s 
****************************************************************************/
static void expand_one(char *Mask,int len)
{
  char *p1;
  while ((p1 = strchr(Mask,'*')) != NULL)
    {
      int lfill = (len+1) - strlen(Mask);
      int l1= (p1 - Mask);
      pstring tmp;
      pstrcpy(tmp,Mask);  
      memset(tmp+l1,'?',lfill);
      pstrcpy(tmp + l1 + lfill,Mask + l1 + 1);	
      pstrcpy(Mask,tmp);      
    }
}

/****************************************************************************
expand a wildcard expression, replacing *s with ?s
****************************************************************************/
void expand_mask(char *Mask,BOOL doext)
{
  pstring mbeg,mext;
  pstring dirpart;
  pstring filepart;
  BOOL hasdot = False;
  char *p1;
  BOOL absolute = (*Mask == '\\');

  *mbeg = *mext = *dirpart = *filepart = 0;

  /* parse the directory and filename */
  if (strchr(Mask,'\\'))
    split_at_last_component(Mask,dirpart,'\\',NULL);

  filename_dos(Mask,filepart);

  pstrcpy(mbeg,filepart);
  if ((p1 = strchr(mbeg,'.')) != NULL)
    {
      hasdot = True;
      *p1 = 0;
      p1++;
      pstrcpy(mext,p1);
    }
  else
    {
      pstrcpy(mext,"");
      if (strlen(mbeg) > 8)
	{
	  pstrcpy(mext,mbeg + 8);
	  mbeg[8] = 0;
	}
    }

  if (*mbeg == 0)
    pstrcpy(mbeg,"????????");
  if ((*mext == 0) && doext && !hasdot)
    pstrcpy(mext,"???");

  if (strequal(mbeg,"*") && *mext==0) 
    pstrcpy(mext,"*");

  /* expand *'s */
  expand_one(mbeg,8);
  if (*mext)
    expand_one(mext,3);

  pstrcpy(Mask,dirpart);
  if (*dirpart || absolute) pstrcat(Mask,"\\");
  pstrcat(Mask,mbeg);
  pstrcat(Mask,".");
  pstrcat(Mask,mext);

  DEBUG(6,("Mask expanded to [%s]\n",Mask));
}  



/****************************************************************************
  make a dir struct
****************************************************************************/
void make_dir_struct(char *buf,char *mask,char *fname,SMB_OFF_T size,int mode,time_t date)
{  
  char *p;
  pstring mask2;

  pstrcpy(mask2,mask);

  if ((mode & aDIR) != 0)
    size = 0;

  memset(buf+1,' ',11);
  if ((p = strchr(mask2,'.')) != NULL)
    {
      *p = 0;
      memcpy(buf+1,mask2,MIN(strlen(mask2),8));
      memcpy(buf+9,p+1,MIN(strlen(p+1),3));
      *p = '.';
    }
  else
    memcpy(buf+1,mask2,MIN(strlen(mask2),11));

  memset(buf+21,'\0',DIR_STRUCT_SIZE-21);
  CVAL(buf,21) = mode;
  put_dos_date(buf,22,date);
  SSVAL(buf,26,size & 0xFFFF);
  SSVAL(buf,28,(size >> 16)&0xFFFF);
  StrnCpy(buf+30,fname,12);
  if (!case_sensitive)
    strupper(buf+30);
  DEBUG(8,("put name [%s] into dir struct\n",buf+30));
}


/*******************************************************************
close the low 3 fd's and open dev/null in their place
********************************************************************/
void close_low_fds(void)
{
  int fd;
  int i;
  close(0); close(1); close(2);
  /* try and use up these file descriptors, so silly
     library routines writing to stdout etc won't cause havoc */
  for (i=0;i<3;i++) {
    fd = sys_open("/dev/null",O_RDWR,0);
    if (fd < 0) fd = sys_open("/dev/null",O_WRONLY,0);
    if (fd < 0) {
      DEBUG(0,("Can't open /dev/null\n"));
      return;
    }
    if (fd != i) {
      DEBUG(0,("Didn't get file descriptor %d\n",i));
      return;
    }
  }
}

/****************************************************************************
Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
else
if SYSV use O_NDELAY
if BSD use FNDELAY
****************************************************************************/
int set_blocking(int fd, BOOL set)
{
  int val;
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

  if((val = fcntl(fd, F_GETFL, 0)) == -1)
	return -1;
  if(set) /* Turn blocking on - ie. clear nonblock flag */
	val &= ~FLAG_TO_SET;
  else
    val |= FLAG_TO_SET;
  return fcntl( fd, F_SETFL, val);
#undef FLAG_TO_SET
}

/****************************************************************************
transfer some data between two fd's
****************************************************************************/
SMB_OFF_T transfer_file(int infd,int outfd,SMB_OFF_T n,char *header,int headlen,int align)
{
  static char *buf=NULL;  
  static int size=0;
  char *buf1,*abuf;
  SMB_OFF_T total = 0;

  DEBUG(4,("transfer_file n=%.0f  (head=%d) called\n",(double)n,headlen));

  if (size == 0) {
    size = lp_readsize();
    size = MAX(size,1024);
  }

  while (!buf && size>0) {
    buf = (char *)Realloc(buf,size+8);
    if (!buf) size /= 2;
  }

  if (!buf) {
    DEBUG(0,("Can't allocate transfer buffer!\n"));
    exit(1);
  }

  abuf = buf + (align%8);

  if (header)
    n += headlen;

  while (n > 0)
  {
    int s = (int)MIN(n,(SMB_OFF_T)size);
    int ret,ret2=0;

    ret = 0;

    if (header && (headlen >= MIN(s,1024))) {
      buf1 = header;
      s = headlen;
      ret = headlen;
      headlen = 0;
      header = NULL;
    } else {
      buf1 = abuf;
    }

    if (header && headlen > 0)
    {
      ret = MIN(headlen,size);
      memcpy(buf1,header,ret);
      headlen -= ret;
      header += ret;
      if (headlen <= 0) header = NULL;
    }

    if (s > ret)
      ret += read(infd,buf1+ret,s-ret);

    if (ret > 0)
    {
      ret2 = (outfd>=0?write_data(outfd,buf1,ret):ret);
      if (ret2 > 0) total += ret2;
      /* if we can't write then dump excess data */
      if (ret2 != ret)
        transfer_file(infd,-1,n-(ret+headlen),NULL,0,0);
    }
    if (ret <= 0 || ret2 != ret)
      return(total);
    n -= ret;
  }
  return(total);
}



/****************************************************************************
find a pointer to a netbios name
****************************************************************************/
static char *name_ptr(char *buf,int ofs)
{
  unsigned char c = *(unsigned char *)(buf+ofs);

  if ((c & 0xC0) == 0xC0)
    {
      uint16 l = RSVAL(buf, ofs) & 0x3FFF;
      DEBUG(5,("name ptr to pos %d from %d is %s\n",l,ofs,buf+l));
      return(buf + l);
    }
  else
    return(buf+ofs);
}  

/****************************************************************************
extract a netbios name from a buf
****************************************************************************/
int name_extract(char *buf,int ofs,char *name)
{
  char *p = name_ptr(buf,ofs);
  int d = PTR_DIFF(p,buf+ofs);
  pstrcpy(name,"");
  if (d < -50 || d > 50) return(0);
  return(name_interpret(p,name));
}
  
/****************************************************************************
return the total storage length of a mangled name
****************************************************************************/
int name_len(char *s1)
{
	/* NOTE: this argument _must_ be unsigned */
	unsigned char *s = (unsigned char *)s1;
	int len;

	/* If the two high bits of the byte are set, return 2. */
	if (0xC0 == (*s & 0xC0))
		return(2);

	/* Add up the length bytes. */
	for (len = 1; (*s); s += (*s) + 1) {
		len += *s + 1;
		SMB_ASSERT(len < 80);
	}

	return(len);
} /* name_len */


/*******************************************************************
sleep for a specified number of milliseconds
********************************************************************/
void msleep(int t)
{
  int tdiff=0;
  struct timeval tval,t1,t2;  
  fd_set fds;

  GetTimeOfDay(&t1);
  GetTimeOfDay(&t2);
  
  while (tdiff < t) {
    tval.tv_sec = (t-tdiff)/1000;
    tval.tv_usec = 1000*((t-tdiff)%1000);
 
    FD_ZERO(&fds);
    errno = 0;
    sys_select(0,&fds,&tval);

    GetTimeOfDay(&t2);
    tdiff = TvalDiff(&t1,&t2);
  }
}


/*********************************************************
* Recursive routine that is called by unix_mask_match.
* Does the actual matching. This is the 'original code' 
* used by the unix matcher.
*********************************************************/

BOOL unix_do_match(char *str, char *regexp, BOOL case_sig)
{
  char *p;

  for( p = regexp; *p && *str; ) {
    switch(*p) {
    case '?':
      str++; p++;
      break;

    case '*':
      /*
       * Look for a character matching 
       * the one after the '*'.
       */
      p++;
      if(!*p)
        return True; /* Automatic match */
      while(*str) {

        while(*str && (case_sig ? (*p != *str) : (toupper(*p)!=toupper(*str))))
          str++;

        /*
         * Patch from weidel@multichart.de. In the case of the regexp
         * '*XX*' we want to ensure there are at least 2 'X' characters
         * in the filename after the '*' for a match to be made.
         */

        {
          int matchcount=0;

          /*
           * Eat all the characters that match, but count how many there were.
           */

          while(*str && (case_sig ? (*p == *str) : (toupper(*p)==toupper(*str)))) {
            str++;
            matchcount++;
          }

          /*
           * Now check that if the regexp had n identical characters that
           * matchcount had at least that many matches.
           */

          while (( *(p+1) && (case_sig ? (*(p+1) == *p) : (toupper(*(p+1))==toupper(*p))))) {
            p++;
            matchcount--;
          }
          if ( matchcount <= 0 ) {
            return False;
          }
        }
        str--; /* We've eaten the match char after the '*' */
        if(unix_do_match(str,p,case_sig))
          return True;
        if(!*str)
          return False;
        else
          str++;
      }
      return False;

    default:
      if(case_sig) {
        if(*str != *p)
          return False;
      } else {
        if(toupper(*str) != toupper(*p))
          return False;
      }
      str++, p++;
      break;
    }
  }

  if(!*p && !*str)
    return True;

  if (!*p && str[0] == '.' && str[1] == 0)
    return(True);
  
  if (!*str && *p == '?')
    {
      while (*p == '?') p++;
      return(!*p);
    }

  if(!*str && (*p == '*' && p[1] == '\0'))
    return True;
  return False;
}


/*********************************************************
* Routine to match a given string with a regexp - uses
* simplified regexp that takes * and ? only. Case can be
* significant or not.
* This is the 'original code' used by the unix matcher.
*********************************************************/

static BOOL unix_mask_match(char *str, char *regexp, BOOL case_sig)
{
  char *p;
  pstring p1, p2;
  fstring ebase,sbase;
  BOOL matched;

  /* Make local copies of str and regexp */
  StrnCpy(p1,regexp,sizeof(pstring)-1);
  StrnCpy(p2,str,sizeof(pstring)-1);

  /* Remove any *? and ** as they are meaningless */
  for(p = p1; *p; p++)
    while( *p == '*' && (p[1] == '?' ||p[1] == '*'))
      (void)pstrcpy( &p[1], &p[2]);

  if (strequal(p1,"*")) return(True);

  DEBUG(8,("unix_mask_match str=<%s> regexp=<%s>, case_sig = %d\n", p2, p1, case_sig));

  fstrcpy(ebase,p1);
  fstrcpy(sbase,p2);

  matched = unix_do_match(sbase,ebase,case_sig);

  DEBUG(8,("unix_mask_match returning %d\n", matched));

  return matched;
}

/*********************************************************
* Recursive routine that is called by mask_match.
* Does the actual matching. Returns True if matched,
* False if failed. This is the 'new' NT style matcher.
* The win9x_semantics parameter is needed as Win9x matching
* is *actually different*. In Win9x, trailing '?' characters
* will only match the *exact* number of characters. Under
* DOS and NT they match any number. This makes no
* sense.....
*********************************************************/

static BOOL do_match(char *str, char *regexp, int case_sig, BOOL win9x_semantics)
{
  char *p;

  for( p = regexp; *p && *str; ) {
    switch(*p) {
    case '?':
      str++; p++;
      break;

    case '*':
      /* Look for a character matching 
         the one after the '*' */
      p++;
      if(!*p)
        return True; /* Automatic match */
      while(*str) {
        while(*str && (case_sig ? (*p != *str) : (toupper(*p)!=toupper(*str))))
          str++;

        /*
         * Patch from weidel@multichart.de. In the case of the regexp
         * '*XX*' we want to ensure there are at least 2 'X' characters
         * in the filename after the '*' for a match to be made.
         */

        {
          int matchcount=0;

          /*
           * Eat all the characters that match, but count how many there were.
           */

          while(*str && (case_sig ? (*p == *str) : (toupper(*p)==toupper(*str)))) {
            str++;
            matchcount++;
          }

          /*
           * Now check that if the regexp had n identical characters that
           * matchcount had at least that many matches.
           */

          while (( *(p+1) && (case_sig ? (*(p+1) == *p) : (toupper(*(p+1))==toupper(*p))))) {
            p++;
            matchcount--;
          }
          if ( matchcount <= 0 ) {
            return False;
          }
        }
        str--; /* We've eaten the match char after the '*' */
        if(do_match(str,p,case_sig,win9x_semantics)) {
          return True;
        }
        if(!*str) {
          return False;
        } else {
          str++;
        }
      }
      return False;

    default:
      if(case_sig) {
        if(*str != *p) {
          return False;
        }
      } else {
        if(toupper(*str) != toupper(*p)) {
          return False;
        }
      }
      str++, p++;
      break;
    }
  }

  if(!*p && !*str)
    return True;

  if (!*p && str[0] == '.' && str[1] == 0) {
    return(True);
  }
  
  if (!win9x_semantics) {
    if (!*str && *p == '?') {
      while (*p == '?')
        p++;
      return(!*p);
    }
  }

  if(!*str && (*p == '*' && p[1] == '\0')) {
    return True;
  }
 
  return False;
}

/*********************************************************
* Routine to match a given string with a regexp - uses
* simplified regexp that takes * and ? only. Case can be
* significant or not.
* The 8.3 handling was rewritten by Ums Harald <Harald.Ums@pro-sieben.de>
* This is the new 'NT style' matcher.
*********************************************************/

BOOL mask_match(char *str, char *regexp, BOOL case_sig, BOOL trans2)
{
  char *p;
  pstring t_pattern, t_filename, te_pattern, te_filename;
  fstring ebase,eext,sbase,sext;
  BOOL matched = False;
  BOOL win9x_semantics = (get_remote_arch() == RA_WIN95) && trans2;

  /* special case - if it is exactly the same then it always matches! */
  if(exact_match(str, regexp, case_sig))
    return True;

  /* Make local copies of str and regexp */
  pstrcpy(t_pattern,regexp);
  pstrcpy(t_filename,str);

  if(trans2) {

    /* a special case for 16 bit apps */
    if (strequal(t_pattern,"????????.???"))
      pstrcpy(t_pattern,"*");

#if 0
    /*
     * Handle broken clients that send us old 8.3 format.
     */
    pstring_sub(t_pattern,"????????","*");
    pstring_sub(t_pattern,".???",".*");
#endif
  }

#if 0
  /* 
   * Not sure if this is a good idea. JRA.
   */
  if(trans2 && is_8_3(t_pattern,False) && is_8_3(t_filename,False))
    trans2 = False;
#endif

#if 0
  if (!strchr(t_filename,'.')) {
    pstrcat(t_filename,".");
  }
#endif

  /* Remove any *? and ** as they are meaningless */
  while(all_string_sub(t_pattern, "*?", "*", sizeof(pstring)))
    ;

  while(all_string_sub(t_pattern, "**", "*", sizeof(pstring)))
    ;

  if (strequal(t_pattern,"*"))
    return(True);

  DEBUG(8,("mask_match str=<%s> regexp=<%s>, case_sig = %d\n", t_filename, t_pattern, case_sig));

  if(trans2) {
    /*
     * Match each component of the regexp, split up by '.'
     * characters.
     */
    char *fp, *rp, *cp2, *cp1;
    BOOL last_wcard_was_star = False;
    int num_path_components, num_regexp_components;

    if(strequal(t_pattern, "."))
      return False; /* A dot pattern never matches... ???? */

    /*
     * NT *always* treats '.' and '..' as identical for a match.... why ?
     */

    if(strequal(t_filename, ".."))
      pstrcpy(t_filename, ".");

    pstrcpy(te_pattern,t_pattern);
    pstrcpy(te_filename,t_filename);
    /*
     * Remove multiple "*." patterns.
     */
    while(all_string_sub(te_pattern, "*.*.", "*.", sizeof(pstring)))
      ;
    num_regexp_components = count_chars(te_pattern, '.');
    num_path_components = count_chars(te_filename, '.');

    /* 
     * Check for special 'hack' case of "DIR a*z". - needs to match a.b.c...z
     */
    if(num_regexp_components == 0)
      matched = do_match( te_filename, te_pattern, case_sig, win9x_semantics);
    else {
      for( cp1 = te_pattern, cp2 = te_filename; cp1;) {
        fp = strchr(cp2, '.');
        if(fp)
          *fp = '\0';
        rp = strchr(cp1, '.');
        if(rp)
          *rp = '\0';

        if(cp1[0] && cp1[strlen(cp1)-1] == '*')
          last_wcard_was_star = True;
        else
          last_wcard_was_star = False;

        if(!do_match(cp2, cp1, case_sig, win9x_semantics))
          break;

        /*
         * Ugly ! Special case for Win9x *only*. If filename is XXXX and pattern extension
         * is '*' or all '?' then disallow match.
         */

        if (win9x_semantics) {
          if (*cp2 == '\0' && str_is_all(cp1, '?'))
            break;
        }

        cp1 = rp ? rp + 1 : NULL;
        cp2 = fp ? fp + 1 : "";

        if(last_wcard_was_star || ((cp1 != NULL) && (*cp1 == '*'))) {
          /* Eat the extra path components. */
          int i;

          for(i = 0; i < num_path_components - num_regexp_components; i++) {
            fp = strchr(cp2, '.');
            if(fp)
              *fp = '\0';

            if((cp1 != NULL) && do_match( cp2, cp1, case_sig, win9x_semantics)) {
              cp2 = fp ? fp + 1 : "";
              break;
            }
            cp2 = fp ? fp + 1 : "";
          }
          num_path_components -= i;
        }
      } 
      if(cp1 == NULL && ((*cp2 == '\0') || last_wcard_was_star))
        matched = True;
    }
  } else {

    /* -------------------------------------------------
     * Behaviour of Win95
     * for 8.3 filenames and 8.3 Wildcards
     * -------------------------------------------------
     */
    if (strequal (t_filename, ".")) {
      /*
       *  Patterns:  *.*  *. ?. ? ????????.??? are valid.
       * 
       */
      if(strequal(t_pattern, "*.*") || strequal(t_pattern, "*.") ||
         strequal(t_pattern, "????????.???") ||
         strequal(t_pattern, "?.") || strequal(t_pattern, "?")) 
        matched = True;
    } else if (strequal (t_filename, "..")) {
      /*
       *  Patterns:  *.*  *. ?. ? *.? ????????.??? are valid.
       *
       */
      if(strequal(t_pattern, "*.*") || strequal(t_pattern, "*.") ||
         strequal(t_pattern, "?.") || strequal(t_pattern, "?") ||
         strequal(t_pattern, "????????.???") ||
         strequal(t_pattern, "*.?") || strequal(t_pattern, "?.*"))
        matched = True;
    } else {

      if ((p = strrchr (t_pattern, '.'))) {
        /*
         * Wildcard has a suffix.
         */
        *p = 0;
        fstrcpy (ebase, t_pattern);
        if (p[1]) {
          fstrcpy (eext, p + 1);
        } else {
          /* pattern ends in DOT: treat as if there is no DOT */
          *eext = 0;
          if (strequal (ebase, "*"))
            return (True);
        }
      } else {
        /*
         * No suffix for wildcard.
         */
        fstrcpy (ebase, t_pattern);
        eext[0] = 0;
      }

      p = strrchr (t_filename, '.');
      if (p && (p[1] == 0)	) {
        /*
         * Filename has an extension of '.' only.
         */
        *p = 0; /* nuke dot at end of string */
        p = 0;  /* and treat it as if there is no extension */
      }

      if (p) {
        /*
         * Filename has an extension.
         */
        *p = 0;
        fstrcpy (sbase, t_filename);
        fstrcpy (sext, p + 1);
        if (*eext) {
          matched = do_match(sbase, ebase, case_sig, False)
                    && do_match(sext, eext, case_sig, False);
        } else {
          /* pattern has no extension */
          /* Really: match complete filename with pattern ??? means exactly 3 chars */
          matched = do_match(str, ebase, case_sig, False);
        }
      } else {
        /* 
         * Filename has no extension.
         */
        fstrcpy (sbase, t_filename);
        fstrcpy (sext, "");
        if (*eext) {
          /* pattern has extension */
          matched = do_match(sbase, ebase, case_sig, False)
                    && do_match(sext, eext, case_sig, False);

        } else {
          matched = do_match(sbase, ebase, case_sig, False);
#ifdef EMULATE_WEIRD_W95_MATCHING
          /*
           * Even Microsoft has some problems
           * Behaviour Win95 -> local disk 
           * is different from Win95 -> smb drive from Nt 4.0
           * This branch would reflect the Win95 local disk behaviour
           */
          if (!matched) {
            /* a? matches aa and a in w95 */
            fstrcat (sbase, ".");
            matched = do_match(sbase, ebase, case_sig, False);
          }
#endif
        }
      }
    }
  }

  DEBUG(8,("mask_match returning %d\n", matched));

  return matched;
}

/****************************************************************************
become a daemon, discarding the controlling terminal
****************************************************************************/
void become_daemon(void)
{
	if (fork()) {
		_exit(0);
	}

  /* detach from the terminal */
#ifdef HAVE_SETSID
	setsid();
#elif defined(TIOCNOTTY)
	{
		int i = sys_open("/dev/tty", O_RDWR, 0);
		if (i != -1) {
			ioctl(i, (int) TIOCNOTTY, (char *)0);      
			close(i);
		}
	}
#endif /* HAVE_SETSID */

	/* Close fd's 0,1,2. Needed if started by rsh */
	close_low_fds();
}


/****************************************************************************
put up a yes/no prompt
****************************************************************************/
BOOL yesno(char *p)
{
  pstring ans;
  printf("%s",p);

  if (!fgets(ans,sizeof(ans)-1,stdin))
    return(False);

  if (*ans == 'y' || *ans == 'Y')
    return(True);

  return(False);
}

/****************************************************************************
set the length of a file from a filedescriptor.
Returns 0 on success, -1 on failure.
****************************************************************************/

int set_filelen(int fd, SMB_OFF_T len)
{
/* According to W. R. Stevens advanced UNIX prog. Pure 4.3 BSD cannot
   extend a file with ftruncate. Provide alternate implementation
   for this */

#ifdef HAVE_FTRUNCATE_EXTEND
  return sys_ftruncate(fd, len);
#else
  SMB_STRUCT_STAT st;
  char c = 0;
  SMB_OFF_T currpos = sys_lseek(fd, (SMB_OFF_T)0, SEEK_CUR);

  if(currpos == -1)
    return -1;
  /* Do an fstat to see if the file is longer than
     the requested size (call ftruncate),
     or shorter, in which case seek to len - 1 and write 1
     byte of zero */
  if(sys_fstat(fd, &st)<0)
    return -1;

#ifdef S_ISFIFO
  if (S_ISFIFO(st.st_mode))
    return 0;
#endif

  if(st.st_size == len)
    return 0;
  if(st.st_size > len)
    return sys_ftruncate(fd, len);

  if(sys_lseek(fd, len-1, SEEK_SET) != len -1)
    return -1;
  if(write(fd, &c, 1)!=1)
    return -1;
  /* Seek to where we were */
  if(sys_lseek(fd, currpos, SEEK_SET) != currpos)
    return -1;
  return 0;
#endif
}


#ifdef HPUX
/****************************************************************************
this is a version of setbuffer() for those machines that only have setvbuf
****************************************************************************/
 void setbuffer(FILE *f,char *buf,int bufsize)
{
  setvbuf(f,buf,_IOFBF,bufsize);
}
#endif


/****************************************************************************
parse out a filename from a path name. Assumes dos style filenames.
****************************************************************************/
static char *filename_dos(char *path,char *buf)
{
  char *p = strrchr(path,'\\');

  if (!p)
    pstrcpy(buf,path);
  else
    pstrcpy(buf,p+1);

  return(buf);
}



/****************************************************************************
expand a pointer to be a particular size
****************************************************************************/
void *Realloc(void *p,size_t size)
{
  void *ret=NULL;

  if (size == 0) {
    if (p) free(p);
    DEBUG(5,("Realloc asked for 0 bytes\n"));
    return NULL;
  }

  if (!p)
    ret = (void *)malloc(size);
  else
    ret = (void *)realloc(p,size);

#ifdef MEM_MAN
  {
	extern FILE *dbf;
	smb_mem_write_info(ret, dbf);
  }
#endif

  if (!ret)
    DEBUG(0,("Memory allocation error: failed to expand to %d bytes\n",(int)size));

  return(ret);
}


/****************************************************************************
get my own name and IP
****************************************************************************/
BOOL get_myname(char *my_name)
{
	pstring hostname;

	*hostname = 0;

	/* get my host name */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return False;
	} 

	/* Ensure null termination. */
	hostname[sizeof(hostname)-1] = '\0';

	if (my_name) {
		/* split off any parts after an initial . */
		char *p = strchr(hostname,'.');
		if (p) *p = 0;
		
		fstrcpy(my_name,hostname);
	}
	
	return(True);
}

/****************************************************************************
interpret a protocol description string, with a default
****************************************************************************/
int interpret_protocol(char *str,int def)
{
  if (strequal(str,"NT1"))
    return(PROTOCOL_NT1);
  if (strequal(str,"LANMAN2"))
    return(PROTOCOL_LANMAN2);
  if (strequal(str,"LANMAN1"))
    return(PROTOCOL_LANMAN1);
  if (strequal(str,"CORE"))
    return(PROTOCOL_CORE);
  if (strequal(str,"COREPLUS"))
    return(PROTOCOL_COREPLUS);
  if (strequal(str,"CORE+"))
    return(PROTOCOL_COREPLUS);
  
  DEBUG(0,("Unrecognised protocol level %s\n",str));
  
  return(def);
}

/****************************************************************************
 Return true if a string could be a pure IP address.
****************************************************************************/

BOOL is_ipaddress(const char *str)
{
  BOOL pure_address = True;
  int i;
  
  for (i=0; pure_address && str[i]; i++)
    if (!(isdigit((int)str[i]) || str[i] == '.'))
      pure_address = False;

  /* Check that a pure number is not misinterpreted as an IP */
  pure_address = pure_address && (strchr(str, '.') != NULL);

  return pure_address;
}

/****************************************************************************
interpret an internet address or name into an IP address in 4 byte form
****************************************************************************/

uint32 interpret_addr(char *str)
{
  struct hostent *hp;
  uint32 res;

  if (strcmp(str,"0.0.0.0") == 0) return(0);
  if (strcmp(str,"255.255.255.255") == 0) return(0xFFFFFFFF);

  /* if it's in the form of an IP address then get the lib to interpret it */
  if (is_ipaddress(str)) {
    res = inet_addr(str);
  } else {
    /* otherwise assume it's a network name of some sort and use 
       Get_Hostbyname */
    if ((hp = Get_Hostbyname(str)) == 0) {
      DEBUG(3,("Get_Hostbyname: Unknown host. %s\n",str));
      return 0;
    }
    if(hp->h_addr == NULL) {
      DEBUG(3,("Get_Hostbyname: host address is invalid for host %s\n",str));
      return 0;
    }
    putip((char *)&res,(char *)hp->h_addr);
  }

  if (res == (uint32)-1) return(0);

  return(res);
}

/*******************************************************************
  a convenient addition to interpret_addr()
  ******************************************************************/
struct in_addr *interpret_addr2(char *str)
{
  static struct in_addr ret;
  uint32 a = interpret_addr(str);
  ret.s_addr = a;
  return(&ret);
}

/*******************************************************************
  check if an IP is the 0.0.0.0
  ******************************************************************/
BOOL zero_ip(struct in_addr ip)
{
  uint32 a;
  putip((char *)&a,(char *)&ip);
  return(a == 0);
}


/*******************************************************************
 matchname - determine if host name matches IP address 
 ******************************************************************/
BOOL matchname(char *remotehost,struct in_addr  addr)
{
  struct hostent *hp;
  int     i;
  
  if ((hp = Get_Hostbyname(remotehost)) == 0) {
    DEBUG(0,("Get_Hostbyname(%s): lookup failure.\n", remotehost));
    return False;
  } 

  /*
   * Make sure that gethostbyname() returns the "correct" host name.
   * Unfortunately, gethostbyname("localhost") sometimes yields
   * "localhost.domain". Since the latter host name comes from the
   * local DNS, we just have to trust it (all bets are off if the local
   * DNS is perverted). We always check the address list, though.
   */
  
  if (strcasecmp(remotehost, hp->h_name)
      && strcasecmp(remotehost, "localhost")) {
    DEBUG(0,("host name/name mismatch: %s != %s\n",
	     remotehost, hp->h_name));
    return False;
  }
	
  /* Look up the host address in the address list we just got. */
  for (i = 0; hp->h_addr_list[i]; i++) {
    if (memcmp(hp->h_addr_list[i], (caddr_t) & addr, sizeof(addr)) == 0)
      return True;
  }

  /*
   * The host name does not map to the original host address. Perhaps
   * someone has compromised a name server. More likely someone botched
   * it, but that could be dangerous, too.
   */
  
  DEBUG(0,("host name/address mismatch: %s != %s\n",
	   inet_ntoa(addr), hp->h_name));
  return False;
}


#if (defined(HAVE_NETGROUP) && defined(WITH_AUTOMOUNT))
/******************************************************************
 Remove any mount options such as -rsize=2048,wsize=2048 etc.
 Based on a fix from <Thomas.Hepper@icem.de>.
*******************************************************************/

static void strip_mount_options( pstring *str)
{
  if (**str == '-')
  { 
    char *p = *str;
    while(*p && !isspace(*p))
      p++;
    while(*p && isspace(*p))
      p++;
    if(*p) {
      pstring tmp_str;

      pstrcpy(tmp_str, p);
      pstrcpy(*str, tmp_str);
    }
  }
}

/*******************************************************************
 Patch from jkf@soton.ac.uk
 Split Luke's automount_server into YP lookup and string splitter
 so can easily implement automount_path(). 
 As we may end up doing both, cache the last YP result. 
*******************************************************************/

#ifdef WITH_NISPLUS_HOME
static char *automount_lookup(char *user_name)
{
  static fstring last_key = "";
  static pstring last_value = "";
 
  char *nis_map = (char *)lp_nis_home_map_name();
 
  char nis_domain[NIS_MAXNAMELEN + 1];
  char buffer[NIS_MAXATTRVAL + 1];
  nis_result *result;
  nis_object *object;
  entry_obj  *entry;
 
  strncpy(nis_domain, (char *)nis_local_directory(), NIS_MAXNAMELEN);
  nis_domain[NIS_MAXNAMELEN] = '\0';
 
  DEBUG(5, ("NIS+ Domain: %s\n", nis_domain));
 
  if (strcmp(user_name, last_key))
  {
    slprintf(buffer, sizeof(buffer)-1, "[%s=%s]%s.%s", "key", user_name, nis_map, nis_domain);
    DEBUG(5, ("NIS+ querystring: %s\n", buffer));
 
    if (result = nis_list(buffer, RETURN_RESULT, NULL, NULL))
    {
       if (result->status != NIS_SUCCESS)
      {
        DEBUG(3, ("NIS+ query failed: %s\n", nis_sperrno(result->status)));
        fstrcpy(last_key, ""); pstrcpy(last_value, "");
      }
      else
      {
        object = result->objects.objects_val;
        if (object->zo_data.zo_type == ENTRY_OBJ)
        {
           entry = &object->zo_data.objdata_u.en_data;
           DEBUG(5, ("NIS+ entry type: %s\n", entry->en_type));
           DEBUG(3, ("NIS+ result: %s\n", entry->en_cols.en_cols_val[1].ec_value.ec_value_val));
 
           pstrcpy(last_value, entry->en_cols.en_cols_val[1].ec_value.ec_value_val);
           pstring_sub(last_value, "&", user_name);
           fstrcpy(last_key, user_name);
        }
      }
    }
    nis_freeresult(result);
  }

  strip_mount_options(&last_value);

  DEBUG(4, ("NIS+ Lookup: %s resulted in %s\n", user_name, last_value));
  return last_value;
}
#else /* WITH_NISPLUS_HOME */
static char *automount_lookup(char *user_name)
{
  static fstring last_key = "";
  static pstring last_value = "";

  int nis_error;        /* returned by yp all functions */
  char *nis_result;     /* yp_match inits this */
  int nis_result_len;  /* and set this */
  char *nis_domain;     /* yp_get_default_domain inits this */
  char *nis_map = (char *)lp_nis_home_map_name();

  if ((nis_error = yp_get_default_domain(&nis_domain)) != 0)
  {
    DEBUG(3, ("YP Error: %s\n", yperr_string(nis_error)));
    return last_value;
  }

  DEBUG(5, ("NIS Domain: %s\n", nis_domain));

  if (!strcmp(user_name, last_key))
  {
    nis_result = last_value;
    nis_result_len = strlen(last_value);
    nis_error = 0;
  }
  else
  {
    if ((nis_error = yp_match(nis_domain, nis_map,
                              user_name, strlen(user_name),
                              &nis_result, &nis_result_len)) != 0)
    {
      DEBUG(3, ("YP Error: \"%s\" while looking up \"%s\" in map \"%s\"\n", 
               yperr_string(nis_error), user_name, nis_map));
    }
    if (!nis_error && nis_result_len >= sizeof(pstring))
    {
      nis_result_len = sizeof(pstring)-1;
    }
    fstrcpy(last_key, user_name);
    strncpy(last_value, nis_result, nis_result_len);
    last_value[nis_result_len] = '\0';
  }

  strip_mount_options(&last_value);

  DEBUG(4, ("YP Lookup: %s resulted in %s\n", user_name, last_value));
  return last_value;
}
#endif /* WITH_NISPLUS_HOME */
#endif

/*******************************************************************
 Patch from jkf@soton.ac.uk
 This is Luke's original function with the NIS lookup code
 moved out to a separate function.
*******************************************************************/
static char *automount_server(char *user_name)
{
	static pstring server_name;

	/* use the local machine name as the default */
	/* this will be the default if WITH_AUTOMOUNT is not used or fails */
	pstrcpy(server_name, local_machine);

#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))

	if (lp_nis_home_map())
	{
	        int home_server_len;
		char *automount_value = automount_lookup(user_name);
		home_server_len = strcspn(automount_value,":");
		DEBUG(5, ("NIS lookup succeeded.  Home server length: %d\n",home_server_len));
		if (home_server_len > sizeof(pstring))
		{
			home_server_len = sizeof(pstring);
		}
		strncpy(server_name, automount_value, home_server_len);
                server_name[home_server_len] = '\0';
	}
#endif

	DEBUG(4,("Home server: %s\n", server_name));

	return server_name;
}

/*******************************************************************
 Patch from jkf@soton.ac.uk
 Added this to implement %p (NIS auto-map version of %H)
*******************************************************************/
static char *automount_path(char *user_name)
{
	static pstring server_path;

	/* use the passwd entry as the default */
	/* this will be the default if WITH_AUTOMOUNT is not used or fails */
	/* pstrcpy() copes with get_user_home_dir() returning NULL */
	pstrcpy(server_path, get_user_home_dir(user_name));

#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))

	if (lp_nis_home_map())
	{
	        char *home_path_start;
		char *automount_value = automount_lookup(user_name);
		home_path_start = strchr(automount_value,':');
		if (home_path_start != NULL)
		{
		  DEBUG(5, ("NIS lookup succeeded.  Home path is: %s\n",
		        home_path_start?(home_path_start+1):""));
		  pstrcpy(server_path, home_path_start+1);
		}
	}
#endif

	DEBUG(4,("Home server path: %s\n", server_path));

	return server_path;
}

/*******************************************************************
 Given a pointer to a %$(NAME) expand it as an environment variable.
 Return the number of characters by which the pointer should be advanced.
 Based on code by Branko Cibej <branko.cibej@hermes.si>
 When this is called p points at the '%' character.
********************************************************************/

static size_t expand_env_var(char *p, int len)
{
	fstring envname;
	char *envval;
	char *q, *r;
	int copylen;

	if (p[1] != '$')
		return 1;

	if (p[2] != '(')
		return 2;

	/*
	 * Look for the terminating ')'.
	 */

	if ((q = strchr(p,')')) == NULL) {
		DEBUG(0,("expand_env_var: Unterminated environment variable [%s]\n", p));
		return 2;
	}

	/*
	 * Extract the name from within the %$(NAME) string.
	 */

	r = p+3;
	copylen = MIN((q-r),(sizeof(envname)-1));
	strncpy(envname,r,copylen);
	envname[copylen] = '\0';

	if ((envval = getenv(envname)) == NULL) {
		DEBUG(0,("expand_env_var: Environment variable [%s] not set\n", envname));
		return 2;
	}

	/*
	 * Copy the full %$(NAME) into envname so it
	 * can be replaced.
	 */

	copylen = MIN((q+1-p),(sizeof(envname)-1));
	strncpy(envname,p,copylen);
	envname[copylen] = '\0';
	string_sub(p,envname,envval,len);
	return 0; /* Allow the environment contents to be parsed. */
}

/*******************************************************************
 Substitute strings with useful parameters.
 Rewritten by Stefaan A Eeckels <Stefaan.Eeckels@ecc.lu> and
 Paul Rippin <pr3245@nopc.eurostat.cec.be>.
********************************************************************/

void standard_sub_basic(char *str)
{
	char *s, *p;
	char pidstr[10];
	struct passwd *pass;
	char *username = sam_logon_in_ssb ? samlogon_user : sesssetup_user;

	for (s = str ; s && *s && (p = strchr(s,'%')); s = p )
	{
		int l = sizeof(pstring) - (int)(p-str);

		if (l < 0) {
			DEBUG(0,("ERROR: string overflow by %d in standard_sub_basic(%.50s)\n", 
				 -l, str));
			
			return;
		}

		switch (*(p+1))
		{
			case 'G' :
			{
				if ((pass = Get_Pwnam(username,False))!=NULL) {
					string_sub(p,"%G",gidtoname(pass->pw_gid),l);
				} else {
					p += 2;
				}
				break;
			}
			case 'N' : string_sub(p,"%N", automount_server(username),l); break;
			case 'I' : string_sub(p,"%I", client_addr(Client),l); break;
			case 'L' : string_sub(p,"%L", local_machine,l); break;
			case 'M' : string_sub(p,"%M", client_name(Client),l); break;
			case 'R' : string_sub(p,"%R", remote_proto,l); break;
			case 'T' : string_sub(p,"%T", timestring(False),l); break;
			case 'U' : string_sub(p,"%U", username,l); break;
			case 'a' : string_sub(p,"%a", remote_arch,l); break;
			case 'd' :
			{
				slprintf(pidstr,sizeof(pidstr) - 1, "%d",(int)getpid());
				string_sub(p,"%d", pidstr,l);
				break;
			}
			case 'h' : string_sub(p,"%h", myhostname(),l); break;
			case 'm' : string_sub(p,"%m", remote_machine,l); break;
			case 'v' : string_sub(p,"%v", VERSION,l); break;
			case '$' : p += expand_env_var(p,l); break; /* Expand environment variables */
			case '\0': p++; break; /* don't run off end if last character is % */
			default  : p+=2; break;
		}
	}
	return;
}


/****************************************************************************
 Do some standard substitutions in a string.
****************************************************************************/

void standard_sub(connection_struct *conn,char *str)
{
	char *p, *s, *home;

	for (s=str; (p=strchr(s, '%'));s=p) {
		int l = sizeof(pstring) - (int)(p-str);

		switch (*(p+1)) {
		case 'H': 
			if ((home = get_user_home_dir(conn->user))) {
				string_sub(p,"%H",home,l);
			} else {
				p += 2;
			}
			break;
			
		case 'P': 
			string_sub(p,"%P",conn->connectpath,l); 
			break;
			
		case 'S': 
			string_sub(p,"%S",
				   lp_servicename(SNUM(conn)),l); 
			break;
			
		case 'g': 
			string_sub(p,"%g",
				   gidtoname(conn->gid),l); 
			break;
		case 'u': 
			string_sub(p,"%u",conn->user,l); 
			break;
			
			/* Patch from jkf@soton.ac.uk Left the %N (NIS
			 * server name) in standard_sub_basic as it is
			 * a feature for logon servers, hence uses the
			 * username.  The %p (NIS server path) code is
			 * here as it is used instead of the default
			 * "path =" string in [homes] and so needs the
			 * service name, not the username.  */
		case 'p': 
			string_sub(p,"%p",
				   automount_path(lp_servicename(SNUM(conn))),l); 
			break;
		case '\0': 
			p++; 
			break; /* don't run off the end of the string 
				*/
			
		default: p+=2; 
			break;
		}
	}
	
	standard_sub_basic(str);
}



/*******************************************************************
are two IPs on the same subnet?
********************************************************************/
BOOL same_net(struct in_addr ip1,struct in_addr ip2,struct in_addr mask)
{
  uint32 net1,net2,nmask;

  nmask = ntohl(mask.s_addr);
  net1  = ntohl(ip1.s_addr);
  net2  = ntohl(ip2.s_addr);
            
  return((net1 & nmask) == (net2 & nmask));
}


/****************************************************************************
a wrapper for gethostbyname() that tries with all lower and all upper case 
if the initial name fails
****************************************************************************/
struct hostent *Get_Hostbyname(const char *name)
{
  char *name2 = strdup(name);
  struct hostent *ret;

  if (!name2)
    {
      DEBUG(0,("Memory allocation error in Get_Hostbyname! panic\n"));
      exit(0);
    }

   
  /* 
   * This next test is redundent and causes some systems (with
   * broken isalnum() calls) problems.
   * JRA.
   */

#if 0
  if (!isalnum(*name2))
    {
      free(name2);
      return(NULL);
    }
#endif /* 0 */

  ret = sys_gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }

  /* try with all lowercase */
  strlower(name2);
  ret = sys_gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }

  /* try with all uppercase */
  strupper(name2);
  ret = sys_gethostbyname(name2);
  if (ret != NULL)
    {
      free(name2);
      return(ret);
    }
  
  /* nothing works :-( */
  free(name2);
  return(NULL);
}


/****************************************************************************
check if a process exists. Does this work on all unixes?
****************************************************************************/

BOOL process_exists(pid_t pid)
{
	return(kill(pid,0) == 0 || errno != ESRCH);
}


/*******************************************************************
turn a uid into a user name
********************************************************************/
char *uidtoname(uid_t uid)
{
  static char name[40];
  struct passwd *pass = sys_getpwuid(uid);
  if (pass) return(pass->pw_name);
  slprintf(name, sizeof(name) - 1, "%d",(int)uid);
  return(name);
}


/*******************************************************************
turn a gid into a group name
********************************************************************/

char *gidtoname(gid_t gid)
{
	static char name[40];
	struct group *grp = getgrgid(gid);
	if (grp) return(grp->gr_name);
	slprintf(name,sizeof(name) - 1, "%d",(int)gid);
	return(name);
}

/*******************************************************************
turn a user name into a uid
********************************************************************/
uid_t nametouid(const char *name)
{
	struct passwd *pass;
	char *p;
	uid_t u;

	u = strtol(name, &p, 0);
	if (p != name) return u;

	pass = sys_getpwnam(name);
	if (pass) return(pass->pw_uid);
	return (uid_t)-1;
}

/*******************************************************************
turn a group name into a gid
********************************************************************/
gid_t nametogid(const char *name)
{
	struct group *grp;
	char *p;
	gid_t g;

	g = strtol(name, &p, 0);
	if (p != name) return g;

	grp = getgrnam(name);
	if (grp) return(grp->gr_gid);
	return (gid_t)-1;
}

/*******************************************************************
something really nasty happened - panic!
********************************************************************/
void smb_panic(char *why)
{
	char *cmd = lp_panic_action();
	if (cmd && *cmd) {
		system(cmd);
	}
	DEBUG(0,("PANIC: %s\n", why));
	dbgflush();
	abort();
}


/*******************************************************************
a readdir wrapper which just returns the file name
********************************************************************/
char *readdirname(DIR *p)
{
	SMB_STRUCT_DIRENT *ptr;
	char *dname;

	if (!p) return(NULL);
  
	ptr = (SMB_STRUCT_DIRENT *)sys_readdir(p);
	if (!ptr) return(NULL);

	dname = ptr->d_name;

#ifdef NEXT2
	if (telldir(p) < 0) return(NULL);
#endif

#ifdef HAVE_BROKEN_READDIR
	/* using /usr/ucb/cc is BAD */
	dname = dname - 2;
#endif

	{
		static pstring buf;
		memcpy(buf, dname, NAMLEN(ptr)+1);
		dname = buf;
	}

	return(dname);
}

/*******************************************************************
 Utility function used to decide if the last component 
 of a path matches a (possibly wildcarded) entry in a namelist.
********************************************************************/

BOOL is_in_path(char *name, name_compare_entry *namelist)
{
  pstring last_component;
  char *p;

  DEBUG(8, ("is_in_path: %s\n", name));

  /* if we have no list it's obviously not in the path */
  if((namelist == NULL ) || ((namelist != NULL) && (namelist[0].name == NULL))) 
  {
    DEBUG(8,("is_in_path: no name list.\n"));
    return False;
  }

  /* Get the last component of the unix name. */
  p = strrchr(name, '/');
  strncpy(last_component, p ? ++p : name, sizeof(last_component)-1);
  last_component[sizeof(last_component)-1] = '\0'; 

  for(; namelist->name != NULL; namelist++)
  {
    if(namelist->is_wild)
    {
      /* 
       * Look for a wildcard match. Use the old
       * 'unix style' mask match, rather than the
       * new NT one.
       */
      if (unix_mask_match(last_component, namelist->name, case_sensitive))
      {
         DEBUG(8,("is_in_path: mask match succeeded\n"));
         return True;
      }
    }
    else
    {
      if((case_sensitive && (strcmp(last_component, namelist->name) == 0))||
       (!case_sensitive && (StrCaseCmp(last_component, namelist->name) == 0)))
        {
         DEBUG(8,("is_in_path: match succeeded\n"));
         return True;
        }
    }
  }
  DEBUG(8,("is_in_path: match not found\n"));
 
  return False;
}

/*******************************************************************
 Strip a '/' separated list into an array of 
 name_compare_enties structures suitable for 
 passing to is_in_path(). We do this for
 speed so we can pre-parse all the names in the list 
 and don't do it for each call to is_in_path().
 namelist is modified here and is assumed to be 
 a copy owned by the caller.
 We also check if the entry contains a wildcard to
 remove a potentially expensive call to mask_match
 if possible.
********************************************************************/
 
void set_namearray(name_compare_entry **ppname_array, char *namelist)
{
  char *name_end;
  char *nameptr = namelist;
  int num_entries = 0;
  int i;

  (*ppname_array) = NULL;

  if((nameptr == NULL ) || ((nameptr != NULL) && (*nameptr == '\0'))) 
    return;

  /* We need to make two passes over the string. The
     first to count the number of elements, the second
     to split it.
   */
  while(*nameptr) 
    {
      if ( *nameptr == '/' ) 
        {
          /* cope with multiple (useless) /s) */
          nameptr++;
          continue;
        }
      /* find the next / */
      name_end = strchr(nameptr, '/');

      /* oops - the last check for a / didn't find one. */
      if (name_end == NULL)
        break;

      /* next segment please */
      nameptr = name_end + 1;
      num_entries++;
    }

  if(num_entries == 0)
    return;

  if(( (*ppname_array) = (name_compare_entry *)malloc( 
           (num_entries + 1) * sizeof(name_compare_entry))) == NULL)
        {
    DEBUG(0,("set_namearray: malloc fail\n"));
    return;
        }

  /* Now copy out the names */
  nameptr = namelist;
  i = 0;
  while(*nameptr)
             {
      if ( *nameptr == '/' ) 
      {
          /* cope with multiple (useless) /s) */
          nameptr++;
          continue;
      }
      /* find the next / */
      if ((name_end = strchr(nameptr, '/')) != NULL) 
      {
          *name_end = 0;
         }

      /* oops - the last check for a / didn't find one. */
      if(name_end == NULL) 
        break;

      (*ppname_array)[i].is_wild = ((strchr( nameptr, '?')!=NULL) ||
                                (strchr( nameptr, '*')!=NULL));
      if(((*ppname_array)[i].name = strdup(nameptr)) == NULL)
      {
        DEBUG(0,("set_namearray: malloc fail (1)\n"));
        return;
      }

      /* next segment please */
      nameptr = name_end + 1;
      i++;
    }
  
  (*ppname_array)[i].name = NULL;

  return;
}

/****************************************************************************
routine to free a namearray.
****************************************************************************/

void free_namearray(name_compare_entry *name_array)
{
  if(name_array == 0)
    return;

  if(name_array->name != NULL)
    free(name_array->name);

  free((char *)name_array);
}

/****************************************************************************
 Pathetically try and map a 64 bit lock offset into 31 bits. I hate Windows :-).
****************************************************************************/

uint32 map_lock_offset(uint32 high, uint32 low)
{
  unsigned int i;
  uint32 mask = 0;
  uint32 highcopy = high;

  /*
   * Try and find out how many significant bits there are in high.
   */

  for(i = 0; highcopy; i++)
    highcopy >>= 1;

  /*
   * We use 31 bits not 32 here as POSIX
   * lock offsets may not be negative.
   */

  mask = (~0) << (31 - i);

  if(low & mask)
    return 0; /* Fail. */

  high <<= (31 - i);

  return (high|low);
}

/****************************************************************************
routine to do file locking
****************************************************************************/

BOOL fcntl_lock(int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
#if HAVE_FCNTL_LOCK
  SMB_STRUCT_FLOCK lock;
  int ret;
#if defined(LARGE_SMB_OFF_T)
  /*
   * In the 64 bit locking case we store the original
   * values in case we have to map to a 32 bit lock on
   * a filesystem that doesn't support 64 bit locks.
   */
  SMB_OFF_T orig_offset = offset;
  SMB_OFF_T orig_count = count;
#endif /* LARGE_SMB_OFF_T */

  if(lp_ole_locking_compat()) {
    SMB_OFF_T mask2= ((SMB_OFF_T)0x3) << (SMB_OFF_T_BITS-4);
    SMB_OFF_T mask = (mask2<<2);

    /* make sure the count is reasonable, we might kill the lockd otherwise */
    count &= ~mask;

    /* the offset is often strange - remove 2 of its bits if either of
       the top two bits are set. Shift the top ones by two bits. This
       still allows OLE2 apps to operate, but should stop lockd from
       dieing */
    if ((offset & mask) != 0)
      offset = (offset & ~mask) | (((offset & mask) >> 2) & mask2);
  } else {
    SMB_OFF_T mask2 = ((SMB_OFF_T)0x4) << (SMB_OFF_T_BITS-4);
    SMB_OFF_T mask = (mask2<<1);
    SMB_OFF_T neg_mask = ~mask;

    /* interpret negative counts as large numbers */
    if (count < 0)
      count &= ~mask;

    /* no negative offsets */
    if(offset < 0)
      offset &= ~mask;

    /* count + offset must be in range */
    while ((offset < 0 || (offset + count < 0)) && mask)
    {
      offset &= ~mask;
      mask = ((mask >> 1) & neg_mask);
    }
  }

  DEBUG(8,("fcntl_lock %d %d %.0f %.0f %d\n",fd,op,(double)offset,(double)count,type));

  lock.l_type = type;
  lock.l_whence = SEEK_SET;
  lock.l_start = offset;
  lock.l_len = count;
  lock.l_pid = 0;

  errno = 0;

  ret = fcntl(fd,op,&lock);
  if (errno == EFBIG)
  {
    if( DEBUGLVL( 0 ))
    {
      dbgtext("fcntl_lock: WARNING: lock request at offset %.0f, length %.0f returned\n", (double)offset,(double)count);
      dbgtext("a 'file too large' error. This can happen when using 64 bit lock offsets\n");
      dbgtext("on 32 bit NFS mounted file systems. Retrying with 32 bit truncated length.\n");
    }
    /* 32 bit NFS file system, retry with smaller offset */
    errno = 0;
    lock.l_len = count & 0x7fffffff;
    ret = fcntl(fd,op,&lock);
  }

  if (errno != 0)
    DEBUG(3,("fcntl lock gave errno %d (%s)\n",errno,strerror(errno)));

  /* a lock query */
  if (op == SMB_F_GETLK)
  {
    if ((ret != -1) &&
        (lock.l_type != F_UNLCK) && 
        (lock.l_pid != 0) && 
        (lock.l_pid != getpid()))
    {
      DEBUG(3,("fd %d is locked by pid %d\n",fd,(int)lock.l_pid));
      return(True);
    }

    /* it must be not locked or locked by me */
    return(False);
  }

  /* a lock set or unset */
  if (ret == -1)
  {
    DEBUG(3,("lock failed at offset %.0f count %.0f op %d type %d (%s)\n",
          (double)offset,(double)count,op,type,strerror(errno)));

    /* perhaps it doesn't support this sort of locking?? */
    if (errno == EINVAL)
    {

#if defined(LARGE_SMB_OFF_T)
      {
        /*
         * Ok - if we get here then we have a 64 bit lock request
         * that has returned EINVAL. Try and map to 31 bits for offset
         * and length and try again. This may happen if a filesystem
         * doesn't support 64 bit offsets (efs/ufs) although the underlying
         * OS does.
         */
        uint32 off_low = (orig_offset & 0xFFFFFFFF);
        uint32 off_high = ((orig_offset >> 32) & 0xFFFFFFFF);

        lock.l_len = (orig_count & 0x7FFFFFFF);
        lock.l_start = (SMB_OFF_T)map_lock_offset(off_high, off_low);
        ret = fcntl(fd,op,&lock);
        if (ret == -1)
        {
          if (errno == EINVAL)
          {
            DEBUG(3,("locking not supported? returning True\n"));
            return(True);
          }
          return False;
        }
        DEBUG(3,("64 -> 32 bit modified lock call successful\n"));
        return True;
      }
#else /* LARGE_SMB_OFF_T */
      DEBUG(3,("locking not supported? returning True\n"));
      return(True);
#endif /* LARGE_SMB_OFF_T */
    }

    return(False);
  }

  /* everything went OK */
  DEBUG(8,("Lock call successful\n"));

  return(True);
#else
  return(False);
#endif
}

/*******************************************************************
is the name specified one of my netbios names
returns true is it is equal, false otherwise
********************************************************************/
BOOL is_myname(char *s)
{
  int n;
  BOOL ret = False;

  for (n=0; my_netbios_names[n]; n++) {
    if (strequal(my_netbios_names[n], s))
      ret=True;
  }
  DEBUG(8, ("is_myname(\"%s\") returns %d\n", s, ret));
  return(ret);
}

/*******************************************************************
set the horrid remote_arch string based on an enum.
********************************************************************/
void set_remote_arch(enum remote_arch_types type)
{
  ra_type = type;
  switch( type )
  {
  case RA_WFWG:
    fstrcpy(remote_arch, "WfWg");
    return;
  case RA_OS2:
    fstrcpy(remote_arch, "OS2");
    return;
  case RA_WIN95:
    fstrcpy(remote_arch, "Win95");
    return;
  case RA_WINNT:
    fstrcpy(remote_arch, "WinNT");
    return;
  case RA_WIN2K:
    fstrcpy(remote_arch, "Win2K");
    return;
  case RA_SAMBA:
    fstrcpy(remote_arch,"Samba");
    return;
  default:
    ra_type = RA_UNKNOWN;
    fstrcpy(remote_arch, "UNKNOWN");
    break;
  }
}

/*******************************************************************
 Get the remote_arch type.
********************************************************************/
enum remote_arch_types get_remote_arch(void)
{
  return ra_type;
}


/*******************************************************************
align a pointer to a multiple of 2 bytes
********************************************************************/
char *align2(char *q, char *base)
{
    if (PTR_DIFF(q, base) & 1)
    {
        q++;
    }
    return q;
}

/*******************************************************************
 align a pointer to a multiple of 4 bytes.
 ********************************************************************/
char *align4(char *q, char *base)
{
    int mod = PTR_DIFF(q, base) & 3;
    if (mod != 0)
    {
        q += 4-mod;
    }
    return q;
}

void out_ascii(FILE *f, unsigned char *buf,int len)
{
	int i;
	for (i=0;i<len;i++)
	{
		fprintf(f, "%c", isprint(buf[i])?buf[i]:'.');
	}
}

void out_data(FILE *f,char *buf1,int len, int per_line)
{
	unsigned char *buf = (unsigned char *)buf1;
	int i=0;
	if (len<=0)
	{
		return;
	}

	fprintf(f, "[%03X] ",i);
	for (i=0;i<len;)
	{
		fprintf(f, "%02X ",(int)buf[i]);
		i++;
		if (i%(per_line/2) == 0) fprintf(f, " ");
		if (i%per_line == 0)
		{      
			out_ascii(f,&buf[i-per_line  ],per_line/2); fprintf(f, " ");
			out_ascii(f,&buf[i-per_line/2],per_line/2); fprintf(f, "\n");
			if (i<len) fprintf(f, "[%03X] ",i);
		}
	}
	if ((i%per_line) != 0)
	{
		int n;

		n = per_line - (i%per_line);
		fprintf(f, " ");
		if (n>(per_line/2)) fprintf(f, " ");
		while (n--)
		{
			fprintf(f, "   ");
		}
		n = MIN(per_line/2,i%per_line);
		out_ascii(f,&buf[i-(i%per_line)],n); fprintf(f, " ");
		n = (i%per_line) - n;
		if (n>0) out_ascii(f,&buf[i-n],n); 
		fprintf(f, "\n");    
	}
}

void print_asc(int level, unsigned char *buf,int len)
{
	int i;
	for (i=0;i<len;i++)
		DEBUG(level,("%c", isprint(buf[i])?buf[i]:'.'));
}

void dump_data(int level,char *buf1,int len)
{
  unsigned char *buf = (unsigned char *)buf1;
  int i=0;
  if (len<=0) return;

  DEBUG(level,("[%03X] ",i));
  for (i=0;i<len;) {
    DEBUG(level,("%02X ",(int)buf[i]));
    i++;
    if (i%8 == 0) DEBUG(level,(" "));
    if (i%16 == 0) {      
      print_asc(level,&buf[i-16],8); DEBUG(level,(" "));
      print_asc(level,&buf[i-8],8); DEBUG(level,("\n"));
      if (i<len) DEBUG(level,("[%03X] ",i));
    }
  }
  if (i%16) {
    int n;

    n = 16 - (i%16);
    DEBUG(level,(" "));
    if (n>8) DEBUG(level,(" "));
    while (n--) DEBUG(level,("   "));

    n = MIN(8,i%16);
    print_asc(level,&buf[i-(i%16)],n); DEBUG(level,(" "));
    n = (i%16) - n;
    if (n>0) print_asc(level,&buf[i-n],n); 
    DEBUG(level,("\n"));    
  }
}

char *tab_depth(int depth)
{
	static pstring spaces;
	memset(spaces, ' ', depth * 4);
	spaces[depth * 4] = 0;
	return spaces;
}

/*****************************************************************************
 * Provide a checksum on a string
 *
 *  Input:  s - the null-terminated character string for which the checksum
 *              will be calculated.
 *
 *  Output: The checksum value calculated for s.
 *
 * ****************************************************************************
 */
int str_checksum(const char *s)
{
	int res = 0;
	int c;
	int i=0;
	
	while(*s) {
		c = *s;
		res ^= (c << (i % 15)) ^ (c >> (15-(i%15)));
		s++;
		i++;
	}
	return(res);
} /* str_checksum */



/*****************************************************************
zero a memory area then free it. Used to catch bugs faster
*****************************************************************/  
void zero_free(void *p, size_t size)
{
	memset(p, 0, size);
	free(p);
}


/*****************************************************************
set our open file limit to a requested max and return the limit
*****************************************************************/  
int set_maxfiles(int requested_max)
{
#if (defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE))
	struct rlimit rlp;
	int saved_current_limit;

	if(getrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: getrlimit (1) for RLIMIT_NOFILE failed with error %s\n",
			strerror(errno) ));
		/* just guess... */
		return requested_max;
	}

	/* 
     * Set the fd limit to be real_max_open_files + MAX_OPEN_FUDGEFACTOR to
	 * account for the extra fd we need 
	 * as well as the log files and standard
	 * handles etc. Save the limit we want to set in case
	 * we are running on an OS that doesn't support this limit (AIX)
	 * which always returns RLIM_INFINITY for rlp.rlim_max.
	 */

	saved_current_limit = rlp.rlim_cur = MIN(requested_max,rlp.rlim_max);

	if(setrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: setrlimit for RLIMIT_NOFILE for %d files failed with error %s\n", 
			(int)rlp.rlim_cur, strerror(errno) ));
		/* just guess... */
		return saved_current_limit;
	}

	if(getrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: getrlimit (2) for RLIMIT_NOFILE failed with error %s\n",
			strerror(errno) ));
		/* just guess... */
		return saved_current_limit;
    }

#if defined(RLIM_INFINITY)
	if(rlp.rlim_cur == RLIM_INFINITY)
		return saved_current_limit;
#endif

    if((int)rlp.rlim_cur > saved_current_limit)
		return saved_current_limit;

	return rlp.rlim_cur;
#else /* !defined(HAVE_GETRLIMIT) || !defined(RLIMIT_NOFILE) */
	/*
	 * No way to know - just guess...
	 */
	return requested_max;
#endif
}

/*****************************************************************
 splits out the start of the key (HKLM or HKU) and the rest of the key
 *****************************************************************/  
BOOL reg_split_key(char *full_keyname, uint32 *reg_type, char *key_name)
{
	pstring tmp;

	if (!next_token(&full_keyname, tmp, "\\", sizeof(tmp)))
	{
		return False;
	}

	(*reg_type) = 0;

	DEBUG(10, ("reg_split_key: hive %s\n", tmp));

	if (strequal(tmp, "HKLM") || strequal(tmp, "HKEY_LOCAL_MACHINE"))
	{
		(*reg_type) = HKEY_LOCAL_MACHINE;
	}
	else if (strequal(tmp, "HKU") || strequal(tmp, "HKEY_USERS"))
	{
		(*reg_type) = HKEY_USERS;
	}
	else
	{
		DEBUG(10,("reg_split_key: unrecognised hive key %s\n", tmp));
		return False;
	}
	
	if (next_token(NULL, tmp, "\n\r", sizeof(tmp)))
	{
		fstrcpy(key_name, tmp);
	}
	else
	{
		key_name[0] = 0;
	}

	DEBUG(10, ("reg_split_key: name %s\n", key_name));

	return True;
}


/*****************************************************************
like mktemp() but make sure that no % characters are used
% characters are bad for us because of the macro subs
 *****************************************************************/  
char *smbd_mktemp(char *template)
{
	char *p = mktemp(template);
	char *p2;
	SMB_STRUCT_STAT st;

	if (!p) return NULL;

	while ((p2=strchr(p,'%'))) {
		p2[0] = 'A';
		while (sys_stat(p,&st) == 0 && p2[0] < 'Z') {
			/* damn, it exists */
			p2[0]++;
		}
		if (p2[0] == 'Z') {
			/* oh well ... better return something */
			p2[0] = '%';
			return p;
		}
	}

	return p;
}

/*****************************************************************
possibly replace mkstemp if it is broken
 *****************************************************************/  
int smb_mkstemp(char *template)
{
#if HAVE_SECURE_MKSTEMP
	return mkstemp(template);
#else
	/* have a reasonable go at emulating it. Hope that
	   the system mktemp() isn't completly hopeless */
	if (!mktemp(template)) return -1;
	return open(template, O_CREAT|O_EXCL|O_RDWR, 0600);
#endif
}


/*****************************************************************
like strdup but for memory
 *****************************************************************/  
void *memdup(void *p, size_t size)
{
	void *p2;
	p2 = malloc(size);
	if (!p2) return NULL;
	memcpy(p2, p, size);
	return p2;
}

/*****************************************************************
get local hostname and cache result
 *****************************************************************/  
char *myhostname(void)
{
	static pstring ret;
	if (ret[0] == 0) {
		get_myname(ret);
	}
	return ret;
}

/*******************************************************************
 Given a filename - get its directory name
 NB: Returned in static storage.  Caveats:
 o  Not safe in thread environment.
 o  Caller must not free.
 o  If caller wishes to preserve, they should copy.
********************************************************************/

char *parent_dirname(const char *path)
{
	static pstring dirpath;
	char *p;

	if (!path)
		return(NULL);

	pstrcpy(dirpath, path);
	p = strrchr(dirpath, '/');  /* Find final '/', if any */
	if (!p) {
		pstrcpy(dirpath, ".");    /* No final "/", so dir is "." */
	} else {
		if (p == dirpath)
			++p;    /* For root "/", leave "/" in place */
		*p = '\0';
	}
	return dirpath;
}
