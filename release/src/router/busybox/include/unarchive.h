/* vi: set sw=4 ts=4: */
#ifndef UNARCHIVE_H
#define UNARCHIVE_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

enum {
#if BB_BIG_ENDIAN
	COMPRESS_MAGIC = 0x1f9d,
	GZIP_MAGIC  = 0x1f8b,
	BZIP2_MAGIC = 'B' * 256 + 'Z',
	/* .xz signature: 0xfd, '7', 'z', 'X', 'Z', 0x00 */
	/* More info at: http://tukaani.org/xz/xz-file-format.txt */
	XZ_MAGIC1   = 0xfd * 256 + '7',
	XZ_MAGIC2   = (('z' * 256 + 'X') * 256 + 'Z') * 256 + 0,
	/* Different form: 32 bits, then 16 bits: */
	XZ_MAGIC1a  = ((0xfd * 256 + '7') * 256 + 'z') * 256 + 'X',
	XZ_MAGIC2a  = 'Z' * 256 + 0,
#else
	COMPRESS_MAGIC = 0x9d1f,
	GZIP_MAGIC  = 0x8b1f,
	BZIP2_MAGIC = 'Z' * 256 + 'B',
	XZ_MAGIC1   = '7' * 256 + 0xfd,
	XZ_MAGIC2   = ((0 * 256 + 'Z') * 256 + 'X') * 256 + 'z',
	XZ_MAGIC1a  = (('X' * 256 + 'z') * 256 + '7') * 256 + 0xfd,
	XZ_MAGIC2a  = 0 * 256 + 'Z',
#endif
};

typedef struct file_header_t {
	char *name;
	char *link_target;
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	char *tar__uname;
	char *tar__gname;
#endif
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
} file_header_t;

struct hardlinks_t;

typedef struct archive_handle_t {
	/* Flags. 1st since it is most used member */
	unsigned ah_flags;

	/* The raw stream as read from disk or stdin */
	int src_fd;

	/* Define if the header and data component should be processed */
	char FAST_FUNC (*filter)(struct archive_handle_t *);
	/* List of files that have been accepted */
	llist_t *accept;
	/* List of files that have been rejected */
	llist_t *reject;
	/* List of files that have successfully been worked on */
	llist_t *passed;

	/* Currently processed file's header */
	file_header_t *file_header;

	/* Process the header component, e.g. tar -t */
	void FAST_FUNC (*action_header)(const file_header_t *);

	/* Process the data component, e.g. extract to filesystem */
	void FAST_FUNC (*action_data)(struct archive_handle_t *);

	/* Function that skips data */
	void FAST_FUNC (*seek)(int fd, off_t amount);

	/* Count processed bytes */
	off_t offset;

	/* Archiver specific. Can make it a union if it ever gets big */
#if ENABLE_TAR || ENABLE_DPKG || ENABLE_DPKG_DEB
	smallint tar__end;
# if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	char* tar__longname;
	char* tar__linkname;
# endif
#if ENABLE_FEATURE_TAR_TO_COMMAND
	char* tar__to_command;
#endif
# if ENABLE_FEATURE_TAR_SELINUX
	char* tar__global_sctx;
	char* tar__next_file_sctx;
# endif
#endif
#if ENABLE_CPIO || ENABLE_RPM2CPIO || ENABLE_RPM
	uoff_t cpio__blocks;
	struct hardlinks_t *cpio__hardlinks_to_create;
	struct hardlinks_t *cpio__created_hardlinks;
#endif
#if ENABLE_DPKG || ENABLE_DPKG_DEB
	/* Temporary storage */
	char *dpkg__buffer;
	/* How to process any sub archive, e.g. get_header_tar_gz */
	char FAST_FUNC (*dpkg__action_data_subarchive)(struct archive_handle_t *);
	/* Contains the handle to a sub archive */
	struct archive_handle_t *dpkg__sub_archive;
#endif
#if ENABLE_FEATURE_AR_CREATE
	const char *ar__name;
	struct archive_handle_t *ar__out;
#endif
} archive_handle_t;
/* bits in ah_flags */
#define ARCHIVE_RESTORE_DATE        (1 << 0)
#define ARCHIVE_CREATE_LEADING_DIRS (1 << 1)
#define ARCHIVE_UNLINK_OLD          (1 << 2)
#define ARCHIVE_EXTRACT_QUIET       (1 << 3)
#define ARCHIVE_EXTRACT_NEWER       (1 << 4)
#define ARCHIVE_DONT_RESTORE_OWNER  (1 << 5)
#define ARCHIVE_DONT_RESTORE_PERM   (1 << 6)
#define ARCHIVE_NUMERIC_OWNER       (1 << 7)
#define ARCHIVE_O_TRUNC             (1 << 8)


