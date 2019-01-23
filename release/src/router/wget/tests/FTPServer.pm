# Part of this code was borrowed from Richard Jones's Net::FTPServer
# http://www.annexia.org/freeware/netftpserver

package FTPServer;

use strict;
use warnings;

use Cwd;
use Socket;
use IO::Socket::INET;
use IO::Seekable;
use POSIX qw(strftime);

my $log        = undef;
my $GOT_SIGURG = 0;

# CONSTANTS

# connection states
my %_connection_states = (
                          'NEWCONN'  => 0x01,
                          'WAIT4PWD' => 0x02,
                          'LOGGEDIN' => 0x04,
                          'TWOSOCKS' => 0x08,
                         );

# subset of FTP commands supported by these server and the respective
# connection states in which they are allowed
my %_commands = (

    # Standard commands from RFC 959.
    'CWD' => $_connection_states{LOGGEDIN} | $_connection_states{TWOSOCKS},

    #   'EPRT' => $_connection_states{LOGGEDIN},
    #   'EPSV' => $_connection_states{LOGGEDIN},
    'LIST' => $_connection_states{TWOSOCKS},

    #   'LPRT' => $_connection_states{LOGGEDIN},
    #   'LPSV' => $_connection_states{LOGGEDIN},
    'PASS' => $_connection_states{WAIT4PWD},
    'PASV' => $_connection_states{LOGGEDIN},
    'PORT' => $_connection_states{LOGGEDIN},
    'PWD'  => $_connection_states{LOGGEDIN} | $_connection_states{TWOSOCKS},
    'QUIT' => $_connection_states{LOGGEDIN} | $_connection_states{TWOSOCKS},
    'REST' => $_connection_states{TWOSOCKS},
    'RETR' => $_connection_states{TWOSOCKS},
    'SYST' => $_connection_states{LOGGEDIN},
    'TYPE' => $_connection_states{LOGGEDIN} | $_connection_states{TWOSOCKS},
    'USER' => $_connection_states{NEWCONN},

    # From ftpexts Internet Draft.
    'SIZE' => $_connection_states{LOGGEDIN} | $_connection_states{TWOSOCKS},
);

# COMMAND-HANDLING ROUTINES

sub _CWD_command
{
    my ($conn, $cmd, $path) = @_;
    my $paths = $conn->{'paths'};

    local $_;
    my $new_path = FTPPaths::path_merge($conn->{'dir'}, $path);

    # Split the path into its component parts and process each separately.
    if (!$paths->dir_exists($new_path))
    {
        print {$conn->{socket}} "550 Directory not found.\r\n";
        return;
    }

    $conn->{'dir'} = $new_path;
    print {$conn->{socket}} "200 directory changed to $new_path.\r\n";
}

sub _LIST_command
{
    my ($conn, $cmd, $path) = @_;
    my $paths = $conn->{'paths'};

    my $ReturnEmptyList =
      ($paths->GetBehavior('list_empty_if_list_a') && $path eq '-a');
    my $SkipHiddenFiles =
      ($paths->GetBehavior('list_no_hidden_if_list') && (!$path));

    if ($paths->GetBehavior('list_fails_if_list_a') && $path eq '-a')
    {
        print {$conn->{socket}} "500 Unknown command\r\n";
        return;
    }

    if (!$paths->GetBehavior('list_dont_clean_path'))
    {
        # This is something of a hack. Some clients expect a Unix server
        # to respond to flags on the 'ls command line'. Remove these flags
        # and ignore them. This is particularly an issue with ncftp 2.4.3.
        $path =~ s/^-[a-zA-Z0-9]+\s?//;
    }

    my $dir = $conn->{'dir'};

    print STDERR "_LIST_command - dir is: $dir\n";

    # Parse the first elements of the path until we find the appropriate
    # working directory.
    local $_;

    my $listing;
    if (!$ReturnEmptyList)
    {
        $dir = FTPPaths::path_merge($dir, $path);
        $listing = $paths->get_list($dir, $SkipHiddenFiles);
        unless ($listing)
        {
            print {$conn->{socket}} "550 File or directory not found.\r\n";
            return;
        }
    }

    print STDERR "_LIST_command - dir is: $dir\n" if $log;

    print {$conn->{socket}} "150 Opening data connection for file listing.\r\n";

    # Open a path back to the client.
    my $sock = __open_data_connection($conn);
    unless ($sock)
    {
        print {$conn->{socket}} "425 Can't open data connection.\r\n";
        return;
    }

    if (!$ReturnEmptyList)
    {
        for my $item (@$listing)
        {
            print $sock "$item\r\n";
        }
    }

    unless ($sock->close)
    {
        print {$conn->{socket}} "550 Error closing data connection: $!\r\n";
        return;
    }

    print {$conn->{socket}}
      "226 Listing complete. Data connection has been closed.\r\n";
}

