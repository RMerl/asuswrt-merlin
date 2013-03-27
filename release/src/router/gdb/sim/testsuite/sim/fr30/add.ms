# fr30 testcase for add $Rj,$Ri
# cpu {}

	.include "testutils.inc"

	START

	.text
	.global add
add:
	add ac,ac
	fail
	EXIT 0
