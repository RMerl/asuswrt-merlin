package HTTPServer;

use strict;
use warnings;

use HTTP::Daemon;
use HTTP::Status;
use HTTP::Headers;
use HTTP::Response;

our @ISA = qw(HTTP::Daemon);
my $VERSION = 0.01;

my $CRLF = "\015\012";    # "\r\n" is not portable
my $log  = undef;

sub run
{
    my ($self, $urls, $synch_callback) = @_;
    my $initialized = 0;

    while (1)
    {
        if (!$initialized)
        {
            $synch_callback->();
            $initialized = 1;
        }
        my $con = $self->accept();
        print STDERR "Accepted a new connection\n" if $log;
        while (my $req = $con->get_request)
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
                print STDERR "Method: ", $req->method, "\n";
                print STDERR "Path: ", $url_path, "\n";
                print STDERR "Available URLs: ", "\n";
                foreach my $key (keys %$urls)
                {
                    print STDERR $key, "\n";
                }
            }
            if (exists($urls->{$url_path}))
            {
                print STDERR "Serving requested URL: ", $url_path, "\n" if $log;
                next unless ($req->method eq "HEAD" || $req->method eq "GET");

                my $url_rec = $urls->{$url_path};
                $self->send_response($req, $url_rec, $con);
            }
            else
            {
                print STDERR "Requested wrong URL: ", $url_path, "\n" if $log;
                $con->send_error($HTTP::Status::RC_FORBIDDEN);
                last;
            }
        }
        print STDERR "Closing connection\n" if $log;
        $con->close;
    }
}

sub send_response
{
    my ($self, $req, $url_rec, $con) = @_;

    # create response
    my ($code, $msg, $headers);
    my $send_content = ($req->method eq "GET");
    if (exists $url_rec->{'auth_method'})
    {
        ($send_content, $code, $msg, $headers) =
          $self->handle_auth($req, $url_rec);
    }
    elsif (!$self->verify_request_headers($req, $url_rec))
    {
        ($send_content, $code, $msg, $headers) =
          ('', 400, 'Mismatch on expected headers', {});
    }
    else
    {
        ($code, $msg) = @{$url_rec}{'code', 'msg'};
        $headers = $url_rec->{headers};
    }
    my $resp = HTTP::Response->new($code, $msg);
    print STDERR "HTTP::Response: \n", $resp->as_string if $log;

    while (my ($name, $value) = each %{$headers})
    {
        # print STDERR "setting header: $name = $value\n";
        $value = $self->_substitute_port($value);
        $resp->header($name => $value);
    }
    print STDERR "HTTP::Response with headers: \n", $resp->as_string if $log;

    if ($send_content)
    {
        my $content = $url_rec->{content};
        if (exists($url_rec->{headers}{"Content-Length"}))
        {
            # Content-Length and length($content) don't match
            # manually prepare the HTTP response
            $con->send_basic_header($url_rec->{code}, $resp->message,
                                    $resp->protocol);
            print $con $resp->headers_as_string($CRLF);
            print $con $CRLF;
            print $con $content;
            next;
        }
        if ($req->header("Range") && !$url_rec->{'force_code'})
        {
            $req->header("Range") =~ m/bytes=(\d*)-(\d*)/;
            my $content_len = length($content);
            my $start       = $1 ? $1 : 0;
            my $end         = $2 ? $2 : ($content_len - 1);
            my $len         = $2 ? ($2 - $start) : ($content_len - $start);
            if ($len > 0)
            {
                $resp->header("Accept-Ranges"  => "bytes");
                $resp->header("Content-Length" => $len);
                $resp->header(
                           "Content-Range" => "bytes $start-$end/$content_len");
                $resp->header("Keep-Alive" => "timeout=15, max=100");
                $resp->header("Connection" => "Keep-Alive");
                $con->send_basic_header(206,
                                        "Partial Content", $resp->protocol);
                print $con $resp->headers_as_string($CRLF);
                print $con $CRLF;
                print $con substr($content, $start, $len);
            }
            else
            {
                $con->send_basic_header(416, "Range Not Satisfiable",
                                        $resp->protocol);
                $resp->header("Keep-Alive" => "timeout=15, max=100");
                $resp->header("Connection" => "Keep-Alive");
                print $con $CRLF;
            }
            next;
        }

        # fill in content
        $content = $self->_substitute_port($content) if defined $content;
        $resp->content($content);
        print STDERR "HTTP::Response with content: \n", $resp->as_string
          if $log;
    }

    $con->send_response($resp);
    print STDERR "HTTP::Response sent: \n", $resp->as_string if $log;
}

