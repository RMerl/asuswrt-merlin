struct A
{
  struct B *b_ptr_in_a;
};

struct C
{
  struct B
  {
    int member_of_B_in_C;
  };
};
