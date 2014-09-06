/*
 * Don't include ourselves twice 
 */
#ifndef _WINEXTDLL_H
#define _WINEXTDLL_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Declare our publically-visible functions.
     * Typically, these will include the initialization and shutdown functions,
     *  the main request callback routine and any writeable object methods.
     *
     * Function prototypes are provided for the callback routine ('FindVarMethod')
     *  and writeable object methods ('WriteMethod').
     */
     void     init_winExtDLL(void);
     void     shutdown_winExtDLL(void);
   
#ifdef __cplusplus
}
#endif

#endif                          /* _WINEXTDLL_H */
