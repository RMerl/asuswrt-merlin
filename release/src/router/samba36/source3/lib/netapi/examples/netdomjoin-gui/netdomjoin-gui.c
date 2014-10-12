/*
 *  Unix SMB/CIFS implementation.
 *  Join Support (gtk + netapi)
 *  Copyright (C) Guenther Deschner 2007-2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include <netapi.h>

#define MAX_CRED_LEN 256
#define MAX_NETBIOS_NAME_LEN 15

#define SAMBA_ICON_PATH  "/usr/share/pixmaps/samba/samba.ico"
#define SAMBA_IMAGE_PATH "/usr/share/pixmaps/samba/logo.png"
#define SAMBA_IMAGE_PATH_SMALL "/usr/share/pixmaps/samba/logo-small.png"

#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); x=NULL;} } while(0)

static gboolean verbose = FALSE;

typedef struct join_state {
	struct libnetapi_ctx *ctx;
	GtkWidget *window_main;
	GtkWidget *window_parent;
	GtkWidget *window_do_change;
	GtkWidget *window_creds_prompt;
	GtkWidget *entry_account;
	GtkWidget *entry_password;
	GtkWidget *entry_domain;
	GtkWidget *entry_ou_list;
	GtkWidget *entry_workgroup;
	GtkWidget *button_ok;
	GtkWidget *button_apply;
	GtkWidget *button_ok_creds;
	GtkWidget *button_get_ous;
	GtkWidget *label_reboot;
	GtkWidget *label_current_name_buffer;
	GtkWidget *label_current_name_type;
	GtkWidget *label_full_computer_name;
	GtkWidget *label_winbind;
	uint16_t name_type_initial;
	uint16_t name_type_new;
	char *name_buffer_initial;
	char *name_buffer_new;
	char *password;
	char *account;
	char *comment;
	char *comment_new;
	char *my_fqdn;
	char *my_dnsdomain;
	char *my_hostname;
	uint16_t server_role;
	gboolean settings_changed;
	gboolean hostname_changed;
	uint32_t stored_num_ous;
	char *target_hostname;
	uid_t uid;
} join_state;

static void callback_creds_prompt(GtkWidget *widget,
				  gpointer data,
				  const char *label_string,
				  gpointer cont_fn);


static void debug(const char *format, ...)
{
	va_list args;

	if (!verbose) {
		return;
	}

	va_start(args, format);
	g_vprintf(format, args);
	va_end(args);
}

static gboolean callback_delete_event(GtkWidget *widget,
				      GdkEvent *event,
				      gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

static void callback_do_close_data(GtkWidget *widget,
				   gpointer data)
{
	debug("callback_do_close_data called\n");

	if (data) {
		gtk_widget_destroy(GTK_WIDGET(data));
	}
}

static void callback_do_close_widget(GtkWidget *widget,
				     gpointer data)
{
	debug("callback_do_close_widget called\n");

	if (widget) {
		gtk_widget_destroy(widget);
	}
}

static void callback_do_freeauth(GtkWidget *widget,
				 gpointer data)
{
	struct join_state *state = (struct join_state *)data;

	debug("callback_do_freeauth called\n");

	SAFE_FREE(state->account);
	SAFE_FREE(state->password);

	if (state->window_creds_prompt) {
		gtk_widget_destroy(GTK_WIDGET(state->window_creds_prompt));
		state->window_creds_prompt = NULL;
	}
}

static void callback_do_freeauth_and_close(GtkWidget *widget,
					   gpointer data)
{
	struct join_state *state = (struct join_state *)data;

	debug("callback_do_freeauth_and_close called\n");

	SAFE_FREE(state->account);
	SAFE_FREE(state->password);

	if (state->window_creds_prompt) {
		gtk_widget_destroy(GTK_WIDGET(state->window_creds_prompt));
		state->window_creds_prompt = NULL;
	}
	if (state->window_do_change) {
		gtk_widget_destroy(GTK_WIDGET(state->window_do_change));
		state->window_do_change = NULL;
	}
}

static void free_join_state(struct join_state *s)
{
	SAFE_FREE(s->name_buffer_initial);
	SAFE_FREE(s->name_buffer_new);
	SAFE_FREE(s->password);
	SAFE_FREE(s->account);
	SAFE_FREE(s->comment);
	SAFE_FREE(s->comment_new);
	SAFE_FREE(s->my_fqdn);
	SAFE_FREE(s->my_dnsdomain);
	SAFE_FREE(s->my_hostname);
}

static void do_cleanup(struct join_state *state)
{
	libnetapi_free(state->ctx);
	free_join_state(state);
}

static void callback_apply_description_change(GtkWidget *widget,
					      gpointer data)
{
	struct join_state *state = (struct join_state *)data;
	NET_API_STATUS status = 0;
	uint32_t parm_err = 0;
	struct SERVER_INFO_1005 info1005;
	GtkWidget *dialog;

	info1005.sv1005_comment = state->comment_new;

	status = NetServerSetInfo(state->target_hostname,
				  1005,
				  (uint8_t *)&info1005,
				  &parm_err);
	if (status) {
		debug("NetServerSetInfo failed with: %s\n",
			libnetapi_errstr(status));
		dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_main),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"Failed to change computer description: %s.",
						libnetapi_get_error_string(state->ctx, status));
		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_main));

		g_signal_connect_swapped(dialog, "response",
					 G_CALLBACK(gtk_widget_destroy),
					 dialog);

		gtk_widget_show(dialog);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(state->button_apply), FALSE);
}

static void callback_do_exit(GtkWidget *widget,
			     gpointer data)
{
#if 0
	GtkWidget *dialog;
	gint result;
#endif
	struct join_state *state = (struct join_state *)data;

	if (!state->settings_changed) {
		callback_delete_event(NULL, NULL, NULL);
		return;
	}

#if 0
	dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_main),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_YES_NO,
					"You must restart your computer before the new settings will take effect.");
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	switch (result) {
		case GTK_RESPONSE_YES:
			g_print("would reboot here\n");
			break;
		case GTK_RESPONSE_NO:
		default:
			break;
	}
	if (dialog) {
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
#endif
	if (state->window_main) {
		gtk_widget_destroy(GTK_WIDGET(state->window_main));
		state->window_main = NULL;
	}
	do_cleanup(state);
	exit(0);
}


static void callback_do_reboot(GtkWidget *widget,
			       gpointer data,
			       gpointer data2)
{
	GtkWidget *dialog;
	struct join_state *state = (struct join_state *)data2;

	debug("callback_do_reboot\n");

	state->settings_changed = TRUE;
	dialog = gtk_message_dialog_new(GTK_WINDOW(data),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					"You must restart this computer for the changes to take effect.");
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
#if 0
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gtk_widget_destroy),
				 dialog);

	debug("showing dialog\n");
	gtk_widget_show(dialog);
#else
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(GTK_WIDGET(dialog));
#endif

	gtk_label_set_text(GTK_LABEL(state->label_reboot),
			   "Changes will take effect after you restart this computer");

	debug("destroying do_change window\n");
	gtk_widget_destroy(GTK_WIDGET(state->window_do_change));

	{
		uint32_t status;
		const char *buffer;
		uint16_t type;

		status = NetGetJoinInformation(state->target_hostname,
					       &buffer,
					       &type);
		if (status != 0) {
			g_print("failed to query status\n");
			return;
		}

		debug("got new status: %s\n", buffer);

		SAFE_FREE(state->name_buffer_new);
		state->name_buffer_new = strdup(buffer);
		state->name_type_new = type;
		state->name_buffer_initial = strdup(buffer);
		state->name_type_initial = type;
		NetApiBufferFree((void *)buffer);

		gtk_label_set_text(GTK_LABEL(state->label_current_name_buffer),
				   state->name_buffer_new);
		if (state->name_type_new == NetSetupDomainName) {
			gtk_label_set_text(GTK_LABEL(state->label_current_name_type),
					   "Domain:");
		} else {
			gtk_label_set_text(GTK_LABEL(state->label_current_name_type),
					   "Workgroup:");
		}
	}
}

static void callback_return_username(GtkWidget *widget,
				     gpointer data)
{
	const gchar *entry_text;
	struct join_state *state = (struct join_state *)data;
	debug("callback_return_username called\n");
	if (!widget) {
		return;
	}
	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	if (!entry_text) {
		return;
	}
	debug("callback_return_username: %s\n", entry_text);
	SAFE_FREE(state->account);
	state->account = strdup(entry_text);
}

static void callback_return_username_and_enter(GtkWidget *widget,
					       gpointer data)
{
	const gchar *entry_text;
	struct join_state *state = (struct join_state *)data;
	if (!widget) {
		return;
	}
	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	if (!entry_text) {
		return;
	}
	debug("callback_return_username_and_enter: %s\n", entry_text);
	SAFE_FREE(state->account);
	state->account = strdup(entry_text);
	g_signal_emit_by_name(state->button_ok_creds, "clicked");
}

static void callback_return_password(GtkWidget *widget,
				     gpointer data)
{
	const gchar *entry_text;
	struct join_state *state = (struct join_state *)data;
	debug("callback_return_password called\n");
	if (!widget) {
		return;
	}
	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	if (!entry_text) {
		return;
	}
#ifdef DEBUG_PASSWORD
	debug("callback_return_password: %s\n", entry_text);
#else
	debug("callback_return_password: (not printed)\n");
#endif
	SAFE_FREE(state->password);
	state->password = strdup(entry_text);
}

static void callback_return_password_and_enter(GtkWidget *widget,
					       gpointer data)
{
	const gchar *entry_text;
	struct join_state *state = (struct join_state *)data;
	if (!widget) {
		return;
	}
	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	if (!entry_text) {
		return;
	}
#ifdef DEBUG_PASSWORD
	debug("callback_return_password_and_enter: %s\n", entry_text);
#else
	debug("callback_return_password_and_enter: (not printed)\n");
#endif
	SAFE_FREE(state->password);
	state->password = strdup(entry_text);
	g_signal_emit_by_name(state->button_ok_creds, "clicked");
}

static void callback_do_storeauth(GtkWidget *widget,
				  gpointer data)
{
	struct join_state *state = (struct join_state *)data;

	debug("callback_do_storeauth called\n");

	SAFE_FREE(state->account);
	SAFE_FREE(state->password);

	callback_return_username(state->entry_account, (gpointer)state);
	callback_return_password(state->entry_password, (gpointer)state);

	if (state->window_creds_prompt) {
		gtk_widget_destroy(GTK_WIDGET(state->window_creds_prompt));
		state->window_creds_prompt = NULL;
	}
}

static void callback_continue(GtkWidget *widget,
			      gpointer data)
{
	struct join_state *state = (struct join_state *)data;

	gtk_widget_grab_focus(GTK_WIDGET(state->button_ok));
	g_signal_emit_by_name(state->button_ok, "clicked");
}

static void callback_do_storeauth_and_continue(GtkWidget *widget,
					       gpointer data)
{
	callback_do_storeauth(widget, data);
	callback_continue(NULL, data);
}

static void callback_do_storeauth_and_scan(GtkWidget *widget,
					   gpointer data)
{
	struct join_state *state = (struct join_state *)data;
	callback_do_storeauth(widget, data);
	g_signal_emit_by_name(state->button_get_ous, "clicked");
}

static void callback_do_hostname_change(GtkWidget *widget,
					gpointer data)
{
	GtkWidget *dialog;
	const char *str = NULL;

	struct join_state *state = (struct join_state *)data;

	switch (state->name_type_initial) {
		case NetSetupDomainName: {
#if 0
			NET_API_STATUS status;
			const char *newname;
			char *p = NULL;

			newname = strdup(gtk_label_get_text(GTK_LABEL(state->label_full_computer_name)));
			if (!newname) {
				return;
			}

			p = strchr(newname, '.');
			if (p) {
				*p = '\0';
			}

			if (!state->account || !state->password) {
				debug("callback_do_hostname_change: no creds yet\n");
				callback_creds_prompt(NULL, state,
						      "Enter the name and password of an account with permission to change a computer name in a the domain.",
						      callback_do_storeauth_and_continue);
			}

			if (!state->account || !state->password) {
				debug("callback_do_hostname_change: still no creds???\n");
				return;
			}

			status = NetRenameMachineInDomain(state->target_hostname,
							  newname,
							  state->account,
							  state->password,
							  NETSETUP_ACCT_CREATE);
			SAFE_FREE(newname);
			/* we renamed the machine in the domain */
			if (status == 0) {
				return;
			}
			str = libnetapi_get_error_string(state->ctx, status);
