/**
 * crypto.h - Exports for dealing with encrypted files.  Part of the
 *            Linux-NTFS project.
 *
 * Copyright (c) 2007 Yura Pakhuchiy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_CRYPTO_H
#define _NTFS_CRYPTO_H

extern ntfschar NTFS_EFS[5];

/*
 * This is our Big Secret (TM) structure, so do not allow anyone even read it
 * values. ;-) In fact, it is private because exist only in libntfs version
 * compiled with cryptography support, so users can not depend on it.
 */
typedef struct _ntfs_crypto_attr ntfs_crypto_attr;

/*
 * These functions should not be used directly. They are called for encrypted
 * attributes from corresponding functions without _crypto_ part.
 */

extern int ntfs_crypto_attr_open(ntfs_attr *na);
extern void ntfs_crypto_attr_close(ntfs_attr *na);

extern s64 ntfs_crypto_attr_pread(ntfs_attr *na, const s64 pos, s64 count,
		void *b);

#endif /* _NTFS_CRYPTO_H */
