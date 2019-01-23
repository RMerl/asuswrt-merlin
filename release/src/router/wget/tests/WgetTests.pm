package WgetTest;

use strict;
use warnings;

our $VERSION = 0.01;

use Carp;
use Cwd;
use English qw(-no_match_vars);
use File::Path;
use IO::Handle;
use POSIX qw(locale_h);
use locale;

our $WGETPATH = '../src/wget -d --no-config';
our $VALGRIND_SUPP_FILE = Cwd::getcwd();
if (defined $ENV{'srcdir'}) {
    $VALGRIND_SUPP_FILE = $VALGRIND_SUPP_FILE
                          . "/" . $ENV{'srcdir'};
}
$VALGRIND_SUPP_FILE = $VALGRIND_SUPP_FILE . '/valgrind-suppressions';

my @unexpected_downloads = ();

{
    my %_attr_data = (    # DEFAULT
                       _cmdline         => q{},
                       _workdir         => Cwd::getcwd(),
                       _errcode         => 0,
                       _existing        => {},
                       _input           => {},
                       _name            => $PROGRAM_NAME,
                       _output          => {},
                       _server_behavior => {},
                     );

    sub _default_for
    {
        my ($self, $attr) = @_;
        return $_attr_data{$attr};
    }

    sub _standard_keys
    {
        return keys %_attr_data;
    }
}

sub new
{
    my ($caller, %args) = @_;
    my $caller_is_obj = ref $caller;
    my $class = $caller_is_obj || $caller;

    #print STDERR "class = ", $class, "\n";
    #print STDERR "_attr_data {workdir} = ", $WgetTest::_attr_data{_workdir}, "\n";
    my $self = bless {}, $class;
    for my $attrname ($self->_standard_keys())
    {

        #print STDERR "attrname = ", $attrname, " value = ";
        my ($argname) = ($attrname =~ m/^_(.*)/msx);
        if (exists $args{$argname})
        {

            #printf STDERR "Setting up $attrname\n";
            $self->{$attrname} = $args{$argname};
        }
        elsif ($caller_is_obj)
        {

            #printf STDERR "Copying $attrname\n";
            $self->{$attrname} = $caller->{$attrname};
        }
        else
        {
            #printf STDERR "Using default for $attrname\n";
            $self->{$attrname} = $self->_default_for($attrname);
        }

        #print STDERR $attrname, '=', $self->{$attrname}, "\n";
    }

    #printf STDERR "_workdir default = ", $self->_default_for("_workdir");
    return $self;
}

sub run
{
    my $self           = shift;
    my $result_message = "Test successful.\n";
    my $errcode;

    $self->{_name} =~ s{.*/}{}msx;          # remove path
    $self->{_name} =~ s{[.][^.]+$}{}msx;    # remove extension
    printf "Running test $self->{_name}\n";

    # Setup
    my $new_result = $self->_setup();
    chdir "$self->{_workdir}/$self->{_name}/input"
      or carp "Could not chdir to input directory: $ERRNO";
    if (defined $new_result)
    {
        $result_message = $new_result;
        $errcode        = 1;
        goto cleanup;
    }

    # Launch server
    my $pid = $self->_fork_and_launch_server();

    # Call wget
    chdir "$self->{_workdir}/$self->{_name}/output"
      or carp "Could not chdir to output directory: $ERRNO";

    my $cmdline = $self->{_cmdline};
    $cmdline = $self->_substitute_port($cmdline);
    $cmdline =
      ($cmdline =~ m{^/.*}msx) ? $cmdline : "$self->{_workdir}/$cmdline";

    my $valgrind = $ENV{VALGRIND_TESTS};
    if (!defined $valgrind)
    {
        $valgrind = 0;
    }

    my $gdb = $ENV{GDB_TESTS};
    if (!defined $gdb)
    {
        $gdb = 0;
    }

    if ($gdb == 1)
    {
        $cmdline = 'gdb --args ' . $cmdline;
    }
    elsif ($valgrind eq "1")
    {
        $cmdline =
          'valgrind --suppressions=' . $VALGRIND_SUPP_FILE
          . ' --error-exitcode=301 --leak-check=yes --track-origins=yes '
          . $cmdline;
    }
    elsif ($valgrind ne q{} && $valgrind ne "0")
    {
        $cmdline = "$valgrind $cmdline";
    }

    print "Calling $cmdline\n";
    $errcode = system $cmdline;
    $errcode >>= 8;    # XXX: should handle abnormal error codes.

    # Shutdown server
    # if we didn't explicitly kill the server, we would have to call
    # waitpid ($pid, 0) here in order to wait for the child process to
    # terminate
    kill 'TERM', $pid;

    # Verify download
    if ($errcode != $self->{_errcode})
    {
        $result_message =
          "Test failed: wrong code returned (was: $errcode, expected: $self->{_errcode})\n";
        goto CLEANUP;
    }
    my $error_str;
    if ($error_str = $self->_verify_download())
    {
        $result_message = $error_str;
    }

  CLEANUP:
    $self->_cleanup();

    print $result_message;
    return $errcode != $self->{_errcode} || ($error_str ? 1 : 0);
}