#else
			str = "To be implemented: call NetRenameMachineInDomain\n";
#endif
			break;
		}
		case NetSetupWorkgroupName:
			str = "To be implemented: call SetComputerNameEx\n";
			break;
		default:
			break;
	}

	dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					"%s",str);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_main));
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gtk_widget_destroy),
				 dialog);
	gtk_widget_show(dialog);
}

static void callback_creds_prompt(GtkWidget *widget,
				  gpointer data,
				  const char *label_string,
				  gpointer cont_fn)
{
	GtkWidget *window;
	GtkWidget *box1;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *label;

	struct join_state *state = (struct join_state *)data;

	debug("callback_creds_prompt\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);

	gtk_window_set_title(GTK_WINDOW(window), "Computer Name Changes");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_widget_set_size_request(GTK_WIDGET(window), 380, 280);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), SAMBA_ICON_PATH, NULL);
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(state->window_do_change));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(callback_do_close_widget), NULL);

	state->window_creds_prompt = window;
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	box1 = gtk_vbox_new(FALSE, 0);

	gtk_container_add(GTK_CONTAINER(window), box1);

	label = gtk_label_new(label_string);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	gtk_box_pack_start(GTK_BOX(box1), label, FALSE, FALSE, 0);

	gtk_widget_show(label);

	/* USER NAME */
	label = gtk_label_new("User name:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(box1), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	state->entry_account = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->entry_account), MAX_CRED_LEN);
	g_signal_connect(G_OBJECT(state->entry_account), "activate",
			 G_CALLBACK(callback_return_username_and_enter),
			 (gpointer)state);
	gtk_editable_select_region(GTK_EDITABLE(state->entry_account),
				   0, GTK_ENTRY(state->entry_account)->text_length);
	gtk_box_pack_start(GTK_BOX(box1), state->entry_account, TRUE, TRUE, 0);
	gtk_widget_show(state->entry_account);

	/* PASSWORD */
	label = gtk_label_new("Password:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(box1), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	state->entry_password = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(state->entry_password), MAX_CRED_LEN);
	gtk_entry_set_visibility(GTK_ENTRY(state->entry_password), FALSE);
	g_signal_connect(G_OBJECT(state->entry_password), "activate",
			 G_CALLBACK(callback_return_password_and_enter),
			 (gpointer)state);
	gtk_editable_set_editable(GTK_EDITABLE(state->entry_password), TRUE);
	gtk_editable_select_region(GTK_EDITABLE(state->entry_password),
				   0, GTK_ENTRY(state->entry_password)->text_length);
	gtk_box_pack_start(GTK_BOX(box1), state->entry_password, TRUE, TRUE, 0);
	gtk_widget_show(state->entry_password);

	/* BUTTONS */
	bbox = gtk_hbutton_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	gtk_container_add(GTK_CONTAINER(box1), bbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 10);

	state->button_ok_creds = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_widget_grab_focus(GTK_WIDGET(state->button_ok_creds));
	gtk_container_add(GTK_CONTAINER(bbox), state->button_ok_creds);
	g_signal_connect(G_OBJECT(state->button_ok_creds), "clicked",
			 G_CALLBACK(cont_fn),
			 (gpointer)state);
	gtk_widget_show(state->button_ok_creds);

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(callback_do_freeauth),
			 (gpointer)state);
	gtk_widget_show_all(window);
}

