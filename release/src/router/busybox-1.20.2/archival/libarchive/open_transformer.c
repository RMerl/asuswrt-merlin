/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#include "bb_archive.h"

void FAST_FUNC init_transformer_aux_data(transformer_aux_data_t *aux)
{
	memset(aux, 0, sizeof(*aux));
}

int FAST_FUNC check_signature16(transformer_aux_data_t *aux, int src_fd, unsigned magic16)
{
	if (aux && aux->check_signature) {
		uint16_t magic2;
		if (full_read(src_fd, &magic2, 2) != 2 || magic2 != magic16) {
			bb_error_msg("invalid magic");
#if 0 /* possible future extension */
			if (aux->check_signature > 1)
				xfunc_die();
#endif
			return -1;
		}
	}
	return 0;
}

void check_errors_in_children(int signo)
{
	int status;

	if (!signo) {
		/* block waiting for any child */
		if (wait(&status) < 0)
			return; /* probably there are no children */
		goto check_status;
	}

	/* Wait for any child without blocking */
	for (;;) {
		if (wait_any_nohang(&status) < 0)
			/* wait failed?! I'm confused... */
			return;
 check_status:
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			/* this child exited with 0 */
			continue;
		/* Cannot happen?
		if (!WIFSIGNALED(status) && !WIFEXITED(status)) ???; */
		bb_got_signal = 1;
	}
}

/* transformer(), more than meets the eye */
#if BB_MMU
void FAST_FUNC open_transformer(int fd,
	int check_signature,
	IF_DESKTOP(long long) int FAST_FUNC (*transformer)(transformer_aux_data_t *aux, int src_fd, int dst_fd)
)
#else
void FAST_FUNC open_transformer(int fd, const char *transform_prog)
#endif
{
	struct fd_pair fd_pipe;
	int pid;

	xpiped_pair(fd_pipe);
	pid = BB_MMU ? xfork() : xvfork();
	if (pid == 0) {
		/* Child */
		close(fd_pipe.rd); /* we don't want to read from the parent */
		// FIXME: error check?
#if BB_MMU
		{
			transformer_aux_data_t aux;
			init_transformer_aux_data(&aux);
			aux.check_signature = check_signature;
			transformer(&aux, fd, fd_pipe.wr);
			if (ENABLE_FEATURE_CLEAN_UP) {
				close(fd_pipe.wr); /* send EOF */
				close(fd);
			}
			/* must be _exit! bug was actually seen here */
			_exit(EXIT_SUCCESS);
		}
#else
		{
			char *argv[4];
			xmove_fd(fd, 0);
			xmove_fd(fd_pipe.wr, 1);
			argv[0] = (char*)transform_prog;
			argv[1] = (char*)"-cf";
			argv[2] = (char*)"-";
			argv[3] = NULL;
			BB_EXECVP(transform_prog, argv);
			bb_perror_msg_and_die("can't execute '%s'", transform_prog);
		}
#endif
		/* notreached */
	}

	/* parent process */
	close(fd_pipe.wr); /* don't want to write to the child */
	xmove_fd(fd_pipe.rd, fd);
}


#if SEAMLESS_COMPRESSION

/* Used by e.g. rpm which gives us a fd without filename,
 * thus we can't guess the format from filename's extension.
 */
int FAST_FUNC setup_unzip_on_fd(int fd, int fail_if_not_detected)
{
	union {
		uint8_t b[4];
		uint16_t b16[2];
		uint32_t b32[1];
	} magic;
	int offset = -2;
	USE_FOR_MMU(IF_DESKTOP(long long) int FAST_FUNC (*xformer)(transformer_aux_data_t *aux, int src_fd, int dst_fd);)
	USE_FOR_NOMMU(const char *xformer_prog;)

	/* .gz and .bz2 both have 2-byte signature, and their
	 * unpack_XXX_stream wants this header skipped. */
	xread(fd, magic.b16, sizeof(magic.b16[0]));
	if (ENABLE_FEATURE_SEAMLESS_GZ
	 && magic.b16[0] == GZIP_MAGIC
	) {
		USE_FOR_MMU(xformer = unpack_gz_stream;)
		USE_FOR_NOMMU(xformer_prog = "gunzip";)
		goto found_magic;
	}
	if (ENABLE_FEATURE_SEAMLESS_BZ2
	 && magic.b16[0] == BZIP2_MAGIC
	) {
		USE_FOR_MMU(xformer = unpack_bz2_stream;)
		USE_FOR_NOMMU(xformer_prog = "bunzip2";)
		goto found_magic;
	}
	if (ENABLE_FEATURE_SEAMLESS_XZ
	 && magic.b16[0] == XZ_MAGIC1
	) {
		offset = -6;
		xread(fd, magic.b32, sizeof(magic.b32[0]));
		if (magic.b32[0] == XZ_MAGIC2) {
			USE_FOR_MMU(xformer = unpack_xz_stream;)
			USE_FOR_NOMMU(xformer_prog = "unxz";)
			goto found_magic;
		}
	}

	/* No known magic seen */
	if (fail_if_not_detected)
		bb_error_msg_and_die("no gzip"
			IF_FEATURE_SEAMLESS_BZ2("/bzip2")
			IF_FEATURE_SEAMLESS_XZ("/xz")
			" magic");
	xlseek(fd, offset, SEEK_CUR);
	return 1;

 found_magic:
# if BB_MMU
	open_transformer_with_no_sig(fd, xformer);
# else
	/* NOMMU version of open_transformer execs
	 * an external unzipper that wants
	 * file position at the start of the file */
	xlseek(fd, offset, SEEK_CUR);
	open_transformer_with_sig(fd, xformer, xformer_prog);
# endif
	return 0;
}

int FAST_FUNC open_zipped(const char *fname)
{
	char *sfx;
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		return fd;

	sfx = strrchr(fname, '.');
	if (sfx) {
		sfx++;
		if (ENABLE_FEATURE_SEAMLESS_LZMA && strcmp(sfx, "lzma") == 0)
			/* .lzma has no header/signature, just trust it */
			open_transformer_with_sig(fd, unpack_lzma_stream, "unlzma");
		else
		if ((ENABLE_FEATURE_SEAMLESS_GZ && strcmp(sfx, "gz") == 0)
		 || (ENABLE_FEATURE_SEAMLESS_BZ2 && strcmp(sfx, "bz2") == 0)
		 || (ENABLE_FEATURE_SEAMLESS_XZ && strcmp(sfx, "xz") == 0)
		) {
			setup_unzip_on_fd(fd, /*fail_if_not_detected:*/ 1);
		}
	}

	return fd;
}

#endif /* SEAMLESS_COMPRESSION */

void* FAST_FUNC xmalloc_open_zipped_read_close(const char *fname, size_t *maxsz_p)
{
	int fd;
	char *image;

	fd = open_zipped(fname);
	if (fd < 0)
		return NULL;

	image = xmalloc_read(fd, maxsz_p);
	if (!image)
		bb_perror_msg("read error from '%s'", fname);
	close(fd);

	return image;
}
