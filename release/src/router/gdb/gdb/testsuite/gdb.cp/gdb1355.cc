struct mystruct
{
  int m_int;
  char m_char;
  long int m_long_int;
  unsigned int m_unsigned_int;
  long unsigned int m_long_unsigned_int;
  // long long int m_long_long_int;
  // long long unsigned int m_long_long_unsigned_int;
  short int m_short_int;
  short unsigned int m_short_unsigned_int;
  unsigned char m_unsigned_char;
  float m_float;
  double m_double;
  long double m_long_double;
  // complex int m_complex_int;
  // complex float m_complex_float;
  // complex long double m_complex_long_double;
  // wchar_t m_wchar_t;
  bool m_bool;
};

struct mystruct s1 =
{
    117, 'a', 118, 119, 120,
    // 121, 122,
    123, 124, 'b', 125.0, 126.0, 127.0,
    // complex int, complex float, complex long double, wchar_t,
    true
};

int main ()
{
  return 0;
}
