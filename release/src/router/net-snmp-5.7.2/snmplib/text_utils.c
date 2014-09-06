#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <stdio.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#   include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#   include <unistd.h>
#endif
#if HAVE_STRING_H
#   include <string.h>
#else
#  include <strings.h>
#endif

#include <sys/types.h>

#if HAVE_LIMITS_H
#   include <limits.h>
#endif
#if HAVE_SYS_PARAM_H
#   include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif

#include <errno.h>

#if HAVE_DMALLOC_H
#  include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/file_utils.h>
#include <net-snmp/library/text_utils.h>

netsnmp_feature_child_of(text_utils, libnetsnmp)

netsnmp_feature_provide(text_utils)
#ifdef NETSNMP_FEATURE_REQUIRE_TEXT_UTILS
netsnmp_feature_require(file_utils)
#endif /* NETSNMP_FEATURE_REQUIRE_TEXT_UTILS */

#ifndef NETSNMP_FEATURE_REMOVE_TEXT_UTILS
/*------------------------------------------------------------------
 *
 * Prototypes
 *
 */
/*
 * parse methods
 */
void
_pm_save_index_string_string(FILE *f, netsnmp_container *cin,
                             int flags);
void
_pm_save_everything(FILE *f, netsnmp_container *cin, int flags);
void
_pm_user_function(FILE *f, netsnmp_container *cin,
                  netsnmp_line_process_info *lpi, int flags);


/*
 * line processors
 */
int _process_line_tvi(netsnmp_line_info *line_info, void *mem,
                      struct netsnmp_line_process_info_s* lpi);



/*------------------------------------------------------------------
 *
 * Text file processing functions
 *
 */

/**
 * process text file, reading into extras
 */
netsnmp_container *
netsnmp_file_text_parse(netsnmp_file *f, netsnmp_container *cin,
                        int parse_mode, u_int flags, void *context)
{
    netsnmp_container *c = cin;
    FILE              *fin;
    int                rc;

    if (NULL == f)
        return NULL;

    if ((NULL == c) && (!(flags & PM_FLAG_NO_CONTAINER))) {
        c = netsnmp_container_find("text_parse:binary_array");
        if (NULL == c)
            return NULL;
    }

    rc = netsnmp_file_open(f);
    if (rc < 0) { /** error already logged */
        if ((NULL !=c) && (c != cin))
            CONTAINER_FREE(c);
        return NULL;
    }
    
    /*
     * get a stream from the file descriptor. This DOES NOT rewind the
     * file (if fd was previously opened).
     */
    fin = fdopen(f->fd, "r");
    if (NULL == fin) {
        if (NS_FI_AUTOCLOSE(f->ns_flags))
            close(f->fd);
        if ((NULL !=c) && (c != cin))
            CONTAINER_FREE(c);
        return NULL;
    }

    switch (parse_mode) {

        case PM_SAVE_EVERYTHING:
            _pm_save_everything(fin, c, flags);
            break;

        case PM_INDEX_STRING_STRING:
            _pm_save_index_string_string(fin, c, flags);
            break;

        case PM_USER_FUNCTION:
            if (NULL != context)
                _pm_user_function(fin, c, (netsnmp_line_process_info*)context,
                                  flags);
            break;

        default:
            snmp_log(LOG_ERR, "unknown parse mode %d\n", parse_mode);
            break;
    }


    /*
     * close the stream, which will have the side effect of also closing
     * the file descriptor, so we need to reset it.
     */
    fclose(fin);
    f->fd = -1;

    return c;
}

