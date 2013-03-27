#include <stddef.h>

class foo {
public:
  foo  (int);
  foo  (int, const char *);
  foo  (foo&);
  ~foo ();
  void foofunc (int);
  void foofunc (int, signed char *);
  int ifoo;
  const char *ccpfoo;

int overload1arg (void);
int overload1arg (char);         
int overload1arg (signed char);
int overload1arg (unsigned char);
int overload1arg (short);
int overload1arg (unsigned short);
int overload1arg (int);
int overload1arg (unsigned int);
int overload1arg (long);
int overload1arg (unsigned long);
int overload1arg (float);
int overload1arg (double);

int overloadfnarg (void);
int overloadfnarg (int);
int overloadfnarg (int, int (*) (int));

int overloadargs (int a1);
int overloadargs (int a1, int a2);
int overloadargs (int a1, int a2, int a3);
int overloadargs (int a1, int a2, int a3, int a4);
int overloadargs (int a1, int a2, int a3, int a4, int a5);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8, int a9, int a10);
int overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8, int a9, int a10, int a11);


};

int intToChar (char c)
{
  return 297;
}

void marker1()
{}

// Now test how overloading and namespaces interact.

class dummyClass {};

dummyClass dummyInstance;

int overloadNamespace(int i)
{
  return 1;
}

int overloadNamespace(dummyClass d)
{
  return 2;
}

namespace XXX {
  int overloadNamespace (char c)
  {
    return 3;
  }

  void marker2() {}
}

int main () 
{
    char arg2 = 2;
    signed char arg3 =3;
    unsigned char arg4 =4;
    short arg5 =5;
    unsigned short arg6 =6;
    int arg7 =7;
    unsigned int arg8 =8;
    long arg9 =9;
    unsigned long arg10 =10;
    float arg11 =100.0;
    double arg12 = 200.0;

    char *str = (char *) "A";
    foo foo_instance1(111);
    foo foo_instance2(222, str);
    foo foo_instance3(foo_instance2);

    #ifdef usestubs
       set_debug_traps();
       breakpoint();
    #endif

    // Verify that intToChar should work:
    intToChar(1);

    marker1(); // marker1-returns-here
    XXX::marker2(); // marker1-returns-here
    return 0;
}

foo::foo  (int i)                  { ifoo = i; ccpfoo = NULL; }
foo::foo  (int i, const char *ccp) { ifoo = i; ccpfoo = ccp; }
foo::foo  (foo& afoo)              { ifoo = afoo.ifoo; ccpfoo = afoo.ccpfoo;}
foo::~foo ()                       {}


/* Some functions to test overloading by varying one argument type. */

int foo::overload1arg (void)                {  return 1; }
int foo::overload1arg (char arg)            { arg = 0; return 2;}
int foo::overload1arg (signed char arg)     { arg = 0; return 3;}
int foo::overload1arg (unsigned char arg)   { arg = 0; return 4;}
int foo::overload1arg (short arg)           { arg = 0; return 5;}
int foo::overload1arg (unsigned short arg)  { arg = 0; return 6;}
int foo::overload1arg (int arg)             { arg = 0; return 7;}
int foo::overload1arg (unsigned int arg)    { arg = 0; return 8;}
int foo::overload1arg (long arg)            { arg = 0; return 9;}
int foo::overload1arg (unsigned long arg)   { arg = 0; return 10;}
int foo::overload1arg (float arg)           { arg = 0; return 11;}
int foo::overload1arg (double arg)          { arg = 0; return 12;}

/* Test to see that we can explicitly request overloaded functions
   with function pointers in the prototype. */

int foo::overloadfnarg (void) { return ifoo * 20; }
int foo::overloadfnarg (int arg) { arg = 0; return 13;}
int foo::overloadfnarg (int arg, int (*foo) (int))    { return foo(arg); } 

/* Some functions to test overloading by varying argument count. */

int foo::overloadargs (int a1)                 
{ a1 = 0; 
return 1;}

int foo::overloadargs (int a1, int a2)          
{ a1 = a2 = 0; 
return 2;}

int foo::overloadargs (int a1, int a2, int a3)              
{ a1 = a2 = a3 = 0; 
return 3;}

int foo::overloadargs (int a1, int a2, int a3, int a4)
{ a1 = a2 = a3 = a4 = 0; 
return 4;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5)
{ a1 = a2 = a3 = a4 = a5 = 0; 
return 5;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6)
{ a1 = a2 = a3 = a4 = a5 = a6 = 0; 
return 6;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = 0; 
return 7;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8)
{ a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = 0; 
return 8;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8, int a9)
{ 
  a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 = 0; 
  return 9;
}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8, int a9, int a10)
                        { a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 =
                          a10 = 0; return 10;}

int foo::overloadargs (int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                   int a8, int a9, int a10, int a11)
                        { a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = a9 =
                          a10 = a11 = 0; return 11;}



