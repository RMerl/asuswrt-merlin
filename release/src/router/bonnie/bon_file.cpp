#include <fcntl.h>
#include <sys/types.h>
#ifdef OS2
#define INCL_DOSFILEMGR
#include <os2.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "bon_file.h"
#include "bon_time.h"

CPCCHAR rand_chars = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


COpenTest::COpenTest(int chunk_size, bool use_sync, bool *doExit)
 : m_chunk_size(chunk_size)
 , m_number(0)
 , m_number_directories(1)
 , m_max(0)
 , m_min(0)
 , m_size_range(0)
 , m_dirname(NULL)
 , m_file_name_buf(NULL)
 , m_file_names(NULL)
 , m_sync(use_sync)
 , m_directoryHandles(NULL)
 , m_dirIndex(NULL)
 , m_buf(new char[m_chunk_size])
 , m_exit(doExit)
 , m_sync_dir(true)
{
}

void COpenTest::random_sort()
{
  for(int i = 0; i < m_number; i++)
  {
    char *tmp = m_file_names[i];
    int newind = rand() % m_number;
    m_file_names[i] = m_file_names[newind];
    m_file_names[newind] = tmp;
    if(m_dirIndex)
    {
      int tmpInd = m_dirIndex[i];
      m_dirIndex[i] = m_dirIndex[newind];
      m_dirIndex[newind] = tmpInd;
    }
    if(*m_exit) return;
  }
}

COpenTest::~COpenTest()
{
  int i;
  if(m_dirname)
  {
    fprintf(stderr, "Cleaning up test directory after error.\n");
    if(m_file_names)
    {
      for(i = 0; i < m_number; i++)
        unlink(m_file_names[i]);
    }
    if(m_number_directories > 1)
    {
      char buf[6];
      for(i = 0; i < m_number_directories; i++)
      {
        sprintf(buf, "%05d", i);
        if(rmdir(buf))
          io_error("rmdir");
      }
    }
    if(chdir("..") || rmdir(m_dirname))
      io_error("rmdir");
    delete m_dirname;
  }
  if(m_directoryHandles)
  {
    for(i = 0; i < m_number_directories; i++)
      close(m_directoryHandles[i]);
    delete m_directoryHandles;
  }
  delete m_file_name_buf;
  delete m_file_names;
  delete m_dirIndex;
  delete m_buf;
}

void COpenTest::make_names(bool do_random)
{
  delete m_file_name_buf;
  delete m_file_names;
  int names_per_directory = m_number / m_number_directories;
  int names_in_dir = 0;
  int directory_num = 0;
  if(!m_dirIndex && m_sync)
    m_dirIndex = new int[m_number];
  if(m_number_directories == 1)
  {
    m_file_name_buf = new char[(MaxNameLen + 1) * m_number];
  }
  else
  {
    m_file_name_buf = new char[(MaxNameLen + 1 + 6) * m_number];
  }
  m_file_names = new PCHAR[m_number];
  PCHAR buf = m_file_name_buf;
  for(int i = 0; i < m_number; i++)
  {
    if(*m_exit)
    {
      delete m_file_names;
      m_file_names = NULL;
      return;
    }
    char rand_buf[RandExtraLen + 1];
    int len = rand() % (RandExtraLen + 1);
    int j;
    for(j = 0; j < len; j++)
    {
      rand_buf[j] = rand_chars[rand() % strlen(rand_chars)];
    }
    rand_buf[j] = '\0';
    m_file_names[i] = buf;
    if(m_number_directories != 1)
    {
      sprintf(buf, "%05d/", directory_num);
      buf += strlen(buf);
    }
    if(m_sync)
      m_dirIndex[i] = directory_num;
    names_in_dir++;
    if(names_in_dir > names_per_directory)
    {
      names_in_dir = 0;
      directory_num++;
    }
    if(do_random)
    {
      sprintf(buf, "%s%07d", rand_buf, i);
    }
    else
    {
      sprintf(buf, "%07d%s", i, rand_buf);
    }
    buf += strlen(buf) + 1;
  }
}

