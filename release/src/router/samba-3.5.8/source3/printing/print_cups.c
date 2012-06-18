/*
 * Support code for the Common UNIX Printing System ("CUPS")
 *
 * Copyright 1999-2003 by Michael R Sweet.
 * Copyright 2008 Jeremy Allison.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * JRA. Converted to utf8 pull/push.
 */

#include "includes.h"
#include "printing.h"

#ifdef HAVE_CUPS
#include <cups/cups.h>
#include <cups/language.h>

static SIG_ATOMIC_T gotalarm;

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/

static void gotalarm_sig(void)
{
        gotalarm = 1;
}

extern userdom_struct current_user_info;

/*
 * 'cups_passwd_cb()' - The CUPS password callback...
 */

static const char *				/* O - Password or NULL */
cups_passwd_cb(const char *prompt)	/* I - Prompt */
{
	/*
	 * Always return NULL to indicate that no password is available...
	 */

	return (NULL);
}

static http_t *cups_connect(TALLOC_CTX *frame)
{
	http_t *http = NULL;
	char *server = NULL, *p = NULL;
	int port;
	int timeout = lp_cups_connection_timeout();
	size_t size;

	if (lp_cups_server() != NULL && strlen(lp_cups_server()) > 0) {
		if (!push_utf8_talloc(frame, &server, lp_cups_server(), &size)) {
			return NULL;
		}
	} else {
		server = talloc_strdup(frame,cupsServer());
	}
	if (!server) {
		return NULL;
	}

	p = strchr(server, ':');
	if (p) {
		port = atoi(p+1);
		*p = '\0';
	} else {
		port = ippPort();
	}

	DEBUG(10, ("connecting to cups server %s:%d\n",
		   server, port));

	gotalarm = 0;

	if (timeout) {
                CatchSignal(SIGALRM, SIGNAL_CAST gotalarm_sig);
                alarm(timeout);
        }

#ifdef HAVE_HTTPCONNECTENCRYPT
	http = httpConnectEncrypt(server, port, lp_cups_encrypt());
#else
	http = httpConnect(server, port);
#endif


	CatchSignal(SIGALRM, SIGNAL_CAST SIG_IGN);
        alarm(0);

	if (http == NULL) {
		DEBUG(0,("Unable to connect to CUPS server %s:%d - %s\n",
			 server, port, strerror(errno)));
	}

	return http;
}

static void send_pcap_info(const char *name, const char *info, void *pd)
{
	int fd = *(int *)pd;
	size_t namelen = name ? strlen(name)+1 : 0;
	size_t infolen = info ? strlen(info)+1 : 0;

	DEBUG(11,("send_pcap_info: writing namelen %u\n", (unsigned int)namelen));
	if (sys_write(fd, &namelen, sizeof(namelen)) != sizeof(namelen)) {
		DEBUG(10,("send_pcap_info: namelen write failed %s\n",
			strerror(errno)));
		return;
	}
	DEBUG(11,("send_pcap_info: writing infolen %u\n", (unsigned int)infolen));
	if (sys_write(fd, &infolen, sizeof(infolen)) != sizeof(infolen)) {
		DEBUG(10,("send_pcap_info: infolen write failed %s\n",
			strerror(errno)));
		return;
	}
	if (namelen) {
		DEBUG(11,("send_pcap_info: writing name %s\n", name));
		if (sys_write(fd, name, namelen) != namelen) {
			DEBUG(10,("send_pcap_info: name write failed %s\n",
				strerror(errno)));
			return;
		}
	}
	if (infolen) {
		DEBUG(11,("send_pcap_info: writing info %s\n", info));
		if (sys_write(fd, info, infolen) != infolen) {
			DEBUG(10,("send_pcap_info: info write failed %s\n",
				strerror(errno)));
			return;
		}
	}
}

