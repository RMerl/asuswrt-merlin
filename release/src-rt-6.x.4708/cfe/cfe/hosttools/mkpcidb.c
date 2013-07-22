/*  *********************************************************************
    *  Broadcom Common Firmware Environment (CFE)
    *
    *  PCI Table Generator			File: mkpcidb.c
    *  
    *  Author:  Mitch Lichtenberg (mpl@broadcom.com)
    *  
    *  This program munges the PCI table into a form that uses
    *  fewer embedded pointers.  Pointers are evil for the
    *  relocatable version of CFE since they chew up valuable
    *  initialized data segment space, and we only have
    *  64KB of that.
    *  
    *********************************************************************  
    *
    *  Copyright 2000,2001,2002,2003
    *  Broadcom Corporation. All rights reserved.
    *  
    *  This software is furnished under license and may be used and 
    *  copied only in accordance with the following terms and 
    *  conditions.  Subject to these conditions, you may download, 
    *  copy, install, use, modify and distribute modified or unmodified 
    *  copies of this software in source and/or binary form.  No title 
    *  or ownership is transferred hereby.
    *  
    *  1) Any source code used, modified or distributed must reproduce 
    *     and retain this copyright notice and list of conditions 
    *     as they appear in the source file.
    *  
    *  2) No right is granted to use any trade name, trademark, or 
    *     logo of Broadcom Corporation.  The "Broadcom Corporation" 
    *     name may not be used to endorse or promote products derived 
    *     from this software without the prior written permission of 
    *     Broadcom Corporation.
    *  
    *  3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR
    *     IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED
    *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    *     PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
    *     SHALL BROADCOM BE LIABLE FOR ANY DAMAGES WHATSOEVER, AND IN 
    *     PARTICULAR, BROADCOM SHALL NOT BE LIABLE FOR DIRECT, INDIRECT,
    *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
    *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    *     GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    *     BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
    *     OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
    *     TORT (INCLUDING NEGLIGENCE OR OTHERWISE), EVEN IF ADVISED OF 
    *     THE POSSIBILITY OF SUCH DAMAGE.
    ********************************************************************* */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef unsigned short pci_vendor_id_t;
typedef unsigned short pci_product_id_t;

struct pci_knowndev {
    pci_vendor_id_t     vendor;
    pci_product_id_t    product;
    int                 flags;
    char                *vendorname, *productname;
};

#include "../pci/pcidevs.h"
#define PCI_KNOWNDEV_NOPROD 0x01
#include "../pci/pcidevs_data.h" 



struct pci_knowndev2 {
    pci_vendor_id_t vendor;
    pci_product_id_t product;
    int	flags;
    int vendorname;
    int productname;
};

#define MAXPCIDEVS 5000
#define MAXSTRINGTABLE (1024*1024)

struct pci_knowndev2 knowndevs[MAXPCIDEVS];
char stringtable[MAXSTRINGTABLE];
int curstringptr = 0;

int allocstring(char *string)
{
    int ptr;

    if (!string) return -1;

    ptr = curstringptr;
    strcpy(&stringtable[ptr],string);
    curstringptr += strlen(string)+1;
    return ptr;
}

int main(int argc,char *argv[])
{
    struct pci_knowndev2 *outdev;
    const struct pci_knowndev *indev;
    int cnt = 0;
    int idx;

    indev = pci_knowndevs;
    outdev = knowndevs;
    cnt = 0;

    while (indev->vendorname) {
	outdev->vendor = indev->vendor;
	outdev->product = indev->product;
	outdev->flags = indev->flags;
	outdev->vendorname = allocstring(indev->vendorname);
	outdev->productname = allocstring(indev->productname);
	cnt++;
	indev++;
	outdev++;
	}

    outdev->vendor = 0;
    outdev->product = 0;
    outdev->flags = 0;
    outdev->vendorname = -1;
    outdev->productname = -1;
    cnt++;

    fprintf(stderr,"%d total devices (%d bytes), %d bytes of strings\n",
	   cnt,cnt*sizeof(struct pci_knowndev2),curstringptr);

    printf("\n\n\n");
    printf("const static struct pci_knowndev2 _pci_knowndevs[] __attribute__ ((section (\".text\"))) = {\n");
    for (idx = 0; idx < cnt; idx++) {
	printf("\t{0x%04X,0x%04X,0x%08X,%d,%d},\n",
	       knowndevs[idx].vendor,
	       knowndevs[idx].product,
	       knowndevs[idx].flags,
	       knowndevs[idx].vendorname,
	       knowndevs[idx].productname);
	}
    printf("};\n\n\n");
    printf("const static char _pci_knowndevs_text[] __attribute__ ((section (\".text\"))) = {\n");
    for (idx = 0; idx < curstringptr; idx++) {
	if ((idx % 16) == 0) printf("\t");
	printf("0x%02X,",stringtable[idx]);
	if ((idx % 16) == 15) printf("\n");
	}
    printf("0};\n\n");

    printf("static const struct pci_knowndev2 *pci_knowndevs = _pci_knowndevs;\n");
    printf("static const char *pci_knowndevs_text = _pci_knowndevs_text;\n");
    printf("#define PCI_STRING_NULL (-1)\n");
    printf("#define PCI_STRING(x) (&pci_knowndevs_text[(x)])\n\n");



    exit(0);
}
    
    
