static int filelocal = 1;	/* In Data section */
static int filelocal_bss;	/* In BSS section */
#ifndef __STDC__
#define	const	/**/
#endif
static const int filelocal_ro = 201;	/* In Read-Only Data section */

extern void init1();
extern void foo();

int autovars (int bcd, int abc);
int localscopes (int x);
int useit (int val);
void init0();
void marker1 ();
void marker2 ();
void marker3 ();
void marker4 ();

int main ()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  init0 ();
  foo ();
  autovars (5, 6);
  localscopes (0);
}

/* On some systems, such as AIX, unreferenced variables are deleted
   from the executable.  */
void usestatics ()
{
  useit (filelocal);
  useit (filelocal_bss);
  useit (filelocal_ro);
}

void init0 ()
{
  filelocal_bss = 101;
  init1 ();
}

/* This is to derail optimizer in localscopes.
   Return 1 + 2 + . . . + N.  */
#ifdef PROTOTYPES
int
sum_upto (int n)
#else
int
sum_upto (n)
     int n;
#endif
{
  int i;
  int retval = 0;

  for (i = 1; i <= n; ++i)
    retval += i;
  return retval;
}

#ifdef PROTOTYPES
int
useit (int val)
#else
int
useit (val)
#endif
{
    static int usedval;

    usedval = val;
    return val + sum_upto (0);
}

#ifdef PROTOTYPES
int
autovars (int bcd, int abc)
#else
int
autovars (bcd, abc)
     int bcd;
     int abc;
