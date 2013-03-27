void marker1 (void)
{
}

/* misc. function params */

int
qux1 (const char cc, const char /*&*/ccr, const char *ccp, char *const cpc)
{
  return 33;
}

int
qux2 (volatile unsigned char vuc, const volatile int cvi,
      volatile short /*&*/vsr, volatile long *vlp, float *volatile fpv,
      const volatile signed char *const volatile cvscpcv)
{
  return 400;
}

int
main (void)
{
  char lave = 'B';
  unsigned char lavish = 10;
  short lax = 20;
  unsigned short lecherous = 30;
  long lechery = 40;
  unsigned long lectern = 50;
  float leeway = 60;
  double legacy = 70;
  signed char lemonade = 35;

  const char            laconic = 'A';
  const unsigned char   laggard = 1;
  const short           lagoon = 2;
  const unsigned short  laity = 3;
  const long            lambent = 4;
  const unsigned long   laminated = 5;
  const float           lampoon = 6;
  const double          languid = 7;

  /* pointers to constant variables */

  const char           *legend      = &lave;
  const unsigned char  *legerdemain = &lavish;
  const short          *leniency    = &lax;
  const unsigned short *leonine     = &lecherous;
  const long           *lesion      = &lechery;
  const unsigned long  *lethal      = &lectern;
  const float          *lethargic   = &leeway;
  const double         *levity      = &legacy;

  /* constant pointers to constant variables */

  const char           *const lewd          = &laconic;
  const unsigned char  *const lexicographer = &laggard;
  const short          *const lexicon       = &lagoon;
  const unsigned short *const liaison       = &laity;
  const long           *const libation      = &lambent;
  const unsigned long  *const libelous      = &laminated;
  const float          *const libertine     = &lampoon;
  const double         *const libidinous    = &languid;

  /* this is the same as const char * legend .... */

  char           const *languish   = &laconic;
  unsigned char  const *languor    = &laggard;
  short          const *lank       = &lagoon;
  unsigned short const *lapidary   = &laity;
  long           const *larceny    = &lambent;
  unsigned long  const *largess    = &laminated;
  float          const *lascivious = &lampoon;
  double         const *lassitude  = &languid;

  /* constant pointers to variable */

  char           *const   lamprey   = &lave;
  unsigned char  *const   lariat    = &lavish;
  short          *const   laudanum  = &lax;
  unsigned short *const   lecithin  = &lecherous;
  long           *const   leviathan = &lechery;
  unsigned long  *const   libretto  = &lectern;
  float          *const   lissome   = &leeway;
  double         *const   locust    = &legacy;

  /* volatile variables */

  volatile char vox = 'X';
  volatile unsigned char victuals = 13;
  volatile short vixen = 200;
  volatile unsigned short vitriol = 300;
  volatile long vellum = 1000;
  volatile unsigned long valve = 2000;
  volatile float vacuity = 3.0;
  volatile double vertigo = 10.3;
    
  /* pointers to volatile variables */

  volatile char           * vampire = &vox;
  volatile unsigned char  * viper  = &victuals;
  volatile short          * vigour = &vixen;
  volatile unsigned short * vapour = &vitriol;
  volatile long           * ventricle = &vellum;
  volatile unsigned long  * vigintillion = &valve;
  volatile float          * vocation = &vacuity;
  volatile double         * veracity = &vertigo;

  /* volatile pointers to volatile variables */

  volatile char           * volatile vapidity = &vox;
  volatile unsigned char  * volatile velocity = &victuals;
  volatile short          * volatile veneer = &vixen;
  volatile unsigned short * volatile video = &vitriol;
  volatile long           * volatile vacuum = &vellum;
  volatile unsigned long  * volatile veniality = &valve;
  volatile float          * volatile vitality = &vacuity;
  volatile double         * volatile voracity = &vertigo;

  /* const volatile vars */

  const volatile char           victor = 'Y';
  const volatile unsigned char  vicar = 11;

  /* pointers to const volatiles */

  const volatile char              * victory = &victor;
  const volatile unsigned char     * vicarage = &vicar;

  /* const pointers to volatile vars */

  volatile char                    * const vein = &vox;
  volatile unsigned char           * const vogue = &victuals;

  /* const pointers to const volatile vars */

  const volatile char              * const cavern = &victor;
  const volatile unsigned char     * const coverlet = &vicar;

  /* volatile pointers to const vars */

  const char                       * volatile caveat = &laconic;
  const unsigned char              * volatile covenant = &laggard;

  /* volatile pointers to const volatile vars */

  const volatile char              * volatile vizier = &victor;
  const volatile unsigned char     * volatile vanadium = &vicar;

  /* const volatile pointers */

  char                             * const volatile vane = &lave;
  unsigned char                    * const volatile veldt = &lavish;

  /* const volatile pointers to const vars */

  const char                       * const volatile cove = &laconic;
  const unsigned char              * const volatile cavity = &laggard;
    
  /* const volatile pointers to volatile vars */

  volatile char                    * const volatile vagus = &vox;
  volatile unsigned char           * const volatile vagrancy = &victuals;
    
  /* const volatile pointers to const volatile */

  const volatile char              * const volatile vagary = &victor;
  const volatile unsigned char     * const volatile vendor = &vicar;
  
  /* various structs with const members */

  struct crass { char * const ptr; } crass = { lamprey };
  struct crisp { char * const *ptr; } crisp = { &lamprey };

  /* misc. references */
  /*
  const char           & radiation = laconic;
  volatile signed char & remuneration = lemonade;
  */
#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif
  marker1 ();
    

  return 0;
}
