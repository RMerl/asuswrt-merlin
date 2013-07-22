#ifndef BON_TIME_H
#define BON_TIME_H

#include "bonnie.h"

struct report_s
{
  double CPU;
  double StartTime;
  double EndTime;
};

struct delta_s
{
  double CPU;
  double Elapsed;
  double FirstStart;
  double LastStop;
};

class BonTimer
{
public:
  enum RepType { csv, txt };

  BonTimer();

  void timestamp();
  void get_delta_t(tests_t test);
  void get_delta_report(report_s &rep);
  void add_delta_report(report_s &rep, tests_t test);
  int DoReport(CPCCHAR machine, int size, int directory_size
             , int max_size, int min_size, int num_directories
             , int chunk_size, FILE *fp);
  void SetType(RepType type) { m_type = type; }
  double cpu_so_far();
  double time_so_far();
  void PrintHeader(FILE *fp);
  void Initialize();
  static double get_cur_time();
  static double get_cpu_use();
 
private:
  int print_cpu_stat(tests_t test);
  int print_stat(tests_t test);
  int print_file_stat(tests_t test);

  delta_s m_delta[TestCount];
  double m_last_cpustamp;
  double m_last_timestamp;
  RepType m_type;
  int m_file_size;
  int m_directory_size;
  int m_chunk_size;
  FILE *m_fp;

  BonTimer(const BonTimer&);
  BonTimer &operator=(const BonTimer&);
};

#endif
