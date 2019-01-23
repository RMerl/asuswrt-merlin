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
#include "printing/pcap.h"
#include "librpc/gen_ndr/ndr_printcap.h"

#ifdef HAVE_CUPS
#include <cups/cups.h>
#include <cups/language.h>

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetGroupTag(attr)  attr->group_tag
#define ippGetName(attr)      attr->name
#define ippGetValueTag(attr)  attr->value_tag
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#define ippGetInteger(attr, element) attr->values[element].integer
#define ippGetString(attr, element, language) attr->values[element].string.text

static ipp_attribute_t *
ippFirstAttribute(ipp_t *ipp)
{
  if (!ipp)
    return (NULL);
  return (ipp->current = ipp->attrs);
}

static ipp_attribute_t *
ippNextAttribute(ipp_t *ipp)
{
  if (!ipp || !ipp->current)
    return (NULL);
  return (ipp->current = ipp->current->next);
}

static int ippSetOperation(ipp_t *ipp, ipp_op_t op)
{
    ipp->request.op.operation_id = op;
    return (1);
}

static int ippSetRequestId(ipp_t *ipp, int request_id)
{
    ipp->request.any.request_id = request_id;
    return (1);
}
#endif

static SIG_ATOMIC_T gotalarm;

/***************************************************************
 Signal function to tell us we timed out.
****************************************************************/

static void gotalarm_sig(int signum)
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
                CatchSignal(SIGALRM, gotalarm_sig);
                alarm(timeout);
        }

#ifdef HAVE_HTTPCONNECTENCRYPT
	http = httpConnectEncrypt(server, port, lp_cups_encrypt());
#else
	http = httpConnect(server, port);
#endif


	CatchSignal(SIGALRM, SIG_IGN);
        alarm(0);

	if (http == NULL) {
		DEBUG(0,("Unable to connect to CUPS server %s:%d - %s\n",
			 server, port, strerror(errno)));
	}

	return http;
}

static bool send_pcap_blob(DATA_BLOB *pcap_blob, int fd)
{
	size_t ret;

	ret = sys_write(fd, &pcap_blob->length, sizeof(pcap_blob->length));
	if (ret != sizeof(pcap_blob->length)) {
		return false;
	}

	ret = sys_write(fd, pcap_blob->data, pcap_blob->length);
	if (ret != pcap_blob->length) {
		return false;
	}

	DEBUG(10, ("successfully sent blob of len %d\n", (int)ret));
	return true;
}

static bool recv_pcap_blob(TALLOC_CTX *mem_ctx, int fd, DATA_BLOB *pcap_blob)
{
	size_t blob_len;
	size_t ret;

	ret = sys_read(fd, &blob_len, sizeof(blob_len));
	if (ret != sizeof(blob_len)) {
		return false;
	}

	*pcap_blob = data_blob_talloc_named(mem_ctx, NULL, blob_len,
					   "cups pcap");
	if (pcap_blob->length != blob_len) {
		return false;
	}
	ret = sys_read(fd, pcap_blob->data, blob_len);
	if (ret != blob_len) {
		talloc_free(pcap_blob->data);
		return false;
	}

	DEBUG(10, ("successfully recvd blob of len %d\n", (int)ret));
	return true;
}

static bool process_cups_printers_response(TALLOC_CTX *mem_ctx,
					   ipp_t *response,
					   struct pcap_data *pcap_data)
{
	ipp_attribute_t	*attr;
	char *name;
	char *info;
	char *location = NULL;
	struct pcap_printer *printer;
	bool ret_ok = false;

