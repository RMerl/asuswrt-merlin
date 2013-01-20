/* vi: set sw=4 ts=4: */
/*
 * Stripped down version of net-tools for busybox.
 *
 * Author: Ignacio Garcia Perez (iggarpe at gmail dot com)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * There are some differences from the standard net-tools slattach:
 *
 * - The -l option is not supported.
 *
 * - The -F options allows disabling of RTS/CTS flow control.
 */

//usage:#define slattach_trivial_usage
//usage:       "[-cehmLF] [-s SPEED] [-p PROTOCOL] DEVICE"
//usage:#define slattach_full_usage "\n\n"
//usage:       "Attach network interface(s) to serial line(s)\n"
//usage:     "\n	-p PROT	Set protocol (slip, cslip, slip6, clisp6 or adaptive)"
//usage:     "\n	-s SPD	Set line speed"
//usage:     "\n	-e	Exit after initializing device"
//usage:     "\n	-h	Exit when the carrier is lost"
//usage:     "\n	-c PROG	Run PROG when the line is hung up"
//usage:     "\n	-m	Do NOT initialize the line in raw 8 bits mode"
//usage:     "\n	-L	Enable 3-wire operation"
//usage:     "\n	-F	Disable RTS/CTS flow control"

#include "libbb.h"
#include "libiproute/utils.h" /* invarg() */

