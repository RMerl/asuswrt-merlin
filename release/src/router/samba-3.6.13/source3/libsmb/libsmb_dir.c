/*
   Unix SMB/Netbios implementation.
   SMB client library implementation
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Richard Sharpe 2000, 2002
   Copyright (C) John Terpstra 2000
   Copyright (C) Tom Jansen (Ninja ISD) 2002
   Copyright (C) Derrell Lipman 2003-2008
   Copyright (C) Jeremy Allison 2007, 2008

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
#include "libsmb/libsmb.h"
#include "popt_common.h"
#include "libsmbclient.h"
#include "libsmb_internal.h"
#include "rpc_client/cli_pipe.h"
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"
#include "libsmb/nmblib.h"

/*
 * Routine to open a directory
 * We accept the URL syntax explained in SMBC_parse_path(), above.
 */

static void
remove_dir(SMBCFILE *dir)
{
	struct smbc_dir_list *d,*f;

	d = dir->dir_list;
	while (d) {

		f = d; d = d->next;

		SAFE_FREE(f->dirent);
		SAFE_FREE(f);

	}

	dir->dir_list = dir->dir_end = dir->dir_next = NULL;

}

static int
add_dirent(SMBCFILE *dir,
           const char *name,
           const char *comment,
           uint32 type)
{
	struct smbc_dirent *dirent;
	int size;
        int name_length = (name == NULL ? 0 : strlen(name));
        int comment_len = (comment == NULL ? 0 : strlen(comment));

	/*
	 * Allocate space for the dirent, which must be increased by the
	 * size of the name and the comment and 1 each for the null terminator.
	 */

	size = sizeof(struct smbc_dirent) + name_length + comment_len + 2;

	dirent = (struct smbc_dirent *)SMB_MALLOC(size);

	if (!dirent) {

		dir->dir_error = ENOMEM;
		return -1;

	}

	ZERO_STRUCTP(dirent);

	if (dir->dir_list == NULL) {

		dir->dir_list = SMB_MALLOC_P(struct smbc_dir_list);
		if (!dir->dir_list) {

			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_list);

		dir->dir_end = dir->dir_next = dir->dir_list;
	}
	else {

		dir->dir_end->next = SMB_MALLOC_P(struct smbc_dir_list);

		if (!dir->dir_end->next) {

			SAFE_FREE(dirent);
			dir->dir_error = ENOMEM;
			return -1;

		}
		ZERO_STRUCTP(dir->dir_end->next);

		dir->dir_end = dir->dir_end->next;
	}

	dir->dir_end->next = NULL;
	dir->dir_end->dirent = dirent;

	dirent->smbc_type = type;
	dirent->namelen = name_length;
	dirent->commentlen = comment_len;
	dirent->dirlen = size;

        /*
         * dirent->namelen + 1 includes the null (no null termination needed)
         * Ditto for dirent->commentlen.
         * The space for the two null bytes was allocated.
         */
	strncpy(dirent->name, (name?name:""), dirent->namelen + 1);
	dirent->comment = (char *)(&dirent->name + dirent->namelen + 1);
	strncpy(dirent->comment, (comment?comment:""), dirent->commentlen + 1);

	return 0;

}

static void
list_unique_wg_fn(const char *name,
                  uint32 type,
                  const char *comment,
                  void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
        struct smbc_dir_list *dir_list;
        struct smbc_dirent *dirent;
	int dirent_type;
        int do_remove = 0;

	dirent_type = dir->dir_type;

	if (add_dirent(dir, name, comment, dirent_type) < 0) {
		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */
		/* Change cli_NetServerEnum to take a fn
		   returning NTSTATUS... JRA. */
	}

        /* Point to the one just added */
        dirent = dir->dir_end->dirent;

        /* See if this was a duplicate */
        for (dir_list = dir->dir_list;
             dir_list != dir->dir_end;
             dir_list = dir_list->next) {
                if (! do_remove &&
                    strcmp(dir_list->dirent->name, dirent->name) == 0) {
                        /* Duplicate.  End end of list need to be removed. */
                        do_remove = 1;
                }

                if (do_remove && dir_list->next == dir->dir_end) {
                        /* Found the end of the list.  Remove it. */
                        dir->dir_end = dir_list;
                        free(dir_list->next);
                        free(dirent);
                        dir_list->next = NULL;
                        break;
                }
        }
}

static void
list_fn(const char *name,
        uint32 type,
        const char *comment,
        void *state)
{
	SMBCFILE *dir = (SMBCFILE *)state;
	int dirent_type;

	/*
         * We need to process the type a little ...
         *
         * Disk share     = 0x00000000
         * Print share    = 0x00000001
         * Comms share    = 0x00000002 (obsolete?)
         * IPC$ share     = 0x00000003
         *
         * administrative shares:
         * ADMIN$, IPC$, C$, D$, E$ ...  are type |= 0x80000000
         */

	if (dir->dir_type == SMBC_FILE_SHARE) {
		switch (type) {
                case 0 | 0x80000000:
		case 0:
			dirent_type = SMBC_FILE_SHARE;
			break;

		case 1:
			dirent_type = SMBC_PRINTER_SHARE;
			break;

		case 2:
			dirent_type = SMBC_COMMS_SHARE;
			break;

                case 3 | 0x80000000:
		case 3:
			dirent_type = SMBC_IPC_SHARE;
			break;

		default:
			dirent_type = SMBC_FILE_SHARE; /* FIXME, error? */
			break;
		}
	}
	else {
                dirent_type = dir->dir_type;
        }

	if (add_dirent(dir, name, comment, dirent_type) < 0) {
		/* An error occurred, what do we do? */
		/* FIXME: Add some code here */
		/* Change cli_NetServerEnum to take a fn
		   returning NTSTATUS... JRA. */
	}
}

