#!/usr/bin/perl -w
# Generate the spokes of a wheel, for wheel factorization.

# Copyright (C) 2001, 2005, 2009-2011 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

eval 'exec /usr/bin/perl -S $0 ${1+"$@"}'
  if 0;

use strict;
(my $ME = $0) =~ s|.*/||;

# A global destructor to close standard output with error checking.
sub END
{
  defined fileno STDOUT
    or return;
  close STDOUT
    and return;
  warn "$ME: closing standard output: $!\n";
  $? ||= 1;
}

sub is_prime ($)
{
  my ($n) = @_;
  use integer;

  $n == 2
    and return 1;

  my $d = 2;
  my $w = 1;
  while (1)
    {
      my $q = $n / $d;
      $n == $q * $d
        and return 0;
      $d += $w;
      $q < $d
        and last;
      $w = 2;
    }
  return 1;
}

{
  @ARGV == 1
    or die "$ME: missing argument\n";

  my $wheel_size = $ARGV[0];

  my @primes = (2);
  my $product = $primes[0];
  my $n_primes = 1;
  for (my $i = 3; ; $i += 2)
    {
      if (is_prime $i)
        {
          push @primes, $i;
          $product *= $i;
          ++$n_primes == $wheel_size
            and last;
        }
    }

  my $ws_m1 = $wheel_size - 1;
  print <<EOF;
/* The first $ws_m1 elements correspond to the incremental offsets of the
   first $wheel_size primes (@primes).  The $wheel_size(th) element is the
   difference between that last prime and the next largest integer
   that is not a multiple of those primes.  The remaining numbers
   define the wheel.  For more information, see
   http://www.utm.edu/research/primes/glossary/WheelFactorization.html.  */
EOF

  my @increments;
  my $prev = 2;
  for (my $i = 3; ; $i += 2)
    {
      my $rel_prime = 1;
      foreach my $divisor (@primes)
        {
          $i != $divisor && $i % $divisor == 0
            and $rel_prime = 0;
        }

      if ($rel_prime)
        {
          #warn $i, ' ', $i - $prev, "\n";
          push @increments, $i - $prev;
          $prev = $i;

          $product + 1 < $i
            and last;
        }
    }

  print join (",\n", @increments), "\n";

  exit 0;
}
