	comment "netbsd .note"

.section	".note.netbsd.ident", "a"
	.p2align	2

	.long	7
	.long	4
	.long	1
	.ascii	"NetBSD\0\0"
	.long	105010000

	.p2align	2