sub _setup
{
    my $self = shift;

    chdir $self->{_workdir}
      or carp "Could not chdir into $self->{_workdir}: $ERRNO";

    # Create temporary directory
    mkdir $self->{_name} or carp "Could not mkdir '$self->{_name}': $ERRNO";
    chdir $self->{_name}
      or carp "Could not chdir into '$self->{_name}': $ERRNO";
    mkdir 'input'  or carp "Could not mkdir 'input' $ERRNO";
    mkdir 'output' or carp "Could not mkdir 'output': $ERRNO";

    # Setup existing files
    chdir 'output' or carp "Could not chdir into 'output': $ERRNO";
    for my $filename (keys %{$self->{_existing}})
    {
        open my $fh, '>', $filename
          or return "Test failed: cannot open pre-existing file $filename\n";

        my $file = $self->{_existing}->{$filename};
        print {$fh} $file->{content}
          or return "Test failed: cannot write pre-existing file $filename\n";

        close $fh or carp $ERRNO;

        if (exists($file->{timestamp}))
        {
            utime $file->{timestamp}, $file->{timestamp}, $filename
              or return
              "Test failed: cannot set timestamp on pre-existing file $filename\n";
        }
    }

    chdir '../input' or carp "Cannot chdir into '../input': $ERRNO";
    $self->_setup_server();

    chdir $self->{_workdir}
      or carp "Cannot chdir into '$self->{_workdir}': $ERRNO";
    return;
}

sub _cleanup
{
    my $self = shift;

    chdir $self->{_workdir}
      or carp "Could not chdir into '$self->{_workdir}': $ERRNO";
    if (!$ENV{WGET_TEST_NO_CLEANUP})
    {
        File::Path::rmtree($self->{_name});
    }
    return 1;
}

# not a method
sub quotechar
{
    my $c = ord shift;
    if ($c >= 0x7 && $c <= 0xD)
    {
        return q{\\} . qw(a b t n v f r) [$c - 0x7];
    }
    else
    {
        return sprintf '\\x%02x', $c;
    }
}

# not a method
sub _show_diff
{
    my ($expected, $actual) = @_;
    my $SNIPPET_SIZE = 10;

    my $str    = q{};
    my $explen = length $expected;
    my $actlen = length $actual;

    if ($explen != $actlen)
    {
        $str .= "Sizes don't match: expected = $explen, actual = $actlen\n";
    }

    my $min  = $explen <= $actlen ? $explen : $actlen;
    my $line = 1;
    my $col  = 1;
    my $i    = 0;

    while ( $i < $min )
    {
        last if substr($expected, $i, 1) ne substr $actual, $i, 1;
        if (substr($expected, $i, 1) eq "\n")
        {
            $line++;
            $col = 0;
        }
        else
        {
            $col++;
        }
        $i++;
    }
    my $snip_start = $i - ($SNIPPET_SIZE / 2);
    if ($snip_start < 0)
    {
        $SNIPPET_SIZE += $snip_start;    # Take it from the end.
        $snip_start = 0;
    }
    my $exp_snip = substr $expected, $snip_start, $SNIPPET_SIZE;
    my $act_snip = substr $actual,   $snip_start, $SNIPPET_SIZE;
    $exp_snip =~ s/[^[:print:]]/ quotechar($&) /gemsx;
    $act_snip =~ s/[^[:print:]]/ quotechar($&) /gemsx;
    $str .= "Mismatch at line $line, col $col:\n";
    $str .= "    $exp_snip\n";
    $str .= "    $act_snip\n";

    return $str;
}

