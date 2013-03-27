# sh testcase for floating point register shared state (see below).
# mach: all
# as: -isa=shmedia
# ld: -m shelf64

# (fr, dr, fp, fv amd mtrx provide different views of the same architecrual state).
# Hitachi SH-5 CPU volume 1, p. 15.

	.include "media/testutils.inc"

	start

	movi 42, r0
	fmov.ls r0, fr12
	# save this reg.
	fmov.s fr12, fr14

	movi 42, r0
	fmov.qd r0, dr12
	
okay:
	pass
