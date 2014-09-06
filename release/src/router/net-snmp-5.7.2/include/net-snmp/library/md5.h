/*
 * ** **************************************************************************
 * ** md5.h -- Header file for implementation of MD5 Message Digest Algorithm **
 * ** Updated: 2/13/90 by Ronald L. Rivest                                    **
 * ** (C) 1990 RSA Data Security, Inc.                                        **
 * ** **************************************************************************
 */

#ifndef MD5_H
#define MD5_H

#ifdef __cplusplus
extern          "C" {
#endif


    /*
     * MDstruct is the data structure for a message digest computation.
     */
    typedef struct {
        unsigned int    buffer[4];      /* Holds 4-word result of MD computation */
        unsigned char   count[8];       /* Number of bits processed so far */
        unsigned int    done;   /* Nonzero means MD computation finished */
    } MDstruct     , *MDptr;

    /*
     * MDbegin(MD)
     * ** Input: MD -- an MDptr
     * ** Initialize the MDstruct prepatory to doing a message digest computation.
     */
    NETSNMP_IMPORT void MDbegin(MDptr);

    /*
     * MDupdate(MD,X,count)
     * ** Input: MD -- an MDptr
     * **        X -- a pointer to an array of unsigned characters.
     * **        count -- the number of bits of X to use (an unsigned int).
     * ** Updates MD using the first ``count'' bits of X.
     * ** The array pointed to by X is not modified.
     * ** If count is not a multiple of 8, MDupdate uses high bits of last byte.
     * ** This is the basic input routine for a user.
     * ** The routine terminates the MD computation when count < 512, so
     * ** every MD computation should end with one call to MDupdate with a
     * ** count less than 512.  Zero is OK for a count.
     */
    NETSNMP_IMPORT int MDupdate(MDptr, const unsigned char *, unsigned int);

    /*
     * MDprint(MD)
     * ** Input: MD -- an MDptr
     * ** Prints message digest buffer MD as 32 hexadecimal digits.
     * ** Order is from low-order byte of buffer[0] to high-order byte of buffer[3].
     * ** Each byte is printed with high-order hexadecimal digit first.
     */
    extern void     MDprint(MDptr);

    int             MDchecksum(const u_char * data, size_t len, u_char * mac,
                               size_t maclen);
    int             MDsign(const u_char * data, size_t len, u_char * mac,
                           size_t maclen, const u_char * secret,
                           size_t secretlen);
    void            MDget(MDstruct * MD, u_char * buf, size_t buflen);

    /*
     * ** End of md5.h
     * ****************************(cut)****************************************
     */
#ifdef __cplusplus
}
#endif
#endif                          /* MD5_H */
