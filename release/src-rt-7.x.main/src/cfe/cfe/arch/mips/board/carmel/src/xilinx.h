

typedef struct xpgminfo_s {
    /*
     * Info about our FPGA
     */
    uint64_t	INITpin;
    uint64_t	DONEpin;
    uint64_t	PGMpin;
    uint64_t	DINpin;
    uint64_t	DOUTpin;
    uint64_t	CCLKpin;

    /*
     * Private state 
     */
    uint64_t	olddir;		/* old state of GPIO direction bits */
} xpgminfo_t;


/* timeout after 10 ms */
#define XPGM_TIMEOUT 10000000

/* Errors */
#define XPGM_ERR_INIT_NEVER_LOW 	-1
#define XPGM_ERR_INIT_NEVER_HIGH 	-2
#define XPGM_ERR_INIT_NOT_HIGH 		-3
#define XPGM_ERR_DONE_NOT_HIGH 		-4
#define XPGM_ERR_OPEN_FAILED 		-5
#define XPGM_ERR_CLOSE_FAILED 		-6


/*  *********************************************************************
    *  Prototypes
    ********************************************************************* */

int xilinx_program(xpgminfo_t *xi,uint8_t *configBits, int nofBits);