int COpenTest::create_a_file(const char *filename, char *buf, int size, int dir)
{
  FILE_TYPE fd = 0;
#ifdef OS2
  ULONG action = 0;
  ULONG rc = DosOpen(filename, &fd, &action, 0, FILE_NORMAL
                   , OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS
                   , OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE
                   , NULL);
#else
  fd = creat(filename, S_IRUSR | S_IWUSR);
#endif
  if(fd == -1)
  {
    fprintf(stderr, "Can't create file %s\n", filename);
    return -1;
  }
  if(m_max)
  {
    for(int i = 0; i < size; i += m_chunk_size)
    {
      int to_write = size - i;
      if(to_write > m_chunk_size) to_write = m_chunk_size;
#ifdef OS2
      ULONG actual = 0;
      if(DosWrite(fd, PVOID(buf), to_write, &actual))
      {
        fprintf(stderr, "Can't write data.\n");
        return -1;
      }
#else
      if(to_write != write(fd, static_cast<void *>(buf), to_write))
      {
        fprintf(stderr, "Can't write data.\n");
        return -1;
      }
#endif
    }
  }
  if(m_sync)
  {
    if(fsync(fd))
    {
      fprintf(stderr, "Can't sync file.\n");
      return -1;
    }
    if(m_sync_dir && fsync(m_directoryHandles[dir]))
    {
      fprintf(stderr, "Can't sync directory, turning off dir-sync.\n");
      m_sync_dir = false;
    }
  }
  file_close(fd);
  return 0;
}

int COpenTest::create_a_link(const char *original, const char *filename, int dir)
{
#ifdef OS2
  fprintf(stderr, "Not supported on OS/2\n");
  return -1;
#else
  if(m_max == -1)
  {
    if(link(original, filename))
    {
      fprintf(stderr, "Can't create link %s\n", filename);
      return -1;
    }
    if(m_sync)
    {
      if(fsync(m_directoryHandles[dir]))
      {
        fprintf(stderr, "Can't sync file.\n");
        return -1;
      }
    }
  }
  else
  {
    if(symlink(original, filename))
    {
      fprintf(stderr, "Can't create symlink %s\n", filename);
      return -1;
    }
    if(m_sync)
    {
      if(fsync(m_directoryHandles[dir]))
      {
        fprintf(stderr, "Can't sync file.\n");
        return -1;
      }
    }
  }
  return 0;
#endif
}

int COpenTest::create(CPCCHAR dirname, BonTimer &timer, int num, int max_size
                    , int min_size, int num_directories, bool do_random)
{
  if(num_directories >= 100000)
  {
    fprintf(stderr, "Can't have more than 99,999 directories.\n");
    return -1;
  }

  m_number = num * DirectoryUnit;
  m_number_directories = num_directories;
  make_names(do_random);
  m_max = max_size;
  m_min = min_size;
  m_size_range = m_max - m_min;
  m_dirname = new char[strlen(dirname) + 1];
  strcpy(m_dirname, dirname);

  if(make_directory(dirname))
  {
    fprintf(stderr, "Can't make directory %s\n", dirname);
    return -1;
  }
  if(chdir(dirname))
  {
    fprintf(stderr, "Can't change to directory %s\n", dirname);
    return -1;
  }
  int i;
  if(m_sync)
    m_directoryHandles = new FILE_TYPE[num_directories];
  if(num_directories > 1)
  {
    for(i = 0; i < num_directories; i++)
    {
      sprintf(m_buf, "%05d", i);
      if(make_directory(m_buf))
      {
        fprintf(stderr, "Can't make directory %s\n", m_buf);
        return -1;
      }
      if(m_sync)
      {
        m_directoryHandles[i] = open(m_buf, O_RDONLY);
        if(m_directoryHandles[i] == -1)
        {
          fprintf(stderr, "Can't get directory handle.\n");
          return -1;
        }
      }
    }
  }
  else if(m_sync)
  {
    m_directoryHandles[0] = open(".", O_RDONLY);
    if(m_directoryHandles[0] == -1)
    {
      fprintf(stderr, "Can't get directory handle.\n");
      return -1;
    }
  }

  timer.timestamp();
  for(i = 0; i < m_number; i++)
  {
    if(*m_exit)
    {
      return EXIT_CTRL_C;
    }
    // m_max < 0 means link or sym-link
    if(m_max < 0)
    {
      if(i == 0)
      {
        if(create_a_file(m_file_names[0], m_buf, 0, m_dirIndex ? m_dirIndex[0] : 0))
          return -1;
      }
      else
      {
        // create_a_link() looks at m_max to see what to do
        if(create_a_link(m_file_names[0], m_file_names[i], m_dirIndex ? m_dirIndex[i] : 0))
          return -1;
      }
    }
    else
    {
      int size;
      if(m_size_range)
        size = m_min + (rand() % (m_size_range + 1));
      else
        size = m_max;
      if(create_a_file(m_file_names[i], m_buf, size, m_dirIndex ? m_dirIndex[i] : 0))
        return -1;
    }
  }
  timer.get_delta_t(do_random ? CreateRand : CreateSeq);
  return 0;
}

