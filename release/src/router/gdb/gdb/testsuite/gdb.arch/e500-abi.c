#include <spe.h>

/* Test PowerPC SPU extensions.  */

#define vector __attribute__((vector_size(8)))

vector unsigned short f_vec;
vector short g_vec;
vector float h_vec;
vector float i_vec;
vector unsigned int l_vec;
vector int m_vec;
vector int n_vec;

/* dummy variables used in the testfile */
vector unsigned int a_vec_d = {1, 1};
vector int b_vec_d = {0, 0};
vector float c_vec_d = {1.0, 1.0};
vector unsigned int d_vec_d = {0, 0};
vector int e_vec_d = {1, 1};
vector unsigned short f_vec_d = {1, 1, 1, 1};
vector short g_vec_d = {1, 1, 1, 1};
vector float h_vec_d = {1.0, 1.0};
vector float i_vec_d = {2.0, 2.0};
vector unsigned int l_vec_d = {0, 0};
vector int m_vec_d = {0, 0};


vector int
vec_func (vector unsigned int a_vec_f,
          vector int b_vec_f,
          vector float c_vec_f,
          vector unsigned int d_vec_f,
          vector int e_vec_f,
          vector unsigned short f_vec_f,
          vector short g_vec_f,
          vector float h_vec_f,
          vector float i_vec_f,
          vector unsigned int l_vec_f,
          vector int m_vec_f) 
{
  vector int n_vec;


  int x,y,z;
  x = 2;
  y = 3;
 
  z = x + y;
  z++;
  n_vec = __ev_and(a_vec_f, b_vec_f);
  n_vec = __ev_or(c_vec_f, d_vec_f);
  n_vec = __ev_or(e_vec_f, f_vec_f);
  n_vec = __ev_and(g_vec_f, h_vec_f);
  n_vec = __ev_and(i_vec_f, l_vec_f);
  n_vec = __ev_or(m_vec_f, a_vec_f);

  return n_vec;
}

void marker(void) {};

int
main (void)
{
  vector unsigned int a_vec;
  vector int b_vec;
  vector float c_vec;
  vector unsigned int d_vec;
  vector int e_vec;

  vector int res_vec;

  a_vec = (vector unsigned int)__ev_create_u64 ((uint64_t) 55);
  b_vec = __ev_create_s64 ((int64_t) 66);
  c_vec = (vector float) __ev_create_fs (3.14F, 2.18F);
  d_vec = (vector unsigned int) __ev_create_u32 ((uint32_t) 5, (uint32_t) 4);
  e_vec = (vector int) __ev_create_s32 ((int32_t) 5, (int32_t) 6);
  f_vec = (vector unsigned short) __ev_create_u16 ((uint16_t) 6, (uint16_t) 6, (uint16_t) 7, (uint16_t) 1);
  g_vec = (vector short) __ev_create_s16 ((int16_t) 6, (int16_t) 6, (int16_t) 7, (int16_t) 9);
  h_vec = (vector float) __ev_create_sfix32_fs (3.0F, 2.0F);
  i_vec = (vector float) __ev_create_ufix32_fs (3.0F, 2.0F);
  l_vec = (vector unsigned int) __ev_create_ufix32_u32 (3U, 5U);
  m_vec = (vector int) __ev_create_sfix32_s32 (6, 9);

  marker ();

#if 0
/* This line is useful for cut-n-paste from a gdb session. */
vec_func(a_vec,b_vec,c_vec,d_vec,e_vec,f_vec,g_vec,h_vec,i_vec,l_vec,m_vec)
#endif

  res_vec = vec_func (a_vec,  /* goes in r3 */
                      b_vec,  /* goes in r4 */
                      c_vec,  /* goes in r5 */
                      d_vec,  /* goes in r6 */
                      e_vec,  /* goes in r7 */
                      f_vec,  /* goes in r8 */
                      g_vec,  /* goes in r9 */
                      h_vec,  /* goes in r10 */
                      i_vec,  /* goes in stack */
                      l_vec,  /* goes in stack */
                      m_vec);  /* goes in stack */

  return 0;
}
