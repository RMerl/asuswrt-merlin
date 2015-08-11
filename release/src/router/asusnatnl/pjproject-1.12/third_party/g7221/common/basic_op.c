#include "config.h"

#if !PJMEDIA_LIBG7221_FUNCS_INLINED || \
    (PJMEDIA_LIBG7221_FUNCS_INLINED && defined(__BASIC_OP_H__))

/*___________________________________________________________________________
 |                                                                           |
 | Basic arithmetic operators.                                               |
 |___________________________________________________________________________|
*/

/*___________________________________________________________________________
 |                                                                           |
 |   Include-Files                                                           |
 |___________________________________________________________________________|
*/

//#include <stdio.h>
//#include <stdlib.h>
#include "typedef.h"
#include "basic_op.h"
#include <pj/assert.h>

#if (WMOPS)
#include "count.h"
extern BASIC_OP multiCounter[MAXCOUNTERS];
extern int currCounter;

#endif

/*___________________________________________________________________________
 |                                                                           |
 |   Constants and Globals                                                   |
 |___________________________________________________________________________|
*/
#if INCLUDE_UNSAFE
Flag g7221_Overflow = 0;
Flag g7221_Carry = 0;
#endif

/*___________________________________________________________________________
 |                                                                           |
 |   Functions                                                               |
 |___________________________________________________________________________|
*/
/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : shr                                                     |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 16 bit input var1 right var2 positions with    |
 |   sign extension. If var2 is negative, arithmetically shift var1 left by  |
 |   -var2 with sign extension. Saturate the result in case of underflows or |
 |   overflows.                                                              |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) shr (Word16 var1, Word16 var2)
{
    if (var2 < 0)
    {
        if (var2 < -16)
            var2 = -16;
        return shl_nocheck(var1, (Word16) -var2);
    }
    else
    {
        return shr_nocheck(var1, var2);
    }
}
/* ------------------------- End of shr() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : shl                                                     |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 16 bit input var1 left var2 positions.Zero fill|
 |   the var2 LSB of the result. If var2 is negative, arithmetically shift   |
 |   var1 right by -var2 with sign extension. Saturate the result in case of |
 |   underflows or overflows.                                                |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) shl (Word16 var1, Word16 var2)
{
    if (var2 < 0)
    {
        return shr_nocheck(var1, (Word16) -var2);
    }
    else
    {
        return shl_nocheck(var1, var2);
    }
}
/* ------------------------- End of shl() ------------------------- */



/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : mult                                                    |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |    Performs the multiplication of var1 by var2 and gives a 16 bit result  |
 |    which is scaled i.e.:                                                  |
 |             mult(var1,var2) = extract_l(L_shr((var1 times var2),15)) and  |
 |             mult(-32768,-32768) = 32767.                                  |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) mult (Word16 var1, Word16 var2)
{
    Word16 var_out;
    Word32 L_product;

    L_product = (Word32) var1 *(Word32) var2;

    L_product = (L_product & (Word32) 0xffff8000L) >> 15;

    if (L_product & (Word32) 0x00010000L)
        L_product = L_product | (Word32) 0xffff0000L;

    var_out = saturate (L_product);
#if (WMOPS)
    multiCounter[currCounter].mult++;
#endif
    return (var_out);
}
/* ------------------------- End of mult() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_msu                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Subtract the 32   |
 |   bit result to L_var3 with saturation, return a 32 bit result:           |
 |        L_msu(L_var3,var1,var2) = L_sub(L_var3,L_mult(var1,var2)).         |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_msu (Word32 L_var3, Word16 var1, Word16 var2)
{
    Word32 L_var_out;
    Word32 L_product;

    L_product = L_mult (var1, var2);
#if (WMOPS)
    multiCounter[currCounter].L_mult--;
#endif
    L_var_out = L_sub (L_var3, L_product);
#if (WMOPS)
    multiCounter[currCounter].L_sub--;
    multiCounter[currCounter].L_msu++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_msu() ------------------------- */

#if INCLUDE_UNSAFE
/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_macNs                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Add the 32 bit    |
 |   result to L_var3 without saturation, return a 32 bit result. Generate   |
 |   carry and overflow values :                                             |
 |        L_macNs(L_var3,var1,var2) = L_add_c(L_var3,L_mult(var1,var2)).     |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |                                                                           |
 |   Caution :                                                               |
 |                                                                           |
 |    In some cases the Carry flag has to be cleared or set before using     |
 |    operators which take into account its value.                           |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_macNs (Word32 L_var3, Word16 var1, Word16 var2)
{
    Word32 L_var_out;

    L_var_out = L_mult (var1, var2);
#if (WMOPS)
    multiCounter[currCounter].L_mult--;
#endif
    L_var_out = L_add_c (L_var3, L_var_out);
#if (WMOPS)
    multiCounter[currCounter].L_add_c--;
    multiCounter[currCounter].L_macNs++;
#endif
    return (L_var_out);
}
#endif
/* ------------------------- End of L_macNs() ------------------------- */

