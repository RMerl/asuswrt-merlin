#ifndef VSF_STANDALONE_H
#define VSF_STANDALONE_H

struct vsf_client_launch
{
  unsigned int num_children;
  unsigned int num_this_ip;
};

/* vsf_standalone_main()
 * PURPOSE
 * This function starts listening on the network for incoming FTP connections.
 * When it gets one, it returns to the caller in a new process, with file
 * descriptor 0, 1 and 2 set to the network socket of the new client.
 *
 * RETURNS
 * Returns a structure representing the current number of clients, and
 * instances for this IP addresss.
 */
struct vsf_client_launch vsf_standalone_main(void);

#endif /* VSF_STANDALONE_H */

