/*
 * $Id: $
 *
 * Private interface for io objects to provide plug-in io objects
 */

#ifndef _IO_PLUGIN_H_
#define _IO_PLUGIN_H_

typedef struct tag_io_privhandle IO_PRIVHANDLE;
typedef struct tag_io_fnptr IO_FNPTR;
typedef struct tag_io_optionlist IO_OPTIONLIST;

#define IO_LOG_FATAL 0
#define IO_LOG_LOG   1
#define IO_LOG_WARN  3
#define IO_LOG_INFO  5
#define IO_LOG_DEBUG 9
#define IO_LOG_SPAM  10

#ifdef WIN32
# define WAITABLE_T HANDLE
# define SOCKET_T SOCKET
# define FILE_T HANDLE
# define ERR_T DWORD
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
# define WAITABLE_T int
# define SOCKET_T int
# define FILE_T int
# define ERR_T int
#endif

struct tag_io_fnptr {
    int(*fn_open)(IO_PRIVHANDLE *, char *);
    int(*fn_close)(IO_PRIVHANDLE *);
    int(*fn_read)(IO_PRIVHANDLE *, unsigned char *, uint32_t *);
    int(*fn_write)(IO_PRIVHANDLE *, unsigned char *, uint32_t *);
    int(*fn_size)(IO_PRIVHANDLE *, uint64_t *);
    int(*fn_setpos)(IO_PRIVHANDLE *, uint64_t, int);
    int(*fn_getpos)(IO_PRIVHANDLE *, uint64_t *);
    char*(*fn_geterrmsg)(IO_PRIVHANDLE *, ERR_T *, int *);
    int(*fn_getwaitable)(IO_PRIVHANDLE *, int, WAITABLE_T *);
    int(*fn_getfd)(IO_PRIVHANDLE *, FILE_T *);
    int(*fn_getsocket)(IO_PRIVHANDLE *, SOCKET_T *);
};

struct tag_io_privhandle {
    int open;               /**< Whether file is open - don't touch */
    ERR_T err;              /**< Current error code - don't touch */
    int is_local;
    char *err_str;          /**< Current error string - don't touch */
    IO_FNPTR *fnptr;        /**< Set on io_open by checking proto table */
    IO_OPTIONLIST *pol;     /**< List of passed options */
    char *proto;            /**< proto of file */
    int buffering;          /**< are we in linebuffer mode? */
    uint32_t buffer_offset; /**< current offset pointer */
    uint32_t buffer_len;    /**< total size of buffer */
    unsigned char *buffer;  /**< linebuffer */

    /**
     * the following parameters should be set by the provider in
     * the open function (or as soon as is practicable)
     */
    void *private;       /**< Private struct for implementation */
};

extern int io_register_handler(char *proto, IO_FNPTR *fnptr);
extern char* io_option_get(IO_PRIVHANDLE *phandle,char *option,char *dflt);

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

#endif /* _IO_PLUGIN_H_ */
