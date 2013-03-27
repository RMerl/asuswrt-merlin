class A {
public:
    int a;
    int aa;

    A()
    {
        a=1;
        aa=2;
    }
    int afoo();
    int foo();
    
};



class B {
public:
    int b;
    int bb;

    B()
    {
        b=3;
        bb=4;
    }
    int bfoo();
    int foo();
    
};



class C {
public:
    int c;
    int cc;

    C()
    {
        c=5;
        cc=6;
    }
    int cfoo();
    int foo();
    
};



class D : private A, public B, protected C {
public:
    int d;
    int dd;

    D()
    {
        d =7;
        dd=8;
    }
    int dfoo();
    int foo();
    
};


class E : public A, B, protected C {
public:
    int e;
    int ee;

    E()
    {
        e =9;
        ee=10;
    }
    int efoo();
    int foo();
    
};


class F : A, public B, C {
public:
    int f;
    int ff;

    F()
    {
        f =11;
        ff=12;
    }
    int ffoo();
    int foo();
    
};

class G : private A, public B, protected C {
public:
    int g;
    int gg;
    int a;
    int b;
    int c;

    G()
    {
        g =13;
        gg =14;
        a=15;
        b=16;
        c=17;
        
    }
    int gfoo();
    int foo();
    
};




int A::afoo() {
    return 1;
}

int B::bfoo() {
    return 2;
}

int C::cfoo() {
    return 3;
}

int D::dfoo() {
    return 4;
}

int E::efoo() {
    return 5;
}

int F::ffoo() {
    return 6;
}

int G::gfoo() {
    return 77;
}

int A::foo()
{
    return 7;
    
}

int B::foo()
{
    return 8;
    
}

int C::foo()
{
    return 9;
    
}

int D::foo()
{
    return 10;
    
}

int E::foo()
{
    return 11;
    
}

int F::foo()
{
    return 12;
    
}

int G::foo()
{
    return 13;
    
}


void marker1()
{
}


int main(void)
{

    A a_instance;
    B b_instance;
    C c_instance;
    D d_instance;
    E e_instance;
    F f_instance;
    G g_instance;
    
    #ifdef usestubs
       set_debug_traps();
       breakpoint();
    #endif
    

    marker1(); // marker1-returns-here
    
    a_instance.a = 20; // marker1-returns-here
    a_instance.aa = 21;
    b_instance.b = 22;
    b_instance.bb = 23;
    c_instance.c = 24;
    c_instance.cc = 25;
    d_instance.d = 26;
    d_instance.dd = 27;
    e_instance.e = 28;
    e_instance.ee =29;
    f_instance.f =30;
    f_instance.ff =31;
    
    
    

    return 0;
    
}

    
    
