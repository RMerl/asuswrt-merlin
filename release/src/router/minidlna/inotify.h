#ifdef HAVE_INOTIFY
int
inotify_remove_file(const char * path);

void *
start_inotify();
#endif