#if INCLUDE_UNSAFE
/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_msuNs                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Subtract the 32   |
 |   bit result from L_var3 without saturation, return a 32 bit result. Ge-  |
 |   nerate carry and overflow values :                                      |
 |        L_msuNs(L_var3,var1,var2) = L_sub_c(L_var3,L_mult(var1,var2)).     |
 |                                                                           |
 |   Complexity weight : 1                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |                                                                           |
 |   Caution :                                                               |
 |                                                                           |
 |    In some cases the Carry flag has to be cleared or set before using     |
 |    operators which take into account its value.                           |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_msuNs (Word32 L_var3, Word16 var1, Word16 var2)
{
    Word32 L_var_out;

    L_var_out = L_mult (var1, var2);
#if (WMOPS)
    multiCounter[currCounter].L_mult--;
#endif
    L_var_out = L_sub_c (L_var3, L_var_out);
#if (WMOPS)
    multiCounter[currCounter].L_sub_c--;
    multiCounter[currCounter].L_msuNs++;
#endif
    return (L_var_out);
}
#endif

/* ------------------------- End of L_msuNs() ------------------------- */


#if INCLUDE_UNSAFE
/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_add_c                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Performs 32 bits addition of the two 32 bits variables (L_var1+L_var2+C)|
 |   with carry. No saturation. Generate carry and Overflow values. The car- |
 |   ry and overflow values are binary variables which can be tested and as- |
 |   signed values.                                                          |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    L_var2   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |                                                                           |
 |   Caution :                                                               |
 |                                                                           |
 |    In some cases the Carry flag has to be cleared or set before using     |
 |    operators which take into account its value.                           |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_add_c (Word32 L_var1, Word32 L_var2)
{
    Word32 L_var_out;
    Word32 L_test;
    Flag carry_int = 0;

    L_var_out = L_var1 + L_var2 + GET_CARRY();

    L_test = L_var1 + L_var2;

    if ((L_var1 > 0) && (L_var2 > 0) && (L_test < 0))
    {
        SET_OVERFLOW(1);
        carry_int = 0;
    }
    else
    {
        if ((L_var1 < 0) && (L_var2 < 0))
        {
            if (L_test >= 0)
	    {
                SET_OVERFLOW(1);
                carry_int = 1;
	    }
            else
	    {
                SET_OVERFLOW(0);
                carry_int = 1;
	    }
        }
        else
        {
            if (((L_var1 ^ L_var2) < 0) && (L_test >= 0))
            {
                SET_OVERFLOW(0);
                carry_int = 1;
            }
            else
            {
                SET_OVERFLOW(0);
                carry_int = 0;
            }
        }
    }

    if (GET_CARRY())
    {
        if (L_test == MAX_32)
        {
            SET_OVERFLOW(1);
            SET_CARRY(carry_int);
        }
        else
        {
            if (L_test == (Word32) 0xFFFFFFFFL)
            {
                SET_CARRY(1);
            }
            else
            {
                SET_CARRY(carry_int);
            }
        }
    }
    else
    {
        SET_CARRY(carry_int);
    }

#if (WMOPS)
    multiCounter[currCounter].L_add_c++;
#endif
    return (L_var_out);
}
#endif

/* ------------------------- End of L_add_c() ------------------------- */

