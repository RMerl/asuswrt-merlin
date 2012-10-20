#ifdef RTCONFIG_USB_TODO
// TODO implement disk check process here

/*
 * part: partion name
 * action: action bitmap
 */

void 
start_diskchk(void)
{
	char *diskcheck_argv[] = { "diskcheck", NULL };
	pid_t pid;

	if(getpid()!=1) {
		notify_rc("start_diskchk");
		return;
	}

	eval(diskcheck_argv, NULL, 0, &pid);
}

void
stop_diskchk()
{
	if(getpid()!=1) {
		notify_rc("stop_diskchk");
		return;
	}

	killall_tk("diskcheck");
}

int
diskcheck_main(int argc, char *argv[])
{
	// implment diskcheck process here
	// check file system
	// check swap

	return 0;
}

#endif //RTCONFIG_USB_TODO