static void callback_do_join(GtkWidget *widget,
			     gpointer data)
{
	GtkWidget *dialog;

	NET_API_STATUS status;
	const char *err_str = NULL;
	uint32_t join_flags = 0;
	uint32_t unjoin_flags = 0;
	gboolean domain_join = FALSE;
	gboolean try_unjoin = FALSE;
	gboolean join_creds_required = TRUE;
	gboolean unjoin_creds_required = TRUE;
	const char *new_workgroup_type = NULL;
	const char *initial_workgroup_type = NULL;
	const char *account_ou = NULL;

	struct join_state *state = (struct join_state *)data;

	if (state->hostname_changed) {
		callback_do_hostname_change(NULL, state);
		return;
	}

	switch (state->name_type_initial) {
		case NetSetupWorkgroupName:
			initial_workgroup_type = "workgroup";
			break;
		case NetSetupDomainName:
			initial_workgroup_type = "domain";
			break;
		default:
			break;
	}

	switch (state->name_type_new) {
		case NetSetupWorkgroupName:
			new_workgroup_type = "workgroup";
			break;
		case NetSetupDomainName:
			new_workgroup_type = "domain";
			break;
		default:
			break;
	}

	account_ou = gtk_combo_box_get_active_text(GTK_COMBO_BOX(state->entry_ou_list));
	if (account_ou && strlen(account_ou) == 0) {
		account_ou = NULL;
	}

	if ((state->name_type_initial != NetSetupDomainName) &&
	    (state->name_type_new != NetSetupDomainName)) {
		join_creds_required = FALSE;
		unjoin_creds_required = FALSE;
	}

	if (state->name_type_new == NetSetupDomainName) {
		domain_join = TRUE;
		join_creds_required = TRUE;
		join_flags = NETSETUP_JOIN_DOMAIN |
			     NETSETUP_ACCT_CREATE |
			     NETSETUP_DOMAIN_JOIN_IF_JOINED; /* for testing */
	}

	if ((state->name_type_initial == NetSetupDomainName) &&
	    (state->name_type_new == NetSetupWorkgroupName)) {
		try_unjoin = TRUE;
		unjoin_creds_required = TRUE;
		join_creds_required = FALSE;
		unjoin_flags = NETSETUP_JOIN_DOMAIN |
			       NETSETUP_ACCT_DELETE |
			       NETSETUP_IGNORE_UNSUPPORTED_FLAGS;
	}

	if (try_unjoin) {

		debug("callback_do_join: Unjoining\n");

		if (unjoin_creds_required) {
			if (!state->account || !state->password) {
				debug("callback_do_join: no creds yet\n");
				callback_creds_prompt(NULL, state,
						      "Enter the name and password of an account with permission to leave the domain.",
						      callback_do_storeauth_and_continue);
			}

			if (!state->account || !state->password) {
				debug("callback_do_join: still no creds???\n");
				return;
			}
		}

		status = NetUnjoinDomain(state->target_hostname,
					 state->account,
					 state->password,
					 unjoin_flags);
		if (status != 0) {
			callback_do_freeauth(NULL, state);
			err_str = libnetapi_get_error_string(state->ctx, status);
			g_print("callback_do_join: failed to unjoin (%s)\n",
				err_str);
#if 0

	/* in fact we shouldn't annoy the user with an error message here */

			dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"The following error occured attempting to unjoin the %s: \"%s\": %s",
							initial_workgroup_type,
							state->name_buffer_initial,
							err_str);
			gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
#endif
		}

	}

	/* before prompting for creds, make sure we can find a dc */

	if (domain_join) {

		struct DOMAIN_CONTROLLER_INFO *dc_info = NULL;

		status = DsGetDcName(NULL,
				     state->name_buffer_new,
				     NULL,
				     NULL,
				     0,
				     &dc_info);
		if (status != 0) {
			err_str = libnetapi_get_error_string(state->ctx, status);
			g_print("callback_do_join: failed find dc (%s)\n", err_str);

			dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"Failed to find a domain controller for domain: \"%s\": %s",
							state->name_buffer_new,
							err_str);

			gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
			gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
			g_signal_connect_swapped(dialog, "response",
						 G_CALLBACK(gtk_widget_destroy),
						 dialog);

			gtk_widget_show(dialog);

			return;
		}
	}

	if (join_creds_required) {
		if (!state->account || !state->password) {
			debug("callback_do_join: no creds yet\n");
			callback_creds_prompt(NULL, state,
					      "Enter the name and password of an account with permission to join the domain.",
					      callback_do_storeauth_and_continue);
		}

		if (!state->account || !state->password) {
			debug("callback_do_join: still no creds???\n");
			return;
		}
	}

	debug("callback_do_join: Joining a %s named %s using join_flags 0x%08x ",
		new_workgroup_type,
		state->name_buffer_new,
		join_flags);
	if (domain_join) {
		debug("as %s ", state->account);
#ifdef DEBUG_PASSWORD
		debug("with %s ", state->password);
#endif
	}
	debug("\n");

	status = NetJoinDomain(state->target_hostname,
			       state->name_buffer_new,
			       account_ou,
			       state->account,
			       state->password,
			       join_flags);
	if (status != 0) {
		callback_do_freeauth(NULL, state);
		err_str = libnetapi_get_error_string(state->ctx, status);
		g_print("callback_do_join: failed to join (%s)\n", err_str);

		dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"The following error occured attempting to join the %s: \"%s\": %s",
						new_workgroup_type,
						state->name_buffer_new,
						err_str);

		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
		g_signal_connect_swapped(dialog, "response",
					 G_CALLBACK(gtk_widget_destroy),
					 dialog);

		gtk_widget_show(dialog);

		return;
	}

	debug("callback_do_join: Successfully joined %s\n",
		new_workgroup_type);

	callback_do_freeauth(NULL, state);
	dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_OK,
					"Welcome to the %s %s.",
					state->name_buffer_new,
					new_workgroup_type);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	callback_do_reboot(NULL, state->window_parent, state);
}

