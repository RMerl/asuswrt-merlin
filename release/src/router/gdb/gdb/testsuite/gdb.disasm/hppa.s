	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.EXPORT integer_memory_tests,CODE
	.EXPORT integer_indexing_load,CODE
	.EXPORT integer_load_short_memory,CODE
	.EXPORT integer_store_short_memory,CODE
	.EXPORT immediate_tests,CODE
	.EXPORT branch_tests_1,CODE
	.EXPORT branch_tests_2,CODE
	.EXPORT movb_tests,CODE
	.EXPORT movb_nullified_tests,CODE
	.EXPORT movib_tests,CODE
	.EXPORT movib_nullified_tests,CODE
	.EXPORT comb_tests_1,CODE
	.EXPORT comb_tests_2,CODE
	.EXPORT comb_nullified_tests_1,CODE
	.EXPORT comb_nullified_tests_2,CODE
	.EXPORT comib_tests_1,CODE
	.EXPORT comib_tests_2,CODE
	.EXPORT comib_nullified_tests_1,CODE
	.EXPORT comib_nullified_tests_2,CODE
	.EXPORT addb_tests_1,CODE
	.EXPORT addb_tests_2,CODE
	.EXPORT addb_nullified_tests_1,CODE
	.EXPORT addb_nullified_tests_2,CODE
	.EXPORT addib_tests_1,CODE
	.EXPORT addib_tests_2,CODE
	.EXPORT addib_nullified_tests_1,CODE
	.EXPORT addib_nullified_tests_2,CODE
	.EXPORT bb_tests,CODE
	.EXPORT add_tests,CODE
	.EXPORT addl_tests,CODE
	.EXPORT addo_tests,CODE
	.EXPORT addc_tests,CODE
	.EXPORT addco_tests,CODE
	.EXPORT sh1add_tests,CODE
	.EXPORT sh1addl_tests,CODE
	.EXPORT sh1addo_tests,CODE
	.EXPORT sh2add_tests,CODE
	.EXPORT sh2addl_tests,CODE
	.EXPORT sh2addo_tests,CODE
	.EXPORT sh3add_tests,CODE
	.EXPORT sh3addl_tests,CODE
	.EXPORT sh3addo_tests,CODE
	.EXPORT sub_tests,CODE
	.EXPORT subo_tests,CODE
	.EXPORT subb_tests,CODE
	.EXPORT subbo_tests,CODE
	.EXPORT subt_tests,CODE
	.EXPORT subto_tests,CODE
	.EXPORT ds_tests,CODE
	.EXPORT comclr_tests,CODE
	.EXPORT or_tests,CODE
	.EXPORT xor_tests,CODE
	.EXPORT and_tests,CODE
	.EXPORT andcm_tests,CODE
	.EXPORT uxor_tests,CODE
	.EXPORT uaddcm_tests,CODE
	.EXPORT uaddcmt_tests,CODE
	.EXPORT dcor_tests,CODE
	.EXPORT idcor_tests,CODE
	.EXPORT addi_tests,CODE
	.EXPORT addio_tests,CODE
	.EXPORT addit_tests,CODE
	.EXPORT addito_tests,CODE
	.EXPORT subi_tests,CODE
	.EXPORT subio_tests,CODE
	.EXPORT comiclr_tests,CODE
	.EXPORT vshd_tests,CODE
	.EXPORT shd_tests,CODE
	.EXPORT extru_tests,CODE
	.EXPORT extrs_tests,CODE
	.EXPORT zdep_tests,CODE
	.EXPORT dep_tests,CODE
	.EXPORT vextru_tests,CODE
	.EXPORT vextrs_tests,CODE
	.EXPORT zvdep_tests,CODE
	.EXPORT vdep_tests,CODE
	.EXPORT vdepi_tests,CODE
	.EXPORT zvdepi_tests,CODE
	.EXPORT depi_tests,CODE
	.EXPORT zdepi_tests,CODE
	.EXPORT system_control_tests,CODE
	.EXPORT probe_tests,CODE
	.EXPORT lpa_tests,CODE
	.EXPORT purge_tests,CODE
	.EXPORT insert_tests,CODE
	.EXPORT fpu_misc_tests,CODE
	.EXPORT fpu_memory_indexing_tests,CODE
	.EXPORT fpu_short_memory_tests,CODE
	.EXPORT fcpy_tests,CODE
	.EXPORT fabs_tests,CODE
	.EXPORT fsqrt_tests,CODE
	.EXPORT frnd_tests,CODE
	.EXPORT fcnvff_tests,CODE
	.EXPORT fcnvxf_tests,CODE
	.EXPORT fcnvfx_tests,CODE
	.EXPORT fcnvfxt_tests,CODE
	.EXPORT fadd_tests,CODE
	.EXPORT fsub_tests,CODE
	.EXPORT fmpy_tests,CODE
	.EXPORT fdiv_tests,CODE
	.EXPORT frem_tests,CODE
	.EXPORT fcmp_sgl_tests_1,CODE
	.EXPORT fcmp_sgl_tests_2,CODE
	.EXPORT fcmp_sgl_tests_3,CODE
	.EXPORT fcmp_sgl_tests_4,CODE
	.EXPORT fcmp_dbl_tests_1,CODE
	.EXPORT fcmp_dbl_tests_2,CODE
	.EXPORT fcmp_dbl_tests_3,CODE
	.EXPORT fcmp_dbl_tests_4,CODE
	.EXPORT fcmp_quad_tests_1,CODE
	.EXPORT fcmp_quad_tests_2,CODE
	.EXPORT fcmp_quad_tests_3,CODE
	.EXPORT fcmp_quad_tests_4,CODE
	.EXPORT fmpy_addsub_tests,CODE
	.EXPORT xmpyu_tests,CODE
	.EXPORT special_tests,CODE
	.EXPORT sfu_tests,CODE
	.EXPORT copr_tests,CODE
	.EXPORT copr_indexing_load,CODE
	.EXPORT copr_indexing_store,CODE
	.EXPORT copr_short_memory,CODE
	.EXPORT fmemLRbug_tests_1,CODE
	.EXPORT fmemLRbug_tests_2,CODE
	.EXPORT fmemLRbug_tests_3,CODE
	.EXPORT fmemLRbug_tests_4,CODE
	.EXPORT main,CODE
	.EXPORT main,ENTRY,PRIV_LEV=3,RTNVAL=GR
main
	.PROC
	.CALLINFO FRAME=64,NO_CALLS,SAVE_SP
	.ENTRY
	copy %r4,%r1
	copy %r30,%r4
	stwm %r1,64(0,%r30)
; First memory reference instructions.
; Should try corner cases for each field extraction.
; Should deal with s == 0 case somehow?!?
integer_memory_tests
	ldw 0(0,%r4),%r26
	ldh 0(0,%r4),%r26
	ldb 0(0,%r4),%r26
	stw %r26,0(0,%r4)
	sth %r26,0(0,%r4)
	stb %r26,0(0,%r4)

; Should make sure pre/post modes are recognized correctly.
	ldwm 0(0,%r4),%r26
	stwm %r26,0(0,%r4)

integer_indexing_load
	ldwx %r5(0,%r4),%r26
	ldwx,s %r5(0,%r4),%r26
	ldwx,m %r5(0,%r4),%r26
	ldwx,sm %r5(0,%r4),%r26
	ldhx %r5(0,%r4),%r26
	ldhx,s %r5(0,%r4),%r26
	ldhx,m %r5(0,%r4),%r26
	ldhx,sm %r5(0,%r4),%r26
	ldbx %r5(0,%r4),%r26
	ldbx,s %r5(0,%r4),%r26
	ldbx,m %r5(0,%r4),%r26
	ldbx,sm %r5(0,%r4),%r26
	ldwax %r5(%r4),%r26
	ldwax,s %r5(%r4),%r26
	ldwax,m %r5(%r4),%r26
	ldwax,sm %r5(%r4),%r26
	ldcwx %r5(0,%r4),%r26
	ldcwx,s %r5(0,%r4),%r26
	ldcwx,m %r5(0,%r4),%r26
	ldcwx,sm %r5(0,%r4),%r26

integer_load_short_memory
	ldws 0(0,%r4),%r26
	ldws,mb 0(0,%r4),%r26
	ldws,ma 0(0,%r4),%r26
	ldhs 0(0,%r4),%r26
	ldhs,mb 0(0,%r4),%r26
	ldhs,ma 0(0,%r4),%r26
	ldbs 0(0,%r4),%r26
	ldbs,mb 0(0,%r4),%r26
	ldbs,ma 0(0,%r4),%r26
	ldwas 0(%r4),%r26
	ldwas,mb 0(%r4),%r26
	ldwas,ma 0(%r4),%r26
	ldcws 0(0,%r4),%r26
	ldcws,mb 0(0,%r4),%r26
	ldcws,ma 0(0,%r4),%r26

integer_store_short_memory
	stws %r26,0(0,%r4)
	stws,mb %r26,0(0,%r4)
	stws,ma %r26,0(0,%r4)
	sths %r26,0(0,%r4)
	sths,mb %r26,0(0,%r4)
	sths,ma %r26,0(0,%r4)
	stbs %r26,0(0,%r4)
	stbs,mb %r26,0(0,%r4)
	stbs,ma %r26,0(0,%r4)
	stwas %r26,0(%r4)
	stwas,mb %r26,0(%r4)
	stwas,ma %r26,0(%r4)
	stbys %r26,0(0,%r4)
	stbys,b %r26,0(0,%r4)
	stbys,e %r26,0(0,%r4)
	stbys,b,m %r26,0(0,%r4)
	stbys,e,m %r26,0(0,%r4)

