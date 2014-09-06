/*
 * read_config.c
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/** @defgroup read_config parsing various configuration files at run time
 *  @ingroup library
 *
 * The read_config related functions are a fairly extensible  system  of
 * parsing various configuration files at the run time.
 *
 * The idea is that the calling application is able to register
 * handlers for certain tokens specified in certain types
 * of files.  The read_configs function can then be  called
 * to  look  for all the files that it has registrations for,
 * find the first word on each line, and pass  the  remainder
 * to the appropriately registered handler.
 *
 * For persistent configuration storage you will need to use the
 * read_config_read_data, read_config_store, and read_config_store_data
 * APIs in conjunction with first registering a
 * callback so when the agent shutsdown for whatever reason data is written
 * to your configuration files.  The following explains in more detail the
 * sequence to make this happen.
 *
 * This is the callback registration API, you need to call this API with
 * the appropriate parameters in order to configure persistent storage needs.
 *
 *        int snmp_register_callback(int major, int minor,
 *                                   SNMPCallback *new_callback,
 *                                   void *arg);
 *
 * You will need to set major to SNMP_CALLBACK_LIBRARY, minor to
 * SNMP_CALLBACK_STORE_DATA. arg is whatever you want.
 *
 * Your callback function's prototype is:
 * int     (SNMPCallback) (int majorID, int minorID, void *serverarg,
 *                        void *clientarg);
 *
 * The majorID, minorID and clientarg are what you passed in the callback
 * registration above.  When the callback is called you have to essentially
 * transfer all your state from memory to disk. You do this by generating
 * configuration lines into a buffer.  The lines are of the form token
 * followed by token parameters.
 * 
 * Finally storing is done using read_config_store(type, buffer);
 * type is the application name this can be obtained from:
 *
 * netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_APPTYPE);
 *
 * Now, reading back the data: This is done by registering a config handler
 * for your token using the register_config_handler function. Your
 * handler will be invoked and you can parse in the data using the
 * read_config_read APIs.
 *
 *  @{
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <stdio.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#include <errno.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/library/read_config.h>       /* for "internal" definitions */
#include <net-snmp/utilities.h>

#include <net-snmp/library/mib.h>
#include <net-snmp/library/parse.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/callback.h>

netsnmp_feature_child_of(read_config_all, libnetsnmp)

netsnmp_feature_child_of(unregister_app_config_handler, read_config_all)
netsnmp_feature_child_of(read_config_register_app_prenetsnmp_mib_handler, netsnmp_unused)
netsnmp_feature_child_of(read_config_register_const_config_handler, netsnmp_unused)

static int      config_errors;

struct config_files *config_files = NULL;


static struct config_line *
internal_register_config_handler(const char *type_param,
				 const char *token,
				 void (*parser) (const char *, char *),
				 void (*releaser) (void), const char *help,
				 int when)
{
    struct config_files **ctmp = &config_files;
    struct config_line  **ltmp;
    const char           *type = type_param;

    if (type == NULL || *type == '\0') {
        type = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				     NETSNMP_DS_LIB_APPTYPE);
    }

    /*
     * Handle multiple types (recursively)
     */
    if (strchr(type, ':')) {
        struct config_line *ltmp2 = NULL;
        char                buf[STRINGMAX];
        char               *cptr = buf;

        strlcpy(buf, type, STRINGMAX);
        while (cptr) {
            char* c = cptr;
            cptr = strchr(cptr, ':');
            if(cptr) {
                *cptr = '\0';
                ++cptr;
            }
            ltmp2 = internal_register_config_handler(c, token, parser,
                                                     releaser, help, when);
        }
        return ltmp2;
    }
    
    /*
     * Find type in current list  -OR-  create a new file type.
     */
    while (*ctmp != NULL && strcmp((*ctmp)->fileHeader, type)) {
        ctmp = &((*ctmp)->next);
    }

    if (*ctmp == NULL) {
        *ctmp = (struct config_files *)
            calloc(1, sizeof(struct config_files));
        if (!*ctmp) {
            return NULL;
        }

        (*ctmp)->fileHeader = strdup(type);
        DEBUGMSGTL(("9:read_config:type", "new type %s\n", type));
    }

    DEBUGMSGTL(("9:read_config:register_handler", "registering %s %s\n",
                type, token));
    /*
     * Find parser type in current list  -OR-  create a new
     * line parser entry.
     */
    ltmp = &((*ctmp)->start);

    while (*ltmp != NULL && strcmp((*ltmp)->config_token, token)) {
        ltmp = &((*ltmp)->next);
    }

    if (*ltmp == NULL) {
        *ltmp = (struct config_line *)
            calloc(1, sizeof(struct config_line));
        if (!*ltmp) {
            return NULL;
        }

        (*ltmp)->config_time = when;
        (*ltmp)->config_token = strdup(token);
        if (help != NULL)
            (*ltmp)->help = strdup(help);
    }

    /*
     * Add/Replace the parse/free functions for the given line type
     * in the given file type.
     */
    (*ltmp)->parse_line = parser;
    (*ltmp)->free_func = releaser;

    return (*ltmp);

}                               /* end register_config_handler() */

struct config_line *
register_prenetsnmp_mib_handler(const char *type,
                                const char *token,
                                void (*parser) (const char *, char *),
                                void (*releaser) (void), const char *help)
{
    return internal_register_config_handler(type, token, parser, releaser,
					    help, PREMIB_CONFIG);
}