	for (attr = ippFirstAttribute(response); attr != NULL;) {
	       /*
		* Skip leading attributes until we hit a printer...
		*/

		while (attr != NULL && ippGetGroupTag(attr) != IPP_TAG_PRINTER)
			attr = ippNextAttribute(response);

		if (attr == NULL)
			break;

	       /*
		* Pull the needed attributes from this printer...
		*/

		name       = NULL;
		info       = NULL;

		while (attr != NULL && ippGetGroupTag(attr) == IPP_TAG_PRINTER) {
			size_t size;
			if (strcmp(ippGetName(attr), "printer-name") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(mem_ctx,
						&name,
						ippGetString(attr, 0, NULL),
						&size)) {
					goto err_out;
				}
			}

			if (strcmp(ippGetName(attr), "printer-info") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_TEXT) {
				if (!pull_utf8_talloc(mem_ctx,
						&info,
						ippGetString(attr, 0, NULL),
						&size)) {
					goto err_out;
				}
			}

			if (strcmp(ippGetName(attr), "printer-location") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_TEXT) {
				if (!pull_utf8_talloc(mem_ctx,
						&location,
						ippGetString(attr, 0, NULL),
						&size)) {
					goto err_out;
				}
			}

			attr = ippNextAttribute(response);
		}

	       /*
		* See if we have everything needed...
		*/

		if (name == NULL)
			break;

		if (pcap_data->count == 0) {
			printer = talloc_array(mem_ctx, struct pcap_printer, 1);
		} else {
			printer = talloc_realloc(mem_ctx, pcap_data->printers,
						 struct pcap_printer,
						 pcap_data->count + 1);
		}
		if (printer == NULL) {
			goto err_out;
		}
		pcap_data->printers = printer;
		pcap_data->printers[pcap_data->count].name = name;
		pcap_data->printers[pcap_data->count].info = info;
		pcap_data->printers[pcap_data->count].location = location;
		pcap_data->count++;
	}

	ret_ok = true;
err_out:
	return ret_ok;
}

/*
 * request printer list from cups, send result back to up parent via fd.
 * returns true if the (possibly failed) result was successfuly sent to parent.
 */
static bool cups_cache_reload_async(int fd)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct pcap_data pcap_data;
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	cups_lang_t	*language = NULL;	/* Default language */
	static const char *requested[] =/* Requested attributes */
			{
			  "printer-name",
			  "printer-info",
			  "printer-location"
			};
	bool ret = False;
	enum ndr_err_code ndr_ret;
	DATA_BLOB pcap_blob;

	ZERO_STRUCT(pcap_data);
	pcap_data.status = NT_STATUS_UNSUCCESSFUL;

	DEBUG(5, ("reloading cups printcap cache\n"));

       /*
        * Make sure we don't ask for passwords...
	*/

        cupsSetPasswordCB(cups_passwd_cb);

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

	ippSetOperation(request, CUPS_GET_PRINTERS);
	ippSetRequestId(request, 1);

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(requested) / sizeof(requested[0])),
		      NULL, requested);

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer list - %s\n",
			 ippErrorString(cupsLastError())));
		goto out;
	}

	ret = process_cups_printers_response(frame, response, &pcap_data);
	if (!ret) {
		DEBUG(0,("failed to process cups response\n"));
		goto out;
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

	ippSetOperation(request, CUPS_GET_CLASSES);
	ippSetRequestId(request, 1);

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                     "attributes-natural-language", NULL, language->language);

        ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	              "requested-attributes",
		      (sizeof(requested) / sizeof(requested[0])),
		      NULL, requested);

	if ((response = cupsDoRequest(http, request, "/")) == NULL) {
		DEBUG(0,("Unable to get printer list - %s\n",
			 ippErrorString(cupsLastError())));
		goto out;
	}

	ret = process_cups_printers_response(frame, response, &pcap_data);
	if (!ret) {
		DEBUG(0,("failed to process cups response\n"));
		goto out;
	}

	pcap_data.status = NT_STATUS_OK;
 out:
	if (response)
		ippDelete(response);

	if (language)
		cupsLangFree(language);

	if (http)
		httpClose(http);

	ret = false;
	ndr_ret = ndr_push_struct_blob(&pcap_blob, frame, &pcap_data,
				       (ndr_push_flags_fn_t)ndr_push_pcap_data);
	if (ndr_ret == NDR_ERR_SUCCESS) {
		ret = send_pcap_blob(&pcap_blob, fd);
	}

	TALLOC_FREE(frame);
	return ret;
}