; Immediate instructions.
immediate_tests
	ldo 5(%r26),%r26
	ldil L%0xdeadbeef,%r26
	addil L%0xdeadbeef,%r5

; Lots of branch instructions.
; blr with %r0 as return pointer should really be just br <target>,
; but the assemblers can't handle it.
branch_tests_1
	bl main,%r2
	bl,n main,%r2
	b main
	b,n main
	gate main,%r2
	gate,n main,%r2
	blr %r4,%r2
	blr,n %r4,%r2
	blr %r4,%r0
	blr,n %r4,%r0
branch_tests_2
	bv 0(%r2)
	bv,n 0(%r2)
	be 0x1234(%sr1,%r2)
	be,n 0x1234(%sr1,%r2)
	ble 0x1234(%sr1,%r2)
	ble,n 0x1234(%sr1,%r2)

; GAS can't assemble movb,n or movib,n.
movb_tests
	movb %r4,%r26,movb_tests
	movb,= %r4,%r26,movb_tests
	movb,< %r4,%r26,movb_tests
	movb,od %r4,%r26,movb_tests
	movb,tr %r4,%r26,movb_tests
	movb,<> %r4,%r26,movb_tests
	movb,>= %r4,%r26,movb_tests
	movb,ev %r4,%r26,movb_tests
movb_nullified_tests
	movb,n %r4,%r26,movb_tests
	movb,=,n %r4,%r26,movb_tests
	movb,<,n %r4,%r26,movb_tests
	movb,od,n %r4,%r26,movb_tests
	movb,tr,n %r4,%r26,movb_tests
	movb,<>,n %r4,%r26,movb_tests
	movb,>=,n %r4,%r26,movb_tests
	movb,ev,n %r4,%r26,movb_tests

movib_tests
	movib 5,%r26,movib_tests
	movib,= 5,%r26,movib_tests
	movib,< 5,%r26,movib_tests
	movib,od 5,%r26,movib_tests
	movib,tr 5,%r26,movib_tests
	movib,<> 5,%r26,movib_tests
	movib,>= 5,%r26,movib_tests
	movib,ev 5,%r26,movib_tests
movib_nullified_tests
	movib,n 5,%r26,movib_tests
	movib,=,n 5,%r26,movib_tests
	movib,<,n 5,%r26,movib_tests
	movib,od,n 5,%r26,movib_tests
	movib,tr,n 5,%r26,movib_tests
	movib,<>,n 5,%r26,movib_tests
	movib,>=,n 5,%r26,movib_tests
	movib,ev,n 5,%r26,movib_tests

comb_tests_1
	comb %r0,%r4,comb_tests_1
	comb,= %r0,%r4,comb_tests_1
	comb,< %r0,%r4,comb_tests_1
	comb,<= %r0,%r4,comb_tests_1
	comb,<< %r0,%r4,comb_tests_1
	comb,<<= %r0,%r4,comb_tests_1
	comb,sv %r0,%r4,comb_tests_1
	comb,od %r0,%r4,comb_tests_1

comb_tests_2
	comb,tr %r0,%r4,comb_tests_2
	comb,<> %r0,%r4,comb_tests_2
	comb,>= %r0,%r4,comb_tests_2
	comb,> %r0,%r4,comb_tests_2
	comb,>>= %r0,%r4,comb_tests_2
	comb,>> %r0,%r4,comb_tests_2
	comb,nsv %r0,%r4,comb_tests_2
	comb,ev %r0,%r4,comb_tests_2

comb_nullified_tests_1
	comb,n %r0,%r4,comb_tests_1
	comb,=,n %r0,%r4,comb_tests_1
	comb,<,n %r0,%r4,comb_tests_1
	comb,<=,n %r0,%r4,comb_tests_1
	comb,<<,n %r0,%r4,comb_tests_1
	comb,<<=,n %r0,%r4,comb_tests_1
	comb,sv,n %r0,%r4,comb_tests_1
	comb,od,n %r0,%r4,comb_tests_1

comb_nullified_tests_2
	comb,tr,n %r0,%r4,comb_tests_2
	comb,<>,n %r0,%r4,comb_tests_2
	comb,>=,n %r0,%r4,comb_tests_2
	comb,>,n %r0,%r4,comb_tests_2
	comb,>>=,n %r0,%r4,comb_tests_2
	comb,>>,n %r0,%r4,comb_tests_2
	comb,nsv,n %r0,%r4,comb_tests_2
	comb,ev,n %r0,%r4,comb_tests_2

comib_tests_1
	comib 0,%r4,comib_tests_1
	comib,= 0,%r4,comib_tests_1
	comib,< 0,%r4,comib_tests_1
	comib,<= 0,%r4,comib_tests_1
	comib,<< 0,%r4,comib_tests_1
	comib,<<= 0,%r4,comib_tests_1
	comib,sv 0,%r4,comib_tests_1
	comib,od 0,%r4,comib_tests_1

comib_tests_2
	comib,tr 0,%r4,comib_tests_2
	comib,<> 0,%r4,comib_tests_2
	comib,>= 0,%r4,comib_tests_2
	comib,> 0,%r4,comib_tests_2
	comib,>>= 0,%r4,comib_tests_2
	comib,>> 0,%r4,comib_tests_2
	comib,nsv 0,%r4,comib_tests_2
	comib,ev 0,%r4,comib_tests_2

comib_nullified_tests_1
	comib,n 0,%r4,comib_tests_1
	comib,=,n 0,%r4,comib_tests_1
	comib,<,n 0,%r4,comib_tests_1
	comib,<=,n 0,%r4,comib_tests_1
	comib,<<,n 0,%r4,comib_tests_1
	comib,<<=,n 0,%r4,comib_tests_1
	comib,sv,n 0,%r4,comib_tests_1
	comib,od,n 0,%r4,comib_tests_1

comib_nullified_tests_2
	comib,tr,n 0,%r4,comib_tests_2
	comib,<>,n 0,%r4,comib_tests_2
	comib,>=,n 0,%r4,comib_tests_2
	comib,>,n 0,%r4,comib_tests_2
	comib,>>=,n 0,%r4,comib_tests_2
	comib,>>,n 0,%r4,comib_tests_2
	comib,nsv,n 0,%r4,comib_tests_2
	comib,ev,n 0,%r4,comib_tests_2

addb_tests_1
	addb %r1,%r4,addb_tests_1
	addb,= %r1,%r4,addb_tests_1
	addb,< %r1,%r4,addb_tests_1
	addb,<= %r1,%r4,addb_tests_1
	addb,nuv %r1,%r4,addb_tests_1
	addb,znv %r1,%r4,addb_tests_1
	addb,sv %r1,%r4,addb_tests_1
	addb,od %r1,%r4,addb_tests_1

addb_tests_2
	addb,tr %r1,%r4,addb_tests_2
	addb,<> %r1,%r4,addb_tests_2
	addb,>= %r1,%r4,addb_tests_2
	addb,> %r1,%r4,addb_tests_2
	addb,uv %r1,%r4,addb_tests_2
	addb,vnz %r1,%r4,addb_tests_2
	addb,nsv %r1,%r4,addb_tests_2
	addb,ev %r1,%r4,addb_tests_2

addb_nullified_tests_1
	addb,n %r1,%r4,addb_tests_1
	addb,=,n %r1,%r4,addb_tests_1
	addb,<,n %r1,%r4,addb_tests_1
	addb,<=,n %r1,%r4,addb_tests_1
	addb,nuv,n %r1,%r4,addb_tests_1
	addb,znv,n %r1,%r4,addb_tests_1
	addb,sv,n %r1,%r4,addb_tests_1
	addb,od,n %r1,%r4,addb_tests_1

addb_nullified_tests_2
	addb,tr,n %r1,%r4,addb_tests_2
	addb,<>,n %r1,%r4,addb_tests_2
	addb,>=,n %r1,%r4,addb_tests_2
	addb,>,n %r1,%r4,addb_tests_2
	addb,uv,n %r1,%r4,addb_tests_2
	addb,vnz,n %r1,%r4,addb_tests_2
	addb,nsv,n %r1,%r4,addb_tests_2
	addb,ev,n %r1,%r4,addb_tests_2

addib_tests_1
	addib -1,%r4,addib_tests_1
	addib,= -1,%r4,addib_tests_1
	addib,< -1,%r4,addib_tests_1
	addib,<= -1,%r4,addib_tests_1
	addib,nuv -1,%r4,addib_tests_1
	addib,znv -1,%r4,addib_tests_1
	addib,sv -1,%r4,addib_tests_1
	addib,od -1,%r4,addib_tests_1

addib_tests_2
	addib,tr -1,%r4,addib_tests_2
	addib,<> -1,%r4,addib_tests_2
	addib,>= -1,%r4,addib_tests_2
	addib,> -1,%r4,addib_tests_2
	addib,uv -1,%r4,addib_tests_2
	addib,vnz -1,%r4,addib_tests_2
	addib,nsv -1,%r4,addib_tests_2
	addib,ev -1,%r4,addib_tests_2

