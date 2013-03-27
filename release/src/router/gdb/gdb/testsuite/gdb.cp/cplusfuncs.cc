#include <stddef.h>

class foo {
public:
  foo  (int);
  foo  (int, const char *);
  foo  (foo&);
  ~foo ();

  void  operator *      (foo&);
  void  operator %      (foo&);
  void  operator -      (foo&);
  void  operator >>     (foo&);
  void  operator !=     (foo&);
  void  operator >      (foo&);
  void  operator >=     (foo&);
  void  operator |      (foo&);
  void  operator &&     (foo&);
  void  operator !      (void);
  void  operator ++     (int);
  void  operator =      (foo&);
  void  operator +=     (foo&);
  void  operator *=     (foo&);
  void  operator %=     (foo&);
  void  operator >>=    (foo&);
  void  operator |=     (foo&);
  void  operator ,      (foo&);
  void  operator /      (foo&);
  void  operator +      (foo&);
  void  operator <<     (foo&);
  void  operator ==     (foo&);
  void  operator <      (foo&);
  void  operator <=     (foo&);
  void  operator &      (foo&);
  void  operator ^      (foo&);
  void  operator ||     (foo&);
  void  operator ~      (void);
  void  operator --     (int);
  foo*  operator ->     (void);
  void  operator -=     (foo&);
  void  operator /=     (foo&);
  void  operator <<=    (foo&);
  void  operator &=     (foo&);
  void  operator ^=     (foo&);
  void  operator ->*    (foo&);
  void  operator []     (foo&);
  void  operator ()     (foo&);
  void* operator new    (size_t) throw ();
  void  operator delete (void *);
  /**/  operator int    ();
  /**/  operator char*  ();

  int foofunc (int); // forced to have int return type, which is required
  int foofunc (int, signed char *); // forced to have int return type, which is required
  int ifoo;
  const char *ccpfoo;
};

#ifdef usestubs
extern "C" { 
   void set_debug_traps();
   void breakpoint();
};
#endif

int main () {
#ifdef usestubs
   set_debug_traps();
   breakpoint();
#endif
   int z=3;
}

foo::foo  (int i)                  { ifoo = i;}
foo::foo  (int i, const char *ccp) { ifoo = i; ccpfoo = ccp; }
foo::foo  (foo& afoo)              { afoo.ifoo = 0; }
foo::~foo ()                       {}

void  foo::operator *      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator %      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator -      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator >>     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator !=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator >      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator >=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator |      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator &&     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator !      (void) {}
void  foo::operator ++     (int ival) { ival = 0; }
void  foo::operator =      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator +=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator *=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator %=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator >>=    (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator |=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ,      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator /      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator +      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator <<     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ==     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator <      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator <=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator &      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ^      (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ||     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ~      (void) {}
void  foo::operator --     (int ival) { ival = 0; }
foo*  foo::operator ->     (void) {return this;}
void  foo::operator -=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator /=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator <<=    (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator &=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ^=     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ->*    (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator []     (foo& afoo) { afoo.ifoo = 0; }
void  foo::operator ()     (foo& afoo) { afoo.ifoo = 0; }
void* foo::operator new    (size_t ival) throw () { ival = 0; return 0; }
void  foo::operator delete (void *ptr) { ptr = 0; }
/**/  foo::operator int    () { return 0; }
/**/  foo::operator char*  () { return 0; }

/* Some functions to test overloading by varying one argument type. */

void overload1arg (void)		{          }
void overload1arg (char arg)		{ arg = 0; }
void overload1arg (signed char arg)	{ arg = 0; }
void overload1arg (unsigned char arg)	{ arg = 0; }
void overload1arg (short arg)		{ arg = 0; }
void overload1arg (unsigned short arg)	{ arg = 0; }
void overload1arg (int arg)		{ arg = 0; }
void overload1arg (unsigned int arg)	{ arg = 0; }
void overload1arg (long arg)		{ arg = 0; }
void overload1arg (unsigned long arg)	{ arg = 0; }
void overload1arg (float arg)		{ arg = 0; }
void overload1arg (double arg)		{ arg = 0; }

/* Some functions to test overloading by varying argument count. */

void overloadargs (int a1)				{ a1 = 0; }
void overloadargs (int a1, int a2)			{ a1 = a2 = 0; }
void overloadargs (int a1, int a2, int a3)		{ a1 = a2 = a3 = 0; }
void overloadargs (int a1, int a2, int a3, int a4)
			{ a1 = a2 = a3 = a4 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5)
			{ a1 = a2 = a3 = a4 = a5 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6)
			{ a1 = a2 = a3 = a4 = a5 = a6 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7)
			{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
		   int a8)
			{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
		   int a8, int a9)
			{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
		   int a8, int a9, int a10)
			{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 =
			  a10 = 0; }
void overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
		   int a8, int a9, int a10, int a11)
			{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 =
			  a10 = a11 == 0; }

/* Some hairy function definitions.
   Use typedefs to help maintain sanity. */

typedef int   (*PFPc_i)(char *);
typedef short (*PFPl_s)(long *);
typedef short (*PFPc_s)(char *);
typedef int   (*PFl_i)(long);
typedef PFl_i (*PFPc_PFl_i)(char *);
typedef PFl_i (*PFPi_PFl_i)(int *);
typedef PFl_i (*PFPFPc_i_PFl_i)(PFPc_i);
typedef PFl_i (*PFs_PFl_i)(short);
typedef int   (*PFPFPl_s_i)(PFPl_s);
typedef int   (*PFPFPc_s_i)(PFPc_s);

PFs_PFl_i hairyfunc1 (int arg)			{ arg = 0; return 0; }
int       hairyfunc2 (PFPc_i arg)		{ arg = 0; return 0; }
int	  hairyfunc3 (PFPFPl_s_i arg)		{ arg = 0; return 0; }
int	  hairyfunc4 (PFPFPc_s_i arg)		{ arg = 0; return 0; }
int	  hairyfunc5 (PFPc_PFl_i arg)		{ arg = 0; return 0; }
int	  hairyfunc6 (PFPi_PFl_i arg)		{ arg = 0; return 0; }
int	  hairyfunc7 (PFPFPc_i_PFl_i arg)	{ arg = 0; return 0; }

/* gdb has two demanglers (one for g++ 2.95, one for g++ 3).
   These marker functions help me figure out which demangler is in use. */

char *	dm_type_char_star (char * p)		{ return p; }
int	dm_type_foo_ref (foo & foo)		{ return foo.ifoo; }
int *	dm_type_int_star (int * p)		{ return p; }
long *	dm_type_long_star (long * p)		{ return p; }
int	dm_type_unsigned_int (unsigned int i)	{ return i; }
int	dm_type_void (void)			{ return 0; }
void *	dm_type_void_star (void * p)		{ return p; }