sub _PASS_command
{
    my ($conn, $cmd, $pass) = @_;

    # TODO: implement authentication?

    print STDERR "switching to LOGGEDIN state\n" if $log;
    $conn->{state} = $_connection_states{LOGGEDIN};

    if ($conn->{username} eq "anonymous")
    {
        print {$conn->{socket}}
          "202 Anonymous user access is always granted.\r\n";
    }
    else
    {
        print {$conn->{socket}}
          "230 Authentication not implemented yet, access is always granted.\r\n";
    }
}

sub _PASV_command
{
    my ($conn, $cmd, $rest) = @_;

    # Open a listening socket - but don't actually accept on it yet.
    "0" =~ /(0)/;    # Perl 5.7 / IO::Socket::INET bug workaround.
    my $sock = IO::Socket::INET->new(
                                     LocalHost => '127.0.0.1',
                                     LocalPort => '0',
                                     Listen    => 1,
                                     Reuse     => 1,
                                     Proto     => 'tcp',
                                     Type      => SOCK_STREAM
                                    );

    unless ($sock)
    {
        # Return a code 550 here, even though this is not in the RFC. XXX
        print {$conn->{socket}} "550 Can't open a listening socket.\r\n";
        return;
    }

    $conn->{passive}        = 1;
    $conn->{passive_socket} = $sock;

    # Get our port number.
    my $sockport = $sock->sockport;

    # Split the port number into high and low components.
    my $p1 = int($sockport / 256);
    my $p2 = $sockport % 256;

    $conn->{state} = $_connection_states{TWOSOCKS};

    # We only accept connections from localhost.
    print {$conn->{socket}} "227 Entering Passive Mode (127,0,0,1,$p1,$p2)\r\n";
}

sub _PORT_command
{
    my ($conn, $cmd, $rest) = @_;

    # The arguments to PORT are a1,a2,a3,a4,p1,p2 where a1 is the
    # most significant part of the address (eg. 127,0,0,1) and
    # p1 is the most significant part of the port.
    unless ($rest =~
        /^\s*(\d{1,3}),\s*(\d{1,3}),\s*(\d{1,3}),\s*(\d{1,3}),\s*(\d{1,3}),\s*(\d{1,3})/
      )
    {
        print {$conn->{socket}} "501 Syntax error in PORT command.\r\n";
        return;
    }

    # Check host address.
    unless (   $1 > 0
            && $1 < 224
            && $2 >= 0
            && $2 < 256
            && $3 >= 0
            && $3 < 256
            && $4 >= 0
            && $4 < 256)
    {
        print {$conn->{socket}} "501 Invalid host address.\r\n";
        return;
    }

    # Construct host address and port number.
    my $peeraddrstring = "$1.$2.$3.$4";
    my $peerport       = $5 * 256 + $6;

    # Check port number.
    unless ($peerport > 0 && $peerport < 65536)
    {
        print {$conn->{socket}} "501 Invalid port number.\r\n";
    }

    $conn->{peeraddrstring} = $peeraddrstring;
    $conn->{peeraddr}       = inet_aton($peeraddrstring);
    $conn->{peerport}       = $peerport;
    $conn->{passive}        = 0;

    $conn->{state} = $_connection_states{TWOSOCKS};

    print {$conn->{socket}} "200 PORT command OK.\r\n";
}

sub _PWD_command
{
    my ($conn, $cmd, $rest) = @_;

    # See RFC 959 Appendix II and draft-ietf-ftpext-mlst-11.txt section 6.2.1.
    my $pathname = $conn->{dir};
    $pathname =~ s,/+$,, unless $pathname eq "/";
    $pathname =~ tr,/,/,s;

    print {$conn->{socket}} "257 \"$pathname\"\r\n";
}

