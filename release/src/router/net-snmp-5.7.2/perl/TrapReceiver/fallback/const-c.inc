#define PERL_constant_NOTFOUND	1
#define PERL_constant_NOTDEF	2
#define PERL_constant_ISIV	3
#define PERL_constant_ISNO	4
#define PERL_constant_ISNV	5
#define PERL_constant_ISPV	6
#define PERL_constant_ISPVN	7
#define PERL_constant_ISSV	8
#define PERL_constant_ISUNDEF	9
#define PERL_constant_ISUV	10
#define PERL_constant_ISYES	11

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif

static int
constant_25 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     NETSNMPTRAPD_AUTH_HANDLER NETSNMPTRAPD_HANDLER_FAIL
     NETSNMPTRAPD_POST_HANDLER */
  /* Offset 15 gives the best switch position.  */
  switch (name[15]) {
  case 'N':
    if (memEQ(name, "NETSNMPTRAPD_HANDLER_FAIL", 25)) {
    /*                              ^                */
#ifdef NETSNMPTRAPD_HANDLER_FAIL
      *iv_return = NETSNMPTRAPD_HANDLER_FAIL;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'S':
    if (memEQ(name, "NETSNMPTRAPD_POST_HANDLER", 25)) {
    /*                              ^                */
#ifdef NETSNMPTRAPD_POST_HANDLER
      *iv_return = NETSNMPTRAPD_POST_HANDLER;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "NETSNMPTRAPD_AUTH_HANDLER", 25)) {
    /*                              ^                */
#ifdef NETSNMPTRAPD_AUTH_HANDLER
      *iv_return = NETSNMPTRAPD_AUTH_HANDLER;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant (pTHX_ const char *name, STRLEN len, IV *iv_return) {
  /* Initially switch on the length of the name.  */
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!/usr/bin/perl -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(NETSNMPTRAPD_AUTH_HANDLER NETSNMPTRAPD_HANDLER_BREAK
	       NETSNMPTRAPD_HANDLER_FAIL NETSNMPTRAPD_HANDLER_FINISH
	       NETSNMPTRAPD_HANDLER_OK NETSNMPTRAPD_POST_HANDLER
	       NETSNMPTRAPD_PRE_HANDLER));

print constant_types(); # macro defs
foreach (C_constant ("NetSNMP::TrapReceiver", 'constant', 'IV', $types, undef, 3, @names) ) {
    print $_, "\n"; # C constant subs
}
print "#### XS Section:\n";
print XS_constant ("NetSNMP::TrapReceiver", $types);
__END__
   */

  switch (len) {
  case 23:
    if (memEQ(name, "NETSNMPTRAPD_HANDLER_OK", 23)) {
#ifdef NETSNMPTRAPD_HANDLER_OK
      *iv_return = NETSNMPTRAPD_HANDLER_OK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 24:
    if (memEQ(name, "NETSNMPTRAPD_PRE_HANDLER", 24)) {
#ifdef NETSNMPTRAPD_PRE_HANDLER
      *iv_return = NETSNMPTRAPD_PRE_HANDLER;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 25:
    return constant_25 (aTHX_ name, iv_return);
    break;
  case 26:
    if (memEQ(name, "NETSNMPTRAPD_HANDLER_BREAK", 26)) {
#ifdef NETSNMPTRAPD_HANDLER_BREAK
      *iv_return = NETSNMPTRAPD_HANDLER_BREAK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 27:
    if (memEQ(name, "NETSNMPTRAPD_HANDLER_FINISH", 27)) {
#ifdef NETSNMPTRAPD_HANDLER_FINISH
      *iv_return = NETSNMPTRAPD_HANDLER_FINISH;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

