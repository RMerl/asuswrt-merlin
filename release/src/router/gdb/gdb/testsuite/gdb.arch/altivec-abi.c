#include <altivec.h>

vector short             vshort = {111, 222, 333, 444, 555, 666, 777, 888};
vector unsigned short   vushort = {100, 200, 300, 400, 500, 600, 700, 800};
vector int                 vint = {-10, -20, -30, -40};
vector unsigned int       vuint = {1111, 2222, 3333, 4444};
vector char               vchar = {'a','b','c','d','e','f','g','h','i','l','m','n','o','p','q','r'};
vector unsigned char     vuchar = {'A','B','C','D','E','F','G','H','I','L','M','N','O','P','Q','R'};
vector float             vfloat = {1.25, 3.75, 5.5, 1.25};

vector short             vshort_d = {0,0,0,0,0,0,0,0};
vector unsigned short   vushort_d = {0,0,0,0,0,0,0,0};
vector int                 vint_d = {0,0,0,0};
vector unsigned int       vuint_d = {0,0,0,0};
vector char               vchar_d = {'z','z','z','z','z','z','z','z','z','z','z','z','z','z','z','z'};
vector unsigned char     vuchar_d = {'Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z','Z'};
vector float             vfloat_d = {1.0, 1.0, 1.0, 1.0};
    
struct test_vec_struct
{
   vector signed short vshort1;
   vector signed short vshort2;
   vector signed short vshort3;
   vector signed short vshort4;
};

static vector signed short test4[4] =
{
   (vector signed short) {1, 2, 3, 4, 5, 6, 7, 8},
   (vector signed short) {11, 12, 13, 14, 15, 16, 17, 18},
   (vector signed short) {21, 22, 23, 24, 25, 26, 27, 28},
   (vector signed short) {31, 32, 33, 34, 35, 36, 37, 38}
};

void
struct_of_vector_func (struct test_vec_struct vector_struct)
{
  vector_struct.vshort1 = vec_add (vector_struct.vshort1, vector_struct.vshort2);
  vector_struct.vshort3 = vec_add (vector_struct.vshort3, vector_struct.vshort4);
}

void
array_of_vector_func (vector signed short *matrix)
{
   matrix[0]  = vec_add (matrix[0], matrix[1]);
   matrix[2]  = vec_add (matrix[2], matrix[3]);
}
 
vector int
vec_func (vector short vshort_f,             /* goes in v2 */
          vector unsigned short vushort_f,   /* goes in v3 */
          vector int vint_f,                 /* goes in v4 */
          vector unsigned int vuint_f,       /* goes in v5 */
          vector char vchar_f,               /* goes in v6 */
          vector unsigned char vuchar_f,     /* goes in v7 */
          vector float vfloat_f,             /* goes in v8 */
          vector short x_f,                  /* goes in v9 */
          vector int y_f,                    /* goes in v10 */
          vector char a_f,                   /* goes in v11 */
          vector float b_f,                  /* goes in v12 */
          vector float c_f,                  /* goes in v13 */
          vector int intv_on_stack_f)
{

   vector int vint_res;
   vector unsigned int vuint_res;
   vector short vshort_res;
   vector unsigned short vushort_res;
   vector char vchar_res;
   vector float vfloat_res;
   vector unsigned char vuchar_res;

   vint_res  = vec_add (vint_f, intv_on_stack_f);
   vint_res  = vec_add (vint_f, y_f);
   vuint_res  = vec_add (vuint_f, ((vector unsigned int) {5,6,7,8}));
   vshort_res  = vec_add (vshort_f, x_f);
   vushort_res  = vec_add (vushort_f,
                           ((vector unsigned short) {1,2,3,4,5,6,7,8}));
   vchar_res  = vec_add (vchar_f, a_f);
   vfloat_res  = vec_add (vfloat_f, b_f);
   vfloat_res  = vec_add (c_f, ((vector float) {1.1,1.1,1.1,1.1}));
   vuchar_res  = vec_add (vuchar_f,
               ((vector unsigned char) {'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a'}));

    return vint_res;
}

void marker(void) {};

int
main (void)
{ 
  vector int result = {-1,-1,-1,-1};
  vector short x = {1,2,3,4,5,6,7,8};
  vector int y = {12, 22, 32, 42};
  vector int intv_on_stack = {12, 34, 56, 78};
  vector char a = {'v','e','c','t','o','r',' ','o','f',' ','c','h','a','r','s','.' };
  vector float b = {5.5, 4.5, 3.75, 2.25};
  vector float c = {1.25, 3.5, 5.5, 7.75};

  vector short x_d = {0,0,0,0,0,0,0,0};
  vector int y_d = {0,0,0,0};
  vector int intv_on_stack_d = {0,0,0,0};
  vector char a_d = {'q','q','q','q','q','q','q','q','q','q','q','q','q','q','q','q'};
  vector float b_d = {5.0, 5.0, 5.0, 5.0};
  vector float c_d = {3.0, 3.0, 3.0, 3.0};
  
  int var_int = 44;
  short var_short = 3;
  struct test_vec_struct vect_struct;
  
  vect_struct.vshort1 = (vector signed short){1, 2, 3, 4, 5, 6, 7, 8};
  vect_struct.vshort2 = (vector signed short){11, 12, 13, 14, 15, 16, 17, 18};
  vect_struct.vshort3 = (vector signed short){21, 22, 23, 24, 25, 26, 27, 28};
  vect_struct.vshort4 = (vector signed short){31, 32, 33, 34, 35, 36, 37, 38};

  marker ();
#if 0
  /* This line is useful for cutting and pasting from the gdb command line.  */
vec_func(vshort,vushort,vint,vuint,vchar,vuchar,vfloat,x,y,a,b,c,intv_on_stack)
#endif
  result = vec_func (vshort,    /* goes in v2 */
                     vushort,   /* goes in v3 */
                     vint,      /* goes in v4 */
                     vuint,     /* goes in v5 */
                     vchar,     /* goes in v6 */
                     vuchar,    /* goes in v7 */
                     vfloat,    /* goes in v8 */
                     x,    /* goes in v9 */
                     y,    /* goes in v10 */
                     a,    /* goes in v11 */
                     b,    /* goes in v12 */
                     c,    /* goes in v13 */
                     intv_on_stack);

   struct_of_vector_func (vect_struct);
   array_of_vector_func (test4);

  return 0;
}

