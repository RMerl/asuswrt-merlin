class A
{
};

class B : virtual public A
{
};

class C : public A
{
     protected:
         B myB;
};

int main()
{
     C *obj = new C();
     return 0;
}
