/* Longjump free calls to GDB internal routines.

   Copyright (C) 1999, 2000, 2005, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "value.h"
#include "exceptions.h"
#include "wrapper.h"
#include "ui-out.h"

int
gdb_parse_exp_1 (char **stringptr, struct block *block, int comma,
		 struct expression **expression)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *expression = parse_exp_1 (stringptr, block, comma);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_evaluate_expression (struct expression *exp, struct value **value)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *value = evaluate_expression(exp);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_fetch_lazy (struct value *val)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      value_fetch_lazy (val);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_equal (struct value *val1, struct value *val2, int *result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_equal (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_assign (struct value *val1, struct value *val2,
		  struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_assign (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_subscript (struct value *val1, struct value *val2,
		     struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_subscript (val1, val2);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_value_ind (struct value *val, struct value **result)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *result = value_ind (val);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

int
gdb_parse_and_eval_type (char *p, int length, struct type **type)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
      *type = parse_and_eval_type (p, length);
    }

  if (except.reason < 0)
    return 0;
  return 1;
}

enum gdb_rc
gdb_value_struct_elt (struct ui_out *uiout, struct value **result,
		      struct value **argp, struct value **args, char *name,
		      int *static_memfuncp, char *err)
{
  volatile struct gdb_exception except;

  TRY_CATCH (except, RETURN_MASK_ALL)
    {
      *result = value_struct_elt (argp, args, name, static_memfuncp, err);
    }

  if (except.reason < 0)
    return GDB_RC_FAIL;
  return GDB_RC_OK;
}
