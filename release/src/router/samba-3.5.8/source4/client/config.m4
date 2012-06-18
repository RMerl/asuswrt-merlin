case "$host_os" in
  *linux*)
  	SMB_ENABLE(mount.cifs, YES)
  	SMB_ENABLE(umount.cifs, YES)
  	SMB_ENABLE(cifs.upcall, NO) # Disabled for now
	;;
   *)
  	SMB_ENABLE(mount.cifs, NO)
  	SMB_ENABLE(umount.cifs, NO)
  	SMB_ENABLE(cifs.upcall, NO)
	;;
esac

