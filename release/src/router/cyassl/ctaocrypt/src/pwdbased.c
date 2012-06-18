/* pwdbased.c
 *
 * Copyright (C) 2006-2010 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */


#ifndef NO_PWDBASED

#include "pwdbased.h"
#include <string.h>


int PBKDF1(byte* output, const byte* passwd, int pLen, const byte* salt,
           int sLen, int iterations, int kLen, int hashType)
{
    Md5  md5;
    Sha  sha;
    int  hLen = (hashType == MD5) ? MD5_DIGEST_SIZE : SHA_DIGEST_SIZE;
    int  i;
    byte buffer[SHA_DIGEST_SIZE];  /* max size */

    if (hashType != MD5 && hashType != SHA)
        return -1;

    if (kLen > hLen)
        return -1;

    if (iterations < 1)
        return -1;

    if (hashType == MD5) {
        InitMd5(&md5);
        Md5Update(&md5, passwd, pLen);
        Md5Update(&md5, salt,   sLen);
        Md5Final(&md5,  buffer);
    }
    else {
        InitSha(&sha);
        ShaUpdate(&sha, passwd, pLen);
        ShaUpdate(&sha, salt,   sLen);
        ShaFinal(&sha,  buffer);
    }

    for (i = 1; i < iterations; i++) {
        if (hashType == MD5) {
            Md5Update(&md5, buffer, hLen);
            Md5Final(&md5,  buffer);
        }
        else {
            ShaUpdate(&sha, buffer, hLen);
            ShaFinal(&sha,  buffer);
        }
    }
    memcpy(output, buffer, kLen);

    return 0;
}


#endif /* NO_PWDBASED */

