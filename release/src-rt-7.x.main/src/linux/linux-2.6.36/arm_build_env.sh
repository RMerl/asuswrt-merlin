#!/bin/sh 

# TOOL_BASE=/projects/hnd/tools/linux/linaro-arm-eabi-4.6.2/
TOOL_BASE=/projects/hnd/tools/linux/hndtools-armeabi-2011.09/

export PATH=${PATH}:${TOOL_BASE}/bin

export ARCH=arm

# PREFIX=arm-eabi-
export PREFIX=arm-none-eabi-
export NM_TOOL=${PREFIX}nm

alias nm=${NM_TOOL}
alias objdump=${PREFIX}objdump
alias trx='/projects/hnd/tools/linux/bin/trx'

${PREFIX}gcc --version
