//
//  Debug.c
//
//  Created by Txia Junda on 2011/8/31.
//  Copyright 2011年 ASUS. All rights reserved.
//

#include <stdio.h>    //printf function
#include "ASUS_Discovery_Debug.h"

//#define _ASUS_DEVICE_DISCOVERY_DEBUG

void myAsusDiscoveryDebugPrint(char *pc)
{
    #ifdef _ASUS_DEVICE_DISCOVERY_DEBUG
    printf("%s\n", pc);
    #endif
}