#ifndef NETSNMP_FEATURE_REMOVE_READ_CONFIG_REGISTER_APP_PRENETSNMP_MIB_HANDLER
struct config_line *
register_app_prenetsnmp_mib_handler(const char *token,
                                    void (*parser) (const char *, char *),
                                    void (*releaser) (void),
                                    const char *help)
{
    return (register_prenetsnmp_mib_handler
            (NULL, token, parser, releaser, help));
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_CONFIG_REGISTER_APP_PRENETSNMP_MIB_HANDLER */

/**
 * register_config_handler registers handlers for certain tokens specified in
 * certain types of files.
 *
 * Allows a module writer use/register multiple configuration files based off
 * of the type parameter.  A module writer may want to set up multiple
 * configuration files to separate out related tasks/variables or just for
 * management of where to put tokens as the module or modules get more complex
 * in regard to handling token registrations.
 *
 * @param type     the configuration file used, e.g., if snmp.conf is the
 *                 file where the token is located use "snmp" here.
 *                 Multiple colon separated tokens might be used.
 *                 If NULL or "" then the configuration file used will be
 *                 \<application\>.conf.
 *
 * @param token    the token being parsed from the file.  Must be non-NULL.
 *
 * @param parser   the handler function pointer that use  the specified
 *                 token and the rest of the line to do whatever is required
 *                 Should be non-NULL in order to make use of this API.
 *
 * @param releaser if non-NULL, the function specified is called when
 *                 unregistering config handler or when configuration
 *                 files are re-read.
 *                 This function should free any resources allocated by
 *                 the token handler function.
 *
 * @param help     if non-NULL, used to display help information on the
 *                 expected arguments after the token.
 *
 * @return Pointer to a new config line entry or NULL on error.
 */
struct config_line *
register_config_handler(const char *type,
			const char *token,
			void (*parser) (const char *, char *),
			void (*releaser) (void), const char *help)
{
    return internal_register_config_handler(type, token, parser, releaser,
					    help, NORMAL_CONFIG);
}

#ifndef NETSNMP_FEATURE_REMOVE_READ_CONFIG_REGISTER_CONST_CONFIG_HANDLER
struct config_line *
register_const_config_handler(const char *type,
                              const char *token,
                              void (*parser) (const char *, const char *),
                              void (*releaser) (void), const char *help)
{
    return internal_register_config_handler(type, token,
                                            (void(*)(const char *, char *))
                                            parser, releaser,
					    help, NORMAL_CONFIG);
}
#endif /* NETSNMP_FEATURE_REMOVE_READ_CONFIG_REGISTER_CONST_CONFIG_HANDLER */

struct config_line *
register_app_config_handler(const char *token,
                            void (*parser) (const char *, char *),
                            void (*releaser) (void), const char *help)
{
    return (register_config_handler(NULL, token, parser, releaser, help));
}



/**
 * uregister_config_handler un-registers handlers given a specific type_param
 * and token.
 *
 * @param type_param the configuration file used where the token is located.
 *                   Used to lookup the config file entry
 * 
 * @param token      the token that is being unregistered
 *
 * @return void
 */
void
unregister_config_handler(const char *type_param, const char *token)
{
    struct config_files **ctmp = &config_files;
    struct config_line  **ltmp;
    const char           *type = type_param;

    if (type == NULL || *type == '\0') {
        type = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				     NETSNMP_DS_LIB_APPTYPE);
    }

    /*
     * Handle multiple types (recursively)
     */
    if (strchr(type, ':')) {
        char                buf[STRINGMAX];
        char               *cptr = buf;

        strlcpy(buf, type, STRINGMAX);
        while (cptr) {
            char* c = cptr;
            cptr = strchr(cptr, ':');
            if(cptr) {
                *cptr = '\0';
                ++cptr;
            }
            unregister_config_handler(c, token);
        }
        return;
    }
    
    /*
     * find type in current list 
     */
    while (*ctmp != NULL && strcmp((*ctmp)->fileHeader, type)) {
        ctmp = &((*ctmp)->next);
    }

    if (*ctmp == NULL) {
        /*
         * Not found, return. 
         */
        return;
    }

    ltmp = &((*ctmp)->start);
    if (*ltmp == NULL) {
        /*
         * Not found, return. 
         */
        return;
    }
    if (strcmp((*ltmp)->config_token, token) == 0) {
        /*
         * found it at the top of the list 
         */
        struct config_line *ltmp2 = (*ltmp)->next;
        if ((*ltmp)->free_func)
            (*ltmp)->free_func();
        SNMP_FREE((*ltmp)->config_token);
        SNMP_FREE((*ltmp)->help);
        SNMP_FREE(*ltmp);
        (*ctmp)->start = ltmp2;
        return;
    }
    while ((*ltmp)->next != NULL
           && strcmp((*ltmp)->next->config_token, token)) {
        ltmp = &((*ltmp)->next);
    }
    if ((*ltmp)->next != NULL) {
        struct config_line *ltmp2 = (*ltmp)->next->next;
        if ((*ltmp)->next->free_func)
            (*ltmp)->next->free_func();
        SNMP_FREE((*ltmp)->next->config_token);
        SNMP_FREE((*ltmp)->next->help);
        SNMP_FREE((*ltmp)->next);
        (*ltmp)->next = ltmp2;
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_UNREGISTER_APP_CONFIG_HANDLER
void
unregister_app_config_handler(const char *token)
{
    unregister_config_handler(NULL, token);
}
#endif /* NETSNMP_FEATURE_REMOVE_UNREGISTER_APP_CONFIG_HANDLER */

void
unregister_all_config_handlers(void)
{
    struct config_files *ctmp, *save;
    struct config_line *ltmp;

    /*
     * Keep using config_files until there are no more! 
     */
    for (ctmp = config_files; ctmp;) {
        for (ltmp = ctmp->start; ltmp; ltmp = ctmp->start) {
            unregister_config_handler(ctmp->fileHeader,
                                      ltmp->config_token);
        }
        SNMP_FREE(ctmp->fileHeader);
        save = ctmp->next;
        SNMP_FREE(ctmp);
        ctmp = save;
        config_files = save;
    }
}

#ifdef TESTING
void
print_config_handlers(void)
{
    struct config_files *ctmp = config_files;
    struct config_line *ltmp;

    for (; ctmp != NULL; ctmp = ctmp->next) {
        DEBUGMSGTL(("read_config", "read_conf: %s\n", ctmp->fileHeader));
        for (ltmp = ctmp->start; ltmp != NULL; ltmp = ltmp->next)
            DEBUGMSGTL(("read_config", "                   %s\n",
                        ltmp->config_token));
    }
}
#endif

static unsigned int  linecount;
static const char   *curfilename;

struct config_line *
read_config_get_handlers(const char *type)
{
    struct config_files *ctmp = config_files;
    for (; ctmp != NULL && strcmp(ctmp->fileHeader, type);
         ctmp = ctmp->next);
    if (ctmp)
        return ctmp->start;
    return NULL;
}

int
read_config_with_type_when(const char *filename, const char *type, int when)
{
    struct config_line *ctmp = read_config_get_handlers(type);
    if (ctmp)
        return read_config(filename, ctmp, when);
    else
        DEBUGMSGTL(("read_config",
                    "read_config: I have no registrations for type:%s,file:%s\n",
                    type, filename));
    return SNMPERR_GENERR;     /* No config files read */
}

int
read_config_with_type(const char *filename, const char *type)
{
    return read_config_with_type_when(filename, type, EITHER_CONFIG);
}


struct config_line *
read_config_find_handler(struct config_line *line_handlers,
                         const char *token)
{
    struct config_line *lptr;

    for (lptr = line_handlers; lptr != NULL; lptr = lptr->next) {
        if (!strcasecmp(token, lptr->config_token)) {
            return lptr;
        }
    }
    return NULL;
}


/*
 * searches a config_line linked list for a match 
 */
int
run_config_handler(struct config_line *lptr,
                   const char *token, char *cptr, int when)
{
    char           *cp;
    lptr = read_config_find_handler(lptr, token);
    if (lptr != NULL) {
        if (when == EITHER_CONFIG || lptr->config_time == when) {
            DEBUGMSGTL(("read_config:parser",
                        "Found a parser.  Calling it: %s / %s\n", token,
                        cptr));
            /*
             * Make sure cptr is non-null
             */
            char tmpbuf[1];
            if (!cptr) {
                tmpbuf[0] = '\0';
                cptr = tmpbuf;
            }

            /*
             * Stomp on any trailing whitespace
             */
            cp = &(cptr[strlen(cptr)-1]);
            while ((cp > cptr) && isspace((unsigned char)(*cp))) {
                *(cp--) = '\0';
            }
            (*(lptr->parse_line)) (token, cptr);
        }
        else
            DEBUGMSGTL(("9:read_config:parser",
                        "%s handler not registered for this time\n", token));
    } else if (when != PREMIB_CONFIG && 
	       !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				       NETSNMP_DS_LIB_NO_TOKEN_WARNINGS)) {
	netsnmp_config_warn("Unknown token: %s.", token);
        return SNMPERR_GENERR;
    }
    return SNMPERR_SUCCESS;
}

/*
 * takens an arbitrary string and tries to intepret it based on the
 * known configuration handlers for all registered types.  May produce
 * inconsistent results when multiple tokens of the same name are
 * registered under different file types. 
 */

/*
 * we allow = delimeters here 
 */
#define SNMP_CONFIG_DELIMETERS " \t="

int
snmp_config_when(char *line, int when)
{
    char           *cptr, buf[STRINGMAX];
    struct config_line *lptr = NULL;
    struct config_files *ctmp = config_files;
    char           *st;

    if (line == NULL) {
        config_perror("snmp_config() called with a null string.");
        return SNMPERR_GENERR;
    }

    strlcpy(buf, line, STRINGMAX);
    cptr = strtok_r(buf, SNMP_CONFIG_DELIMETERS, &st);
    if (!cptr) {
        netsnmp_config_warn("Wrong format: %s", line);
        return SNMPERR_GENERR;
    }
    if (cptr[0] == '[') {
        if (cptr[strlen(cptr) - 1] != ']') {
	    netsnmp_config_error("no matching ']' for type %s.", cptr + 1);
            return SNMPERR_GENERR;
        }
        cptr[strlen(cptr) - 1] = '\0';
        lptr = read_config_get_handlers(cptr + 1);
        if (lptr == NULL) {
	    netsnmp_config_error("No handlers regestered for type %s.",
				 cptr + 1);
            return SNMPERR_GENERR;
        }
        cptr = strtok_r(NULL, SNMP_CONFIG_DELIMETERS, &st);
        lptr = read_config_find_handler(lptr, cptr);
    } else {
        /*
         * we have to find a token 
         */
        for (; ctmp != NULL && lptr == NULL; ctmp = ctmp->next)
            lptr = read_config_find_handler(ctmp->start, cptr);
    }
    if (lptr == NULL && netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
					  NETSNMP_DS_LIB_NO_TOKEN_WARNINGS)) {
	netsnmp_config_warn("Unknown token: %s.", cptr);
        return SNMPERR_GENERR;
    }

    /*
     * use the original string instead since strtok_r messed up the original 
     */
    line = skip_white(line + (cptr - buf) + strlen(cptr) + 1);

    return (run_config_handler(lptr, cptr, line, when));
}

int
netsnmp_config(char *line)
{
    int             ret = SNMP_ERR_NOERROR;
    DEBUGMSGTL(("snmp_config", "remembering line \"%s\"\n", line));
    netsnmp_config_remember(line);      /* always remember it so it's read
                                         * processed after a free_config()
                                         * call */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
			       NETSNMP_DS_LIB_HAVE_READ_CONFIG)) {
        DEBUGMSGTL(("snmp_config", "  ... processing it now\n"));
        ret = snmp_config_when(line, NORMAL_CONFIG);
    }
    return ret;
}

