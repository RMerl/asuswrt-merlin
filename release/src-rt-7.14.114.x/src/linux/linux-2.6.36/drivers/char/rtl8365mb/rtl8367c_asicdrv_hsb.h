#ifndef _RTL8367C_ASICDRV__HSB_H_
#define _RTL8367C_ASICDRV__HSB_H_

#include <rtl8367c_asicdrv.h>

#define RTL8367C_FIELDSEL_FORMAT_NUMBER      (16)
#define RTL8367C_FIELDSEL_MAX_OFFSET         (255)

enum FIELDSEL_FORMAT_FORMAT
{
    FIELDSEL_FORMAT_DEFAULT = 0,
    FIELDSEL_FORMAT_RAW,
	FIELDSEL_FORMAT_LLC,
	FIELDSEL_FORMAT_IPV4,
	FIELDSEL_FORMAT_ARP,
	FIELDSEL_FORMAT_IPV6,
	FIELDSEL_FORMAT_IPPAYLOAD,
	FIELDSEL_FORMAT_L4PAYLOAD,
    FIELDSEL_FORMAT_END
};

extern ret_t rtl8367c_setAsicFieldSelector(rtk_uint32 index, rtk_uint32 format, rtk_uint32 offset);
extern ret_t rtl8367c_getAsicFieldSelector(rtk_uint32 index, rtk_uint32* pFormat, rtk_uint32* pOffset);

#endif /*_RTL8367C_ASICDRV__HSB_H_*/