static struct fd_event *cache_fd_event;

static bool cups_pcap_load_async(struct tevent_context *ev,
				 struct messaging_context *msg_ctx,
				 int *pfd)
{
	int fds[2];
	pid_t pid;
	NTSTATUS status;

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

	status = reinit_after_fork(msg_ctx, ev, procid_self(), true);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("cups_pcap_load_async: reinit_after_fork() failed\n"));
		smb_panic("cups_pcap_load_async: reinit_after_fork() failed");
	}

	close(fds[0]);
	cups_cache_reload_async(fds[1]);
	close(fds[1]);
	_exit(0);
}

struct cups_async_cb_args {
	int pipe_fd;
	struct event_context *event_ctx;
	struct messaging_context *msg_ctx;
	void (*post_cache_fill_fn)(struct event_context *,
				   struct messaging_context *);
};

static void cups_async_callback(struct event_context *event_ctx,
				struct fd_event *event,
				uint16 flags,
				void *p)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct cups_async_cb_args *cb_args = (struct cups_async_cb_args *)p;
	struct pcap_cache *tmp_pcap_cache = NULL;
	bool ret_ok;
	struct pcap_data pcap_data;
	DATA_BLOB pcap_blob;
	enum ndr_err_code ndr_ret;
	int i;

	DEBUG(5,("cups_async_callback: callback received for printer data. "
		"fd = %d\n", cb_args->pipe_fd));

	ret_ok = recv_pcap_blob(frame, cb_args->pipe_fd, &pcap_blob);
	if (!ret_ok) {
		DEBUG(0,("failed to recv pcap blob\n"));
		goto err_out;
	}

	ndr_ret = ndr_pull_struct_blob(&pcap_blob, frame, &pcap_data,
				       (ndr_pull_flags_fn_t)ndr_pull_pcap_data);
	if (ndr_ret != NDR_ERR_SUCCESS) {
		goto err_out;
	}

	if (!NT_STATUS_IS_OK(pcap_data.status)) {
		DEBUG(0,("failed to retrieve printer list: %s\n",
			 nt_errstr(pcap_data.status)));
		goto err_out;
	}

	for (i = 0; i < pcap_data.count; i++) {
		ret_ok = pcap_cache_add_specific(&tmp_pcap_cache,
						 pcap_data.printers[i].name,
						 pcap_data.printers[i].info,
						 pcap_data.printers[i].location);
		if (!ret_ok) {
			DEBUG(0, ("failed to add to tmp pcap cache\n"));
			goto err_out;
		}
	}

	/* replace the system-wide pcap cache with a (possibly empty) new one */
	ret_ok = pcap_cache_replace(tmp_pcap_cache);
	if (!ret_ok) {
		DEBUG(0, ("failed to replace pcap cache\n"));
	} else if (cb_args->post_cache_fill_fn != NULL) {
		/* Caller requested post cache fill callback */
		cb_args->post_cache_fill_fn(cb_args->event_ctx,
					    cb_args->msg_ctx);
	}
err_out:
	pcap_cache_destroy_specific(&tmp_pcap_cache);
	TALLOC_FREE(frame);
	close(cb_args->pipe_fd);
	TALLOC_FREE(cb_args);
	TALLOC_FREE(cache_fd_event);
}

bool cups_cache_reload(struct tevent_context *ev,
		       struct messaging_context *msg_ctx,
		       void (*post_cache_fill_fn)(struct tevent_context *,
						  struct messaging_context *))
{
	struct cups_async_cb_args *cb_args;
	int *p_pipe_fd;