void
netsnmp_config_remember_in_list(char *line,
                                struct read_config_memory **mem)
{
    if (mem == NULL)
        return;

    while (*mem != NULL)
        mem = &((*mem)->next);

    *mem = SNMP_MALLOC_STRUCT(read_config_memory);
    if (*mem != NULL) {
        if (line)
            (*mem)->line = strdup(line);
    }
}

void
netsnmp_config_remember_free_list(struct read_config_memory **mem)
{
    struct read_config_memory *tmpmem;
    while (*mem) {
        SNMP_FREE((*mem)->line);
        tmpmem = (*mem)->next;
        SNMP_FREE(*mem);
        *mem = tmpmem;
    }
}

void
netsnmp_config_process_memory_list(struct read_config_memory **memp,
                                   int when, int clear)
{

    struct read_config_memory *mem;

    if (!memp)
        return;

    mem = *memp;

    while (mem) {
        DEBUGMSGTL(("read_config:mem", "processing memory: %s\n", mem->line));
        snmp_config_when(mem->line, when);
        mem = mem->next;
    }

    if (clear)
        netsnmp_config_remember_free_list(memp);
}

/*
 * default storage location implementation 
 */
static struct read_config_memory *memorylist = NULL;

void
netsnmp_config_remember(char *line)
{
    netsnmp_config_remember_in_list(line, &memorylist);
}

void
netsnmp_config_process_memories(void)
{
    netsnmp_config_process_memory_list(&memorylist, EITHER_CONFIG, 1);
}

void
netsnmp_config_process_memories_when(int when, int clear)
{
    netsnmp_config_process_memory_list(&memorylist, when, clear);
}

/*******************************************************************-o-******
 * read_config
 *
 * Parameters:
 *	*filename
 *	*line_handler
 *	 when
 *
 * Read <filename> and process each line in accordance with the list of
 * <line_handler> functions.
 *
 *
 * For each line in <filename>, search the list of <line_handler>'s 
 * for an entry that matches the first token on the line.  This comparison is
 * case insensitive.
 *
 * For each match, check that <when> is the designated time for the
 * <line_handler> function to be executed before processing the line.
 *
 * Returns SNMPERR_SUCCESS if the file is processed successfully.
 * Returns SNMPERR_GENERR  if it cannot.
 *    Note that individual config token errors do not trigger SNMPERR_GENERR
 *    It's only if the whole file cannot be processed for some reason.
 */
int
read_config(const char *filename,
            struct config_line *line_handler, int when)
{
    static int      depth = 0;
    static int      files = 0;

    const char * const prev_filename = curfilename;
    const unsigned int prev_linecount = linecount;

    FILE           *ifile;
    char           *line = NULL;  /* current line buffer */
    size_t          linesize = 0; /* allocated size of line */

    /* reset file counter when recursion depth is 0 */
    if (depth == 0)
        files = 0;

    if ((ifile = fopen(filename, "r")) == NULL) {
#ifdef ENOENT
        if (errno == ENOENT) {
            DEBUGMSGTL(("read_config", "%s: %s\n", filename,
                        strerror(errno)));
        } else
#endif                          /* ENOENT */
#ifdef EACCES
        if (errno == EACCES) {
            DEBUGMSGTL(("read_config", "%s: %s\n", filename,
                        strerror(errno)));
        } else
#endif                          /* EACCES */
        {
            snmp_log_perror(filename);
        }
        return SNMPERR_GENERR;
    }

#define CONFIG_MAX_FILES 4096
    if (files > CONFIG_MAX_FILES) {
        netsnmp_config_error("maximum conf file count (%d) exceeded\n",
                             CONFIG_MAX_FILES);
	fclose(ifile);
        return SNMPERR_GENERR;
    }
#define CONFIG_MAX_RECURSE_DEPTH 16
    if (depth > CONFIG_MAX_RECURSE_DEPTH) {
        netsnmp_config_error("nested include depth > %d\n",
                             CONFIG_MAX_RECURSE_DEPTH);
	fclose(ifile);
        return SNMPERR_GENERR;
    }

    linecount = 0;
    curfilename = filename;

    ++files;
    ++depth;

    DEBUGMSGTL(("read_config:file", "Reading configuration %s (%d)\n",
                filename, when));

    while (ifile) {
        size_t              linelen = 0; /* strlen of the current line */
        char               *cptr;
        struct config_line *lptr = line_handler;

        for (;;) {
            if (linesize <= linelen + 1) {
                char *tmp = realloc(line, linesize + 256);
                if (tmp) {
                    line = tmp;
                    linesize += 256;
                } else {
                    netsnmp_config_error("Failed to allocate memory\n");
                    free(line);
                    fclose(ifile);
                    return SNMPERR_GENERR;
                }
            }
            if (fgets(line + linelen, linesize - linelen, ifile) == NULL) {
                line[linelen] = '\0';
                fclose (ifile);
                ifile = NULL;
                break;
            }

            linelen += strlen(line + linelen);

            if (line[linelen - 1] == '\n') {
              line[linelen - 1] = '\0';
              break;
            }
        }

        ++linecount;
        DEBUGMSGTL(("9:read_config:line", "%s:%d examining: %s\n",
                    filename, linecount, line));
        /*
         * check blank line or # comment 
         */
        if ((cptr = skip_white(line))) {
            char token[STRINGMAX];

            cptr = copy_nword(cptr, token, sizeof(token));
            if (token[0] == '[') {
                if (token[strlen(token) - 1] != ']') {
		    netsnmp_config_error("no matching ']' for type %s.",
					 &token[1]);
                    continue;
                }
                token[strlen(token) - 1] = '\0';
                lptr = read_config_get_handlers(&token[1]);
                if (lptr == NULL) {
		    netsnmp_config_error("No handlers regestered for type %s.",
					 &token[1]);
                    continue;
                }
                DEBUGMSGTL(("read_config:context",
                            "Switching to new context: %s%s\n",
                            ((cptr) ? "(this line only) " : ""),
                            &token[1]));
                if (cptr == NULL) {
                    /*
                     * change context permanently 
                     */
                    line_handler = lptr;
                    continue;
                } else {
                    /*
                     * the rest of this line only applies. 
                     */
                    cptr = copy_nword(cptr, token, sizeof(token));
                }
            } else if ((token[0] == 'i') && (strncasecmp(token,"include", 7 )==0)) {
                if ( strcasecmp( token, "include" )==0) {
                    if (when != PREMIB_CONFIG && 
	                !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				                NETSNMP_DS_LIB_NO_TOKEN_WARNINGS)) {
	                netsnmp_config_warn("Ambiguous token '%s' - use 'includeSearch' (or 'includeFile') instead.", token);
                    }
                    continue;
                } else if ( strcasecmp( token, "includedir" )==0) {
                    DIR *d;
                    struct dirent *entry;
                    char  fname[SNMP_MAXPATH];
                    int   len;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
		            netsnmp_config_error("Blank line following %s token.", token);
                        continue;
                    }
                    if ((d=opendir(cptr)) == NULL ) {
                        if (when != PREMIB_CONFIG)
                            netsnmp_config_error("Can't open include dir '%s'.", cptr);
                        continue;
                    }
                    while ((entry = readdir( d )) != NULL ) {
                        if ( entry->d_name && entry->d_name[0] != '.') {
                            len = NAMLEN(entry);
                            if ((len > 5) && (strcmp(&(entry->d_name[len-5]),".conf") == 0)) {
                                snprintf(fname, SNMP_MAXPATH, "%s/%s",
                                         cptr, entry->d_name);
                                (void)read_config(fname, line_handler, when);
                            }
                        }
                    }
                    closedir(d);
                    continue;
                } else if ( strcasecmp( token, "includefile" )==0) {
                    char  fname[SNMP_MAXPATH], *cp;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
		            netsnmp_config_error("Blank line following %s token.", token);
                        continue;
                    }
                    if ( cptr[0] == '/' ) {
                        strlcpy(fname, cptr, SNMP_MAXPATH);
                    } else {
                        strlcpy(fname, filename, SNMP_MAXPATH);
                        cp = strrchr(fname, '/');
                        if (!cp)
                            fname[0] = '\0';
                        else
                            *(++cp) = '\0';
                        strlcat(fname, cptr, SNMP_MAXPATH);
                    }
                    if (read_config(fname, line_handler, when) !=
                        SNMPERR_SUCCESS && when != PREMIB_CONFIG)
                        netsnmp_config_error("Included file '%s' not found.",
                                             fname);
                    continue;
                } else if ( strcasecmp( token, "includesearch" )==0) {
                    struct config_files ctmp;
                    int len, ret;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
		            netsnmp_config_error("Blank line following %s token.", token);
                        continue;
                    }
                    len = strlen(cptr);
                    ctmp.fileHeader = cptr;
                    ctmp.start = line_handler;
                    ctmp.next = NULL;
                    if ((len > 5) && (strcmp(&cptr[len-5],".conf") == 0))
                       cptr[len-5] = 0; /* chop off .conf */
                    ret = read_config_files_of_type(when,&ctmp);
                    if ((len > 5) && (cptr[len-5] == 0))
                       cptr[len-5] = '.'; /* restore .conf */
                    if (( ret != SNMPERR_SUCCESS ) && (when != PREMIB_CONFIG))
		        netsnmp_config_error("Included config '%s' not found.", cptr);
                    continue;
                } else {
                    lptr = line_handler;
                }
            } else {
                lptr = line_handler;
            }
            if (cptr == NULL) {
		netsnmp_config_error("Blank line following %s token.", token);
            } else {
                DEBUGMSGTL(("read_config:line", "%s:%d examining: %s\n",
                            filename, linecount, line));
                run_config_handler(lptr, token, cptr, when);
            }
        }
    }
    free(line);
    linecount = prev_linecount;
    curfilename = prev_filename;
    --depth;
    return SNMPERR_SUCCESS;

}                               /* end read_config() */



