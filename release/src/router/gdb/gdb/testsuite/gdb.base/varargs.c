/* varargs.c - 
 * (Added as part of fix for bug 15306 - "call" to varargs functions fails)
 * This program is intended to let me try out "call" to varargs functions
 * with varying numbers of declared args and various argument types.
 * - RT 9/27/95
 */

#include <stdio.h>
#include <stdarg.h>

int find_max1(int, ...);
int find_max2(int, int, ...);
double find_max_double(int, double, ...);

char ch;
unsigned char uc;
short s;
unsigned short us;
int a,b,c,d;
int max_val;
long long ll;
float fa,fb,fc,fd;
double da,db,dc,dd;
double dmax_val;

int main() {
  c = -1;
  uc = 1;
  s = -2;
  us = 2;
  a = 1;
  b = 60;
  max_val = find_max1(1, 60);
  max_val = find_max1(a, b);
  a = 3;
  b = 1;
  c = 4;
  d = 2;
  max_val = find_max1(3, 1, 4, 2);
  max_val = find_max2(a, b, c, d);
  da = 3.0;
  db = 1.0;
  dc = 4.0;
  dd = 2.0;
  dmax_val = find_max_double(3, 1.0, 4.0, 2.0);
  dmax_val = find_max_double(a, db, dc, dd);
  
  return 0;
}

/* Integer varargs, 1 declared arg */

int find_max1(int num_vals, ...) {
  int max_val = 0;
  int x;
  int i;
  va_list argp;
  va_start(argp, num_vals);
  printf("find_max(%d,", num_vals);
  for (i = 0; i < num_vals; i++) {
    x = va_arg(argp, int);
    if (max_val < x) max_val = x;
    if (i < num_vals - 1)
      printf(" %d,", x);
    else
      printf(" %d)", x);
  }
  printf(" returns %d\n", max_val);
  return max_val;
}

/* Integer varargs, 2 declared args */

int find_max2(int num_vals, int first_val, ...) {
  int max_val = 0;
  int x;
  int i;
  va_list argp;
  va_start(argp, first_val);
  x = first_val;
  if (max_val < x) max_val = x;
  printf("find_max(%d, %d", num_vals, first_val);
  for (i = 1; i < num_vals; i++) {
    x = va_arg(argp, int);
    if (max_val < x) max_val = x;
    printf(", %d", x);
  }
  printf(") returns %d\n", max_val);
  return max_val;
}

/* Double-float varargs, 2 declared args */

double find_max_double(int num_vals, double first_val, ...) {
  double max_val = 0;
  double x;
  int i;
  va_list argp;
  va_start(argp, first_val);
  x = first_val;
  if (max_val < x) max_val = x;
  printf("find_max(%d, %f", num_vals, first_val);
  for (i = 1; i < num_vals; i++) {
    x = va_arg(argp, double);
    if (max_val < x) max_val = x;
    printf(", %f", x);
  }
  printf(") returns %f\n", max_val);
  return max_val;
}

