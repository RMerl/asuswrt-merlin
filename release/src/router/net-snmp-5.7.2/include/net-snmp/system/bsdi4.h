#include "bsdi.h"
#define bsdi2 bsdi2             /* bsdi4 is a superset of bsdi2 */
#undef NPROC_SYMBOL
#undef PROC_SYMBOL

#define MNTTYPE_UFS	"ufs"
#define BerkelyFS	1
#define MNTTYPE_MSDOS	"msdos"
#define MNTTYPE_ISO9660 "cd9660"
#define MNTTYPE_NFS 	"nfs"
#define MNTTYPE_MFS 	"mfs"
