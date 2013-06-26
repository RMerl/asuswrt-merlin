/* 
   base160 code used by ldb_sqlite3

   Copyright (C) 2004 Derrell Lipman

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/


/*
 * ldb_sqlite3_base160()
 *
 * Convert an integer value to a string containing the base 160 representation
 * of the integer.  We always convert to a string representation that is 4
 * bytes in length, and we always null terminate.
 *
 * Parameters:
 *   val --
 *     The value to be converted
 *
 *   result --
 *     Buffer in which the result is to be placed
 *
 * Returns:
 *   nothing
 */
static unsigned char        base160tab[161] =
{
    48 , 49 , 50 , 51 , 52 , 53 , 54 , 55 , 56 , 57 , /* 0-9 */
    58 , 59 , 65 , 66 , 67 , 68 , 69 , 70 , 71 , 72 , /* : ; A-H */
    73 , 74 , 75 , 76 , 77 , 78 , 79 , 80 , 81 , 82 , /* I-R */
    83 , 84 , 85 , 86 , 87 , 88 , 89 , 90 , 97 , 98 , /* S-Z , a-b */
    99 , 100, 101, 102, 103, 104, 105, 106, 107, 108, /* c-l */
    109, 110, 111, 112, 113, 114, 115, 116, 117, 118, /* m-v */
    119, 120, 121, 122, 160, 161, 162, 163, 164, 165, /* w-z, latin1 */
    166, 167, 168, 169, 170, 171, 172, 173, 174, 175, /* latin1 */
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, /* latin1 */
    186, 187, 188, 189, 190, 191, 192, 193, 194, 195, /* latin1 */
    196, 197, 198, 199, 200, 201, 202, 203, 204, 205, /* latin1 */
    206, 207, 208, 209, 210, 211, 212, 213, 214, 215, /* latin1 */
    216, 217, 218, 219, 220, 221, 222, 223, 224, 225, /* latin1 */
    226, 227, 228, 229, 230, 231, 232, 233, 234, 235, /* latin1 */
    236, 237, 238, 239, 240, 241, 242, 243, 244, 245, /* latin1 */
    246, 247, 248, 249, 250, 251, 252, 253, 254, 255, /* latin1 */
    '\0'
};


/*
 * lsqlite3_base160()
 *
 * Convert an unsigned long integer into a base160 representation of the
 * number.
 *
 * Parameters:
 *   val --
 *     value to be converted
 *
 *   result --
 *     character array, 5 bytes long, into which the base160 representation
 *     will be placed.  The result will be a four-digit representation of the
 *     number (with leading zeros prepended as necessary), and null
 *     terminated.
 *
 * Returns:
 *   Nothing
 */
void
lsqlite3_base160(unsigned long val,
                 unsigned char result[5])
{
    int             i;

    for (i = 3; i >= 0; i--) {
        
        result[i] = base160tab[val % 160];
        val /= 160;
    }

    result[4] = '\0';
}


/*
 * lsqlite3_base160Next()
 *
 * Retrieve the next-greater number in the base160 sequence for the terminal
 * tree node (the last four digits).  Only one tree level (four digits) are
 * operated on.
 *
 * Parameters:
 *   base160 -- a character array containing either an empty string (in which
 *              case no operation is performed), or a string of base160 digits
 *              with a length of a multiple of four digits.
 *
 *              Upon return, the trailing four digits (one tree level) will
 *              have been incremented by 1.
 *
 * Returns:
 *   base160 -- the modified array
 */
char *
lsqlite3_base160Next(char base160[])
{
    int             i;
    int             len;
    unsigned char * pTab;
    char *          pBase160 = base160;

    /*
     * We need a minimum of four digits, and we will always get a multiple of
     * four digits.
     */
    if ((len = strlen(pBase160)) >= 4)
    {
        pBase160 += strlen(pBase160) - 1;

        /* We only carry through four digits: one level in the tree */
        for (i = 0; i < 4; i++) {

            /* What base160 value does this digit have? */
            pTab = strchr(base160tab, *pBase160);

            /* Is there a carry? */
            if (pTab < base160tab + sizeof(base160tab) - 1) {

                /* Nope.  Just increment this value and we're done. */
                *pBase160 = *++pTab;
                break;
            } else {

                /*
                 * There's a carry.  This value gets base160tab[0], we
                 * decrement the buffer pointer to get the next higher-order
                 * digit, and continue in the loop.
                 */
                *pBase160-- = base160tab[0];
            }
        }
    }

    return base160;
}
