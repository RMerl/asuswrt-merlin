/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Username handling
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
extern int DEBUGLEVEL;

/* internal functions */
static struct passwd *uname_string_combinations(char *s, struct passwd * (*fn) (char *), int N);
static struct passwd *uname_string_combinations2(char *s, int offset, struct passwd * (*fn) (char *), int N);

/****************************************************************************
 Get a users home directory.
****************************************************************************/

char *get_user_home_dir(char *user)
{
  static struct passwd *pass;

  pass = Get_Pwnam(user, False);

  if (!pass) return(NULL);
  return(pass->pw_dir);      
}


/*******************************************************************
 Map a username from a dos name to a unix name by looking in the username
 map. Note that this modifies the name in place.
 This is the main function that should be called *once* on
 any incoming or new username - in order to canonicalize the name.
 This is being done to de-couple the case conversions from the user mapping
 function. Previously, the map_username was being called
 every time Get_Pwnam was called.
 Returns True if username was changed, false otherwise.
********************************************************************/

BOOL map_username(char *user)
{
  static BOOL initialised=False;
  static fstring last_from,last_to;
  FILE *f;
  char *mapfile = lp_username_map();
  char *s;
  pstring buf;
  BOOL mapped_user = False;

  if (!*user)
    return False;

  if (!*mapfile)
    return False;

  if (!initialised) {
    *last_from = *last_to = 0;
    initialised = True;
  }

  if (strequal(user,last_to))
    return False;

  if (strequal(user,last_from)) {
    DEBUG(3,("Mapped user %s to %s\n",user,last_to));
    fstrcpy(user,last_to);
    return True;
  }
  
  f = sys_fopen(mapfile,"r");
  if (!f) {
    DEBUG(0,("can't open username map %s\n",mapfile));
    return False;
  }

  DEBUG(4,("Scanning username map %s\n",mapfile));

  while((s=fgets_slash(buf,sizeof(buf),f))!=NULL) {
    char *unixname = s;
    char *dosname = strchr(unixname,'=');
    BOOL return_if_mapped = False;

    if (!dosname)
      continue;

    *dosname++ = 0;

    while (isspace(*unixname))
      unixname++;
    if ('!' == *unixname) {
      return_if_mapped = True;
      unixname++;
      while (*unixname && isspace(*unixname))
        unixname++;
    }
    
    if (!*unixname || strchr("#;",*unixname))
      continue;

    {
      int l = strlen(unixname);
      while (l && isspace(unixname[l-1])) {
        unixname[l-1] = 0;
        l--;
      }
    }

    if (strchr(dosname,'*') || user_in_list(user,dosname)) {
      DEBUG(3,("Mapped user %s to %s\n",user,unixname));
      mapped_user = True;
      fstrcpy(last_from,user);
      sscanf(unixname,"%s",user);
      fstrcpy(last_to,user);
      if(return_if_mapped) { 
        fclose(f);
        return True;
      }
    }
  }

  fclose(f);

  /*
   * Setup the last_from and last_to as an optimization so 
   * that we don't scan the file again for the same user.
   */
  fstrcpy(last_from,user);
  fstrcpy(last_to,user);

  return mapped_user;
}

/****************************************************************************
 Get_Pwnam wrapper
****************************************************************************/

static struct passwd *_Get_Pwnam(char *s)
{
  struct passwd *ret;

  ret = sys_getpwnam(s);
  if (ret) {
#ifdef HAVE_GETPWANAM
    struct passwd_adjunct *pwret;
    pwret = getpwanam(s);
    if (pwret && pwret->pwa_passwd) {
      pstrcpy(ret->pw_passwd,pwret->pwa_passwd);
    }
#endif
  }

  return(ret);
}


/****************************************************************************
 A wrapper for getpwnam() that tries with all lower and all upper case 
 if the initial name fails. Also tried with first letter capitalised
 Note that this can change user!
****************************************************************************/

struct passwd *Get_Pwnam(char *user,BOOL allow_change)
{
  fstring user2;
  int last_char;
  int usernamelevel = lp_usernamelevel();

  struct passwd *ret;  

  if (!user || !(*user))
    return(NULL);

  StrnCpy(user2,user,sizeof(user2)-1);

  if (!allow_change) {
    user = &user2[0];
  }

  ret = _Get_Pwnam(user);
  if (ret)
    return(ret);

  strlower(user);
  ret = _Get_Pwnam(user);
  if (ret)
    return(ret);

  strupper(user);
  ret = _Get_Pwnam(user);
  if (ret)
    return(ret);

  /* Try with first letter capitalised. */
  if (strlen(user) > 1)
    strlower(user+1);  
  ret = _Get_Pwnam(user);
  if (ret)
    return(ret);

  /* try with last letter capitalised */
  strlower(user);
  last_char = strlen(user)-1;
  user[last_char] = toupper(user[last_char]);
  ret = _Get_Pwnam(user);
  if (ret)
    return(ret);

  /* Try all combinations up to usernamelevel. */
  strlower(user);
  ret = uname_string_combinations(user, _Get_Pwnam, usernamelevel);
  if (ret)
    return(ret);