netsnmp_feature_child_of(text_token_container_from_file, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_TEXT_TOKEN_CONTAINER_FROM_FILE
netsnmp_container *
netsnmp_text_token_container_from_file(const char *file, u_int flags,
                                       netsnmp_container *cin, void *context)
{
    netsnmp_line_process_info  lpi;
    netsnmp_container         *c = cin, *c_rc;
    netsnmp_file              *fp;

    if (NULL == file)
        return NULL;

    /*
     * allocate file resources
     */
    fp = netsnmp_file_fill(NULL, file, O_RDONLY, 0, 0);
    if (NULL == fp) /** msg already logged */
        return NULL;

    memset(&lpi, 0x0, sizeof(lpi));
    lpi.mem_size = sizeof(netsnmp_token_value_index);
    lpi.process = _process_line_tvi;
    lpi.user_context = context;

    if (NULL == c) {
        c = netsnmp_container_find("string:binary_array");
        if (NULL == c) {
            snmp_log(LOG_ERR,"malloc failed\n");
            netsnmp_file_release(fp);
            return NULL;
        }
    }

    c_rc = netsnmp_file_text_parse(fp, c, PM_USER_FUNCTION, 0, &lpi);

    /*
     * if we got a bad return and the user didn't pass us a container,
     * we need to release the container we allocated.
     */
    if ((NULL == c_rc) && (NULL == cin)) {
        CONTAINER_FREE(c);
        c = NULL;
    }
    else
        c = c_rc;

    /*
     * release file resources
     */
    netsnmp_file_release(fp);
    
    return c;
}
#endif /* NETSNMP_FEATURE_REMOVE_TEXT_TOKEN_CONTAINER_FROM_FILE */


/*------------------------------------------------------------------
 *
 * Text file process modes helper functions
 *
 */

/**
 * @internal
 * parse mode: save everything
 */
void
_pm_save_everything(FILE *f, netsnmp_container *cin, int flags)
{
    char               line[STRINGMAX], *ptr;
    size_t             len;

    netsnmp_assert(NULL != f);
    netsnmp_assert(NULL != cin);

    while (fgets(line, sizeof(line), f) != NULL) {

        ptr = line;
        len = strlen(line) - 1;
        if (line[len] == '\n')
            line[len] = 0;

        /*
         * save blank line or comment?
         */
        if (flags & PM_FLAG_SKIP_WHITESPACE) {
            if (NULL == (ptr = skip_white(ptr)))
                continue;
        }

        ptr = strdup(line);
        if (NULL == ptr) {
            snmp_log(LOG_ERR,"malloc failed\n");
            break;
        }

        CONTAINER_INSERT(cin,ptr);
    }
}

/**
 * @internal
 * parse mode: 
 */
void
_pm_save_index_string_string(FILE *f, netsnmp_container *cin,
                             int flags)
{
    char                        line[STRINGMAX], *ptr;
    netsnmp_token_value_index  *tvi;
    size_t                      count = 0, len;

    netsnmp_assert(NULL != f);
    netsnmp_assert(NULL != cin);

    while (fgets(line, sizeof(line), f) != NULL) {

        ++count;
        ptr = line;
        len = strlen(line) - 1;
        if (line[len] == '\n')
            line[len] = 0;

        /*
         * save blank line or comment?
         */
        if (flags & PM_FLAG_SKIP_WHITESPACE) {
            if (NULL == (ptr = skip_white(ptr)))
                continue;
        }

        tvi = SNMP_MALLOC_TYPEDEF(netsnmp_token_value_index);
        if (NULL == tvi) {
            snmp_log(LOG_ERR,"malloc failed\n");
            break;
        }
            
        /*
         * copy whole line, then set second pointer to
         * after token. One malloc, 2 strings!
         */
        tvi->index = count;
        tvi->token = strdup(line);
        if (NULL == tvi->token) {
            snmp_log(LOG_ERR,"malloc failed\n");
            free(tvi);
            break;
        }
        tvi->value.cp = skip_not_white(tvi->token);
        if (NULL != tvi->value.cp) {
            *(tvi->value.cp) = 0;
            ++(tvi->value.cp);
        }
        CONTAINER_INSERT(cin, tvi);
    }
}

/**
 * @internal
 * parse mode: 
 */
void
_pm_user_function(FILE *f, netsnmp_container *cin,
                  netsnmp_line_process_info *lpi, int flags)
{
    char                        buf[STRINGMAX];
    netsnmp_line_info           li;
    void                       *mem = NULL;
    int                         rc;

    netsnmp_assert(NULL != f);
    netsnmp_assert(NULL != cin);

    /*
     * static buf, or does the user want the memory?
     */
    if (flags & PMLP_FLAG_ALLOC_LINE) {
        if (0 != lpi->line_max)
            li.line_max =  lpi->line_max;
        else
            li.line_max = STRINGMAX;
        li.line = (char *)calloc(li.line_max, 1);
        if (NULL == li.line) {
            snmp_log(LOG_ERR,"malloc failed\n");
            return;
        }
    }
    else {
        li.line = buf;
        li.line_max = sizeof(buf);
    }
        
    li.index = 0;
    while (fgets(li.line, li.line_max, f) != NULL) {

        ++li.index;
        li.start = li.line;
        li.line_len = strlen(li.line) - 1;
        if ((!(lpi->flags & PMLP_FLAG_LEAVE_NEWLINE)) &&
            (li.line[li.line_len] == '\n'))
            li.line[li.line_len] = 0;
        
        /*
         * save blank line or comment?
         */
        if (!(lpi->flags & PMLP_FLAG_PROCESS_WHITESPACE)) {
            if (NULL == (li.start = skip_white(li.start)))
                continue;
        }

        /*
         *  do we need to allocate memory for the use?
         * if the last call didn't use the memory we allocated,
         * re-use it. Otherwise, allocate new chunk.
         */
        if ((0 != lpi->mem_size) && (NULL == mem)) {
            mem = calloc(lpi->mem_size, 1);
            if (NULL == mem) {
                snmp_log(LOG_ERR,"malloc failed\n");
                break;
            }
        }

        /*
         * do they want a copy ot the line?
         */
        if (lpi->flags & PMLP_FLAG_STRDUP_LINE) {
            li.start = strdup(li.start);
            if (NULL == li.start) {
                snmp_log(LOG_ERR,"malloc failed\n");
                break;
            }
        }
        else if (lpi->flags & PMLP_FLAG_ALLOC_LINE) {
            li.start = li.line;
        }

        /*
         * call the user function. If the function wants to save
         * the memory chunk, insert it in the container, the clear
         * pointer so we reallocate next time.
         */
        li.start_len = strlen(li.start);
        rc = (*lpi->process)(&li, mem, lpi);
        if (PMLP_RC_MEMORY_USED == rc) {

            if (!(lpi->flags & PMLP_FLAG_NO_CONTAINER))
                CONTAINER_INSERT(cin, mem);
            
            mem = NULL;
            
            if (lpi->flags & PMLP_FLAG_ALLOC_LINE) {
	        li.line = (char *)calloc(li.line_max, 1);
                if (NULL == li.line) {
                    snmp_log(LOG_ERR,"malloc failed\n");
                    break;
                }
            }
        }
        else if (PMLP_RC_MEMORY_UNUSED == rc ) {
            /*
             * they didn't use the memory. if li.start was a strdup, we have
             * to release it. leave mem, we can re-use it (its a fixed size).
             */
            if (lpi->flags & PMLP_FLAG_STRDUP_LINE)
                free(li.start); /* no point in SNMP_FREE */
        }
        else {
            if (PMLP_RC_STOP_PROCESSING != rc )
                snmp_log(LOG_ERR, "unknown rc %d from text processor\n", rc);
            break;
        }
    }
    SNMP_FREE(mem);
}

/*------------------------------------------------------------------
 *
 * Test line process helper functions
 *
 */
/**
 * @internal
 * process token value index line
 */
int
_process_line_tvi(netsnmp_line_info *line_info, void *mem,
                  struct netsnmp_line_process_info_s* lpi)
{
    netsnmp_token_value_index *tvi = (netsnmp_token_value_index *)mem;
    char                      *ptr;

    /*
     * get token
     */
    ptr = skip_not_white(line_info->start);
    if (NULL == ptr) {
        DEBUGMSGTL(("text:util:tvi", "no value after token '%s'\n",
                    line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }

    /*
     * null terminate, search for value;
     */
    *(ptr++) = 0;
    ptr = skip_white(ptr);
    if (NULL == ptr) {
        DEBUGMSGTL(("text:util:tvi", "no value after token '%s'\n",
                    line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }

    /*
     * get value
     */
    switch((int)(intptr_t)lpi->user_context) {

        case PMLP_TYPE_UNSIGNED:
            tvi->value.ul = strtoul(ptr, NULL, 0);
            if ((errno == ERANGE) && (ULONG_MAX == tvi->value.ul))
                snmp_log(LOG_WARNING,"value overflow\n");
            break;


        case PMLP_TYPE_INTEGER:
            tvi->value.sl = strtol(ptr, NULL, 0);
            if ((errno == ERANGE) &&
                ((LONG_MAX == tvi->value.sl) ||
                 (LONG_MIN == tvi->value.sl)))
                snmp_log(LOG_WARNING,"value over/under-flow\n");
            break;

        case PMLP_TYPE_STRING:
            tvi->value.cp = strdup(ptr);
            break;

        case PMLP_TYPE_BOOLEAN:
            if (isdigit((unsigned char)(*ptr)))
                tvi->value.ul = strtoul(ptr, NULL, 0);
            else if (strcasecmp(ptr,"true") == 0)
                tvi->value.ul = 1;
            else if (strcasecmp(ptr,"false") == 0)
                tvi->value.ul = 0;
            else {
                snmp_log(LOG_WARNING,"bad value for boolean\n");
                return PMLP_RC_MEMORY_UNUSED;
            }
            break;

        default:
            snmp_log(LOG_ERR,"unsupported value type %d\n",
                     (int)(intptr_t)lpi->user_context);
            break;
    }
    
    /*
     * save token and value
     */
    tvi->token = strdup(line_info->start);
    tvi->index = line_info->index;

    return PMLP_RC_MEMORY_USED;
}

#else  /* ! NETSNMP_FEATURE_REMOVE_TEXT_UTILS */
netsnmp_feature_unused(text_utils);
#endif /* ! NETSNMP_FEATURE_REMOVE_TEXT_UTILS */
