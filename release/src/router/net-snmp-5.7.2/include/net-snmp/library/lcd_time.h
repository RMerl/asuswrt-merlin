/*
 * lcd_time.h
 */

#ifndef _LCD_TIME_H
#define _LCD_TIME_H

#ifdef __cplusplus
extern          "C" {
#endif


    /*
     * undefine to enable time synchronization only on authenticated packets 
     */
#define LCD_TIME_SYNC_OPT 1

    /*
     * Macros and definitions.
     */
#define ETIMELIST_SIZE	23



    typedef struct enginetime_struct {
        u_char         *engineID;
        u_int           engineID_len;

        u_int           engineTime;
        u_int           engineBoot;
        /*
         * Time & boots values received from last authenticated
         * *   message within the previous time window.
         */

        time_t          lastReceivedEngineTime;
        /*
         * Timestamp made when engineTime/engineBoots was last
         * *   updated.  Measured in seconds.
         */

#ifdef LCD_TIME_SYNC_OPT
        u_int           authenticatedFlag;
#endif
        struct enginetime_struct *next;
    } enginetime   , *Enginetime;




    /*
     * Macros for streamlined engineID existence checks --
     *
     *      e       is char *engineID,
     *      e_l     is u_int engineID_len.
     *
     *
     *  ISENGINEKNOWN(e, e_l)
     *      Returns:
     *              TRUE    If engineID is recoreded in the EngineID List;
     *              FALSE   Otherwise.
     *
     *  ENSURE_ENGINE_RECORD(e, e_l)
     *      Adds the given engineID to the EngineID List if it does not exist
     *              already.  engineID is added with a <enginetime, engineboots>
     *              tuple of <0,0>.  ALWAYS succeeds -- except in case of a
     *              fatal internal error.
     *      Returns:
     *              SNMPERR_SUCCESS On success;
     *              SNMPERR_GENERR  Otherwise.
     *
     *  MAKENEW_ENGINE_RECORD(e, e_l)
     *      Returns:
     *              SNMPERR_SUCCESS If engineID already exists in the EngineID List;
     *              SNMPERR_GENERR  Otherwise -and- invokes ENSURE_ENGINE_RECORD()
     *                                      to add an entry to the EngineID List.
     *
     * XXX  Requres the following declaration in modules calling ISENGINEKNOWN():
     *      static u_int    dummy_etime, dummy_eboot;
     */
#define ISENGINEKNOWN(e, e_l)					\
	( (get_enginetime(e, e_l,				\
		&dummy_eboot, &dummy_etime, TRUE) == SNMPERR_SUCCESS)	\
		? TRUE						\
		: FALSE )

#define ENSURE_ENGINE_RECORD(e, e_l)				\
	( (set_enginetime(e, e_l, 0, 0, FALSE) == SNMPERR_SUCCESS)	\
		? SNMPERR_SUCCESS				\
		: SNMPERR_GENERR )

#define MAKENEW_ENGINE_RECORD(e, e_l)				\
	( (ISENGINEKNOWN(e, e_l) == TRUE)			\
		? SNMPERR_SUCCESS				\
		: (ENSURE_ENGINE_RECORD(e, e_l), SNMPERR_GENERR) )



    /*
     * Prototypes.
     */
    int             get_enginetime(const u_char * engineID, u_int engineID_len,
                                   u_int * engine_boot,
                                   u_int * engine_time,
                                   u_int authenticated);

    int             get_enginetime_ex(u_char * engineID,
                                      u_int engineID_len,
                                      u_int * engine_boot,
                                      u_int * engine_time,
                                      u_int * last_engine_time,
                                      u_int authenticated);

    int             set_enginetime(const u_char * engineID, u_int engineID_len,
                                   u_int engine_boot, u_int engine_time,
                                   u_int authenticated);

         
         
         
         
         
         
        Enginetime
        search_enginetime_list(const u_char * engineID, u_int engineID_len);

    int             hash_engineID(const u_char * engineID, u_int engineID_len);

    void            dump_etimelist_entry(Enginetime e, int count);
    void            dump_etimelist(void);
    void            free_etimelist(void);
    void            free_enginetime(unsigned char *engineID, size_t engineID_len);

#ifdef __cplusplus
}
#endif
#endif                          /* _LCD_TIME_H */
