#ifndef NETSNMP_FILE_UTILS_H
#define NETSNMP_FILE_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

    
    /*------------------------------------------------------------------
     *
     * structures
     *
     */
    typedef struct netsnmp_file_s {
        
        /** file name */
        char                   *name;
        
        /** file descriptor for the file */
        int                     fd;

        /** filesystem flags */
        int                     fs_flags;

        /** open/create mode */
        mode_t                  mode;

        /** netsnmp flags */
        u_int                   ns_flags;

        /** file stats */
        struct stat            *stats;

        /*
         * future expansion
         */
        netsnmp_data_list      *extras;

    } netsnmp_file;


    
    /*------------------------------------------------------------------
     *
     * Prototypes
     *
     */
    netsnmp_file *netsnmp_file_new(const char *name, int fs_flags, mode_t mode,
                                   u_int ns_flags);

    netsnmp_file * netsnmp_file_create(void);
    netsnmp_file * netsnmp_file_fill(netsnmp_file * filei, const char* name,
                                     int fs_flags, mode_t mode, u_int ns_flags);
    int netsnmp_file_release(netsnmp_file * filei);

    int netsnmp_file_open(netsnmp_file * filei);
    int netsnmp_file_close(netsnmp_file * filei);

    /** support netsnmp_file containers */
    int netsnmp_file_compare_name(netsnmp_file *lhs, netsnmp_file *rhs);
    void netsnmp_file_container_free(netsnmp_file *file, void *context);



    /*------------------------------------------------------------------
     *
     * flags
     *
     */
#define NETSNMP_FILE_NO_AUTOCLOSE                         0x00000001
#define NETSNMP_FILE_STATS                                0x00000002
#define NETSNMP_FILE_AUTO_OPEN                            0x00000004

    /*------------------------------------------------------------------
     *
     * macros
     *
     */
#define NS_FI_AUTOCLOSE(x) (0 == (x & NETSNMP_FILE_NO_AUTOCLOSE))
#define NS_FI_(x) (0 == (x & NETSNMP_FILE_))

    
        
#ifdef __cplusplus
}
#endif

#endif /* NETSNMP_FILE_UTILS_H */
