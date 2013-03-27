/* This test code is from Wendell Baker (wbaker@comet.berkeley.edu) */

#include <stddef.h>

int a_i;
char a_c;
double a_d;

typedef void *Pix;

int
f(int i)
{ return 0; }

int
f(int i, char c)
{ return 0; }

int
f(int i, char c, double d)
{ return 0; }

int
f(int i, char c, double d, char *cs)
{ return 0; }

int
f(int i, char c, double d, char *cs, void (*fig)(int, char))
{ return 0; }

int
f(int i, char c, double d, char *cs, void (*fig)(char, int))
{ return 0; }

class R {
public:
    int i;
};
class S {
public:
    int i;
};
class T {
public:
    int i;
};

char g(char, const char, volatile char)
{ return 'c'; }
char g(R, char&, const char&, volatile char&)
{ return 'c'; }
char g(char*, const char*, volatile char*)
{ return 'c'; }
char g(S, char*&, const char*&, volatile char*&)
{ return 'c'; }

signed char g(T,signed char, const signed char, volatile signed char)
{ return 'c'; }
signed char g(T, R, signed char&, const signed char&, volatile signed char&)
{ return 'c'; }
signed char g(T, signed char*, const signed char*, volatile signed char*)
{ return 'c'; }
signed char g(T, S, signed char*&, const signed char*&, volatile signed char*&)
{ return 'c'; }

unsigned char g(unsigned char, const unsigned char, volatile unsigned char)
{ return 'c'; }
unsigned char g(R, unsigned char&, const unsigned char&, volatile unsigned char&)
{ return 'c'; }
unsigned char g(unsigned char*, const unsigned char*, volatile unsigned char*)
{ return 'c'; }
unsigned char g(S, unsigned char*&, const unsigned char*&, volatile unsigned char*&)
{ return 'c'; }

short g(short, const short, volatile short)
{ return 0; }
short g(R, short&, const short&, volatile short&)
{ return 0; }
short g(short*, const short*, volatile short*)
{ return 0; }
short g(S, short*&, const short*&, volatile short*&)
{ return 0; }

signed short g(T, signed short, const signed short, volatile signed short)
{ return 0; }
signed short g(T, R, signed short&, const signed short&, volatile signed short&)
{ return 0; }
signed short g(T, signed short*, const signed short*, volatile signed short*)
{ return 0; }
signed short g(T, S, double, signed short*&, const signed short*&, volatile signed short*&)
{ return 0; }

unsigned short g(unsigned short, const unsigned short, volatile unsigned short)
{ return 0; }
unsigned short g(R, unsigned short&, const unsigned short&, volatile unsigned short&)
{ return 0; }
unsigned short g(unsigned short*, const unsigned short*, volatile unsigned short*)
{ return 0; }
unsigned short g(S, unsigned short*&, const unsigned short*&, volatile unsigned short*&)
{ return 0; }

int g(int, const int, volatile int)
{ return 0; }
int g(R, int&, const int&, volatile int&)
{ return 0; }
int g(int*, const int*, volatile int*)
{ return 0; }
int g(S, int*&, const int*&, volatile int*&)
{ return 0; }

signed int g(T, signed int, const signed int, volatile signed int)
{ return 0; }
signed int g(T, R, signed int&, const signed int&, volatile signed int&)
{ return 0; }
signed int g(T, signed int*, const signed int*, volatile signed int*)
{ return 0; }
signed int g(T, S, signed int*&, const signed int*&, volatile signed int*&)
{ return 0; }

unsigned int g(unsigned int, const unsigned int, volatile unsigned int)
{ return 0; }
unsigned int g(R, unsigned int&, const unsigned int&, volatile unsigned int&)
{ return 0; }
unsigned int g(unsigned int*, const unsigned int*, volatile unsigned int*)
{ return 0; }
unsigned int g(S, unsigned int*&, const unsigned int*&, volatile unsigned int*&)
{ return 0; }

long g(long, const long, volatile long)
{ return 0; }
long g(R, long&, const long&, volatile long&)
{ return 0; }
long g(long*, const long*, volatile long*)
{ return 0; }
long g(S, long*&, const long*&, volatile long*&)
{ return 0; }

signed long g(T, signed long, const signed long, volatile signed long)
{ return 0; }
signed long g(T, R, signed long&, const signed long&, volatile signed long&)
{ return 0; }
signed long g(T, signed long*, const signed long*, volatile signed long*)
{ return 0; }
signed long g(T, S, signed long*&, const signed long*&, volatile signed long*&)
{ return 0; }

