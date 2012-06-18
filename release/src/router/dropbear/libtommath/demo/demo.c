#include <time.h>

#ifdef IOWNANATHLON
#include <unistd.h>
#define SLEEP sleep(4)
#else
#define SLEEP
#endif

#include "tommath.h"

void ndraw(mp_int * a, char *name)
{
   char buf[16000];

   printf("%s: ", name);
   mp_toradix(a, buf, 10);
   printf("%s\n", buf);
}

static void draw(mp_int * a)
{
   ndraw(a, "");
}


unsigned long lfsr = 0xAAAAAAAAUL;

int lbit(void)
{
   if (lfsr & 0x80000000UL) {
      lfsr = ((lfsr << 1) ^ 0x8000001BUL) & 0xFFFFFFFFUL;
      return 1;
   } else {
      lfsr <<= 1;
      return 0;
   }
}

int myrng(unsigned char *dst, int len, void *dat)
{
   int x;

   for (x = 0; x < len; x++)
      dst[x] = rand() & 0xFF;
   return len;
}



char cmd[4096], buf[4096];
int main(void)
{
   mp_int a, b, c, d, e, f;
   unsigned long expt_n, add_n, sub_n, mul_n, div_n, sqr_n, mul2d_n, div2d_n,
      gcd_n, lcm_n, inv_n, div2_n, mul2_n, add_d_n, sub_d_n, t;
   unsigned rr;
   int i, n, err, cnt, ix, old_kara_m, old_kara_s;
   mp_digit mp;


   mp_init(&a);
   mp_init(&b);
   mp_init(&c);
   mp_init(&d);
   mp_init(&e);
   mp_init(&f);

   srand(time(NULL));

#if 0
   // test montgomery 
   printf("Testing montgomery...\n");
   for (i = 1; i < 10; i++) {
      printf("Testing digit size: %d\n", i);
      for (n = 0; n < 1000; n++) {
         mp_rand(&a, i);
         a.dp[0] |= 1;

         // let's see if R is right
         mp_montgomery_calc_normalization(&b, &a);
         mp_montgomery_setup(&a, &mp);

         // now test a random reduction 
         for (ix = 0; ix < 100; ix++) {
             mp_rand(&c, 1 + abs(rand()) % (2*i));
             mp_copy(&c, &d);
             mp_copy(&c, &e);

             mp_mod(&d, &a, &d);
             mp_montgomery_reduce(&c, &a, mp);
             mp_mulmod(&c, &b, &a, &c);

             if (mp_cmp(&c, &d) != MP_EQ) { 
printf("d = e mod a, c = e MOD a\n");
mp_todecimal(&a, buf); printf("a = %s\n", buf);
mp_todecimal(&e, buf); printf("e = %s\n", buf);
mp_todecimal(&d, buf); printf("d = %s\n", buf);
mp_todecimal(&c, buf); printf("c = %s\n", buf);
printf("compare no compare!\n"); exit(EXIT_FAILURE); }
         }
      }
   }
   printf("done\n");

   // test mp_get_int
   printf("Testing: mp_get_int\n");
   for (i = 0; i < 1000; ++i) {
      t = ((unsigned long) rand() * rand() + 1) & 0xFFFFFFFF;
      mp_set_int(&a, t);
      if (t != mp_get_int(&a)) {
	 printf("mp_get_int() bad result!\n");
	 return 1;
      }
   }
   mp_set_int(&a, 0);
   if (mp_get_int(&a) != 0) {
      printf("mp_get_int() bad result!\n");
      return 1;
   }
   mp_set_int(&a, 0xffffffff);
   if (mp_get_int(&a) != 0xffffffff) {
      printf("mp_get_int() bad result!\n");
      return 1;
   }
   // test mp_sqrt
   printf("Testing: mp_sqrt\n");
   for (i = 0; i < 1000; ++i) {
      printf("%6d\r", i);
      fflush(stdout);
      n = (rand() & 15) + 1;
      mp_rand(&a, n);
      if (mp_sqrt(&a, &b) != MP_OKAY) {
	 printf("mp_sqrt() error!\n");
	 return 1;
      }
      mp_n_root(&a, 2, &a);
      if (mp_cmp_mag(&b, &a) != MP_EQ) {
	 printf("mp_sqrt() bad result!\n");
	 return 1;
      }
   }

   printf("\nTesting: mp_is_square\n");
   for (i = 0; i < 1000; ++i) {
      printf("%6d\r", i);
      fflush(stdout);

      /* test mp_is_square false negatives */
      n = (rand() & 7) + 1;
      mp_rand(&a, n);
      mp_sqr(&a, &a);
      if (mp_is_square(&a, &n) != MP_OKAY) {
	 printf("fn:mp_is_square() error!\n");
	 return 1;
      }
      if (n == 0) {
	 printf("fn:mp_is_square() bad result!\n");
	 return 1;
      }

      /* test for false positives */
      mp_add_d(&a, 1, &a);
      if (mp_is_square(&a, &n) != MP_OKAY) {
	 printf("fp:mp_is_square() error!\n");
	 return 1;
      }
      if (n == 1) {
	 printf("fp:mp_is_square() bad result!\n");
	 return 1;
      }

   }
   printf("\n\n");

   /* test for size */
   for (ix = 10; ix < 128; ix++) {
      printf("Testing (not safe-prime): %9d bits    \r", ix);
      fflush(stdout);
      err =
	 mp_prime_random_ex(&a, 8, ix,
			    (rand() & 1) ? LTM_PRIME_2MSB_OFF :
			    LTM_PRIME_2MSB_ON, myrng, NULL);
      if (err != MP_OKAY) {
	 printf("failed with err code %d\n", err);
	 return EXIT_FAILURE;
      }
      if (mp_count_bits(&a) != ix) {
	 printf("Prime is %d not %d bits!!!\n", mp_count_bits(&a), ix);
	 return EXIT_FAILURE;
      }
   }

   for (ix = 16; ix < 128; ix++) {
      printf("Testing (   safe-prime): %9d bits    \r", ix);
      fflush(stdout);
      err =
	 mp_prime_random_ex(&a, 8, ix,
			    ((rand() & 1) ? LTM_PRIME_2MSB_OFF :
			     LTM_PRIME_2MSB_ON) | LTM_PRIME_SAFE, myrng,
			    NULL);
      if (err != MP_OKAY) {
	 printf("failed with err code %d\n", err);
	 return EXIT_FAILURE;
      }
      if (mp_count_bits(&a) != ix) {
	 printf("Prime is %d not %d bits!!!\n", mp_count_bits(&a), ix);
	 return EXIT_FAILURE;
      }
      /* let's see if it's really a safe prime */
      mp_sub_d(&a, 1, &a);
      mp_div_2(&a, &a);
      mp_prime_is_prime(&a, 8, &cnt);
      if (cnt != MP_YES) {
	 printf("sub is not prime!\n");
	 return EXIT_FAILURE;
      }
   }

   printf("\n\n");

   mp_read_radix(&a, "123456", 10);
   mp_toradix_n(&a, buf, 10, 3);
   printf("a == %s\n", buf);
   mp_toradix_n(&a, buf, 10, 4);
   printf("a == %s\n", buf);
   mp_toradix_n(&a, buf, 10, 30);
   printf("a == %s\n", buf);


#if 0
   for (;;) {
      fgets(buf, sizeof(buf), stdin);
      mp_read_radix(&a, buf, 10);
      mp_prime_next_prime(&a, 5, 1);
      mp_toradix(&a, buf, 10);
      printf("%s, %lu\n", buf, a.dp[0] & 3);
   }
#endif

   /* test mp_cnt_lsb */
   printf("testing mp_cnt_lsb...\n");
   mp_set(&a, 1);
   for (ix = 0; ix < 1024; ix++) {
      if (mp_cnt_lsb(&a) != ix) {
	 printf("Failed at %d, %d\n", ix, mp_cnt_lsb(&a));
	 return 0;
      }
      mp_mul_2(&a, &a);
   }

/* test mp_reduce_2k */
   printf("Testing mp_reduce_2k...\n");
   for (cnt = 3; cnt <= 128; ++cnt) {
      mp_digit tmp;

      mp_2expt(&a, cnt);
      mp_sub_d(&a, 2, &a);	/* a = 2**cnt - 2 */


      printf("\nTesting %4d bits", cnt);
      printf("(%d)", mp_reduce_is_2k(&a));
      mp_reduce_2k_setup(&a, &tmp);
      printf("(%d)", tmp);
      for (ix = 0; ix < 1000; ix++) {
	 if (!(ix & 127)) {
	    printf(".");
	    fflush(stdout);
	 }
	 mp_rand(&b, (cnt / DIGIT_BIT + 1) * 2);
	 mp_copy(&c, &b);
	 mp_mod(&c, &a, &c);
	 mp_reduce_2k(&b, &a, 2);
	 if (mp_cmp(&c, &b)) {
	    printf("FAILED\n");
	    exit(0);
	 }
      }
   }

/* test mp_div_3  */
   printf("Testing mp_div_3...\n");
   mp_set(&d, 3);
   for (cnt = 0; cnt < 10000;) {
      mp_digit r1, r2;

      if (!(++cnt & 127))
	 printf("%9d\r", cnt);
      mp_rand(&a, abs(rand()) % 128 + 1);
      mp_div(&a, &d, &b, &e);
      mp_div_3(&a, &c, &r2);

      if (mp_cmp(&b, &c) || mp_cmp_d(&e, r2)) {
	 printf("\n\nmp_div_3 => Failure\n");
      }
   }
   printf("\n\nPassed div_3 testing\n");

/* test the DR reduction */
   printf("testing mp_dr_reduce...\n");
   for (cnt = 2; cnt < 32; cnt++) {
      printf("%d digit modulus\n", cnt);
      mp_grow(&a, cnt);
      mp_zero(&a);
      for (ix = 1; ix < cnt; ix++) {
	 a.dp[ix] = MP_MASK;
      }
      a.used = cnt;
      a.dp[0] = 3;

      mp_rand(&b, cnt - 1);
      mp_copy(&b, &c);

      rr = 0;
      do {
	 if (!(rr & 127)) {
	    printf("%9lu\r", rr);
	    fflush(stdout);
	 }
	 mp_sqr(&b, &b);
	 mp_add_d(&b, 1, &b);
	 mp_copy(&b, &c);

	 mp_mod(&b, &a, &b);
	 mp_dr_reduce(&c, &a, (((mp_digit) 1) << DIGIT_BIT) - a.dp[0]);

	 if (mp_cmp(&b, &c) != MP_EQ) {
	    printf("Failed on trial %lu\n", rr);
	    exit(-1);

	 }
      } while (++rr < 500);
      printf("Passed DR test for %d digits\n", cnt);
   }

#endif

/* test the mp_reduce_2k_l code */
#if 0
#if 0
/* first load P with 2^1024 - 0x2A434 B9FDEC95 D8F9D550 FFFFFFFF FFFFFFFF */
   mp_2expt(&a, 1024);
   mp_read_radix(&b, "2A434B9FDEC95D8F9D550FFFFFFFFFFFFFFFF", 16);
   mp_sub(&a, &b, &a);
#elif 1
/*  p = 2^2048 - 0x1 00000000 00000000 00000000 00000000 4945DDBF 8EA2A91D 5776399B B83E188F  */
   mp_2expt(&a, 2048);
   mp_read_radix(&b,
		 "1000000000000000000000000000000004945DDBF8EA2A91D5776399BB83E188F",
		 16);
   mp_sub(&a, &b, &a);
#endif

   mp_todecimal(&a, buf);
   printf("p==%s\n", buf);
/* now mp_reduce_is_2k_l() should return */
   if (mp_reduce_is_2k_l(&a) != 1) {
      printf("mp_reduce_is_2k_l() return 0, should be 1\n");
      return EXIT_FAILURE;
   }
   mp_reduce_2k_setup_l(&a, &d);
   /* now do a million square+1 to see if it varies */
   mp_rand(&b, 64);
   mp_mod(&b, &a, &b);
   mp_copy(&b, &c);
   printf("testing mp_reduce_2k_l...");
   fflush(stdout);
   for (cnt = 0; cnt < (1UL << 20); cnt++) {
      mp_sqr(&b, &b);
      mp_add_d(&b, 1, &b);
      mp_reduce_2k_l(&b, &a, &d);
      mp_sqr(&c, &c);
      mp_add_d(&c, 1, &c);
      mp_mod(&c, &a, &c);
      if (mp_cmp(&b, &c) != MP_EQ) {
	 printf("mp_reduce_2k_l() failed at step %lu\n", cnt);
	 mp_tohex(&b, buf);
	 printf("b == %s\n", buf);
	 mp_tohex(&c, buf);
	 printf("c == %s\n", buf);
	 return EXIT_FAILURE;
      }
   }
   printf("...Passed\n");
#endif

   div2_n = mul2_n = inv_n = expt_n = lcm_n = gcd_n = add_n =
      sub_n = mul_n = div_n = sqr_n = mul2d_n = div2d_n = cnt = add_d_n =
      sub_d_n = 0;

   /* force KARA and TOOM to enable despite cutoffs */
   KARATSUBA_SQR_CUTOFF = KARATSUBA_MUL_CUTOFF = 8;
   TOOM_SQR_CUTOFF = TOOM_MUL_CUTOFF = 16;

   for (;;) {
      /* randomly clear and re-init one variable, this has the affect of triming the alloc space */
      switch (abs(rand()) % 7) {
      case 0:
	 mp_clear(&a);
	 mp_init(&a);
	 break;
      case 1:
	 mp_clear(&b);
	 mp_init(&b);
	 break;
      case 2:
	 mp_clear(&c);
	 mp_init(&c);
	 break;
      case 3:
	 mp_clear(&d);
	 mp_init(&d);
	 break;
      case 4:
	 mp_clear(&e);
	 mp_init(&e);
	 break;
      case 5:
	 mp_clear(&f);
	 mp_init(&f);
	 break;
      case 6:
	 break;			/* don't clear any */
      }


      printf
	 ("%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu/%4lu ",
	  add_n, sub_n, mul_n, div_n, sqr_n, mul2d_n, div2d_n, gcd_n, lcm_n,
	  expt_n, inv_n, div2_n, mul2_n, add_d_n, sub_d_n);
      fgets(cmd, 4095, stdin);
      cmd[strlen(cmd) - 1] = 0;
      printf("%s  ]\r", cmd);
      fflush(stdout);
      if (!strcmp(cmd, "mul2d")) {
	 ++mul2d_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 sscanf(buf, "%d", &rr);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);

	 mp_mul_2d(&a, rr, &a);
	 a.sign = b.sign;
	 if (mp_cmp(&a, &b) != MP_EQ) {
	    printf("mul2d failed, rr == %d\n", rr);
	    draw(&a);
	    draw(&b);
	    return 0;
	 }
      } else if (!strcmp(cmd, "div2d")) {
	 ++div2d_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 sscanf(buf, "%d", &rr);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);

	 mp_div_2d(&a, rr, &a, &e);
	 a.sign = b.sign;
	 if (a.used == b.used && a.used == 0) {
	    a.sign = b.sign = MP_ZPOS;
	 }
	 if (mp_cmp(&a, &b) != MP_EQ) {
	    printf("div2d failed, rr == %d\n", rr);
	    draw(&a);
	    draw(&b);
	    return 0;
	 }
      } else if (!strcmp(cmd, "add")) {
	 ++add_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_copy(&a, &d);
	 mp_add(&d, &b, &d);
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("add %lu failure!\n", add_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    return 0;
	 }

	 /* test the sign/unsigned storage functions */

	 rr = mp_signed_bin_size(&c);
	 mp_to_signed_bin(&c, (unsigned char *) cmd);
	 memset(cmd + rr, rand() & 255, sizeof(cmd) - rr);
	 mp_read_signed_bin(&d, (unsigned char *) cmd, rr);
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("mp_signed_bin failure!\n");
	    draw(&c);
	    draw(&d);
	    return 0;
	 }


	 rr = mp_unsigned_bin_size(&c);
	 mp_to_unsigned_bin(&c, (unsigned char *) cmd);
	 memset(cmd + rr, rand() & 255, sizeof(cmd) - rr);
	 mp_read_unsigned_bin(&d, (unsigned char *) cmd, rr);
	 if (mp_cmp_mag(&c, &d) != MP_EQ) {
	    printf("mp_unsigned_bin failure!\n");
	    draw(&c);
	    draw(&d);
	    return 0;
	 }

      } else if (!strcmp(cmd, "sub")) {
	 ++sub_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_copy(&a, &d);
	 mp_sub(&d, &b, &d);
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("sub %lu failure!\n", sub_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    return 0;
	 }
      } else if (!strcmp(cmd, "mul")) {
	 ++mul_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_copy(&a, &d);
	 mp_mul(&d, &b, &d);
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("mul %lu failure!\n", mul_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    return 0;
	 }
      } else if (!strcmp(cmd, "div")) {
	 ++div_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&d, buf, 64);

	 mp_div(&a, &b, &e, &f);
	 if (mp_cmp(&c, &e) != MP_EQ || mp_cmp(&d, &f) != MP_EQ) {
	    printf("div %lu %d, %d, failure!\n", div_n, mp_cmp(&c, &e),
		   mp_cmp(&d, &f));
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    draw(&e);
	    draw(&f);
	    return 0;
	 }

      } else if (!strcmp(cmd, "sqr")) {
	 ++sqr_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 mp_copy(&a, &c);
	 mp_sqr(&c, &c);
	 if (mp_cmp(&b, &c) != MP_EQ) {
	    printf("sqr %lu failure!\n", sqr_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    return 0;
	 }
      } else if (!strcmp(cmd, "gcd")) {
	 ++gcd_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_copy(&a, &d);
	 mp_gcd(&d, &b, &d);
	 d.sign = c.sign;
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("gcd %lu failure!\n", gcd_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    return 0;
	 }
      } else if (!strcmp(cmd, "lcm")) {
	 ++lcm_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_copy(&a, &d);
	 mp_lcm(&d, &b, &d);
	 d.sign = c.sign;
	 if (mp_cmp(&c, &d) != MP_EQ) {
	    printf("lcm %lu failure!\n", lcm_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    return 0;
	 }
      } else if (!strcmp(cmd, "expt")) {
	 ++expt_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&d, buf, 64);
	 mp_copy(&a, &e);
	 mp_exptmod(&e, &b, &c, &e);
	 if (mp_cmp(&d, &e) != MP_EQ) {
	    printf("expt %lu failure!\n", expt_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    draw(&e);
	    return 0;
	 }
      } else if (!strcmp(cmd, "invmod")) {
	 ++inv_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&c, buf, 64);
	 mp_invmod(&a, &b, &d);
	 mp_mulmod(&d, &a, &b, &e);
	 if (mp_cmp_d(&e, 1) != MP_EQ) {
	    printf("inv [wrong value from MPI?!] failure\n");
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    draw(&d);
	    mp_gcd(&a, &b, &e);
	    draw(&e);
	    return 0;
	 }

      } else if (!strcmp(cmd, "div2")) {
	 ++div2_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 mp_div_2(&a, &c);
	 if (mp_cmp(&c, &b) != MP_EQ) {
	    printf("div_2 %lu failure\n", div2_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    return 0;
	 }
      } else if (!strcmp(cmd, "mul2")) {
	 ++mul2_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 mp_mul_2(&a, &c);
	 if (mp_cmp(&c, &b) != MP_EQ) {
	    printf("mul_2 %lu failure\n", mul2_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    return 0;
	 }
      } else if (!strcmp(cmd, "add_d")) {
	 ++add_d_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 sscanf(buf, "%d", &ix);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 mp_add_d(&a, ix, &c);
	 if (mp_cmp(&b, &c) != MP_EQ) {
	    printf("add_d %lu failure\n", add_d_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    printf("d == %d\n", ix);
	    return 0;
	 }
      } else if (!strcmp(cmd, "sub_d")) {
	 ++sub_d_n;
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&a, buf, 64);
	 fgets(buf, 4095, stdin);
	 sscanf(buf, "%d", &ix);
	 fgets(buf, 4095, stdin);
	 mp_read_radix(&b, buf, 64);
	 mp_sub_d(&a, ix, &c);
	 if (mp_cmp(&b, &c) != MP_EQ) {
	    printf("sub_d %lu failure\n", sub_d_n);
	    draw(&a);
	    draw(&b);
	    draw(&c);
	    printf("d == %d\n", ix);
	    return 0;
	 }
      }
   }
   return 0;
}

/* $Source: /cvs/libtom/libtommath/demo/demo.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2005/06/24 11:32:07 $ */