#if INCLUDE_UNSAFE
/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_sub_c                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Performs 32 bits subtraction of the two 32 bits variables with carry    |
 |   (borrow) : L_var1-L_var2-C. No saturation. Generate carry and Overflow  |
 |   values. The carry and overflow values are binary variables which can    |
 |   be tested and assigned values.                                          |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    L_var2   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |                                                                           |
 |   Caution :                                                               |
 |                                                                           |
 |    In some cases the Carry flag has to be cleared or set before using     |
 |    operators which take into account its value.                           |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_sub_c (Word32 L_var1, Word32 L_var2)
{
    Word32 L_var_out;
    Word32 L_test;
    Flag carry_int = 0;

    if (GET_CARRY())
    {
        SET_CARRY(0);
        if (L_var2 != MIN_32)
        {
            L_var_out = L_add_c (L_var1, -L_var2);
#if (WMOPS)
            multiCounter[currCounter].L_add_c--;
#endif
        }
        else
        {
            L_var_out = L_var1 - L_var2;
            if (L_var1 > 0L)
            {
                SET_OVERFLOW(1);
                SET_CARRY(0);
            }
        }
    }
    else
    {
        L_var_out = L_var1 - L_var2 - (Word32) 0X00000001L;
        L_test = L_var1 - L_var2;

        if ((L_test < 0) && (L_var1 > 0) && (L_var2 < 0))
        {
            SET_OVERFLOW(1);
            carry_int = 0;
        }
        else if ((L_test > 0) && (L_var1 < 0) && (L_var2 > 0))
        {
            SET_OVERFLOW(1);
            carry_int = 1;
        }
        else if ((L_test > 0) && ((L_var1 ^ L_var2) > 0))
        {
            SET_OVERFLOW(0);
            carry_int = 1;
        }
        if (L_test == MIN_32)
        {
            SET_OVERFLOW(1);
            SET_CARRY(carry_int);
        }
        else
        {
            SET_CARRY(carry_int);
        }
    }

#if (WMOPS)
    multiCounter[currCounter].L_sub_c++;
#endif
    return (L_var_out);
}
#endif
/* ------------------------- End of L_sub_c() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_negate                                                |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Negate the 32 bit variable L_var1 with saturation; saturate in the case |
 |   where input is -2147483648 (0x8000 0000).                               |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_negate (Word32 L_var1)
{
    Word32 L_var_out;

    L_var_out = (L_var1 == MIN_32) ? MAX_32 : -L_var1;
#if (WMOPS)
    multiCounter[currCounter].L_negate++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_negate() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : mult_r                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Same as mult with rounding, i.e.:                                       |
 |     mult_r(var1,var2) = extract_l(L_shr(((var1 * var2) + 16384),15)) and  |
 |     mult_r(-32768,-32768) = 32767.                                        |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) mult_r (Word16 var1, Word16 var2)
{
    Word16 var_out;
    Word32 L_product_arr;

    L_product_arr = (Word32) var1 *(Word32) var2;       /* product */
    L_product_arr += (Word32) 0x00004000L;      /* round */
    L_product_arr &= (Word32) 0xffff8000L;
    L_product_arr >>= 15;       /* shift */

    if (L_product_arr & (Word32) 0x00010000L)   /* sign extend when necessary */
    {
        L_product_arr |= (Word32) 0xffff0000L;
    }
    var_out = saturate (L_product_arr);
#if (WMOPS)
    multiCounter[currCounter].mult_r++;
#endif
    return (var_out);
}
/* ------------------------- End of mult_r() ------------------------- */