	cb_args = TALLOC_P(NULL, struct cups_async_cb_args);
	if (cb_args == NULL) {
		return false;
	}

	cb_args->post_cache_fill_fn = post_cache_fill_fn;
	cb_args->event_ctx = ev;
	cb_args->msg_ctx = msg_ctx;
	p_pipe_fd = &cb_args->pipe_fd;
	*p_pipe_fd = -1;

	/* Set up an async refresh. */
	if (!cups_pcap_load_async(ev, msg_ctx, p_pipe_fd)) {
		talloc_free(cb_args);
		return false;
	}

	DEBUG(10,("cups_cache_reload: async read on fd %d\n",
		*p_pipe_fd ));

	/* Trigger an event when the pipe can be read. */
	cache_fd_event = event_add_fd(ev,
				NULL, *p_pipe_fd,
				EVENT_FD_READ,
				cups_async_callback,
				(void *)cb_args);
	if (!cache_fd_event) {
		close(*p_pipe_fd);
		TALLOC_FREE(cb_args);
		return false;
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

	ippSetOperation(request, IPP_CANCEL_JOB);
	ippSetRequestId(request, 1);

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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
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

	ippSetOperation(request, IPP_HOLD_JOB);
	ippSetRequestId(request, 1);

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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
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

	ippSetOperation(request, IPP_RELEASE_JOB);
	ippSetRequestId(request, 1);

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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
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

static int cups_job_submit(int snum, struct printjob *pjob,
			   enum printing_types printing_type,
			   char *lpq_cmd)
{
	TALLOC_CTX *frame = talloc_stackframe();
	int		ret = 1;		/* Return value */
	http_t		*http = NULL;		/* HTTP connection to server */
	ipp_t		*request = NULL,	/* IPP Request */
			*response = NULL;	/* IPP Response */
	ipp_attribute_t *attr_job_id = NULL;	/* IPP Attribute "job-id" */
	cups_lang_t	*language = NULL;	/* Default language */
	char		uri[HTTP_MAX_URI]; /* printer-uri attribute */
	char *new_jobname = NULL;
	int		num_options = 0;
	cups_option_t 	*options = NULL;
	char *printername = NULL;
	char *user = NULL;
	char *jobname = NULL;
	char *cupsoptions = NULL;
	char *filename = NULL;
	size_t size;

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

	ippSetOperation(request, IPP_PRINT_JOB);
	ippSetRequestId(request, 1);

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, lp_printername(snum),
			      &size)) {
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

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
	             "job-originating-host-name", NULL,
		     pjob->clientmachine);

