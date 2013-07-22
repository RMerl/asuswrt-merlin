
using namespace std;

#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include "bonnie.h"
#ifdef HAVE_VECTOR
#include <vector>
#else
#include <vector.h>
#endif

// Read the specified number of megabytes of data from the fd and return the
// amount of time elapsed in seconds.
double access_data(int fd, int size, void *buf, int chunk_size, int do_write);

// Returns the mean of the values in the array.  If the array contains
// more than 2 items then discard the highest and lowest thirds of the
// results before calculating the mean.
double average(double *array, int count);
void printavg(int position, double avg, int block_size);

const int MEG = 1024*1024;
const int DEFAULT_CHUNK_SIZE = 1;
typedef double *PDOUBLE;

void usage()
{
  printf("Usage: zcav [-b block-size] [-c count] [-n number of megs to read]\n"
         "            [-u uid-to-use:gid-to-use] [-g gid-to-use]\n"
         "            [-f] file-name\n"
         "File name of \"-\" means standard input\n"
         "Count is the number of times to read the data (default 1).\n"
         "Version: " BON_VERSION "\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  vector<double *> times;
  vector<int> count;
  int block_size = 256;

  int max_loops = 1, pass_size = 0, chunk_size = DEFAULT_CHUNK_SIZE;
  int do_write = 0;
  char *file_name = NULL;

  char *userName = NULL, *groupName = NULL;
  int c;
  while(-1 != (c = getopt(argc, argv, "-c:b:f:g:n:u:w")) )
  {
    switch(char(c))
    {
      case 'b':
      {
        int rc = sscanf(optarg, "%d:%d", &block_size, &chunk_size);
        if(rc == 1)
          chunk_size = DEFAULT_CHUNK_SIZE;
        else if(rc != 2)
          usage();
      }
      break;
      case 'c':
        max_loops = atoi(optarg);
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
      case 'n':
        pass_size = atoi(optarg);
      break;
      case 'w':
        do_write = 1;
      break;
      case 'f':
      case char(1):
        file_name = optarg;
      break;
      default:
        usage();
    }
  }

  pass_size = pass_size / block_size;

  if(userName || groupName)
  {
    if(bon_setugid(userName, groupName, false))
      return 1;
    if(userName)
      free(userName);
  }

  if(max_loops < 1 || block_size < 1 || chunk_size < 1
    || chunk_size > block_size)
    usage();
  if(!file_name)
    usage();
  printf("#loops: %d, version: %s\n", max_loops, BON_VERSION);

  int i;
  void *buf = calloc(chunk_size * MEG, 1);
  int fd;
  if(strcmp(file_name, "-"))
  {
    if(do_write)
      fd = open(file_name, O_WRONLY);
    else
      fd = open(file_name, O_RDONLY);
    if(fd == -1)
    {
      printf("Can't open %s\n", file_name);
      return 1;
    }
  }
  else
  {
    fd = 0;
  }
  if(max_loops > 1)
  {
    struct stat stat_out, stat_err;
    if(fstat(1, &stat_out) || fstat(2, &stat_err))
    {
      printf("Can't stat stdout/stderr.\n");
      return 1;
    }
    for(int loops = 0; loops < max_loops; loops++)
    {
      if(lseek(fd, 0, SEEK_SET))
      {
        printf("Can't llseek().\n");
        return 1;
      }
      double total_read_time = 0.0;
      for(i = 0; (loops == 0 || times[0][i] != -1.0) && (!pass_size || i < pass_size); i++)
      {
        double read_time = access_data(fd, block_size, buf, chunk_size, do_write);
        total_read_time += read_time;
        if(loops == 0)
        {
          times.push_back(new double[max_loops]);
          count.push_back(0);
        }
        times[i][loops] = read_time;
        if(read_time < 0.0)
        {
          if(i == 0)
          {
            fprintf(stderr, "Data file/device too small.\n");
            return 1;
          }
          times[i][0] = -1.0;
          break;
        }
        count[i]++;
      }
      time_t now = time(NULL);
      struct tm *cur_time = localtime(&now);
      fprintf(stderr, "# Finished loop %d, %d:%02d:%02d\n", loops + 1
            , cur_time->tm_hour, cur_time->tm_min, cur_time->tm_sec);
      printf("# Read %d gigs in %d seconds, %d megabytes per second.\n"
           , i * block_size / 1024, int(total_read_time)
           , int(double(i * block_size) / total_read_time));
      if(stat_out.st_dev != stat_err.st_dev || stat_out.st_ino != stat_err.st_ino)
      {
        fprintf(stderr, "Read %d gigs in %d seconds, %d megabytes per second.\n"
             , i * block_size / 1024, int(total_read_time)
             , int(double(i * block_size) / total_read_time));
      }
    }
    printf("#\n#block offset (GiB), MiB/s, time\n");
    for(i = 0; count[i]; i++)
    {
      printavg(i, average(times[i], count[i]), block_size);
    }
  }
  else
  {
    printf("#block offset (GiB), MiB/s, time\n");
    double total_read_time = 0.0;
    for(i = 0; !pass_size || i < pass_size; i++)
    {
      double read_time = access_data(fd, block_size, buf, chunk_size, do_write);
      if(read_time < 0.0)
        break;
      printavg(i, read_time, block_size);
      total_read_time += read_time;
    }
    if(i == 0)
    {
      fprintf(stderr, "File/device too small.\n");
      return 1;
    }
    printf("# Read %d gigs in %d seconds, %d megabytes per second.\n"
         , i * block_size / 1024, int(total_read_time)
         , int(double(i * block_size) / total_read_time));
  }
  return 0;
}

void printavg(int position, double avg, int block_size)
{
  if(avg < MinTime)
    printf("#%.2f ++++ %.3f \n", float(position) * float(block_size) / 1024.0, avg);
  else
    printf("%.2f %.2f %.3f\n", float(position) * float(block_size) / 1024.0, float(double(block_size) / avg), avg);
}

int compar(const void *a, const void *b)
{
  double *c = (double *)(a);
  double *d = (double *)(b);
  if(*c < *d) return -1;
  if(*c > *d) return 1;
  return 0;
}

// Returns the mean of the values in the array.  If the array contains
// more than 2 items then discard the highest and lowest thirds of the
// results before calculating the mean.
double average(double *array, int count)
{
  qsort(array, count, sizeof(double), compar);
  int skip = count / 3;
  int arr_items = count - (skip * 2);
  double total = 0.0;
  for(int i = skip; i < (count - skip); i++)
  {
    total += array[i];
  }
  return total / arr_items;
}

// just like read() or write() but will not return a partial result and the
// size is expressed in MEG.
ssize_t access_all(int fd, void *buf, size_t chunk_size, int do_write)
{
  ssize_t total = 0;
  chunk_size *= MEG;
  while(total != static_cast<ssize_t>(chunk_size) )
  {
    ssize_t rc;
    // for both read and write just pass the base address of the buffer
    // as we don't care for the data, if we ever do checksums we have to
    // change this
    if(do_write)
      rc = write(fd, buf, chunk_size - total);
    else
      rc = read(fd, buf, chunk_size - total);
    if(rc == -1 || rc == 0)
      return -1;
    total += rc;
  }
  if(do_write && fsync(fd))
    return -1;
  return total / MEG;
}

// Read the specified number of megabytes of data from the fd and return the
// amount of time elapsed in seconds.  If do_write == 1 then write data.
double access_data(int fd, int size, void *buf, int chunk_size, int do_write)
{
  struct timeval tp;
 
  if (gettimeofday(&tp, static_cast<struct timezone *>(NULL)) == -1)
  {
    printf("Can't get time.\n");
    return -1.0;
  }
  double start = double(tp.tv_sec) +
    (double(tp.tv_usec) / 1000000.0);

  for(int i = 0; i < size; i += chunk_size)
  {
    int access_size = chunk_size;
    if(i + chunk_size > size)
      access_size = size - i;
    int rc = access_all(fd, buf, access_size, do_write);
    if(rc != access_size)
      return -1.0;
  }
  if (gettimeofday(&tp, static_cast<struct timezone *>(NULL)) == -1)
  {
    printf("Can't get time.\n");
    return -1.0;
  }
  return (double(tp.tv_sec) + (double(tp.tv_usec) / 1000000.0))
        - start;
}

