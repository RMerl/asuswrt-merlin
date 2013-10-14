include rt2880_app.conf
INSTDIR = $(INSTALLDIR)/sbin/

ra_dir_$(CONFIG_RALINKAPP_REG_RW)      += reg
ra_dir_$(CONFIG_RALINKAPP_ATED)                += ated
ra_dir_$(CONFIG_RALINKAPP_CSR)         += csr_test
ra_dir_$(CONFIG_RALINKAPP_FLASH)       += flash
ra_dir_$(CONFIG_RALINKAPP_HWNAT)       += hw_nat
ra_dir_$(CONFIG_RALINKAPP_MII_MGR)     += mii_mgr
ra_dir_$(CONFIG_RALINKAPP_NVRAM)       += nvram
ra_dir_$(CONFIG_RALINKAPP_GPIO)                += gpio
ra_dir_$(CONFIG_RALINKAPP_SPI)         += spicmd
ra_dir_$(CONFIG_RALINKAPP_I2C)         += i2ccmd
ra_dir_$(CONFIG_RALINKAPP_SCRIPTS)     += scripts
ra_dir_$(CONFIG_RALINKAPP_MEMORY_USAGE)        += mem_usage
ra_dir_$(CONFIG_RALINKAPP_SWITCH)      += switch
ra_dir_$(CONFIG_RALINKAPP_I2S)         += i2scmd
ra_dir_$(CONFIG_RALINKAPP_PCM)         += pcmcmd
ra_dir_$(CONFIG_RALINKAPP_CLS)         += clscmd
ra_dir_$(CONFIG_RALINKAPP_SUPERDMZ)    += super_dmz
ra_dir_$(CONFIG_RALINKAPP_SWQOS)       += sw_qos
#ra_dir_$(CONFIG_RALINKAPP_WAPI)               += wapi

all romfs:

clean:

install:
	install -d $(INSTDIR)
	for i in $(ra_dir_y) ; do install -D prebuild/$$i $(INSTDIR) ; done
	install -D prebuild/asus-raether $(INSTDIR)
