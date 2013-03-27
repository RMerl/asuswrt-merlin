.include "t-macros.i"

	start


	
	;; Check that the instruction @REP_E is executed when it
	;; is reached using a branch instruction
	
	ldi r2, 1
test_rep_1:
	rep	r2, end_rep_1
	nop || nop
	nop || nop
	nop || nop
	nop || nop
	ldi	r3, 46
	bra	end_rep_1
	ldi	r3, 42
end_rep_1:
	addi	r3, 1

	check 1 r3 47


	;; Check that the loop is executed the correct number of times

	ldi	r2, 10
	ldi	r3, 0
	ldi	r4, 0
test_rep_2:
	rep	r2, end_rep_2
	nop || nop
	nop || nop
	nop || nop
	nop || nop
	nop || nop
	addi	r3, 1
end_rep_2:
	addi	r4, 1

	check 2 r3 10
	check 3 r4 10

	exit0
