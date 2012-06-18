#!/usr/bin/perl 
$X= "X" x 32;
$U="[U          ]";
$L="LCT-00000000";
print("#\n# SMB password file.\n#\n");

while ( <> ) {
  next unless (/^[A-Za-z0-9_]/);
  chop;
  @V = split(/:/);
  printf( "%s:%s:%s:%s:%s:%s:%s\n", $V[0], $V[2], $X, $X, $U, $L, $V[4]);
}

