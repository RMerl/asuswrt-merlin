/* An include file that actually causes code to be generated in the including file.  This is known to cause problems on some systems. */
#ifdef PROTOTYPES
extern void bar(int);
static void foo (int x)
#else
static void foo (x) int x;
#endif
{
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
    bar (x++);
}
