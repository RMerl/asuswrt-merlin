#!/usr/bin/perl
#
#  Reformat 'snmpcmd -d' style raw dump output
#    into something a little easier to understand.
#


sub parse_dump {
    #
    #  Basic formatting technique:
    #     Display the contents of each nested SEQUENCE
    #        indented from the enclosing level.
    #     Individual data fields are all on one line
    #
    my @data    = @_;
    my $indent  = shift( @data );
    my $datalen = shift( @data );

    while ( $datalen > 0 ) {
        my ($tag, $tlen, $tmp);
        my ($tag1, $tag2 );
        $tag  = shift( @data );
        $tmp  = shift( @data );
        $tlen = hex($tmp);
        #
        # Handle 2-octet lengths
        if ( $tlen >= 128 ) {
            $tlen -= 128;
            $tmp   = shift( @data );
            $tlen += hex($tmp);
        }
        $datalen -= ($tlen + 2 );

        $tag1 = substr($tag, 0, 1);
        $tag2 = substr($tag, 1, 1);
    
        #
        # Sequence-based tags - display and indent
        #
        if ( $tag1 eq 3 ) {
            print " "x$indent, "$tag $tmp\n";
            parse_dump( $indent+3, $tlen, @data );
        }
        elsif ( $tag1 eq "A" ) {
            print " "x$indent, "$tag $tmp\n";
            parse_dump( $indent+3, $tlen, @data );
        }

        #
        # Leaf-data tags - just display
        #
        else {
            $val = "";
            while ( $tlen > 0 ) {
               $val .= " ";
               $val .= shift( @data );
               $tlen--;
            }
            if ( $tag1 eq "0" ) {     # leaf data
                print " "x$indent, "$tag $tmp$val\n";
            }
            elsif ( $tag1 eq "8" ) {  # exceptions
                print " "x$indent, "$tag $tmp$val\n";
            }
            else {                    # unknown
                print " "x$indent, "$tag $tmp$val\n";
            }
        }
    }
}

$inpacket=0;
$rawdump="";

while (<>) {
    if ( $inpacket ) {
        #
        # Strip off the extraneous junk, and join
        #   the raw dump output into a single line
        #
        if ( /^[0-9]*: / ) {
            chomp;
            s/^[0-9]*: //;
            s/   .*$//;
            s/  / /g;
            $rawdump = "$rawdump $_";
        } else {
            #
            # Once this line is complete, display the
            #   dump in a vaguely sensible layout
            #
            @rawdata = split( " ", $rawdump );
            parse_dump( 3, $#rawdata, @rawdata );
            $inpacket=0;
            $rawdump="";
        }
    } else {
        #
        # Pass everything else through untouched
        #
        print;
        if ( /^Sending / || /^Received / ) {
            $inpacket=1;
            $rawdump="";
        }
    }
}
