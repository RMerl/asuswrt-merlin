#!/usr/bin/perl -w
#
# Convert a Samba 1.9.18 smbpasswd file format into
# a Samba 2.0 smbpasswd file format.
# Read from stdin and write to stdout for simplicity.
# Set the last change time to the time of conversion.
while ( <> ) {
  @V = split(/:/);
  if ( ! /^\#/ ) {
    $V[6] = $V[4] . "\n";
    $V[5] = sprintf( "LCT-%X", time());
    $V[4] = "[U          ]";
  }
  print( join( ':', @V));
}
