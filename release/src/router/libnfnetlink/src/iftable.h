#ifndef _IFTABLE_H
#define _IFTABLE_H

int iftable_delete(u_int32_t dst, u_int32_t mask, u_int32_t gw, u_int32_t oif);
int iftable_insert(u_int32_t dst, u_int32_t mask, u_int32_t gw, u_int32_t oif);

int iftable_init(void);
void iftable_fini(void);

int iftable_dump(FILE *outfd);
int iftable_up(unsigned int index);
#endif
