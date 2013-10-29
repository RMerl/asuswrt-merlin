#ifndef _namecheap_h_
#define _namecheap_h_

#define NC_DEFAULT_SERVER "dynamicdns.park-your-domain.com"
#define NC_DEFAULT_PORT "80"
#define NC_REQUEST "/update"

extern const char *NC_fields_used[];

extern int NC_update_entry(void);
extern int NC_check_info(void);

#endif