static void callback_enter_hostname_and_unlock(GtkWidget *widget,
					       gpointer data)
{
	const gchar *entry_text = NULL;
	char *str = NULL;
	struct join_state *state = (struct join_state *)data;

	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	debug("callback_enter_hostname_and_unlock: %s\n", entry_text);
	if (!entry_text || entry_text[0] == 0) {
		state->hostname_changed = FALSE;
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		gtk_label_set_text(GTK_LABEL(state->label_full_computer_name), "");
		return;
	}
	if (strcasecmp(state->my_hostname, entry_text) == 0) {
		state->hostname_changed = FALSE;
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		/* return; */
	} else {
		state->hostname_changed = TRUE;
	}

	if (state->name_type_initial == NetSetupDomainName) {
		if (asprintf(&str, "%s.%s", entry_text, state->my_dnsdomain) == -1) {
			return;
		}
	} else {
		if (asprintf(&str, "%s.", entry_text) == -1) {
			return;
		}
	}
	gtk_label_set_text(GTK_LABEL(state->label_full_computer_name), str);
	free(str);

	if (state->hostname_changed && entry_text && entry_text[0] != 0 && entry_text[0] != '.') {
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), TRUE);
	}
}

static void callback_enter_computer_description_and_unlock(GtkWidget *widget,
							   gpointer data)
{
	const gchar *entry_text = NULL;
	struct join_state *state = (struct join_state *)data;
	int string_unchanged = FALSE;

	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	debug("callback_enter_computer_description_and_unlock: %s\n",
		entry_text);
#if 0
	if (!entry_text || entry_text[0] == 0) {
		string_unchanged = 1;
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_apply),
					 FALSE);
		return;
	}
#endif
	if (entry_text && state->comment && strcasecmp(state->comment, entry_text) == 0) {
		string_unchanged = TRUE;
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_apply),
					 FALSE);
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(state->button_apply), TRUE);
	SAFE_FREE(state->comment_new);
	state->comment_new = strdup(entry_text);

}


static void callback_enter_workgroup_and_unlock(GtkWidget *widget,
						gpointer data)
{
	const gchar *entry_text = NULL;
	struct join_state *state = (struct join_state *)data;

	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	debug("callback_enter_workgroup_and_unlock: %s\n", entry_text);
	if (!entry_text || entry_text[0] == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		return;
	}
	if ((strcasecmp(state->name_buffer_initial, entry_text) == 0) &&
	    (state->name_type_initial == NetSetupWorkgroupName)) {
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		return;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), TRUE);
	SAFE_FREE(state->name_buffer_new);
	state->name_buffer_new = strdup(entry_text);
	state->name_type_new = NetSetupWorkgroupName;
}

static void callback_enter_domain_and_unlock(GtkWidget *widget,
					     gpointer data)
{
	const gchar *entry_text = NULL;
	struct join_state *state = (struct join_state *)data;

	entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
	debug("callback_enter_domain_and_unlock: %s\n", entry_text);
	if (!entry_text || entry_text[0] == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		return;
	}
	if ((strcasecmp(state->name_buffer_initial, entry_text) == 0) &&
	    (state->name_type_initial == NetSetupDomainName)) {
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
		return;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_ou_list), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_get_ous), TRUE);
	SAFE_FREE(state->name_buffer_new);
	state->name_buffer_new = strdup(entry_text);
	state->name_type_new = NetSetupDomainName;
}

static void callback_apply_continue(GtkWidget *widget,
				    gpointer data)
{
	struct join_state *state = (struct join_state *)data;

	gtk_widget_grab_focus(GTK_WIDGET(state->button_apply));
	g_signal_emit_by_name(state->button_apply, "clicked");
}

static void callback_do_join_workgroup(GtkWidget *widget,
				       gpointer data)
{
	struct join_state *state = (struct join_state *)data;
	debug("callback_do_join_workgroup choosen\n");
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_workgroup), TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(state->entry_workgroup));
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_domain), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_ou_list), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_get_ous), FALSE);
	callback_enter_workgroup_and_unlock(state->entry_workgroup, state); /* TEST */
}

