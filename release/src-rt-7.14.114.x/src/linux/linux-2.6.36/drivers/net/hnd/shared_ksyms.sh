#!/bin/sh
#
# Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.      
#       
# Permission to use, copy, modify, and/or distribute this software for any      
# purpose with or without fee is hereby granted, provided that the above      
# copyright notice and this permission notice appear in all copies.      
#       
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES      
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF      
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY      
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES      
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION      
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN      
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.      
#
# $Id: shared_ksyms.sh,v 1.2 2008-12-05 20:10:41 $
#

cat <<EOF
#include <linux/version.h>
#include <linux/module.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
EOF

for file in $* ; do
    ${NM} $file | sed -ne 's/[0-9A-Fa-f]* [BDRT] \([^ ]*\)/extern void \1; EXPORT_SYMBOL(\1);/p'
done