static NTSTATUS
dir_list_fn(const char *mnt,
            struct file_info *finfo,
            const char *mask,
            void *state)
{

	if (add_dirent((SMBCFILE *)state, finfo->name, "",
		       (finfo->mode&FILE_ATTRIBUTE_DIRECTORY?SMBC_DIR:SMBC_FILE)) < 0) {
		SMBCFILE *dir = (SMBCFILE *)state;
		return map_nt_error_from_unix(dir->dir_error);
	}
	return NT_STATUS_OK;
}

static int
net_share_enum_rpc(struct cli_state *cli,
                   void (*fn)(const char *name,
                              uint32 type,
                              const char *comment,
                              void *state),
                   void *state)
{
        int i;
	WERROR result;
	uint32 preferred_len = 0xffffffff;
        uint32 type;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr1 ctr1;
	fstring name = "";
        fstring comment = "";
	struct rpc_pipe_client *pipe_hnd = NULL;
        NTSTATUS nt_status;
	uint32_t resume_handle = 0;
	uint32_t total_entries = 0;
	struct dcerpc_binding_handle *b;

        /* Open the server service pipe */
        nt_status = cli_rpc_pipe_open_noauth(cli, &ndr_table_srvsvc.syntax_id,
					     &pipe_hnd);
        if (!NT_STATUS_IS_OK(nt_status)) {
                DEBUG(1, ("net_share_enum_rpc pipe open fail!\n"));
                return -1;
        }

	ZERO_STRUCT(info_ctr);
	ZERO_STRUCT(ctr1);

	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &ctr1;

	b = pipe_hnd->binding_handle;

        /* Issue the NetShareEnum RPC call and retrieve the response */
	nt_status = dcerpc_srvsvc_NetShareEnumAll(b, talloc_tos(),
						  pipe_hnd->desthost,
						  &info_ctr,
						  preferred_len,
						  &total_entries,
						  &resume_handle,
						  &result);

        /* Was it successful? */
	if (!NT_STATUS_IS_OK(nt_status)) {
                /*  Nope.  Go clean up. */
		result = ntstatus_to_werror(nt_status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
                /*  Nope.  Go clean up. */
		goto done;
        }

	if (total_entries == 0) {
                /*  Nope.  Go clean up. */
		result = WERR_GENERAL_FAILURE;
		goto done;
	}

        /* For each returned entry... */
        for (i = 0; i < info_ctr.ctr.ctr1->count; i++) {

                /* pull out the share name */
		fstrcpy(name, info_ctr.ctr.ctr1->array[i].name);

                /* pull out the share's comment */
		fstrcpy(comment, info_ctr.ctr.ctr1->array[i].comment);

                /* Get the type value */
                type = info_ctr.ctr.ctr1->array[i].type;

                /* Add this share to the list */
                (*fn)(name, type, comment, state);
        }

done:
        /* Close the server service pipe */
        TALLOC_FREE(pipe_hnd);

        /* Tell 'em if it worked */
        return W_ERROR_IS_OK(result) ? 0 : -1;
}


/*
 * Verify that the options specified in a URL are valid
 */
int
SMBC_check_options(char *server,
                   char *share,
                   char *path,
                   char *options)
{
        DEBUG(4, ("SMBC_check_options(): server='%s' share='%s' "
                  "path='%s' options='%s'\n",
                  server, share, path, options));

        /* No options at all is always ok */
        if (! *options) return 0;

        /* Currently, we don't support any options. */
        return -1;
}


SMBCFILE *
SMBC_opendir_ctx(SMBCCTX *context,
                 const char *fname)
{
        int saved_errno;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *options = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        uint16 mode;
        char *p = NULL;
	SMBCSRV *srv  = NULL;
	SMBCFILE *dir = NULL;
	struct sockaddr_storage rem_ss;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
	        DEBUG(4, ("no valid context\n"));
		TALLOC_FREE(frame);
		errno = EINVAL + 8192;
		return NULL;

	}

	if (!fname) {
		DEBUG(4, ("no valid fname\n"));
		TALLOC_FREE(frame);
		errno = EINVAL + 8193;
		return NULL;
	}

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            &options)) {
	        DEBUG(4, ("no valid path\n"));
		TALLOC_FREE(frame);
		errno = EINVAL + 8194;
		return NULL;
	}

	DEBUG(4, ("parsed path: fname='%s' server='%s' share='%s' "
                  "path='%s' options='%s'\n",
                  fname, server, share, path, options));

        /* Ensure the options are valid */
        if (SMBC_check_options(server, share, path, options)) {
                DEBUG(4, ("unacceptable options (%s)\n", options));
		TALLOC_FREE(frame);
                errno = EINVAL + 8195;
                return NULL;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			TALLOC_FREE(frame);
			errno = ENOMEM;
			return NULL;
		}
	}

	dir = SMB_MALLOC_P(SMBCFILE);

	if (!dir) {
		TALLOC_FREE(frame);
		errno = ENOMEM;
		return NULL;
	}

	ZERO_STRUCTP(dir);

	dir->cli_fd   = 0;
	dir->fname    = SMB_STRDUP(fname);
	dir->srv      = NULL;
	dir->offset   = 0;
	dir->file     = False;
	dir->dir_list = dir->dir_next = dir->dir_end = NULL;

	if (server[0] == (char)0) {

                int i;
                int count;
                int max_lmb_count;
                struct sockaddr_storage *ip_list;
                struct sockaddr_storage server_addr;
                struct user_auth_info u_info;
		NTSTATUS status;

		if (share[0] != (char)0 || path[0] != (char)0) {

			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			errno = EINVAL + 8196;
			return NULL;
		}

                /* Determine how many local master browsers to query */
                max_lmb_count = (smbc_getOptionBrowseMaxLmbCount(context) == 0
                                 ? INT_MAX
                                 : smbc_getOptionBrowseMaxLmbCount(context));

		memset(&u_info, '\0', sizeof(u_info));
		u_info.username = talloc_strdup(frame,user);
		u_info.password = talloc_strdup(frame,password);
		if (!u_info.username || !u_info.password) {
			if (dir) {
				SAFE_FREE(dir->fname);
				SAFE_FREE(dir);
			}
			TALLOC_FREE(frame);
			return NULL;
		}

		/*
                 * We have server and share and path empty but options
                 * requesting that we scan all master browsers for their list
                 * of workgroups/domains.  This implies that we must first try
                 * broadcast queries to find all master browsers, and if that
                 * doesn't work, then try our other methods which return only
                 * a single master browser.
                 */

                ip_list = NULL;
		status = name_resolve_bcast(MSBROWSE, 1, talloc_tos(),
					    &ip_list, &count);
                if (!NT_STATUS_IS_OK(status))
		{

                        TALLOC_FREE(ip_list);

                        if (!find_master_ip(workgroup, &server_addr)) {

				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
                                errno = ENOENT;
                                return NULL;
                        }

			ip_list = (struct sockaddr_storage *)talloc_memdup(
				talloc_tos(), &server_addr,
				sizeof(server_addr));
			if (ip_list == NULL) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				errno = ENOMEM;
				return NULL;
			}
                        count = 1;
                }

                for (i = 0; i < count && i < max_lmb_count; i++) {
			char addr[INET6_ADDRSTRLEN];
			char *wg_ptr = NULL;
                	struct cli_state *cli = NULL;

			print_sockaddr(addr, sizeof(addr), &ip_list[i]);
                        DEBUG(99, ("Found master browser %d of %d: %s\n",
                                   i+1, MAX(count, max_lmb_count),
                                   addr));

                        cli = get_ipc_connect_master_ip(talloc_tos(),
							&ip_list[i],
                                                        &u_info,
							&wg_ptr);
			/* cli == NULL is the master browser refused to talk or
			   could not be found */
			if (!cli) {
				continue;
			}

			workgroup = talloc_strdup(frame, wg_ptr);
			server = talloc_strdup(frame, cli->desthost);

                        cli_shutdown(cli);

			if (!workgroup || !server) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				errno = ENOMEM;
				return NULL;
			}

                        DEBUG(4, ("using workgroup %s %s\n",
                                  workgroup, server));

                        /*
                         * For each returned master browser IP address, get a
                         * connection to IPC$ on the server if we do not
                         * already have one, and determine the
                         * workgroups/domains that it knows about.
                         */

                        srv = SMBC_server(frame, context, True, server, "IPC$",
                                          &workgroup, &user, &password);
                        if (!srv) {
                                continue;
                        }

                        dir->srv = srv;
                        dir->dir_type = SMBC_WORKGROUP;

                        /* Now, list the stuff ... */

                        if (!cli_NetServerEnum(srv->cli,
                                               workgroup,
                                               SV_TYPE_DOMAIN_ENUM,
                                               list_unique_wg_fn,
                                               (void *)dir)) {
                                continue;
                        }
                }

                TALLOC_FREE(ip_list);
        } else {
                /*
                 * Server not an empty string ... Check the rest and see what
                 * gives
                 */
		if (*share == '\0') {
			if (*path != '\0') {

                                /* Should not have empty share with path */
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				errno = EINVAL + 8197;
				return NULL;

			}

			/*
                         * We don't know if <server> is really a server name
                         * or is a workgroup/domain name.  If we already have
                         * a server structure for it, we'll use it.
                         * Otherwise, check to see if <server><1D>,
                         * <server><1B>, or <server><20> translates.  We check
                         * to see if <server> is an IP address first.
                         */

                        /*
                         * See if we have an existing server.  Do not
                         * establish a connection if one does not already
                         * exist.
                         */
                        srv = SMBC_server(frame, context, False,
                                          server, "IPC$",
                                          &workgroup, &user, &password);

                        /*
                         * If no existing server and not an IP addr, look for
                         * LMB or DMB
                         */
			if (!srv &&
                            !is_ipaddress(server) &&
			    (resolve_name(server, &rem_ss, 0x1d, false) ||   /* LMB */
                             resolve_name(server, &rem_ss, 0x1b, false) )) { /* DMB */
				/*
				 * "server" is actually a workgroup name,
				 * not a server. Make this clear.
				 */
				char *wgroup = server;
				fstring buserver;

				dir->dir_type = SMBC_SERVER;

				/*
				 * Get the backup list ...
				 */
				if (!name_status_find(wgroup, 0, 0,
                                                      &rem_ss, buserver)) {
					char addr[INET6_ADDRSTRLEN];

					print_sockaddr(addr, sizeof(addr), &rem_ss);
                                        DEBUG(0,("Could not get name of "
                                                "local/domain master browser "
                                                "for workgroup %s from "
						"address %s\n",
						wgroup,
						addr));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					errno = EPERM;
					return NULL;

				}

				/*
                                 * Get a connection to IPC$ on the server if
                                 * we do not already have one
                                 */
				srv = SMBC_server(frame, context, True,
                                                  buserver, "IPC$",
                                                  &workgroup,
                                                  &user, &password);
				if (!srv) {
				        DEBUG(0, ("got no contact to IPC$\n"));
					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;

				}

				dir->srv = srv;

				/* Now, list the servers ... */
				if (!cli_NetServerEnum(srv->cli, wgroup,
                                                       0x0000FFFE, list_fn,
						       (void *)dir)) {

					if (dir) {
						SAFE_FREE(dir->fname);
						SAFE_FREE(dir);
					}
					TALLOC_FREE(frame);
					return NULL;
				}
			} else if (srv ||
                                   (resolve_name(server, &rem_ss, 0x20, false))) {

                                /*
                                 * If we hadn't found the server, get one now
                                 */
                                if (!srv) {
                                        srv = SMBC_server(frame, context, True,
                                                          server, "IPC$",
                                                          &workgroup,
                                                          &user, &password);
                                }

                                if (!srv) {
                                        if (dir) {
                                                SAFE_FREE(dir->fname);
                                                SAFE_FREE(dir);
                                        }
					TALLOC_FREE(frame);
                                        return NULL;

                                }

                                dir->dir_type = SMBC_FILE_SHARE;
                                dir->srv = srv;

                                /* List the shares ... */

                                if (net_share_enum_rpc(
                                            srv->cli,
                                            list_fn,
                                            (void *) dir) < 0 &&
                                    cli_RNetShareEnum(
                                            srv->cli,
                                            list_fn,
                                            (void *)dir) < 0) {

                                        errno = cli_errno(srv->cli);
                                        if (dir) {
                                                SAFE_FREE(dir->fname);
                                                SAFE_FREE(dir);
                                        }
					TALLOC_FREE(frame);
                                        return NULL;

                                }
                        } else {
                                /* Neither the workgroup nor server exists */
                                errno = ECONNREFUSED;
                                if (dir) {
                                        SAFE_FREE(dir->fname);
                                        SAFE_FREE(dir);
                                }
				TALLOC_FREE(frame);
                                return NULL;
			}

		}
		else {
                        /*
                         * The server and share are specified ... work from
                         * there ...
                         */
			char *targetpath;
			struct cli_state *targetcli;
			NTSTATUS status;

			/* We connect to the server and list the directory */
			dir->dir_type = SMBC_FILE_SHARE;

			srv = SMBC_server(frame, context, True, server, share,
                                          &workgroup, &user, &password);

			if (!srv) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			dir->srv = srv;

			/* Now, list the files ... */

                        p = path + strlen(path);
			path = talloc_asprintf_append(path, "\\*");
			if (!path) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			if (!cli_resolve_path(frame, "", context->internal->auth_info,
						srv->cli, path,
						&targetcli, &targetpath)) {
				d_printf("Could not resolve %s\n", path);
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				TALLOC_FREE(frame);
				return NULL;
			}

			status = cli_list(targetcli, targetpath,
					  FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN,
					  dir_list_fn, (void *)dir);
			if (!NT_STATUS_IS_OK(status)) {
				if (dir) {
					SAFE_FREE(dir->fname);
					SAFE_FREE(dir);
				}
				saved_errno = SMBC_errno(context, targetcli);

                                if (saved_errno == EINVAL) {
                                        /*
                                         * See if they asked to opendir
                                         * something other than a directory.
                                         * If so, the converted error value we
                                         * got would have been EINVAL rather
                                         * than ENOTDIR.
                                         */
                                        *p = '\0'; /* restore original path */

                                        if (SMBC_getatr(context, srv, path,
                                                        &mode, NULL,
                                                        NULL, NULL, NULL, NULL,
                                                        NULL) &&
                                            ! IS_DOS_DIR(mode)) {

                                                /* It is.  Correct the error value */
                                                saved_errno = ENOTDIR;
                                        }
                                }

                                /*
                                 * If there was an error and the server is no
                                 * good any more...
                                 */
                                if (cli_is_error(targetcli) &&
                                    smbc_getFunctionCheckServer(context)(context, srv)) {

                                        /* ... then remove it. */
                                        if (smbc_getFunctionRemoveUnusedServer(context)(context,
                                                                                        srv)) {
                                                /*
                                                 * We could not remove the
                                                 * server completely, remove
                                                 * it from the cache so we
                                                 * will not get it again. It
                                                 * will be removed when the
                                                 * last file/dir is closed.
                                                 */
                                                smbc_getFunctionRemoveCachedServer(context)(context, srv);
                                        }
                                }

				TALLOC_FREE(frame);
                                errno = saved_errno;
				return NULL;
			}
		}

	}

	DLIST_ADD(context->internal->files, dir);
	TALLOC_FREE(frame);
	return dir;

}

