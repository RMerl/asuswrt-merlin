///
/// kzalloc should be used rather than kmalloc followed by memset 0
///
// Confidence: High
// Copyright: (C) 2009-2010 Julia Lawall, Nicolas Palix, DIKU.  GPLv2.
// Copyright: (C) 2009-2010 Gilles Muller, INRIA/LiP6.  GPLv2.
// URL: http://coccinelle.lip6.fr/rules/kzalloc.html
// Options: -no_includes -include_headers
//
// Keywords: kmalloc, kzalloc
// Version min: < 2.6.12 kmalloc
// Version min:   2.6.14 kzalloc
//

virtual context
virtual patch
virtual org
virtual report

//----------------------------------------------------------
//  For context mode
//----------------------------------------------------------

@depends on context@
type T, T2;
expression x;
expression E1,E2;
statement S;
@@

* x = (T)kmalloc(E1,E2);
  if ((x==NULL) || ...) S
* memset((T2)x,0,E1);

//----------------------------------------------------------
//  For patch mode
//----------------------------------------------------------

@depends on patch@
type T, T2;
expression x;
expression E1,E2;
statement S;
@@

- x = (T)kmalloc(E1,E2);
+ x = kzalloc(E1,E2);
  if ((x==NULL) || ...) S
- memset((T2)x,0,E1);

//----------------------------------------------------------
//  For org mode
//----------------------------------------------------------

@r depends on org || report@
type T, T2;
expression x;
expression E1,E2;
statement S;
position p;
@@

 x = (T)kmalloc@p(E1,E2);
 if ((x==NULL) || ...) S
 memset((T2)x,0,E1);

@script:python depends on org@
p << r.p;
x << r.x;
@@

msg="%s" % (x)
msg_safe=msg.replace("[","@(").replace("]",")")
coccilib.org.print_todo(p[0], msg_safe)

@script:python depends on report@
p << r.p;
x << r.x;
@@

msg="WARNING: kzalloc should be used for %s, instead of kmalloc/memset" % (x)
coccilib.report.print_report(p[0], msg)