addib_nullified_tests_1
	addib,n -1,%r4,addib_tests_1
	addib,=,n -1,%r4,addib_tests_1
	addib,<,n -1,%r4,addib_tests_1
	addib,<=,n -1,%r4,addib_tests_1
	addib,nuv,n -1,%r4,addib_tests_1
	addib,znv,n -1,%r4,addib_tests_1
	addib,sv,n -1,%r4,addib_tests_1
	addib,od,n -1,%r4,addib_tests_1

addib_nullified_tests_2
	addib,tr,n -1,%r4,addib_tests_2
	addib,<>,n -1,%r4,addib_tests_2
	addib,>=,n -1,%r4,addib_tests_2
	addib,>,n -1,%r4,addib_tests_2
	addib,uv,n -1,%r4,addib_tests_2
	addib,vnz,n -1,%r4,addib_tests_2
	addib,nsv,n -1,%r4,addib_tests_2
	addib,ev,n -1,%r4,addib_tests_2


; Needs to check lots of stuff (like corner bit cases)
bb_tests
	bvb,< %r4,bb_tests
	bvb,>= %r4,bb_tests
	bvb,<,n %r4,bb_tests
	bvb,>=,n %r4,bb_tests
	bb,< %r4,5,bb_tests
	bb,>= %r4,5,bb_tests
	bb,<,n %r4,5,bb_tests
	bb,>=,n %r4,5,bb_tests
	
; Computational instructions
add_tests
	add  %r4,%r5,%r6
	add,=  %r4,%r5,%r6
	add,<  %r4,%r5,%r6
	add,<=  %r4,%r5,%r6
	add,nuv  %r4,%r5,%r6
	add,znv  %r4,%r5,%r6
	add,sv  %r4,%r5,%r6
	add,od  %r4,%r5,%r6
	add,tr  %r4,%r5,%r6
	add,<>  %r4,%r5,%r6
	add,>=  %r4,%r5,%r6
	add,>  %r4,%r5,%r6
	add,uv  %r4,%r5,%r6
	add,vnz  %r4,%r5,%r6
	add,nsv  %r4,%r5,%r6
	add,ev  %r4,%r5,%r6

addl_tests
	addl  %r4,%r5,%r6
	addl,=  %r4,%r5,%r6
	addl,<  %r4,%r5,%r6
	addl,<=  %r4,%r5,%r6
	addl,nuv  %r4,%r5,%r6
	addl,znv  %r4,%r5,%r6
	addl,sv  %r4,%r5,%r6
	addl,od  %r4,%r5,%r6
	addl,tr  %r4,%r5,%r6
	addl,<>  %r4,%r5,%r6
	addl,>=  %r4,%r5,%r6
	addl,>  %r4,%r5,%r6
	addl,uv  %r4,%r5,%r6
	addl,vnz  %r4,%r5,%r6
	addl,nsv  %r4,%r5,%r6
	addl,ev  %r4,%r5,%r6

addo_tests
	addo  %r4,%r5,%r6
	addo,=  %r4,%r5,%r6
	addo,<  %r4,%r5,%r6
	addo,<=  %r4,%r5,%r6
	addo,nuv  %r4,%r5,%r6
	addo,znv  %r4,%r5,%r6
	addo,sv  %r4,%r5,%r6
	addo,od  %r4,%r5,%r6
	addo,tr  %r4,%r5,%r6
	addo,<>  %r4,%r5,%r6
	addo,>=  %r4,%r5,%r6
	addo,>  %r4,%r5,%r6
	addo,uv  %r4,%r5,%r6
	addo,vnz  %r4,%r5,%r6
	addo,nsv  %r4,%r5,%r6
	addo,ev  %r4,%r5,%r6

addc_tests
	addc  %r4,%r5,%r6
	addc,=  %r4,%r5,%r6
	addc,<  %r4,%r5,%r6
	addc,<=  %r4,%r5,%r6
	addc,nuv  %r4,%r5,%r6
	addc,znv  %r4,%r5,%r6
	addc,sv  %r4,%r5,%r6
	addc,od  %r4,%r5,%r6
	addc,tr  %r4,%r5,%r6
	addc,<>  %r4,%r5,%r6
	addc,>=  %r4,%r5,%r6
	addc,>  %r4,%r5,%r6
	addc,uv  %r4,%r5,%r6
	addc,vnz  %r4,%r5,%r6
	addc,nsv  %r4,%r5,%r6
	addc,ev  %r4,%r5,%r6

addco_tests
	addco  %r4,%r5,%r6
	addco,=  %r4,%r5,%r6
	addco,<  %r4,%r5,%r6
	addco,<=  %r4,%r5,%r6
	addco,nuv  %r4,%r5,%r6
	addco,znv  %r4,%r5,%r6
	addco,sv  %r4,%r5,%r6
	addco,od  %r4,%r5,%r6
	addco,tr  %r4,%r5,%r6
	addco,<>  %r4,%r5,%r6
	addco,>=  %r4,%r5,%r6
	addco,>  %r4,%r5,%r6
	addco,uv  %r4,%r5,%r6
	addco,vnz  %r4,%r5,%r6
	addco,nsv  %r4,%r5,%r6
	addco,ev  %r4,%r5,%r6

sh1add_tests
	sh1add  %r4,%r5,%r6
	sh1add,=  %r4,%r5,%r6
	sh1add,<  %r4,%r5,%r6
	sh1add,<=  %r4,%r5,%r6
	sh1add,nuv  %r4,%r5,%r6
	sh1add,znv  %r4,%r5,%r6
	sh1add,sv  %r4,%r5,%r6
	sh1add,od  %r4,%r5,%r6
	sh1add,tr  %r4,%r5,%r6
	sh1add,<>  %r4,%r5,%r6
	sh1add,>=  %r4,%r5,%r6
	sh1add,>  %r4,%r5,%r6
	sh1add,uv  %r4,%r5,%r6
	sh1add,vnz  %r4,%r5,%r6
	sh1add,nsv  %r4,%r5,%r6
	sh1add,ev  %r4,%r5,%r6

sh1addl_tests
	sh1addl  %r4,%r5,%r6
	sh1addl,=  %r4,%r5,%r6
	sh1addl,<  %r4,%r5,%r6
	sh1addl,<=  %r4,%r5,%r6
	sh1addl,nuv  %r4,%r5,%r6
	sh1addl,znv  %r4,%r5,%r6
	sh1addl,sv  %r4,%r5,%r6
	sh1addl,od  %r4,%r5,%r6
	sh1addl,tr  %r4,%r5,%r6
	sh1addl,<>  %r4,%r5,%r6
	sh1addl,>=  %r4,%r5,%r6
	sh1addl,>  %r4,%r5,%r6
	sh1addl,uv  %r4,%r5,%r6
	sh1addl,vnz  %r4,%r5,%r6
	sh1addl,nsv  %r4,%r5,%r6
	sh1addl,ev  %r4,%r5,%r6

sh1addo_tests
	sh1addo  %r4,%r5,%r6
	sh1addo,=  %r4,%r5,%r6
	sh1addo,<  %r4,%r5,%r6
	sh1addo,<=  %r4,%r5,%r6
	sh1addo,nuv  %r4,%r5,%r6
	sh1addo,znv  %r4,%r5,%r6
	sh1addo,sv  %r4,%r5,%r6
	sh1addo,od  %r4,%r5,%r6
	sh1addo,tr  %r4,%r5,%r6
	sh1addo,<>  %r4,%r5,%r6
	sh1addo,>=  %r4,%r5,%r6
	sh1addo,>  %r4,%r5,%r6
	sh1addo,uv  %r4,%r5,%r6
	sh1addo,vnz  %r4,%r5,%r6
	sh1addo,nsv  %r4,%r5,%r6
	sh1addo,ev  %r4,%r5,%r6


sh2add_tests
	sh2add  %r4,%r5,%r6
	sh2add,=  %r4,%r5,%r6
	sh2add,<  %r4,%r5,%r6
	sh2add,<=  %r4,%r5,%r6
	sh2add,nuv  %r4,%r5,%r6
	sh2add,znv  %r4,%r5,%r6
	sh2add,sv  %r4,%r5,%r6
	sh2add,od  %r4,%r5,%r6
	sh2add,tr  %r4,%r5,%r6
	sh2add,<>  %r4,%r5,%r6
	sh2add,>=  %r4,%r5,%r6
	sh2add,>  %r4,%r5,%r6
	sh2add,uv  %r4,%r5,%r6
	sh2add,vnz  %r4,%r5,%r6
	sh2add,nsv  %r4,%r5,%r6
	sh2add,ev  %r4,%r5,%r6

sh2addl_tests
	sh2addl  %r4,%r5,%r6
	sh2addl,=  %r4,%r5,%r6
	sh2addl,<  %r4,%r5,%r6
	sh2addl,<=  %r4,%r5,%r6
	sh2addl,nuv  %r4,%r5,%r6
	sh2addl,znv  %r4,%r5,%r6
	sh2addl,sv  %r4,%r5,%r6
	sh2addl,od  %r4,%r5,%r6
	sh2addl,tr  %r4,%r5,%r6
	sh2addl,<>  %r4,%r5,%r6
	sh2addl,>=  %r4,%r5,%r6
	sh2addl,>  %r4,%r5,%r6
	sh2addl,uv  %r4,%r5,%r6
	sh2addl,vnz  %r4,%r5,%r6
	sh2addl,nsv  %r4,%r5,%r6
	sh2addl,ev  %r4,%r5,%r6