int COpenTest::delete_random(BonTimer &timer)
{
  random_sort();
  timer.timestamp();
  int i;
  for(i = 0; i < m_number; i++)
  {
    if(unlink(m_file_names[i]))
    {
      fprintf(stderr, "Can't delete file %s\n", m_file_names[i]);
      return -1;
    }
    if(m_sync && m_sync_dir)
    {
      if(fsync(m_directoryHandles[m_dirIndex[i]]))
      {
        fprintf(stderr, "Can't sync directory, turning off dir-sync.\n");
        m_sync_dir = false;
      }
    }
  }
  if(m_number_directories > 1)
  {
    char buf[6];
    for(i = 0; i < m_number_directories; i++)
    {
      sprintf(buf, "%05d", i);
      if(m_sync)
      {
        close(m_directoryHandles[i]);
      }
      if(rmdir(buf))
      {
        io_error("rmdir");
        return -1;
      }
    }
  }
  else
  {
    if(m_sync)
    {
      close(m_directoryHandles[0]);
    }
  }
  if(chdir("..") || rmdir(m_dirname))
  {
    io_error("rmdir");
    return -1;
  }
  delete m_dirname;
  m_dirname = NULL;
  timer.get_delta_t(DelRand);
  return 0;
}

int COpenTest::delete_sequential(BonTimer &timer)
{
  timer.timestamp();
  int count = 0;
  for(int i = 0; i < m_number_directories; i++)
  {
    char buf[6];
    if(m_number_directories != 1)
    {
      sprintf(buf, "%05d", i);
      if(chdir(buf))
      {
        fprintf(stderr, "Can't change to directory %s\n", buf);
        return -1;
      }
    }
#ifdef OS2
    HDIR d = 0;
    ULONG entries = 1;
    FILEFINDBUF3 findBuf;
    ULONG rc = DosFindFirst("*", &d
                          , FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY
                          , &findBuf, sizeof(findBuf), &entries, FIL_STANDARD);
    if(rc || !entries)
    {
      fprintf(stderr, "Can't open directory.\n");
      return -1;
    }
    do
    {
      if(unlink(findBuf.achName))
      {
        fprintf(stderr, "Can't delete file %s\n", findBuf.achName);
        DosFindClose(d);
        return -1;
      }
      count++;
      rc = DosFindNext(d, &findBuf, sizeof(findBuf), &entries);
    } while(!rc && entries == 1);
    DosFindClose(d);
#else
    DIR *d = opendir(".");
    if(!d)
    {
      fprintf(stderr, "Can't open directory.\n");
      return -1;
    }
    dirent *file_ent;

    while((file_ent = readdir(d)) != NULL)
    {
      if(file_ent->d_name[0] != '.')
      {
        if(unlink(file_ent->d_name))
        {
          fprintf(stderr, "Can't delete file %s\n", file_ent->d_name);
          return -1;
        }


        if(m_sync && m_sync_dir)
        {
          if(fsync(m_directoryHandles[i]))
          {
            fprintf(stderr, "Can't sync directory, turning off dir-sync.\n");
            m_sync_dir = false;
          }
        }
        count++;
      }
    }
    closedir(d);
#endif
    if(m_sync)
    {
      close(m_directoryHandles[i]);
    }
    if(m_number_directories != 1)
    {
      if(chdir("..") || rmdir(buf))
      {
        io_error("rmdir");
        return -1;
      }
    }
  }
  if(chdir("..") || rmdir(m_dirname))
  {
    io_error("rmdir");
    return -1;
  }
  delete m_dirname;
  m_dirname = NULL;
  if(count != m_number)
  {
    fprintf(stderr, "Expected %d files but only got %d\n", m_number, count);
    return -1;
  }
  timer.get_delta_t(DelSeq);
  return 0;
}

