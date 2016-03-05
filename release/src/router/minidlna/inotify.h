int
inotify_insert_file(char * name, const char * path);

int
inotify_insert_directory(int fd, char *name, const char * path);

int
inotify_remove_file(const char * path);

int
inotify_remove_directory(int fd, const char * path);

#ifdef HAVE_INOTIFY
void *
start_inotify();
#endif