sh2addo_tests
	sh2addo  %r4,%r5,%r6
	sh2addo,=  %r4,%r5,%r6
	sh2addo,<  %r4,%r5,%r6
	sh2addo,<=  %r4,%r5,%r6
	sh2addo,nuv  %r4,%r5,%r6
	sh2addo,znv  %r4,%r5,%r6
	sh2addo,sv  %r4,%r5,%r6
	sh2addo,od  %r4,%r5,%r6
	sh2addo,tr  %r4,%r5,%r6
	sh2addo,<>  %r4,%r5,%r6
	sh2addo,>=  %r4,%r5,%r6
	sh2addo,>  %r4,%r5,%r6
	sh2addo,uv  %r4,%r5,%r6
	sh2addo,vnz  %r4,%r5,%r6
	sh2addo,nsv  %r4,%r5,%r6
	sh2addo,ev  %r4,%r5,%r6


sh3add_tests
	sh3add  %r4,%r5,%r6
	sh3add,=  %r4,%r5,%r6
	sh3add,<  %r4,%r5,%r6
	sh3add,<=  %r4,%r5,%r6
	sh3add,nuv  %r4,%r5,%r6
	sh3add,znv  %r4,%r5,%r6
	sh3add,sv  %r4,%r5,%r6
	sh3add,od  %r4,%r5,%r6
	sh3add,tr  %r4,%r5,%r6
	sh3add,<>  %r4,%r5,%r6
	sh3add,>=  %r4,%r5,%r6
	sh3add,>  %r4,%r5,%r6
	sh3add,uv  %r4,%r5,%r6
	sh3add,vnz  %r4,%r5,%r6
	sh3add,nsv  %r4,%r5,%r6
	sh3add,ev  %r4,%r5,%r6

sh3addl_tests
	sh3addl  %r4,%r5,%r6
	sh3addl,=  %r4,%r5,%r6
	sh3addl,<  %r4,%r5,%r6
	sh3addl,<=  %r4,%r5,%r6
	sh3addl,nuv  %r4,%r5,%r6
	sh3addl,znv  %r4,%r5,%r6
	sh3addl,sv  %r4,%r5,%r6
	sh3addl,od  %r4,%r5,%r6
	sh3addl,tr  %r4,%r5,%r6
	sh3addl,<>  %r4,%r5,%r6
	sh3addl,>=  %r4,%r5,%r6
	sh3addl,>  %r4,%r5,%r6
	sh3addl,uv  %r4,%r5,%r6
	sh3addl,vnz  %r4,%r5,%r6
	sh3addl,nsv  %r4,%r5,%r6
	sh3addl,ev  %r4,%r5,%r6

sh3addo_tests
	sh3addo  %r4,%r5,%r6
	sh3addo,=  %r4,%r5,%r6
	sh3addo,<  %r4,%r5,%r6
	sh3addo,<=  %r4,%r5,%r6
	sh3addo,nuv  %r4,%r5,%r6
	sh3addo,znv  %r4,%r5,%r6
	sh3addo,sv  %r4,%r5,%r6
	sh3addo,od  %r4,%r5,%r6
	sh3addo,tr  %r4,%r5,%r6
	sh3addo,<>  %r4,%r5,%r6
	sh3addo,>=  %r4,%r5,%r6
	sh3addo,>  %r4,%r5,%r6
	sh3addo,uv  %r4,%r5,%r6
	sh3addo,vnz  %r4,%r5,%r6
	sh3addo,nsv  %r4,%r5,%r6
	sh3addo,ev  %r4,%r5,%r6


sub_tests
	sub %r4,%r5,%r6
	sub,= %r4,%r5,%r6
	sub,< %r4,%r5,%r6
	sub,<= %r4,%r5,%r6
	sub,<< %r4,%r5,%r6
	sub,<<= %r4,%r5,%r6
	sub,sv %r4,%r5,%r6
	sub,od %r4,%r5,%r6
	sub,tr %r4,%r5,%r6
	sub,<> %r4,%r5,%r6
	sub,>= %r4,%r5,%r6
	sub,> %r4,%r5,%r6
	sub,>>= %r4,%r5,%r6
	sub,>> %r4,%r5,%r6
	sub,nsv %r4,%r5,%r6
	sub,ev %r4,%r5,%r6

subo_tests
	subo %r4,%r5,%r6
	subo,= %r4,%r5,%r6
	subo,< %r4,%r5,%r6
	subo,<= %r4,%r5,%r6
	subo,<< %r4,%r5,%r6
	subo,<<= %r4,%r5,%r6
	subo,sv %r4,%r5,%r6
	subo,od %r4,%r5,%r6
	subo,tr %r4,%r5,%r6
	subo,<> %r4,%r5,%r6
	subo,>= %r4,%r5,%r6
	subo,> %r4,%r5,%r6
	subo,>>= %r4,%r5,%r6
	subo,>> %r4,%r5,%r6
	subo,nsv %r4,%r5,%r6
	subo,ev %r4,%r5,%r6

subb_tests
	subb %r4,%r5,%r6
	subb,= %r4,%r5,%r6
	subb,< %r4,%r5,%r6
	subb,<= %r4,%r5,%r6
	subb,<< %r4,%r5,%r6
	subb,<<= %r4,%r5,%r6
	subb,sv %r4,%r5,%r6
	subb,od %r4,%r5,%r6
	subb,tr %r4,%r5,%r6
	subb,<> %r4,%r5,%r6
	subb,>= %r4,%r5,%r6
	subb,> %r4,%r5,%r6
	subb,>>= %r4,%r5,%r6
	subb,>> %r4,%r5,%r6
	subb,nsv %r4,%r5,%r6
	subb,ev %r4,%r5,%r6

subbo_tests
	subbo %r4,%r5,%r6
	subbo,= %r4,%r5,%r6
	subbo,< %r4,%r5,%r6
	subbo,<= %r4,%r5,%r6
	subbo,<< %r4,%r5,%r6
	subbo,<<= %r4,%r5,%r6
	subbo,sv %r4,%r5,%r6
	subbo,od %r4,%r5,%r6
	subbo,tr %r4,%r5,%r6
	subbo,<> %r4,%r5,%r6
	subbo,>= %r4,%r5,%r6
	subbo,> %r4,%r5,%r6
	subbo,>>= %r4,%r5,%r6
	subbo,>> %r4,%r5,%r6
	subbo,nsv %r4,%r5,%r6
	subbo,ev %r4,%r5,%r6

subt_tests
	subt %r4,%r5,%r6
	subt,= %r4,%r5,%r6
	subt,< %r4,%r5,%r6
	subt,<= %r4,%r5,%r6
	subt,<< %r4,%r5,%r6
	subt,<<= %r4,%r5,%r6
	subt,sv %r4,%r5,%r6
	subt,od %r4,%r5,%r6
	subt,tr %r4,%r5,%r6
	subt,<> %r4,%r5,%r6
	subt,>= %r4,%r5,%r6
	subt,> %r4,%r5,%r6
	subt,>>= %r4,%r5,%r6
	subt,>> %r4,%r5,%r6
	subt,nsv %r4,%r5,%r6
	subt,ev %r4,%r5,%r6

subto_tests
	subto %r4,%r5,%r6
	subto,= %r4,%r5,%r6
	subto,< %r4,%r5,%r6
	subto,<= %r4,%r5,%r6
	subto,<< %r4,%r5,%r6
	subto,<<= %r4,%r5,%r6
	subto,sv %r4,%r5,%r6
	subto,od %r4,%r5,%r6
	subto,tr %r4,%r5,%r6
	subto,<> %r4,%r5,%r6
	subto,>= %r4,%r5,%r6
	subto,> %r4,%r5,%r6
	subto,>>= %r4,%r5,%r6
	subto,>> %r4,%r5,%r6
	subto,nsv %r4,%r5,%r6
	subto,ev %r4,%r5,%r6

ds_tests
	ds %r4,%r5,%r6
	ds,= %r4,%r5,%r6
	ds,< %r4,%r5,%r6
	ds,<= %r4,%r5,%r6
	ds,<< %r4,%r5,%r6
	ds,<<= %r4,%r5,%r6
	ds,sv %r4,%r5,%r6
	ds,od %r4,%r5,%r6
	ds,tr %r4,%r5,%r6
	ds,<> %r4,%r5,%r6
	ds,>= %r4,%r5,%r6
	ds,> %r4,%r5,%r6
	ds,>>= %r4,%r5,%r6
	ds,>> %r4,%r5,%r6
	ds,nsv %r4,%r5,%r6
	ds,ev %r4,%r5,%r6

comclr_tests
	comclr %r4,%r5,%r6
	comclr,= %r4,%r5,%r6
	comclr,< %r4,%r5,%r6
	comclr,<= %r4,%r5,%r6
	comclr,<< %r4,%r5,%r6
	comclr,<<= %r4,%r5,%r6
	comclr,sv %r4,%r5,%r6
	comclr,od %r4,%r5,%r6
	comclr,tr %r4,%r5,%r6
	comclr,<> %r4,%r5,%r6
	comclr,>= %r4,%r5,%r6
	comclr,> %r4,%r5,%r6
	comclr,>>= %r4,%r5,%r6
	comclr,>> %r4,%r5,%r6
	comclr,nsv %r4,%r5,%r6
	comclr,ev %r4,%r5,%r6

