/* vt_mode.h */
/*
support vtp-sessions

written by Christian A. Lademann <cal@zls.com>
*/

/*
02.05.95:cal:ported to samba-1.9.13
*/

#ifndef	__vt_mode_h__
#	define	__vt_mode_h__

#	define	VT_CLOSED	0
#	define	VT_OPEN		1

#	define	MS_NONE		0
#	define	MS_PTY		1
#	define	MS_STREAM	2
#	define	MS_VTY		3

#	define	VT_MAXREAD	32


#	undef	EXTERN

#	ifndef __vt_mode_c__
#		define	EXTERN	extern
#		define	DEFAULT(v)
#	else
#		define	EXTERN
#		define	DEFAULT(v)	=(v)
#	endif

	EXTERN int	VT_Status		DEFAULT(VT_CLOSED),
				VT_Fd			DEFAULT(-1),
				VT_ChildPID		DEFAULT(-1);

	EXTERN BOOL	VT_Mode			DEFAULT(False),
				VT_ChildDied	DEFAULT(False);

	EXTERN char	*VT_Line		DEFAULT(NULL);

#	undef	EXTERN


#endif /* __vt_mode_h__ */
