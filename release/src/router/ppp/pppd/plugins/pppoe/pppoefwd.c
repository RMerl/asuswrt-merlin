#include "pppoe.h"

void fatal (char *fmt, ...)
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    vprintf( fmt, pvar);
    va_end(pvar);

    exit(1);			/* as promised */
}

void info (char *fmt, ...)
{
    va_list pvar;

#if defined(__STDC__)
    va_start(pvar, fmt);
#else
    char *fmt;
    va_start(pvar);
    fmt = va_arg(pvar, char *);
#endif

    vprintf( fmt, pvar);
    va_end(pvar);

}


int main(int argc, char** argv){
    int ret;
    struct session *ses = (struct session *)malloc(sizeof(struct session));

    if(!ses) return -1;

    ret = relay_init_ses(ses,argv[1],argv[2]);
    
    if( ret < 0 ){
	return -1;
    }

    ses->log_to_fd = 1;
    ses->opt_debug=1;
    while(1)
	ret = session_connect(ses);
    
    
    
    return ret;


}
