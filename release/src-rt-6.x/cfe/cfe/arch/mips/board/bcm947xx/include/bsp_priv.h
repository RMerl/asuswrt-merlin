
#ifndef _LANGUAGE_ASSEMBLY

/* Global SB handle */
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih

/* BSP UI initialization */
extern int ui_init_bcm947xxcmds(void);


#if CFG_ET
extern void et_addcmd(void);
#endif

#if CFG_WLU
extern void wl_addcmd(void);
#endif

#endif
