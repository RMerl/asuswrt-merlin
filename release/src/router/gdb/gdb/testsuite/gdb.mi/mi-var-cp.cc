/* Copyright 2006, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

void reference_update_tests ()
{
  /*: BEGIN: reference_update :*/
  int x = 167;
  /*: mi_create_varobj "RX" "rx" "create varobj for rx" :*/
  int& rx = x;
  /*: mi_varobj_update RX {RX} "update RX (1)"
      mi_check_varobj_value RX 167 "check RX: expect 167"
      :*/
  x = 567;
  /*: mi_varobj_update RX {RX} "update RX (2)"
      mi_check_varobj_value RX 567 "check RX: expect 567"
      :*/  
  x = 567;
  /*: mi_varobj_update RX {} "update RX (3)"
      mi_delete_varobj RX "delete RX"
    :*/
  /* Dummy assignment to keep 'x' in scope.  */
  x = 444;    

  /*: END: reference_update :*/
}

struct S { int i; int j; };
struct S2 : S {};
        
int base_in_reference_test (S2& s2)
{
  /*: BEGIN: base_in_reference :*/
  return s2.i;
  /*: 
    mi_create_varobj "S2" "s2" "create varobj for s2"
    mi_list_varobj_children "S2" {
       {"S2.S" "S" "1" "S"}
    } "list children of s2"
    mi_list_varobj_children "S2.S" {
       {"S2.S.public" "public" "2"}
    } "list children of s2.s"
    mi_list_varobj_children "S2.S.public" {
       {"S2.S.public.i" "i" "0" "int"}
       {"S2.S.public.j" "j" "0" "int"}
    } "list children of s2.s.public"

    mi_check_varobj_value "S2.S.public.i" "67" "check S2.S.public.i"
    mi_check_varobj_value "S2.S.public.j" "89" "check S2.S.public.j"
    mi_delete_varobj S2 "delete S2"
    
  :*/
  /*: END: base_in_reference :*/
}
        
void base_in_reference_test_main ()
{
  S2 s;
  s.i = 67;
  s.j = 89;
  base_in_reference_test (s);
}

int reference_to_pointer ()
{
  /*: BEGIN: reference_to_pointer :*/  
  S s, *ptr_s, *& rptr_s = ptr_s;
  s.i = 67;
  s.j = 89;
  ptr_s = &s;
  /*: 
    mi_create_varobj RPTR rptr_s "create varobj for rptr_s"

    mi_list_varobj_children RPTR {{RPTR.public public 2}}	\
    "list public child of RPTR"

    mi_list_varobj_children  RPTR.public	\
    {{RPTR.public.i i 0 int}
    {RPTR.public.j j 0 int}} "list children of reference to pointer"

    mi_check_varobj_value RPTR.public.i 67 "check i member"
    mi_check_varobj_value RPTR.public.j 89 "check j member"
    mi_delete_varobj RPTR "delete RPTR"
  :*/
  return 99;
  /*: END: reference_to_pointer :*/
}

int reference_to_struct ()
{
  /*: BEGIN: reference_to_struct :*/
  S s = {7, 8};
  S& r = s;
  /*:
    mi_create_varobj S s "create varobj for s"
    mi_create_varobj R r "create varobj for s"
    mi_gdb_test "-var-show-attributes S" \
	"\\^done,attr=\"noneditable\"" \
	"check attributes of S"
    mi_gdb_test "-var-show-attributes R" \
	"\\^done,attr=\"noneditable\"" \
	"check attributes of R"
    :*/
  s.i = 56;
  /*: mi_varobj_update * [] "-var-update should not list structure varobjs"
    :*/
  return 99;
  /*: END: reference_to_struct :*/
}

struct Base1
{
  int i;
};

struct Base2
{
  int i;
};

struct Derived : public Base1, public Base2
{
  int i;
};

/* Test for the -var-info-path-expression command.  Although
   said command is not specific to C++, it's of more importance
   to C++ than to C, so we test it in mi-var-cp test.  */
int path_expression ()
{
  /*: BEGIN: path_expression :*/
  int i = 10;
  int *ip = &i;
  /*: mi_create_varobj IP ip "create varobj for ip"
      mi_list_varobj_children IP {{IP.\\*ip \\*ip 0 int}} "list children of IP"
      mi_gdb_test "-var-info-path-expression IP.*ip" \
          "\\^done,path_expr=\"\\*\\(ip\\)\"" \
	  "-var-info-path-expression IP.*ip"
    :*/
  Derived d;
  Derived *dp = &d;
  /*: mi_create_varobj DP dp "create varobj for dp"
      mi_list_varobj_children DP                        \
      {{DP.Base1 Base1 1 Base1}                         \
       {DP.Base2 Base2 1 Base2}                         \
       {DP.public public 1}} "list children of DP"
      mi_gdb_test "-var-info-path-expression DP.Base1" \
          "\\^done,path_expr=\"\\(\\*\\(Base1\\*\\) dp\\)\"" \
	  "-var-info-path-expression DP.Base1"       
      mi_list_varobj_children DP.public {               \
        {DP.public.i i 0 int}                           \
      } "list children of DP.public"
      mi_gdb_test "-var-info-path-expression DP.public.i" \
          "\\^done,path_expr=\"\\(\\(dp\\)->i\\)\"" \
	  "-var-info-path-expression DP.public.i"
      mi_list_varobj_children DP.Base1 {                 \
        {DP.Base1.public public 1}                             \
      } "list children of DP.Base1"
      mi_list_varobj_children DP.Base1.public {               \
        {DP.Base1.public.i i 0 int}                           \
      } "list children of DP.Base1.public"
      mi_gdb_test "-var-info-path-expression DP.Base1.public.i" \
          "\\^done,path_expr=\"\\(\\(\\(\\*\\(Base1\\*\\) dp\\)\\).i\\)\"" \
	  "-var-info-path-expression DP.Base1.public.i"

      mi_gdb_test "-var-info-path-expression DP.public" \
          "\\^done,path_expr=\"\"" \
	  "-var-info-path-expression DP.public"

      mi_create_varobj D d "create varobj for d"
      mi_list_varobj_children D                        \
      {{D.Base1 Base1 1 Base1}                         \
       {D.Base2 Base2 1 Base2}                         \
       {D.public public 1}} "list children of D"
      mi_gdb_test "-var-info-path-expression D.Base1" \
          "\\^done,path_expr=\"\\(\\(Base1\\) d\\)\"" \
	  "-var-info-path-expression D.Base1"
  :*/
  int array[4] = {1,2,3};
  array[3] = 10;
  /*: mi_create_varobj A array "create varobj for array"
      mi_list_varobj_children A { \
          {A.0 0 0 int}
          {A.1 1 0 int}
          {A.2 2 0 int}
          {A.3 3 0 int}} "list children of A"
      mi_gdb_test "-var-info-path-expression A.2" \
          "\\^done,path_expr=\"\\(array\\)\\\[2\\\]\"" \
	  "-var-info-path-expression A.2"
    :*/

  return 99;
  /*: END: path_expression :*/
}

int main ()
{
  reference_update_tests ();
  base_in_reference_test_main ();
  reference_to_pointer ();
  reference_to_struct ();
  path_expression ();
  return 0;
}
