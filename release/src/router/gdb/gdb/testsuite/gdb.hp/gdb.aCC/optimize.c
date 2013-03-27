/* Source for debugging optimimzed code test.

    cc -g -O -o optimize optimize.c
*/
int callee();
int test_opt;

int main()
{
   int a,b,c,d,e,f,g,h;

   a = 10;;

   /* Value propagate
    */
   b = 2 * a + 1;
   c = 3 * b + 2;

   /* Re-use expressions
    */
   d = (2 * a + 1) * (3 * b + 2);
   e = (2 * a + 1) * (3 * b + 2);

   /* Create dead stores--do lines still exist?
    */
   d = (2 * a + 1) * (3 * b + 2);
   e = (2 * a + 1) * (3 * b + 2);
   d = (2 * a + 1) * (3 * b + 2);
   e = (2 * a + 1) * (3 * b + 2);

   /* Alpha and psi motion
    */
   if( test_opt ) {
       f = e - d;
       f = f--;
   }
   else {
       f = e - d;
       f = f + d * e;
   }

   /* Chi and Rho motion
    */
   h = 0;
   do {
       h++;
       a = b * c + d * e;  /* Chi */
       f = f + d * e;
       g = f + d * e;      /* Rho */
       callee( g+1 );
       test_opt = (test_opt != 1);  /* Cycles */
   } while( g && h < 10);

   /* Opps for tail recursion, unrolling,
    * folding, evaporating
    */
   for( a = 0; a < 100; a++ ) {
       callee( callee ( callee( a )));
       callee( callee ( callee( a )));
       callee( callee ( callee( a )));
   }

   return callee( test_opt );
}

/* defined late to keep line numbers the same
*/
int callee( x )
    int x;      /* not used! */
{
    test_opt++; /* side effect */

    return test_opt;
}

/* end */