/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : shr_r                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Same as shr(var1,var2) but with rounding. Saturate the result in case of|
 |   underflows or overflows :                                               |
 |    - If var2 is greater than zero :                                       |
 |          if (sub(shl(shr(var1,var2),1),shr(var1,sub(var2,1))))            |
 |          is equal to zero                                                 |
 |                     then                                                  |
 |                     shr_r(var1,var2) = shr(var1,var2)                     |
 |                     else                                                  |
 |                     shr_r(var1,var2) = add(shr(var1,var2),1)              |
 |    - If var2 is less than or equal to zero :                              |
 |                     shr_r(var1,var2) = shr(var1,var2).                    |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) shr_r (Word16 var1, Word16 var2)
{
    Word16 var_out;

    if (var2 > 15)
    {
        var_out = 0;
    }
    else
    {
        var_out = shr (var1, var2);
#if (WMOPS)
        multiCounter[currCounter].shr--;
#endif

        if (var2 > 0)
        {
            if ((var1 & ((Word16) 1 << (var2 - 1))) != 0)
            {
                var_out++;
            }
        }
    }
#if (WMOPS)
    multiCounter[currCounter].shr_r++;
#endif
    return (var_out);
}
/* ------------------------- End of shr_r() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : mac_r                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Add the 32 bit    |
 |   result to L_var3 with saturation. Round the LS 16 bits of the result    |
 |   into the MS 16 bits with saturation and shift the result right by 16.   |
 |   Return a 16 bit result.                                                 |
 |            mac_r(L_var3,var1,var2) = round(L_mac(L_var3,var1,var2))       |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 8000 <= L_var_out <= 0x0000 7fff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) mac_r (Word32 L_var3, Word16 var1, Word16 var2)
{
    Word16 var_out;

    L_var3 = L_mac (L_var3, var1, var2);
#if (WMOPS)
    multiCounter[currCounter].L_mac--;
#endif
    L_var3 = L_add (L_var3, (Word32) 0x00008000L);
#if (WMOPS)
    multiCounter[currCounter].L_add--;
#endif
    var_out = extract_h (L_var3);
#if (WMOPS)
    multiCounter[currCounter].extract_h--;
    multiCounter[currCounter].mac_r++;
#endif
    return (var_out);
}
/* ------------------------- End of mac_r() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : msu_r                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiply var1 by var2 and shift the result left by 1. Subtract the 32   |
 |   bit result to L_var3 with saturation. Round the LS 16 bits of the res-  |
 |   ult into the MS 16 bits with saturation and shift the result right by   |
 |   16. Return a 16 bit result.                                             |
 |            msu_r(L_var3,var1,var2) = round(L_msu(L_var3,var1,var2))       |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 8000 <= L_var_out <= 0x0000 7fff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) msu_r (Word32 L_var3, Word16 var1, Word16 var2)
{
    Word16 var_out;

    L_var3 = L_msu (L_var3, var1, var2);
#if (WMOPS)
    multiCounter[currCounter].L_msu--;
#endif
    L_var3 = L_add (L_var3, (Word32) 0x00008000L);
#if (WMOPS)
    multiCounter[currCounter].L_add--;
#endif
    var_out = extract_h (L_var3);
#if (WMOPS)
    multiCounter[currCounter].extract_h--;
    multiCounter[currCounter].msu_r++;
#endif
    return (var_out);
}
/* ------------------------- End of msu_r() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_deposit_h                                             |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Deposit the 16 bit var1 into the 16 MS bits of the 32 bit output. The   |
 |   16 LS bits of the output are zeroed.                                    |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var_out <= 0x7fff 0000.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_deposit_h (Word16 var1)
{
    Word32 L_var_out;

    L_var_out = (Word32) var1 << 16;
#if (WMOPS)
    multiCounter[currCounter].L_deposit_h++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_deposit_h() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_deposit_l                                             |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Deposit the 16 bit var1 into the 16 LS bits of the 32 bit output. The   |
 |   16 MS bits of the output are sign extended.                             |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0xFFFF 8000 <= var_out <= 0x0000 7fff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_deposit_l (Word16 var1)
{
    Word32 L_var_out;

    L_var_out = (Word32) var1;
#if (WMOPS)
    multiCounter[currCounter].L_deposit_l++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_deposit_l() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_shr_r                                                 |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Same as L_shr(L_var1,var2) but with rounding. Saturate the result in    |
 |   case of underflows or overflows :                                       |
 |    - If var2 is greater than zero :                                       |
 |          if (L_sub(L_shl(L_shr(L_var1,var2),1),L_shr(L_var1,sub(var2,1))))|
 |          is equal to zero                                                 |
 |                     then                                                  |
 |                     L_shr_r(L_var1,var2) = L_shr(L_var1,var2)             |
 |                     else                                                  |
 |                     L_shr_r(L_var1,var2) = L_add(L_shr(L_var1,var2),1)    |
 |    - If var2 is less than or equal to zero :                              |
 |                     L_shr_r(L_var1,var2) = L_shr(L_var1,var2).            |
 |                                                                           |
 |   Complexity weight : 3                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1                                                                 |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var1 <= 0x7fff ffff.                   |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var_out <= 0x7fff ffff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_shr_r (Word32 L_var1, Word16 var2)
{
    Word32 L_var_out;

    if (var2 > 31)
    {
        L_var_out = 0;
    }
    else
    {
        L_var_out = L_shr (L_var1, var2);
#if (WMOPS)
        multiCounter[currCounter].L_shr--;
#endif
        if (var2 > 0)
        {
            if ((L_var1 & ((Word32) 1 << (var2 - 1))) != 0)
            {
                L_var_out++;
            }
        }
    }
#if (WMOPS)
    multiCounter[currCounter].L_shr_r++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_shr_r() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_abs                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |    Absolute value of L_var1; Saturate in case where the input is          |
 |                                                               -214783648  |
 |                                                                           |
 |   Complexity weight : 3                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1                                                                 |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var1 <= 0x7fff ffff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x0000 0000 <= var_out <= 0x7fff ffff.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_abs (Word32 L_var1)
{
    Word32 L_var_out;

    if (L_var1 == MIN_32)
    {
        L_var_out = MAX_32;
    }
    else
    {
        if (L_var1 < 0)
        {
            L_var_out = -L_var1;
        }
        else
        {
            L_var_out = L_var1;
        }
    }

#if (WMOPS)
    multiCounter[currCounter].L_abs++;
#endif
    return (L_var_out);
}
/* ------------------------- End of L_abs() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : norm_s                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Produces the number of left shift needed to normalize the 16 bit varia- |
 |   ble var1 for positive values on the interval with minimum of 16384 and  |
 |   maximum of 32767, and for negative values on the interval with minimum  |
 |   of -32768 and maximum of -16384; in order to normalize the result, the  |
 |   following operation must be done :                                      |
 |                    norm_var1 = shl(var1,norm_s(var1)).                    |
 |                                                                           |
 |   Complexity weight : 15                                                  |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 0000 <= var_out <= 0x0000 000f.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) norm_s (Word16 var1)
{
    Word16 var_out;

    if (var1 == 0)
    {
        var_out = 0;
    }
    else
    {
        if ((UWord16)var1 == (UWord16)0xffff)
        {
            var_out = 15;
        }
        else
        {
            if (var1 < 0)
            {
                var1 = (Word16)(~var1);
            }
            for (var_out = 0; var1 < 0x4000; var_out++)
            {
                var1 <<= 1;
            }
        }
    }

#if (WMOPS)
    multiCounter[currCounter].norm_s++;
#endif
    return (var_out);
}
/* ------------------------- End of norm_s() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : div_s                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Produces a result which is the fractional integer division of var1  by  |
 |   var2; var1 and var2 must be positive and var2 must be greater or equal  |
 |   to var1; the result is positive (leading bit equal to 0) and truncated  |
 |   to 16 bits.                                                             |
 |   If var1 = var2 then div(var1,var2) = 32767.                             |
 |                                                                           |
 |   Complexity weight : 18                                                  |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    var1                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 0000 <= var1 <= var2 and var2 != 0.            |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : var1 <= var2 <= 0x0000 7fff and var2 != 0.            |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 0000 <= var_out <= 0x0000 7fff.                |
 |             It's a Q15 value (point between b15 and b14).                 |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) div_s (Word16 var1, Word16 var2)
{
    Word16 var_out = 0;
    Word16 iteration;
    Word32 L_num;
    Word32 L_denom;

    if ((var1 > var2) || (var1 < 0) || (var2 < 0))
    {
        //printf ("Division Error var1=%d  var2=%d\n", var1, var2);
        //abort(); /* exit (0); */
	pj_assert(!"Division Error");
    }
    if (var2 == 0)
    {
        //printf ("Division by 0, Fatal error \n");
        //abort(); /* exit (0); */
	assert(!"Division by 0");
    }
    if (var1 == 0)
    {
        var_out = 0;
    }
    else
    {
        if (var1 == var2)
        {
            var_out = MAX_16;
        }
        else
        {
            L_num = L_deposit_l (var1);
#if (WMOPS)
            multiCounter[currCounter].L_deposit_l--;
#endif
            L_denom = L_deposit_l (var2);
#if (WMOPS)
            multiCounter[currCounter].L_deposit_l--;
#endif

            for (iteration = 0; iteration < 15; iteration++)
            {
                var_out <<= 1;
                L_num <<= 1;

                if (L_num >= L_denom)
                {
                    L_num = L_sub (L_num, L_denom);
#if (WMOPS)
                    multiCounter[currCounter].L_sub--;
#endif
                    var_out = add (var_out, 1);
#if (WMOPS)
                    multiCounter[currCounter].add--;
#endif
                }
            }
        }
    }

