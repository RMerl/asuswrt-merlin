#include <signal.h>
#include <unistd.h>

#ifdef __sh__
#define signal(a,b)	/* Signals not supported on this target - make them go away */
#endif

/* Signal handlers, we set breakpoints in them to make sure that the
   signals really get delivered.  */

#ifdef PROTOTYPES
void
handle_ABRT (int sig)
#else
void
handle_ABRT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_HUP (int sig)
#else
void
handle_HUP (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_QUIT (int sig)
#else
void
handle_QUIT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_ILL (int sig)
#else
void
handle_ILL (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_EMT (int sig)
#else
void
handle_EMT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_FPE (int sig)
#else
void
handle_FPE (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_BUS (int sig)
#else
void
handle_BUS (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_SEGV (int sig)
#else
void
handle_SEGV (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_SYS (int sig)
#else
void
handle_SYS (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_PIPE (int sig)
#else
void
handle_PIPE (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_ALRM (int sig)
#else
void
handle_ALRM (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_URG (int sig)
#else
void
handle_URG (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_TSTP (int sig)
#else
void
handle_TSTP (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_CONT (int sig)
#else
void
handle_CONT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_CHLD (int sig)
#else
void
handle_CHLD (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_TTIN (int sig)
#else
void
handle_TTIN (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_TTOU (int sig)
#else
void
handle_TTOU (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_IO (int sig)
#else
void
handle_IO (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_XCPU (int sig)
#else
void
handle_XCPU (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_XFSZ (int sig)
#else
void
handle_XFSZ (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_VTALRM (int sig)
#else
void
handle_VTALRM (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_PROF (int sig)
#else
void
handle_PROF (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_WINCH (int sig)
#else
void
handle_WINCH (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_LOST (int sig)
#else
void
handle_LOST (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_USR1 (int sig)
#else
void
handle_USR1 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_USR2 (int sig)
#else
void
handle_USR2 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_PWR (int sig)
#else
void
handle_PWR (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_POLL (int sig)
#else
void
handle_POLL (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_WIND (int sig)
#else
void
handle_WIND (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_PHONE (int sig)
#else
void
handle_PHONE (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_WAITING (int sig)
#else
void
handle_WAITING (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_LWP (int sig)
#else
void
handle_LWP (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_DANGER (int sig)
#else
void
handle_DANGER (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_GRANT (int sig)
#else
void
handle_GRANT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_RETRACT (int sig)
#else
void
handle_RETRACT (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_MSG (int sig)
#else
void
handle_MSG (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_SOUND (int sig)
#else
void
handle_SOUND (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_SAK (int sig)
#else
void
handle_SAK (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_PRIO (int sig)
#else
void
handle_PRIO (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_33 (int sig)
#else
void
handle_33 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_34 (int sig)
#else
void
handle_34 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_35 (int sig)
#else
void
handle_35 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_36 (int sig)
#else
void
handle_36 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_37 (int sig)
#else
void
handle_37 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_38 (int sig)
#else
void
handle_38 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_39 (int sig)
#else
void
handle_39 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_40 (int sig)
#else
void
handle_40 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_41 (int sig)
#else
void
handle_41 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_42 (int sig)
#else
void
handle_42 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_43 (int sig)
#else
void
handle_43 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_44 (int sig)
#else
void
handle_44 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_45 (int sig)
#else
void
handle_45 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_46 (int sig)
#else
void
handle_46 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_47 (int sig)
#else
void
handle_47 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_48 (int sig)
#else
void
handle_48 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_49 (int sig)
#else
void
handle_49 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_50 (int sig)
#else
void
handle_50 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_51 (int sig)
#else
void
handle_51 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_52 (int sig)
#else
void
handle_52 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_53 (int sig)
#else
void
handle_53 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_54 (int sig)
#else
void
handle_54 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_55 (int sig)
#else
void
handle_55 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_56 (int sig)
#else
void
handle_56 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_57 (int sig)
#else
void
handle_57 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_58 (int sig)
#else
void
handle_58 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_59 (int sig)
#else
void
handle_59 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_60 (int sig)
#else
void
handle_60 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_61 (int sig)
#else
void
handle_61 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_62 (int sig)
#else
void
handle_62 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_63 (int sig)
#else
void
handle_63 (sig)
     int sig;
#endif
{
}

#ifdef PROTOTYPES
void
handle_TERM (int sig)
#else
void
handle_TERM (sig)
     int sig;
#endif
{
}

/* Functions to send signals.  These also serve as markers.  */
int
gen_ABRT ()
{
  kill (getpid (), SIGABRT);
  return 0;
}  

int
gen_HUP ()
{
#ifdef SIGHUP
  kill (getpid (), SIGHUP);
#else
  handle_HUP (0);
#endif
return 0;
}  

int
gen_QUIT ()
{
#ifdef SIGQUIT
  kill (getpid (), SIGQUIT);
#else
  handle_QUIT (0);
#endif
return 0;
}

int
gen_ILL ()
{
#ifdef SIGILL
  kill (getpid (), SIGILL);
#else
  handle_ILL (0);
#endif
return 0;
}

int
gen_EMT ()
{
#ifdef SIGEMT
  kill (getpid (), SIGEMT);
#else
  handle_EMT (0);
#endif
return 0;
}

int x;

int
gen_FPE ()
{
  /* The intent behind generating SIGFPE this way is to check the mapping
     from the CPU exception itself to the signals.  It would be nice to
     do the same for SIGBUS, SIGSEGV, etc., but I suspect that even this
     test might turn out to be insufficiently portable.  */

#if 0
  /* Loses on the PA because after the signal handler executes we try to
     re-execute the failing instruction again.  Perhaps we could siglongjmp
     out of the signal handler?  */
  /* The expect script looks for the word "kill"; don't delete it.  */
  return 5 / x; /* and we both started jumping up and down yelling kill */
#else
  kill (getpid (), SIGFPE);
#endif
return 0;
}

int
gen_BUS ()
{
#ifdef SIGBUS
  kill (getpid (), SIGBUS);
#else
  handle_BUS (0);
#endif
return 0;
}

int
gen_SEGV ()
{
#ifdef SIGSEGV
  kill (getpid (), SIGSEGV);
#else
  handle_SEGV (0);
#endif
return 0;
}

int
gen_SYS ()
{
#ifdef SIGSYS
  kill (getpid (), SIGSYS);
#else
  handle_SYS (0);
#endif
return 0;
}

int
gen_PIPE ()
{
#ifdef SIGPIPE
  kill (getpid (), SIGPIPE);
#else
  handle_PIPE (0);
#endif
return 0;
}

int
gen_ALRM ()
{
#ifdef SIGALRM
  kill (getpid (), SIGALRM);
#else
  handle_ALRM (0);
#endif
return 0;
}

int
gen_URG ()
{
#ifdef SIGURG
  kill (getpid (), SIGURG);
#else
  handle_URG (0);
#endif
return 0;
}

int
gen_TSTP ()
{
#ifdef SIGTSTP
  kill (getpid (), SIGTSTP);
#else
  handle_TSTP (0);
#endif
return 0;
}

int
gen_CONT ()
{
#ifdef SIGCONT
  kill (getpid (), SIGCONT);
#else
  handle_CONT (0);
#endif
return 0;
}

int
gen_CHLD ()
{
#ifdef SIGCHLD
  kill (getpid (), SIGCHLD);
#else
  handle_CHLD (0);
#endif
return 0;
}

int
gen_TTIN ()
{
#ifdef SIGTTIN
  kill (getpid (), SIGTTIN);
#else
  handle_TTIN (0);
#endif
return 0;
}

int
gen_TTOU ()
{
#ifdef SIGTTOU
  kill (getpid (), SIGTTOU);
#else
  handle_TTOU (0);
#endif
return 0;
}

int
gen_IO ()
{
#ifdef SIGIO
  kill (getpid (), SIGIO);
#else
  handle_IO (0);
#endif
return 0;
}

int
gen_XCPU ()
{
#ifdef SIGXCPU
  kill (getpid (), SIGXCPU);
#else
  handle_XCPU (0);
#endif
return 0;
}

int
gen_XFSZ ()
{
#ifdef SIGXFSZ
  kill (getpid (), SIGXFSZ);
#else
  handle_XFSZ (0);
#endif
return 0;
}

int
gen_VTALRM ()
{
#ifdef SIGVTALRM
  kill (getpid (), SIGVTALRM);
#else
  handle_VTALRM (0);
#endif
return 0;
}

int
gen_PROF ()
{
#ifdef SIGPROF
  kill (getpid (), SIGPROF);
#else
  handle_PROF (0);
#endif
return 0;
}

int
gen_WINCH ()
{
#ifdef SIGWINCH
  kill (getpid (), SIGWINCH);
#else
  handle_WINCH (0);
#endif
return 0;
}

int
gen_LOST ()
{
#if defined(SIGLOST) && (!defined(SIGABRT) || SIGLOST != SIGABRT)
  kill (getpid (), SIGLOST);
#else
  handle_LOST (0);
#endif
return 0;
}

int
gen_USR1 ()
{
#ifdef SIGUSR1
  kill (getpid (), SIGUSR1);
#else
  handle_USR1 (0);
#endif
return 0;
}

int
gen_USR2 ()
{
#ifdef SIGUSR2
  kill (getpid (), SIGUSR2);
#else
  handle_USR2 (0);
#endif
return 0;
}  

int
gen_PWR ()
{
#ifdef SIGPWR
  kill (getpid (), SIGPWR);
#else
  handle_PWR (0);
#endif
return 0;
}

int
gen_POLL ()
{
#if defined (SIGPOLL) && (!defined (SIGIO) || SIGPOLL != SIGIO)
  kill (getpid (), SIGPOLL);
#else
  handle_POLL (0);
#endif
return 0;
}

int
gen_WIND ()
{
#ifdef SIGWIND
  kill (getpid (), SIGWIND);
#else
  handle_WIND (0);
#endif
return 0;
}

int
gen_PHONE ()
{
#ifdef SIGPHONE
  kill (getpid (), SIGPHONE);
#else
  handle_PHONE (0);
#endif
return 0;
}

int
gen_WAITING ()
{
#ifdef SIGWAITING
  kill (getpid (), SIGWAITING);
#else
  handle_WAITING (0);
#endif
return 0;
}

int
gen_LWP ()
{
#ifdef SIGLWP
  kill (getpid (), SIGLWP);
#else
  handle_LWP (0);
#endif
return 0;
}

int
gen_DANGER ()
{
#ifdef SIGDANGER
  kill (getpid (), SIGDANGER);
#else
  handle_DANGER (0);
#endif
return 0;
}

int
gen_GRANT ()
{
#ifdef SIGGRANT
  kill (getpid (), SIGGRANT);
#else
  handle_GRANT (0);
#endif
return 0;
}

int
gen_RETRACT ()
{
#ifdef SIGRETRACT
  kill (getpid (), SIGRETRACT);
#else
  handle_RETRACT (0);
#endif
return 0;
}

int
gen_MSG ()
{
#ifdef SIGMSG
  kill (getpid (), SIGMSG);
#else
  handle_MSG (0);
#endif
return 0;
}

int
gen_SOUND ()
{
#ifdef SIGSOUND
  kill (getpid (), SIGSOUND);
#else
  handle_SOUND (0);
#endif
return 0;
}

int
gen_SAK ()
{
#ifdef SIGSAK
  kill (getpid (), SIGSAK);
#else
  handle_SAK (0);
#endif
return 0;
}

int
gen_PRIO ()
{
#ifdef SIGPRIO
  kill (getpid (), SIGPRIO);
#else
  handle_PRIO (0);
#endif
return 0;
}

int
gen_33 ()
{
#ifdef SIG33
  kill (getpid (), 33);
#else
  handle_33 (0);
#endif
return 0;
}

int
gen_34 ()
{
#ifdef SIG34
  kill (getpid (), 34);
#else
  handle_34 (0);
#endif
return 0;
}

int
gen_35 ()
{
#ifdef SIG35
  kill (getpid (), 35);
#else
  handle_35 (0);
#endif
return 0;
}

int
gen_36 ()
{
#ifdef SIG36
  kill (getpid (), 36);
#else
  handle_36 (0);
#endif
return 0;
}

int
gen_37 ()
{
#ifdef SIG37
  kill (getpid (), 37);
#else
  handle_37 (0);
#endif
return 0;
}

int
gen_38 ()
{
#ifdef SIG38
  kill (getpid (), 38);
#else
  handle_38 (0);
#endif
return 0;
}

int
gen_39 ()
{
#ifdef SIG39
  kill (getpid (), 39);
#else
  handle_39 (0);
#endif
return 0;
}

int
gen_40 ()
{
#ifdef SIG40
  kill (getpid (), 40);
#else
  handle_40 (0);
#endif
return 0;
}

int
gen_41 ()
{
#ifdef SIG41
  kill (getpid (), 41);
#else
  handle_41 (0);
#endif
return 0;
}

int
gen_42 ()
{
#ifdef SIG42
  kill (getpid (), 42);
#else
  handle_42 (0);
#endif
return 0;
}

int
gen_43 ()
{
#ifdef SIG43
  kill (getpid (), 43);
#else
  handle_43 (0);
#endif
return 0;
}

int
gen_44 ()
{
#ifdef SIG44
  kill (getpid (), 44);
#else
  handle_44 (0);
#endif
return 0;
}

int
gen_45 ()
{
#ifdef SIG45
  kill (getpid (), 45);
#else
  handle_45 (0);
#endif
return 0;
}

int
gen_46 ()
{
#ifdef SIG46
  kill (getpid (), 46);
#else
  handle_46 (0);
#endif
return 0;
}

int
gen_47 ()
{
#ifdef SIG47
  kill (getpid (), 47);
#else
  handle_47 (0);
#endif
return 0;
}

int
gen_48 ()
{
#ifdef SIG48
  kill (getpid (), 48);
#else
  handle_48 (0);
#endif
return 0;
}

int
gen_49 ()
{
#ifdef SIG49
  kill (getpid (), 49);
#else
  handle_49 (0);
#endif
return 0;
}

int
gen_50 ()
{
#ifdef SIG50
  kill (getpid (), 50);
#else
  handle_50 (0);
#endif
return 0;
}

int
gen_51 ()
{
#ifdef SIG51
  kill (getpid (), 51);
#else
  handle_51 (0);
#endif
return 0;
}

int
gen_52 ()
{
#ifdef SIG52
  kill (getpid (), 52);
#else
  handle_52 (0);
#endif
return 0;
}

int
gen_53 ()
{
#ifdef SIG53
  kill (getpid (), 53);
#else
  handle_53 (0);
#endif
return 0;
}

int
gen_54 ()
{
#ifdef SIG54
  kill (getpid (), 54);
#else
  handle_54 (0);
#endif
return 0;
}

int
gen_55 ()
{
#ifdef SIG55
  kill (getpid (), 55);
#else
  handle_55 (0);
#endif
return 0;
}

int
gen_56 ()
{
#ifdef SIG56
  kill (getpid (), 56);
#else
  handle_56 (0);
#endif
return 0;
}

int
gen_57 ()
{
#ifdef SIG57
  kill (getpid (), 57);
#else
  handle_57 (0);
#endif
return 0;
}

int
gen_58 ()
{
#ifdef SIG58
  kill (getpid (), 58);
#else
  handle_58 (0);
#endif
return 0;
}

int
gen_59 ()
{
#ifdef SIG59
  kill (getpid (), 59);
#else
  handle_59 (0);
#endif
return 0;
}

int
gen_60 ()
{
#ifdef SIG60
  kill (getpid (), 60);
#else
  handle_60 (0);
#endif
return 0;
}

int
gen_61 ()
{
#ifdef SIG61
  kill (getpid (), 61);
#else
  handle_61 (0);
#endif
return 0;
}

int
gen_62 ()
{
#ifdef SIG62
  kill (getpid (), 62);
#else
  handle_62 (0);
#endif
return 0;
}

int
gen_63 ()
{
#ifdef SIG63
  kill (getpid (), 63);
#else
  handle_63 (0);
#endif
return 0;
}

int
gen_TERM ()
{
  kill (getpid (), SIGTERM);
return 0;
}  

int
main ()
{
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif
  signal (SIGABRT, handle_ABRT);
#ifdef SIGHUP
  signal (SIGHUP, handle_HUP);
#endif
#ifdef SIGQUIT
  signal (SIGQUIT, handle_QUIT);
#endif
#ifdef SIGILL
  signal (SIGILL, handle_ILL);
#endif
#ifdef SIGEMT
  signal (SIGEMT, handle_EMT);
#endif
#ifdef SIGFPE
  signal (SIGFPE, handle_FPE);
#endif
#ifdef SIGBUS
  signal (SIGBUS, handle_BUS);
#endif
#ifdef SIGSEGV
  signal (SIGSEGV, handle_SEGV);
#endif
#ifdef SIGSYS
  signal (SIGSYS, handle_SYS);
#endif
#ifdef SIGPIPE
  signal (SIGPIPE, handle_PIPE);
#endif
#ifdef SIGALRM
  signal (SIGALRM, handle_ALRM);
#endif
#ifdef SIGURG
  signal (SIGURG, handle_URG);
#endif
#ifdef SIGTSTP
  signal (SIGTSTP, handle_TSTP);
#endif
#ifdef SIGCONT
  signal (SIGCONT, handle_CONT);
#endif
#ifdef SIGCHLD
  signal (SIGCHLD, handle_CHLD);
#endif
#ifdef SIGTTIN
  signal (SIGTTIN, handle_TTIN);
#endif
#ifdef SIGTTOU
  signal (SIGTTOU, handle_TTOU);
#endif
#ifdef SIGIO
  signal (SIGIO, handle_IO);
#endif
#ifdef SIGXCPU
  signal (SIGXCPU, handle_XCPU);
#endif
#ifdef SIGXFSZ
  signal (SIGXFSZ, handle_XFSZ);
#endif
#ifdef SIGVTALRM
  signal (SIGVTALRM, handle_VTALRM);
#endif
#ifdef SIGPROF
  signal (SIGPROF, handle_PROF);
#endif
#ifdef SIGWINCH
  signal (SIGWINCH, handle_WINCH);
#endif
#if defined(SIGLOST) && (!defined(SIGABRT) || SIGLOST != SIGABRT)
  signal (SIGLOST, handle_LOST);
#endif
#ifdef SIGUSR1
  signal (SIGUSR1, handle_USR1);
#endif
#ifdef SIGUSR2
  signal (SIGUSR2, handle_USR2);
#endif
#ifdef SIGPWR
  signal (SIGPWR, handle_PWR);
#endif
#if defined (SIGPOLL) && (!defined (SIGIO) || SIGPOLL != SIGIO)
  signal (SIGPOLL, handle_POLL);
#endif
#ifdef SIGWIND
  signal (SIGWIND, handle_WIND);
#endif
#ifdef SIGPHONE
  signal (SIGPHONE, handle_PHONE);
#endif
#ifdef SIGWAITING
  signal (SIGWAITING, handle_WAITING);
#endif
#ifdef SIGLWP
  signal (SIGLWP, handle_LWP);
#endif
#ifdef SIGDANGER
  signal (SIGDANGER, handle_DANGER);
#endif
#ifdef SIGGRANT
  signal (SIGGRANT, handle_GRANT);
#endif
#ifdef SIGRETRACT
  signal (SIGRETRACT, handle_RETRACT);
#endif
#ifdef SIGMSG
  signal (SIGMSG, handle_MSG);
#endif
#ifdef SIGSOUND
  signal (SIGSOUND, handle_SOUND);
#endif
#ifdef SIGSAK
  signal (SIGSAK, handle_SAK);
#endif
#ifdef SIGPRIO
  signal (SIGPRIO, handle_PRIO);
#endif
#ifdef __Lynx__
  /* Lynx doesn't seem to have anything in signal.h for this.  */
  signal (33, handle_33);
  signal (34, handle_34);
  signal (35, handle_35);
  signal (36, handle_36);
  signal (37, handle_37);
  signal (38, handle_38);
  signal (39, handle_39);
  signal (40, handle_40);
  signal (41, handle_41);
  signal (42, handle_42);
  signal (43, handle_43);
  signal (44, handle_44);
  signal (45, handle_45);
  signal (46, handle_46);
  signal (47, handle_47);
  signal (48, handle_48);
  signal (49, handle_49);
  signal (50, handle_50);
  signal (51, handle_51);
  signal (52, handle_52);
  signal (53, handle_53);
  signal (54, handle_54);
  signal (55, handle_55);
  signal (56, handle_56);
  signal (57, handle_57);
  signal (58, handle_58);
  signal (59, handle_59);
  signal (60, handle_60);
  signal (61, handle_61);
  signal (62, handle_62);
  signal (63, handle_63);
#endif /* lynx */
  signal (SIGTERM, handle_TERM);

  x = 0;

  gen_ABRT ();
  gen_HUP ();
  gen_QUIT ();
  gen_ILL ();
  gen_EMT ();
  gen_FPE ();
  gen_BUS ();
  gen_SEGV ();
  gen_SYS ();
  gen_PIPE ();
  gen_ALRM ();
  gen_URG ();
  gen_TSTP ();
  gen_CONT ();
  gen_CHLD ();
  gen_TTIN ();
  gen_TTOU ();
  gen_IO ();
  gen_XCPU ();
  gen_XFSZ ();
  gen_VTALRM ();
  gen_PROF ();
  gen_WINCH ();
  gen_LOST ();
  gen_USR1 ();
  gen_USR2 ();
  gen_PWR ();
  gen_POLL ();
  gen_WIND ();
  gen_PHONE ();
  gen_WAITING ();
  gen_LWP ();
  gen_DANGER ();
  gen_GRANT ();
  gen_RETRACT ();
  gen_MSG ();
  gen_SOUND ();
  gen_SAK ();
  gen_PRIO ();
  gen_33 ();
  gen_34 ();
  gen_35 ();
  gen_36 ();
  gen_37 ();
  gen_38 ();
  gen_39 ();
  gen_40 ();
  gen_41 ();
  gen_42 ();
  gen_43 ();
  gen_44 ();
  gen_45 ();
  gen_46 ();
  gen_47 ();
  gen_48 ();
  gen_49 ();
  gen_50 ();
  gen_51 ();
  gen_52 ();
  gen_53 ();
  gen_54 ();
  gen_55 ();
  gen_56 ();
  gen_57 ();
  gen_58 ();
  gen_59 ();
  gen_60 ();
  gen_61 ();
  gen_62 ();
  gen_63 ();
  gen_TERM ();

  return 0;
}
