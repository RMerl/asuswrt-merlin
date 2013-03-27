# mach: crisv0 crisv3 crisv8 crisv10 crisv32
# output: 1c\n

 .include "testutils.inc"
 start
 moveq 0,r3

 clearf nzvc
 setf nzv
 bcc 0f
 addq 1,r3
 fail

0:
 clearf nzvc
 setf nzv
 bcs dofail
 addq 1,r3

 clearf nzvc
 setf ncv
 bne 1f
 addq 1,r3

dofail:
 fail

1:
 clearf nzvc
 setf ncv
 beq dofail
 addq 1,r3

 clearf nzvc
 setf ncz
 bvc 2f
 addq 1,r3
 fail

2:
 clearf nzvc
 setf ncz
 bvs dofail
 addq 1,r3

 clearf nzvc
 setf vcz
 bpl 3f
 addq 1,r3
 fail

3:
 clearf nzvc
 setf vcz
 bmi dofail
 addq 1,r3

 clearf nzvc
 setf nv
 bls dofail
 addq 1,r3

 clearf nzvc
 setf nv
 bhi 4f
 addq 1,r3
 fail

4:
 clearf nzvc
 setf zc
 bge 5f
 addq 1,r3
 fail

5:
 clearf nzvc
 setf zc
 blt dofail
 addq 1,r3

 clearf nzvc
 setf c
 bgt 6f
 addq 1,r3
 fail

6:
 clearf nzvc
 setf c
 ble dofail
 addq 1,r3

;;;;;;;;;;

 setf nzvc
 clearf nzv
 bcc dofail
 addq 1,r3

 setf nzvc
 clearf nzv
 bcs 0f
 addq 1,r3
 fail

0:
 setf nzvc
 clearf ncv
 bne dofail
 addq 1,r3

 setf nzvc
 clearf ncv
 beq 1f
 addq 1,r3
 fail

1:
 setf nzvc
 clearf ncz
 bvc dofail
 addq 1,r3

 setf nzvc
 clearf ncz
 bvs 2f
 addq 1,r3
 fail

2:
 setf nzvc
 clearf vcz
 bpl dofail
 addq 1,r3

 setf nzvc
 clearf vcz
 bmi 3f
 addq 1,r3
 fail

3:
 setf nzvc
 clearf nv
 bls 4f
 addq 1,r3
 fail

4:
 setf nzvc
 clearf nv
 bhi dofail
 addq 1,r3

 setf zvc
 clearf nzc
 bge dofail
 addq 1,r3

 setf nzc
 clearf vzc
 blt 5f
 addq 1,r3
 fail

5:
 setf nzvc
 clearf c
 bgt dofail
 addq 1,r3

 setf nzvc
 clearf c
 ble 6f
 addq 1,r3
 fail

6:
 dumpr3
 quit
