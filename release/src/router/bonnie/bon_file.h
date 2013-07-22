#ifndef BON_IO
#define BON_IO

#include "bonnie.h"
class BonTimer;

typedef unsigned long MASK_TYPE;

class COpenTest
{
public:
  COpenTest(int chunk_size, bool use_sync, bool *doExit);
  ~COpenTest();

  int create(CPCCHAR dirname, BonTimer &timer, int num, int max_size
                    , int min_size, int num_directories, bool do_random);
  int delete_random(BonTimer &timer);
  int delete_sequential(BonTimer &timer);
  int stat_random(BonTimer &timer);
  int stat_sequential(BonTimer &timer);

private:
  void make_names(bool do_random);
  int stat_file(CPCCHAR file);
  int create_a_file(const char *filename, char *buf, int size, int dir);
  int create_a_link(const char *original, const char *filename, int dir);

  const int m_chunk_size;
  int m_number; // the total number of files to create
  int m_number_directories; // the number of directories to store files in
  int m_max; // maximum file size (negative for links)
  int m_min; // minimum file size
  int m_size_range; // m_max - m_min
  char *m_dirname; // name of the master directory
  char *m_file_name_buf; // buffer to store all file names
  char **m_file_names; // pointer to entries in m_file_name_buf
  bool m_sync; // do we sync after every significant operation?
  FILE_TYPE *m_directoryHandles; // handles to the directories for m_sync
  int *m_dirIndex; // which directory we are in
  char *m_buf;
  bool *m_exit;
  bool m_sync_dir;

  void random_sort();

  COpenTest(const COpenTest &t);
  COpenTest & operator =(const COpenTest &t);
};

#endif
