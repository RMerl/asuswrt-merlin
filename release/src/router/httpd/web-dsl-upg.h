int separate_tc_fw_from_trx();
unsigned long crc32_no_comp(unsigned long crc,const unsigned char* buf, int len);
int check_tc_firmware_crc();
int dsl_check_imagefile_str(char *fname);
int truncate_trx(void);
void do_upgrade_adsldrv();
int compare_linux_image(void);

