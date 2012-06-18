#ifdef RTCONFIG_USB_TODO
// TODO implement disk check process here

/*
 * part: partion name
 * action: action bitmap
 */

void 
start_diskchk(void)
{
	if(getpid()!=1) {
		notify_rc("start_diskchk");
		return;
	}

	system("diskcheck &");
}

void
stop_diskchk()
{
	if(getpid()!=1) {
		notify_rc("stop_diskchk");
		return;
	}
}

int
diskcheck_main(int argc, char *argv[])
{
	// implment diskcheck process here
	// check file system
	// check swap
}

#endif //RTCONFIG_USB_TODO
