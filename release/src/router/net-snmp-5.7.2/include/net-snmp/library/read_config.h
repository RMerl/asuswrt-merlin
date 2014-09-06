/*
 *  read_config.h: reads configuration files for extensible sections.
 *
 */
#ifndef READ_CONFIG_H
#define READ_CONFIG_H

#ifdef __cplusplus
extern          "C" {
#endif

#define STRINGMAX 1024

#define NORMAL_CONFIG 0
#define PREMIB_CONFIG 1
#define EITHER_CONFIG 2

#include <net-snmp/config_api.h>

    /*
     * Defines a set of file types and the parse and free functions
     * which process the syntax following a given token in a given file.
     */
    struct config_files {
        char           *fileHeader;     /* Label for entire file. */
        struct config_line *start;
        struct config_files *next;
    };

    struct config_line {
        char           *config_token;   /* Label for each line parser
                                         * in the given file. */
        void            (*parse_line) (const char *, char *);
        void            (*free_func) (void);
        struct config_line *next;
        char            config_time;    /* {NORMAL,PREMIB,EITHER}_CONFIG */
        char           *help;
    };

    struct read_config_memory {
        char           *line;
        struct read_config_memory *next;
    };


    NETSNMP_IMPORT
    int             netsnmp_config(char *);     /* parse a simple line: token=values */
    NETSNMP_IMPORT
    void            netsnmp_config_remember(char *);    /* process later, during snmp_init() */
    void            netsnmp_config_process_memories(void);      /* run all memories through parser */
    int             read_config(const char *, struct config_line *, int);
    int             read_config_files(int);
    NETSNMP_IMPORT
    void            free_config(void);
#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2&& __GNUC_MINOR__ < 8)
    NETSNMP_IMPORT
    void            netsnmp_config_error(const char *, ...);
    void            netsnmp_config_warn(const char *, ...);
#else
    NETSNMP_IMPORT
    void            netsnmp_config_error(const char *, ...)
	__attribute__((__format__(__printf__, 1, 2)));
    void            netsnmp_config_warn(const char *, ...)
	__attribute__((__format__(__printf__, 1, 2)));
#endif

    NETSNMP_IMPORT
    char           *skip_white(char *);
    const char     *skip_white_const(const char *);
    NETSNMP_IMPORT
    char           *skip_not_white(char *);
    const char     *skip_not_white_const(const char *);
    NETSNMP_IMPORT
    char           *skip_token(char *);
    NETSNMP_IMPORT
    const char     *skip_token_const(const char *);
    NETSNMP_IMPORT
    char           *copy_nword(char *, char *, int);
    NETSNMP_IMPORT
    const char     *copy_nword_const(const char *, char *, int);
    NETSNMP_IMPORT
    char           *copy_word(char *, char *);  /* do not use */
    NETSNMP_IMPORT
    int             read_config_with_type(const char *, const char *);
    NETSNMP_IMPORT
    char           *read_config_save_octet_string(char *saveto,
                                                  u_char * str,
                                                  size_t len);
    NETSNMP_IMPORT
    char           *read_config_read_octet_string(const char *readfrom,
                                                  u_char ** str,
                                                  size_t * len);
    const char     *read_config_read_octet_string_const(const char *readfrom,
                                                        u_char ** str,
                                                        size_t * len);
    NETSNMP_IMPORT
    char           *read_config_read_objid(char *readfrom, oid ** objid,
                                           size_t * len);
    const char     *read_config_read_objid_const(const char *readfrom,
                                                 oid ** objid,
                                                 size_t * len);
    NETSNMP_IMPORT
    char           *read_config_save_objid(char *saveto, oid * objid,
                                           size_t len);
    NETSNMP_IMPORT
    char           *read_config_read_data(int type, char *readfrom,
                                          void *dataptr, size_t * len);
    NETSNMP_IMPORT
    char           *read_config_read_memory(int type, char *readfrom,
                                            char *dataptr, size_t * len);
    NETSNMP_IMPORT
    char           *read_config_store_data(int type, char *storeto,
                                           void *dataptr, size_t * len);
    char           *read_config_store_data_prefix(char prefix, int type,
                                                  char *storeto,
                                                  void *dataptr, size_t len);
    int  read_config_files_of_type(int when, struct config_files *ctmp);
    NETSNMP_IMPORT
    void            read_config_store(const char *type, const char *line);
    NETSNMP_IMPORT
    void            read_app_config_store(const char *line);
    NETSNMP_IMPORT
    void            snmp_save_persistent(const char *type);
    NETSNMP_IMPORT
    void            snmp_clean_persistent(const char *type);
    struct config_line *read_config_get_handlers(const char *type);

    /*
     * external memory list handlers 
     */
    void            snmp_config_remember_in_list(char *line,
                                                 struct read_config_memory
                                                 **mem);
    void            snmp_config_process_memory_list(struct
                                                    read_config_memory
                                                    **mem, int, int);
    void            snmp_config_remember_free_list(struct
                                                   read_config_memory
                                                   **mem);

    void            set_configuration_directory(const char *dir);
    NETSNMP_IMPORT
    const char     *get_configuration_directory(void);
    void            set_persistent_directory(const char *dir);
    const char     *get_persistent_directory(void);
    void            set_temp_file_pattern(const char *pattern);
    NETSNMP_IMPORT
    const char     *get_temp_file_pattern(void);
    NETSNMP_IMPORT
    void            handle_long_opt(const char *myoptarg);


#ifdef __cplusplus
}
#endif
#endif                          /* READ_CONFIG_H */
