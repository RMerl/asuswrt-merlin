
/*
 * COPYRIGHT NOTICE:
 * Copyright (c) Tim Bray, 1990.
 * Copyright (c) Russell Coker, 1999.  I have updated the program, added
 * support for >2G on 32bit machines, and tests for file creation.
 * Licensed under the GPL version 2.0.
 * DISCLAIMER:
 * This program is provided AS IS with no warranty of any kind, and
 * The author makes no representation with respect to the adequacy of this
 *  program for any particular purpose or with respect to its adequacy to
 *  produce any particular result, and
 * The author shall not be liable for loss or damage arising out of
 *  the use of this program regardless of how sustained, and
 * In no event shall the author be liable for special, direct, indirect
 *  or consequential damage, loss, costs or fees or expenses of any
 *  nature or kind.
 */

#ifdef OS2
#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSQUEUES
#define INCL_DOSPROCESS
#include <os2.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include "bonnie.h"
#include "bon_io.h"
#include "bon_file.h"
#include "bon_time.h"
#include "semaphore.h"
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <string.h>
#include <sys/utsname.h>
#include <signal.h>

#ifdef AIX_MEM_SIZE
#include <cf.h>
#include <sys/cfgodm.h>
#include <sys/cfgdb.h>
#endif

void usage();

class CGlobalItems
{
public:
  bool quiet;
  bool fast;
  bool sync_bonnie;
  bool use_direct_io;
  BonTimer timer;
  int ram;
  Semaphore sem;
  char *name;
  bool bufSync;
  int  chunk_bits;
  int chunk_size() const { return m_chunk_size; }
  bool *doExit;
  void set_chunk_size(int size)
    { delete m_buf; pa_new(size, m_buf, m_buf_pa); m_chunk_size = size; }

  // Return the page-aligned version of the local buffer
  char *buf() { return m_buf_pa; }

  CGlobalItems(bool *exitFlag);
  ~CGlobalItems() { delete name; delete m_buf; }

  void decrement_and_wait(int nr_sem);

  void SetName(CPCCHAR path)
  {
    delete name;
    name = new char[strlen(path) + 15];
#ifdef OS2
    ULONG myPid = 0;
    DosQuerySysInfo(QSV_FOREGROUND_PROCESS, QSV_FOREGROUND_PROCESS
                  , &myPid, sizeof(myPid));
#else
    pid_t myPid = getpid();
#endif
    sprintf(name, "%s/Bonnie.%d", path, int(myPid));
  }
private:
  int m_chunk_size;
  char *m_buf;     // Pointer to the entire buffer
  char *m_buf_pa;  // Pointer to the page-aligned version of the same buffer

  CGlobalItems(const CGlobalItems &f);
  CGlobalItems & operator =(const CGlobalItems &f);

  // Implement a page-aligned version of new.
  // 'p' is the pointer created
  // 'page_aligned_p' is the page-aligned pointer created
  void pa_new(unsigned int num_bytes, char *&p, char *&page_aligned_p)
  {
#ifdef NON_UNIX
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    long page_size = system_info.dwPageSize;
#else
    int page_size = getpagesize();
#endif
    p = ::new char [num_bytes + page_size];

    page_aligned_p = (char *)((((unsigned long)p + page_size - 1) / page_size) * page_size);
  }
};

CGlobalItems::CGlobalItems(bool *exitFlag)
 : quiet(false)
 , fast(false)
 , sync_bonnie(false)
 , use_direct_io(false)
 , timer()
 , ram(0)
 , sem(SemKey, TestCount)
 , name(NULL)
 , bufSync(false)
 , chunk_bits(DefaultChunkBits)
 , doExit(exitFlag)
 , m_chunk_size(DefaultChunkSize)
 , m_buf(NULL)
 , m_buf_pa(NULL)
{
  pa_new(m_chunk_size, m_buf, m_buf_pa);
  SetName(".");
}

void CGlobalItems::decrement_and_wait(int nr_sem)
{
  if(sem.decrement_and_wait(nr_sem))
    exit(1);
}

int TestDirOps(int directory_size, int max_size, int min_size
             , int num_directories, CGlobalItems &globals);
int TestFileOps(int file_size, CGlobalItems &globals);

static bool exitNow;
static bool already_printed_error;

#ifdef USE_SA_SIGACTION
#define SIGNAL_NUMBER siginf->si_signo
#else
#define SIGNAL_NUMBER sig
#endif