static void callback_do_join_domain(GtkWidget *widget,
				    gpointer data)
{
	struct join_state *state = (struct join_state *)data;
	debug("callback_do_join_domain choosen\n");
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_domain), TRUE);
	gtk_widget_grab_focus(GTK_WIDGET(state->entry_domain));
	gtk_widget_set_sensitive(GTK_WIDGET(state->entry_workgroup), FALSE);
	callback_enter_domain_and_unlock(state->entry_domain, state); /* TEST */
}

static void callback_do_getous(GtkWidget *widget,
			       gpointer data)
{
	NET_API_STATUS status;
	uint32_t num_ous = 0;
	const char **ous = NULL;
	int i;
	const char *domain = NULL;
	struct DOMAIN_CONTROLLER_INFO *dc_info = NULL;
	const char *err_str = NULL;
	GtkWidget *dialog;

	struct join_state *state = (struct join_state *)data;

	debug("callback_do_getous called\n");

	domain = state->name_buffer_new ? state->name_buffer_new : state->name_buffer_initial;

	status = DsGetDcName(NULL,
			     domain,
			     NULL,
			     NULL,
			     0,
			     &dc_info);
	if (status != 0) {
		err_str = libnetapi_get_error_string(state->ctx, status);
		g_print("callback_do_getous: failed find dc (%s)\n", err_str);

		dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_parent),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_CLOSE,
						"Failed to find a domain controller for domain: \"%s\": %s",
						domain,
						err_str);

		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
		g_signal_connect_swapped(dialog, "response",
					 G_CALLBACK(gtk_widget_destroy),
					 dialog);

		gtk_widget_show(dialog);

		return;
	}

	if (!state->account || !state->password) {
		debug("callback_do_getous: no creds yet\n");
		callback_creds_prompt(NULL, state,
				      "Enter the name and password of an account with permission to join the domain.",
				      callback_do_storeauth_and_scan);
	}

	if (!state->account || !state->password) {
		debug("callback_do_getous: still no creds ???\n");
		return;
	}

	status = NetGetJoinableOUs(state->target_hostname,
				   domain,
				   state->account,
				   state->password,
				   &num_ous, &ous);
	if (status != NET_API_STATUS_SUCCESS) {
		callback_do_freeauth(NULL, state);
		debug("failed to call NetGetJoinableOUs: %s\n",
			libnetapi_get_error_string(state->ctx, status));
		dialog = gtk_message_dialog_new(NULL,
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_INFO,
						GTK_BUTTONS_OK,
						"Failed to query joinable OUs: %s",
						libnetapi_get_error_string(state->ctx, status));
		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window_do_change));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		return;
	}

	for (i=0; i<state->stored_num_ous; i++) {
		gtk_combo_box_remove_text(GTK_COMBO_BOX(state->entry_ou_list), 0);
	}
	for (i=0; i<num_ous && ous[i] != NULL; i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(state->entry_ou_list),
					  ous[i]);
	}
	NetApiBufferFree(ous);
	state->stored_num_ous = num_ous;
	gtk_combo_box_set_active(GTK_COMBO_BOX(state->entry_ou_list), num_ous-1);
}

