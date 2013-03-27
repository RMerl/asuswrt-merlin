
int smbc_wrapper_opendir(connection* con, const char *durl);

struct smbc_dirent* smbc_wrapper_readdir(connection* con, unsigned int dh);

int smbc_wrapper_closedir(connection* con, int dh);


int smbc_wrapper_stat(connection* con, const char *url, struct stat *st);


int smbc_wrapper_unlink(connection* con, const char *furl);


uint32_t smbc_wrapper_rmdir(connection* con, const char *dname);


uint32_t smbc_wrapper_mkdir(connection* con, const char *fname, mode_t mode);


uint32_t smbc_wrapper_rename(connection* con, char *src, char *dst);


int smbc_wrapper_open(connection* con, const char *furl, int flags, mode_t mode);


int smbc_wrapper_close(connection* con, int fd);


size_t smbc_wrapper_read(connection* con, int fd, void *buf, size_t bufsize);


size_t smbc_wrapper_write(connection* con, int fd, const void *buf, size_t bufsize, uint16_t write_mode );

void process_share_link_for_router_sync_use();




