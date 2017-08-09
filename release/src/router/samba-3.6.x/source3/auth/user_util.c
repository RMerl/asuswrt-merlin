/*
   Unix SMB/CIFS implementation.
   Username handling
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1997-2001.
   Copyright (C) Volker Lendecke 2006

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
#include "auth.h"

/*******************************************************************
 Map a username from a dos name to a unix name by looking in the username
 map. Note that this modifies the name in place.
 This is the main function that should be called *once* on
 any incoming or new username - in order to canonicalize the name.
 This is being done to de-couple the case conversions from the user mapping
 function. Previously, the map_username was being called
 every time Get_Pwnam_alloc was called.
 Returns True if username was changed, false otherwise.
********************************************************************/

static char *last_from = NULL;
static char *last_to = NULL;

static const char *get_last_from(void)
{
	if (!last_from) {
		return "";
	}
	return last_from;
}

static const char *get_last_to(void)
{
	if (!last_to) {
		return "";
	}
	return last_to;
}

static bool set_last_from_to(const char *from, const char *to)
{
	char *orig_from = last_from;
	char *orig_to = last_to;

	last_from = SMB_STRDUP(from);
	last_to = SMB_STRDUP(to);

	SAFE_FREE(orig_from);
	SAFE_FREE(orig_to);

	if (!last_from || !last_to) {
		SAFE_FREE(last_from);
		SAFE_FREE(last_to);
		return false;
	}
	return true;
}

static char *skip_space(char *s)
{
	while (isspace((int)(*s))) {
		s += 1;
	}
	return s;
}

static bool fetch_map_from_gencache(TALLOC_CTX *ctx,
			const char *user_in,
			char **p_user_out)
{
	char *key, *value;
	bool found;

	if (lp_username_map_cache_time() == 0) {
		return false;
	}

	key = talloc_asprintf_strupper_m(ctx, "USERNAME_MAP/%s",
					 user_in);
	if (key == NULL) {
		return false;
	}
	found = gencache_get(key, &value, NULL);
	TALLOC_FREE(key);
	if (!found) {
		return false;
	}
	TALLOC_FREE(*p_user_out);
	*p_user_out = talloc_strdup(ctx, value);
	SAFE_FREE(value);
	if (!*p_user_out) {
		return false;
	}
	return true;
}

static void store_map_in_gencache(TALLOC_CTX *ctx, const char *from, const char *to)
{
	char *key;
	int cache_time = lp_username_map_cache_time();

	if (cache_time == 0) {
		return;
	}

	key = talloc_asprintf_strupper_m(ctx, "USERNAME_MAP/%s",
					 from);
        if (key == NULL) {
                return;
        }
	gencache_set(key, to, cache_time + time(NULL));
	TALLOC_FREE(key);
}

/****************************************************************************
 Check if a user is in a netgroup user list. If at first we don't succeed,
 try lower case.
****************************************************************************/

bool user_in_netgroup(TALLOC_CTX *ctx, const char *user, const char *ngname)
{
#ifdef HAVE_NETGROUP
	static char *my_yp_domain = NULL;
	char *lowercase_user = NULL;

	if (my_yp_domain == NULL) {
		yp_get_default_domain(&my_yp_domain);
	}

	if (my_yp_domain == NULL) {
		DEBUG(5,("Unable to get default yp domain, "
			"let's try without specifying it\n"));
	}

	DEBUG(5,("looking for user %s of domain %s in netgroup %s\n",
		user, my_yp_domain?my_yp_domain:"(ANY)", ngname));

	if (innetgr(ngname, NULL, user, my_yp_domain)) {
		DEBUG(5,("user_in_netgroup: Found\n"));
		return true;
	}

	/*
	 * Ok, innetgr is case sensitive. Try once more with lowercase
	 * just in case. Attempt to fix #703. JRA.
	 */
	lowercase_user = talloc_strdup(ctx, user);
	if (!lowercase_user) {
		return false;
	}
	strlower_m(lowercase_user);

	if (strcmp(user,lowercase_user) == 0) {
		/* user name was already lower case! */
		return false;
	}

	DEBUG(5,("looking for user %s of domain %s in netgroup %s\n",
		lowercase_user, my_yp_domain?my_yp_domain:"(ANY)", ngname));

	if (innetgr(ngname, NULL, lowercase_user, my_yp_domain)) {
		DEBUG(5,("user_in_netgroup: Found\n"));
		return true;
	}
#endif /* HAVE_NETGROUP */
	return false;
}

/****************************************************************************
 Check if a user is in a user list - can check combinations of UNIX
 and netgroup lists.
****************************************************************************/

bool user_in_list(TALLOC_CTX *ctx, const char *user,const char **list)
{
	if (!list || !*list)
		return False;

	DEBUG(10,("user_in_list: checking user %s in list\n", user));

	while (*list) {

		DEBUG(10,("user_in_list: checking user |%s| against |%s|\n",
			  user, *list));

		/*
		 * Check raw username.
		 */
		if (strequal(user, *list))
			return(True);

		/*
		 * Now check to see if any combination
		 * of UNIX and netgroups has been specified.
		 */

		if(**list == '@') {
			/*
			 * Old behaviour. Check netgroup list
			 * followed by UNIX list.
			 */
			if(user_in_netgroup(ctx, user, *list +1))
				return True;
			if(user_in_group(user, *list +1))
				return True;
		} else if (**list == '+') {

			if((*(*list +1)) == '&') {
				/*
				 * Search UNIX list followed by netgroup.
				 */
				if(user_in_group(user, *list +2))
					return True;
				if(user_in_netgroup(ctx, user, *list +2))
					return True;

			} else {

				/*
				 * Just search UNIX list.
				 */

				if(user_in_group(user, *list +1))
					return True;
			}

		} else if (**list == '&') {

			if(*(*list +1) == '+') {
				/*
				 * Search netgroup list followed by UNIX list.
				 */
				if(user_in_netgroup(ctx, user, *list +2))
					return True;
				if(user_in_group(user, *list +2))
					return True;
			} else {
				/*
				 * Just search netgroup list.
				 */
				if(user_in_netgroup(ctx, user, *list +1))
					return True;
			}
		}

		list++;
	}
	return(False);
}