void
free_config(void)
{
    struct config_files *ctmp = config_files;
    struct config_line *ltmp;

    for (; ctmp != NULL; ctmp = ctmp->next)
        for (ltmp = ctmp->start; ltmp != NULL; ltmp = ltmp->next)
            if (ltmp->free_func)
                (*(ltmp->free_func)) ();
}

/*
 * Return SNMPERR_SUCCESS if any config files are processed
 * Return SNMPERR_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int
read_configs_optional(const char *optional_config, int when)
{
    char *newp, *cp, *st = NULL;
    int              ret = SNMPERR_GENERR;
    char *type = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				       NETSNMP_DS_LIB_APPTYPE);

    if ((NULL == optional_config) || (NULL == type))
        return ret;

    DEBUGMSGTL(("read_configs_optional",
                "reading optional configuration tokens for %s\n", type));
    
    newp = strdup(optional_config);      /* strtok_r messes it up */
    cp = strtok_r(newp, ",", &st);
    while (cp) {
        struct stat     statbuf;
        if (stat(cp, &statbuf)) {
            DEBUGMSGTL(("read_config",
                        "Optional File \"%s\" does not exist.\n", cp));
            snmp_log_perror(cp);
        } else {
            DEBUGMSGTL(("read_config:opt",
                        "Reading optional config file: \"%s\"\n", cp));
            if ( read_config_with_type_when(cp, type, when) == SNMPERR_SUCCESS )
                ret = SNMPERR_SUCCESS;
        }
        cp = strtok_r(NULL, ",", &st);
    }
    free(newp);
    return ret;
}

void
read_configs(void)
{
    char *optional_config = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					       NETSNMP_DS_LIB_OPTIONALCONFIG);

    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY,
                        SNMP_CALLBACK_PRE_READ_CONFIG, NULL);

    DEBUGMSGTL(("read_config", "reading normal configuration tokens\n"));

    if ((NULL != optional_config) && (*optional_config == '-')) {
        (void)read_configs_optional(++optional_config, NORMAL_CONFIG);
        optional_config = NULL; /* clear, so we don't read them twice */
    }

    (void)read_config_files(NORMAL_CONFIG);

    /*
     * do this even when the normal above wasn't done 
     */
    if (NULL != optional_config)
        (void)read_configs_optional(optional_config, NORMAL_CONFIG);

    netsnmp_config_process_memories_when(NORMAL_CONFIG, 1);

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_HAVE_READ_CONFIG, 1);
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY,
                        SNMP_CALLBACK_POST_READ_CONFIG, NULL);
}

void
read_premib_configs(void)
{
    char *optional_config = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					       NETSNMP_DS_LIB_OPTIONALCONFIG);

    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY,
                        SNMP_CALLBACK_PRE_PREMIB_READ_CONFIG, NULL);

    DEBUGMSGTL(("read_config", "reading premib configuration tokens\n"));

    if ((NULL != optional_config) && (*optional_config == '-')) {
        (void)read_configs_optional(++optional_config, PREMIB_CONFIG);
        optional_config = NULL; /* clear, so we don't read them twice */
    }

    (void)read_config_files(PREMIB_CONFIG);

    if (NULL != optional_config)
        (void)read_configs_optional(optional_config, PREMIB_CONFIG);

    netsnmp_config_process_memories_when(PREMIB_CONFIG, 0);

    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_HAVE_READ_PREMIB_CONFIG, 1);
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY,
                        SNMP_CALLBACK_POST_PREMIB_READ_CONFIG, NULL);
}

/*******************************************************************-o-******
 * set_configuration_directory
 *
 * Parameters:
 *      char *dir - value of the directory
 * Sets the configuration directory. Multiple directories can be
 * specified, but need to be seperated by 'ENV_SEPARATOR_CHAR'.
 */
void
set_configuration_directory(const char *dir)
{
    netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
			  NETSNMP_DS_LIB_CONFIGURATION_DIR, dir);
}

/*******************************************************************-o-******
 * get_configuration_directory
 *
 * Parameters: -
 * Retrieve the configuration directory or directories.
 * (For backwards compatibility that is:
 *       SNMPCONFPATH, SNMPSHAREPATH, SNMPLIBPATH, HOME/.snmp
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value.
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char     *
get_configuration_directory(void)
{
    char            defaultPath[SPRINT_MAX_LEN];
    char           *homepath;

    if (NULL == netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				      NETSNMP_DS_LIB_CONFIGURATION_DIR)) {
        homepath = netsnmp_getenv("HOME");
        snprintf(defaultPath, sizeof(defaultPath), "%s%c%s%c%s%s%s%s",
                SNMPCONFPATH, ENV_SEPARATOR_CHAR,
                SNMPSHAREPATH, ENV_SEPARATOR_CHAR, SNMPLIBPATH,
                ((homepath == NULL) ? "" : ENV_SEPARATOR),
                ((homepath == NULL) ? "" : homepath),
                ((homepath == NULL) ? "" : "/.snmp"));
        defaultPath[ sizeof(defaultPath)-1 ] = 0;
        set_configuration_directory(defaultPath);
    }
    return (netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_CONFIGURATION_DIR));
}

/*******************************************************************-o-******
 * set_persistent_directory
 *
 * Parameters:
 *      char *dir - value of the directory
 * Sets the configuration directory. 
 * No multiple directories may be specified.
 * (However, this is not checked)
 */
void
set_persistent_directory(const char *dir)
{
    netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
			  NETSNMP_DS_LIB_PERSISTENT_DIR, dir);
}

/*******************************************************************-o-******
 * get_persistent_directory
 *
 * Parameters: -
 * Function will retrieve the persisten directory value.
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value. 
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char     *
get_persistent_directory(void)
{
    if (NULL == netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				      NETSNMP_DS_LIB_PERSISTENT_DIR)) {
        const char *persdir = netsnmp_getenv("SNMP_PERSISTENT_DIR");
        if (NULL == persdir)
            persdir = NETSNMP_PERSISTENT_DIRECTORY;
        set_persistent_directory(persdir);
    }
    return (netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_PERSISTENT_DIR));
}

/*******************************************************************-o-******
 * set_temp_file_pattern
 *
 * Parameters:
 *      char *pattern - value of the file pattern
 * Sets the temp file pattern. 
 * Multiple patterns may not be specified.
 * (However, this is not checked)
 */
void
set_temp_file_pattern(const char *pattern)
{
    netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
			  NETSNMP_DS_LIB_TEMP_FILE_PATTERN, pattern);
}