  if (allow_change)
    fstrcpy(user,user2);

  return(NULL);
}

/****************************************************************************
 Check if a user is in a netgroup user list.
****************************************************************************/

static BOOL user_in_netgroup_list(char *user,char *ngname)
{
#ifdef HAVE_NETGROUP
  static char *mydomain = NULL;
  if (mydomain == NULL)
    yp_get_default_domain(&mydomain);

  if(mydomain == NULL) {
    DEBUG(5,("Unable to get default yp domain\n"));
  } else {
    DEBUG(5,("looking for user %s of domain %s in netgroup %s\n",
          user, mydomain, ngname));
    DEBUG(5,("innetgr is %s\n",
          innetgr(ngname, NULL, user, mydomain)
          ? "TRUE" : "FALSE"));

    if (innetgr(ngname, NULL, user, mydomain))
      return (True);
  }
#endif /* HAVE_NETGROUP */
  return False;
}

/****************************************************************************
 Check if a user is in a UNIX user list.
****************************************************************************/

static BOOL user_in_group_list(char *user,char *gname)
{
#ifdef HAVE_GETGRENT
	struct group *gptr;
	char **member;  
	struct passwd *pass = Get_Pwnam(user,False);

	if (pass) { 
		gptr = getgrgid(pass->pw_gid);
		if (gptr && strequal(gptr->gr_name,gname))
			return(True); 
	} 

	setgrent();
	while ((gptr = (struct group *)getgrent())) {
		if (!strequal(gptr->gr_name,gname))
			continue;
		member = gptr->gr_mem;
		while (member && *member) {
			if (strequal(*member,user)) {
				endgrent();
				return(True);
			}
			member++;
		}
	}

	endgrent();
#endif /* HAVE_GETGRNAM */
	return False;
}	      

/****************************************************************************
 Check if a user is in a user list - can check combinations of UNIX
 and netgroup lists.
****************************************************************************/

BOOL user_in_list(char *user,char *list)
{
  pstring tok;
  char *p=list;

  while (next_token(&p,tok,LIST_SEP, sizeof(tok))) {
    /*
     * Check raw username.
     */
    if (strequal(user,tok))
      return(True);

    /*
     * Now check to see if any combination
     * of UNIX and netgroups has been specified.
     */

    if(*tok == '@') {
      /*
       * Old behaviour. Check netgroup list
       * followed by UNIX list.
       */
      if(user_in_netgroup_list(user,&tok[1]))
        return True;
      if(user_in_group_list(user,&tok[1]))
        return True;
    } else if (*tok == '+') {

      if(tok[1] == '&') {
        /*
         * Search UNIX list followed by netgroup.
         */
        if(user_in_group_list(user,&tok[2]))
          return True;
        if(user_in_netgroup_list(user,&tok[2]))
          return True;

      } else {

        /*
         * Just search UNIX list.
         */

        if(user_in_group_list(user,&tok[1]))
          return True;
      }

    } else if (*tok == '&') {

      if(tok[1] == '+') {
        /*
         * Search netgroup list followed by UNIX list.
         */
        if(user_in_netgroup_list(user,&tok[2]))
          return True;
        if(user_in_group_list(user,&tok[2]))
          return True;
      } else {
        /*
         * Just search netgroup list.
         */
        if(user_in_netgroup_list(user,&tok[1]))
          return True;
      }
    }
  }
  return(False);
}

/* The functions below have been taken from password.c and slightly modified */
/****************************************************************************
 Apply a function to upper/lower case combinations
 of a string and return true if one of them returns true.
 Try all combinations with N uppercase letters.
 offset is the first char to try and change (start with 0)
 it assumes the string starts lowercased
****************************************************************************/

static struct passwd *uname_string_combinations2(char *s,int offset,struct passwd *(*fn)(char *),int N)
{
  ssize_t len = (ssize_t)strlen(s);
  int i;
  struct passwd *ret;

#ifdef PASSWORD_LENGTH
  len = MIN(len,PASSWORD_LENGTH);
#endif

  if (N <= 0 || offset >= len)
    return(fn(s));

  for (i=offset;i<(len-(N-1));i++) {
    char c = s[i];
    if (!islower(c))
      continue;
    s[i] = toupper(c);
    ret = uname_string_combinations2(s,i+1,fn,N-1);
    if(ret)
      return(ret);
    s[i] = c;
  }
  return(NULL);
}

/****************************************************************************
 Apply a function to upper/lower case combinations
 of a string and return true if one of them returns true.
 Try all combinations with up to N uppercase letters.
 offset is the first char to try and change (start with 0)
 it assumes the string starts lowercased
****************************************************************************/

static struct passwd * uname_string_combinations(char *s,struct passwd * (*fn)(char *),int N)
{
  int n;
  struct passwd *ret;

  for (n=1;n<=N;n++) {
    ret = uname_string_combinations2(s,0,fn,n);
    if(ret)
      return(ret);
  }
  return(NULL);
}