#if (WMOPS)
    multiCounter[currCounter].div_s++;
#endif
    return (var_out);
}
/* ------------------------- End of div_s() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : norm_l                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Produces the number of left shifts needed to normalize the 32 bit varia-|
 |   ble L_var1 for positive values on the interval with minimum of          |
 |   1073741824 and maximum of 2147483647, and for negative values on the in-|
 |   terval with minimum of -2147483648 and maximum of -1073741824; in order |
 |   to normalize the result, the following operation must be done :         |
 |                   norm_L_var1 = L_shl(L_var1,norm_l(L_var1)).             |
 |                                                                           |
 |   Complexity weight : 30                                                  |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1                                                                 |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var1 <= 0x7fff ffff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x0000 0000 <= var_out <= 0x0000 001f.                |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) norm_l (Word32 L_var1)
{
    Word16 var_out;

    if (L_var1 == 0)
    {
        var_out = 0;
    }
    else
    {
        if (L_var1 == (Word32) 0xffffffffL)
        {
            var_out = 31;
        }
        else
        {
            if (L_var1 < 0)
            {
                L_var1 = ~L_var1;
            }
            for (var_out = 0; L_var1 < (Word32) 0x40000000L; var_out++)
            {
                L_var1 <<= 1;
            }
        }
    }

#if (WMOPS)
    multiCounter[currCounter].norm_l++;
#endif
    return (var_out);
}
/* ------------------------- End of norm_l() ------------------------- */


