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

/* vsf_parseconf_parse_setting()
 * PURPOSE
 * Handle a given name=value setting and apply it. Any whitespace at the
 * beginning is skipped.
 * PARAMETERS
 * p_settings    - the name=value pair to apply
 * errs_fatal    - errors will cause the calling process to exit if not 0
 */
void vsf_parseconf_load_setting(const char* p_setting, int errs_fatal);

#endif /* VSF_PARSECONF_H */

