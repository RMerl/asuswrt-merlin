

#pragma once

#include <stdlib.h>
#include <process.h>
#include <io.h>

#define EOPNOTSUPP  95

#define O_NONBLOCK  0
#define O_RDONLY    _O_RDONLY
#define O_RDWR      _O_RDWR

#define popen   _popen
#define pclose  _pclose
#define sleep   _sleep
#define stat    _stat
#define open    _open
#define close   _close
#define fstat   _fstat
#define read    _read
#define write   _write
#define off_t   _off_t
#define lseek   _lseek
#define putenv  _putenv
#define getpid  _getpid
#define utimbuf _utimbuf
#define sys_nerr _sys_nerr
#define sys_errlist _sys_errlist
#define isatty _isatty
#define getch _getch

#include <grp.h>
#include <pwd.h>


// no-oped sync
__inline void sync(void){};



#define gettimeofday(p, v) ((p)->tv_sec = (p)->tv_usec = 0)


#define strcasecmp _stricmp