/*******************************************************************-o-******
 * get_temp_file_pattern
 *
 * Parameters: -
 * Function will retrieve the temp file pattern value.
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value. 
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char     *
get_temp_file_pattern(void)
{
    if (NULL == netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				      NETSNMP_DS_LIB_TEMP_FILE_PATTERN)) {
        set_temp_file_pattern(NETSNMP_TEMP_FILE_PATTERN);
    }
    return (netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_TEMP_FILE_PATTERN));
}

/**
 * utility routine for read_config_files
 *
 * Return SNMPERR_SUCCESS if any config files are processed
 * Return SNMPERR_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
static int
read_config_files_in_path(const char *path, struct config_files *ctmp,
                          int when, const char *perspath, const char *persfile)
{
    int             done, j;
    char            configfile[300];
    char           *cptr1, *cptr2, *envconfpath;
    struct stat     statbuf;
    int             ret = SNMPERR_GENERR;

    if ((NULL == path) || (NULL == ctmp))
        return SNMPERR_GENERR;

    envconfpath = strdup(path);

    DEBUGMSGTL(("read_config:path", " config path used for %s:%s (persistent path:%s)\n",
                ctmp->fileHeader, envconfpath, perspath));
    cptr1 = cptr2 = envconfpath;
    done = 0;
    while ((!done) && (*cptr2 != 0)) {
        while (*cptr1 != 0 && *cptr1 != ENV_SEPARATOR_CHAR)
            cptr1++;
        if (*cptr1 == 0)
            done = 1;
        else
            *cptr1 = 0;

        DEBUGMSGTL(("read_config:dir", " config dir: %s\n", cptr2 ));
        if (stat(cptr2, &statbuf) != 0) {
            /*
             * Directory not there, continue 
             */
            DEBUGMSGTL(("read_config:dir", " Directory not present: %s\n", cptr2 ));
            cptr2 = ++cptr1;
            continue;
        }
#ifdef S_ISDIR
        if (!S_ISDIR(statbuf.st_mode)) {
            /*
             * Not a directory, continue 
             */
            DEBUGMSGTL(("read_config:dir", " Not a directory: %s\n", cptr2 ));
            cptr2 = ++cptr1;
            continue;
        }
#endif

        /*
         * for proper persistent storage retrieval, we need to read old backup
         * copies of the previous storage files.  If the application in
         * question has died without the proper call to snmp_clean_persistent,
         * then we read all the configuration files we can, starting with
         * the oldest first.
         */
        if (strncmp(cptr2, perspath, strlen(perspath)) == 0 ||
            (persfile != NULL &&
             strncmp(cptr2, persfile, strlen(persfile)) == 0)) {
            DEBUGMSGTL(("read_config:persist", " persist dir: %s\n", cptr2 ));
            /*
             * limit this to the known storage directory only 
             */
            for (j = 0; j <= NETSNMP_MAX_PERSISTENT_BACKUPS; j++) {
                snprintf(configfile, sizeof(configfile),
                         "%s/%s.%d.conf", cptr2,
                         ctmp->fileHeader, j);
                configfile[ sizeof(configfile)-1 ] = 0;
                if (stat(configfile, &statbuf) != 0) {
                    /*
                     * file not there, continue 
                     */
                    break;
                } else {
                    /*
                     * backup exists, read it 
                     */
                    DEBUGMSGTL(("read_config_files",
                                "old config file found: %s, parsing\n",
                                configfile));
                    if (read_config(configfile, ctmp->start, when) == SNMPERR_SUCCESS)
                        ret = SNMPERR_SUCCESS;
                }
            }
        }
        snprintf(configfile, sizeof(configfile),
                 "%s/%s.conf", cptr2, ctmp->fileHeader);
        configfile[ sizeof(configfile)-1 ] = 0;
        if (read_config(configfile, ctmp->start, when) == SNMPERR_SUCCESS)
            ret = SNMPERR_SUCCESS;
        snprintf(configfile, sizeof(configfile),
                 "%s/%s.local.conf", cptr2, ctmp->fileHeader);
        configfile[ sizeof(configfile)-1 ] = 0;
        if (read_config(configfile, ctmp->start, when) == SNMPERR_SUCCESS)
            ret = SNMPERR_SUCCESS;

        if(done)
            break;

        cptr2 = ++cptr1;
    }
    SNMP_FREE(envconfpath);
    return ret;
}

/*******************************************************************-o-******
 * read_config_files
 *
 * Parameters:
 *	when	== PREMIB_CONFIG, NORMAL_CONFIG  -or-  EITHER_CONFIG
 *
 *
 * Traverse the list of config file types, performing the following actions
 * for each --
 *
 * First, build a search path for config files.  If the contents of 
 * environment variable SNMPCONFPATH are NULL, then use the following
 * path list (where the last entry exists only if HOME is non-null):
 *
 *	SNMPSHAREPATH:SNMPLIBPATH:${HOME}/.snmp
 *
 * Then, In each of these directories, read config files by the name of:
 *
 *	<dir>/<fileHeader>.conf		-AND-
 *	<dir>/<fileHeader>.local.conf
 *
 * where <fileHeader> is taken from the config file type structure.
 *
 *
 * PREMIB_CONFIG causes free_config() to be invoked prior to any other action.
 *
 *
 * EXITs if any 'config_errors' are logged while parsing config file lines.
 *
 * Return SNMPERR_SUCCESS if any config files are processed
 * Return SNMPERR_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int
read_config_files_of_type(int when, struct config_files *ctmp)
{
    const char     *confpath, *persfile, *envconfpath;
    char           *perspath;
    int             ret = SNMPERR_GENERR;

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DONT_PERSIST_STATE)
        || netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                  NETSNMP_DS_LIB_DISABLE_CONFIG_LOAD)
        || (NULL == ctmp)) return ret;

    /*
     * these shouldn't change
     */
    confpath = get_configuration_directory();
    persfile = netsnmp_getenv("SNMP_PERSISTENT_FILE");
    envconfpath = netsnmp_getenv("SNMPCONFPATH");


        /*
         * read the config files. strdup() the result of
         * get_persistent_directory() to avoid that parsing the "persistentDir"
         * keyword transforms the perspath pointer into a dangling pointer.
         */
        perspath = strdup(get_persistent_directory());
        if (envconfpath == NULL) {
            /*
             * read just the config files (no persistent stuff), since
             * persistent path can change via conf file. Then get the
             * current persistent directory, and read files there.
             */
            if ( read_config_files_in_path(confpath, ctmp, when, perspath,
                                      persfile) == SNMPERR_SUCCESS )
                ret = SNMPERR_SUCCESS;
            free(perspath);
            perspath = strdup(get_persistent_directory());
            if ( read_config_files_in_path(perspath, ctmp, when, perspath,
                                      persfile) == SNMPERR_SUCCESS )
                ret = SNMPERR_SUCCESS;
        }
        else {
            /*
             * only read path specified by user
             */
            if ( read_config_files_in_path(envconfpath, ctmp, when, perspath,
                                      persfile) == SNMPERR_SUCCESS )
                ret = SNMPERR_SUCCESS;
        }
        free(perspath);
        return ret;
}

/*
 * Return SNMPERR_SUCCESS if any config files are processed
 * Return SNMPERR_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int
read_config_files(int when) {

    struct config_files *ctmp = config_files;
    int                  ret  = SNMPERR_GENERR;

    config_errors = 0;

    if (when == PREMIB_CONFIG)
        free_config();

    /*
     * read all config file types 
     */
    for (; ctmp != NULL; ctmp = ctmp->next) {
        if ( read_config_files_of_type(when, ctmp) == SNMPERR_SUCCESS )
            ret = SNMPERR_SUCCESS;
    }

    if (config_errors) {
        snmp_log(LOG_ERR, "net-snmp: %d error(s) in config file(s)\n",
                 config_errors);
    }
    return ret;
}

void
read_config_print_usage(const char *lead)
{
    struct config_files *ctmp = config_files;
    struct config_line *ltmp;

    if (lead == NULL)
        lead = "";

    for (ctmp = config_files; ctmp != NULL; ctmp = ctmp->next) {
        snmp_log(LOG_INFO, "%sIn %s.conf and %s.local.conf:\n", lead,
                 ctmp->fileHeader, ctmp->fileHeader);
        for (ltmp = ctmp->start; ltmp != NULL; ltmp = ltmp->next) {
            DEBUGIF("read_config_usage") {
                if (ltmp->config_time == PREMIB_CONFIG)
                    DEBUGMSG(("read_config_usage", "*"));
                else
                    DEBUGMSG(("read_config_usage", " "));
            }
            if (ltmp->help) {
                snmp_log(LOG_INFO, "%s%s%-24s %s\n", lead, lead,
                         ltmp->config_token, ltmp->help);
            } else {
                DEBUGIF("read_config_usage") {
                    snmp_log(LOG_INFO, "%s%s%-24s [NO HELP]\n", lead, lead,
                             ltmp->config_token);
                }
            }
        }
    }
}