/*
  *****************************************************************
   Additional operators extracted from the G.723.1 Library
   Adapted for WMOPS calculations
  *****************************************************************
*/

/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : L_mls                                                   |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Multiplies a 16 bit word v by a 32 bit word Lv and returns a 32 bit     |
 |   word (multiplying 16 by 32 bit words gives 48 bit word; the function    |
 |   extracts the 32 MSB and shift the result to the left by 1).             |
 |                                                                           |
 |   A 32 bit word can be written as                                         |
 |    Lv = a  +  b * 2^16                                                    |
 |   where a= unsigned 16 LSBs and b= signed 16 MSBs.                        |
 |   The function returns v * Lv  /  2^15  which is equivalent to            |
 |        a*v / 2^15 + b*v*2                                                 |
 |                                                                           |
 |   Complexity weight : 6 [to be confirmed]                                 |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |   Lv                                                                      |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var1 <= 0x7fff ffff.                   |
 |   v                                                                       |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0x8000 <= var1 <= 0x7fff.                             |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    var_out                                                                |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var_out <= 0x7fff ffff.                |
 |                                                                           |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(Word32) L_mls (Word32 Lv, Word16 v)
{
   Word32   Temp  ;

   Temp = Lv & (Word32) 0x0000ffff ;
   Temp = Temp * (Word32) v ;
   Temp = L_shr_nocheck( Temp, (Word16) 15 ) ;
   Temp = L_mac( Temp, v, extract_h(Lv) ) ;

#if (WMOPS)
   multiCounter[currCounter].L_shr--;
   multiCounter[currCounter].L_mac--;
   multiCounter[currCounter].extract_h--;
   multiCounter[currCounter].L_mls++;
#endif

   return Temp ;
}
/* ------------------------- End of L_mls() ------------------------- */


/*__________________________________________________________________________
|                                                                           |
|   Function Name : div_l                                                   |
|                                                                           |
|   Purpose :                                                               |
|                                                                           |
|   Produces a result which is the fractional integer division of L_var1 by|
|   var2; L_var1 and var2 must be positive and var2 << 16 must be greater or|
|   equal to L_var1; the result is positive (leading bit equal to 0) and    |
|   truncated to 16 bits.                                                   |
|   If L_var1 == var2 << 16 then div_l(L_var1,var2) = 32767.                |
|                                                                           |
|   Complexity weight : 20                                                  |
|                                                                           |
|   Inputs :                                                                |
|                                                                           |
|    L_var1                                                                 |
|             32 bit long signed integer (Word32) whose value falls in the  |
|             range : 0x0000 0000 <= var1 <= (var2 << 16)  and var2 != 0.   |
|             L_var1 must be considered as a Q.31 value                     |
|                                                                           |
|    var2                                                                   |
|             16 bit short signed integer (Word16) whose value falls in the |
|             range : var1 <= (var2<< 16) <= 0x7fff0000 and var2 != 0.      |
|             var2 must be considered as a Q.15 value                       |
|                                                                           |
|   Outputs :                                                               |
|                                                                           |
|    none                                                                   |
|                                                                           |
|   Return Value :                                                          |
|                                                                           |
|    var_out                                                                |
|             16 bit short signed integer (Word16) whose value falls in the |
|             range : 0x0000 0000 <= var_out <= 0x0000 7fff.                |
|             It's a Q15 value (point between b15 and b14).                 |
|___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) div_l (Word32  L_num, Word16 den)
{
    Word16   var_out = (Word16)0;
    Word32   L_den;
    Word16   iteration;

#if (WMOPS)
    multiCounter[currCounter].div_l++;
#endif

    if ( den == (Word16) 0 ) {
        //printf("Division by 0 in div_l, Fatal error \n");
        //exit(0);
	assert(!"Division by 0");
    }

    if ( (L_num < (Word32) 0) || (den < (Word16) 0) ) {
        //printf("Division Error in div_l, Fatal error \n");
        //exit(0);
	assert(!"Division Error");
    }

    L_den = L_deposit_h( den ) ;
#if (WMOPS)
    multiCounter[currCounter].L_deposit_h--;
#endif

    if ( L_num >= L_den ){
        return MAX_16 ;
    }
    else {
        L_num = L_shr_nocheck(L_num, (Word16)1) ;
        L_den = L_shr_nocheck(L_den, (Word16)1);
#if (WMOPS)
        multiCounter[currCounter].L_shr-=2;
#endif
        for(iteration=(Word16)0; iteration< (Word16)15;iteration++) {
            var_out = shl_nocheck( var_out, (Word16)1);
            L_num   = L_shl_nocheck( L_num, (Word16)1);
#if (WMOPS)
            multiCounter[currCounter].shl--;
            multiCounter[currCounter].L_shl--;
#endif
            if (L_num >= L_den) {
                L_num = L_sub(L_num,L_den);
                var_out = add(var_out, (Word16)1);
#if (WMOPS)
            multiCounter[currCounter].L_sub--;
            multiCounter[currCounter].add--;
#endif
            }
        }

        return var_out;
    }
}
/* ------------------------- End of div_l() ------------------------- */


