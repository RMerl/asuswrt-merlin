C OFFSET(i)
C Expands to 4*i, or to the empty string if i is zero
define(<OFFSET>, <ifelse($1,0,,eval(4*$1))>)

dnl LREG(reg) gives the 8-bit register corresponding to the given 32-bit register.
define(<LREG>,<ifelse(
	$1, %eax, %al,
	$1, %ebx, %bl,
	$1, %ecx, %cl,
	$1, %edx, %dl)>)dnl

define(<HREG>,<ifelse(
	$1, %eax, %ah,
	$1, %ebx, %bh,
	$1, %ecx, %ch,
	$1, %edx, %dh)>)dnl
