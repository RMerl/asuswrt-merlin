#ifndef NETSNMP_TEXT_UTILS_H
#define NETSNMP_TEXT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

    
    /*------------------------------------------------------------------
     *
     * text file processing
     *
     */
    netsnmp_container *
    netsnmp_file_text_parse(netsnmp_file *f, netsnmp_container *cin,
                            int parse_mode, u_int flags, void *context);

#define PM_SAVE_EVERYTHING                                             1
#define PM_INDEX_STRING_STRING                                         2
#define PM_USER_FUNCTION                                               3

#define PM_FLAG_NO_CONTAINER                                  0x00000001
#define PM_FLAG_SKIP_WHITESPACE                               0x00000002


    /*
     * line processing user function
     */
    struct netsnmp_line_process_info_s; /* fwd decl */

    typedef struct netsnmp_line_info_s {

        size_t                     index;

        char                      *line;
        size_t                     line_len;
        size_t                     line_max;

        char                      *start;
        size_t                     start_len;

    } netsnmp_line_info;

    typedef int (Netsnmp_Process_Text_Line)
        (netsnmp_line_info *line_info, void *mem,
         struct netsnmp_line_process_info_s* lpi);

    typedef struct netsnmp_line_process_info_s {

        size_t                     line_max; /* defaults to STRINGMAX if 0 */
        size_t                     mem_size;

        u_int                      flags;

        Netsnmp_Process_Text_Line *process;

        void                      *user_context;
        
    } netsnmp_line_process_info;
    
/*
 * user function return codes
 */
#define PMLP_RC_STOP_PROCESSING                           -1
#define PMLP_RC_MEMORY_USED                                0
#define PMLP_RC_MEMORY_UNUSED                              1

    
/** ALLOC_LINE: wasteful, but fast */
#define PMLP_FLAG_ALLOC_LINE                               0x00000001
/** STRDUP_LINE: slower if you don't keep memory in most cases */
#define PMLP_FLAG_STRDUP_LINE                              0x00000002
/** don't strip trailing newlines */
#define PMLP_FLAG_LEAVE_NEWLINE                            0x00000004
/** don't skip blank or comment lines */
#define PMLP_FLAG_PROCESS_WHITESPACE                       0x00000008
/** just process line, don't save it */
#define PMLP_FLAG_NO_CONTAINER                             0x00000010
    

    /*
     * a few useful pre-defined helpers
     */

    typedef struct netsnmp_token_value_index_s {

        char               *token;
        netsnmp_cvalue      value;
        size_t              index;

    } netsnmp_token_value_index;

    netsnmp_container *netsnmp_text_token_container_from_file(const char *file,
                                                              u_int flags,
                                                              netsnmp_container *c,
                                                              void *context);
/*
 * flags
 */
#define NSTTC_FLAG_TYPE_CONTEXT_DIRECT                      0x00000001


#define PMLP_TYPE_UNSIGNED                                  1
#define PMLP_TYPE_INTEGER                                   2
#define PMLP_TYPE_STRING                                    3
#define PMLP_TYPE_BOOLEAN                                   4

        
#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_TEXT_UTILS_H */