	if (!push_utf8_talloc(frame, &jobname, pjob->jobname, &size)) {
		goto out;
	}
	new_jobname = talloc_asprintf(frame,
			"%s%.8u %s", PRINT_SPOOL_PREFIX,
			pjob->jobid, jobname);
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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to print file to %s - %s\n",
				 lp_printername(snum),
			         ippErrorString(cupsLastError())));
		} else {
			ret = 0;
			attr_job_id = ippFindAttribute(response, "job-id", IPP_TAG_INTEGER);
			if(attr_job_id) {
				pjob->sysjob = ippGetInteger(attr_job_id, 0);
				DEBUG(5,("cups_job_submit: job-id %d\n", pjob->sysjob));
			} else {
				DEBUG(0,("Missing job-id attribute in IPP response"));
			}
		}
	} else {
		DEBUG(0,("Unable to print file to `%s' - %s\n",
			 lp_printername(snum),
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

	ippSetOperation(request, IPP_GET_JOBS);
	ippSetRequestId(request, 1);

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

	if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
		DEBUG(0,("Unable to get jobs for %s - %s\n", uri,
			 ippErrorString(ippGetStatusCode(response))));
		goto out;
	}

       /*
        * Process the jobs...
	*/

        qcount = 0;
	qalloc = 0;
	queue  = NULL;

        for (attr = ippFirstAttribute(response); attr != NULL; attr = ippNextAttribute(response)) {
	       /*
		* Skip leading attributes until we hit a job...
		*/

		while (attr != NULL && ippGetGroupTag(attr) != IPP_TAG_JOB)
			attr = ippNextAttribute(response);

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

		while (attr != NULL && ippGetGroupTag(attr) == IPP_TAG_JOB) {
			if (ippGetName(attr) == NULL) {
				attr = ippNextAttribute(response);
				break;
			}

			if (strcmp(ippGetName(attr), "job-id") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_INTEGER)
				job_id = ippGetInteger(attr, 0);

			if (strcmp(ippGetName(attr), "job-k-octets") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_INTEGER)
				job_k_octets = ippGetInteger(attr, 0);

			if (strcmp(ippGetName(attr), "job-priority") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_INTEGER)
				job_priority = ippGetInteger(attr, 0);

			if (strcmp(ippGetName(attr), "job-state") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_ENUM)
				job_status = (ipp_jstate_t)ippGetInteger(attr, 0);

			if (strcmp(ippGetName(attr), "time-at-creation") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_INTEGER)
				job_time = ippGetInteger(attr, 0);

			if (strcmp(ippGetName(attr), "job-name") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&job_name,
						ippGetString(attr, 0, NULL),
						&size)) {
					goto out;
				}
			}

			if (strcmp(ippGetName(attr), "job-originating-user-name") == 0 &&
			    ippGetValueTag(attr) == IPP_TAG_NAME) {
				if (!pull_utf8_talloc(frame,
						&user_name,
						ippGetString(attr, 0, NULL),
						&size)) {
					goto out;
				}
			}

			attr = ippNextAttribute(response);
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

		temp->sysjob   = job_id;
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

	ippSetOperation(request, IPP_GET_PRINTER_ATTRIBUTES);
	ippSetRequestId(request, 1);

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

	if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
		DEBUG(0,("Unable to get printer status for %s - %s\n", printername,
			 ippErrorString(ippGetStatusCode(response))));
		goto out;
	}

       /*
        * Get the current printer status and convert it to the SAMBA values.
	*/

        if ((attr = ippFindAttribute(response, "printer-state", IPP_TAG_ENUM)) != NULL) {
		if (ippGetInteger(attr, 0) == IPP_PRINTER_STOPPED)
			status->status = LPSTAT_STOPPED;
		else
			status->status = LPSTAT_OK;
	}

        if ((attr = ippFindAttribute(response, "printer-state-message",
	                             IPP_TAG_TEXT)) != NULL) {
		char *msg = NULL;
		if (!pull_utf8_talloc(frame, &msg,
				ippGetString(attr, 0, NULL),
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

	ippSetOperation(request, IPP_PAUSE_PRINTER);
	ippSetRequestId(request, 1);

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, lp_printername(snum),
			      &size)) {
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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to pause printer %s - %s\n",
				 lp_printername(snum),
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to pause printer %s - %s\n",
			 lp_printername(snum),
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

	ippSetOperation(request, IPP_RESUME_PRINTER);
	ippSetRequestId(request, 1);

	language = cupsLangDefault();

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                     "attributes-charset", NULL, "utf-8");

	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
        	     "attributes-natural-language", NULL, language->language);

	if (!push_utf8_talloc(frame, &printername, lp_printername(snum),
			      &size)) {
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
		if (ippGetStatusCode(response) >= IPP_OK_CONFLICT) {
			DEBUG(0,("Unable to resume printer %s - %s\n",
				 lp_printername(snum),
				ippErrorString(cupsLastError())));
		} else {
			ret = 0;
		}
	} else {
		DEBUG(0,("Unable to resume printer %s - %s\n",
			 lp_printername(snum),
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

#else
 /* this keeps fussy compilers happy */
 void print_cups_dummy(void);
 void print_cups_dummy(void) {}
#endif /* HAVE_CUPS */
