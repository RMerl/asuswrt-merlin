param_default(`SWARM_PCMCIA_FLASH_FILE',		`')
dev(`SWARM PCMCIA Flash')
dev_props(`dnl
/sb1250gen/flash@2
	reg		0x00000000 0x4000000
	cs		6
	type		flash
	erase?		false
	wp?		true
')
ifelse(SWARM_PCMCIA_FLASH_FILE, `', `', `dev_props(`dnl
	file		SWARM_PCMCIA_FLASH_FILE`'
')')