or_tests
	or %r4,%r5,%r6
	or,= %r4,%r5,%r6
	or,< %r4,%r5,%r6
	or,<= %r4,%r5,%r6
	or,od %r4,%r5,%r6
	or,tr %r4,%r5,%r6
	or,<> %r4,%r5,%r6
	or,>= %r4,%r5,%r6
	or,> %r4,%r5,%r6
	or,ev %r4,%r5,%r6
xor_tests
	xor %r4,%r5,%r6
	xor,= %r4,%r5,%r6
	xor,< %r4,%r5,%r6
	xor,<= %r4,%r5,%r6
	xor,od %r4,%r5,%r6
	xor,tr %r4,%r5,%r6
	xor,<> %r4,%r5,%r6
	xor,>= %r4,%r5,%r6
	xor,> %r4,%r5,%r6
	xor,ev %r4,%r5,%r6

and_tests
	and %r4,%r5,%r6
	and,= %r4,%r5,%r6
	and,< %r4,%r5,%r6
	and,<= %r4,%r5,%r6
	and,od %r4,%r5,%r6
	and,tr %r4,%r5,%r6
	and,<> %r4,%r5,%r6
	and,>= %r4,%r5,%r6
	and,> %r4,%r5,%r6
	and,ev %r4,%r5,%r6

andcm_tests
	andcm %r4,%r5,%r6
	andcm,= %r4,%r5,%r6
	andcm,< %r4,%r5,%r6
	andcm,<= %r4,%r5,%r6
	andcm,od %r4,%r5,%r6
	andcm,tr %r4,%r5,%r6
	andcm,<> %r4,%r5,%r6
	andcm,>= %r4,%r5,%r6
	andcm,> %r4,%r5,%r6
	andcm,ev %r4,%r5,%r6


uxor_tests
	uxor %r4,%r5,%r6
	uxor,sbz %r4,%r5,%r6
	uxor,shz %r4,%r5,%r6
	uxor,sdc %r4,%r5,%r6
	uxor,sbc %r4,%r5,%r6
	uxor,shc %r4,%r5,%r6
	uxor,tr %r4,%r5,%r6
	uxor,nbz %r4,%r5,%r6
	uxor,nhz %r4,%r5,%r6
	uxor,ndc %r4,%r5,%r6
	uxor,nbc %r4,%r5,%r6
	uxor,nhc %r4,%r5,%r6

uaddcm_tests
	uaddcm %r4,%r5,%r6
	uaddcm,sbz %r4,%r5,%r6
	uaddcm,shz %r4,%r5,%r6
	uaddcm,sdc %r4,%r5,%r6
	uaddcm,sbc %r4,%r5,%r6
	uaddcm,shc %r4,%r5,%r6
	uaddcm,tr %r4,%r5,%r6
	uaddcm,nbz %r4,%r5,%r6
	uaddcm,nhz %r4,%r5,%r6
	uaddcm,ndc %r4,%r5,%r6
	uaddcm,nbc %r4,%r5,%r6
	uaddcm,nhc %r4,%r5,%r6

uaddcmt_tests
	uaddcmt %r4,%r5,%r6
	uaddcmt,sbz %r4,%r5,%r6
	uaddcmt,shz %r4,%r5,%r6
	uaddcmt,sdc %r4,%r5,%r6
	uaddcmt,sbc %r4,%r5,%r6
	uaddcmt,shc %r4,%r5,%r6
	uaddcmt,tr %r4,%r5,%r6
	uaddcmt,nbz %r4,%r5,%r6
	uaddcmt,nhz %r4,%r5,%r6
	uaddcmt,ndc %r4,%r5,%r6
	uaddcmt,nbc %r4,%r5,%r6
	uaddcmt,nhc %r4,%r5,%r6

dcor_tests
	dcor %r4,%r5
	dcor,sbz %r4,%r5
	dcor,shz %r4,%r5
	dcor,sdc %r4,%r5
	dcor,sbc %r4,%r5
	dcor,shc %r4,%r5
	dcor,tr %r4,%r5
	dcor,nbz %r4,%r5
	dcor,nhz %r4,%r5
	dcor,ndc %r4,%r5
	dcor,nbc %r4,%r5
	dcor,nhc %r4,%r5

idcor_tests
	idcor %r4,%r5
	idcor,sbz %r4,%r5
	idcor,shz %r4,%r5
	idcor,sdc %r4,%r5
	idcor,sbc %r4,%r5
	idcor,shc %r4,%r5
	idcor,tr %r4,%r5
	idcor,nbz %r4,%r5
	idcor,nhz %r4,%r5
	idcor,ndc %r4,%r5
	idcor,nbc %r4,%r5
	idcor,nhc %r4,%r5

addi_tests
	addi  123,%r5,%r6
	addi,=  123,%r5,%r6
	addi,<  123,%r5,%r6
	addi,<=  123,%r5,%r6
	addi,nuv  123,%r5,%r6
	addi,znv  123,%r5,%r6
	addi,sv  123,%r5,%r6
	addi,od  123,%r5,%r6
	addi,tr  123,%r5,%r6
	addi,<>  123,%r5,%r6
	addi,>=  123,%r5,%r6
	addi,>  123,%r5,%r6
	addi,uv  123,%r5,%r6
	addi,vnz  123,%r5,%r6
	addi,nsv  123,%r5,%r6
	addi,ev  123,%r5,%r6

addio_tests
	addio  123,%r5,%r6
	addio,=  123,%r5,%r6
	addio,<  123,%r5,%r6
	addio,<=  123,%r5,%r6
	addio,nuv  123,%r5,%r6
	addio,znv  123,%r5,%r6
	addio,sv  123,%r5,%r6
	addio,od  123,%r5,%r6
	addio,tr  123,%r5,%r6
	addio,<>  123,%r5,%r6
	addio,>=  123,%r5,%r6
	addio,>  123,%r5,%r6
	addio,uv  123,%r5,%r6
	addio,vnz  123,%r5,%r6
	addio,nsv  123,%r5,%r6
	addio,ev  123,%r5,%r6

addit_tests
	addit  123,%r5,%r6
	addit,=  123,%r5,%r6
	addit,<  123,%r5,%r6
	addit,<=  123,%r5,%r6
	addit,nuv  123,%r5,%r6
	addit,znv  123,%r5,%r6
	addit,sv  123,%r5,%r6
	addit,od  123,%r5,%r6
	addit,tr  123,%r5,%r6
	addit,<>  123,%r5,%r6
	addit,>=  123,%r5,%r6
	addit,>  123,%r5,%r6
	addit,uv  123,%r5,%r6
	addit,vnz  123,%r5,%r6
	addit,nsv  123,%r5,%r6
	addit,ev  123,%r5,%r6

addito_tests
	addito  123,%r5,%r6
	addito,=  123,%r5,%r6
	addito,<  123,%r5,%r6
	addito,<=  123,%r5,%r6
	addito,nuv  123,%r5,%r6
	addito,znv  123,%r5,%r6
	addito,sv  123,%r5,%r6
	addito,od  123,%r5,%r6
	addito,tr  123,%r5,%r6
	addito,<>  123,%r5,%r6
	addito,>=  123,%r5,%r6
	addito,>  123,%r5,%r6
	addito,uv  123,%r5,%r6
	addito,vnz  123,%r5,%r6
	addito,nsv  123,%r5,%r6
	addito,ev  123,%r5,%r6

subi_tests
	subi 123,%r5,%r6
	subi,= 123,%r5,%r6
	subi,< 123,%r5,%r6
	subi,<= 123,%r5,%r6
	subi,<< 123,%r5,%r6
	subi,<<= 123,%r5,%r6
	subi,sv 123,%r5,%r6
	subi,od 123,%r5,%r6
	subi,tr 123,%r5,%r6
	subi,<> 123,%r5,%r6
	subi,>= 123,%r5,%r6
	subi,> 123,%r5,%r6
	subi,>>= 123,%r5,%r6
	subi,>> 123,%r5,%r6
	subi,nsv 123,%r5,%r6
	subi,ev 123,%r5,%r6

subio_tests
	subio 123,%r5,%r6
	subio,= 123,%r5,%r6
	subio,< 123,%r5,%r6
	subio,<= 123,%r5,%r6
	subio,<< 123,%r5,%r6
	subio,<<= 123,%r5,%r6
	subio,sv 123,%r5,%r6
	subio,od 123,%r5,%r6
	subio,tr 123,%r5,%r6
	subio,<> 123,%r5,%r6
	subio,>= 123,%r5,%r6
	subio,> 123,%r5,%r6
	subio,>>= 123,%r5,%r6
	subio,>> 123,%r5,%r6
	subio,nsv 123,%r5,%r6
	subio,ev 123,%r5,%r6