/*__________________________________________________________________________
|                                                                           |
|   Function Name : i_mult                                                  |
|                                                                           |
|   Purpose :                                                               |
|                                                                           |
|   Integer 16-bit multiplication. No overflow protection is performed if   |
|   ORIGINAL_G7231 is defined.                                              |
|                                                                           |
|   Complexity weight : TBD                                                 |
|                                                                           |
|   Inputs :                                                                |
|                                                                           |
|    a                                                                      |
|             16 bit short signed integer (Word16).                         |
|                                                                           |
|    b                                                                      |
|             16 bit short signed integer (Word16).                         |
|                                                                           |
|   Outputs :                                                               |
|                                                                           |
|    none                                                                   |
|                                                                           |
|   Return Value :                                                          |
|                                                                           |
|             16 bit short signed integer (Word16). No overflow checks      |
|             are performed if ORIGINAL_G7231 is defined.                   |
|___________________________________________________________________________|
*/
LIBG7221_DEF(Word16) i_mult (Word16 a, Word16 b)
{
#ifdef ORIGINAL_G7231
   return a*b ;
#else
   Word32 register c=a*b;
#if (WMOPS)
    multiCounter[currCounter].i_mult++;
#endif
   return saturate(c) ;
#endif
}
/* ------------------------- End of i_mult() ------------------------- */


/*
   ********************************************************************** 
   The following three operators are not part of the original 
   G.729/G.723.1 set of basic operators and implement shiftless
   accumulation operation.
   ********************************************************************** 
*/

/*___________________________________________________________________________
 |
 |   Function Name : L_mult0
 |
 |   Purpose :
 |
 |   L_mult0 is the 32 bit result of the multiplication of var1 times var2
 |   without one left shift.
 |
 |   Complexity weight : 1
 |
 |   Inputs :
 |
 |    var1     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |    var2     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |   Return Value :
 |
 |    L_var_out
 |             32 bit long signed integer (Word32) whose value falls in the
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.
 |___________________________________________________________________________
*/
LIBG7221_DEF(Word32) L_mult0 (Word16 var1,Word16 var2)
{
  Word32 L_var_out;

  L_var_out = (Word32)var1 * (Word32)var2;

#if (WMOPS)
    multiCounter[currCounter].L_mult0++;
#endif
  return(L_var_out);
}
/* ------------------------- End of L_mult0() ------------------------- */


/*___________________________________________________________________________
 |
 |   Function Name : L_mac0
 |
 |   Purpose :
 |
 |   Multiply var1 by var2 (without left shift) and add the 32 bit result to
 |   L_var3 with saturation, return a 32 bit result:
 |        L_mac0(L_var3,var1,var2) = L_add(L_var3,(L_mult0(var1,var2)).
 |
 |   Complexity weight : 1
 |
 |   Inputs :
 |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.
 |
 |    var1     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |    var2     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |   Return Value :
 |
 |    L_var_out
 |             32 bit long signed integer (Word32) whose value falls in the
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.
 |___________________________________________________________________________
*/
LIBG7221_DEF(Word32) L_mac0 (Word32 L_var3, Word16 var1, Word16 var2)
{
  Word32 L_var_out;
  Word32 L_product;

  L_product = L_mult0(var1,var2);
  L_var_out = L_add(L_var3,L_product);

#if (WMOPS)
    multiCounter[currCounter].L_mac0++;
    multiCounter[currCounter].L_mult0--;
    multiCounter[currCounter].L_add--;
#endif
  return(L_var_out);
}
/* ------------------------- End of L_mac0() ------------------------- */