sub _verify_download
{
    my $self = shift;

    chdir "$self->{_workdir}/$self->{_name}/output"
      or carp "Could not chdir into output directory: $ERRNO";

    # use slurp mode to read file content
    my $old_input_record_separator = $INPUT_RECORD_SEPARATOR;
    local $INPUT_RECORD_SEPARATOR = undef;

    while (my ($filename, $filedata) = each %{$self->{_output}})
    {
        open my $fh, '<', $filename
          or return "Test failed: file $filename not downloaded\n";

        my $content = <$fh>;

        close $fh or carp $ERRNO;

        my $expected_content = $filedata->{'content'};
        $expected_content = $self->_substitute_port($expected_content);
        if ($content ne $expected_content)
        {
            return "Test failed: wrong content for file $filename\n"
              . _show_diff($expected_content, $content);
        }

        if (exists($filedata->{'timestamp'}))
        {
            my (
                $dev,   $ino,     $mode, $nlink, $uid,
                $gid,   $rdev,    $size, $atime, $mtime,
                $ctime, $blksize, $blocks
               )
              = stat $filename;

            $mtime == $filedata->{'timestamp'}
              or return "Test failed: wrong timestamp for file $filename: expected = $filedata->{'timestamp'}, actual = $mtime\n";
        }

    }

    local $INPUT_RECORD_SEPARATOR = $old_input_record_separator;

    # make sure no unexpected files were downloaded
    chdir "$self->{_workdir}/$self->{_name}/output"
      or carp "Could not change into output directory: $ERRNO";

    __dir_walk(
        q{.},
        sub {
            if (!(exists $self->{_output}{$_[0]} || $self->{_existing}{$_[0]}))
            {
                push @unexpected_downloads, $_[0];
            }
        },
        sub { shift; return @_ }
              );
    if (@unexpected_downloads)
    {
        return 'Test failed: unexpected downloaded files [' .
          (join ', ', @unexpected_downloads) . "]\n";

    }

    return q{};
}

sub __dir_walk
{
    my ($top, $filefunc, $dirfunc) = @_;

    my $DIR;

    if (-d $top)
    {
        my $file;
        if (!opendir $DIR, $top)
        {
            warn "Couldn't open directory $DIR: $ERRNO; skipping.\n";
            return;
        }

        my @results;
        while ($file = readdir $DIR)
        {
            next if $file eq q{.} || $file eq q{..};
            my $nextdir = $top eq q{.} ? $file : "$top/$file";
            push @results, __dir_walk($nextdir, $filefunc, $dirfunc);
        }

        return $dirfunc ? $dirfunc->($top, @results) : ();
    }
    else
    {
        return $filefunc ? $filefunc->($top) : ();
    }
}

sub _fork_and_launch_server
{
    my $self = shift;

    pipe FROM_CHILD, TO_PARENT or croak 'Cannot create pipe!';
    TO_PARENT->autoflush();

    my $pid = fork;
    if ($pid < 0)
    {
        carp 'Cannot fork';
    }
    elsif ($pid == 0)
    {

        # child
        close FROM_CHILD or carp $ERRNO;

        # FTP Server has to start with english locale due to use of strftime month names in LIST command
        setlocale(LC_ALL, 'C');
        $self->_launch_server(
            sub {
                print {*TO_PARENT} "SYNC\n";
                close TO_PARENT or carp $ERRNO;
            }
        );
    }
    else
    {
        # father
        close TO_PARENT or carp $ERRNO;
        chomp(my $line = <FROM_CHILD>);
        close FROM_CHILD or carp $ERRNO;
    }

    return $pid;
}

1;

# vim: et ts=4 sw=4