/* POSIX tar Header Block, from POSIX 1003.1-1990  */
#define TAR_BLOCK_SIZE 512
#define NAME_SIZE      100
#define NAME_SIZE_STR "100"
typedef struct tar_header_t {       /* byte offset */
	char name[NAME_SIZE];     /*   0-99 */
	char mode[8];             /* 100-107 */
	char uid[8];              /* 108-115 */
	char gid[8];              /* 116-123 */
	char size[12];            /* 124-135 */
	char mtime[12];           /* 136-147 */
	char chksum[8];           /* 148-155 */
	char typeflag;            /* 156-156 */
	char linkname[NAME_SIZE]; /* 157-256 */
	/* POSIX:   "ustar" NUL "00" */
	/* GNU tar: "ustar  " NUL */
	/* Normally it's defined as magic[6] followed by
	 * version[2], but we put them together to save code.
	 */
	char magic[8];            /* 257-264 */
	char uname[32];           /* 265-296 */
	char gname[32];           /* 297-328 */
	char devmajor[8];         /* 329-336 */
	char devminor[8];         /* 337-344 */
	char prefix[155];         /* 345-499 */
	char padding[12];         /* 500-512 (pad to exactly TAR_BLOCK_SIZE) */
} tar_header_t;
struct BUG_tar_header {
	char c[sizeof(tar_header_t) == TAR_BLOCK_SIZE ? 1 : -1];
};



/* Info struct unpackers can fill out to inform users of thing like
 * timestamps of unpacked files */
typedef struct unpack_info_t {
	time_t mtime;
} unpack_info_t;

extern archive_handle_t *init_handle(void) FAST_FUNC;

extern char filter_accept_all(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_list(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_list_reassign(archive_handle_t *archive_handle) FAST_FUNC;
extern char filter_accept_reject_list(archive_handle_t *archive_handle) FAST_FUNC;

extern void unpack_ar_archive(archive_handle_t *ar_archive) FAST_FUNC;

extern void data_skip(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_all(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_to_stdout(archive_handle_t *archive_handle) FAST_FUNC;
extern void data_extract_to_command(archive_handle_t *archive_handle) FAST_FUNC;

extern void header_skip(const file_header_t *file_header) FAST_FUNC;
extern void header_list(const file_header_t *file_header) FAST_FUNC;
extern void header_verbose_list(const file_header_t *file_header) FAST_FUNC;

extern char get_header_ar(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_cpio(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_gz(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_bz2(archive_handle_t *archive_handle) FAST_FUNC;
extern char get_header_tar_lzma(archive_handle_t *archive_handle) FAST_FUNC;

extern void seek_by_jump(int fd, off_t amount) FAST_FUNC;
extern void seek_by_read(int fd, off_t amount) FAST_FUNC;

extern void data_align(archive_handle_t *archive_handle, unsigned boundary) FAST_FUNC;
extern const llist_t *find_list_entry(const llist_t *list, const char *filename) FAST_FUNC;
extern const llist_t *find_list_entry2(const llist_t *list, const char *filename) FAST_FUNC;

/* A bit of bunzip2 internals are exposed for compressed help support: */
typedef struct bunzip_data bunzip_data;
int start_bunzip(bunzip_data **bdp, int in_fd, const unsigned char *inbuf, int len) FAST_FUNC;
int read_bunzip(bunzip_data *bd, char *outbuf, int len) FAST_FUNC;
void dealloc_bunzip(bunzip_data *bd) FAST_FUNC;

typedef struct inflate_unzip_result {
	off_t bytes_out;
	uint32_t crc;
} inflate_unzip_result;

IF_DESKTOP(long long) int inflate_unzip(inflate_unzip_result *res, off_t compr_size, int src_fd, int dst_fd) FAST_FUNC;
/* xz unpacker takes .xz stream from offset 6 */
IF_DESKTOP(long long) int unpack_xz_stream(int src_fd, int dst_fd) FAST_FUNC;
/* lzma unpacker takes .lzma stream from offset 0 */
IF_DESKTOP(long long) int unpack_lzma_stream(int src_fd, int dst_fd) FAST_FUNC;
/* the rest wants 2 first bytes already skipped by the caller */
IF_DESKTOP(long long) int unpack_bz2_stream(int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_gz_stream(int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_gz_stream_with_info(int src_fd, int dst_fd, unpack_info_t *info) FAST_FUNC;
IF_DESKTOP(long long) int unpack_Z_stream(int src_fd, int dst_fd) FAST_FUNC;
/* wrapper which checks first two bytes to be "BZ" */
IF_DESKTOP(long long) int unpack_bz2_stream_prime(int src_fd, int dst_fd) FAST_FUNC;

char* append_ext(char *filename, const char *expected_ext) FAST_FUNC;
int bbunpack(char **argv,
	    IF_DESKTOP(long long) int FAST_FUNC (*unpacker)(unpack_info_t *info),
	    char* FAST_FUNC (*make_new_name)(char *filename, const char *expected_ext),
	    const char *expected_ext
) FAST_FUNC;

#if BB_MMU
void open_transformer(int fd,
	IF_DESKTOP(long long) int FAST_FUNC (*transformer)(int src_fd, int dst_fd)) FAST_FUNC;
#define open_transformer(fd, transformer, transform_prog) open_transformer(fd, transformer)
#else
void open_transformer(int src_fd, const char *transform_prog) FAST_FUNC;
#define open_transformer(fd, transformer, transform_prog) open_transformer(fd, transform_prog)
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