int COpenTest::stat_file(CPCCHAR file)
{
  struct stat st;
  if(stat(file, &st))
  {
    fprintf(stderr, "Can't stat file %s\n", file);
    return -1;
  }
  if(st.st_size)
  {
    FILE_TYPE fd = 0;
#ifdef OS2
    ULONG action = 0;
    ULONG rc = DosOpen(file, &fd, &action, 0, FILE_NORMAL
                     , OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS
                     , OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE
                     , NULL);
    if(rc)
      fd = -1;
#else
    fd = open(file, O_RDONLY);
#endif
    if(fd == -1)
    {
      fprintf(stderr, "Can't open file %s\n", file);
      return -1;
    }
    for(int i = 0; i < st.st_size; i += m_chunk_size)
    {
      int to_read = st.st_size - i;
      if(to_read > m_chunk_size) to_read = m_chunk_size;
#ifdef OS2
      ULONG actual = 0;
      rc = DosRead(fd, PVOID(m_buf), to_read, &actual);
      if(to_read != actual || rc)
#else
      if(to_read != read(fd, static_cast<void *>(m_buf), to_read))
#endif
      {
        fprintf(stderr, "Can't read data.\n");
        return -1;
      }
    }
    file_close(fd);
  }
  return 0;
}

int COpenTest::stat_random(BonTimer &timer)
{
  random_sort();
  timer.timestamp();

  int i;
  for(i = 0; i < m_number; i++)
  {
    if(-1 == stat_file(m_file_names[i]))
      return -1;
  }
  timer.get_delta_t(StatRand);
  return 0;
}

int COpenTest::stat_sequential(BonTimer &timer)
{
  timer.timestamp();
  int count = 0;
  for(int i = 0; i < m_number_directories; i++)
  {
    char buf[6];
    if(m_number_directories != 1)
    {
      sprintf(buf, "%05d", i);
      if(chdir(buf))
      {
        fprintf(stderr, "Can't change to directory %s\n", buf);
        return -1;
      }
    }
#ifdef OS2
    HDIR d = 0;
    ULONG entries = 1;
    FILEFINDBUF3 findBuf;
    ULONG rc = DosFindFirst("*", &d
                          , FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY
                          , &findBuf, sizeof(findBuf), &entries, FIL_STANDARD);
    if(rc || !entries)
    {
      fprintf(stderr, "Can't open directory.\n");
      if(m_number_directories != 1)
      {
        if(chdir(".."))
          fprintf(stderr, "Can't chdir().\n");
      }
      return -1;
    }
    do
    {
      if(*m_exit)
      {
        return EXIT_CTRL_C;
      }
      if(findBuf.achName[0] != '.') // our files do not start with a dot
      {
        if(-1 == stat_file(findBuf.achName))
        {
          if(m_number_directories != 1)
          {
            if(chdir(".."))
              fprintf(stderr, "Can't chdir().\n");
          }
          return -1;
        }
        count++;
      }
      rc = DosFindNext(d, &findBuf, sizeof(findBuf), &entries);
    } while(!rc && entries == 1);
    DosFindClose(d);
#else
    DIR *d = opendir(".");
    if(!d)
    {
      fprintf(stderr, "Can't open directory.\n");
      if(m_number_directories != 1)
      {
        if(chdir(".."))
          fprintf(stderr, "Can't chdir().\n");
      }
      return -1;
    }
    dirent *file_ent;
    while((file_ent = readdir(d)) != NULL)
    {
      if(*m_exit)
      {
        if(m_number_directories != 1 && chdir(".."))
        {
          fprintf(stderr, "Can't change to directory ..\n");
          return -1;
        }
        return EXIT_CTRL_C;
      }
      if(file_ent->d_name[0] != '.') // our files do not start with a dot
      {
        if(-1 == stat_file(file_ent->d_name))
        {
          if(m_number_directories != 1)
          {
            if(chdir(".."))
              fprintf(stderr, "Can't chdir().\n");
          }
          return -1;
        }
        count++;
      }
    }
    closedir(d);
#endif
    if(m_number_directories != 1 && chdir(".."))
    {
      fprintf(stderr, "Can't change to directory ..\n");
      return -1;
    }
  }
  if(count != m_number)
  {
    fprintf(stderr, "Expected %d files but only got %d\n", m_number, count);
    return -1;
  }
  timer.get_delta_t(StatSeq);
  return 0;
}

