// Tests for local types

void marker1 (void)
{ 
}

void marker2 (void)
{
}
  
int foobar (int x)
{
  class Local {
  public:
    int loc1;
    char loc_foo (char c)
    {
      return c + 3;
    }
  };

  Local l;
  static Local l1;
  char  c;

  marker1 ();

  l.loc1 = 23;

  c = l.loc_foo('x');
  return c + 2;
}

int main()
{
  int c;
  
  c = foobar (31);
  
 { // inner block
   class InnerLocal {
   public:
     char ilc;
     int * ip;
     int il_foo (unsigned const char & uccr)
     {
       return uccr + 333;
     }
     class NestedInnerLocal {
     public:
       int nil;
       int nil_foo (int i)
       {
         return i * 27;
       }
     };
     NestedInnerLocal nest1;
   };

   InnerLocal il;

   il.ilc = 'b';
   il.ip = &c;
   marker2();
 }
}
