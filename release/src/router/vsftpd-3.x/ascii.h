#ifndef VSFTP_ASCII_H
#define VSFTP_ASCII_H

struct mystr;

/* vsf_ascii_ascii_to_bin()
 * PURPOSE
 * This function converts an input buffer from ascii format to binary format.
 * This entails ripping out all occurences of '\r' that are followed by '\n'.
 *  The result is stored in "p_out".
 * PARAMETERS
 * p_in         - the input and output buffer, which MUST BE at least as big as
 *                "in_len" PLUS ONE. This is to cater for a leading '\r' in the
 *                buffer if certain conditions are met.
 * in_len       - the length in bytes of the  buffer.
 * prev_cr      - set to non-zero if this buffer fragment was immediately
 *                preceeded by a '\r'.
 * RETURNS
 * The number of characters stored in the buffer, the buffer address, and
 * if we ended on a '\r'.
 */
struct ascii_to_bin_ret
{
  unsigned int stored;
  int last_was_cr;
  char* p_buf;
};
struct ascii_to_bin_ret vsf_ascii_ascii_to_bin(
  char* p_in, unsigned int in_len, int prev_cr);

/* vsf_ascii_bin_to_ascii()
 * PURPOSE
 * This function converts an input buffer from binary format to ascii format.
 * This entails replacing all occurences of '\n' with '\r\n'. The result is
 * stored in "p_out".
 * PARAMETERS
 * p_in         - the input buffer, which is not modified
 * p_out        - the output buffer, which MUST BE at least TWICE as big as
 *                "in_len"
 * in_len       - the length in bytes of the input buffer
 * prev_cr      - set to non-zero if this buffer fragment was immediately
 *                preceeded by a '\r'.
 * RETURNS
 * The number of characters stored in the output buffer, and whether the last
 * character stored was '\r'.
 */
struct bin_to_ascii_ret
{
  unsigned int stored;
  int last_was_cr;
};
struct bin_to_ascii_ret vsf_ascii_bin_to_ascii(const char* p_in,
                                               char* p_out,
                                               unsigned int in_len,
                                               int prev_cr);

#endif /* VSFTP_ASCII_H */

