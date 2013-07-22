/* DOS partition types */
#include "common.h"
#include "nls.h"

struct systypes i386_sys_types[] = {
	{0x00, N_("Empty")},
	{0x01, N_("FAT12")},
	{0x02, N_("XENIX root")},
	{0x03, N_("XENIX usr")},
	{0x04, N_("FAT16 <32M")},
	{0x05, N_("Extended")},		/* DOS 3.3+ extended partition */
	{0x06, N_("FAT16")},		/* DOS 16-bit >=32M */
	{0x07, N_("HPFS/NTFS/exFAT")},	/* OS/2 IFS, eg, HPFS or NTFS or QNX or exFAT */
	{0x08, N_("AIX")},		/* AIX boot (AIX -- PS/2 port) or SplitDrive */
	{0x09, N_("AIX bootable")},	/* AIX data or Coherent */
	{0x0a, N_("OS/2 Boot Manager")},/* OS/2 Boot Manager */
	{0x0b, N_("W95 FAT32")},
	{0x0c, N_("W95 FAT32 (LBA)")},/* LBA really is `Extended Int 13h' */
	{0x0e, N_("W95 FAT16 (LBA)")},
	{0x0f, N_("W95 Ext'd (LBA)")},
	{0x10, N_("OPUS")},
	{0x11, N_("Hidden FAT12")},
	{0x12, N_("Compaq diagnostics")},
	{0x14, N_("Hidden FAT16 <32M")},
	{0x16, N_("Hidden FAT16")},
	{0x17, N_("Hidden HPFS/NTFS")},
	{0x18, N_("AST SmartSleep")},
	{0x1b, N_("Hidden W95 FAT32")},
	{0x1c, N_("Hidden W95 FAT32 (LBA)")},
	{0x1e, N_("Hidden W95 FAT16 (LBA)")},
	{0x24, N_("NEC DOS")},
	{0x27, N_("Hidden NTFS WinRE")},
	{0x39, N_("Plan 9")},
	{0x3c, N_("PartitionMagic recovery")},
	{0x40, N_("Venix 80286")},
	{0x41, N_("PPC PReP Boot")},
	{0x42, N_("SFS")},
	{0x4d, N_("QNX4.x")},
	{0x4e, N_("QNX4.x 2nd part")},
	{0x4f, N_("QNX4.x 3rd part")},
	{0x50, N_("OnTrack DM")},
	{0x51, N_("OnTrack DM6 Aux1")},	/* (or Novell) */
	{0x52, N_("CP/M")},		/* CP/M or Microport SysV/AT */
	{0x53, N_("OnTrack DM6 Aux3")},
	{0x54, N_("OnTrackDM6")},
	{0x55, N_("EZ-Drive")},
	{0x56, N_("Golden Bow")},
	{0x5c, N_("Priam Edisk")},
	{0x61, N_("SpeedStor")},
	{0x63, N_("GNU HURD or SysV")},	/* GNU HURD or Mach or Sys V/386 (such as ISC UNIX) */
	{0x64, N_("Novell Netware 286")},
	{0x65, N_("Novell Netware 386")},
	{0x70, N_("DiskSecure Multi-Boot")},
	{0x75, N_("PC/IX")},
	{0x80, N_("Old Minix")},	/* Minix 1.4a and earlier */
	{0x81, N_("Minix / old Linux")},/* Minix 1.4b and later */
	{0x82, N_("Linux swap / Solaris")},
	{0x83, N_("Linux")},
	{0x84, N_("OS/2 hidden C: drive")},
	{0x85, N_("Linux extended")},
	{0x86, N_("NTFS volume set")},
	{0x87, N_("NTFS volume set")},
	{0x88, N_("Linux plaintext")},
	{0x8e, N_("Linux LVM")},
	{0x93, N_("Amoeba")},
	{0x94, N_("Amoeba BBT")},	/* (bad block table) */
	{0x9f, N_("BSD/OS")},		/* BSDI */
	{0xa0, N_("IBM Thinkpad hibernation")},
	{0xa5, N_("FreeBSD")},		/* various BSD flavours */
	{0xa6, N_("OpenBSD")},
	{0xa7, N_("NeXTSTEP")},
	{0xa8, N_("Darwin UFS")},
	{0xa9, N_("NetBSD")},
	{0xab, N_("Darwin boot")},
	{0xaf, N_("HFS / HFS+")},
	{0xb7, N_("BSDI fs")},
	{0xb8, N_("BSDI swap")},
	{0xbb, N_("Boot Wizard hidden")},
	{0xbe, N_("Solaris boot")},
	{0xbf, N_("Solaris")},
	{0xc1, N_("DRDOS/sec (FAT-12)")},
	{0xc4, N_("DRDOS/sec (FAT-16 < 32M)")},
	{0xc6, N_("DRDOS/sec (FAT-16)")},
	{0xc7, N_("Syrinx")},
	{0xda, N_("Non-FS data")},
	{0xdb, N_("CP/M / CTOS / ...")},/* CP/M or Concurrent CP/M or
					   Concurrent DOS or CTOS */
	{0xde, N_("Dell Utility")},	/* Dell PowerEdge Server utilities */
	{0xdf, N_("BootIt")},		/* BootIt EMBRM */
	{0xe1, N_("DOS access")},	/* DOS access or SpeedStor 12-bit FAT
					   extended partition */
	{0xe3, N_("DOS R/O")},		/* DOS R/O or SpeedStor */
	{0xe4, N_("SpeedStor")},	/* SpeedStor 16-bit FAT extended
					   partition < 1024 cyl. */
	{0xeb, N_("BeOS fs")},
	{0xee, N_("GPT")},		/* Intel EFI GUID Partition Table */
	{0xef, N_("EFI (FAT-12/16/32)")},/* Intel EFI System Partition */
	{0xf0, N_("Linux/PA-RISC boot")},/* Linux/PA-RISC boot loader */
	{0xf1, N_("SpeedStor")},
	{0xf4, N_("SpeedStor")},	/* SpeedStor large partition */
	{0xf2, N_("DOS secondary")},	/* DOS 3.3+ secondary */
	{0xfb, N_("VMware VMFS")},
	{0xfc, N_("VMware VMKCORE")},	/* VMware kernel dump partition */
	{0xfd, N_("Linux raid autodetect")},/* New (2.2.x) raid partition with
					       autodetect using persistent
					       superblock */
	{0xfe, N_("LANstep")},		/* SpeedStor >1024 cyl. or LANstep */
	{0xff, N_("BBT")},		/* Xenix Bad Block Table */
	{ 0, 0 }
};
