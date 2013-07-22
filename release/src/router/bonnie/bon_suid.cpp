#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "bonnie.h"

int bon_setugid(CPCCHAR userName, CPCCHAR groupName, bool quiet)
{
  int id = 0;
  uid_t userId = 0;
  gid_t groupId = 0;
  bool setGroup = false;
  struct passwd *pw;
  struct group *gr;
  if(userName)
  {
    if(sscanf(userName, "%d", &id) == 1)
    {
      userId = uid_t(id);
      pw = getpwuid(userId);
      if(pw)
      {
        groupId = pw->pw_gid;
        setGroup = true;
      }
      else
      {
        gr = getgrnam("nogroup");
        if(gr)
          groupId = gr->gr_gid;
          setGroup = true;
      }
    }
    else
    {
      pw = getpwnam(userName);
      if(!pw)
      {
        fprintf(stderr, "Can't find user %s\n", userName);
        return 1;
      }
      userId = pw->pw_uid;
      groupId = pw->pw_gid;
      setGroup = true;
    }
  }
  if(groupName)
  {
    if(sscanf(groupName, "%d", &id) == 1)
    {
      groupId = gid_t(id);
      setGroup = true;
    }
    else
    {
      gr = getgrnam(groupName);
      if(!gr)
      {
        fprintf(stderr, "Can't find group %s\n", groupName);
        return 1;
      }
      groupId = gr->gr_gid;
      setGroup = true;
    }
  }
  if(setGroup)
  {
    if(setgid(groupId))
    {
      fprintf(stderr, "Can't set gid to %d.\n", int(groupId));
      return 1;
    }
  }
  if(setuid(userId))
  {
    fprintf(stderr, "Can't set uid to %d.\n", int(userId));
    return 1;
  }
  if(!quiet)
    fprintf(stderr, "Using uid:%d, gid:%d.\n", int(getuid()), int(getgid()));
  return 0;
}