/**
 * read_config_store intended for use by applications to store permenant
 * configuration information generated by sets or persistent counters.
 * Appends line to a file named either ENV(SNMP_PERSISTENT_FILE) or
 *   "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf".
 * Adds a trailing newline to the stored file if necessary.
 *
 * @param type is the application name
 * @param line is the configuration line written to the application name's
 * configuration file
 *      
 * @return void
  */
void
read_config_store(const char *type, const char *line)
{
#ifdef NETSNMP_PERSISTENT_DIRECTORY
    char            file[512], *filep;
    FILE           *fout;
#ifdef NETSNMP_PERSISTENT_MASK
    mode_t          oldmask;
#endif

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DONT_PERSIST_STATE)
     || netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DISABLE_PERSISTENT_LOAD)) return;

    /*
     * store configuration directives in the following order of preference:
     * 1. ENV variable SNMP_PERSISTENT_FILE
     * 2. configured <NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf
     */
    if ((filep = netsnmp_getenv("SNMP_PERSISTENT_FILE")) == NULL) {
        snprintf(file, sizeof(file),
                 "%s/%s.conf", get_persistent_directory(), type);
        file[ sizeof(file)-1 ] = 0;
        filep = file;
    }
#ifdef NETSNMP_PERSISTENT_MASK
    oldmask = umask(NETSNMP_PERSISTENT_MASK);
#endif
    if (mkdirhier(filep, NETSNMP_AGENT_DIRECTORY_MODE, 1)) {
        snmp_log(LOG_ERR,
                 "Failed to create the persistent directory for %s\n",
                 file);
    }
    if ((fout = fopen(filep, "a")) != NULL) {
        fprintf(fout, "%s", line);
        if (line[strlen(line)] != '\n')
            fprintf(fout, "\n");
        DEBUGMSGTL(("read_config:store", "storing: %s\n", line));
        fclose(fout);
    } else {
        snmp_log(LOG_ERR, "read_config_store open failure on %s\n", filep);
    }
#ifdef NETSNMP_PERSISTENT_MASK
    umask(oldmask);
#endif

#endif
}                               /* end read_config_store() */

void
read_app_config_store(const char *line)
{
    read_config_store(netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					    NETSNMP_DS_LIB_APPTYPE), line);
}




/*******************************************************************-o-******
 * snmp_save_persistent
 *
 * Parameters:
 *	*type
 *      
 *
 * Save the file "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf" into a backup copy
 * called "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.%d.conf", which %d is an
 * incrementing number on each call, but less than NETSNMP_MAX_PERSISTENT_BACKUPS.
 *
 * Should be called just before all persistent information is supposed to be
 * written to move aside the existing persistent cache.
 * snmp_clean_persistent should then be called afterward all data has been
 * saved to remove these backup files.
 *
 * Note: on an rename error, the files are removed rather than saved.
 *
 */
void
snmp_save_persistent(const char *type)
{
    char            file[512], fileold[SPRINT_MAX_LEN];
    struct stat     statbuf;
    int             j;

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DONT_PERSIST_STATE)
     || netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DISABLE_PERSISTENT_SAVE)) return;

    DEBUGMSGTL(("snmp_save_persistent", "saving %s files...\n", type));
    snprintf(file, sizeof(file),
             "%s/%s.conf", get_persistent_directory(), type);
    file[ sizeof(file)-1 ] = 0;
    if (stat(file, &statbuf) == 0) {
        for (j = 0; j <= NETSNMP_MAX_PERSISTENT_BACKUPS; j++) {
            snprintf(fileold, sizeof(fileold),
                     "%s/%s.%d.conf", get_persistent_directory(), type, j);
            fileold[ sizeof(fileold)-1 ] = 0;
            if (stat(fileold, &statbuf) != 0) {
                DEBUGMSGTL(("snmp_save_persistent",
                            " saving old config file: %s -> %s.\n", file,
                            fileold));
                if (rename(file, fileold)) {
                    snmp_log(LOG_ERR, "Cannot rename %s to %s\n", file, fileold);
                     /* moving it failed, try nuking it, as leaving
                      * it around is very bad. */
                    if (unlink(file) == -1)
                        snmp_log(LOG_ERR, "Cannot unlink %s\n", file);
                }
                break;
            }
        }
    }
    /*
     * save a warning header to the top of the new file 
     */
    snprintf(fileold, sizeof(fileold),
            "%s%s# Please save normal configuration tokens for %s in SNMPCONFPATH/%s.conf.\n# Only \"createUser\" tokens should be placed here by %s administrators.\n%s",
            "#\n# net-snmp (or ucd-snmp) persistent data file.\n#\n############################################################################\n# STOP STOP STOP STOP STOP STOP STOP STOP STOP \n",
            "#\n#          **** DO NOT EDIT THIS FILE ****\n#\n# STOP STOP STOP STOP STOP STOP STOP STOP STOP \n############################################################################\n#\n# DO NOT STORE CONFIGURATION ENTRIES HERE.\n",
            type, type, type,
	    "# (Did I mention: do not edit this file?)\n#\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    fileold[ sizeof(fileold)-1 ] = 0;
    read_config_store(type, fileold);
}


/*******************************************************************-o-******
 * snmp_clean_persistent
 *
 * Parameters:
 *	*type
 *      
 *
 * Unlink all backup files called "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.%d.conf".
 *
 * Should be called just after we successfull dumped the last of the
 * persistent data, to remove the backup copies of previous storage dumps.
 *
 * XXX  Worth overwriting with random bytes first?  This would
 *	ensure that the data is destroyed, even a buffer containing the
 *	data persists in memory or swap.  Only important if secrets
 *	will be stored here.
 */
void
snmp_clean_persistent(const char *type)
{
    char            file[512];
    struct stat     statbuf;
    int             j;

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DONT_PERSIST_STATE)
     || netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DISABLE_PERSISTENT_SAVE)) return;

    DEBUGMSGTL(("snmp_clean_persistent", "cleaning %s files...\n", type));
    snprintf(file, sizeof(file),
             "%s/%s.conf", get_persistent_directory(), type);
    file[ sizeof(file)-1 ] = 0;
    if (stat(file, &statbuf) == 0) {
        for (j = 0; j <= NETSNMP_MAX_PERSISTENT_BACKUPS; j++) {
            snprintf(file, sizeof(file),
                     "%s/%s.%d.conf", get_persistent_directory(), type, j);
            file[ sizeof(file)-1 ] = 0;
            if (stat(file, &statbuf) == 0) {
                DEBUGMSGTL(("snmp_clean_persistent",
                            " removing old config file: %s\n", file));
                if (unlink(file) == -1)
                    snmp_log(LOG_ERR, "Cannot unlink %s\n", file);
            }
        }
    }
}




/*
 * config_perror: prints a warning string associated with a file and
 * line number of a .conf file and increments the error count. 
 */
static void
config_vlog(int level, const char *levelmsg, const char *str, va_list args)
{
    char tmpbuf[256];
    char* buf = tmpbuf;
    int len = snprintf(tmpbuf, sizeof(tmpbuf), "%s: line %d: %s: %s\n",
		       curfilename, linecount, levelmsg, str);
    if (len >= (int)sizeof(tmpbuf)) {
	buf = (char*)malloc(len + 1);
	sprintf(buf, "%s: line %d: %s: %s\n",
		curfilename, linecount, levelmsg, str);
    }
    snmp_vlog(level, buf, args);
    if (buf != tmpbuf)
	free(buf);
}

void
netsnmp_config_error(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    config_vlog(LOG_ERR, "Error", str, args);
    va_end(args);
    config_errors++;
}

void
netsnmp_config_warn(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    config_vlog(LOG_WARNING, "Warning", str, args);
    va_end(args);
}

void
config_perror(const char *str)
{
    netsnmp_config_error("%s", str);
}

void
config_pwarn(const char *str)
{
    netsnmp_config_warn("%s", str);
}

/*
 * skip all white spaces and return 1 if found something either end of
 * line or a comment character 
 */
char           *
skip_white(char *ptr)
{
    return NETSNMP_REMOVE_CONST(char *, skip_white_const(ptr));
}

const char     *
skip_white_const(const char *ptr)
{
    if (ptr == NULL)
        return (NULL);
    while (*ptr != 0 && isspace((unsigned char)*ptr))
        ptr++;
    if (*ptr == 0 || *ptr == '#')
        return (NULL);
    return (ptr);
}