sub _REST_command
{
    my ($conn, $cmd, $restart_from) = @_;

    unless ($restart_from =~ /^([1-9][0-9]*|0)$/)
    {
        print {$conn->{socket}}
          "501 REST command needs a numeric argument.\r\n";
        return;
    }

    $conn->{restart} = $1;

    print {$conn->{socket}} "350 Restarting next transfer at $1.\r\n";
}

sub _RETR_command
{
    my ($conn, $cmd, $path) = @_;

    $path = FTPPaths::path_merge($conn->{dir}, $path);
    my $info = $conn->{'paths'}->get_info($path);

    unless ($info->{'_type'} eq 'f')
    {
        print {$conn->{socket}} "550 File not found.\r\n";
        return;
    }

    print {$conn->{socket}} "150 Opening "
      . ($conn->{type} eq 'A' ? "ASCII mode" : "BINARY mode")
      . " data connection.\r\n";

    # Open a path back to the client.
    my $sock = __open_data_connection($conn);

    unless ($sock)
    {
        print {$conn->{socket}} "425 Can't open data connection.\r\n";
        return;
    }

    my $content = $info->{'content'};

    # Restart the connection from previous point?
    if ($conn->{restart})
    {
        $content = substr($content, $conn->{restart});
        $conn->{restart} = 0;
    }

    # What mode are we sending this file in?
    unless ($conn->{type} eq 'A')    # Binary type.
    {
        my ($r, $buffer, $n, $w, $sent);

        # Copy data.
        $sent = 0;
        while ($sent < length($content))
        {
            $buffer = substr($content, $sent, 65536);
            $r = length $buffer;

            # Restart alarm clock timer.
            alarm $conn->{idle_timeout};

            for ($n = 0 ; $n < $r ;)
            {
                $w = syswrite($sock, $buffer, $r - $n, $n);

                # Cleanup and exit if there was an error.
                unless (defined $w)
                {
                    close $sock;
                    print {$conn->{socket}}
                      "426 File retrieval error: $!. Data connection has been closed.\r\n";
                    return;
                }

                $n += $w;
            }

            # Transfer aborted by client?
            if ($GOT_SIGURG)
            {
                $GOT_SIGURG = 0;
                close $sock;
                print {$conn->{socket}}
                  "426 Transfer aborted. Data connection closed.\r\n";
                return;
            }
            $sent += $r;
        }

        # Cleanup and exit if there was an error.
        unless (defined $r)
        {
            close $sock;
            print {$conn->{socket}}
              "426 File retrieval error: $!. Data connection has been closed.\r\n";
            return;
        }
    }
    else
    {    # ASCII type.
            # Copy data.
        my @lines = split /\r\n?|\n/, $content;
        for (@lines)
        {
            # Remove any native line endings.
            s/[\n\r]+$//;

            # Restart alarm clock timer.
            alarm $conn->{idle_timeout};

            # Write the line with telnet-format line endings.
            print $sock "$_\r\n";

            # Transfer aborted by client?
            if ($GOT_SIGURG)
            {
                $GOT_SIGURG = 0;
                close $sock;
                print {$conn->{socket}}
                  "426 Transfer aborted. Data connection closed.\r\n";
                return;
            }
        }
    }

    unless (close($sock))
    {
        print {$conn->{socket}} "550 File retrieval error: $!.\r\n";
        return;
    }

    print {$conn->{socket}}
      "226 File retrieval complete. Data connection has been closed.\r\n";
}

sub _SIZE_command
{
    my ($conn, $cmd, $path) = @_;

    $path = FTPPaths::path_merge($conn->{dir}, $path);
    my $info = $conn->{'paths'}->get_info($path);
    unless ($info)
    {
        print {$conn->{socket}} "550 File or directory not found.\r\n";
        return;
    }

    if ($info->{'_type'} eq 'd')
    {
        print {$conn->{socket}}
          "550 SIZE command is not supported on directories.\r\n";
        return;
    }

    my $size = length $info->{'content'};

    print {$conn->{socket}} "213 $size\r\n";
}

sub _SYST_command
{
    my ($conn, $cmd, $dummy) = @_;

    if ($conn->{'paths'}->GetBehavior('syst_response'))
    {
        print {$conn->{socket}} $conn->{'paths'}->GetBehavior('syst_response')
          . "\r\n";
    }
    else
    {
        print {$conn->{socket}} "215 UNIX Type: L8\r\n";
    }
}

