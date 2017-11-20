package SSLServer;

# This is only HTTPS server for now.
# But it is named SSLServer to easily distinguish from HTTPServer

use strict;
use warnings;
use lib '.';

use HTTP::Daemon;
use HTTP::Status;
use HTTP::Headers;
use HTTP::Response;

# Skip this test rather than fail it when the module isn't installed
if (!eval {require IO::Socket::SSL;1;}) {
    print STDERR "This test needs the perl module \"IO::Socket::SSL\".\n";
    print STDERR "Install e.g. on Debian with 'apt-get install libio-socket-ssl-perl'\n";
    print STDERR " or if using cpanminus 'cpanm IO::Socket::SSL' could be used to install it.\n";
    exit 77; # skip
}

#use IO::Socket::SSLX; # 'debug4';
use HTTPServer;

our @ISA = qw(IO::Socket::SSL HTTP::Daemon::ClientConn HTTP::Daemon HTTPServer);

my $VERSION = 0.01;

my $CRLF = "\015\012";    # "\r\n" is not portable

# Config options for server
my $log  = undef;
my $DEBUG = undef;

my %ssl_params;

my $sslsock;
my $plaincon;
my %args;

#$HTTP::Daemon::DEBUG=5;
#*DEBUG = \$HTTP::Daemon::DEBUG;

$args{SSL_error_trap} ||= \&ssl_error;

my $class = 'SSLServer';
my $self  = {};
$self = bless $self, $class;

sub init
{
    my $self = shift;
    my %sargs = @_;

    %ssl_params = %sargs;
    unless (exists($ssl_params{'lhostname'}) &&
            exists($ssl_params{'sslport'})   &&
            exists($ssl_params{'ciphers'})   &&
            exists($ssl_params{'cafile'})    &&
            exists($ssl_params{'certfile'})  &&
            exists($ssl_params{'keyfile'})) {
        die "Required parameters for SSL tests are missing";
    }
}

sub ssl_setup_conn
{
    $sslsock = IO::Socket::SSL->new(LocalAddr       => $ssl_params{'lhostname'},
                                    LocalPort       => $ssl_params{'sslport'},
                                    Listen          => 10,
                                    Timeout         => 30,
                                    ReuseAddr       => 1,
                                    SSL_cipher_list => $ssl_params{'ciphers'},
                                    SSL_verify_mode => 0x00,
                                    SSL_ca_file     => $ssl_params{'cafile'},
                                    SSL_cert_file   => $ssl_params{'certfile'},
                                    SSL_key_file    => $ssl_params{'keyfile'});

    $sslsock || warn $IO::Socket::SSL::ERROR;
    return $sslsock;
}

sub fileno
{
    my $self = shift;
    my $fn = ${*$self}{'_SSL_fileno'};
    return defined($fn) ? $fn : $self->SUPER::fileno();
}

sub accept
{
    my $self = shift;
    my $pkg = shift || "SSLServer";
    my ($sock, $peer) = $sslsock->accept($pkg);
    if ($sock) {
        ${*$sock}{'httpd_daemon'} = $self;
        ${*$self}{'httpd_daemon'} = $sock;
        my $fileno = ${*$self}{'_SSL_fileno'} = &fileno($self);
        my $f = $sock->fileno;
        return wantarray ? ($sock, $peer) : $sock;
    }
    else {
        print STDERR "Failed to get socket from SSL\n" if $DEBUG;
        return;
    }

}

sub _default_port { 443; }
sub _default_scheme { "https"; }

sub url
{
    my $self = shift;
    my $url = $self->SUPER::url;
    return $url if ($self->can("HTTP::Daemon::_default_port"));

    # Workaround for old versions of HTTP::Daemon
    $url =~ s!^http:!https:!;
    $url =~ s!/$!:80/! unless ($url =~ m!:(?:\d+)/$!);
    $url =~ s!:443/$!/!;
    return $url;
}

sub _need_more
{
    my $self = shift;
    if ($_[1]) {
        my($timeout, $fdset) = @_[1,2];
        print STDERR "select(,,,$timeout)\n" if $DEBUG;
        my $n = select($fdset,undef,undef,$timeout);
        unless ($n) {
            $self->reason(defined($n) ? "Timeout" : "select: $!");
            return;
        }
    }
    my $total = 0;
    while (1){
        print STDERR sprintf("sysread() already %d\n",$total) if $DEBUG;
        my $n = sysread(${*$self}{'httpd_daemon'}, $_[0], 2048, length($_[0]));
        print STDERR sprintf("sysread() just \$n=%s\n",(defined $n?$n:'undef')) if $DEBUG;
        $total += $n if defined $n;
        last if $! =~ 'Resource temporarily unavailable';
        #SSL_Error because of aggressive reading

        $self->reason(defined($n) ? "Client closed" : "sysread: $!") unless $n;
        last unless $n;
        last unless $n == 2048;
    }
    $total;
}

sub daemon
{
    my $self = shift;
    ${*$self}{'httpd_daemon'};
}

sub conn
{
    my $self = shift;
    ${*$self}{'sslcon'};
}

sub run
{
    my ($self, $urls, $synch_callback) = @_;
    my $initialized = 0;
    my $sslsock;

    while (1)
    {
        if (!$initialized)
        {
            $sslsock = $self->ssl_setup_conn();
            $sslsock || warn "Failed to get ssl sock";

            $initialized = 1;
            open (LOGFILE, '>', "/tmp/wgetserver.log");
            LOGFILE->autoflush(1);
            print LOGFILE "Starting logging";
            $synch_callback->() if $synch_callback;
        }

        my $con = $self->accept();
        ${*$self}{'sslcon'} = $con;

        while (my $req = $self->get_request)
        {
            #my $url_path = $req->url->path;
            my $url_path = $req->url->as_string;
            if ($url_path =~ m{/$})
            {    # append 'index.html'
                $url_path .= 'index.html';
            }

            #if ($url_path =~ m{^/}) { # remove trailing '/'
            #    $url_path = substr ($url_path, 1);
            #}
            if ($log)
            {
                print LOGFILE "Method: ", $req->method, "\n";
                print LOGFILE "Path: ", $url_path, "\n";
                print LOGFILE "Available URLs: ", "\n";
                foreach my $key (keys %$urls)
                {
                    print LOGFILE $key, "\n";
                }
            }
            if (exists($urls->{$url_path}))
            {
                print LOGFILE "Serving requested URL: ", $url_path, "\n" if $log;
                next unless ($req->method eq "HEAD" || $req->method eq "GET");

                my $url_rec = $urls->{$url_path};
                HTTPServer::send_response($self, $req, $url_rec, $con);
                last;
            }
            else
            {
                print LOGFILE "Requested wrong URL: ", $url_path, "\n" if $log;
                $con->send_error($HTTP::Status::RC_FORBIDDEN);
                last;
            }
            last;
        }
        print LOGFILE "Closing connection\n" if $log;
        close(LOGFILE);
        $con->close();
    }
}

1;

# vim: et ts=4 sw=4