unsigned long g(unsigned long, const unsigned long, volatile unsigned long)
{ return 0; }
unsigned long g(S, unsigned long&, const unsigned long&, volatile unsigned long&)
{ return 0; }
unsigned long g(unsigned long*, const unsigned long*, volatile unsigned long*)
{ return 0; }
unsigned long g(S, unsigned long*&, const unsigned long*&, volatile unsigned long*&)
{ return 0; }

#ifdef __GNUC__
long long g(long long, const long long, volatile long long)
{ return 0; }
long long g(S, long long&, const long long&, volatile long long&)
{ return 0; }
long long g(long long*, const long long*, volatile long long*)
{ return 0; }
long long g(R, long long*&, const long long*&, volatile long long*&)
{ return 0; }

signed long long g(T, signed long long, const signed long long, volatile signed long long)
{ return 0; }
signed long long g(T, R, signed long long&, const signed long long&, volatile signed long long&)
{ return 0; }
signed long long g(T, signed long long*, const signed long long*, volatile signed long long*)
{ return 0; }
signed long long g(T, S, signed long long*&, const signed long long*&, volatile signed long long*&)
{ return 0; }

unsigned long long g(unsigned long long, const unsigned long long, volatile unsigned long long)
{ return 0; }
unsigned long long g(R, unsigned long long*, const unsigned long long*, volatile unsigned long long*)
{ return 0; }
unsigned long long g(unsigned long long&, const unsigned long long&, volatile unsigned long long&)
{ return 0; }
unsigned long long g(S, unsigned long long*&, const unsigned long long*&, volatile unsigned long long*&)
{ return 0; }
#endif

float g(float, const float, volatile float)
{ return 0; }
float g(char, float&, const float&, volatile float&)
{ return 0; }
float g(float*, const float*, volatile float*)
{ return 0; }
float g(char, float*&, const float*&, volatile float*&)
{ return 0; }

double g(double, const double, volatile double)
{ return 0; }
double g(char, double&, const double&, volatile double&)
{ return 0; }
double g(double*, const double*, volatile double*)
{ return 0; }
double g(char, double*&, const double*&, volatile double*&)
{ return 0; }

#ifdef __GNUC__
long double g(long double, const long double, volatile long double)
{ return 0; }
long double g(char, long double&, const long double&, volatile long double&)
{ return 0; }
long double g(long double*, const long double*, volatile long double*)
{ return 0; }
long double g(char, long double*&, const long double*&, volatile long double*&)
{ return 0; }
#endif

class c {
public:
    c(int) {};
    int i;
};

class c g(c, const c, volatile c)
{ return 0; }
c g(char, c&, const c&, volatile c&)
{ return 0; }
c g(c*, const c*, volatile c*)
{ return 0; }
c g(char, c*&, const c*&, volatile c*&)
{ return 0; }

/*
void h(char = 'a')
{ }
void h(char, signed char = 'a')
{ }
void h(unsigned char = 'a')
{ }
*/
/*
void h(char = (char)'a')
{ }
void h(char, signed char = (signed char)'a')
{ }
void h(unsigned char = (unsigned char)'a')
{ }


void h(short = (short)43)
{ }
void h(char, signed short = (signed short)43)
{ }
void h(unsigned short = (unsigned short)43)
{ }

void h(int = (int)43)
{ }
void h(char, signed int = (signed int)43)
{ }
void h(unsigned int = (unsigned int)43)
{ }


void h(long = (long)43)
{ }
void h(char, signed long = (signed long)43)
{ }
void h(unsigned long = (unsigned long)43)
{ }

#ifdef __GNUC__
void h(long long = 43)
{ }
void h(char, signed long long = 43)
{ }
void h(unsigned long long = 43)
{ }
#endif

void h(float = 4.3e-10)
{ }
void h(double = 4.3)
{ }
#ifdef __GNUC__
void h(long double = 4.33e33)
{ }
#endif
*/

/* An unneeded printf() definition - actually, just a stub - used to occupy
   this space.  It has been removed and replaced with this comment which
   exists to occupy some lines so that templates.exp won't need adjustment.  */

class T1 {
public:
    static void* operator new(size_t) throw ();
    static void operator delete(void *pointer);

    void operator=(const T1&);
    T1& operator=(int);

    int operator==(int) const;
    int operator==(const T1&) const;
    int operator!=(int) const;
    int operator!=(const T1&) const;

    int operator<=(int) const;
    int operator<=(const T1&) const;
    int operator<(int) const;
    int operator<(const T1&) const;
    int operator>=(int) const;
    int operator>=(const T1&) const;
    int operator>(int) const;
    int operator>(const T1&) const;

    void operator+(int) const;
    T1& operator+(const T1&) const;
    void operator+=(int) const;
    T1& operator+=(const T1&) const;

    T1& operator++() const;

    void operator-(int) const;
    T1& operator-(const T1&) const;
    void operator-=(int) const;
    T1& operator-=(const T1&) const;

