
using namespace std;

#include <sys/time.h>
#ifdef OS2
#define INCL_DOSFILEMGR
#define INCL_BASE
#include <os2.h>
#include "os2-perfutil.h"
/*
   Convert 8-byte (low, high) time value to double
*/
#define LL2F(high, low) (4294967296.0*(high)+(low))
#else
#include <unistd.h>
#include <sys/resource.h>
#endif
#include "bon_time.h"
#include <time.h>
#include <string.h>

#ifndef NON_UNIX
#include "conf.h"
#endif

#ifdef HAVE_ALGORITHM
#include <algorithm>
#else

#ifdef HAVE_ALGO
#include <algo>
#else
#ifdef HAVE_ALGO_H
#include <algo.h>
#else
#define min(XX,YY) ((XX) < (YY) ? (XX) : (YY))
#define max(XX,YY) ((XX) > (YY) ? (XX) : (YY))
#endif
#endif

#endif

#define TIMEVAL_TO_DOUBLE(XX) (double((XX).tv_sec) + double((XX).tv_usec) / 1000000.0)

void BonTimer::timestamp()
{
  m_last_timestamp = get_cur_time();
  m_last_cpustamp = get_cpu_use();
}

double
BonTimer::time_so_far()
{
  return get_cur_time() - m_last_timestamp;
}

double
BonTimer::cpu_so_far()
{
  return get_cpu_use() - m_last_cpustamp;
}

double
BonTimer::get_cur_time()
{
#ifdef OS2
  ULONG count = 0;
  ULONG rc = DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &count, sizeof(count));
  if(rc)
    return 0.0;
  return double(count)/1000.0;
#else
  struct timeval tp;
 
  if (gettimeofday(&tp, static_cast<struct timezone *>(NULL)) == -1)
    io_error("gettimeofday", true);
  return TIMEVAL_TO_DOUBLE(tp);
#endif
}

double
BonTimer::get_cpu_use()
{
#ifdef OS2
  double      busy_val, busy_val_prev;
  double      intr_val, intr_val_prev;
  CPUUTIL     CPUUtil;

  ULONG rc = DosPerfSysCall(CMD_KI_RDCNT,(ULONG) &CPUUtil,0,0);
  if(rc)
    io_error("times", true);
  return LL2F(CPUUtil.ulBusyHigh, CPUUtil.ulBusyLow)
       + LL2F(CPUUtil.ulIntrHigh, CPUUtil.ulIntrLow);
#else
  struct rusage res_usage;

  getrusage(RUSAGE_SELF, &res_usage);
  return TIMEVAL_TO_DOUBLE(res_usage.ru_utime) + TIMEVAL_TO_DOUBLE(res_usage.ru_stime);
#endif
}

void BonTimer::get_delta_t(tests_t test)
{
  m_delta[test].Elapsed = time_so_far();
  m_delta[test].CPU = cpu_so_far();
  timestamp();
}

void BonTimer::get_delta_report(report_s &rep)
{
  rep.EndTime = get_cur_time();
  rep.CPU = cpu_so_far();
  timestamp();
}

void BonTimer::add_delta_report(report_s &rep, tests_t test)
{
  if(m_delta[test].CPU == 0.0)
  {
    m_delta[test].FirstStart = rep.StartTime;
    m_delta[test].LastStop = rep.EndTime;
  }
  else
  {
    m_delta[test].FirstStart = min(m_delta[test].FirstStart, rep.StartTime);
    m_delta[test].LastStop = max(m_delta[test].LastStop, rep.EndTime);
  }
  m_delta[test].CPU += rep.CPU;
  m_delta[test].Elapsed = m_delta[test].LastStop - m_delta[test].FirstStart;
}

BonTimer::BonTimer()
 : m_type(txt)
{
  Initialize();
}

void
BonTimer::Initialize()
{
  for(int i = 0; i < TestCount; i++)
  {
    m_delta[i].CPU = 0.0;
    m_delta[i].Elapsed = 0.0;
  }
  timestamp();
}

