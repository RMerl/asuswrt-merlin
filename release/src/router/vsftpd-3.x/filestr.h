#ifndef VSF_FILESTR_H
#define VSF_FILESTR_H

/* Forward declares */
struct mystr;

/* str_fileread()
 * PURPOSE
 * Read the contents of a file into a string buffer, up to a size limit of
 * "maxsize"
 * PARAMETERS
 * p_str        - destination buffer object to contain the file
 * p_filename   - the filename to try and read into the buffer
 * maxsize      - the maximum amount of buffer we will fill. Larger files will
 *                be truncated.
 * RETURNS
 * An integer representing the success/failure of opening the file
 * "p_filename". Zero indicates success. If successful, the file is read into
 * the "p_str" string object. If not successful, "p_str" will point to an
 * empty buffer.
 */
int str_fileread(struct mystr* p_str, const char* p_filename,
                 unsigned int maxsize);

#endif /* VSF_FILESTR_H */

