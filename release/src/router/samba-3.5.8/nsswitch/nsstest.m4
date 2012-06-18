case "$host_os" in
	*linux*)
		SMB_ENABLE(nsstest,YES)
	;;
	*)
		SMB_ENABLE(nsstest,NO)
	;;
esac