char           *
skip_not_white(char *ptr)
{
    return NETSNMP_REMOVE_CONST(char *, skip_not_white_const(ptr));
}

const char     *
skip_not_white_const(const char *ptr)
{
    if (ptr == NULL)
        return (NULL);
    while (*ptr != 0 && !isspace((unsigned char)*ptr))
        ptr++;
    if (*ptr == 0 || *ptr == '#')
        return (NULL);
    return (ptr);
}

char           *
skip_token(char *ptr)
{
    return NETSNMP_REMOVE_CONST(char *, skip_token_const(ptr));
}

const char     *
skip_token_const(const char *ptr)
{
    ptr = skip_white_const(ptr);
    ptr = skip_not_white_const(ptr);
    ptr = skip_white_const(ptr);
    return (ptr);
}

/*
 * copy_word
 * copies the next 'token' from 'from' into 'to', maximum len-1 characters.
 * currently a token is anything seperate by white space
 * or within quotes (double or single) (i.e. "the red rose" 
 * is one token, \"the red rose\" is three tokens)
 * a '\' character will allow a quote character to be treated
 * as a regular character 
 * It returns a pointer to first non-white space after the end of the token
 * being copied or to 0 if we reach the end.
 * Note: Partially copied words (greater than len) still returns a !NULL ptr
 * Note: partially copied words are, however, null terminated.
 */

char           *
copy_nword(char *from, char *to, int len)
{
    return NETSNMP_REMOVE_CONST(char *, copy_nword_const(from, to, len));
}

const char           *
copy_nword_const(const char *from, char *to, int len)
{
    char            quote;
    if (!from || !to)
        return NULL;
    if ((*from == '\"') || (*from == '\'')) {
        quote = *(from++);
        while ((*from != quote) && (*from != 0)) {
            if ((*from == '\\') && (*(from + 1) != 0)) {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *(from + 1);
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                }
                from = from + 2;
            } else {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *from++;
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                } else
                    from++;
            }
        }
        if (*from == 0) {
            DEBUGMSGTL(("read_config_copy_word",
                        "no end quote found in config string\n"));
        } else
            from++;
    } else {
        while (*from != 0 && !isspace((unsigned char)(*from))) {
            if ((*from == '\\') && (*(from + 1) != 0)) {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *(from + 1);
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                }
                from = from + 2;
            } else {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *from++;
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                } else
                    from++;
            }
        }
    }
    if (len > 0)
        *to = 0;
    from = skip_white_const(from);
    return (from);
}                               /* copy_nword */

/*
 * copy_word
 * copies the next 'token' from 'from' into 'to'.
 * currently a token is anything seperate by white space
 * or within quotes (double or single) (i.e. "the red rose" 
 * is one token, \"the red rose\" is three tokens)
 * a '\' character will allow a quote character to be treated
 * as a regular character 
 * It returns a pointer to first non-white space after the end of the token
 * being copied or to 0 if we reach the end.
 */

static int      have_warned = 0;
char           *
copy_word(char *from, char *to)
{
    if (!have_warned) {
        snmp_log(LOG_INFO,
                 "copy_word() called.  Use copy_nword() instead.\n");
        have_warned = 1;
    }
    return copy_nword(from, to, SPRINT_MAX_LEN);
}                               /* copy_word */

/*
 * read_config_save_octet_string(): saves an octet string as a length
 * followed by a string of hex 
 */
char           *
read_config_save_octet_string(char *saveto, u_char * str, size_t len)
{
    int             i;
    u_char         *cp;

    /*
     * is everything easily printable 
     */
    for (i = 0, cp = str; i < (int) len && cp &&
         (isalpha(*cp) || isdigit(*cp) || *cp == ' '); cp++, i++);

    if (len != 0 && i == (int) len) {
        *saveto++ = '"';
        memcpy(saveto, str, len);
        saveto += len;
        *saveto++ = '"';
        *saveto = '\0';
    } else {
        if (str != NULL) {
            sprintf(saveto, "0x");
            saveto += 2;
            for (i = 0; i < (int) len; i++) {
                sprintf(saveto, "%02x", str[i]);
                saveto = saveto + 2;
            }
        } else {
            sprintf(saveto, "\"\"");
            saveto += 2;
        }
    }
    return saveto;
}

/**
 * Reads an octet string that was saved by the
 * read_config_save_octet_string() function.
 *
 * @param[in]     readfrom Pointer to the input data to be parsed.
 * @param[in,out] str      Pointer to the output buffer pointer. The data
 *   written to the output buffer will be '\0'-terminated. If *str == NULL,
 *   an output buffer will be allocated that is one byte larger than the
 *   data stored.
 * @param[in,out] len      If str != NULL, *len is the size of the buffer *str
 *   points at. If str == NULL, the value passed via *len is ignored.
 *   Before this function returns the number of bytes read will be stored
 *   in *len. If a buffer overflow occurs, *len will be set to 0.
 *
 * @return A pointer to the next character in the input to be parsed if
 *   parsing succeeded; NULL when the end of the input string has been reached
 *   or if an error occurred.
 */
char           *
read_config_read_octet_string(const char *readfrom, u_char ** str,
                              size_t * len)
{
    return NETSNMP_REMOVE_CONST(char *,
               read_config_read_octet_string_const(readfrom, str, len));
}

const char     *
read_config_read_octet_string_const(const char *readfrom, u_char ** str,
                                    size_t * len)
{
    u_char         *cptr;
    const char     *cptr1;
    u_int           tmp;
    size_t          i, ilen;

    if (readfrom == NULL || str == NULL || len == NULL)
        return NULL;

    if (strncasecmp(readfrom, "0x", 2) == 0) {
        /*
         * A hex string submitted. How long? 
         */
        readfrom += 2;
        cptr1 = skip_not_white_const(readfrom);
        if (cptr1)
            ilen = (cptr1 - readfrom);
        else
            ilen = strlen(readfrom);

        if (ilen % 2) {
            snmp_log(LOG_WARNING,"invalid hex string: wrong length\n");
            DEBUGMSGTL(("read_config_read_octet_string",
                        "invalid hex string: wrong length"));
            return NULL;
        }
        ilen = ilen / 2;

        /*
         * malloc data space if needed (+1 for good measure) 
         */
        if (*str == NULL) {
            *str = (u_char *) malloc(ilen + 1);
            if (!*str)
                return NULL;
        } else {
            /*
             * require caller to have +1, and bail if not enough space.
             */
            if (ilen >= *len) {
                snmp_log(LOG_WARNING,"buffer too small to read octet string (%lu < %lu)\n",
                         (unsigned long)*len, (unsigned long)ilen);
                DEBUGMSGTL(("read_config_read_octet_string",
                            "buffer too small (%lu < %lu)", (unsigned long)*len, (unsigned long)ilen));
                *len = 0;
                cptr1 = skip_not_white_const(readfrom);
                return skip_white_const(cptr1);
            }
        }

        /*
         * copy validated data 
         */
        cptr = *str;
        for (i = 0; i < ilen; i++) {
            if (1 == sscanf(readfrom, "%2x", &tmp))
                *cptr++ = (u_char) tmp;
            else {
                /*
                 * we may lose memory, but don't know caller's buffer XX free(cptr); 
                 */
                return (NULL);
            }
            readfrom += 2;
        }
        /*
         * Terminate the output buffer.
         */
        *cptr++ = '\0';
        *len = ilen;
        readfrom = skip_white_const(readfrom);
    } else {
        /*
         * Normal string 
         */

        /*
         * malloc string space if needed (including NULL terminator) 
         */
        if (*str == NULL) {
            char            buf[SNMP_MAXBUF];
            readfrom = copy_nword_const(readfrom, buf, sizeof(buf));

            *len = strlen(buf);
            *str = (u_char *) malloc(*len + 1);
            if (*str == NULL)
                return NULL;
            memcpy(*str, buf, *len + 1);
        } else {
            readfrom = copy_nword_const(readfrom, (char *) *str, *len);
            if (*len)
                *len = strlen((char *) *str);
        }
    }

    return readfrom;
}

/*
 * read_config_save_objid(): saves an objid as a numerical string 
 */
char           *
read_config_save_objid(char *saveto, oid * objid, size_t len)
{
    int             i;

    if (len == 0) {
        strcat(saveto, "NULL");
        saveto += strlen(saveto);
        return saveto;
    }

    /*
     * in case len=0, this makes it easier to read it back in 
     */
    for (i = 0; i < (int) len; i++) {
        sprintf(saveto, ".%" NETSNMP_PRIo "d", objid[i]);
        saveto += strlen(saveto);
    }
    return saveto;
}