static bool cups_cache_reload_async(int fd)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct pcap_cache *tmp_pcap_cache = NULL;
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	ipp_attribute_t	*attr;		/* Current attribute */
	cups_lang_t	*language = NULL;	/* Default language */
	char		*name,		/* printer-name attribute */
			*info;		/* printer-info attribute */
	static const char *requested[] =/* Requested attributes */
			{
			  "printer-name",
			  "printer-info"
			};
	bool ret = False;
	size_t size;

	DEBUG(5, ("reloading cups printcap cache\n"));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build a CUPS_GET_PRINTERS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    requested-attributes
	*/

	request = ippNew();

	request->request.op.operation_id = CUPS_GET_PRINTERS;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(requested) / sizeof(requested[0])),
		      NULL, requested);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer list - %s\n",
			 ippErrorString(cupsLastError())));
		goto out;
	}

	for (attr = response->attrs; attr != NULL;) {
	       /*
		* Skip leading attributes until we hit a printer...
		*/

		while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
			attr = attr->next;

		if (attr == NULL)
        		break;

	       /*
		* Pull the needed attributes from this printer...
		*/

		name       = NULL;
		info       = NULL;

		while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER) {
        		if (strcmp(attr->name, "printer-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&name,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		if (strcmp(attr->name, "printer-info") == 0 &&
			    attr->value_tag == IPP_TAG_TEXT) {
				if (!pull_utf8_talloc(frame,
						&info,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		attr = attr->next;
		}

	       /*
		* See if we have everything needed...
		*/

		if (name == NULL)
			break;

		if (!pcap_cache_add_specific(&tmp_pcap_cache, name, info)) {
			goto out;
		}
	}

	ippDelete(response);
	response = NULL;

       /*
	* Build a CUPS_GET_CLASSES request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    requested-attributes
	*/

	request = ippNew();

	request->request.op.operation_id = CUPS_GET_CLASSES;
	request->request.op.request_id   = 1;

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(requested) / sizeof(requested[0])),
		      NULL, requested);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer list - %s\n",
			 ippErrorString(cupsLastError())));
		goto out;
	}

	for (attr = response->attrs; attr != NULL;) {
	       /*
		* Skip leading attributes until we hit a printer...
		*/

		while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
			attr = attr->next;

		if (attr == NULL)
        		break;

	       /*
		* Pull the needed attributes from this printer...
		*/

		name       = NULL;
		info       = NULL;

		while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER) {
        		if (strcmp(attr->name, "printer-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&name,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		if (strcmp(attr->name, "printer-info") == 0 &&
			    attr->value_tag == IPP_TAG_TEXT) {
				if (!pull_utf8_talloc(frame,
						&info,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		attr = attr->next;
		}

	       /*
		* See if we have everything needed...
		*/

		if (name == NULL)
			break;

		if (!pcap_cache_add_specific(&tmp_pcap_cache, name, info)) {
			goto out;
		}
	}

	ret = True;

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	/* Send all the entries up the pipe. */
	if (tmp_pcap_cache) {
		pcap_printer_fn_specific(tmp_pcap_cache,
				send_pcap_info,
				(void *)&fd);

		pcap_cache_destroy_specific(&tmp_pcap_cache);
	}
	TALLOC_FREE(frame);
	return ret;
}

static struct pcap_cache *local_pcap_copy;
struct fd_event *cache_fd_event;

static bool cups_pcap_load_async(int *pfd)
{
	int fds[2];
	pid_t pid;

	*pfd = -1;

	if (cache_fd_event) {
		DEBUG(3,("cups_pcap_load_async: already waiting for "
			"a refresh event\n" ));
		return false;
	}

	DEBUG(5,("cups_pcap_load_async: asynchronously loading cups printers\n"));

	if (pipe(fds) == -1) {
		return false;
	}

	pid = sys_fork();
	if (pid == (pid_t)-1) {
		DEBUG(10,("cups_pcap_load_async: fork failed %s\n",
			strerror(errno) ));
		close(fds[0]);
		close(fds[1]);
		return false;
	}

	if (pid) {
		DEBUG(10,("cups_pcap_load_async: child pid = %u\n",
			(unsigned int)pid ));
		/* Parent. */
		close(fds[1]);
		*pfd = fds[0];
		return true;
	}

	/* Child. */

	close_all_print_db();

	if (!NT_STATUS_IS_OK(reinit_after_fork(smbd_messaging_context(),
					       smbd_event_context(), true))) {
		DEBUG(0,("cups_pcap_load_async: reinit_after_fork() failed\n"));
		smb_panic("cups_pcap_load_async: reinit_after_fork() failed");
	}

	close(fds[0]);
	cups_cache_reload_async(fds[1]);
	close(fds[1]);
	_exit(0);
}

static void cups_async_callback(struct event_context *event_ctx,
				struct fd_event *event,
				uint16 flags,
				void *p)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int fd = *(int *)p;
	struct pcap_cache *tmp_pcap_cache = NULL;

	DEBUG(5,("cups_async_callback: callback received for printer data. "
		"fd = %d\n", fd));

	while (1) {
		char *name = NULL, *info = NULL;
		size_t namelen = 0, infolen = 0;
		ssize_t ret = -1;

		ret = sys_read(fd, &namelen, sizeof(namelen));
		if (ret == 0) {
			/* EOF */
			break;
		}
		if (ret != sizeof(namelen)) {
			DEBUG(10,("cups_async_callback: namelen read failed %d %s\n",
				errno, strerror(errno)));
			break;
		}

		DEBUG(11,("cups_async_callback: read namelen %u\n",
			(unsigned int)namelen));

		ret = sys_read(fd, &infolen, sizeof(infolen));
		if (ret == 0) {
			/* EOF */
			break;
		}
		if (ret != sizeof(infolen)) {
			DEBUG(10,("cups_async_callback: infolen read failed %s\n",
				strerror(errno)));
			break;
		}

		DEBUG(11,("cups_async_callback: read infolen %u\n",
			(unsigned int)infolen));

		if (namelen) {
			name = TALLOC_ARRAY(frame, char, namelen);
			if (!name) {
				break;
			}
			ret = sys_read(fd, name, namelen);
			if (ret == 0) {
				/* EOF */
				break;
			}
			if (ret != namelen) {
				DEBUG(10,("cups_async_callback: name read failed %s\n",
					strerror(errno)));
				break;
			}
			DEBUG(11,("cups_async_callback: read name %s\n",
				name));
		} else {
			name = NULL;
		}
		if (infolen) {
			info = TALLOC_ARRAY(frame, char, infolen);
			if (!info) {
				break;
			}
			ret = sys_read(fd, info, infolen);
			if (ret == 0) {
				/* EOF */
				break;
			}
			if (ret != infolen) {
				DEBUG(10,("cups_async_callback: info read failed %s\n",
					strerror(errno)));
				break;
			}
			DEBUG(11,("cups_async_callback: read info %s\n",
				info));
		} else {
			info = NULL;
		}

		/* Add to our local pcap cache. */
		pcap_cache_add_specific(&tmp_pcap_cache, name, info);
		TALLOC_FREE(name);
		TALLOC_FREE(info);
	}

	TALLOC_FREE(frame);
	if (tmp_pcap_cache) {
		/* We got a namelist, replace our local cache. */
		pcap_cache_destroy_specific(&local_pcap_copy);
		local_pcap_copy = tmp_pcap_cache;

		/* And the systemwide pcap cache. */
		pcap_cache_replace(local_pcap_copy);
	} else {
		DEBUG(2,("cups_async_callback: failed to read a new "
			"printer list\n"));
	}
	close(fd);
	TALLOC_FREE(p);
	TALLOC_FREE(cache_fd_event);
}

bool cups_cache_reload(void)
{
	int *p_pipe_fd = TALLOC_P(NULL, int);

	if (!p_pipe_fd) {
		return false;
	}

	*p_pipe_fd = -1;

	/* Set up an async refresh. */
	if (!cups_pcap_load_async(p_pipe_fd)) {
		return false;
	}
	if (!local_pcap_copy) {
		/* We have no local cache, wait directly for
		 * async refresh to complete.
		 */
		DEBUG(10,("cups_cache_reload: sync read on fd %d\n",
			*p_pipe_fd ));

		cups_async_callback(smbd_event_context(),
					NULL,
					EVENT_FD_READ,
					(void *)p_pipe_fd);
		if (!local_pcap_copy) {
			return false;
		}
	} else {
		/* Replace the system cache with our
		 * local copy. */
		pcap_cache_replace(local_pcap_copy);

		DEBUG(10,("cups_cache_reload: async read on fd %d\n",
			*p_pipe_fd ));

		/* Trigger an event when the pipe can be read. */
		cache_fd_event = event_add_fd(smbd_event_context(),
					NULL, *p_pipe_fd,
					EVENT_FD_READ,
					cups_async_callback,
					(void *)p_pipe_fd);
		if (!cache_fd_event) {
			close(*p_pipe_fd);
			TALLOC_FREE(p_pipe_fd);
			return false;
		}
	}
	return true;
}

/*
 * 'cups_job_delete()' - Delete a job.
 */

static int cups_job_delete(const char *sharename, const char *lprm_command, struct printjob *pjob)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	char *user = NULL;
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	size_t size;

	DEBUG(5,("cups_job_delete(%s, %p (%d))\n", sharename, pjob, pjob->sysjob));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build an IPP_CANCEL_JOB request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    job-uri
	*    requesting-user-name
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_CANCEL_JOB;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/jobs/%d", pjob->sysjob);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &user, pjob->user, &size)) {
		goto out;
	}

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, user);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/jobs")) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to cancel job %d - %s\n", pjob->sysjob,
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to cancel job %d - %s\n", pjob->sysjob,
			ippErrorString(cupsLastError())));
	}

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}