    T1& operator--() const;

    void operator*(int) const;
    T1& operator*(const T1&) const;
    void operator*=(int) const;
    T1& operator*=(const T1&) const;

    void operator/(int) const;
    T1& operator/(const T1&) const;
    void operator/=(int) const;
    T1& operator/=(const T1&) const;

    void operator%(int) const;
    T1& operator%(const T1&) const;
    void operator%=(int) const;
    T1& operator%=(const T1&) const;

    void operator&&(int) const;
    T1& operator&&(const T1&) const;

    void operator||(int) const;
    T1& operator||(const T1&) const;

    void operator&(int) const;
    T1& operator&(const T1&) const;
    void operator&=(int) const;
    T1& operator&=(const T1&) const;

    void operator|(int) const;
    T1& operator|(const T1&) const;
    void operator|=(int) const;
    T1& operator|=(const T1&) const;

    void operator^(int) const;
    T1& operator^(const T1&) const;
    void operator^=(int) const;
    T1& operator^=(const T1&) const;

    T1& operator!() const;
    T1& operator~() const;
};

void* 
T1::operator new(size_t) throw ()
{ return 0; }

void
T1::operator delete(void *pointer)
{ }

class T2 {
public:
    T2(int i): integer(i)
	{ }
    int integer;
};

int operator==(const T2&, const T2&)
{ return 0; }
int operator==(const T2&, char)
{ return 0; }
int operator!=(const T2&, const T2&)
{ return 0; }
int operator!=(const T2&, char)
{ return 0; }

int operator<=(const T2&, const T2&)
{ return 0; }
int operator<=(const T2&, char)
{ return 0; }
int operator<(const T2&, const T2&)
{ return 0; }
int operator<(const T2&, char)
{ return 0; }
int operator>=(const T2&, const T2&)
{ return 0; }
int operator>=(const T2&, char)
{ return 0; }
int operator>(const T2&, const T2&)
{ return 0; }
int operator>(const T2&, char)
{ return 0; }

T2 operator+(const T2 t, int i)
{ return t.integer + i; }
T2 operator+(const T2 a, const T2& b)
{ return a.integer + b.integer; }
T2& operator+=(T2& t, int i)
{ t.integer += i; return t; }
T2& operator+=(T2& a, const T2& b)
{ a.integer += b.integer; return a; }

T2 operator-(const T2 t, int i)
{ return t.integer - i; }
T2 operator-(const T2 a, const T2& b)
{ return a.integer - b.integer; }
T2& operator-=(T2& t, int i)
{ t.integer -= i; return t; }
T2& operator-=(T2& a, const T2& b)
{ a.integer -= b.integer; return a; }

T2 operator*(const T2 t, int i)
{ return t.integer * i; }
T2 operator*(const T2 a, const T2& b)
{ return a.integer * b.integer; }
T2& operator*=(T2& t, int i)
{ t.integer *= i; return t; }
T2& operator*=(T2& a, const T2& b)
{ a.integer *= b.integer; return a; }

T2 operator/(const T2 t, int i)
{ return t.integer / i; }
T2 operator/(const T2 a, const T2& b)
{ return a.integer / b.integer; }
T2& operator/=(T2& t, int i)
{ t.integer /= i; return t; }
T2& operator/=(T2& a, const T2& b)
{ a.integer /= b.integer; return a; }

T2 operator%(const T2 t, int i)
{ return t.integer % i; }
T2 operator%(const T2 a, const T2& b)
{ return a.integer % b.integer; }
T2& operator%=(T2& t, int i)
{ t.integer %= i; return t; }
T2& operator%=(T2& a, const T2& b)
{ a.integer %= b.integer; return a; }

template<class T>
class T5 {
public:
    T5(int);
    T5(const T5<T>&);
    ~T5();
    static void* operator new(size_t) throw ();
    static void operator delete(void *pointer);
    int value();
    
    static T X;
    T x;
    int val;
};

template<class T>
T5<T>::T5(int v)
{ val = v; }

template<class T>
T5<T>::T5(const T5<T>&)
{}

template<class T>
T5<T>::~T5()
{}

template<class T>
void*
T5<T>::operator new(size_t) throw ()
{ return 0; }

template<class T>
void
T5<T>::operator delete(void *pointer)
{ }

template<class T>
int
T5<T>::value()
{ return val; }


#if ! defined(__GNUC__) || defined(GCC_BUG)
template<class T>
T T5<T>::X;
#endif




T5<char> t5c(1);
T5<int> t5i(2);
T5<int (*)(char, void *)> t5fi1(3);
T5<int (*)(int, double **, void *)> t5fi2(4);

 




