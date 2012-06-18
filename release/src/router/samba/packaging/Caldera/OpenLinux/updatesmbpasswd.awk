#!/usr/bin/gawk -f
BEGIN {OFS=FS=":"} 
{
	if( $0 ~ "^#" ) {
		print $0
	} else if( (length($4) == 32) && (($4 ~ "^[0-9A-F]*$") || ($4 ~ "^[X]*$") || ( $4 ~ "^[*]*$"))) {
		print $0
	} else {
		$4 = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
		print $0
	}
}
