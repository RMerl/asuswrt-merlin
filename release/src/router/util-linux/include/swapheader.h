#ifndef _SWAPHEADER_H
#define _SWAPHEADER_H

struct swap_header_v1 {
        char         bootbits[1024];    /* Space for disklabel etc. */
	unsigned int version;
	unsigned int last_page;
	unsigned int nr_badpages;
	unsigned int padding[125];
	unsigned int badpages[1];
};


#define SWAP_UUID_LENGTH 16
#define SWAP_LABEL_LENGTH 16

struct swap_header_v1_2 {
	char	      bootbits[1024];    /* Space for disklabel etc. */
	unsigned int  version;
	unsigned int  last_page;
	unsigned int  nr_badpages;
	unsigned char uuid[SWAP_UUID_LENGTH];
	char	      volume_name[SWAP_LABEL_LENGTH];
	unsigned int  padding[117];
	unsigned int  badpages[1];
};

#endif /* _SWAPHEADER_H */
