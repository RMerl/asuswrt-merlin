#!/usr/bin/perl -w
while ( <> ) {
  print;
  @V = split(/:/);
  $_ = $V[3];
  if ( $V[0] !~ /^\#/ && !(/^[0-9A-F]{32}$/ || /^X{32}$/ || /^\*{32}$/) ) {
    $V[3] = "X" x 32;
  }
  print( join( ':', @V));
}