/*
 * 'cups_job_pause()' - Pause a job.
 */

static int cups_job_pause(int snum, struct printjob *pjob)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	char *user = NULL;
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	size_t size;

	DEBUG(5,("cups_job_pause(%d, %p (%d))\n", snum, pjob, pjob->sysjob));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build an IPP_HOLD_JOB request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    job-uri
	*    requesting-user-name
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_HOLD_JOB;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/jobs/%d", pjob->sysjob);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &user, pjob->user, &size)) {
		goto out;
	}
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, user);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/jobs")) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to hold job %d - %s\n", pjob->sysjob,
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to hold job %d - %s\n", pjob->sysjob,
			ippErrorString(cupsLastError())));
	}

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}


/*
 * 'cups_job_resume()' - Resume a paused job.
 */

static int cups_job_resume(int snum, struct printjob *pjob)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	char *user = NULL;
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	size_t size;

	DEBUG(5,("cups_job_resume(%d, %p (%d))\n", snum, pjob, pjob->sysjob));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build an IPP_RELEASE_JOB request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    job-uri
	*    requesting-user-name
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_RELEASE_JOB;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/jobs/%d", pjob->sysjob);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "job-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &user, pjob->user, &size)) {
		goto out;
	}
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, user);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/jobs")) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to release job %d - %s\n", pjob->sysjob,
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to release job %d - %s\n", pjob->sysjob,
			ippErrorString(cupsLastError())));
	}

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}


