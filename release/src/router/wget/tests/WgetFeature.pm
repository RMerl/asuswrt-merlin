package WgetFeature;

use strict;
use warnings;

use WgetTests;

our %skip_messages;
require 'WgetFeature.cfg';

sub import
{
    my ($class, $feature) = @_;

    my $output = `$WgetTest::WGETPATH --version`;
    my ($list) = $output =~ /^([\+\-]\S+(?:\s+[\+\-]\S+)+)/m;
    my %have_features = map {
        my $feature = $_;
           $feature =~ s/^.//;
          ($feature, /^\+/ ? 1 : 0);
    } split /\s+/, $list;

    unless ($have_features{$feature}) {
        print $skip_messages{$feature}, "\n";
        exit 2; # skip
    }
}

1;
