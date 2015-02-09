OUTPUT_FORMAT("elf32-bigmips")
OUTPUT_ARCH("mipssb1")

SECTIONS
{
  .rodata :
  {
	download_start = .;
	*(.data);
	download_end = .;
  } 
}