# Generates appropriate response content based on the authentication
# status of the URL.
sub handle_auth
{
    my ($self, $req, $url_rec) = @_;
    my ($send_content, $code, $msg, $headers);

    # Catch failure to set code, msg:
    $code = 500;
    $msg  = "Didn't set response code in handle_auth";

    # Most cases, we don't want to send content.
    $send_content = 0;

    # Initialize headers
    $headers = {};
    my $authhdr = $req->header('Authorization');

    # Have we sent the challenge yet?
    unless ($url_rec->{auth_challenged} || $url_rec->{auth_no_challenge})
    {
        # Since we haven't challenged yet, we'd better not
        # have received authentication (for our testing purposes).
        if ($authhdr)
        {
            $code = 400;
            $msg  = "You sent auth before I sent challenge";
        }
        else
        {
            # Send challenge
            $code = 401;
            $msg  = "Authorization Required";
            $headers->{'WWW-Authenticate'} =
              $url_rec->{'auth_method'} . " realm=\"wget-test\"";
            $url_rec->{auth_challenged} = 1;
        }
    }
    elsif (!defined($authhdr))
    {
        # We've sent the challenge; we should have received valid
        # authentication with this one. A normal server would just
        # resend the challenge; but since this is a test, wget just
        # failed it.
        $code = 400;
        $msg  = "You didn't send auth after I sent challenge";
        if ($url_rec->{auth_no_challenge})
        {
            $msg = "--auth-no-challenge but no auth sent.";
        }
    }
    else
    {
        my ($sent_method) = ($authhdr =~ /^(\S+)/g);
        unless ($sent_method eq $url_rec->{'auth_method'})
        {
            # Not the authorization type we were expecting.
            $code = 400;
            $msg  = "Expected auth type $url_rec->{'auth_method'} but got "
              . "$sent_method";
        }
        elsif (
               (
                   $sent_method eq 'Digest'
                && &verify_auth_digest($authhdr, $url_rec, \$msg)
               )
               || (   $sent_method eq 'Basic'
                   && &verify_auth_basic($authhdr, $url_rec, \$msg))
              )
        {
            # SUCCESSFUL AUTH: send expected message, headers, content.
            ($code, $msg) = @{$url_rec}{'code', 'msg'};
            $headers      = $url_rec->{headers};
            $send_content = 1;
        }
        else
        {
            $code = 400;
        }
    }

    return ($send_content, $code, $msg, $headers);
}

sub verify_auth_digest
{
    return undef;    # Not yet implemented.
}

sub verify_auth_basic
{
    require MIME::Base64;
    my ($authhdr, $url_rec, $msgref) = @_;
    my $expected =
      MIME::Base64::encode_base64(
                                $url_rec->{'user'} . ':' . $url_rec->{'passwd'},
                                '');
    my ($got) = $authhdr =~ /^Basic (.*)$/;
    if ($got eq $expected)
    {
        return 1;
    }
    else
    {
        $$msgref = "Wanted ${expected} got ${got}";
        return undef;
    }
}

sub verify_request_headers
{
    my ($self, $req, $url_rec) = @_;

    return 1 unless exists $url_rec->{'request_headers'};
    for my $hdrname (keys %{$url_rec->{'request_headers'}})
    {
        my $must_not_match;
        my $ehdr = $url_rec->{'request_headers'}{$hdrname};
        if ($must_not_match = ($hdrname =~ /^!(\w+)/))
        {
            $hdrname = $1;
        }
        my $rhdr = $req->header($hdrname);
        if ($must_not_match)
        {
            if (defined $rhdr && $rhdr =~ $ehdr)
            {
                $rhdr = '' unless defined $rhdr;
                print STDERR "\n*** Match forbidden $hdrname: $rhdr =~ $ehdr\n";
                return undef;
            }
        }
        else
        {
            unless (defined $rhdr && $rhdr =~ $ehdr)
            {
                $rhdr = '' unless defined $rhdr;
                print STDERR "\n*** Mismatch on $hdrname: $rhdr =~ $ehdr\n";
                return undef;
            }
        }
    }

    return 1;
}

sub _substitute_port
{
    my $self = shift;
    my $ret  = shift;
    $ret =~ s/\Q{{port}}/$self->sockport/eg;
    return $ret;
}

1;

# vim: et ts=4 sw=4
