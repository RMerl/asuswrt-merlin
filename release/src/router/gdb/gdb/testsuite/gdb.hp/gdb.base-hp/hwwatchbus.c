/* This program raises a SIGBUS signal on HP-UX when the
   pointer "bogus_p" is dereferenced.
   */
int *  bogus_p = (int *)3;

int main()
{
  *bogus_p = 0xdeadbeef;
}
