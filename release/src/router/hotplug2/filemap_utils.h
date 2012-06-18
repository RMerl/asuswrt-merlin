/*****************************************************************************\
*  _  _       _          _              ___                                   *
* | || | ___ | |_  _ __ | | _  _  __ _ |_  )                                  *
* | __ |/ _ \|  _|| '_ \| || || |/ _` | / /                                   *
* |_||_|\___/ \__|| .__/|_| \_,_|\__, |/___|                                  *
*                 |_|            |___/                                        *
\*****************************************************************************/

#ifndef FILEMAP_UTILS_H
#define FILEMAP_UTILS_H 1
struct filemap_t {
	int fd;
	off_t size;
	void *map;
};

int map_file(const char *, struct filemap_t *);
int unmap_file(struct filemap_t *);
#endif
