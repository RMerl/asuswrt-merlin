package WgetTest;
$VERSION = 0.01;

use strict;
use warnings;

use Cwd;
use File::Path;
use POSIX qw(locale_h);
use locale;

our $WGETPATH = "../src/wget";

my @unexpected_downloads = ();

{
    my %_attr_data = ( # DEFAULT
        _cmdline      => "",
        _workdir      => Cwd::getcwd(),
        _errcode      => 0,
        _existing     => {},
        _input        => {},
        _name         => $0,
        _output       => {},
        _server_behavior => {},
    );

    sub _default_for
    {
        my ($self, $attr) = @_;
        $_attr_data{$attr};
    }

    sub _standard_keys
    {
        keys %_attr_data;
    }
}


sub new {
    my ($caller, %args) = @_;
    my $caller_is_obj = ref($caller);
    my $class = $caller_is_obj || $caller;
    #print STDERR "class = ", $class, "\n";
    #print STDERR "_attr_data {workdir} = ", $WgetTest::_attr_data{_workdir}, "\n";
    my $self = bless {}, $class;
    foreach my $attrname ($self->_standard_keys()) {
        #print STDERR "attrname = ", $attrname, " value = ";
        my ($argname) = ($attrname =~ /^_(.*)/);
        if (exists $args{$argname}) {
            #printf STDERR "Setting up $attrname\n";
            $self->{$attrname} = $args{$argname};
        } elsif ($caller_is_obj) {
            #printf STDERR "Copying $attrname\n";
            $self->{$attrname} = $caller->{$attrname};
        } else {
            #printf STDERR "Using default for $attrname\n";
            $self->{$attrname} = $self->_default_for($attrname);
        }
        #print STDERR $attrname, '=', $self->{$attrname}, "\n";
    }
    #printf STDERR "_workdir default = ", $self->_default_for("_workdir");
    return $self;
}


sub run {
    my $self = shift;
    my $result_message = "Test successful.\n";
    my $errcode;

    $self->{_name} =~ s{.*/}{}; # remove path
    $self->{_name} =~ s{\.[^.]+$}{}; # remove extension
    printf "Running test $self->{_name}\n";

    # Setup
    my $new_result = $self->_setup();
    chdir ("$self->{_workdir}/$self->{_name}/input");
    if (defined $new_result) {
        $result_message = $new_result;
        $errcode = 1;
        goto cleanup;
    }

    # Launch server
    my $pid = $self->_fork_and_launch_server();

    # Call wget
    chdir ("$self->{_workdir}/$self->{_name}/output");
    my $cmdline = $self->{_cmdline};
    $cmdline = $self->_substitute_port($cmdline);
    print "Calling $cmdline\n";
    $errcode =
        ($cmdline =~ m{^/.*})
            ? system ($cmdline)
            : system ("$self->{_workdir}/$cmdline");
    $errcode >>= 8; # XXX: should handle abnormal error codes.

    # Shutdown server
    # if we didn't explicitely kill the server, we would have to call
    # waitpid ($pid, 0) here in order to wait for the child process to
    # terminate
    kill ('TERM', $pid);

    # Verify download
    unless ($errcode == $self->{_errcode}) {
        $result_message = "Test failed: wrong code returned (was: $errcode, expected: $self->{_errcode})\n";
        goto cleanup;
    }
    my $error_str;
    if ($error_str = $self->_verify_download()) {
        $result_message = $error_str;
    }

  cleanup:
    $self->_cleanup();

    print $result_message;
    return $errcode != $self->{_errcode} || ($error_str ? 1 : 0);
}


sub _setup {
    my $self = shift;

    #print $self->{_name}, "\n";
    chdir ($self->{_workdir});

    # Create temporary directory
    mkdir ($self->{_name});
    chdir ($self->{_name});
    mkdir ("input");
    mkdir ("output");

    # Setup existing files
    chdir ("output");
    foreach my $filename (keys %{$self->{_existing}}) {
        open (FILE, ">$filename")
            or return "Test failed: cannot open pre-existing file $filename\n";

        my $file = $self->{_existing}->{$filename};
        print FILE $file->{content}
            or return "Test failed: cannot write pre-existing file $filename\n";

        close (FILE);

        if (exists($file->{timestamp})) {
            utime $file->{timestamp}, $file->{timestamp}, $filename
                or return "Test failed: cannot set timestamp on pre-existing file $filename\n";
        }
    }

    chdir ("../input");
    $self->_setup_server();

    chdir ($self->{_workdir});
    return;
}


sub _cleanup {
    my $self = shift;

    chdir ($self->{_workdir});
    File::Path::rmtree ($self->{_name}) unless $ENV{WGET_TEST_NO_CLEANUP};
}

