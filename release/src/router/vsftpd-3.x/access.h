#ifndef VSF_ACCESS_H
#define VSF_ACCESS_H

struct mystr;

/* vsf_access_check_file()
 * PURPOSE
 * Check whether the current session has permission to access the given
 * filename.
 * PARAMETERS
 * p_filename_str  - the filename to check access for
 * RETURNS
 * Returns 1 if access is granted, otherwise 0.
 */
int vsf_access_check_file(const struct mystr* p_filename_str);

/* vsf_access_check_file_visible()
 * PURPOSE
 * Check whether the current session has permission to view the given
 * filename in directory listings.
 * PARAMETERS
 * p_filename_str  - the filename to check visibility for
 * RETURNS
 * Returns 1 if the file should be visible, otherwise 0.
 */
int vsf_access_check_file_visible(const struct mystr* p_filename_str);

#endif /* VSF_ACCESS_H */