class x {
public:
    int (*manage[5])(double,
		     void *(*malloc)(unsigned size),
		     void (*free)(void *pointer));
    int (*device[5])(int open(const char *, unsigned mode, unsigned perms, int extra), 
		     int *(*read)(int fd, void *place, unsigned size),
		     int *(*write)(int fd, void *place, unsigned size),
		     void (*close)(int fd));
};
T5<x> t5x(5);

#if !defined(__GNUC__) || (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6)
template class T5<char>;
template class T5<int>;
template class T5<int (*)(char, void *)>;
template class T5<int (*)(int, double **, void *)>;
template class T5<x>;
#endif

class T7 {
public:
    static int get();
    static void put(int);
};

int
T7::get()
{ return 1; }

void
T7::put(int i)
{
    // nothing
}

// More template kinds.  GDB 4.16 didn't handle these, but
// Wildebeest does.  Note: Assuming HP aCC is used to compile
// this file; with g++ or HP cfront or other compilers the
// demangling may not get done correctly.

// Ordinary template, to be instantiated with different types
template<class T>
class Foo {
public:
  int x;
  T t;
  T foo (int, T);
};


template<class T> T Foo<T>::foo (int i, T tt)
{
  return tt;
}

// Template with int parameter

template<class T, int sz>
class Bar {
public:
  int x;
  T t;
  T bar (int, T);
};


template<class T, int sz> T Bar<T, sz>::bar (int i, T tt)
{
  if (i < sz)
    return tt;
  else
    return 0;
}

// function template with int parameter
template<class T> int dummy (T tt, int i)
{
  return tt;
}

// Template with partial specializations
template<class T1, class T2>
class Spec {
public:
  int x;
  T1 spec (T2);
};

template<class T1, class T2>
T1 Spec<T1, T2>::spec (T2 t2)
{
  return 0;
}

template<class T>
class Spec<T, T*> {
public:
  int x;
  T spec (T*);
};

template<class T>
T Spec<T, T*>::spec (T * tp)
{
  return *tp;
}

// Template with char parameter
template<class T, char sz>
class Baz {
public:
  int x;
  T t;
  T baz (int, T);
};

template<class T, char sz> T Baz<T, sz>::baz (int i, T tt)
{
  if (i < sz)
    return tt;
  else
    return 0;
}

// Template with char * parameter
template<class T, char * sz>
class Qux {
public:
  int x;
  T t;
  T qux (int, T);
};

template<class T, char * sz> T Qux<T, sz>::qux (int i, T tt)
{
  if (sz[0] == 'q')
    return tt;
  else
    return 0;
}

// Template with a function pointer parameter
template<class T, int (*f)(int) >
class Qux1 {
public:
  int x;
  T t;
  T qux (int, T);
};

template<class T, int (*f)(int)> T Qux1<T, f>::qux (int i, T tt)
{
  if (f != 0)
    return tt;
  else
    return 0;
}

// Some functions to provide as arguments to template
int gf1 (int a) {
  return a * 2 + 13;
}
int gf2 (int a) {
  return a * 2 + 26;
}

char string[3];


// Template for nested instantiations

template<class T>
class Garply {
public:
  int x;
  T t;
  T garply (int, T);
};

template<class T> T Garply<T>::garply (int i, T tt)
{
  if (i > x)
    return tt;
  else
    {
      x += i;
      return tt;
    }
}


int main()
{
    int i;
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    i = i + 1;

    // New tests added here

  Foo<int> fint={0,0};
  Foo<char> fchar={0,0};
  Foo<volatile char *> fvpchar = {0, 0};

  Bar<int, 33> bint;
  Bar<int, (4 > 3)> bint2;

  Baz<int, 's'> bazint;
  Baz<char, 'a'> bazint2;

  Qux<char, string> quxint2;
  Qux<int, string> quxint;

  Qux1<int, gf1> qux11;

  int x = fint.foo(33, 47);
  char c = fchar.foo(33, 'x');
  volatile char * cp = fvpchar.foo(33, 0);
  
  int y = dummy<int> (400, 600);

  int z = bint.bar(55, 66);
  z += bint2.bar(55, 66);

  c = bazint2.baz(4, 'y');
  c = quxint2.qux(4, 'z');
  
  y = bazint.baz(4,3);
  y = quxint.qux(4, 22);
  y += qux11.qux(4, 22);

  y *= gf1(y) - gf2(y);
  
  Spec<int, char> sic;
  Spec<int, int *> siip;

  sic.spec ('c');
  siip.spec (&x);

  Garply<int> f;
  Garply<char> fc;
  f.x = 13;

  Garply<Garply<char> > nf;
  nf.x = 31;
  
  x = f.garply (3, 4);
  
  fc = nf.garply (3, fc);

  y = x + fc.x;
  

  return 0;
    
}













