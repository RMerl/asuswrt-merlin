package WgetFeature;

use strict;
use warnings;

our $VERSION = 0.01;

use English qw(-no_match_vars);
use WgetTests;

sub import
{
    my ($class, @required_feature) = @_;

    # create a list of available features from 'wget --version' output
    my $output = `$WgetTest::WGETPATH --version`;
    my ($list) = $output =~ m/^([+-]\S+(?:\s+[+-]\S+)+)/msx;
    my %have_features;
    for my $f (split m/\s+/msx, $list)
    {
        my $feat = $f;
        $feat =~ s/^.//msx;
        $have_features{$feat} = $f =~ m/^[+]/msx ? 1 : 0;
    }

    foreach (@required_feature)
    {
        if (!$have_features{$_})
        {
            print "Skipped test: Wget misses feature '$_'\n";
            print "Features available from 'wget --version' output:\n";
            foreach (keys %have_features)
            {
                print "    $_=$have_features{$_}\n";
            }
            exit 77; # skip
        }
    }
}

1;