/*
 * Routine to close a directory
 */

int
SMBC_closedir_ctx(SMBCCTX *context,
                  SMBCFILE *dir)
{
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {
		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;
	}

	remove_dir(dir); /* Clean it up */

	DLIST_REMOVE(context->internal->files, dir);

	if (dir) {

		SAFE_FREE(dir->fname);
		SAFE_FREE(dir);    /* Free the space too */
	}

	TALLOC_FREE(frame);
	return 0;

}

static void
smbc_readdir_internal(SMBCCTX * context,
                      struct smbc_dirent *dest,
                      struct smbc_dirent *src,
                      int max_namebuf_len)
{
        if (smbc_getOptionUrlEncodeReaddirEntries(context)) {

                /* url-encode the name.  get back remaining buffer space */
                max_namebuf_len =
                        smbc_urlencode(dest->name, src->name, max_namebuf_len);

                /* We now know the name length */
                dest->namelen = strlen(dest->name);

                /* Save the pointer to the beginning of the comment */
                dest->comment = dest->name + dest->namelen + 1;

                /* Copy the comment */
                strncpy(dest->comment, src->comment, max_namebuf_len - 1);
                dest->comment[max_namebuf_len - 1] = '\0';

                /* Save other fields */
                dest->smbc_type = src->smbc_type;
                dest->commentlen = strlen(dest->comment);
                dest->dirlen = ((dest->comment + dest->commentlen + 1) -
                                (char *) dest);
        } else {

                /* No encoding.  Just copy the entry as is. */
                memcpy(dest, src, src->dirlen);
                dest->comment = (char *)(&dest->name + src->namelen + 1);
        }

}

