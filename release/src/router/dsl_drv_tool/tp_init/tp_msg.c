#ifdef WIN32
#include <conio.h>
#endif
#include <stdio.h>
#include <string.h>
#include "tp_msg.h"

void ClearInput()
{
    char c;
    while (1)
    {
        c=getchar();
        if (c==0x0a) break;
    }
}


void ShowMsgAndInput(char* input, char* input_str)
{
    printf("0 : init\r\n");
    printf("1 : SYSINFO\r\n");
    printf("2 : SYSSTATUS\r\n");
    printf("3 : SYSTRAFFIC\r\n");
    printf("4 : SHOW WAN\r\n");
    printf("5 : SHOW ADSL\r\n");    
    printf("6 : SHOW LAN\r\n");        
    printf("7 : PVC DISP\r\n");    
    printf("8 : ADSL STATUS\r\n");           
    printf("a : add PVC (VPI/VCI)\r\n");    
    printf("v : add a default PVC (2 1234 0 35 0 0)\r\n");        
    //printf("s : send to WAN port with default VLAN ID 1234\r\n");    
    printf("i : add PVCs (1 : 35, 2 : 36, 3 : 37)\r\n");        
    printf("s : show PVCs (1 : 35, 2 : 36, 3 : 37)\r\n");            
    printf("c : enter user command\r\n");
    printf("q : quit\r\n");        
    printf("\r\nYour command : ");                   
    
    *input = getchar();
    ClearInput();    
    printf("Input is %c\r\n", *input);                       
    if (*input == 'a')
    {
    /*
        printf("VPI : ");                       
        scanf("%d", input_vpi);
        printf("VCI : ");                       
        scanf("%d", input_vci);        
    */        
        printf("Idx VID VPI VCI Encap Mode (3 1235 0 34 0 0) : ");
        gets(input_str);
        printf("PVC is %s\r\n", input_str);            
    }
    else if (*input == 'c')
    {
        printf("User command : ");                       
        gets(input_str);
        printf("Cmd is %s\r\n", input_str);                               
    }    
    else if (*input == 'v')
    {
        strcpy(input_str, "2 1234 0 35 0 0");
    }
    
}
