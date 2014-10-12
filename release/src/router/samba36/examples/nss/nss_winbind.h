/* 
   nss sample code for extended winbindd functionality

   Copyright (C) Andrew Tridgell (tridge@samba.org)   
   Copyright (C) Volker Lendecke (vl@samba.org)

   you are free to use this code in any way you see fit, including
   without restriction, using this code in your own products. You do
   not need to give any attribution.
*/

#define _GNU_SOURCE

#include <pwd.h>
#include <grp.h>

struct nss_state {
	void *dl_handle;
	char *nss_name;
	char pwnam_buf[512];
};

/*
  establish a link to the nss library
  Return 0 on success and -1 on error
*/
int nss_open(struct nss_state *nss, const char *nss_path);

/*
  close and cleanup a nss state
*/
void nss_close(struct nss_state *nss);

/*
  make a getpwnam call. 
  Return 0 on success and -1 on error
*/
int nss_getpwent(struct nss_state *nss, struct passwd *pwd);

/*
  make a setpwent call. 
  Return 0 on success and -1 on error
*/
int nss_setpwent(struct nss_state *nss);

/*
  make a endpwent call. 
  Return 0 on success and -1 on error
*/
int nss_endpwent(struct nss_state *nss);

/*
  convert a name to a SID
  caller frees
  Return 0 on success and -1 on error
*/
int nss_nametosid(struct nss_state *nss, const char *name, char **sid);

/*
  convert a SID to a name
  caller frees
  Return 0 on success and -1 on error
*/
int nss_sidtoname(struct nss_state *nss, const char *sid, char **name);

/*
  return a list of group SIDs for a user SID
  the returned list is NULL terminated
  Return 0 on success and -1 on error
*/
int nss_getusersids(struct nss_state *nss, const char *user_sid, char ***sids);

/*
  convert a sid to a uid
  Return 0 on success and -1 on error
*/
int nss_sidtouid(struct nss_state *nss, const char *sid, uid_t *uid);

/*
  convert a sid to a gid
  Return 0 on success and -1 on error
*/
int nss_sidtogid(struct nss_state *nss, const char *sid, gid_t *gid);

/*
  convert a uid to a sid
  caller frees
  Return 0 on success and -1 on error
*/
int nss_uidtosid(struct nss_state *nss, uid_t uid, char **sid);

/*
  convert a gid to a sid
  caller frees
  Return 0 on success and -1 on error
*/
int nss_gidtosid(struct nss_state *nss, gid_t gid, char **sid);