comiclr_tests
	comiclr 123,%r5,%r6
	comiclr,= 123,%r5,%r6
	comiclr,< 123,%r5,%r6
	comiclr,<= 123,%r5,%r6
	comiclr,<< 123,%r5,%r6
	comiclr,<<= 123,%r5,%r6
	comiclr,sv 123,%r5,%r6
	comiclr,od 123,%r5,%r6
	comiclr,tr 123,%r5,%r6
	comiclr,<> 123,%r5,%r6
	comiclr,>= 123,%r5,%r6
	comiclr,> 123,%r5,%r6
	comiclr,>>= 123,%r5,%r6
	comiclr,>> 123,%r5,%r6
	comiclr,nsv 123,%r5,%r6
	comiclr,ev 123,%r5,%r6

vshd_tests
	vshd %r4,%r5,%r6
	vshd,= %r4,%r5,%r6
	vshd,< %r4,%r5,%r6
	vshd,od %r4,%r5,%r6
	vshd,tr %r4,%r5,%r6
	vshd,<> %r4,%r5,%r6
	vshd,>= %r4,%r5,%r6
	vshd,ev %r4,%r5,%r6

shd_tests
	shd %r4,%r5,5,%r6
	shd,= %r4,%r5,5,%r6
	shd,< %r4,%r5,5,%r6
	shd,od %r4,%r5,5,%r6
	shd,tr %r4,%r5,5,%r6
	shd,<> %r4,%r5,5,%r6
	shd,>= %r4,%r5,5,%r6
	shd,ev %r4,%r5,5,%r6

extru_tests
	extru %r4,5,10,%r6
	extru,= %r4,5,10,%r6
	extru,< %r4,5,10,%r6
	extru,od %r4,5,10,%r6
	extru,tr %r4,5,10,%r6
	extru,<> %r4,5,10,%r6
	extru,>= %r4,5,10,%r6
	extru,ev %r4,5,10,%r6

extrs_tests
	extrs %r4,5,10,%r6
	extrs,= %r4,5,10,%r6
	extrs,< %r4,5,10,%r6
	extrs,od %r4,5,10,%r6
	extrs,tr %r4,5,10,%r6
	extrs,<> %r4,5,10,%r6
	extrs,>= %r4,5,10,%r6
	extrs,ev %r4,5,10,%r6

zdep_tests
	zdep %r4,5,10,%r6
	zdep,= %r4,5,10,%r6
	zdep,< %r4,5,10,%r6
	zdep,od %r4,5,10,%r6
	zdep,tr %r4,5,10,%r6
	zdep,<> %r4,5,10,%r6
	zdep,>= %r4,5,10,%r6
	zdep,ev %r4,5,10,%r6

dep_tests
	dep %r4,5,10,%r6
	dep,= %r4,5,10,%r6
	dep,< %r4,5,10,%r6
	dep,od %r4,5,10,%r6
	dep,tr %r4,5,10,%r6
	dep,<> %r4,5,10,%r6
	dep,>= %r4,5,10,%r6
	dep,ev %r4,5,10,%r6

vextru_tests
	vextru %r4,5,%r6
	vextru,= %r4,5,%r6
	vextru,< %r4,5,%r6
	vextru,od %r4,5,%r6
	vextru,tr %r4,5,%r6
	vextru,<> %r4,5,%r6
	vextru,>= %r4,5,%r6
	vextru,ev %r4,5,%r6

vextrs_tests
	vextrs %r4,5,%r6
	vextrs,= %r4,5,%r6
	vextrs,< %r4,5,%r6
	vextrs,od %r4,5,%r6
	vextrs,tr %r4,5,%r6
	vextrs,<> %r4,5,%r6
	vextrs,>= %r4,5,%r6
	vextrs,ev %r4,5,%r6

zvdep_tests
	zvdep %r4,5,%r6
	zvdep,= %r4,5,%r6
	zvdep,< %r4,5,%r6
	zvdep,od %r4,5,%r6
	zvdep,tr %r4,5,%r6
	zvdep,<> %r4,5,%r6
	zvdep,>= %r4,5,%r6
	zvdep,ev %r4,5,%r6


vdep_tests
	vdep %r4,5,%r6
	vdep,= %r4,5,%r6
	vdep,< %r4,5,%r6
	vdep,od %r4,5,%r6
	vdep,tr %r4,5,%r6
	vdep,<> %r4,5,%r6
	vdep,>= %r4,5,%r6
	vdep,ev %r4,5,%r6

vdepi_tests
	vdepi -1,5,%r6
	vdepi,= -1,5,%r6
	vdepi,< -1,5,%r6
	vdepi,od -1,5,%r6
	vdepi,tr -1,5,%r6
	vdepi,<> -1,5,%r6
	vdepi,>= -1,5,%r6
	vdepi,ev -1,5,%r6

zvdepi_tests
	zvdepi -1,5,%r6
	zvdepi,= -1,5,%r6
	zvdepi,< -1,5,%r6
	zvdepi,od -1,5,%r6
	zvdepi,tr -1,5,%r6
	zvdepi,<> -1,5,%r6
	zvdepi,>= -1,5,%r6
	zvdepi,ev -1,5,%r6

depi_tests
	depi -1,4,10,%r6
	depi,= -1,4,10,%r6
	depi,< -1,4,10,%r6
	depi,od -1,4,10,%r6
	depi,tr -1,4,10,%r6
	depi,<> -1,4,10,%r6
	depi,>= -1,4,10,%r6
	depi,ev -1,4,10,%r6

zdepi_tests
	zdepi -1,4,10,%r6
	zdepi,= -1,4,10,%r6
	zdepi,< -1,4,10,%r6
	zdepi,od -1,4,10,%r6
	zdepi,tr -1,4,10,%r6
	zdepi,<> -1,4,10,%r6
	zdepi,>= -1,4,10,%r6
	zdepi,ev -1,4,10,%r6


system_control_tests
	break 5,12
	rfi
	rfir
	ssm 5,%r4
	rsm 5,%r4
	mtsm %r4
	ldsid (%sr0,%r5),%r4
	mtsp %r4,%sr0
	mtctl %r4,%cr10
	mfsp %sr0,%r4
	mfctl %cr10,%r4
	sync
	syncdma
	diag 1234

probe_tests
	prober (%sr0,%r5),%r6,%r7
	proberi (%sr0,%r5),1,%r7
	probew (%sr0,%r5),%r6,%r7
	probewi (%sr0,%r5),1,%r7
	
lpa_tests
	lpa %r4(%sr0,%r5),%r6
	lpa,m %r4(%sr0,%r5),%r6
	lha %r4(%sr0,%r5),%r6
	lha,m %r4(%sr0,%r5),%r6
	lci %r4(%sr0,%r5),%r6

purge_tests
	pdtlb %r4(%sr0,%r5)
	pdtlb,m %r4(%sr0,%r5)
	pitlb %r4(%sr0,%r5)
	pitlb,m %r4(%sr0,%r5)
	pdtlbe %r4(%sr0,%r5)
	pdtlbe,m %r4(%sr0,%r5)
	pitlbe %r4(%sr0,%r5)
	pitlbe,m %r4(%sr0,%r5)
	pdc %r4(%sr0,%r5)
	pdc,m %r4(%sr0,%r5)
	fdc %r4(%sr0,%r5)
	fdc,m %r4(%sr0,%r5)
	fic %r4(%sr0,%r5)
	fic,m %r4(%sr0,%r5)
	fdce %r4(%sr0,%r5)
	fdce,m %r4(%sr0,%r5)
	fice %r4(%sr0,%r5)
	fice,m %r4(%sr0,%r5)

insert_tests
	idtlba %r4,(%sr0,%r5)
	iitlba %r4,(%sr0,%r5)
	idtlbp %r4,(%sr0,%r5)
	iitlbp %r4,(%sr0,%r5)

fpu_misc_tests
	ftest

fpu_memory_indexing_tests
	fldwx %r4(%sr0,%r5),%fr6
	fldwx,s %r4(%sr0,%r5),%fr6
	fldwx,m %r4(%sr0,%r5),%fr6
	fldwx,sm %r4(%sr0,%r5),%fr6
	flddx %r4(%sr0,%r5),%fr6
	flddx,s %r4(%sr0,%r5),%fr6
	flddx,m %r4(%sr0,%r5),%fr6
	flddx,sm %r4(%sr0,%r5),%fr6
	fstwx %fr6,%r4(%sr0,%r5)
	fstwx,s %fr6,%r4(%sr0,%r5)
	fstwx,m %fr6,%r4(%sr0,%r5)
	fstwx,sm %fr6,%r4(%sr0,%r5)
	fstdx %fr6,%r4(%sr0,%r5)
	fstdx,s %fr6,%r4(%sr0,%r5)
	fstdx,m %fr6,%r4(%sr0,%r5)
	fstdx,sm %fr6,%r4(%sr0,%r5)
	fstqx %fr6,%r4(%sr0,%r5)
	fstqx,s %fr6,%r4(%sr0,%r5)
	fstqx,m %fr6,%r4(%sr0,%r5)
	fstqx,sm %fr6,%r4(%sr0,%r5)

fpu_short_memory_tests
	fldws 0(%sr0,%r5),%fr6
	fldws,mb 0(%sr0,%r5),%fr6
	fldws,ma 0(%sr0,%r5),%fr6
	fldds 0(%sr0,%r5),%fr6
	fldds,mb 0(%sr0,%r5),%fr6
	fldds,ma 0(%sr0,%r5),%fr6
	fstws %fr6,0(%sr0,%r5)
	fstws,mb %fr6,0(%sr0,%r5)
	fstws,ma %fr6,0(%sr0,%r5)
	fstds %fr6,0(%sr0,%r5)
	fstds,mb %fr6,0(%sr0,%r5)
	fstds,ma %fr6,0(%sr0,%r5)
	fstqs %fr6,0(%sr0,%r5)
	fstqs,mb %fr6,0(%sr0,%r5)
	fstqs,ma %fr6,0(%sr0,%r5)


