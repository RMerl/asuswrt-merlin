
void marker1()
{
  return;
}

class A1 {
public:
  int x;
  int y;
};

class A2 {
public:
  int x;
  int y;
};

class A3 {
public:
  int x;
  int y;
};

class X : public A1, public A2 {
public:
  int z;
};

class L : public A1 {
public:
  int z;
};

class LV : public virtual A1 {
public:
  int z;
};

class M : public A2 {
public:
  int w;
};

class N : public L, public M {
public:
  int r;
};

class K : public A1 {
public:
  int i;
};

class KV : public virtual A1 {
public:
  int i;
};

class J : public K, public L {
public:
  int j;
};

class JV : public KV, public LV {
public:
  int jv;
};

class JVA1 : public KV, public LV, public A1 {
public:
  int jva1;
};

class JVA2 : public KV, public LV, public A2 {
public:
  int jva2;
};

class JVA1V : public KV, public LV, public virtual A1 {
public:
  int jva1v;
};

int main()
{
  A1 a1;
  A2 a2;
  A3 a3;
  X x;
  L l;
  M m;
  N n;
  K k;
  J j;
  JV jv;
  JVA1 jva1;
  JVA2 jva2;
  JVA1V jva1v;
  
  int i;

  i += k.i + m.w + a1.x + a2.x + a3.x + x.z + l.z + n.r + j.j;

  marker1();
  
}


  
