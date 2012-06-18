; x86 timer in NASM
;
; Tom St Denis, tomstdenis@iahu.ca
[bits 32]
[section .data]
time dd 0, 0

[section .text]

%ifdef USE_ELF
[global t_start]
t_start:
%else
[global _t_start]
_t_start:
%endif
   push edx
   push eax
   rdtsc
   mov [time+0],edx
   mov [time+4],eax
   pop eax
   pop edx
   ret
   
%ifdef USE_ELF
[global t_read]
t_read:
%else
[global _t_read]
_t_read:
%endif
   rdtsc
   sub eax,[time+4]
   sbb edx,[time+0]
   ret
   