fcpy_tests
	fcpy,sgl %fr5,%fr10
	fcpy,dbl %fr5,%fr10
	fcpy,quad %fr5,%fr10
	fcpy,sgl %fr20,%fr24
	fcpy,dbl %fr20,%fr24

fabs_tests
	fabs,sgl %fr5,%fr10
	fabs,dbl %fr5,%fr10
	fabs,quad %fr5,%fr10
	fabs,sgl %fr20,%fr24
	fabs,dbl %fr20,%fr24

fsqrt_tests
	fsqrt,sgl %fr5,%fr10
	fsqrt,dbl %fr5,%fr10
	fsqrt,quad %fr5,%fr10
	fsqrt,sgl %fr20,%fr24
	fsqrt,dbl %fr20,%fr24

frnd_tests
	frnd,sgl %fr5,%fr10
	frnd,dbl %fr5,%fr10
	frnd,quad %fr5,%fr10
	frnd,sgl %fr20,%fr24
	frnd,dbl %fr20,%fr24
	
fcnvff_tests
	fcnvff,sgl,sgl %fr5,%fr10
	fcnvff,sgl,dbl %fr5,%fr10
	fcnvff,sgl,quad %fr5,%fr10
	fcnvff,dbl,sgl %fr5,%fr10
	fcnvff,dbl,dbl %fr5,%fr10
	fcnvff,dbl,quad %fr5,%fr10
	fcnvff,quad,sgl %fr5,%fr10
	fcnvff,quad,dbl %fr5,%fr10
	fcnvff,quad,quad %fr5,%fr10
	fcnvff,sgl,sgl %fr20,%fr24
	fcnvff,sgl,dbl %fr20,%fr24
	fcnvff,sgl,quad %fr20,%fr24
	fcnvff,dbl,sgl %fr20,%fr24
	fcnvff,dbl,dbl %fr20,%fr24
	fcnvff,dbl,quad %fr20,%fr24
	fcnvff,quad,sgl %fr20,%fr24
	fcnvff,quad,dbl %fr20,%fr24
	fcnvff,quad,quad %fr20,%fr24

fcnvxf_tests
	fcnvxf,sgl,sgl %fr5,%fr10
	fcnvxf,sgl,dbl %fr5,%fr10
	fcnvxf,sgl,quad %fr5,%fr10
	fcnvxf,dbl,sgl %fr5,%fr10
	fcnvxf,dbl,dbl %fr5,%fr10
	fcnvxf,dbl,quad %fr5,%fr10
	fcnvxf,quad,sgl %fr5,%fr10
	fcnvxf,quad,dbl %fr5,%fr10
	fcnvxf,quad,quad %fr5,%fr10
	fcnvxf,sgl,sgl %fr20,%fr24
	fcnvxf,sgl,dbl %fr20,%fr24
	fcnvxf,sgl,quad %fr20,%fr24
	fcnvxf,dbl,sgl %fr20,%fr24
	fcnvxf,dbl,dbl %fr20,%fr24
	fcnvxf,dbl,quad %fr20,%fr24
	fcnvxf,quad,sgl %fr20,%fr24
	fcnvxf,quad,dbl %fr20,%fr24
	fcnvxf,quad,quad %fr20,%fr24

fcnvfx_tests
	fcnvfx,sgl,sgl %fr5,%fr10
	fcnvfx,sgl,dbl %fr5,%fr10
	fcnvfx,sgl,quad %fr5,%fr10
	fcnvfx,dbl,sgl %fr5,%fr10
	fcnvfx,dbl,dbl %fr5,%fr10
	fcnvfx,dbl,quad %fr5,%fr10
	fcnvfx,quad,sgl %fr5,%fr10
	fcnvfx,quad,dbl %fr5,%fr10
	fcnvfx,quad,quad %fr5,%fr10
	fcnvfx,sgl,sgl %fr20,%fr24
	fcnvfx,sgl,dbl %fr20,%fr24
	fcnvfx,sgl,quad %fr20,%fr24
	fcnvfx,dbl,sgl %fr20,%fr24
	fcnvfx,dbl,dbl %fr20,%fr24
	fcnvfx,dbl,quad %fr20,%fr24
	fcnvfx,quad,sgl %fr20,%fr24
	fcnvfx,quad,dbl %fr20,%fr24
	fcnvfx,quad,quad %fr20,%fr24

fcnvfxt_tests
	fcnvfxt,sgl,sgl %fr5,%fr10
	fcnvfxt,sgl,dbl %fr5,%fr10
	fcnvfxt,sgl,quad %fr5,%fr10
	fcnvfxt,dbl,sgl %fr5,%fr10
	fcnvfxt,dbl,dbl %fr5,%fr10
	fcnvfxt,dbl,quad %fr5,%fr10
	fcnvfxt,quad,sgl %fr5,%fr10
	fcnvfxt,quad,dbl %fr5,%fr10
	fcnvfxt,quad,quad %fr5,%fr10
	fcnvfxt,sgl,sgl %fr20,%fr24
	fcnvfxt,sgl,dbl %fr20,%fr24
	fcnvfxt,sgl,quad %fr20,%fr24
	fcnvfxt,dbl,sgl %fr20,%fr24
	fcnvfxt,dbl,dbl %fr20,%fr24
	fcnvfxt,dbl,quad %fr20,%fr24
	fcnvfxt,quad,sgl %fr20,%fr24
	fcnvfxt,quad,dbl %fr20,%fr24
	fcnvfxt,quad,quad %fr20,%fr24

fadd_tests
	fadd,sgl %fr4,%fr8,%fr12
	fadd,dbl %fr4,%fr8,%fr12
	fadd,quad %fr4,%fr8,%fr12
	fadd,sgl %fr20,%fr24,%fr28
	fadd,dbl %fr20,%fr24,%fr28
	fadd,quad %fr20,%fr24,%fr28

fsub_tests
	fsub,sgl %fr4,%fr8,%fr12
	fsub,dbl %fr4,%fr8,%fr12
	fsub,quad %fr4,%fr8,%fr12
	fsub,sgl %fr20,%fr24,%fr28
	fsub,dbl %fr20,%fr24,%fr28
	fsub,quad %fr20,%fr24,%fr28

fmpy_tests
	fmpy,sgl %fr4,%fr8,%fr12
	fmpy,dbl %fr4,%fr8,%fr12
	fmpy,quad %fr4,%fr8,%fr12
	fmpy,sgl %fr20,%fr24,%fr28
	fmpy,dbl %fr20,%fr24,%fr28
	fmpy,quad %fr20,%fr24,%fr28

fdiv_tests
	fdiv,sgl %fr4,%fr8,%fr12
	fdiv,dbl %fr4,%fr8,%fr12
	fdiv,quad %fr4,%fr8,%fr12
	fdiv,sgl %fr20,%fr24,%fr28
	fdiv,dbl %fr20,%fr24,%fr28
	fdiv,quad %fr20,%fr24,%fr28

frem_tests
	frem,sgl %fr4,%fr8,%fr12
	frem,dbl %fr4,%fr8,%fr12
	frem,quad %fr4,%fr8,%fr12
	frem,sgl %fr20,%fr24,%fr28
	frem,dbl %fr20,%fr24,%fr28
	frem,quad %fr20,%fr24,%fr28

fcmp_sgl_tests_1
	fcmp,sgl,false? %fr4,%fr5
	fcmp,sgl,false %fr4,%fr5
	fcmp,sgl,? %fr4,%fr5
	fcmp,sgl,!<=> %fr4,%fr5
	fcmp,sgl,= %fr4,%fr5
	fcmp,sgl,=T %fr4,%fr5
	fcmp,sgl,?= %fr4,%fr5
	fcmp,sgl,!<> %fr4,%fr5
fcmp_sgl_tests_2
	fcmp,sgl,!?>= %fr4,%fr5
	fcmp,sgl,< %fr4,%fr5
	fcmp,sgl,?< %fr4,%fr5
	fcmp,sgl,!>= %fr4,%fr5
	fcmp,sgl,!?> %fr4,%fr5
	fcmp,sgl,<= %fr4,%fr5
	fcmp,sgl,?<= %fr4,%fr5
	fcmp,sgl,!> %fr4,%fr5
fcmp_sgl_tests_3
	fcmp,sgl,!?<= %fr4,%fr5
	fcmp,sgl,> %fr4,%fr5
	fcmp,sgl,?> %fr4,%fr5
	fcmp,sgl,!<= %fr4,%fr5
	fcmp,sgl,!?< %fr4,%fr5
	fcmp,sgl,>= %fr4,%fr5
	fcmp,sgl,?>= %fr4,%fr5
	fcmp,sgl,!< %fr4,%fr5
