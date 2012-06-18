#include <stdio.h>
#include <string.h>

#define IH_NMLEN		32	/* Image Name Length		*/

typedef struct {
	unsigned int ih_magic;	/* Image Header Magic Number	*/
	unsigned int ih_hcrc;	/* Image Header CRC Checksum	*/
	unsigned int ih_time;	/* Image Creation Timestamp	*/
	unsigned int ih_size;	/* Image Data Size		*/
	unsigned int ih_load;	/* Data	 Load  Address		*/
	unsigned int ih_ep;		/* Entry Point Address		*/
	unsigned int ih_dcrc;	/* Image Data CRC Checksum	*/
	unsigned char ih_os;		/* Operating System		*/
	unsigned char ih_arch;	/* CPU architecture		*/
	unsigned char ih_type;	/* Image Type			*/
	unsigned char ih_comp;	/* Compression Type		*/
	unsigned char ih_name[IH_NMLEN];	/* Image Name		*/
} IMAGE_HEADER;

#define SWAP_LONG(x) \
	((unsigned int)( \
		(((unsigned int)(x) & (unsigned int)0x000000ffUL) << 24) | \
		(((unsigned int)(x) & (unsigned int)0x0000ff00UL) <<  8) | \
		(((unsigned int)(x) & (unsigned int)0x00ff0000UL) >>  8) | \
		(((unsigned int)(x) & (unsigned int)0xff000000UL) >> 24) ))
		
		

//char g_Buf4M[1024*1024*4];
char g_Buf8M[1024*1024*8];

// additional 1M for modem firmware
char g_Buf8M_trx[1024*1024*9];


char g_BootVer[4];		

char g_8Mtrx[256];
char g_8Mbtrx[256];

void InitBin()
{
	// personalize boot version to bin file
	g_Buf8M[0x4018a] = g_BootVer[0];
	g_Buf8M[0x4018b] = g_BootVer[1];
	g_Buf8M[0x4018c] = g_BootVer[2];
	g_Buf8M[0x4018d] = g_BootVer[3];
	// 5g
	g_Buf8M[0x40036] = 0x0a;
	// 2.4g
	g_Buf8M[0x48036] = 0x26;
}
	
void Gen8mBin()
{
	FILE* FpInput;
	FILE* FpOutput;
	memset(g_Buf8M,0xff,sizeof(g_Buf8M));
	FpInput = fopen(g_8Mtrx,"rb");
	FpOutput = fopen("Output8M.bin","wb");
	int ReadBytes = fread(&g_Buf8M[0x50000],1,sizeof(g_Buf8M)-0x50000,FpInput);
	fclose (FpInput);
	// 5g
	FpInput = fopen("base/RT3662.bin","rb");
	int ReadBytes2 = fread(&g_Buf8M[0x40000],1,0x48000-0x40000,FpInput);
	fclose (FpInput);
	// 2.4g
	FpInput = fopen("base/RT3092.bin","rb");
	int ReadBytes3 = fread(&g_Buf8M[0x48000],1,0x50000-0x48000,FpInput);
	fclose (FpInput);		
	//
	//printf("read ok\n");
	FpInput = fopen("base/uboot_n55u.img","rb");
	fread(g_Buf8M,1,192*1024,FpInput);
	fclose (FpInput);
	// set init value
	InitBin();
	fwrite(g_Buf8M,1,sizeof(g_Buf8M),FpOutput);
	fclose (FpOutput);
	
	FpOutput = fopen("factory.bin","wb");
	fwrite(&g_Buf8M[0x40000],1,0x10000,FpOutput);
	fclose (FpOutput);
	
	printf("\n8M uboot, factory and trx Done\n");
}


void Gen8mBin_b()
{
	FILE* FpInput;
	FILE* FpOutput;
	memset(g_Buf8M,0xff,sizeof(g_Buf8M));
	FpInput = fopen(g_8Mbtrx,"rb");
	FpOutput = fopen("Output8M_b.bin","wb");
	int ReadBytes = fread(&g_Buf8M[0x50000],1,sizeof(g_Buf8M)-0x50000,FpInput);
	fclose (FpInput);
	// 5g
	FpInput = fopen("base/RT3662.bin","rb");
	int ReadBytes2 = fread(&g_Buf8M[0x40000],1,0x48000-0x40000,FpInput);
	fclose (FpInput);
	// 2.4g
	FpInput = fopen("base/RT3092.bin","rb");
	int ReadBytes3 = fread(&g_Buf8M[0x48000],1,0x50000-0x48000,FpInput);
	fclose (FpInput);	
	//
	//printf("read ok\n");
	FpInput = fopen("base/uboot_n55u_b.img","rb");
	fread(g_Buf8M,1,192*1024,FpInput);
	fclose (FpInput);
	// set init value
	InitBin();
	fwrite(g_Buf8M,1,sizeof(g_Buf8M),FpOutput);
	fclose (FpOutput);
	printf("\n8M_b uboot, factory and trx Done\n");
}

		
void GenTrx()
{
	IMAGE_HEADER* TrxHdr;
	FILE* FpHdr;
	char TrxHdrBuf[512];
	unsigned int OrigSize4m;
	unsigned int OrigSize8m;

// 64 is the trx header
	FpHdr = fopen(g_8Mtrx,"rb");
	if (FpHdr == NULL)
	{
		printf("\n8M.trx not found. letter case wrong?\n");
		return;
	}
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpHdr);	  	
	fclose(FpHdr); 
	TrxHdr = (IMAGE_HEADER*)TrxHdrBuf;
	OrigSize8m = SWAP_LONG(TrxHdr->ih_size) + 64;
