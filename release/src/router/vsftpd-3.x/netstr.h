#ifndef VSFTP_NETSTR_H
#define VSFTP_NETSTR_H

struct mystr;
struct vsf_session;

typedef int (*str_netfd_read_t)(struct vsf_session*
                                p_sess, char*,
                                unsigned int);

/* str_netfd_alloc()
 * PURPOSE
 * Read a string from a network socket into a string buffer object. The string
 * is delimited by a specified string terminator character.
 * If any network related errors occur trying to read the string, this call
 * will exit the program.
 * This method avoids reading one character at a time from the network.
 * PARAMETERS
 * p_sess       - the session object, used for passing into the I/O callbacks
 * p_str        - the destination string object
 * term         - the character which will terminate the string. This character
 *                is included in the returned string.
 * p_readbuf    - pointer to a scratch buffer into which to read from the
 *                network. This buffer must be at least "maxlen" characters!
 * maxlen       - maximum length of string to return. If this limit is passed,
 *                an empty string will be returned.
 * p_peekfunc   - a function called to peek data from the network
 * p_readfunc   - a function called to read data from the network
 * RETURNS
 * -1 upon reaching max buffer length without seeing terminator, or the number
 * of bytes read, _including_ the terminator. 0 for an EOF on the socket.
 * Does not return (exits) for a serious socket error.
 */
int str_netfd_alloc(struct vsf_session* p_sess,
                    struct mystr* p_str,
                    char term,
                    char* p_readbuf,
                    unsigned int maxlen,
                    str_netfd_read_t p_peekfunc,
                    str_netfd_read_t p_readfunc);

/* str_netfd_read()
 * PURPOSE
 * Fills contents of a string buffer object from a (typically network) file
 * descriptor.
 * PARAMETERS
 * p_str        - the string object to be filled
 * fd           - the file descriptor to read from
 * len          - the number of bytes to read
 * RETURNS
 * Number read on success, -1 on failure. The read is considered a failure
 * unless the full requested byte count is read.
 */
int str_netfd_read(struct mystr* p_str, int fd, unsigned int len);

/* str_netfd_write()
 * PURPOSE
 * Write the contents of a string buffer object out to a (typically network)
 * file descriptor.
 * PARAMETERS
 * p_str        - the string object to send
 * fd           - the file descriptor to write to
 * RETURNS
 * Number written on success, -1 on failure. The write is considered a failure
 * unless the full string buffer object is written.
 */
int str_netfd_write(const struct mystr* p_str, int fd);

#endif /* VSFTP_NETSTR_H */

