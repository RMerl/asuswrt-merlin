################################################
# Start MODULE sys_notify_inotify
[MODULE::sys_notify_inotify]
SUBSYSTEM = sys_notify
INIT_FUNCTION = sys_notify_inotify_init
PRIVATE_DEPENDENCIES = LIBEVENTS
# End MODULE sys_notify_inotify
################################################

sys_notify_inotify_OBJ_FILES = $(ntvfssrcdir)/sysdep/inotify.o

################################################
# Start SUBSYSTEM sys_notify
[SUBSYSTEM::sys_notify]
# End SUBSYSTEM sys_notify
################################################

sys_notify_OBJ_FILES = $(ntvfssrcdir)/sysdep/sys_notify.o

[SUBSYSTEM::sys_lease_linux]
PRIVATE_DEPENDENCIES = LIBTEVENT

sys_lease_linux_OBJ_FILES = $(ntvfssrcdir)/sysdep/sys_lease_linux.o

[SUBSYSTEM::sys_lease]

sys_lease_OBJ_FILES = $(ntvfssrcdir)/sysdep/sys_lease.o
