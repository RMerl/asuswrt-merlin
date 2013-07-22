#!/bin/sh
stty sane
tput clear
tput rmacs
tput rmm
tput rmso
tput rmul
tput rs1
tput rs2
tput rs3
bot=$[ ${LINES:-`tput lines`} - 1 ]
if test "$bot" -le "0"; then bot=24; fi
tput csr 0 $bot
