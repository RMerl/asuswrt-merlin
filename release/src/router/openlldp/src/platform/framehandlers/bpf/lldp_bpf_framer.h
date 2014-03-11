/** @file lldp_bpf_framer.h
 * 
 * See LICENSE file for more info.
 *
 * Authors: Terry Simons (terry.simons@gmail.com)
 * 
 **/

#ifdef BPF_FRAMER
#ifndef __BPF_FRAMER_H__
#define __BPF_FRAMER_H__

int _getmac(uint8_t *dest, char *ifname);
int _getip(uint8_t *dest, char *ifname);

#endif /* __BPF_FRAMER_H__ */
#endif /* BPF_FRAMER */
