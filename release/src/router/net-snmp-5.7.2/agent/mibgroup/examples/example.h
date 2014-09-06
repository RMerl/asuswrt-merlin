/*
 *  Template MIB group interface - example.h
 *
 */

/*
 * Don't include ourselves twice 
 */
#ifndef _MIBGROUP_EXAMPLE_H
#define _MIBGROUP_EXAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

    /*
     * We use 'header_generic' from the util_funcs/header_generic module,
     *  so make sure this module is included in the agent.
     */
config_require(util_funcs/header_generic)


    /*
     * Declare our publically-visible functions.
     * Typically, these will include the initialization and shutdown functions,
     *  the main request callback routine and any writeable object methods.
     *
     * Function prototypes are provided for the callback routine ('FindVarMethod')
     *  and writeable object methods ('WriteMethod').
     */
     void     init_example(void);
     FindVarMethod var_example;
     WriteMethod write_exampleint;
     WriteMethod write_exampletrap;
     WriteMethod write_exampletrap2;


    /*
     * Magic number definitions.
     * These must be unique for each object implemented within a
     *  single mib module callback routine.
     *
     * Typically, these will be the last OID sub-component for
     *  each entry, or integers incrementing from 1.
     *  (which may well result in the same values anyway).
     *
     * Here, the second and third objects are form a 'sub-table' and
     *   the magic numbers are chosen to match these OID sub-components.
     * This is purely for programmer convenience.
     * All that really matters is that the numbers are unique.
     */

#define	EXAMPLESTRING		1
#define EXAMPLEINTEGER		21
#define	EXAMPLEOBJECTID         22
#define EXAMPLETIMETICKS	3
#define	EXAMPLEIPADDRESS        4
#define EXAMPLECOUNTER		5
#define	EXAMPLEGAUGE            6
#define	EXAMPLETRIGGERTRAP      7
#define	EXAMPLETRIGGERTRAP2     8

#ifdef __cplusplus
}
#endif

#endif                          /* _MIBGROUP_EXAMPLE_H */