bool map_username(TALLOC_CTX *ctx, const char *user_in, char **p_user_out)
{
	XFILE *f;
	char *mapfile = lp_username_map();
	char *s;
	char buf[512];
	bool mapped_user = False;
	char *cmd = lp_username_map_script();

	*p_user_out = NULL;

	if (!user_in)
		return false;

	/* Initially make a copy of the incoming name. */
	*p_user_out = talloc_strdup(ctx, user_in);
	if (!*p_user_out) {
		return false;
	}

	if (strequal(user_in,get_last_to()))
		return false;

	if (strequal(user_in,get_last_from())) {
		DEBUG(3,("Mapped user %s to %s\n",user_in,get_last_to()));
		TALLOC_FREE(*p_user_out);
		*p_user_out = talloc_strdup(ctx, get_last_to());
		return true;
	}

	if (fetch_map_from_gencache(ctx, user_in, p_user_out)) {
		return true;
	}

	/* first try the username map script */

	if ( *cmd ) {
		char **qlines;
		char *command = NULL;
		int numlines, ret, fd;

		command = talloc_asprintf(ctx,
					"%s \"%s\"",
					cmd,
					user_in);
		if (!command) {
			return false;
		}

		DEBUG(10,("Running [%s]\n", command));
		ret = smbrun(command, &fd);
		DEBUGADD(10,("returned [%d]\n", ret));

		TALLOC_FREE(command);

		if ( ret != 0 ) {
			if (fd != -1)
				close(fd);
			return False;
		}

		numlines = 0;
		qlines = fd_lines_load(fd, &numlines, 0, ctx);
		DEBUGADD(10,("Lines returned = [%d]\n", numlines));
		close(fd);

		/* should be either no lines or a single line with the mapped username */

		if (numlines && qlines) {
			DEBUG(3,("Mapped user %s to %s\n", user_in, qlines[0] ));
			set_last_from_to(user_in, qlines[0]);
			store_map_in_gencache(ctx, user_in, qlines[0]);
			TALLOC_FREE(*p_user_out);
			*p_user_out = talloc_strdup(ctx, qlines[0]);
			if (!*p_user_out) {
				return false;
			}
		}

		TALLOC_FREE(qlines);

		return numlines != 0;
	}

	/* ok.  let's try the mapfile */
	if (!*mapfile)
		return False;

	f = x_fopen(mapfile,O_RDONLY, 0);
	if (!f) {
		DEBUG(0,("can't open username map %s. Error %s\n",mapfile, strerror(errno) ));
		return False;
	}

	DEBUG(4,("Scanning username map %s\n",mapfile));

	while((s=fgets_slash(buf,sizeof(buf),f))!=NULL) {
		char *unixname = s;
		char *dosname = strchr_m(unixname,'=');
		char **dosuserlist;
		bool return_if_mapped = False;

		if (!dosname)
			continue;

		*dosname++ = 0;

		unixname = skip_space(unixname);

		if ('!' == *unixname) {
			return_if_mapped = True;
			unixname = skip_space(unixname+1);
		}

		if (!*unixname || strchr_m("#;",*unixname))
			continue;

		{
			int l = strlen(unixname);
			while (l && isspace((int)unixname[l-1])) {
				unixname[l-1] = 0;
				l--;
			}
		}

		/* skip lines like 'user = ' */

		dosuserlist = str_list_make_v3(ctx, dosname, NULL);
		if (!dosuserlist) {
			DEBUG(0,("Bad username map entry.  Unable to build user list.  Ignoring.\n"));
			continue;
		}

		if (strchr_m(dosname,'*') ||
		    user_in_list(ctx, user_in, (const char **)dosuserlist)) {
			DEBUG(3,("Mapped user %s to %s\n",user_in,unixname));
			mapped_user = True;

			set_last_from_to(user_in, unixname);
			store_map_in_gencache(ctx, user_in, unixname);
			TALLOC_FREE(*p_user_out);
			*p_user_out = talloc_strdup(ctx, unixname);
			if (!*p_user_out) {
				TALLOC_FREE(dosuserlist);
				x_fclose(f);
				return false;
			}

			if ( return_if_mapped ) {
				TALLOC_FREE(dosuserlist);
				x_fclose(f);
				return True;
			}
		}

		TALLOC_FREE(dosuserlist);
	}

	x_fclose(f);

	/*
	 * If we didn't successfully map a user in the loop above,
	 * setup the last_from and last_to as an optimization so
	 * that we don't scan the file again for the same user.
	 */
	if (!mapped_user) {
		DEBUG(8, ("The user '%s' has no mapping. "
			  "Skip it next time.\n", user_in));
		set_last_from_to(user_in, user_in);
		store_map_in_gencache(ctx, user_in, user_in);
	}

	return mapped_user;
}
