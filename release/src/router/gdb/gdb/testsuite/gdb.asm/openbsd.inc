	comment "openbsd .note"

 	.section	".note.openbsd.ident", "a"
	.p2align	2

	.long	8
	.long	4
	.long	1
	.ascii	"OpenBSD\0\0"
	.long	200311

	.p2align	2
