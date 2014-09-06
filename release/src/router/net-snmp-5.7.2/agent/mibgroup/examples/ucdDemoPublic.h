/*
 * ucdDemoPublic.h 
 */

#ifndef _MIBGROUP_UCDDEMOPUBLIC_H
#define _MIBGROUP_UCDDEMOPUBLIC_H

/*
 * we use header_generic from the util_funcs module
 */

config_require(util_funcs/header_generic)

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * Magic number definitions: 
     */
#define   UCDDEMORESETKEYS      1
#define   UCDDEMOPUBLICSTRING   2
#define   UCDDEMOUSERLIST       3
#define   UCDDEMOPASSPHRASE     4
    /*
     * function definitions 
     */
     void     init_ucdDemoPublic(void);
     FindVarMethod var_ucdDemoPublic;
     WriteMethod     write_ucdDemoResetKeys;
     WriteMethod     write_ucdDemoPublicString;


#ifdef __cplusplus
}
#endif

#endif                          /* _MIBGROUP_UCDDEMOPUBLIC_H */