int BonTimer::print_cpu_stat(tests_t test)
{
  if(m_delta[test].Elapsed == 0.0)
  {
    if(m_type == txt)
      fprintf(m_fp, "    ");
    else
      fprintf(m_fp, ",");
    return 0;
  }
  if(m_delta[test].Elapsed < MinTime)
  {
    if(m_type == txt)
      fprintf(m_fp, " +++");
    else
      fprintf(m_fp, ",+++");
    return 0;
  }
  int cpu = int(m_delta[test].CPU / m_delta[test].Elapsed * 100.0);
  if(m_type == txt)
    fprintf(m_fp, " %3d", cpu);
  else
    fprintf(m_fp, ",%d", cpu);
  return 0;
}

int BonTimer::print_stat(tests_t test)
{
  if(m_delta[test].Elapsed == 0.0)
  {
    if(m_type == txt)
      fprintf(m_fp, "      ");
    else
      fprintf(m_fp, ",");
  }
  else if(m_delta[test].Elapsed < MinTime)
  {
    if(m_type == txt)
      fprintf(m_fp, " +++++");
    else
      fprintf(m_fp, ",+++++");
  }
  else
  {
    if(test == Lseek)
    {
      double seek_stat = double(Seeks) / m_delta[test].Elapsed;
      if(m_type == txt)
      {
        if(seek_stat >= 1000.0)
          fprintf(m_fp, " %5.0f", seek_stat);
        else
          fprintf(m_fp, " %5.1f", seek_stat);
      }
      else
        fprintf(m_fp, ",%.1f", seek_stat);
    }
    else
    {
      int res = int(double(m_file_size) / (m_delta[test].Elapsed / 1024.0));
      if(m_type == txt)
        fprintf(m_fp, " %5d", res);
      else
        fprintf(m_fp, ",%d", res);
    }
  }
  return print_cpu_stat(test);
}

int BonTimer::print_file_stat(tests_t test)
{
  if(m_delta[test].Elapsed == 0.0)
  {
    if(m_type == txt)
      fprintf(m_fp, "      ");
    else
      fprintf(m_fp, ",");
  }
  else if(m_delta[test].Elapsed < MinTime)
  {
    if(m_type == txt)
      fprintf(m_fp, " +++++");
    else
      fprintf(m_fp, ",+++++");
  }
  else
  {
    int res = int(double(m_directory_size) * double(DirectoryUnit)
                / m_delta[test].Elapsed);
    if(m_type == txt)
      fprintf(m_fp, " %5d", res);
    else
      fprintf(m_fp, ",%d", res);
  }

  return print_cpu_stat(test);
}

void
BonTimer::PrintHeader(FILE *fp)
{
  fprintf(fp, "name");
  fprintf(fp, ",file_size,putc,putc_cpu,put_block,put_block_cpu,rewrite,rewrite_cpu,getc,getc_cpu,get_block,get_block_cpu,seeks,seeks_cpu");
  fprintf(fp, ",num_files,seq_create,seq_create_cpu,seq_stat,seq_stat_cpu,seq_del,seq_del_cpu,ran_create,ran_create_cpu,ran_stat,ran_stat_cpu,ran_del,ran_del_cpu");
  fprintf(fp, "\n");
  fflush(NULL);
}