/*___________________________________________________________________________
 |
 |   Function Name : L_msu0
 |
 |   Purpose :
 |
 |   Multiply var1 by var2 (without left shift) and subtract the 32 bit
 |   result to L_var3 with saturation, return a 32 bit result:
 |        L_msu0(L_var3,var1,var2) = L_sub(L_var3,(L_mult0(var1,var2)).
 |
 |   Complexity weight : 1
 |
 |   Inputs :
 |
 |    L_var3   32 bit long signed integer (Word32) whose value falls in the
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.
 |
 |    var1     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |    var2     16 bit short signed integer (Word16) whose value falls in the
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.
 |
 |   Return Value :
 |
 |    L_var_out
 |             32 bit long signed integer (Word32) whose value falls in the
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.
 |___________________________________________________________________________
*/
LIBG7221_DEF(Word32) L_msu0 (Word32 L_var3, Word16 var1, Word16 var2)
{
  Word32 L_var_out;
  Word32 L_product;

  L_product = L_mult0(var1,var2);
  L_var_out = L_sub(L_var3,L_product);

#if (WMOPS)
    multiCounter[currCounter].L_msu0++;
    multiCounter[currCounter].L_mult0--;
    multiCounter[currCounter].L_sub--;
#endif
  return(L_var_out);
}
/* ------------------------- End of L_msu0() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : LU_shl                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 32 bit input L_var1 left var2 positions. Zero  |
 |   fill the var2 LSB of the result. If var2 is negative, arithmetically    |
 |   shift L_var1 right by -var2 with sign extension. Saturate the result in |
 |   case of underflows or overflows.                                        |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(UWord32) LU_shl (UWord32 L_var1, Word16 var2)
{
    Word16 neg_var2;
    UWord32 L_var_out = 0;

    if (var2 <= 0)
    {
        if (var2 < -32)
            var2 = -32;
        neg_var2 = negate(var2);
        L_var_out = LU_shr (L_var1, neg_var2);
#if (WMOPS)
        multiCounter[currCounter].negate--;
        multiCounter[currCounter].LU_shr--;
#endif
    }
    else
    {
        for (; var2 > 0; var2--)
        {
            if (L_var1 > (UWord32) 0X7fffffffL)
            {
                SET_OVERFLOW(1);
                L_var_out = UMAX_32;
                break;
            }
            else
            {
                if (L_var1 < (UWord32) 0x00000001L)
                {
                    SET_OVERFLOW(1);
                    L_var_out = (UWord32)MIN_32;
                    break;
                }
            }
            L_var1 *= 2;
            L_var_out = L_var1;
        }
    }
#if (WMOPS)
    multiCounter[currCounter].LU_shl++;
#endif
    return (L_var_out);
}
/* ------------------------- End of LU_shl() ------------------------- */


/*___________________________________________________________________________
 |                                                                           |
 |   Function Name : LU_shr                                                  |
 |                                                                           |
 |   Purpose :                                                               |
 |                                                                           |
 |   Arithmetically shift the 32 bit input L_var1 right var2 positions with  |
 |   sign extension. If var2 is negative, arithmetically shift L_var1 left   |
 |   by -var2 and zero fill the -var2 LSB of the result. Saturate the result |
 |   in case of underflows or overflows.                                     |
 |                                                                           |
 |   Complexity weight : 2                                                   |
 |                                                                           |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1   32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var3 <= 0x7fff ffff.                 |
 |                                                                           |
 |    var2                                                                   |
 |             16 bit short signed integer (Word16) whose value falls in the |
 |             range : 0xffff 8000 <= var1 <= 0x0000 7fff.                   |
 |                                                                           |
 |   Outputs :                                                               |
 |                                                                           |
 |    none                                                                   |
 |                                                                           |
 |   Return Value :                                                          |
 |                                                                           |
 |    L_var_out                                                              |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= L_var_out <= 0x7fff ffff.              |
 |___________________________________________________________________________|
*/
LIBG7221_DEF(UWord32) LU_shr (UWord32 L_var1, Word16 var2)
{
    Word16  neg_var2;
    UWord32 L_var_out;

    if (var2 < 0)
    {
        if (var2 < -32)
            var2 = -32;
        neg_var2 = negate(var2);
        L_var_out = LU_shl (L_var1, neg_var2);
#if (WMOPS)
        multiCounter[currCounter].negate--;
        multiCounter[currCounter].LU_shl--;
#endif
    }
    else
    {
        if (var2 >= 32)
        {
            L_var_out = 0L;
        }
        else
        {
            L_var_out = L_var1 >> var2;
        }
    }
#if (WMOPS)
    multiCounter[currCounter].LU_shr++;
#endif
    return (L_var_out);
}
/* ------------------------- End of LU_shr() ------------------------- */

#endif /* PJMEDIA_LIBG7221_FUNCS_INLINED */

/* ************************** END OF BASOP32.C ************************** */