// 64 is the trx header
	FILE* FpInput;
	FILE* FpOutput;


///////////////////// generate 8M


	memset(g_Buf8M_trx,0xff,sizeof(g_Buf8M_trx));

	FpInput = fopen(g_8Mtrx,"rb");
	if (FpInput == NULL) return;
	int ReadBytes1 = fread(&g_Buf8M_trx[0],1,sizeof(g_Buf8M_trx),FpInput);
	fclose(FpInput);

	FpInput = fopen("../tc_asus_bin/asus_tc.bin","rb");
	if (FpInput == NULL) return;
	int tc_bin_size1 = fread(&g_Buf8M_trx[OrigSize8m],1,sizeof(g_Buf8M_trx)-OrigSize8m,FpInput);
	fclose(FpInput);
	FpOutput = fopen("Output8M.trx","wb");
	fwrite(g_Buf8M_trx,1,OrigSize8m+tc_bin_size1,FpOutput);
	fclose (FpOutput);
	
	printf("\nOutput8M.trx Done\n");

}



void GenTrx_b()
{
	IMAGE_HEADER* TrxHdr;
	FILE* FpHdr;
	char TrxHdrBuf[512];
	unsigned int OrigSize4m;
	unsigned int OrigSize8m;

// 64 is the trx header
	FpHdr = fopen(g_8Mbtrx,"rb");
	if (FpHdr == NULL)
	{
		printf("\n8M_b.trx not found. letter case wrong?\n");
		return;
	}
	fread(TrxHdrBuf,1,sizeof(TrxHdrBuf),FpHdr);	  	
	fclose(FpHdr); 
	TrxHdr = (IMAGE_HEADER*)TrxHdrBuf;
	OrigSize8m = SWAP_LONG(TrxHdr->ih_size) + 64;
// 64 is the trx header
	FILE* FpInput;
	FILE* FpOutput;


///////////////////// generate 8M


	memset(g_Buf8M_trx,0xff,sizeof(g_Buf8M_trx));

	FpInput = fopen(g_8Mbtrx,"rb");
	if (FpInput == NULL) return;
	int ReadBytes1 = fread(&g_Buf8M_trx[0],1,sizeof(g_Buf8M_trx),FpInput);
	fclose(FpInput);

	FpInput = fopen("../tc_asus_bin/asus_tc_b.bin","rb");
	if (FpInput == NULL) return;
	int tc_bin_size1 = fread(&g_Buf8M_trx[OrigSize8m],1,sizeof(g_Buf8M_trx)-OrigSize8m,FpInput);
	fclose(FpInput);

	FpOutput = fopen("Output8M_b.trx","wb");
	fwrite(g_Buf8M_trx,1,OrigSize8m+tc_bin_size1,FpOutput);
	fclose (FpOutput);
	
	printf("\nOutput8M_b.trx Done\n");

}

void LoadBootLoaderVer()
{
	FILE* fp;
	char buf[32];
    fp = fopen("bootver.txt","rb");
    if (fp == NULL) return 0;
	g_BootVer[0] = 0x20;
	g_BootVer[1] = 0x20;	
        g_BootVer[2] = 0x20;
        g_BootVer[3] = 0x20;
	fread(g_BootVer,1,sizeof(g_BootVer),fp);    
    fclose(fp);	
}

void main(int argc, char* argv[])
{
	int i;
	char* pByte;
	int annex_a = 0;
	int annex_b = 0;
	char OldNameTmp[256];
	char OldNameBin[256];
	char OldNameTrx[256];
	if (argc != 2) return;
	printf(argv[1]);
	printf("\n");
	strcpy(g_8Mtrx,argv[1]);
	strcpy(g_8Mbtrx,argv[1]);
	strcpy(OldNameTmp, argv[1]);	
	pByte = OldNameTmp;
	while (1)
	{
		if (*pByte == '.' && (*(pByte+1)) == 't' && (*(pByte+2)) == 'r' && (*(pByte+3)) == 'x')
		{
			*pByte = 0;
			break;
		}
		else if (*pByte == 0)
		{
			break;
		}
		pByte++;
	}
	printf(OldNameTmp);
	printf("\n");
	if (!strncmp(OldNameTmp,"DSL-N55U-B",10))
	{
		annex_b = 1;
	}
	else
	{
		if (!strncmp(OldNameTmp,"DSL-N55U",8))
		{
			annex_a = 1;
		}	
	}
	strcpy(OldNameBin, OldNameTmp);
	strcat(OldNameBin, ".bin");
	strcpy(OldNameTrx, OldNameTmp);
	if (annex_a)
	{
		LoadBootLoaderVer();
		Gen8mBin();
		printf("Annex A , Bin OK\n");
		GenTrx();
		printf("Annex A , Trx OK\n");
		rename("Output8M.bin",OldNameBin);
		strcat(OldNameTrx, "_Annex_A.trx");		
		rename("Output8M.trx",OldNameTrx);
	}	
	if (annex_b)
	{
		LoadBootLoaderVer();
		Gen8mBin_b();
		printf("Annex B , Bin OK\n");
		GenTrx_b();
		printf("Annex B , Trx OK\n");	
		rename("Output8M_b.bin",OldNameBin);
		strcat(OldNameTrx, "_Annex_B.trx");		
		rename("Output8M_b.trx",OldNameTrx);
	}	
}