extern "C"
{
  void ctrl_c_handler(int sig
#ifdef USE_SA_SIGACTION
		    , siginfo_t *siginf, void *unused
#endif
		     )
  {
    if(SIGNAL_NUMBER == SIGXCPU)
      fprintf(stderr, "Exceeded CPU usage.\n");
    else if(SIGNAL_NUMBER == SIGXFSZ)
      fprintf(stderr, "exceeded file storage limits.\n");
    exitNow = true;
  }
}

int main(int argc, char *argv[])
{
  int    file_size = DefaultFileSize;
  int    directory_size = DefaultDirectorySize;
  int    directory_max_size = DefaultDirectoryMaxSize;
  int    directory_min_size = DefaultDirectoryMinSize;
  int    num_bonnie_procs = 0;
  int    num_directories = 1;
  int    count = -1;
  const char * machine = NULL;
  char *userName = NULL, *groupName = NULL;
  CGlobalItems globals(&exitNow);
  bool setSize = false;

  exitNow = false;
  already_printed_error = false;

  struct sigaction sa;
#ifdef USE_SA_SIGACTION
  sa.sa_sigaction = &ctrl_c_handler;
  sa.sa_flags = SA_RESETHAND | SA_SIGINFO;
#else
  sa.sa_handler = ctrl_c_handler;
  sa.sa_flags = SA_RESETHAND;
#endif
  if(sigaction(SIGINT, &sa, NULL)
   || sigaction(SIGXCPU, &sa, NULL)
   || sigaction(SIGXFSZ, &sa, NULL))
  {
    printf("Can't handle SIGINT.\n");
    return 1;
  }
#ifdef USE_SA_SIGACTION
  sa.sa_sigaction = NULL;
#endif
  sa.sa_handler = SIG_IGN;
  if(sigaction(SIGHUP, &sa, NULL))
  {
    printf("Can't handle SIGHUP.\n");
    return 1;
  }

#ifdef _SC_PHYS_PAGES
  int page_size = sysconf(_SC_PAGESIZE);
  int num_pages = sysconf(_SC_PHYS_PAGES);
  if(page_size != -1 && num_pages != -1)
  {
    globals.ram = page_size/1024 * (num_pages/1024);
  }
#else

#ifdef AIX_MEM_SIZE
  struct CuAt *odm_obj;
  int how_many;

  odm_set_path("/etc/objrepos");
  odm_obj = getattr("sys0", "realmem", 0, &how_many);
  globals.ram = atoi(odm_obj->value) / 1024;
  odm_terminate();
  printf("Memory = %d MiB\n", globals.ram);
#endif

#endif

  int int_c;
  while(-1 != (int_c = getopt(argc, argv, "bd:fg:m:n:p:qr:s:u:x:yD")) )
  {
    switch(char(int_c))
    {
      case '?':
      case ':':
        usage();
      break;
      case 'b':
        globals.bufSync = true;
      break;
      case 'd':
        if(chdir(optarg))
        {
          fprintf(stderr, "Can't change to directory \"%s\".\n", optarg);
          usage();
        }
      break;
      case 'f':
        globals.fast = true;
      break;
      case 'm':
        machine = optarg;
      break;
      case 'n':
        sscanf(optarg, "%d:%d:%d:%d", &directory_size
                     , &directory_max_size, &directory_min_size
                     , &num_directories);
      break;
      case 'p':
        num_bonnie_procs = atoi(optarg);
                        /* Set semaphore to # of bonnie++ procs 
                           to synchronize */
      break;
      case 'q':
        globals.quiet = true;
      break;
      case 'r':
        globals.ram = atoi(optarg);
      break;
      case 's':
      {
        char *sbuf = strdup(optarg);
        char *size = strtok(sbuf, ":");
        file_size = atoi(size);
        char c = size[strlen(size) - 1];
        if(c == 'g' || c == 'G')
          file_size *= 1024;
        size = strtok(NULL, "");
        if(size)
        {
          int tmp = atoi(size);
          c = size[strlen(size) - 1];
          if(c == 'k' || c == 'K')
            tmp *= 1024;
          globals.set_chunk_size(tmp);
        }
        setSize = true;
      }
      break;
      case 'g':
        if(groupName)
          usage();
        groupName = optarg;
      break;
      case 'u':
      {
        if(userName)
          usage();
        userName = strdup(optarg);
        int i;
        for(i = 0; userName[i] && userName[i] != ':'; i++)
        {}
        if(userName[i] == ':')
        {
          if(groupName)
            usage();
          userName[i] = '\0';
          groupName = &userName[i + 1];
        }
      }
      break;
      case 'x':
        count = atoi(optarg);
      break;
      case 'y':
                        /* tell procs to synchronize via previous
                           defined semaphore */
        globals.sync_bonnie = true;
      break;
      case 'D':
                        /* open file descriptor with direct I/O */
        globals.use_direct_io = true;
      break;
    }
  }
  if(optind < argc)
    usage();

  if(globals.ram && !setSize)
  {
    if(file_size < (globals.ram * 2))
      file_size = globals.ram * 2;
    // round up to the nearest gig
    if(file_size % 1024 > 512)
      file_size = file_size + 1024 - (file_size % 1024);
  }

  if(machine == NULL)
  {
    struct utsname utsBuf;
    if(uname(&utsBuf) != -1)
      machine = utsBuf.nodename;
  }

  if(userName || groupName)
  {
    if(bon_setugid(userName, groupName, globals.quiet))
      return 1;
    if(userName)
      free(userName);
  }
  else if(geteuid() == 0)
  {
    fprintf(stderr, "You must use the \"-u\" switch when running as root.\n");
    usage();
  }

  if(num_bonnie_procs && globals.sync_bonnie)
    usage();

  if(num_bonnie_procs)
  {
    if(num_bonnie_procs == -1)
    {
      return globals.sem.clear_sem();
    }
    else
    {
      return globals.sem.create(num_bonnie_procs);
    }
  }

  if(globals.sync_bonnie)
  {
    if(globals.sem.get_semid())
      return 1;
  }

  if(file_size < 0 || directory_size < 0 || (!file_size && !directory_size) )
    usage();
  if(globals.chunk_size() < 256 || globals.chunk_size() > Unit)
    usage();
  int i;
  globals.chunk_bits = 0;
  for(i = globals.chunk_size(); i > 1; i = i >> 1, globals.chunk_bits++)
  {}
  if(1 << globals.chunk_bits != globals.chunk_size())
    usage();

  if( (directory_max_size != -1 && directory_max_size != -2)
     && (directory_max_size < directory_min_size || directory_max_size < 0
     || directory_min_size < 0) )
    usage();
  /* If the storage size is too big for the maximum number of files (1000G) */
  if(file_size > IOFileSize * MaxIOFiles)
    usage();
  /* If the file size is so large and the chunk size is so small that we have
   * more than 2G of chunks */
  if(globals.chunk_bits < 20 && file_size > (1 << (31 - 20 + globals.chunk_bits)) )
    usage();
  // if doing more than one test run then we print a header before the
  // csv format output.
  if(count > 1)
  {
    globals.timer.SetType(BonTimer::csv);
    globals.timer.PrintHeader(stdout);
  }
#ifdef OS2
    ULONG myPid = 0;
    DosQuerySysInfo(QSV_FOREGROUND_PROCESS, QSV_FOREGROUND_PROCESS
                  , &myPid, sizeof(myPid));
#else
  pid_t myPid = getpid();
#endif
  srand(myPid ^ time(NULL));
  for(; count > 0 || count == -1; count--)
  {
    globals.timer.Initialize();
    int rc;
    rc = TestFileOps(file_size, globals);
    if(rc) return rc;
    rc = TestDirOps(directory_size, directory_max_size, directory_min_size
                  , num_directories, globals);
    if(rc) return rc;
    // if we are only doing one test run then print a plain-text version of
    // the results before printing a csv version.
    if(count == -1)
    {
      globals.timer.SetType(BonTimer::txt);
      rc = globals.timer.DoReport(machine, file_size, directory_size
                                , directory_max_size, directory_min_size
                                , num_directories, globals.chunk_size()
                                , globals.quiet ? stderr : stdout);
    }
    // print a csv version in every case
    globals.timer.SetType(BonTimer::csv);
    rc = globals.timer.DoReport(machine, file_size, directory_size
                              , directory_max_size, directory_min_size
                              , num_directories, globals.chunk_size(), stdout);
    if(rc) return rc;
  }
}