/*
 * 'cups_job_submit()' - Submit a job for printing.
 */

static int cups_job_submit(int snum, struct printjob *pjob)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	ipp_attribute_t *attr_job_id = NULL;	/* IPP Attribute "job-id" */
	cups_lang_t	*language = NULL;	/* Default language */
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	const char 	*clientname = NULL; 	/* hostname of client for job-originating-host attribute */
	char *new_jobname = NULL;
	int		num_options = 0;
	cups_option_t 	*options = NULL;
	char *printername = NULL;
	char *user = NULL;
	char *jobname = NULL;
	char *cupsoptions = NULL;
	char *filename = NULL;
	size_t size;
	uint32_t jobid = (uint32_t)-1;
	char addr[INET6_ADDRSTRLEN];

	DEBUG(5,("cups_job_submit(%d, %p)\n", snum, pjob));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build an IPP_PRINT_JOB request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    printer-uri
	*    requesting-user-name
	*    [document-data]
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_PRINT_JOB;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, PRINTERNAME(snum), &size)) {
		goto out;
	}
	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/printers/%s",
	         printername);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
        	     "printer-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &user, pjob->user, &size)) {
		goto out;
	}
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, user);

	clientname = client_name(get_client_fd());
	if (strcmp(clientname, "UNKNOWN") == 0) {
		clientname = client_addr(get_client_fd(),addr,sizeof(addr));
	}

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	             "job-originating-host-name", NULL,
		      clientname);

	/* Get the jobid from the filename. */
	jobid = print_parse_jobid(pjob->filename);
	if (jobid == (uint32_t)-1) {
		DEBUG(0,("cups_job_submit: failed to parse jobid from name %s\n",
				pjob->filename ));
		jobid = 0;
	}

	if (!push_utf8_talloc(frame, &jobname, pjob->jobname, &size)) {
		goto out;
	}
	new_jobname = talloc_asprintf(frame,
			"%s%.8u %s", PRINT_SPOOL_PREFIX,
			(unsigned int)jobid,
			jobname);
	if (new_jobname == NULL) {
		goto out;
	}

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name", NULL,
        	     new_jobname);

	/*
	 * add any options defined in smb.conf
	 */

	if (!push_utf8_talloc(frame, &cupsoptions, lp_cups_options(snum), &size)) {
		goto out;
	}
	num_options = 0;
	options     = NULL;
	num_options = cupsParseOptions(cupsoptions, num_options, &options);

	if ( num_options )
		cupsEncodeOptions(request, num_options, options);

       /*
	* Do the request and get back a response...
	*/

	slprintf(uri, sizeof(uri) - 1, "/printers/%s", printername);

	if (!push_utf8_talloc(frame, &filename, pjob->filename, &size)) {
		goto out;
	}
	if ((response = cupsDoFileRequest(http, request, uri, pjob->filename)) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to print file to %s - %s\n", PRINTERNAME(snum),
			         ippErrorString(cupsLastError())));
		} else {
			ret = 0;
			attr_job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER);
			if(attr_job_id) {
				pjob->sysjob = attr_job_id->values[0].integer;
				DEBUG(5,("cups_job_submit: job-id %d\n", pjob->sysjob));
			} else {
				DEBUG(0,("Missing job-id attribute in IPP response"));
			}
		}
	} else {
		DEBUG(0,("Unable to print file to `%s' - %s\n", PRINTERNAME(snum),
			 ippErrorString(cupsLastError())));
	}

	if ( ret == 0 )
		unlink(pjob->filename);
	/* else print_job_end will do it for us */

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);

	return ret;
}

