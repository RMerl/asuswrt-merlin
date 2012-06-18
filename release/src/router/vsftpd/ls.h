/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef VSF_LS_H
#define VSF_LS_H

struct mystr;
struct mystr_list;
struct vsf_sysutil_dir;

/* vsf_ls_populate_dir_list()
 * PURPOSE
 * Given a directory handle, populate a formatted directory entry list (/bin/ls
 * format). Also optionally populate a list of subdirectories.
 * PARAMETERS
 * p_list         - the string list object for the result list of entries
 * p_subdir_list  - the string list object for the result list of
 *                  subdirectories. May be 0 if client is not interested.
 * p_dir          - the directory object to be listed
 * p_base_dir_str - the directory name we are listing, relative to current
 * p_option_str   - the string of options given to the LIST/NLST command
 * p_filter_str   - the filter string given to LIST/NLST - e.g. "*.mp3"
 * is_verbose     - set to 1 for LIST, 0 for NLST
 */
void vsf_ls_populate_dir_list(const char* session_user,	// James
							  struct mystr_list* p_list,
                              struct mystr_list* p_subdir_list,
                              struct vsf_sysutil_dir* p_dir,
                              const struct mystr* p_base_dir_str,
                              const struct mystr* p_option_str,
                              const struct mystr* p_filter_str,
                              int is_verbose);

/* vsf_filename_passes_filter()
 * PURPOSE
 * Determine whether the given filename is matched by the given filter string.
 * The format of the filter string is a small subset of a regular expression.
 * Currently, just * and ? are supported.
 * PARAMETERS
 * p_filename_str  - the filename to match
 * p_filter_str    - the filter to match against
 * RETURNS
 * Returns 1 if there is a match, 0 otherwise.
 */
int vsf_filename_passes_filter(const struct mystr* p_filename_str,
                               const struct mystr* p_filter_str);

#endif /* VSF_LS_H */