int
TestFileOps(int file_size, CGlobalItems &globals)
{
  if(file_size)
  {
    CFileOp file(globals.timer, file_size, globals.chunk_bits, globals.bufSync, globals.use_direct_io);
    int    num_chunks;
    int    words;
    char  *buf = globals.buf();
    int    bufindex;
    int    i;

    if(globals.ram && file_size < globals.ram * 2)
    {
      fprintf(stderr
            , "File size should be double RAM for good results, RAM is %dM.\n"
            , globals.ram);
      return 1;
    }
    // default is we have 1M / 8K * 200 chunks = 25600
    num_chunks = Unit / globals.chunk_size() * file_size;

    int rc;
    rc = file.open(globals.name, true, true);
    if(rc)
      return rc;
    if(exitNow)
      return EXIT_CTRL_C;
    globals.timer.timestamp();

    if(!globals.fast)
    {
      globals.decrement_and_wait(Putc);
      // Fill up a file, writing it a char at a time with the stdio putc() call
      if(!globals.quiet) fprintf(stderr, "Writing with putc()...");
      for(words = 0; words < num_chunks; words++)
      {
        if(file.write_block_putc() == -1)
          return 1;
        if(exitNow)
          return EXIT_CTRL_C;
      }
      fflush(NULL);
      /*
       * note that we always close the file before measuring time, in an
       *  effort to force as much of the I/O out as we can
       */
      file.close();
      globals.timer.get_delta_t(Putc);
      if(!globals.quiet) fprintf(stderr, "done\n");
    }
    /* Write the whole file from scratch, again, with block I/O */
    if(file.reopen(true))
      return 1;
    globals.decrement_and_wait(FastWrite);
    if(!globals.quiet) fprintf(stderr, "Writing intelligently...");
    memset(buf, 0, globals.chunk_size());
    globals.timer.timestamp();
    bufindex = 0;
    // for the number of chunks of file data
    for(i = 0; i < num_chunks; i++)
    {
      if(exitNow)
        return EXIT_CTRL_C;
      // for each chunk in the Unit
      buf[bufindex]++;
      bufindex = (bufindex + 1) % globals.chunk_size();
      if(file.write_block(PVOID(buf)) == -1)
        return io_error("write(2)");
    }
    file.close();
    globals.timer.get_delta_t(FastWrite);
    if(!globals.quiet) fprintf(stderr, "done\n");


    /* Now read & rewrite it using block I/O.  Dirty one word in each block */
    if(file.reopen(false))
      return 1;
    if (file.seek(0, SEEK_SET) == -1)
    {
      if(!globals.quiet) fprintf(stderr, "error in lseek(2) before rewrite\n");
      return 1;
    }
    globals.decrement_and_wait(ReWrite);
    if(!globals.quiet) fprintf(stderr, "Rewriting...");
    globals.timer.timestamp();
    bufindex = 0;
    for(words = 0; words < num_chunks; words++)
    { // for each chunk in the file
      if (file.read_block(PVOID(buf)) == -1)
        return 1;
      bufindex = bufindex % globals.chunk_size();
      buf[bufindex]++;
      bufindex++;
      if (file.seek(-1, SEEK_CUR) == -1)
        return 1;
      if (file.write_block(PVOID(buf)) == -1)
        return io_error("re write(2)");
      if(exitNow)
        return EXIT_CTRL_C;
    }
    file.close();
    globals.timer.get_delta_t(ReWrite);
    if(!globals.quiet) fprintf(stderr, "done\n");


    if(!globals.fast)
    {
      // read them all back with getc()
      if(file.reopen(false, true))
        return 1;
      globals.decrement_and_wait(Getc);
      if(!globals.quiet) fprintf(stderr, "Reading with getc()...");
      globals.timer.timestamp();

      for(words = 0; words < num_chunks; words++)
      {
        if(file.read_block_getc(buf) == -1)
          return 1;
        if(exitNow)
          return EXIT_CTRL_C;
      }

      file.close();
      globals.timer.get_delta_t(Getc);
      if(!globals.quiet) fprintf(stderr, "done\n");
    }

    /* Now suck it in, Chunk at a time, as fast as we can */
    if(file.reopen(false))
      return 1;
    if (file.seek(0, SEEK_SET) == -1)
      return io_error("lseek before read");
    globals.decrement_and_wait(FastRead);
    if(!globals.quiet) fprintf(stderr, "Reading intelligently...");
    globals.timer.timestamp();
    for(i = 0; i < num_chunks; i++)
    { /* per block */
      if ((words = file.read_block(PVOID(buf))) == -1)
        return io_error("read(2)");
      if(exitNow)
        return EXIT_CTRL_C;
    } /* per block */
    file.close();
    globals.timer.get_delta_t(FastRead);
    if(!globals.quiet) fprintf(stderr, "done\n");

    globals.timer.timestamp();
    if(file.seek_test(globals.quiet, globals.sem))
      return 1;

    /*
     * Now test random seeks; first, set up for communicating with children.
     * The object of the game is to do "Seeks" lseek() calls as quickly
     *  as possible.  So we'll farm them out among SeekProcCount processes.
     *  We'll control them by writing 1-byte tickets down a pipe which
     *  the children all read.  We write "Seeks" bytes with val 1, whichever
     *  child happens to get them does it and the right number of seeks get
     *  done.
     * The idea is that since the write() of the tickets is probably
     *  atomic, the parent process likely won't get scheduled while the
     *  children are seeking away.  If you draw a picture of the likely
     *  timelines for three children, it seems likely that the seeks will
     *  overlap very nicely with the process scheduling with the effect
     *  that there will *always* be a seek() outstanding on the file.
     * Question: should the file be opened *before* the fork, so that
     *  all the children are lseeking on the same underlying file object?
     */
  }
  return 0;
}