#endif
{
    int  i0 =  useit (0),  i1 =  useit (1),  i2 =  useit (2);
    int  i3 =  useit (3),  i4 =  useit (4),  i5 =  useit (5);
    int  i6 =  useit (6),  i7 =  useit (7),  i8 =  useit (8);
    int  i9 =  useit (9), i10 = useit (10), i11 = useit (11);
    int i12 = useit (12), i13 = useit (13), i14 = useit (14);
    int i15 = useit (15), i16 = useit (16), i17 = useit (17);
    int i18 = useit (18), i19 = useit (19), i20 = useit (20);
    int i21 = useit (21), i22 = useit (22), i23 = useit (23);
    int i24 = useit (24), i25 = useit (25), i26 = useit (26);
    int i27 = useit (27), i28 = useit (28), i29 = useit (29);
    int i30 = useit (30), i31 = useit (31), i32 = useit (32);
    int i33 = useit (33), i34 = useit (34), i35 = useit (35);
    int i36 = useit (36), i37 = useit (37), i38 = useit (38);
    int i39 = useit (39), i40 = useit (40), i41 = useit (41);
    int i42 = useit (42), i43 = useit (43), i44 = useit (44);
    int i45 = useit (45), i46 = useit (46), i47 = useit (47);
    int i48 = useit (48), i49 = useit (49), i50 = useit (50);
    int i51 = useit (51), i52 = useit (52), i53 = useit (53);
    int i54 = useit (54), i55 = useit (55), i56 = useit (56);
    int i57 = useit (57), i58 = useit (58), i59 = useit (59);
    int i60 = useit (60), i61 = useit (61), i62 = useit (62);
    int i63 = useit (63), i64 = useit (64), i65 = useit (65);
    int i66 = useit (66), i67 = useit (67), i68 = useit (68);
    int i69 = useit (69), i70 = useit (70), i71 = useit (71);
    int i72 = useit (72), i73 = useit (73), i74 = useit (74);
    int i75 = useit (75), i76 = useit (76), i77 = useit (77);
    int i78 = useit (78), i79 = useit (79), i80 = useit (80);
    int i81 = useit (81), i82 = useit (82), i83 = useit (83);
    int i84 = useit (84), i85 = useit (85), i86 = useit (86);
    int i87 = useit (87), i88 = useit (88), i89 = useit (89);
    int i90 = useit (90), i91 = useit (91), i92 = useit (92);
    int i93 = useit (93), i94 = useit (94), i95 = useit (95);
    int i96 = useit (96), i97 = useit (97), i98 = useit (98);
    int i99 = useit (99);

    /* Use all 100 of the local variables to derail agressive optimizers.  */

    useit ( i0); useit ( i1); useit ( i2); useit ( i3); useit ( i4);
    useit ( i5); useit ( i6); useit ( i7); useit ( i8); useit ( i9);
    useit (i10); useit (i11); useit (i12); useit (i13); useit (i14);
    useit (i15); useit (i16); useit (i17); useit (i18); useit (i19);
    useit (i20); useit (i21); useit (i22); useit (i23); useit (i24);
    useit (i25); useit (i26); useit (i27); useit (i28); useit (i29);
    useit (i30); useit (i31); useit (i32); useit (i33); useit (i34);
    useit (i35); useit (i36); useit (i37); useit (i38); useit (i39);
    useit (i40); useit (i41); useit (i42); useit (i43); useit (i44);
    useit (i45); useit (i46); useit (i47); useit (i48); useit (i49);
    useit (i50); useit (i51); useit (i52); useit (i53); useit (i54);
    useit (i55); useit (i56); useit (i57); useit (i58); useit (i59);
    useit (i60); useit (i61); useit (i62); useit (i63); useit (i64);
    useit (i65); useit (i66); useit (i67); useit (i68); useit (i69);
    useit (i70); useit (i71); useit (i72); useit (i73); useit (i74);
    useit (i75); useit (i76); useit (i77); useit (i78); useit (i79);
    useit (i80); useit (i81); useit (i82); useit (i83); useit (i84);
    useit (i85); useit (i86); useit (i87); useit (i88); useit (i89);
    useit (i90); useit (i91); useit (i92); useit (i93); useit (i94);
    useit (i95); useit (i96); useit (i97); useit (i98); useit (i99);

    useit (abc); useit (bcd);

    marker1 ();
    return i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10
      + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i20
      + i21 + i22 + i23 + i24 + i25 + i26 + i27 + i28 + i29 + i30
      + i31 + i32 + i33 + i34 + i35 + i36 + i37 + i38 + i39 + i40
      + i41 + i42 + i43 + i44 + i45 + i46 + i47 + i48 + i49 + i50
      + i51 + i52 + i53 + i54 + i55 + i56 + i57 + i58 + i59 + i60
      + i61 + i62 + i63 + i64 + i65 + i66 + i67 + i68 + i69 + i70
      + i71 + i72 + i73 + i74 + i75 + i76 + i77 + i78 + i79 + i80
      + i81 + i82 + i83 + i84 + i85 + i86 + i87 + i88 + i89 + i90
      + i91 + i92 + i93 + i94 + i95 + i96 + i97 + i98 + i99 + abc + bcd;
}

#ifdef PROTOTYPES
int
localscopes (int x)
#else
int
localscopes (x)
     int x;
#endif
{
    int localval;
    int retval;
    int i;

    localval = 0;
    useit (localval);

    {
	int localval = x + 4 + sum_upto (3); /* 10 */
	int localval1 = x + 5 + sum_upto (3); /* 11 */

	useit (localval);
	useit (localval1);
	marker2 ();
	{
	    int localval = localval1 + 3 + sum_upto (3); /* 20 */
	    int localval2 = localval1 + sum_upto (1); /* 12 */
	    useit (localval);
	    useit (localval2);
	    marker3 ();
	    {
		int localval = localval2 + 3 + sum_upto (5); /* 30 */
		int localval3 = localval2 + sum_upto (1); /* 13 */
		useit (localval);
		useit (localval3);
		marker4 ();
		retval = x + localval1 + localval2 + localval3;
	    }
	}
    }
    return retval;
}

void marker1 () {}
void marker2 () {}
void marker3 () {}
void marker4 () {}