struct globals {
	int handle;
	int saved_disc;
	struct termios saved_state;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define handle       (G.handle      )
#define saved_disc   (G.saved_disc  )
#define saved_state  (G.saved_state )
#define INIT_G() do { } while (0)


/*
 * Save tty state and line discipline
 *
 * It is fine here to bail out on errors, since we haven modified anything yet
 */
static void save_state(void)
{
	/* Save line status */
	if (tcgetattr(handle, &saved_state) < 0)
		bb_perror_msg_and_die("get state");

	/* Save line discipline */
	xioctl(handle, TIOCGETD, &saved_disc);
}

static int set_termios_state_or_warn(struct termios *state)
{
	int ret;

	ret = tcsetattr(handle, TCSANOW, state);
	if (ret < 0) {
		bb_perror_msg("set state");
		return 1; /* used as exitcode */
	}
	return 0;
}

/*
 * Restore state and line discipline for ALL managed ttys
 *
 * Restoring ALL managed ttys is the only way to have a single
 * hangup delay.
 *
 * Go on after errors: we want to restore as many controlled ttys
 * as possible.
 */
static void restore_state_and_exit(int exitcode) NORETURN;
static void restore_state_and_exit(int exitcode)
{
	struct termios state;

	/* Restore line discipline */
	if (ioctl_or_warn(handle, TIOCSETD, &saved_disc) < 0) {
		exitcode = 1;
	}

	/* Hangup */
	memcpy(&state, &saved_state, sizeof(state));
	cfsetispeed(&state, B0);
	cfsetospeed(&state, B0);
	if (set_termios_state_or_warn(&state))
		exitcode = 1;
	sleep(1);

	/* Restore line status */
	if (set_termios_state_or_warn(&saved_state))
		exit(EXIT_FAILURE);
	if (ENABLE_FEATURE_CLEAN_UP)
		close(handle);

	exit(exitcode);
}

/*
 * Set tty state, line discipline and encapsulation
 */
static void set_state(struct termios *state, int encap)
{
	int disc;

	/* Set line status */
	if (set_termios_state_or_warn(state))
		goto bad;
	/* Set line discliple (N_SLIP always) */
	disc = N_SLIP;
	if (ioctl_or_warn(handle, TIOCSETD, &disc) < 0) {
		goto bad;
	}

	/* Set encapsulation (SLIP, CSLIP, etc) */
	if (ioctl_or_warn(handle, SIOCSIFENCAP, &encap) < 0) {
 bad:
		restore_state_and_exit(EXIT_FAILURE);
	}
}

static void sig_handler(int signo UNUSED_PARAM)
{
	restore_state_and_exit(EXIT_SUCCESS);
}

int slattach_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int slattach_main(int argc UNUSED_PARAM, char **argv)
{
	/* Line discipline code table */
	static const char proto_names[] ALIGN1 =
		"slip\0"        /* 0 */
		"cslip\0"       /* 1 */
		"slip6\0"       /* 2 */
		"cslip6\0"      /* 3 */
		"adaptive\0"    /* 8 */
		;

	int i, encap, opt;
	struct termios state;
	const char *proto = "cslip";
	const char *extcmd;   /* Command to execute after hangup */
	const char *baud_str;
	int baud_code = -1;   /* Line baud rate (system code) */

	enum {
		OPT_p_proto  = 1 << 0,
		OPT_s_baud   = 1 << 1,
		OPT_c_extcmd = 1 << 2,
		OPT_e_quit   = 1 << 3,
		OPT_h_watch  = 1 << 4,
		OPT_m_nonraw = 1 << 5,
		OPT_L_local  = 1 << 6,
		OPT_F_noflow = 1 << 7
	};

	INIT_G();

	/* Parse command line options */
	opt = getopt32(argv, "p:s:c:ehmLF", &proto, &baud_str, &extcmd);
	/*argc -= optind;*/
	argv += optind;

	if (!*argv)
		bb_show_usage();

	encap = index_in_strings(proto_names, proto);

	if (encap < 0)
		invarg(proto, "protocol");
	if (encap > 3)
		encap = 8;

	/* We want to know if the baud rate is valid before we start touching the ttys */
	if (opt & OPT_s_baud) {
		baud_code = tty_value_to_baud(xatoi(baud_str));
		if (baud_code < 0)
			invarg(baud_str, "baud rate");
	}

	/* Trap signals in order to restore tty states upon exit */
	if (!(opt & OPT_e_quit)) {
		bb_signals(0
			+ (1 << SIGHUP)
			+ (1 << SIGINT)
			+ (1 << SIGQUIT)
			+ (1 << SIGTERM)
			, sig_handler);
	}

	/* Open tty */
	handle = open(*argv, O_RDWR | O_NDELAY);
	if (handle < 0) {
		char *buf = concat_path_file("/dev", *argv);
		handle = xopen(buf, O_RDWR | O_NDELAY);
		/* maybe if (ENABLE_FEATURE_CLEAN_UP) ?? */
		free(buf);
	}

	/* Save current tty state */
	save_state();

	/* Configure tty */
	memcpy(&state, &saved_state, sizeof(state));
	if (!(opt & OPT_m_nonraw)) { /* raw not suppressed */
		memset(&state.c_cc, 0, sizeof(state.c_cc));
		state.c_cc[VMIN] = 1;
		state.c_iflag = IGNBRK | IGNPAR;
		state.c_oflag = 0;
		state.c_lflag = 0;
		state.c_cflag = CS8 | HUPCL | CREAD
		              | ((opt & OPT_L_local) ? CLOCAL : 0)
		              | ((opt & OPT_F_noflow) ? 0 : CRTSCTS);
		cfsetispeed(&state, cfgetispeed(&saved_state));
		cfsetospeed(&state, cfgetospeed(&saved_state));
	}

	if (opt & OPT_s_baud) {
		cfsetispeed(&state, baud_code);
		cfsetospeed(&state, baud_code);
	}

	set_state(&state, encap);

	/* Exit now if option -e was passed */
	if (opt & OPT_e_quit)
		return 0;

	/* If we're not requested to watch, just keep descriptor open
	 * until we are killed */
	if (!(opt & OPT_h_watch))
		while (1)
			sleep(24*60*60);

	/* Watch line for hangup */
	while (1) {
		if (ioctl(handle, TIOCMGET, &i) < 0 || !(i & TIOCM_CAR))
			goto no_carrier;
		sleep(15);
	}

 no_carrier:

	/* Execute command on hangup */
	if (opt & OPT_c_extcmd)
		system(extcmd);

	/* Restore states and exit */
	restore_state_and_exit(EXIT_SUCCESS);
}
