#include <stdio.h>
#include "sis.h"

void
usage()
{

    printf("usage: sis [-uart1 uart_device1] [-uart2 uart_device2]\n");
    printf("[-nfp] [-freq frequency] [-c batch_file] [files]\n");
    printf("[-sparclite] [-dumbio]\n");
}

void
gen_help()
{

  printf("\n batch <file>          execute a batch file of SIS commands\n");
    printf(" +bp <addr>            add a breakpoint at <addr>\n");
    printf(" -bp <num>             delete breakpoint <num>\n");
    printf(" bp                    print all breakpoints\n");
    printf(" cont [icnt]           continue execution for [icnt] instructions\n");
    printf(" deb <level>           set debug level\n");
    printf(" dis [addr] [count]    disassemble [count] instructions at address [addr]\n");
    printf(" echo <string>         print <string> to the simulator window\n");
#ifdef ERRINJ
    printf(" error <period>        inject error traps in IU and FPU\n");
#endif
    printf(" float                 print the FPU registers\n");
    printf(" go <addr> [icnt]      start execution at <addr> for [icnt] instructions\n");
    printf(" hist [trace_length]   enable/show trace history\n");
    printf(" load  <file_name>     load a file into simulator memory\n");
    printf(" mem [addr] [count]    display memory at [addr] for [count] bytes\n");
    printf(" quit                  exit the simulator\n");
    printf(" perf [reset]          show/reset performance statistics\n");
    printf(" reg [w<0-7>]          show integer registers (or windows, eg 're w2')\n");
    printf(" run [inst_count]      reset and start execution for [icnt] instruction\n");
    printf(" step                  single step\n");
    printf(" tra [inst_count]      trace [inst_count] instructions\n");
    printf("\n type Ctrl-C to interrupt execution\n\n");
}
