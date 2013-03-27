// 2002-05-13

enum region { oriental, egyptian, greek, etruscan, roman };

// Test one.
class gnu_obj_1
{
protected:
  typedef region antiquities;
  static const bool 	test = true;
  static const int 	key1 = 5;
  static long       	key2;

  static antiquities 	value;

public:
  gnu_obj_1(antiquities a, long l) {}
};

const bool gnu_obj_1::test;
const int gnu_obj_1::key1;
long gnu_obj_1::key2 = 77;
gnu_obj_1::antiquities gnu_obj_1::value = oriental;


// Test two.
template<typename T>
class gnu_obj_2: public virtual gnu_obj_1
{
public:
  static antiquities	value_derived;
      
public:
  gnu_obj_2(antiquities b): gnu_obj_1(oriental, 7) { }
}; 

template<typename T>
typename gnu_obj_2<T>::antiquities gnu_obj_2<T>::value_derived = etruscan;

// Test three.
template<typename T>
class gnu_obj_3
{
public:
  typedef region antiquities;
  static gnu_obj_2<int> data;
      
public:
  gnu_obj_3(antiquities b) { }
}; 

template<typename T>
gnu_obj_2<int> gnu_obj_3<T>::data(etruscan);

// 2002-08-16
// Test four.
#include "m-static.h"

// instantiate templates explicitly so their static members will exist
template class gnu_obj_2<int>;
template class gnu_obj_2<long>;
template class gnu_obj_3<long>;

int main()
{
  gnu_obj_1		test1(egyptian, 4589);
  gnu_obj_2<long>	test2(roman);
  gnu_obj_3<long>	test3(greek);
  gnu_obj_4		test4;

  test4.dummy = 0;
  return test4.dummy;	// breakpoint: constructs-done
}
