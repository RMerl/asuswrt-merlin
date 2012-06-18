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
#ifndef VSF_PARSECONF_H
#define VSF_PARSECONF_H

/* vsf_parseconf_load_file()
 * PURPOSE
 * Parse the given file as a vsftpd config file. If the file cannot be
 * opened for whatever reason, a fatal error is raised. If the file contains
 * any syntax errors, a fatal error is raised.
 * If the call returns (no fatal error raised), then the config file was
 * parsed and the global config settings will have been updated.
 * PARAMETERS
 * p_filename     - the name of the config file to parse
 * errs_fatal     - errors will cause the calling process to exit if not 0
 * NOTES
 * If p_filename is NULL, then the last filename passed to this function is
 * used to reload the configuration details.
 */
void vsf_parseconf_load_file(const char* p_filename, int errs_fatal);

#endif /* VSF_PARSECONF_H */