/*
 * 'cups_queue_get()' - Get all the jobs in the print queue.
 */

static int cups_queue_get(const char *sharename,
               enum printing_types printing_type,
               char *lpq_command,
               print_queue_struct **q,
               print_status_struct *status)
{
	TALLOC_CTX *frame = talloc_stackframe();
	char *printername = NULL;
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	ipp_attribute_t	*attr = NULL;		/* Current attribute */
	cups_lang_t	*language = NULL;	/* Default language */
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	int		qcount = 0,		/* Number of active queue entries */
			qalloc = 0;		/* Number of queue entries allocated */
	print_queue_struct *queue = NULL,	/* Queue entries */
			*temp;		/* Temporary pointer for queue */
	char		*user_name = NULL,	/* job-originating-user-name attribute */
			*job_name = NULL;	/* job-name attribute */
	int		job_id;		/* job-id attribute */
	int		job_k_octets;	/* job-k-octets attribute */
	time_t		job_time;	/* time-at-creation attribute */
	ipp_jstate_t	job_status;	/* job-status attribute */
	int		job_priority;	/* job-priority attribute */
	size_t size;
	static const char *jattrs[] =	/* Requested job attributes */
			{
			  "job-id",
			  "job-k-octets",
			  "job-name",
			  "job-originating-user-name",
			  "job-priority",
			  "job-state",
			  "time-at-creation",
			};
	static const char *pattrs[] =	/* Requested printer attributes */
			{
			  "printer-state",
			  "printer-state-message"
			};

	*q = NULL;

	/* HACK ALERT!!!  The problem with support the 'printer name'
	   option is that we key the tdb off the sharename.  So we will
	   overload the lpq_command string to pass in the printername
	   (which is basically what we do for non-cups printers ... using
	   the lpq_command to get the queue listing). */

	if (!push_utf8_talloc(frame, &printername, lpq_command, &size)) {
		goto out;
	}
	DEBUG(5,("cups_queue_get(%s, %p, %p)\n", lpq_command, q, status));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
        * Generate the printer URI...
	*/

	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/printers/%s", printername);

       /*
	* Build an IPP_GET_JOBS request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    requested-attributes
	*    printer-uri
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_GET_JOBS;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(jattrs) / sizeof(jattrs[0])),
		      NULL, jattrs);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get jobs for %s - %s\n", uri,
			 ippErrorString(cupsLastError())));
		goto out;
	}

	if (response->request.status.status_code >= IPP_OK_CONFLICT) {
		DEBUG(0,("Unable to get jobs for %s - %s\n", uri,
			 ippErrorString(response->request.status.status_code)));
		goto out;
	}

       /*
        * Process the jobs...
	*/

        qcount = 0;
	qalloc = 0;
	queue  = NULL;

        for (attr = response->attrs; attr != NULL; attr = attr->next) {
	       /*
		* Skip leading attributes until we hit a job...
		*/

		while (attr != NULL && attr->group_tag != IPP_TAG_JOB)
        		attr = attr->next;

		if (attr == NULL)
			break;

	       /*
	        * Allocate memory as needed...
		*/
		if (qcount >= qalloc) {
			qalloc += 16;

			queue = SMB_REALLOC_ARRAY(queue, print_queue_struct, qalloc);

			if (queue == NULL) {
				DEBUG(0,("cups_queue_get: Not enough memory!"));
				qcount = 0;
				goto out;
			}
		}

		temp = queue + qcount;
		memset(temp, 0, sizeof(print_queue_struct));

	       /*
		* Pull the needed attributes from this job...
		*/

		job_id       = 0;
		job_priority = 50;
		job_status   = IPP_JOB_PENDING;
		job_time     = 0;
		job_k_octets = 0;
		user_name    = NULL;
		job_name     = NULL;

		while (attr != NULL && attr->group_tag == IPP_TAG_JOB) {
        		if (attr->name == NULL) {
				attr = attr->next;
				break;
			}

        		if (strcmp(attr->name, "job-id") == 0 &&
			    attr->value_tag == IPP_TAG_INTEGER)
				job_id = attr->values[0].integer;

        		if (strcmp(attr->name, "job-k-octets") == 0 &&
			    attr->value_tag == IPP_TAG_INTEGER)
				job_k_octets = attr->values[0].integer;

        		if (strcmp(attr->name, "job-priority") == 0 &&
			    attr->value_tag == IPP_TAG_INTEGER)
				job_priority = attr->values[0].integer;

        		if (strcmp(attr->name, "job-state") == 0 &&
			    attr->value_tag == IPP_TAG_ENUM)
				job_status = (ipp_jstate_t)(attr->values[0].integer);

        		if (strcmp(attr->name, "time-at-creation") == 0 &&
			    attr->value_tag == IPP_TAG_INTEGER)
				job_time = attr->values[0].integer;

        		if (strcmp(attr->name, "job-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&job_name,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		if (strcmp(attr->name, "job-originating-user-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&user_name,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

        		attr = attr->next;
		}

	       /*
		* See if we have everything needed...
		*/

		if (user_name == NULL || job_name == NULL || job_id == 0) {
			if (attr == NULL)
				break;
			else
				continue;
		}

		temp->job      = job_id;
		temp->size     = job_k_octets * 1024;
		temp->status   = job_status == IPP_JOB_PENDING ? LPQ_QUEUED :
				 job_status == IPP_JOB_STOPPED ? LPQ_PAUSED :
                                 job_status == IPP_JOB_HELD ? LPQ_PAUSED :
			         LPQ_PRINTING;
		temp->priority = job_priority;
		temp->time     = job_time;
		strlcpy(temp->fs_user, user_name, sizeof(temp->fs_user));
		strlcpy(temp->fs_file, job_name, sizeof(temp->fs_file));

		qcount ++;

		if (attr == NULL)
			break;
	}

	ippDelete(response);
	response = NULL;

       /*
	* Build an IPP_GET_PRINTER_ATTRIBUTES request, which requires the
	* following attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    requested-attributes
	*    printer-uri
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
	request->request.op.request_id   = 1;

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(pattrs) / sizeof(pattrs[0])),
		      NULL, pattrs);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer status for %s - %s\n", printername,
			 ippErrorString(cupsLastError())));
		goto out;
	}

	if (response->request.status.status_code >= IPP_OK_CONFLICT) {
		DEBUG(0,("Unable to get printer status for %s - %s\n", printername,
			 ippErrorString(response->request.status.status_code)));
		goto out;
	}

       /*
        * Get the current printer status and convert it to the SAMBA values.
	*/

        if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL) {
		if (attr->values[0].integer == IPP_PRINTER_STOPPED)
			status->status = LPSTAT_STOPPED;
		else
			status->status = LPSTAT_OK;
	}

        if ((attr = ippFindAttribute(response, "printer-state-message",
	                             IPP_TAG_TEXT)) != NULL) {
		char *msg = NULL;
		if (!pull_utf8_talloc(frame, &msg,
				attr->values[0].string.text,
				&size)) {
			SAFE_FREE(queue);
			qcount = 0;
			goto out;
		}
	        fstrcpy(status->message, msg);
	}

 out:

       /*
        * Return the job queue...
	*/

	*q = queue;

	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return qcount;
}


/*
 * 'cups_queue_pause()' - Pause a print queue.
 */

static int cups_queue_pause(int snum)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	char *printername = NULL;
	char *username = NULL;
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	size_t size;

	DEBUG(5,("cups_queue_pause(%d)\n", snum));

	/*
	 * Make sure we don't ask for passwords...
	 */

        cupsSetPasswordCB(cups_passwd_cb);

	/*
	 * Try to connect to the server...
	 */

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

	/*
	 * Build an IPP_PAUSE_PRINTER request, which requires the following
	 * attributes:
	 *
	 *    attributes-charset
	 *    attributes-natural-language
	 *    printer-uri
	 *    requesting-user-name
	 */

	request = ippNew();

	request->request.op.operation_id = IPP_PAUSE_PRINTER;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, PRINTERNAME(snum), &size)) {
		goto out;
	}
	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/printers/%s",
	         printername);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &username, current_user_info.unix_name, &size)) {
		goto out;
	}
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, username);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/admin/")) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to pause printer %s - %s\n", PRINTERNAME(snum),
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to pause printer %s - %s\n", PRINTERNAME(snum),
			ippErrorString(cupsLastError())));
	}

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}