static void callback_do_change(GtkWidget *widget,
			       gpointer data)
{
	GtkWidget *window;
	GtkWidget *box1;
	GtkWidget *bbox;
	GtkWidget *button_workgroup;
	GtkWidget *button_domain;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *frame_horz;
	GtkWidget *vbox;
	GtkWidget *entry;
	GSList *group;

	struct join_state *state = (struct join_state *)data;

	debug("callback_do_change called\n");

#if 0
	/* FIXME: add proper warnings for Samba as a DC */
	if (state->server_role == 3) {
		GtkWidget *dialog;
		callback_do_freeauth(NULL, state);
		dialog = gtk_message_dialog_new(GTK_WINDOW(state->window_main),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						"Domain controller cannot be moved from one domain to another, they must first be demoted. Renaming this domain controller may cause it to become temporarily unavailable to users and computers. For information on renaming domain controllers, including alternate renaming methods, see Help and Support. To continue renaming this domain controller, click OK.");
		gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
		g_signal_connect_swapped(dialog, "response",
					 G_CALLBACK(gtk_widget_destroy),
					 dialog);

		gtk_widget_show(dialog);
		return;
	}
#endif

	state->button_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
	state->button_get_ous = gtk_button_new_with_label("Scan for joinable OUs");
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);

	gtk_window_set_title(GTK_WINDOW(window), "Computer Name Changes");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(window), 480, 650);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), SAMBA_ICON_PATH, NULL);
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(state->window_main));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(callback_do_close_widget), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	box1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), box1);

	label = gtk_label_new("You can change the name and membership of this computer. Changes may affect access to network ressources.");
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(box1), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	/* COMPUTER NAME */
	label = gtk_label_new("Computer name:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(box1), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	state->label_full_computer_name = gtk_label_new(NULL);
	{
		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), MAX_NETBIOS_NAME_LEN);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(callback_enter_hostname_and_unlock),
				 (gpointer)state);
		gtk_entry_set_text(GTK_ENTRY(entry), state->my_hostname);
		gtk_editable_select_region(GTK_EDITABLE(entry),
					   0, GTK_ENTRY(entry)->text_length);

		gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE); /* ! */
		gtk_box_pack_start(GTK_BOX(box1), entry, TRUE, TRUE, 0);
		gtk_widget_show(entry);
	}

	/* FULL COMPUTER NAME */
	label = gtk_label_new("Full computer name:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(box1), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	{
		const gchar *entry_text;
		char *str = NULL;
		entry_text = gtk_entry_get_text(GTK_ENTRY(entry));
		if (state->name_type_initial == NetSetupDomainName) {
			if (asprintf(&str, "%s.%s", entry_text,
				 state->my_dnsdomain) == -1) {
				return;
			}
		} else {
			if (asprintf(&str, "%s.", entry_text) == -1) {
				return;
			}
		}
		gtk_label_set_text(GTK_LABEL(state->label_full_computer_name),
				   str);
		free(str);
		gtk_misc_set_alignment(GTK_MISC(state->label_full_computer_name), 0, 0);
		gtk_box_pack_start(GTK_BOX(box1),
				   state->label_full_computer_name, TRUE, TRUE, 0);
		gtk_widget_show(state->label_full_computer_name);
	}

	/* BOX */
	frame_horz = gtk_frame_new ("Member Of");
	gtk_box_pack_start(GTK_BOX(box1), frame_horz, TRUE, TRUE, 10);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame_horz), vbox);

	/* TWO ENTRIES */
	state->entry_workgroup = gtk_entry_new();
	state->entry_domain = gtk_entry_new();

	/* DOMAIN */
	button_domain = gtk_radio_button_new_with_label(NULL, "Domain");
	if (state->name_type_initial == NetSetupDomainName) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_domain), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), button_domain, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(button_domain), "clicked",
			 G_CALLBACK(callback_do_join_domain),
			 (gpointer)state);

	{
		gtk_entry_set_max_length(GTK_ENTRY(state->entry_domain), 50);
		g_signal_connect(G_OBJECT(state->entry_domain), "changed",
				 G_CALLBACK(callback_enter_domain_and_unlock),
				 (gpointer)state);
		g_signal_connect(G_OBJECT(state->entry_domain), "activate",
				 G_CALLBACK(callback_continue),
				 (gpointer)state);
		if (state->name_type_initial == NetSetupDomainName) {
			gtk_entry_set_text(GTK_ENTRY(state->entry_domain),
					   state->name_buffer_initial);
			gtk_widget_set_sensitive(state->entry_workgroup, FALSE);
			gtk_widget_set_sensitive(state->entry_domain, TRUE);
		}
		gtk_editable_set_editable(GTK_EDITABLE(state->entry_domain), TRUE);
		gtk_box_pack_start(GTK_BOX(vbox), state->entry_domain, TRUE, TRUE, 0);
		gtk_widget_show(state->entry_domain);
	}
	gtk_widget_show(button_domain);

	/* WORKGROUP */
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button_domain));
	button_workgroup = gtk_radio_button_new_with_label(group, "Workgroup");
	if (state->name_type_initial == NetSetupWorkgroupName) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button_workgroup), TRUE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), button_workgroup, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(button_workgroup), "clicked",
			 G_CALLBACK(callback_do_join_workgroup),
			 (gpointer)state);
	{
		gtk_entry_set_max_length(GTK_ENTRY(state->entry_workgroup),
					 MAX_NETBIOS_NAME_LEN);
		g_signal_connect(G_OBJECT(state->entry_workgroup), "changed",
				 G_CALLBACK(callback_enter_workgroup_and_unlock),
				 (gpointer)state);
		g_signal_connect(G_OBJECT(state->entry_workgroup), "activate",
				 G_CALLBACK(callback_continue),
				 (gpointer)state);

		if (state->name_type_initial == NetSetupWorkgroupName) {
			gtk_entry_set_text(GTK_ENTRY(state->entry_workgroup),
					   state->name_buffer_initial);
			gtk_widget_set_sensitive(GTK_WIDGET(state->entry_domain), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(state->entry_workgroup), TRUE);
		}
		gtk_box_pack_start(GTK_BOX(vbox), state->entry_workgroup, TRUE, TRUE, 0);
		gtk_widget_show(state->entry_workgroup);
	}
	gtk_widget_show(button_workgroup);

	/* Advanced Options */
	frame_horz = gtk_frame_new("Advanced Options");
	gtk_box_pack_start(GTK_BOX(box1), frame_horz, TRUE, TRUE, 10);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(frame_horz), vbox);

	/* OUs */
	gtk_container_add(GTK_CONTAINER(vbox), state->button_get_ous);
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_get_ous), FALSE);
	g_signal_connect(G_OBJECT(state->button_get_ous), "clicked",
			 G_CALLBACK(callback_do_getous),
			 (gpointer)state);

	state->entry_ou_list = gtk_combo_box_entry_new_text();
	gtk_widget_set_sensitive(state->entry_ou_list, FALSE);
	if (state->name_type_initial == NetSetupWorkgroupName) {
		gtk_widget_set_sensitive(state->entry_ou_list, FALSE);
		gtk_widget_set_sensitive(state->button_get_ous, FALSE);
	}
	gtk_box_pack_start(GTK_BOX(vbox), state->entry_ou_list, TRUE, TRUE, 0);
	gtk_widget_show(state->entry_ou_list);

	{
		state->label_winbind = gtk_check_button_new_with_label("Modify winbind configuration");
		gtk_box_pack_start(GTK_BOX(vbox), state->label_winbind, TRUE, TRUE, 0);
		gtk_widget_set_sensitive(state->label_winbind, FALSE);
	}


	/* BUTTONS */
	bbox = gtk_hbutton_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	gtk_container_add(GTK_CONTAINER(box1), bbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 10);

	state->window_do_change = window;
	gtk_widget_set_sensitive(GTK_WIDGET(state->button_ok), FALSE);
	gtk_container_add(GTK_CONTAINER(bbox), state->button_ok);
	g_signal_connect(G_OBJECT(state->button_ok), "clicked",
			 G_CALLBACK(callback_do_join),
			 (gpointer)state);

	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(callback_do_freeauth_and_close),
			 (gpointer)state);

	gtk_widget_show_all(window);
}

static void callback_do_about(GtkWidget *widget,
			     gpointer data)
{
	GdkPixbuf *logo;
	GError    *error = NULL;
	GtkWidget *about;

	struct join_state *state = (struct join_state *)data;

	debug("callback_do_about called\n");

	logo = gdk_pixbuf_new_from_file(SAMBA_IMAGE_PATH,
	                                &error);
	if (logo == NULL) {
		g_print("failed to load logo from %s: %s\n",
			SAMBA_IMAGE_PATH, error->message);
	}

	about = gtk_about_dialog_new();
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(about), "Samba");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), "3.2.0pre3");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about),
		"Copyright Andrew Tridgell and the Samba Team 1992-2008\n"
		"Copyright Günther Deschner 2007-2008");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), "GPLv3");
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), "http://www.samba.org");
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about), "http://www.samba.org");
	if (logo) {
		gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about), logo);
	}
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), "Samba gtk domain join utility");
	gtk_window_set_modal(GTK_WINDOW(about), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(state->window_main));
	g_signal_connect_swapped(about, "response",
				 G_CALLBACK(gtk_widget_destroy),
				 about);

	gtk_widget_show(about);
}

static int draw_main_window(struct join_state *state)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *image;
	GtkWidget *table;
	GtkWidget *entry;
	GdkPixbuf *icon;
	GError    *error = NULL;

	icon = gdk_pixbuf_new_from_file(SAMBA_ICON_PATH,
	                                &error);
	if (icon == NULL) {
		g_print("failed to load icon from %s : %s\n",
			SAMBA_ICON_PATH, error->message);
	}

