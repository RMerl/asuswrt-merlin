/* __location__ macro replacement taken from talloc.h */

/*
  this uses a little trick to allow __LINE__ to be stringified
*/
#ifndef __location__
#define __STRING_LINE1__(s)    #s
#define __STRING_LINE2__(s)   __STRING_LINE1__(s)
#define __STRING_LINE3__  __STRING_LINE2__(__LINE__)
#define __location__ __FILE__ ":" __STRING_LINE3__
#endif

#ifndef __STRING
#define __STRING(s) #s
#endif
