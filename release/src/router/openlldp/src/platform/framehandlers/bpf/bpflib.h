/** @file bpflib.h
 * 
 * See LICENSE file for more info.
 *
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 **/

#ifndef BPFLIB_H
#define BPFLIB_H
#include <sys/types.h>
#include <net/bpf.h>

int bpf_set_timeout(int fd, struct timeval * tv_p);
int bpf_get_data_link(int fd, u_int * dl_p);
int bpf_get_blen(int fd, u_int * blen);
int bpf_set_promiscuous(int bpf_fd);
int bpf_get_stats(int bpf_fd, struct bpf_stat *bs_p);
int bpf_dispose(int bpf_fd);
int bpf_new();
int bpf_setif(int fd, char * en_name);
int bpf_set_immediate(int fd, u_int value);
int bpf_filter_receive_none(int fd);
int bpf_see_sent(int fd, int value);
int bpf_write(int fd, void * pkt, int len);
#endif // BPFLIB_H
