#!/usr/bin/perl

my $expand = 0;

while (<STDIN>) {
  if ($expand) {
    print C;
  } elsif (m,include \<(linux/netfilter_ipv4/ip_set\.h)\>,) {
      $expand = 1;
      open(C, "|gcc -D__KERNEL__ -Iinclude -E - 2>/dev/null| indent -kr -i8") || die "Can't run gcc: $!\n";
      print C;
  } else {
    print;
  }
}
close C;
 