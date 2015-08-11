#ifndef __RESAMPLESUBS_H__
#define __RESAMPLESUBS_H__

typedef char           RES_BOOL;
typedef short          RES_HWORD;
typedef int            RES_WORD;
typedef unsigned short RES_UHWORD;
typedef unsigned int   RES_UWORD;

#ifdef _USRDLL
#   define DECL(T)  __declspec(dllexport) T
#else
#   define DECL(T)  T
#endif

#ifdef __cplusplus
extern "C"
{
#endif

DECL(int) res_SrcLinear(const RES_HWORD X[], RES_HWORD Y[], 
		        double pFactor, RES_UHWORD nx);
DECL(int) res_Resample(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		       RES_UHWORD nx, RES_BOOL LargeF, RES_BOOL Interp);
DECL(int) res_GetXOFF(double pFactor, RES_BOOL LargeF);

#ifdef __cplusplus
}
#endif

#endif