#if 1
	image = gtk_image_new_from_file(SAMBA_IMAGE_PATH_SMALL);
#else
	image = gtk_image_new_from_file("/usr/share/pixmaps/redhat-system_settings.png");
#endif
	if (image == NULL) {
		g_print("failed to load logo from %s : %s\n",
			SAMBA_IMAGE_PATH_SMALL, error->message);
	}

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	state->window_main = window;

	gtk_window_set_title(GTK_WINDOW(window), "Samba - Join Domain dialogue");
	gtk_widget_set_size_request(GTK_WIDGET(window), 600, 600);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), SAMBA_ICON_PATH, NULL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(callback_delete_event), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	main_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(window), main_vbox);

#if 0
	gtk_box_pack_start(GTK_BOX(main_vbox), image, TRUE, TRUE, 10);
	gtk_widget_show(image);
#endif
	/* Hbox */
	hbox = gtk_hbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(main_vbox), hbox);

	{
/*		gtk_box_pack_start(GTK_BOX(main_vbox), image, TRUE, TRUE, 10); */
/*		gtk_misc_set_alignment(GTK_MISC(image), 0, 0); */
		gtk_widget_set_size_request(GTK_WIDGET(image), 150, 40);
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 10);
		gtk_widget_show(image);

		/* Label */
		label = gtk_label_new("Samba uses the following information to identify your computer on the network.");
/*		gtk_misc_set_alignment(GTK_MISC(label), 0, 0); */
		gtk_widget_set_size_request(GTK_WIDGET(label), 400, 40);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
	}

	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
	gtk_container_add(GTK_CONTAINER(main_vbox), vbox);

	/* Table */
	table = gtk_table_new(6, 3, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	{
		/* Label */
		label = gtk_label_new("Computer description:");
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
		gtk_widget_show(label);

		state->button_apply = gtk_button_new_from_stock(GTK_STOCK_APPLY);

		/* Entry */
		entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry), 256);

		if (!state->target_hostname && state->uid != 0) {
			gtk_widget_set_sensitive(GTK_WIDGET(entry), FALSE);
		}
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(callback_enter_computer_description_and_unlock),
				 state);
		g_signal_connect(G_OBJECT(entry), "activate",
				 G_CALLBACK(callback_apply_continue),
				 (gpointer)state);

		gtk_entry_set_text(GTK_ENTRY(entry), (char *)state->comment);
		gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE); /* ! */
		gtk_table_attach_defaults(GTK_TABLE(table), entry, 1, 3, 0, 1);
		gtk_widget_show(entry);
	}

	/* Label */
	label = gtk_label_new("For example: \"Samba \%v\".");
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 3, 1, 2);
	gtk_widget_show(label);

	/* Label */
	label = gtk_label_new("Full computer name:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	gtk_widget_show(label);

	{
		/* Label */
		char *str = NULL;
		if (state->name_type_initial == NetSetupDomainName) {
			if (asprintf(&str, "%s.%s", state->my_hostname,
				 state->my_dnsdomain) == -1) {
				return -1;
			}
		} else {
			if (asprintf(&str, "%s.", state->my_hostname) == -1) {
				return -1;
			}
		}

		label = gtk_label_new(str);
		SAFE_FREE(str);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 3, 2, 3);
		gtk_widget_show(label);
	}

	/* Label */
	if (state->name_type_initial == NetSetupDomainName) {
		label = gtk_label_new("Domain:");
	} else {
		label = gtk_label_new("Workgroup:");
	}
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	gtk_widget_show(label);
	state->label_current_name_type = label;

	/* Label */
	label = gtk_label_new(state->name_buffer_initial);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 3, 3, 4);
	gtk_widget_show(label);
	state->label_current_name_buffer = label;

	{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		label = gtk_label_new("To rename this computer or join a domain, click Change.");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);


	}

	/* bbox */
	bbox = gtk_hbutton_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	gtk_container_add(GTK_CONTAINER(hbox), bbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 10);

	button = gtk_button_new_with_mnemonic("Ch_ange");
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(callback_do_change),
			 (gpointer)state);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	if (!state->target_hostname && state->uid != 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
	}
	gtk_widget_show(button);

	/* Label (hidden) */
	state->label_reboot = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(state->label_reboot), TRUE);
	gtk_misc_set_alignment(GTK_MISC(state->label_reboot), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), state->label_reboot, TRUE, TRUE, 0);
	if (!state->target_hostname && state->uid != 0) {
		gtk_label_set_text(GTK_LABEL(state->label_reboot),
			   "You cannot change computer description as you're not running with root permissions");
	}

	gtk_widget_show(state->label_reboot);

#if 0
	gtk_box_pack_start(GTK_BOX(vbox),
	   create_bbox(window, TRUE, NULL, 10, 85, 20, GTK_BUTTONBOX_END),
		      TRUE, TRUE, 5);
#endif
	{

		GtkWidget *frame;
		GtkWidget *bbox2;
		GtkWidget *button2;

		frame = gtk_frame_new(NULL);
		bbox2 = gtk_hbutton_box_new();

		gtk_container_set_border_width(GTK_CONTAINER(bbox2), 5);
		gtk_container_add(GTK_CONTAINER(frame), bbox2);

		/* Set the appearance of the Button Box */
		gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox2), GTK_BUTTONBOX_END);
		gtk_box_set_spacing(GTK_BOX(bbox2), 10);
		/*gtk_button_box_set_child_size(GTK_BUTTON_BOX(bbox2), child_w, child_h);*/

		button2 = gtk_button_new_from_stock(GTK_STOCK_OK);
		gtk_container_add(GTK_CONTAINER(bbox2), button2);
		g_signal_connect(G_OBJECT(button2), "clicked", G_CALLBACK(callback_do_exit), state);

		button2 = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
		gtk_container_add(GTK_CONTAINER(bbox2), button2);
		g_signal_connect(G_OBJECT(button2), "clicked",
				 G_CALLBACK(callback_delete_event),
				 window);

		gtk_container_add(GTK_CONTAINER(bbox2), state->button_apply);
		g_signal_connect(G_OBJECT(state->button_apply), "clicked",
				 G_CALLBACK(callback_apply_description_change),
				 state);
		gtk_widget_set_sensitive(GTK_WIDGET(state->button_apply), FALSE);

		button2 = gtk_button_new_from_stock(GTK_STOCK_ABOUT);
		gtk_container_add(GTK_CONTAINER(bbox2), button2);
		g_signal_connect(G_OBJECT(button2), "clicked",
				 G_CALLBACK(callback_do_about),
				 state);