sub _TYPE_command
{
    my ($conn, $cmd, $type) = @_;

    # See RFC 959 section 5.3.2.
    if ($type =~ /^([AI])$/i)
    {
        $conn->{type} = $1;
    }
    elsif ($type =~ /^([AI])\sN$/i)
    {
        $conn->{type} = $1;
    }
    elsif ($type =~ /^L\s8$/i)
    {
        $conn->{type} = 'L8';
    }
    else
    {
        print {$conn->{socket}}
          "504 This server does not support TYPE $type.\r\n";
        return;
    }

    print {$conn->{socket}} "200 TYPE changed to $type.\r\n";
}

sub _USER_command
{
    my ($conn, $cmd, $username) = @_;

    print STDERR "username: $username\n" if $log;
    $conn->{username} = $username;

    print STDERR "switching to WAIT4PWD state\n" if $log;
    $conn->{state} = $_connection_states{WAIT4PWD};

    if ($conn->{username} eq "anonymous")
    {
        print {$conn->{socket}} "230 Anonymous user access granted.\r\n";
    }
    else
    {
        print {$conn->{socket}} "331 Password required.\r\n";
    }
}

# HELPER ROUTINES

sub __open_data_connection
{
    my $conn = shift;

    my $sock;

    if ($conn->{passive})
    {
        # Passive mode - wait for a connection from the client.
        accept($sock, $conn->{passive_socket}) or return undef;
    }
    else
    {
        # Active mode - connect back to the client.
        "0" =~ /(0)/;    # Perl 5.7 / IO::Socket::INET bug workaround.
        $sock = IO::Socket::INET->new(
                                      LocalAddr => '127.0.0.1',
                                      PeerAddr  => $conn->{peeraddrstring},
                                      PeerPort  => $conn->{peerport},
                                      Proto     => 'tcp',
                                      Type      => SOCK_STREAM
                                     )
          or return undef;
    }

    return $sock;
}

###########################################################################
# FTPSERVER CLASS
###########################################################################