/*
 * 'cups_queue_resume()' - Restart a print queue.
 */

static int cups_queue_resume(int snum)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	char *printername = NULL;
	char *username = NULL;
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	size_t size;

	DEBUG(5,("cups_queue_resume(%d)\n", snum));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

       /*
	* Try to connect to the server...
	*/

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

       /*
	* Build an IPP_RESUME_PRINTER request, which requires the following
	* attributes:
	*
	*    attributes-charset
	*    attributes-natural-language
	*    printer-uri
	*    requesting-user-name
	*/

	request = ippNew();

	request->request.op.operation_id = IPP_RESUME_PRINTER;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, PRINTERNAME(snum), &size)) {
		goto out;
	}
	slprintf(uri, sizeof(uri) - 1, "ipp://localhost/printers/%s",
	         printername);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", NULL, uri);

	if (!push_utf8_talloc(frame, &username, current_user_info.unix_name, &size)) {
		goto out;
	}
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name",
        	     NULL, username);

       /*
	* Do the request and get back a response...
	*/

	if ((response = cupsDoRequest(http, request, "/admin/")) != NULL) {
		if (response->request.status.status_code >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to resume printer %s - %s\n", PRINTERNAME(snum),
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to resume printer %s - %s\n", PRINTERNAME(snum),
			ippErrorString(cupsLastError())));
	}

 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}

