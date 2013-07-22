#ifndef BON_FILE
#define BON_FILE

#include "bonnie.h"
class Semaphore;
class BonTimer;

class CFileOp
{
public:
  CFileOp(BonTimer &timer, int file_size, int chunk_bits, bool use_sync = false, bool use_direct_io = false);
  int open(CPCCHAR base_name, bool create, bool use_fopen = false);
  ~CFileOp();
  int write_block_putc();
  int write_block(PVOID buf);
  int read_block_getc(char *buf);
  int read_block(PVOID buf);
  int seek(int offset, int whence);
  int doseek(long where, bool update);
  int seek_test(bool quiet, Semaphore &s);
  void close();
  // reopen a file, bools for whether the file should be unlink()'d and
  // creat()'d and for whether fopen should be used
  int reopen(bool create, bool use_fopen = false);
  BonTimer &getTimer() { return m_timer; }
  int chunks() const { return m_total_chunks; }
private:
  int m_open(CPCCHAR base_name, int ind, bool create);

  BonTimer &m_timer;
  FILE **m_stream;
  FILE_TYPE *m_fd;
  bool m_isopen;
  char *m_name;
  bool m_sync;
  bool m_use_direct_io;
  const int m_chunk_bits, m_chunk_size;
  int m_chunks_per_file, m_total_chunks;
  int m_last_file_chunks;
  int m_cur_pos;
  int m_file_ind;
  int m_file_size;
  int m_num_files;
  CFileOp(const CFileOp &f);
  CFileOp & operator =(const CFileOp &f);
  char *m_buf;
};


#endif