/*
 * Routine to get a directory entry
 */

struct smbc_dirent *
SMBC_readdir_ctx(SMBCCTX *context,
                 SMBCFILE *dir)
{
        int maxlen;
	struct smbc_dirent *dirp, *dirent;
	TALLOC_CTX *frame = talloc_stackframe();

	/* Check that all is ok first ... */

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
                DEBUG(0, ("Invalid context in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {

		errno = EBADF;
                DEBUG(0, ("Invalid dir in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
                DEBUG(0, ("Found file vs directory in SMBC_readdir_ctx()\n"));
		TALLOC_FREE(frame);
		return NULL;

	}

	if (!dir->dir_next) {
		TALLOC_FREE(frame);
		return NULL;
        }

        dirent = dir->dir_next->dirent;
        if (!dirent) {

                errno = ENOENT;
		TALLOC_FREE(frame);
                return NULL;

        }

        dirp = &context->internal->dirent;
        maxlen = sizeof(context->internal->_dirent_name);

        smbc_readdir_internal(context, dirp, dirent, maxlen);

        dir->dir_next = dir->dir_next->next;

	TALLOC_FREE(frame);
        return dirp;
}

/*
 * Routine to get directory entries
 */

int
SMBC_getdents_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct smbc_dirent *dirp,
                  int count)
{
	int rem = count;
        int reqd;
        int maxlen;
	char *ndir = (char *)dirp;
	struct smbc_dir_list *dirlist;
	TALLOC_CTX *frame = talloc_stackframe();

	/* Check that all is ok first ... */

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {

		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

	/*
	 * Now, retrieve the number of entries that will fit in what was passed
	 * We have to figure out if the info is in the list, or we need to
	 * send a request to the server to get the info.
	 */

	while ((dirlist = dir->dir_next)) {
		struct smbc_dirent *dirent;
		struct smbc_dirent *currentEntry = (struct smbc_dirent *)ndir;

		if (!dirlist->dirent) {

			errno = ENOENT;  /* Bad error */
			TALLOC_FREE(frame);
			return -1;

		}

                /* Do urlencoding of next entry, if so selected */
                dirent = &context->internal->dirent;
                maxlen = sizeof(context->internal->_dirent_name);
                smbc_readdir_internal(context, dirent,
                                      dirlist->dirent, maxlen);

                reqd = dirent->dirlen;

		if (rem < reqd) {

			if (rem < count) { /* We managed to copy something */

				errno = 0;
				TALLOC_FREE(frame);
				return count - rem;

			}
			else { /* Nothing copied ... */

				errno = EINVAL;  /* Not enough space ... */
				TALLOC_FREE(frame);
				return -1;

			}

		}

		memcpy(currentEntry, dirent, reqd); /* Copy the data in ... */

		currentEntry->comment = &currentEntry->name[0] +
						dirent->namelen + 1;

		ndir += reqd;
		rem -= reqd;

		/* Try and align the struct for the next entry
		   on a valid pointer boundary by appending zeros */
		while((rem > 0) && ((unsigned long long)ndir & (sizeof(void*) - 1))) {
			*ndir = '\0';
			rem--;
			ndir++;
			currentEntry->dirlen++;
		}

		dir->dir_next = dirlist = dirlist -> next;
	}

	TALLOC_FREE(frame);

	if (rem == count)
		return 0;
	else
		return count - rem;

}

/*
 * Routine to create a directory ...
 */

int
SMBC_mkdir_ctx(SMBCCTX *context,
               const char *fname,
               mode_t mode)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_mkdir(%s)\n", fname));

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {

		TALLOC_FREE(frame);
		return -1;  /* errno set by SMBC_server */

	}

	/*d_printf(">>>mkdir: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", context->internal->auth_info,
				srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
                errno = ENOENT;
                TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>mkdir: resolved path as %s\n", targetpath);*/

	if (!NT_STATUS_IS_OK(cli_mkdir(targetcli, targetpath))) {
		errno = SMBC_errno(context, targetcli);
		TALLOC_FREE(frame);
		return -1;

	}

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Our list function simply checks to see if a directory is not empty
 */

static NTSTATUS
rmdir_list_fn(const char *mnt,
              struct file_info *finfo,
              const char *mask,
              void *state)
{
	if (strncmp(finfo->name, ".", 1) != 0 &&
            strncmp(finfo->name, "..", 2) != 0) {
		bool *smbc_rmdir_dirempty = (bool *)state;
		*smbc_rmdir_dirempty = false;
        }
	return NT_STATUS_OK;
}

/*
 * Routine to remove a directory
 */

int
SMBC_rmdir_ctx(SMBCCTX *context,
               const char *fname)
{
	SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
        char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_rmdir(%s)\n", fname));

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {

		TALLOC_FREE(frame);
		return -1;  /* errno set by SMBC_server */

	}

	/*d_printf(">>>rmdir: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", context->internal->auth_info,
				srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
                errno = ENOENT;
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>rmdir: resolved path as %s\n", targetpath);*/

	if (!NT_STATUS_IS_OK(cli_rmdir(targetcli, targetpath))) {

		errno = SMBC_errno(context, targetcli);

		if (errno == EACCES) {  /* Check if the dir empty or not */

                        /* Local storage to avoid buffer overflows */
			char *lpath;
			bool smbc_rmdir_dirempty = true;
			NTSTATUS status;

			lpath = talloc_asprintf(frame, "%s\\*",
						targetpath);
			if (!lpath) {
				errno = ENOMEM;
				TALLOC_FREE(frame);
				return -1;
			}

			status = cli_list(targetcli, lpath,
					  FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN,
					  rmdir_list_fn,
					  &smbc_rmdir_dirempty);

			if (!NT_STATUS_IS_OK(status)) {
				/* Fix errno to ignore latest error ... */
				DEBUG(5, ("smbc_rmdir: "
                                          "cli_list returned an error: %d\n",
					  SMBC_errno(context, targetcli)));
				errno = EACCES;

			}

			if (smbc_rmdir_dirempty)
				errno = EACCES;
			else
				errno = ENOTEMPTY;

		}

		TALLOC_FREE(frame);
		return -1;

	}

	TALLOC_FREE(frame);
	return 0;

}

/*
 * Routine to return the current directory position
 */

off_t
SMBC_telldir_ctx(SMBCCTX *context,
                 SMBCFILE *dir)
{
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (!dir || !SMBC_dlist_contains(context->internal->files, dir)) {

		errno = EBADF;
		TALLOC_FREE(frame);
		return -1;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

        /* See if we're already at the end. */
        if (dir->dir_next == NULL) {
                /* We are. */
		TALLOC_FREE(frame);
                return -1;
        }

	/*
	 * We return the pointer here as the offset
	 */
	TALLOC_FREE(frame);
        return (off_t)(long)dir->dir_next->dirent;
}

/*
 * A routine to run down the list and see if the entry is OK
 */

static struct smbc_dir_list *
check_dir_ent(struct smbc_dir_list *list,
              struct smbc_dirent *dirent)
{

	/* Run down the list looking for what we want */

	if (dirent) {

		struct smbc_dir_list *tmp = list;

		while (tmp) {

			if (tmp->dirent == dirent)
				return tmp;

			tmp = tmp->next;

		}

	}

	return NULL;  /* Not found, or an error */

}


/*
 * Routine to seek on a directory
 */

int
SMBC_lseekdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  off_t offset)
{
	long int l_offset = offset;  /* Handle problems of size */
	struct smbc_dirent *dirent = (struct smbc_dirent *)l_offset;
	struct smbc_dir_list *list_ent = (struct smbc_dir_list *)NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (dir->file != False) { /* FIXME, should be dir, perhaps */

		errno = ENOTDIR;
		TALLOC_FREE(frame);
		return -1;

	}

	/* Now, check what we were passed and see if it is OK ... */

	if (dirent == NULL) {  /* Seek to the begining of the list */

		dir->dir_next = dir->dir_list;
		TALLOC_FREE(frame);
		return 0;

	}

        if (offset == -1) {     /* Seek to the end of the list */
                dir->dir_next = NULL;
		TALLOC_FREE(frame);
                return 0;
        }

	/* Now, run down the list and make sure that the entry is OK       */
	/* This may need to be changed if we change the format of the list */

	if ((list_ent = check_dir_ent(dir->dir_list, dirent)) == NULL) {
		errno = EINVAL;   /* Bad entry */
		TALLOC_FREE(frame);
		return -1;
	}

	dir->dir_next = list_ent;

	TALLOC_FREE(frame);
	return 0;
}

/*
 * Routine to fstat a dir
 */

int
SMBC_fstatdir_ctx(SMBCCTX *context,
                  SMBCFILE *dir,
                  struct stat *st)
{

	if (!context || !context->internal->initialized) {

		errno = EINVAL;
		return -1;
	}

	/* No code yet ... */
	return 0;
}

int
SMBC_chmod_ctx(SMBCCTX *context,
               const char *fname,
               mode_t newmode)
{
        SMBCSRV *srv = NULL;
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	char *path = NULL;
	uint16 mode;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_chmod(%s, 0%3o)\n", fname, (unsigned int)newmode));

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* errno set by SMBC_server */
	}
	
	/*d_printf(">>>unlink: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", context->internal->auth_info,
				srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
                errno = ENOENT;
		TALLOC_FREE(frame);
		return -1;
	}

	mode = 0;

	if (!(newmode & (S_IWUSR | S_IWGRP | S_IWOTH))) mode |= FILE_ATTRIBUTE_READONLY;
	if ((newmode & S_IXUSR) && lp_map_archive(-1)) mode |= FILE_ATTRIBUTE_ARCHIVE;
	if ((newmode & S_IXGRP) && lp_map_system(-1)) mode |= FILE_ATTRIBUTE_SYSTEM;
	if ((newmode & S_IXOTH) && lp_map_hidden(-1)) mode |= FILE_ATTRIBUTE_HIDDEN;

	if (!NT_STATUS_IS_OK(cli_setatr(targetcli, targetpath, mode, 0))) {
		errno = SMBC_errno(context, targetcli);
		TALLOC_FREE(frame);
		return -1;
	}

	TALLOC_FREE(frame);
        return 0;
}

int
SMBC_utimes_ctx(SMBCCTX *context,
                const char *fname,
                struct timeval *tbuf)
{
        SMBCSRV *srv = NULL;
	char *server = NULL;
	char *share = NULL;
	char *user = NULL;
	char *password = NULL;
	char *workgroup = NULL;
	char *path = NULL;
        time_t access_time;
        time_t write_time;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

        if (tbuf == NULL) {
                access_time = write_time = time(NULL);
        } else {
                access_time = tbuf[0].tv_sec;
                write_time = tbuf[1].tv_sec;
        }

        if (DEBUGLVL(4)) {
                char *p;
                char atimebuf[32];
                char mtimebuf[32];

                strncpy(atimebuf, ctime(&access_time), sizeof(atimebuf) - 1);
                atimebuf[sizeof(atimebuf) - 1] = '\0';
                if ((p = strchr(atimebuf, '\n')) != NULL) {
                        *p = '\0';
                }

                strncpy(mtimebuf, ctime(&write_time), sizeof(mtimebuf) - 1);
                mtimebuf[sizeof(mtimebuf) - 1] = '\0';
                if ((p = strchr(mtimebuf, '\n')) != NULL) {
                        *p = '\0';
                }

                dbgtext("smbc_utimes(%s, atime = %s mtime = %s)\n",
                        fname, atimebuf, mtimebuf);
        }

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;      /* errno set by SMBC_server */
	}

        if (!SMBC_setatr(context, srv, path,
                         0, access_time, write_time, 0, 0)) {
		TALLOC_FREE(frame);
                return -1;      /* errno set by SMBC_setatr */
        }

	TALLOC_FREE(frame);
        return 0;
}

/*
 * Routine to unlink() a file
 */

int
SMBC_unlink_ctx(SMBCCTX *context,
                const char *fname)
{
	char *server = NULL;
        char *share = NULL;
        char *user = NULL;
        char *password = NULL;
        char *workgroup = NULL;
	char *path = NULL;
	char *targetpath = NULL;
	struct cli_state *targetcli = NULL;
	SMBCSRV *srv = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!context || !context->internal->initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;

	}

	if (!fname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;

	}

	if (SMBC_parse_path(frame,
                            context,
                            fname,
                            &workgroup,
                            &server,
                            &share,
                            &path,
                            &user,
                            &password,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
        }

	if (!user || user[0] == (char)0) {
		user = talloc_strdup(frame, smbc_getUser(context));
		if (!user) {
			errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	srv = SMBC_server(frame, context, True,
                          server, share, &workgroup, &user, &password);

	if (!srv) {
		TALLOC_FREE(frame);
		return -1;  /* SMBC_server sets errno */

	}

	/*d_printf(">>>unlink: resolving %s\n", path);*/
	if (!cli_resolve_path(frame, "", context->internal->auth_info,
				srv->cli, path,
				&targetcli, &targetpath)) {
		d_printf("Could not resolve %s\n", path);
                errno = ENOENT;
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>unlink: resolved path as %s\n", targetpath);*/

	if (!NT_STATUS_IS_OK(cli_unlink(targetcli, targetpath, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN))) {

		errno = SMBC_errno(context, targetcli);

		if (errno == EACCES) { /* Check if the file is a directory */

			int saverr = errno;
			SMB_OFF_T size = 0;
			uint16 mode = 0;
			struct timespec write_time_ts;
                        struct timespec access_time_ts;
                        struct timespec change_time_ts;
			SMB_INO_T ino = 0;

			if (!SMBC_getatr(context, srv, path, &mode, &size,
					 NULL,
                                         &access_time_ts,
                                         &write_time_ts,
                                         &change_time_ts,
                                         &ino)) {

				/* Hmmm, bad error ... What? */

				errno = SMBC_errno(context, targetcli);
				TALLOC_FREE(frame);
				return -1;

			}
			else {

				if (IS_DOS_DIR(mode))
					errno = EISDIR;
				else
					errno = saverr;  /* Restore this */

			}
		}

		TALLOC_FREE(frame);
		return -1;

	}

	TALLOC_FREE(frame);
	return 0;  /* Success ... */

}

/*
 * Routine to rename() a file
 */

int
SMBC_rename_ctx(SMBCCTX *ocontext,
                const char *oname,
                SMBCCTX *ncontext,
                const char *nname)
{
	char *server1 = NULL;
        char *share1 = NULL;
        char *server2 = NULL;
        char *share2 = NULL;
        char *user1 = NULL;
        char *user2 = NULL;
        char *password1 = NULL;
        char *password2 = NULL;
        char *workgroup = NULL;
	char *path1 = NULL;
        char *path2 = NULL;
        char *targetpath1 = NULL;
        char *targetpath2 = NULL;
	struct cli_state *targetcli1 = NULL;
        struct cli_state *targetcli2 = NULL;
	SMBCSRV *srv = NULL;
	TALLOC_CTX *frame = talloc_stackframe();

	if (!ocontext || !ncontext ||
	    !ocontext->internal->initialized ||
	    !ncontext->internal->initialized) {

		errno = EINVAL;  /* Best I can think of ... */
		TALLOC_FREE(frame);
		return -1;
	}

	if (!oname || !nname) {
		errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	DEBUG(4, ("smbc_rename(%s,%s)\n", oname, nname));

	if (SMBC_parse_path(frame,
                            ocontext,
                            oname,
                            &workgroup,
                            &server1,
                            &share1,
                            &path1,
                            &user1,
                            &password1,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!user1 || user1[0] == (char)0) {
		user1 = talloc_strdup(frame, smbc_getUser(ocontext));
		if (!user1) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	if (SMBC_parse_path(frame,
                            ncontext,
                            nname,
                            NULL,
                            &server2,
                            &share2,
                            &path2,
                            &user2,
                            &password2,
                            NULL)) {
                errno = EINVAL;
		TALLOC_FREE(frame);
                return -1;
	}

	if (!user2 || user2[0] == (char)0) {
		user2 = talloc_strdup(frame, smbc_getUser(ncontext));
		if (!user2) {
                	errno = ENOMEM;
			TALLOC_FREE(frame);
			return -1;
		}
	}

	if (strcmp(server1, server2) || strcmp(share1, share2) ||
	    strcmp(user1, user2)) {
		/* Can't rename across file systems, or users?? */
		errno = EXDEV;
		TALLOC_FREE(frame);
		return -1;
	}

	srv = SMBC_server(frame, ocontext, True,
                          server1, share1, &workgroup, &user1, &password1);
	if (!srv) {
		TALLOC_FREE(frame);
		return -1;

	}

	/* set the credentials to make DFS work */
	smbc_set_credentials_with_fallback(ocontext,
					   workgroup,
				     	   user1,
				    	   password1);

	/*d_printf(">>>rename: resolving %s\n", path1);*/
	if (!cli_resolve_path(frame, "", ocontext->internal->auth_info,
				srv->cli,
				path1,
				&targetcli1, &targetpath1)) {
		d_printf("Could not resolve %s\n", path1);
                errno = ENOENT;
		TALLOC_FREE(frame);
		return -1;
	}
	
	/* set the credentials to make DFS work */
	smbc_set_credentials_with_fallback(ncontext,
					   workgroup,
				           user2,
				           password2);
	
	/*d_printf(">>>rename: resolved path as %s\n", targetpath1);*/
	/*d_printf(">>>rename: resolving %s\n", path2);*/
	if (!cli_resolve_path(frame, "", ncontext->internal->auth_info,
				srv->cli, 
				path2,
				&targetcli2, &targetpath2)) {
		d_printf("Could not resolve %s\n", path2);
                errno = ENOENT;
		TALLOC_FREE(frame);
		return -1;
	}
	/*d_printf(">>>rename: resolved path as %s\n", targetpath2);*/

	if (strcmp(targetcli1->desthost, targetcli2->desthost) ||
            strcmp(targetcli1->share, targetcli2->share))
	{
		/* can't rename across file systems */
		errno = EXDEV;
		TALLOC_FREE(frame);
		return -1;
	}

	if (!NT_STATUS_IS_OK(cli_rename(targetcli1, targetpath1, targetpath2))) {
		int eno = SMBC_errno(ocontext, targetcli1);

		if (eno != EEXIST ||
		    !NT_STATUS_IS_OK(cli_unlink(targetcli1, targetpath2, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)) ||
		    !NT_STATUS_IS_OK(cli_rename(targetcli1, targetpath1, targetpath2))) {

			errno = eno;
			TALLOC_FREE(frame);
			return -1;

		}
	}

	TALLOC_FREE(frame);
	return 0; /* Success */
}