/*******************************************************************
 * CUPS printing interface definitions...
 ******************************************************************/

struct printif	cups_printif =
{
	PRINT_CUPS,
	cups_queue_get,
	cups_queue_pause,
	cups_queue_resume,
	cups_job_delete,
	cups_job_pause,
	cups_job_resume,
	cups_job_submit,
};

bool cups_pull_comment_location(NT_PRINTER_INFO_LEVEL_2 *printer)
{
	TALLOC_CTX *frame = talloc_stackframe();
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	ipp_attribute_t	*attr;		/* Current attribute */
	cups_lang_t	*language = NULL;	/* Default language */
	char		uri[HTTP_MAX_URI];
	char *server = NULL;
	char *sharename = NULL;
	char *name = NULL;
	static const char *requested[] =/* Requested attributes */
			{
			  "printer-name",
			  "printer-info",
			  "printer-location"
			};
	bool ret = False;
	size_t size;

	DEBUG(5, ("pulling %s location\n", printer->sharename));

	/*
	 * Make sure we don't ask for passwords...
	 */

        cupsSetPasswordCB(cups_passwd_cb);

	/*
	 * Try to connect to the server...
	 */

	if ((http = cups_connect(frame)) == NULL) {
		goto out;
	}

	request = ippNew();

	request->request.op.operation_id = IPP_GET_PRINTER_ATTRIBUTES;
	request->request.op.request_id   = 1;

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

	if (lp_cups_server() != NULL && strlen(lp_cups_server()) > 0) {
		if (!push_utf8_talloc(frame, &server, lp_cups_server(), &size)) {
			goto out;
		}
	} else {
		server = talloc_strdup(frame,cupsServer());
	}
	if (server) {
		goto out;
	}
	if (!push_utf8_talloc(frame, &sharename, printer->sharename, &size)) {
		goto out;
	}
	slprintf(uri, sizeof(uri) - 1, "ipp://%s/printers/%s",
		 server, sharename);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
                     "printer-uri", NULL, uri);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(requested) / sizeof(requested[0])),
		      NULL, requested);

	/*
	 * Do the request and get back a response...
	 */

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer attributes - %s\n",
			 ippErrorString(cupsLastError())));
		goto out;
	}

	for (attr = response->attrs; attr != NULL;) {
		/*
		 * Skip leading attributes until we hit a printer...
		 */

		while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
			attr = attr->next;

		if (attr == NULL)
        		break;

		/*
		 * Pull the needed attributes from this printer...
		 */

		while ( attr && (attr->group_tag == IPP_TAG_PRINTER) ) {
        		if (strcmp(attr->name, "printer-name") == 0 &&
			    attr->value_tag == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&name,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
			}

			/* Grab the comment if we don't have one */
        		if ( (strcmp(attr->name, "printer-info") == 0)
			     && (attr->value_tag == IPP_TAG_TEXT)
			     && !strlen(printer->comment) )
			{
				char *comment = NULL;
				if (!pull_utf8_talloc(frame,
						&comment,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
				DEBUG(5,("cups_pull_comment_location: Using cups comment: %s\n",
					 comment));
			    	strlcpy(printer->comment,
					comment,
					sizeof(printer->comment));
			}

			/* Grab the location if we don't have one */
			if ( (strcmp(attr->name, "printer-location") == 0)
			     && (attr->value_tag == IPP_TAG_TEXT)
			     && !strlen(printer->location) )
			{
				char *location = NULL;
				if (!pull_utf8_talloc(frame,
						&location,
						attr->values[0].string.text,
						&size)) {
					goto out;
				}
				DEBUG(5,("cups_pull_comment_location: Using cups location: %s\n",
					 location));
			    	strlcpy(printer->location,
					location,
					sizeof(printer->location));
			}

        		attr = attr->next;
		}

		/*
		 * We have everything needed...
		 */

		if (name != NULL)
			break;
	}

	ret = True;

 out:
	if (response)
		ippDelete(response);

	if (request) {
		ippDelete(request);
	}

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	TALLOC_FREE(frame);
	return ret;
}

#else
 /* this keeps fussy compilers happy */
 void print_cups_dummy(void);
 void print_cups_dummy(void) {}
#endif /* HAVE_CUPS */