int
TestDirOps(int directory_size, int max_size, int min_size
         , int num_directories, CGlobalItems &globals)
{
  COpenTest open_test(globals.chunk_size(), globals.bufSync, globals.doExit);
  if(!directory_size)
  {
    return 0;
  }
  // if directory_size (in K) * data per file*2 > (ram << 10) (IE memory /1024)
  // then the storage of file names will take more than half RAM and there
  // won't be enough RAM to have Bonnie++ paged in and to have a reasonable
  // meta-data cache.
  if(globals.ram && directory_size * MaxDataPerFile * 2 > (globals.ram << 10))
  {
    fprintf(stderr
        , "When testing %dK of files in %d MiB of RAM the system is likely to\n"
           "start paging Bonnie++ data and the test will give suspect\n"
           "results, use less files or install more RAM for this test.\n"
          , directory_size, globals.ram);
    return 1;
  }
  // Can't use more than 1G of RAM
  if(directory_size * MaxDataPerFile > (1 << 20))
  {
    fprintf(stderr, "Not enough ram to test with %dK files.\n"
                  , directory_size);
    return 1;
  }
  globals.decrement_and_wait(CreateSeq);
  if(!globals.quiet) fprintf(stderr, "Create files in sequential order...");
  if(open_test.create(globals.name, globals.timer, directory_size
                    , max_size, min_size, num_directories, false))
    return 1;
  globals.decrement_and_wait(StatSeq);
  if(!globals.quiet) fprintf(stderr, "done.\nStat files in sequential order...");
  if(open_test.stat_sequential(globals.timer))
    return 1;
  globals.decrement_and_wait(DelSeq);
  if(!globals.quiet) fprintf(stderr, "done.\nDelete files in sequential order...");
  if(open_test.delete_sequential(globals.timer))
    return 1;
  if(!globals.quiet) fprintf(stderr, "done.\n");

  globals.decrement_and_wait(CreateRand);
  if(!globals.quiet) fprintf(stderr, "Create files in random order...");
  if(open_test.create(globals.name, globals.timer, directory_size
                    , max_size, min_size, num_directories, true))
    return 1;
  globals.decrement_and_wait(StatRand);
  if(!globals.quiet) fprintf(stderr, "done.\nStat files in random order...");
  if(open_test.stat_random(globals.timer))
    return 1;
  globals.decrement_and_wait(DelRand);
  if(!globals.quiet) fprintf(stderr, "done.\nDelete files in random order...");
  if(open_test.delete_random(globals.timer))
    return 1;
  if(!globals.quiet) fprintf(stderr, "done.\n");
  return 0;
}

void
usage()
{
  fprintf(stderr,
    "usage: bonnie++ [-d scratch-dir] [-s size(MiB)[:chunk-size(b)]]\n"
    "                [-n number-to-stat[:max-size[:min-size][:num-directories]]]\n"
    "                [-m machine-name]\n"
    "                [-r ram-size-in-MiB]\n"
    "                [-x number-of-tests] [-u uid-to-use:gid-to-use] [-g gid-to-use]\n"
    "                [-q] [-f] [-b] [-D] [-p processes | -y]\n"
    "\nVersion: " BON_VERSION "\n");
  exit(1);
}

int
io_error(CPCCHAR message, bool do_exit)
{
  char buf[1024];

  if(!already_printed_error && !do_exit)
  {
    sprintf(buf, "Bonnie: drastic I/O error (%s)", message);
    perror(buf);
    already_printed_error = 1;
  }
  if(do_exit)
    exit(1);
  return(1);
}