int
BonTimer::DoReport(CPCCHAR machine, int file_size, int directory_size
                 , int max_size, int min_size, int num_directories
                 , int chunk_size, FILE *fp)
{
  m_fp = fp;
  m_file_size = file_size;
  m_directory_size = directory_size;
  m_chunk_size = chunk_size;
  const int txt_machine_size = 20;
  if(m_file_size)
  {
    if(m_type == txt)
    {
      fprintf(m_fp, "Version %5s       ", BON_VERSION);
      fprintf(m_fp,
        "------Sequential Output------ --Sequential Input- --Random-\n");
      fprintf(m_fp, "                    ");
      fprintf(m_fp,
        "-Per Chr- --Block-- -Rewrite- -Per Chr- --Block-- --Seeks--\n");
      if(m_chunk_size == DefaultChunkSize)
        fprintf(m_fp, "Machine        Size ");
      else
        fprintf(m_fp, "Machine   Size:chnk ");
      fprintf(m_fp, "K/sec %%CP K/sec %%CP K/sec %%CP K/sec %%CP K/sec ");
      fprintf(m_fp, "%%CP  /sec %%CP\n");
    }
    char size_buf[1024];
    if(m_file_size % 1024 == 0)
      sprintf(size_buf, "%dG", m_file_size / 1024);
    else
      sprintf(size_buf, "%dM", m_file_size);
    if(m_chunk_size != DefaultChunkSize)
    {
      char *tmp = size_buf + strlen(size_buf);
      if(m_chunk_size >= 1024)
        sprintf(tmp, ":%dk", m_chunk_size / 1024);
      else
        sprintf(tmp, ":%d", m_chunk_size);
    }
    char buf[4096];
    if(m_type == txt)
    {
      // copy machine name to buf
      //
      snprintf(buf
#ifndef NO_SNPRINTF
              , txt_machine_size - 1
#endif
              , "%s                  ", machine);
      buf[txt_machine_size - 1] = '\0';
      // set cur to point to a byte past where we end the machine name
      // size of the buf - size of the new data - 1 for the space - 1 for the
      // terminating zero on the string
      char *cur = &buf[txt_machine_size - strlen(size_buf) - 2];
      *cur = ' '; // make cur a space
      cur++; // increment to where we store the size
      strcpy(cur, size_buf);  // copy the size in
      fputs(buf, m_fp);
    }
    else
    {
      printf("%s,%s", machine, size_buf);
    }
    print_stat(Putc);
    print_stat(FastWrite);
    print_stat(ReWrite);
    print_stat(Getc);
    print_stat(FastRead);
    print_stat(Lseek);
    if(m_type == txt)
      fprintf(m_fp, "\n");
  }
  else if(m_type == csv)
  {
    fprintf(m_fp, "%s,,,,,,,,,,,,,", machine);
  }

  if(m_directory_size)
  {
    char buf[128];
    char *tmp;
    sprintf(buf, "%d", m_directory_size);
    if(max_size == -1)
    {
      strcat(buf, ":link");
    }
    else if(max_size == -2)
    {
      strcat(buf, ":symlink");
    }
    else if(max_size)
    {
      tmp = &buf[strlen(buf)];
      sprintf(tmp, ":%d:%d", max_size, min_size);
    }
    if(num_directories > 1)
    {
      tmp = &buf[strlen(buf)];
      sprintf(tmp, "/%d", num_directories);
    }
    if(m_type == txt)
    {
      if(m_file_size)
        fprintf(m_fp, "                    ");
      else
        fprintf(m_fp, "Version %5s       ", BON_VERSION);
      fprintf(m_fp,
        "------Sequential Create------ --------Random Create--------\n");
      if(m_file_size)
        fprintf(m_fp, "                    ");
      else
        fprintf(m_fp, "%-19.19s ", machine);
      fprintf(m_fp,
           "-Create-- --Read--- -Delete-- -Create-- --Read--- -Delete--\n");
      if(min_size)
      {
        fprintf(m_fp, "files:max:min       ");
      }
      else
      {
        if(max_size)
          fprintf(m_fp, "files:max           ");
        else
          fprintf(m_fp, "              files ");
      }
      fprintf(m_fp, " /sec %%CP  /sec %%CP  /sec %%CP  /sec %%CP  /sec ");
      fprintf(m_fp, "%%CP  /sec %%CP\n");
      fprintf(m_fp, "%19s", buf);
    }
    else
    {
      fprintf(m_fp, ",%s", buf);
    }
    print_file_stat(CreateSeq);
    print_file_stat(StatSeq);
    print_file_stat(DelSeq);
    print_file_stat(CreateRand);
    print_file_stat(StatRand);
    print_file_stat(DelRand);
    fprintf(m_fp, "\n");
  }
  else if(m_type == csv)
  {
    fprintf(m_fp, ",,,,,,,,,,,,,\n");
  }
  fflush(stdout);
  return 0;
}