fcmp_sgl_tests_4
	fcmp,sgl,!?= %fr4,%fr5
	fcmp,sgl,<> %fr4,%fr5
	fcmp,sgl,!= %fr4,%fr5
	fcmp,sgl,!=T %fr4,%fr5
	fcmp,sgl,!? %fr4,%fr5
	fcmp,sgl,<=> %fr4,%fr5
	fcmp,sgl,true? %fr4,%fr5
	fcmp,sgl,true %fr4,%fr5

fcmp_dbl_tests_1
	fcmp,dbl,false? %fr4,%fr5
	fcmp,dbl,false %fr4,%fr5
	fcmp,dbl,? %fr4,%fr5
	fcmp,dbl,!<=> %fr4,%fr5
	fcmp,dbl,= %fr4,%fr5
	fcmp,dbl,=T %fr4,%fr5
	fcmp,dbl,?= %fr4,%fr5
	fcmp,dbl,!<> %fr4,%fr5
fcmp_dbl_tests_2
	fcmp,dbl,!?>= %fr4,%fr5
	fcmp,dbl,< %fr4,%fr5
	fcmp,dbl,?< %fr4,%fr5
	fcmp,dbl,!>= %fr4,%fr5
	fcmp,dbl,!?> %fr4,%fr5
	fcmp,dbl,<= %fr4,%fr5
	fcmp,dbl,?<= %fr4,%fr5
	fcmp,dbl,!> %fr4,%fr5
fcmp_dbl_tests_3
	fcmp,dbl,!?<= %fr4,%fr5
	fcmp,dbl,> %fr4,%fr5
	fcmp,dbl,?> %fr4,%fr5
	fcmp,dbl,!<= %fr4,%fr5
	fcmp,dbl,!?< %fr4,%fr5
	fcmp,dbl,>= %fr4,%fr5
	fcmp,dbl,?>= %fr4,%fr5
	fcmp,dbl,!< %fr4,%fr5
fcmp_dbl_tests_4
	fcmp,dbl,!?= %fr4,%fr5
	fcmp,dbl,<> %fr4,%fr5
	fcmp,dbl,!= %fr4,%fr5
	fcmp,dbl,!=T %fr4,%fr5
	fcmp,dbl,!? %fr4,%fr5
	fcmp,dbl,<=> %fr4,%fr5
	fcmp,dbl,true? %fr4,%fr5
	fcmp,dbl,true %fr4,%fr5

fcmp_quad_tests_1
	fcmp,quad,false? %fr4,%fr5
	fcmp,quad,false %fr4,%fr5
	fcmp,quad,? %fr4,%fr5
	fcmp,quad,!<=> %fr4,%fr5
	fcmp,quad,= %fr4,%fr5
	fcmp,quad,=T %fr4,%fr5
	fcmp,quad,?= %fr4,%fr5
	fcmp,quad,!<> %fr4,%fr5
fcmp_quad_tests_2
	fcmp,quad,!?>= %fr4,%fr5
	fcmp,quad,< %fr4,%fr5
	fcmp,quad,?< %fr4,%fr5
	fcmp,quad,!>= %fr4,%fr5
	fcmp,quad,!?> %fr4,%fr5
	fcmp,quad,<= %fr4,%fr5
	fcmp,quad,?<= %fr4,%fr5
	fcmp,quad,!> %fr4,%fr5
fcmp_quad_tests_3
	fcmp,quad,!?<= %fr4,%fr5
	fcmp,quad,> %fr4,%fr5
	fcmp,quad,?> %fr4,%fr5
	fcmp,quad,!<= %fr4,%fr5
	fcmp,quad,!?< %fr4,%fr5
	fcmp,quad,>= %fr4,%fr5
	fcmp,quad,?>= %fr4,%fr5
	fcmp,quad,!< %fr4,%fr5
fcmp_quad_tests_4
	fcmp,quad,!?= %fr4,%fr5
	fcmp,quad,<> %fr4,%fr5
	fcmp,quad,!= %fr4,%fr5
	fcmp,quad,!=T %fr4,%fr5
	fcmp,quad,!? %fr4,%fr5
	fcmp,quad,<=> %fr4,%fr5
	fcmp,quad,true? %fr4,%fr5
	fcmp,quad,true %fr4,%fr5

fmpy_addsub_tests
	fmpyadd,sgl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpyadd,dbl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpysub,sgl %fr16,%fr17,%fr18,%fr19,%fr20
	fmpysub,dbl %fr16,%fr17,%fr18,%fr19,%fr20

xmpyu_tests
	xmpyu %fr4,%fr5,%fr6

special_tests
	gfw %r4(%sr0,%r5)
	gfw,m %r4(%sr0,%r5)
	gfr %r4(%sr0,%r5)
	gfr,m %r4(%sr0,%r5)

sfu_tests
	spop0,4,5
	spop0,4,115
	spop0,4,5,n
	spop0,4,115,n
	spop1,4,5 5
	spop1,4,115 5
	spop1,4,5,n 5
	spop1,4,115,n 5
	spop2,4,5 5
	spop2,4,115 5
	spop2,4,5,n 5
	spop2,4,115,n 5
	spop3,4,5 5,6
	spop3,4,115 5,6
	spop3,4,5,n 5,6
	spop3,4,115,n 5,6

copr_tests
	copr,4,5
	copr,4,115
	copr,4,5,n
	copr,4,115,n

copr_indexing_load 
	cldwx,4 5(0,4),26
	cldwx,4,s 5(0,4),26
	cldwx,4,m 5(0,4),26
	cldwx,4,sm 5(0,4),26
	clddx,4 5(0,4),26
	clddx,4,s 5(0,4),26
	clddx,4,m 5(0,4),26
	clddx,4,sm 5(0,4),26

copr_indexing_store 
	cstwx,4 26,5(0,4)
	cstwx,4,s 26,5(0,4)
	cstwx,4,m 26,5(0,4)
	cstwx,4,sm 26,5(0,4)
	cstdx,4 26,5(0,4)
	cstdx,4,s 26,5(0,4)
	cstdx,4,m 26,5(0,4)
	cstdx,4,sm 26,5(0,4)

copr_short_memory 
	cldws,4 0(0,4),26
	cldws,4,mb 0(0,4),26
	cldws,4,ma 0(0,4),26
	cldds,4 0(0,4),26
	cldds,4,mb 0(0,4),26
	cldds,4,ma 0(0,4),26
	cstws,4 26,0(0,4)
	cstws,4,mb 26,0(0,4)
	cstws,4,ma 26,0(0,4)
	cstds,4 26,0(0,4)
	cstds,4,mb 26,0(0,4)
	cstds,4,ma 26,0(0,4)

fmemLRbug_tests_1
	fstws	%fr6R,0(%r26)
	fstws	%fr6L,4(%r26)
	fstws	%fr6,8(%r26)
	fstds	%fr6R,0(%r26)
	fstds	%fr6L,4(%r26)
	fstds	%fr6,8(%r26)
	fldws	0(%r26),%fr6R
	fldws	4(%r26),%fr6L
	fldws	8(%r26),%fr6
	fldds	0(%r26),%fr6R
	fldds	4(%r26),%fr6L
	fldds	8(%r26),%fr6

fmemLRbug_tests_2
	fstws	%fr6R,0(%sr0,%r26)
	fstws	%fr6L,4(%sr0,%r26)
	fstws	%fr6,8(%sr0,%r26)
	fstds	%fr6R,0(%sr0,%r26)
	fstds	%fr6L,4(%sr0,%r26)
	fstds	%fr6,8(%sr0,%r26)
	fldws	0(%sr0,%r26),%fr6R
	fldws	4(%sr0,%r26),%fr6L
	fldws	8(%sr0,%r26),%fr6
	fldds	0(%sr0,%r26),%fr6R
	fldds	4(%sr0,%r26),%fr6L
	fldds	8(%sr0,%r26),%fr6

fmemLRbug_tests_3
	fstwx	%fr6R,%r25(%r26)
	fstwx	%fr6L,%r25(%r26)
	fstwx	%fr6,%r25(%r26)
	fstdx	%fr6R,%r25(%r26)
	fstdx	%fr6L,%r25(%r26)
	fstdx	%fr6,%r25(%r26)
	fldwx	%r25(%r26),%fr6R
	fldwx	%r25(%r26),%fr6L
	fldwx	%r25(%r26),%fr6
	flddx	%r25(%r26),%fr6R
	flddx	%r25(%r26),%fr6L
	flddx	%r25(%r26),%fr6

fmemLRbug_tests_4
	fstwx	%fr6R,%r25(%sr0,%r26)
	fstwx	%fr6L,%r25(%sr0,%r26)
	fstwx	%fr6,%r25(%sr0,%r26)
	fstdx	%fr6R,%r25(%sr0,%r26)
	fstdx	%fr6L,%r25(%sr0,%r26)
	fstdx	%fr6,%r25(%sr0,%r26)
	fldwx	%r25(%sr0,%r26),%fr6R
	fldwx	%r25(%sr0,%r26),%fr6L
	fldwx	%r25(%sr0,%r26),%fr6
	flddx	%r25(%sr0,%r26),%fr6R
	flddx	%r25(%sr0,%r26),%fr6L
	flddx	%r25(%sr0,%r26),%fr6

	ldw 0(0,%r4),%r26
	ldw 0(0,%r4),%r26
	ldo 64(%r4),%r30
	ldwm -64(0,%r30),%r4
	bv,n 0(%r2)
	.EXIT
	.PROCEND
