/*
   Unix SMB/CIFS implementation.
   Share Database of available printers.
   Copyright (C) Simo Sorce 2010

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

#ifndef _PRINTER_LIST_H_
#define _PRINTER_LIST_H_

bool printer_list_parent_init(void);

/**
 * @brief Get the comment and the last refresh time from the printer list
 *        database.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  name     The printer name to lookup.
 *
 * @param[out] comment  A pointer to store the comment of the printer.
 *
 * @param[out] location A pointer to store the location of the printer.
 *
 * @param[out] last_refresh A pointer to store the last refresh time of the
 *                          printer.
 *
 * @return              NT_STATUS_OK on success, a correspoining NTSTATUS error
 *                      code on a failure.
 */
NTSTATUS printer_list_get_printer(TALLOC_CTX *mem_ctx,
				  const char *name,
				  const char **comment,
				  const char **location,
				  time_t *last_refresh);

/**
 * @brief Add a printer to the printer list database.
 *
 * @param[in]  mem_ctx  The talloc memory context to use.
 *
 * @param[in]  name     The printer name to store in the db.
 *
 * @param[in]  comment  The comment to store in the db.
 *
 * @param[in]  location  The location to store in the db.
 *
 * @param[in]  last_refresh The last refresh time of the printer to store in
 *                          the db.
 *
 * @return              NT_STATUS_OK on success, a correspoining NTSTATUS error
 *                      code on a failure.
 */
NTSTATUS printer_list_set_printer(TALLOC_CTX *mem_ctx,
				  const char *name,
				  const char *comment,
				  const char *location,
				  time_t last_refresh);

/**
 * @brief Get the time of the last refresh of the printer database.
 *
 * @param[out] last_refresh The last refresh time in the db.
 *
 * @return              NT_STATUS_OK on success, a correspoining NTSTATUS error
 *                      code on a failure.
 */
NTSTATUS printer_list_get_last_refresh(time_t *last_refresh);

/**
 * @brief Mark the database as reloaded.
 *
 * This sets the last refresh time to the current time. You can get the last
 * reload/refresh time of the database with printer_list_get_last_refresh().
 *
 * @return              NT_STATUS_OK on success, a correspoining NTSTATUS error
 *                      code on a failure.
 */
NTSTATUS printer_list_mark_reload(void);

/**
 * @brief Cleanup old entries in the database.
 *
 * Entries older than the last refresh times will be deleted.
 *
 * @return              NT_STATUS_OK on success, a correspoining NTSTATUS error
 *                      code on a failure.
 */
NTSTATUS printer_list_clean_old(void);

NTSTATUS printer_list_run_fn(void (*fn)(const char *, const char *, const char *, void *),
			     void *private_data);
#endif /* _PRINTER_LIST_H_ */