# not a method
sub quotechar {
    my $c = ord( shift );
    if ($c >= 0x7 && $c <= 0xD) {
       return '\\' . qw(a b t n v f r)[$c - 0x7];
    } else {
        return sprintf('\\x%02x', $c);
    }
}

# not a method
sub _show_diff {
    my $SNIPPET_SIZE = 10;

    my ($expected, $actual) = @_;

    my $str = '';
    my $explen = length $expected;
    my $actlen = length $actual;

    if ($explen != $actlen) {
        $str .= "Sizes don't match: expected = $explen, actual = $actlen\n";
    }

    my $min = $explen <= $actlen? $explen : $actlen;
    my $line = 1;
    my $col = 1;
    my $i;
    for ($i=0; $i != $min; ++$i) {
        last if substr($expected, $i, 1) ne substr($actual, $i, 1);
        if (substr($expected, $i, 1) eq '\n') {
            $line++;
            $col = 0;
        } else {
            $col++;
        }
    }
    my $snip_start = $i - ($SNIPPET_SIZE / 2);
    if ($snip_start < 0) {
        $SNIPPET_SIZE += $snip_start; # Take it from the end.
        $snip_start = 0;
    }
    my $exp_snip = substr($expected, $snip_start, $SNIPPET_SIZE);
    my $act_snip = substr($actual, $snip_start, $SNIPPET_SIZE);
    $exp_snip =~s/[^[:print:]]/ quotechar($&) /ge;
    $act_snip =~s/[^[:print:]]/ quotechar($&) /ge;
    $str .= "Mismatch at line $line, col $col:\n";
    $str .= "    $exp_snip\n";
    $str .= "    $act_snip\n";

    return $str;
}

sub _verify_download {
    my $self = shift;

    chdir ("$self->{_workdir}/$self->{_name}/output");

    # use slurp mode to read file content
    my $old_input_record_separator = $/;
    undef $/;

    while (my ($filename, $filedata) = each %{$self->{_output}}) {
        open (FILE, $filename)
            or return "Test failed: file $filename not downloaded\n";

        my $content = <FILE>;
        my $expected_content = $filedata->{'content'};
        $expected_content = $self->_substitute_port($expected_content);
        unless ($content eq $expected_content) {
            return "Test failed: wrong content for file $filename\n"
                . _show_diff($expected_content, $content);
        }

        if (exists($filedata->{'timestamp'})) {
            my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
                $atime, $mtime, $ctime, $blksize, $blocks) = stat FILE;

            $mtime == $filedata->{'timestamp'}
                or return "Test failed: wrong timestamp for file $filename\n";
        }

        close (FILE);
    }

    $/ = $old_input_record_separator;

    # make sure no unexpected files were downloaded
    chdir ("$self->{_workdir}/$self->{_name}/output");

    __dir_walk('.',
               sub { push @unexpected_downloads,
                          $_[0] unless (exists $self->{_output}{$_[0]} || $self->{_existing}{$_[0]}) },
               sub { shift; return @_ } );
    if (@unexpected_downloads) {
        return "Test failed: unexpected downloaded files [" . join(', ', @unexpected_downloads) . "]\n";
    }

    return "";
}


sub __dir_walk {
    my ($top, $filefunc, $dirfunc) = @_;

    my $DIR;

    if (-d $top) {
        my $file;
        unless (opendir $DIR, $top) {
            warn "Couldn't open directory $DIR: $!; skipping.\n";
            return;
        }

        my @results;
        while ($file = readdir $DIR) {
            next if $file eq '.' || $file eq '..';
            my $nextdir = $top eq '.' ? $file : "$top/$file";
            push @results, __dir_walk($nextdir, $filefunc, $dirfunc);
        }

        return $dirfunc ? $dirfunc->($top, @results) : () ;
    } else {
        return $filefunc ? $filefunc->($top) : () ;
    }
}


sub _fork_and_launch_server
{
    my $self = shift;

    pipe(FROM_CHILD, TO_PARENT) or die "Cannot create pipe!";
    select((select(TO_PARENT), $| = 1)[0]);

    my $pid = fork();
    if ($pid < 0) {
        die "Cannot fork";
    } elsif ($pid == 0) {
        # child
        close FROM_CHILD;
        # FTP Server has to start with english locale due to use of strftime month names in LIST command
        setlocale(LC_ALL,"C");
        $self->_launch_server(sub { print TO_PARENT "SYNC\n"; close TO_PARENT });
    } else {
        # father
        close TO_PARENT;
        chomp(my $line = <FROM_CHILD>);
        close FROM_CHILD;
    }

    return $pid;
}

1;

# vim: et ts=4 sw=4
