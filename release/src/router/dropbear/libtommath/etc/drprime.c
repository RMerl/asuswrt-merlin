/* Makes safe primes of a DR nature */
#include <tommath.h>

int sizes[] = { 1+256/DIGIT_BIT, 1+512/DIGIT_BIT, 1+768/DIGIT_BIT, 1+1024/DIGIT_BIT, 1+2048/DIGIT_BIT, 1+4096/DIGIT_BIT };
int main(void)
{
   int res, x, y;
   char buf[4096];
   FILE *out;
   mp_int a, b;
   
   mp_init(&a);
   mp_init(&b);
   
   out = fopen("drprimes.txt", "w");
   for (x = 0; x < (int)(sizeof(sizes)/sizeof(sizes[0])); x++) {
   top:
       printf("Seeking a %d-bit safe prime\n", sizes[x] * DIGIT_BIT);
       mp_grow(&a, sizes[x]);
       mp_zero(&a);
       for (y = 1; y < sizes[x]; y++) {
           a.dp[y] = MP_MASK;
       }
       
       /* make a DR modulus */
       a.dp[0] = -1;
       a.used = sizes[x];
       
       /* now loop */
       res = 0;
       for (;;) { 
          a.dp[0] += 4;
          if (a.dp[0] >= MP_MASK) break;
          mp_prime_is_prime(&a, 1, &res);
          if (res == 0) continue;
          printf("."); fflush(stdout);
          mp_sub_d(&a, 1, &b);
          mp_div_2(&b, &b);
          mp_prime_is_prime(&b, 3, &res);  
          if (res == 0) continue;
          mp_prime_is_prime(&a, 3, &res);
          if (res == 1) break;
	}
        
        if (res != 1) {
           printf("Error not DR modulus\n"); sizes[x] += 1; goto top;
        } else {
           mp_toradix(&a, buf, 10);
           printf("\n\np == %s\n\n", buf);
           fprintf(out, "%d-bit prime:\np == %s\n\n", mp_count_bits(&a), buf); fflush(out);
        }           
   }
   fclose(out);
   
   mp_clear(&a);
   mp_clear(&b);
   
   return 0;
}


/* $Source: /cvs/libtom/libtommath/etc/drprime.c,v $ */
/* $Revision: 1.2 $ */
/* $Date: 2005/05/05 14:38:47 $ */