#if 0
		button2 = gtk_button_new_from_stock(GTK_STOCK_HELP);
		gtk_container_add(GTK_CONTAINER(bbox2), button2);
		g_signal_connect(G_OBJECT(button2), "clicked",
				 G_CALLBACK(callback_do_about),
				 window);
#endif
		gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);
	}

	gtk_widget_show_all(window);

	return 0;
}

static int init_join_state(struct join_state **state)
{
	struct join_state *s;

	s = (struct join_state *)malloc(sizeof(struct join_state));
	if (!s) {
		return -1;
	}

	memset(s, '\0', sizeof(struct join_state));

	*state = s;

	return 0;
}

static NET_API_STATUS get_server_properties(struct join_state *state)
{
	struct SERVER_INFO_101 *info101 = NULL;
	struct SERVER_INFO_1005 *info1005 = NULL;
	NET_API_STATUS status;

	status = NetServerGetInfo(state->target_hostname,
				  101,
				  (uint8_t **)&info101);
	if (status == 0) {
		state->comment = strdup(info101->sv101_comment);
		if (!state->comment) {
			return -1;
		}
		SAFE_FREE(state->my_hostname);
		state->my_hostname = strdup(info101->sv101_name);
		if (!state->my_hostname) {
			return -1;
		}
		NetApiBufferFree(info101);
		return NET_API_STATUS_SUCCESS;
	}

	switch (status) {
		case 124: /* WERR_UNKNOWN_LEVEL */
		case 50: /* WERR_NOT_SUPPORTED */
			break;
		default:
			goto failed;
	}

	status = NetServerGetInfo(state->target_hostname,
				  1005,
				  (uint8_t **)&info1005);
	if (status == 0) {
		state->comment = strdup(info1005->sv1005_comment);
		if (!state->comment) {
			return -1;
		}
		NetApiBufferFree(info1005);
		return NET_API_STATUS_SUCCESS;
	}

 failed:
	printf("NetServerGetInfo failed with: %s\n",
		libnetapi_get_error_string(state->ctx, status));

	return status;
}

static int initialize_join_state(struct join_state *state,
				 const char *debug_level,
				 const char *target_hostname,
				 const char *target_username)
{
	struct libnetapi_ctx *ctx = NULL;
	NET_API_STATUS status = 0;

	status = libnetapi_init(&ctx);
	if (status) {
		return status;
	}

	if (debug_level) {
		libnetapi_set_debuglevel(ctx, debug_level);
	}

	if (target_hostname) {
		state->target_hostname = strdup(target_hostname);
		if (!state->target_hostname) {
			return -1;
		}
	}

	if (target_username) {
		char *puser = strdup(target_username);
		char *p = NULL;

		if ((p = strchr(puser,'%'))) {
			size_t len;
			*p = 0;
			libnetapi_set_username(ctx, puser);
			libnetapi_set_password(ctx, p+1);
			len = strlen(p+1);
			memset(strchr(target_username,'%')+1,'X',len);
		} else {
			libnetapi_set_username(ctx, puser);
		}
		free(puser);
	}

	{
		char my_hostname[HOST_NAME_MAX];
		const char *p = NULL;
		struct hostent *hp = NULL;

		if (gethostname(my_hostname, sizeof(my_hostname)) == -1) {
			return -1;
		}

		p = strchr(my_hostname, '.');
		if (p) {
			my_hostname[strlen(my_hostname)-strlen(p)] = '\0';
		}
		state->my_hostname = strdup(my_hostname);
		if (!state->my_hostname) {
			return -1;
		}
		debug("state->my_hostname: %s\n", state->my_hostname);

		hp = gethostbyname(my_hostname);
		if (!hp || !hp->h_name || !*hp->h_name) {
			return -1;
		}

		state->my_fqdn = strdup(hp->h_name);
		if (!state->my_fqdn) {
			return -1;
		}
		debug("state->my_fqdn: %s\n", state->my_fqdn);

		p = strchr(state->my_fqdn, '.');
		if (p) {
			p++;
			state->my_dnsdomain = strdup(p);
		} else {
			state->my_dnsdomain = strdup("");
		}
		if (!state->my_dnsdomain) {
			return -1;
		}
		debug("state->my_dnsdomain: %s\n", state->my_dnsdomain);
	}

	{
		const char *buffer = NULL;
		uint16_t type = 0;
		status = NetGetJoinInformation(state->target_hostname,
					       &buffer,
					       &type);
		if (status != 0) {
			printf("NetGetJoinInformation failed with: %s\n",
				libnetapi_get_error_string(state->ctx, status));
			return status;
		}
		debug("NetGetJoinInformation gave: %s and %d\n", buffer, type);
		state->name_buffer_initial = strdup(buffer);
		if (!state->name_buffer_initial) {
			return -1;
		}
		state->name_type_initial = type;
		NetApiBufferFree((void *)buffer);
	}

	status = get_server_properties(state);
	if (status != 0) {
		return -1;
	}

	state->uid = geteuid();

	state->ctx = ctx;

	return 0;
}

int main(int argc, char **argv)
{
	GOptionContext *context = NULL;
	static const char *debug_level = NULL;
	static const char *target_hostname = NULL;
	static const char *target_username = NULL;
	struct join_state *state = NULL;
	GError *error = NULL;
	int ret = 0;

	static GOptionEntry entries[] = {
		{ "debug", 'd', 0, G_OPTION_ARG_STRING, &debug_level, "Debug level (for samba)", "N" },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Verbose output", 0 },
		{ "target", 'S', 0, G_OPTION_ARG_STRING, &target_hostname, "Target hostname", 0 },
		{ "username", 'U', 0, G_OPTION_ARG_STRING, &target_username, "Target hostname", 0 },
		{ NULL }
	};

	context = g_option_context_new("- Samba domain join utility");
	g_option_context_add_main_entries(context, entries, NULL);
/*	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE); */
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, &error);

	gtk_init(&argc, &argv);
	g_set_application_name("Samba");

	ret = init_join_state(&state);
	if (ret) {
		return ret;
	}

	ret = initialize_join_state(state, debug_level,
				    target_hostname,
				    target_username);
	if (ret) {
		return ret;
	}

	draw_main_window(state);

	gtk_main();

	do_cleanup(state);

	return 0;
}