{
    my %_attr_data = (    # DEFAULT
                       _input           => undef,
                       _localAddr       => 'localhost',
                       _localPort       => undef,
                       _reuseAddr       => 1,
                       _rootDir         => Cwd::getcwd(),
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

sub new
{
    my ($caller, %args) = @_;
    my $caller_is_obj = ref($caller);
    my $class         = $caller_is_obj || $caller;
    my $self          = bless {}, $class;
    foreach my $attrname ($self->_standard_keys())
    {
        my ($argname) = ($attrname =~ /^_(.*)/);
        if (exists $args{$argname})
        {
            $self->{$attrname} = $args{$argname};
        }
        elsif ($caller_is_obj)
        {
            $self->{$attrname} = $caller->{$attrname};
        }
        else
        {
            $self->{$attrname} = $self->_default_for($attrname);
        }
    }

    # create server socket
    "0" =~ /(0)/;    # Perl 5.7 / IO::Socket::INET bug workaround.
    $self->{_server_sock} =
      IO::Socket::INET->new(
                            LocalHost => $self->{_localAddr},
                            LocalPort => $self->{_localPort},
                            Listen    => 1,
                            Reuse     => $self->{_reuseAddr},
                            Proto     => 'tcp',
                            Type      => SOCK_STREAM
                           )
      or die "bind: $!";

    foreach my $file (keys %{$self->{_input}})
    {
        my $ref = \$self->{_input}{$file}{content};
        $$ref =~ s/\Q{{port}}/$self->sockport/eg;
    }

    return $self;
}

sub run
{
    my ($self, $synch_callback) = @_;
    my $initialized = 0;

    # turn buffering off on STDERR
    select((select(STDERR), $| = 1)[0]);

    # initialize command table
    my $command_table = {};
    foreach (keys %_commands)
    {
        my $subname = "_${_}_command";
        $command_table->{$_} = \&$subname;
    }

    my $old_ils = $/;
    $/ = "\r\n";

    if (!$initialized)
    {
        $synch_callback->();
        $initialized = 1;
    }

    $SIG{CHLD} = sub { wait };
    my $server_sock = $self->{_server_sock};

    # the accept loop
    while (my $client_addr = accept(my $socket, $server_sock))
    {
        # turn buffering off on $socket
        select((select($socket), $| = 1)[0]);

        # find out who connected
        my ($client_port, $client_ip) = sockaddr_in($client_addr);
        my $client_ipnum = inet_ntoa($client_ip);

        # print who connected
        print STDERR "got a connection from: $client_ipnum\n" if $log;

        # fork off a process to handle this connection.
        # my $pid = fork();
        # unless (defined $pid) {
        #     warn "fork: $!";
        #     sleep 5; # Back off in case system is overloaded.
        #     next;
        # }

        if (1)
        {    # Child process.

            # install signals
            $SIG{URG} = sub {
                $GOT_SIGURG = 1;
            };

            $SIG{PIPE} = sub {
                print STDERR "Client closed connection abruptly.\n";
                exit;
            };

            $SIG{ALRM} = sub {
                print STDERR
                  "Connection idle timeout expired. Closing server.\n";
                exit;
            };

            #$SIG{CHLD} = 'IGNORE';

            print STDERR "in child\n" if $log;

            my $conn = {
                'paths' =>
                  FTPPaths->new($self->{'_input'}, $self->{'_server_behavior'}),
                'socket'  => $socket,
                'state'   => $_connection_states{NEWCONN},
                'dir'     => '/',
                'restart' => 0,
                'idle_timeout' => 60,                  # 1 minute timeout
                'rootdir'      => $self->{_rootDir},
                       };

            print {$conn->{socket}}
              "220 GNU Wget Testing FTP Server ready.\r\n";

            # command handling loop
            for (; ;)
            {
                print STDERR "waiting for request\n" if $log;

                last unless defined(my $req = <$socket>);

                # Remove trailing CRLF.
                $req =~ s/[\n\r]+$//;

                print STDERR "received request $req\n" if $log;

                # Get the command.
                # See also RFC 2640 section 3.1.
                unless ($req =~ m/^([A-Z]{3,4})\s?(.*)/i)
                {
                    # badly formed command
                    exit 0;
                }

                # The following strange 'eval' is necessary to work around a
                # very odd bug in Perl 5.6.0. The following assignment to
                # $cmd will fail in some cases unless you use $1 in some sort
                # of an expression beforehand.
                # - RWMJ 2002-07-05.
                eval '$1 eq $1';

                my ($cmd, $rest) = (uc $1, $2);

                # Got a command which matches in the table?
                unless (exists $command_table->{$cmd})
                {
                    print {$conn->{socket}} "500 Unrecognized command.\r\n";
                    next;
                }

                # Command requires user to be authenticated?
                unless ($_commands{$cmd} | $conn->{state})
                {
                    print {$conn->{socket}} "530 Not logged in.\r\n";
                    next;
                }

                # Handle the QUIT command specially.
                if ($cmd eq "QUIT")
                {
                    print {$conn->{socket}}
                      "221 Goodbye. Service closing connection.\r\n";
                    last;
                }

                if (defined($self->{_server_behavior}{fail_on_pasv})
                    && $cmd eq 'PASV')
                {
                    undef $self->{_server_behavior}{fail_on_pasv};
                    close $socket;
                    last;
                }

                if (defined($self->{_server_behavior}{pasv_not_supported})
                    && $cmd eq 'PASV')
                {
                    print {$conn->{socket}}
                      "500 PASV not supported.\r\n";
                    next;
                }

                # Run the command.
                &{$command_table->{$cmd}}($conn, $cmd, $rest);
            }
        }
        else
        {    # Father
            close $socket;
        }
    }

    $/ = $old_ils;
}

sub sockport
{
    my $self = shift;
    return $self->{_server_sock}->sockport;
}

package FTPPaths;

use POSIX qw(strftime);

# not a method
sub final_component
{
    my $path = shift;

    $path =~ s|.*/||;
    return $path;
}

# not a method
sub path_merge
{
    my ($path_a, $path_b) = @_;

    if (!$path_b)
    {
        return $path_a;
    }

    if ($path_b =~ m.^/.)
    {
        $path_a = '';
        $path_b =~ s.^/..;
    }
    $path_a =~ s./$..;

    my @components = split m{/}msx, $path_b;

    foreach my $c (@components)
    {
        if ($c =~ /^\.?$/)
        {
            next;
        }
        elsif ($c eq '..')
        {
            if (!$path_a)
            {
                next;
            }
            $path_a =~ s|/[^/]*$||;
        }
        else
        {
            $path_a .= "/$c";
        }
    }

    return $path_a;
}

sub new
{
    my ($this, @args) = @_;
    my $class = ref($this) || $this;
    my $self = {};
    bless $self, $class;
    $self->initialize(@args);
    return $self;
}

sub initialize
{
    my ($self, $urls, $behavior) = @_;
    my $paths = {_type => 'd'};

    # From a path like '/foo/bar/baz.txt', construct $paths such that
    # $paths->{'foo'}->{'bar'}->{'baz.txt'} is
    # $urls->{'/foo/bar/baz.txt'}.
    for my $path (keys %$urls)
    {
        my @components = split m{/}msx, $path;
        shift @components;
        my $x = $paths;
        for my $c (@components)
        {
            if (!exists $x->{$c})
            {
                $x->{$c} = {_type => 'd'};
            }
            $x = $x->{$c};
        }
        %$x = %{$urls->{$path}};
        $x->{_type} = 'f';
    }

    $self->{'_paths'}    = $paths;
    $self->{'_behavior'} = $behavior;
    return 1;
}

sub get_info
{
    my ($self, $path, $node) = @_;
    $node = $self->{'_paths'} unless $node;
    my @components = split('/', $path);
    shift @components if @components && $components[0] eq '';

    for my $c (@components)
    {
        if ($node->{'_type'} eq 'd')
        {
            $node = $node->{$c};
        }
        else
        {
            return;
        }
    }
    return $node;
}

sub dir_exists
{
    my ($self, $path) = @_;
    return $self->path_exists($path, 'd');
}

sub path_exists
{
    # type is optional, in which case we don't check it.
    my ($self, $path, $type) = @_;
    my $paths = $self->{'_paths'};

    die "Invalid path $path (not absolute).\n" unless $path =~ m.^/.;
    my $info = $self->get_info($path);
    return 0 unless defined($info);
    return $info->{'_type'} eq $type if defined($type);
    return 1;
}

sub _format_for_list
{
    my ($self, $name, $info) = @_;

    # XXX: mode should be specifyable as part of the node info.
    my $mode_str;
    if ($info->{'_type'} eq 'd')
    {
        $mode_str = 'dr-xr-xr-x';
    }
    else
    {
        $mode_str = '-r--r--r--';
    }

    my $size = 0;
    if ($info->{'_type'} eq 'f')
    {
        $size = length $info->{'content'};
        if ($self->{'_behavior'}{'bad_list'})
        {
            $size = 0;
        }
    }
    my $date = strftime("%b %d %H:%M", localtime);
    return "$mode_str 1  0  0  $size $date $name";
}

sub get_list
{
    my ($self, $path, $no_hidden) = @_;
    my $info = $self->get_info($path);
    if (!defined $info)
    {
        return;
    }
    my $list = [];

    if ($info->{'_type'} eq 'd')
    {
        for my $item (keys %$info)
        {
            next if $item =~ /^_/;

            # 2013-10-17 Andrea Urbani (matfanjol)
            #            I skip the hidden files if requested
            if (   ($no_hidden)
                && (defined($info->{$item}->{'attr'}))
                && (index($info->{$item}->{'attr'}, "H") >= 0))
            {
                # This is an hidden file and I don't want to see it!
                print STDERR "get_list: Skipped hidden file [$item]\n";
            }
            else
            {
                push @$list, $self->_format_for_list($item, $info->{$item});
            }
        }
    }
    else
    {
        push @$list, $self->_format_for_list(final_component($path), $info);
    }

    return $list;
}

# 2013-10-17 Andrea Urbani (matfanjol)
# It returns the behavior of the given name.
# In this file I handle also the following behaviors:
#  list_dont_clean_path  : if defined, the command
#                           $path =~ s/^-[a-zA-Z0-9]+\s?//;
#                          is not runt and the given path
#                          remains the original one
#  list_empty_if_list_a  : if defined, "LIST -a" returns an
#                          empty content
#  list_fails_if_list_a  : if defined, "LIST -a" returns an
#                          error
#  list_no_hidden_if_list: if defined, "LIST" doesn't return
#                          hidden files.
#                          To define an hidden file add
#                            attr => "H"
#                          to the url files
#  syst_response         : if defined, its content is printed
#                          out as SYST response
sub GetBehavior
{
    my ($self, $name) = @_;
    return $self->{'_behavior'}{$name};
}

1;

# vim: et ts=4 sw=4