/*
 * read_config_read_objid(): reads an objid from a format saved by the above 
 */
char           *
read_config_read_objid(char *readfrom, oid ** objid, size_t * len)
{
    return NETSNMP_REMOVE_CONST(char *,
             read_config_read_objid_const(readfrom, objid, len));
}

const char     *
read_config_read_objid_const(const char *readfrom, oid ** objid, size_t * len)
{

    if (objid == NULL || readfrom == NULL || len == NULL)
        return NULL;

    if (*objid == NULL) {
        *len = 0;
        if ((*objid = (oid *) malloc(MAX_OID_LEN * sizeof(oid))) == NULL)
            return NULL;
        *len = MAX_OID_LEN;
    }

    if (strncmp(readfrom, "NULL", 4) == 0) {
        /*
         * null length oid 
         */
        *len = 0;
    } else {
        /*
         * qualify the string for read_objid 
         */
        char            buf[SPRINT_MAX_LEN];
        copy_nword_const(readfrom, buf, sizeof(buf));

        if (!read_objid(buf, *objid, len)) {
            DEBUGMSGTL(("read_config_read_objid", "Invalid OID"));
            *len = 0;
            return NULL;
        }
    }

    readfrom = skip_token_const(readfrom);
    return readfrom;
}

/**
 * read_config_read_data reads data of a given type from a token(s) on a
 * configuration line.  The supported types are:
 *
 *    - ASN_INTEGER
 *    - ASN_TIMETICKS
 *    - ASN_UNSIGNED
 *    - ASN_OCTET_STR
 *    - ASN_BIT_STR
 *    - ASN_OBJECT_ID
 *
 * @param type the asn data type to be read in.
 *
 * @param readfrom the configuration line data to be read.
 *
 * @param dataptr an allocated pointer expected to match the type being read
 *        (int *, u_int *, char **, oid **)
 *
 * @param len is the length of an asn oid or octet/bit string, not required
 *            for the asn integer, unsigned integer, and timeticks types
 *
 * @return the next token in the configuration line.  NULL if none left or
 * if an unknown type.
 * 
 */
char           *
read_config_read_data(int type, char *readfrom, void *dataptr,
                      size_t * len)
{
    int            *intp;
    char          **charpp;
    oid           **oidpp;
    unsigned int   *uintp;

    if (dataptr && readfrom)
        switch (type) {
        case ASN_INTEGER:
            intp = (int *) dataptr;
            *intp = atoi(readfrom);
            readfrom = skip_token(readfrom);
            return readfrom;

        case ASN_TIMETICKS:
        case ASN_UNSIGNED:
            uintp = (unsigned int *) dataptr;
            *uintp = strtoul(readfrom, NULL, 0);
            readfrom = skip_token(readfrom);
            return readfrom;

        case ASN_IPADDRESS:
            intp = (int *) dataptr;
            *intp = inet_addr(readfrom);
            if ((*intp == -1) &&
                (strncmp(readfrom, "255.255.255.255", 15) != 0))
                return NULL;
            readfrom = skip_token(readfrom);
            return readfrom;

        case ASN_OCTET_STR:
        case ASN_BIT_STR:
            charpp = (char **) dataptr;
            return read_config_read_octet_string(readfrom,
                                                 (u_char **) charpp, len);

        case ASN_OBJECT_ID:
            oidpp = (oid **) dataptr;
            return read_config_read_objid(readfrom, oidpp, len);

        default:
            DEBUGMSGTL(("read_config_read_data", "Fail: Unknown type: %d",
                        type));
            return NULL;
        }
    return NULL;
}

/*
 * read_config_read_memory():
 * 
 * similar to read_config_read_data, but expects a generic memory
 * pointer rather than a specific type of pointer.  Len is expected to
 * be the amount of available memory.
 */
char           *
read_config_read_memory(int type, char *readfrom,
                        char *dataptr, size_t * len)
{
    int            *intp;
    unsigned int   *uintp;
    char            buf[SPRINT_MAX_LEN];

    if (!dataptr || !readfrom)
        return NULL;

    switch (type) {
    case ASN_INTEGER:
        if (*len < sizeof(int))
            return NULL;
        intp = (int *) dataptr;
        readfrom = copy_nword(readfrom, buf, sizeof(buf));
        *intp = atoi(buf);
        *len = sizeof(int);
        return readfrom;

    case ASN_COUNTER:
    case ASN_TIMETICKS:
    case ASN_UNSIGNED:
        if (*len < sizeof(unsigned int))
            return NULL;
        uintp = (unsigned int *) dataptr;
        readfrom = copy_nword(readfrom, buf, sizeof(buf));
        *uintp = strtoul(buf, NULL, 0);
        *len = sizeof(unsigned int);
        return readfrom;

    case ASN_IPADDRESS:
        if (*len < sizeof(int))
            return NULL;
        intp = (int *) dataptr;
        readfrom = copy_nword(readfrom, buf, sizeof(buf));
        *intp = inet_addr(buf);
        if ((*intp == -1) && (strcmp(buf, "255.255.255.255") != 0))
            return NULL;
        *len = sizeof(int);
        return readfrom;

    case ASN_OCTET_STR:
    case ASN_BIT_STR:
    case ASN_PRIV_IMPLIED_OCTET_STR:
        return read_config_read_octet_string(readfrom,
                                             (u_char **) & dataptr, len);

    case ASN_PRIV_IMPLIED_OBJECT_ID:
    case ASN_OBJECT_ID:
        readfrom =
            read_config_read_objid(readfrom, (oid **) & dataptr, len);
        *len *= sizeof(oid);
        return readfrom;

    case ASN_COUNTER64:
        if (*len < sizeof(U64))
            return NULL;
        *len = sizeof(U64);
        read64((U64 *) dataptr, readfrom);
        readfrom = skip_token(readfrom);
        return readfrom;
    }

    DEBUGMSGTL(("read_config_read_memory", "Fail: Unknown type: %d", type));
    return NULL;
}

/**
 * read_config_store_data stores data of a given type to a configuration line
 * into the storeto buffer.
 * Calls read_config_store_data_prefix with the prefix parameter set to a char
 * space.  The supported types are:
 *
 *    - ASN_INTEGER
 *    - ASN_TIMETICKS
 *    - ASN_UNSIGNED
 *    - ASN_OCTET_STR
 *    - ASN_BIT_STR
 *    - ASN_OBJECT_ID
 *
 * @param type    the asn data type to be stored
 *
 * @param storeto a pre-allocated char buffer which will contain the data
 *                to be stored
 *
 * @param dataptr contains the value to be stored, the supported pointers:
 *                (int *, u_int *, char **, oid **)
 *
 * @param len     is the length of the value to be stored
 *                (not required for the asn integer, unsigned integer,
 *                 and timeticks types)
 *
 * @return character pointer to the end of the line. NULL if an unknown type.
 */
char           *
read_config_store_data(int type, char *storeto, void *dataptr, size_t * len)
{
    return read_config_store_data_prefix(' ', type, storeto, dataptr,
                                                         (len ? *len : 0));
}

char           *
read_config_store_data_prefix(char prefix, int type, char *storeto,
                              void *dataptr, size_t len)
{
    int            *intp;
    u_char        **charpp;
    unsigned int   *uintp;
    struct in_addr  in;
    oid           **oidpp;

    if (dataptr && storeto)
        switch (type) {
        case ASN_INTEGER:
            intp = (int *) dataptr;
            sprintf(storeto, "%c%d", prefix, *intp);
            return (storeto + strlen(storeto));

        case ASN_TIMETICKS:
        case ASN_UNSIGNED:
            uintp = (unsigned int *) dataptr;
            sprintf(storeto, "%c%u", prefix, *uintp);
            return (storeto + strlen(storeto));

        case ASN_IPADDRESS:
            in.s_addr = *(unsigned int *) dataptr; 
            sprintf(storeto, "%c%s", prefix, inet_ntoa(in));
            return (storeto + strlen(storeto));

        case ASN_OCTET_STR:
        case ASN_BIT_STR:
            *storeto++ = prefix;
            charpp = (u_char **) dataptr;
            return read_config_save_octet_string(storeto, *charpp, len);

        case ASN_OBJECT_ID:
            *storeto++ = prefix;
            oidpp = (oid **) dataptr;
            return read_config_save_objid(storeto, *oidpp, len);

        default:
            DEBUGMSGTL(("read_config_store_data_prefix",
                        "Fail: Unknown type: %d", type));
            return NULL;
        }
    return NULL;
}

/** @} */
