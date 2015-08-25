#!/usr/bin/env perl
#
#  $Id: mt-daapd-ssc.pl 954 2006-04-15 04:45:25Z rpedde $
#
#   -*- perl -*-
#
#  ----------------------------------------------------------------------
#  Server side media format conversion script for mt-daapd.
#  ----------------------------------------------------------------------
#  Copyright & 2005
#  Timo J. Rinne <tri@iki.fi>
#  ----------------------------------------------------------------------
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

#
# This is quite rudimentary but handles most cases quite well.
#
# TODO-list:
#   - Make the guessing of the wav length reliable.
#   - Possibly implement some kind of caching.
#
# I'll probably never have time for above things, but now it should
# be easy for someone to take this over, since basically all you have
# to do is to make the filter program and tweak the configuration.
#
#                                                            //tri
#

require 'sys/syscall.ph';

use POSIX ":sys_wait_h";
use IO::Handle;
use IO::File;
use IPC::Open2;
use IPC::Open3;

$SIG{PIPE} = 'IGNORE';

{
  umask(0077);
  if ($#ARGV < 1) {
    usage();
  }
  my ($fn) = $ARGV[0];
  my ($off) = 0 + $ARGV[1];
  my ($forgelen) = undef;
  if ($#ARGV > 1) {
    $forgelen = 0.0 + $ARGV[2];
    if ($forgelen < 0.01) {
      $forgelen = 0.0;
    }
  } else {
    $forgelen = 0.0;
  }
  if ($off < 0) {
    usage();
  }
  if ($fn =~ m/^..*\.wav$/i) {
    passthru_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.wave$/i) {
    passthru_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.flac$/i) {
    flac_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.fla$/i) {
    flac_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.mp4$/i) {
    mpeg4_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.m4a$/i) {
    mpeg4_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.m4p$/i) {
    mpeg4_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.aac$/i) {
    mpeg4_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.shn$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.mp3$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.mpg$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.ogg$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.avi$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } elsif ($fn =~ m/^..*\.au$/i) {
    ffmpeg_proc($fn, $off, $forgelen);
  } else {
    mplayer_proc($fn, $off, $forgelen);
  }
  exit;
}

sub usage
{
  print STDERR "usage: ssc-script file offset\n";
  exit -1;
}

sub tmp_file
{
  my ($r) = sprintf "/tmp/t-%d.wav", $$;
  return $r;
}

sub wav_loop
{
  my ($f) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($buf);
  my ($buflen);
  my ($buftry) = 4096;
  my ($hdr);

  $hdr = read_wav_hdr($f);
  if (! defined $hdr) {
    return undef;
  }

  my ($chunk_data_length,
      $format_data_length,
      $compression_code,
      $channel_count,
      $sample_rate,
      $sample_bit_length,
      $data_length) = parse_wav_hdr($hdr);

  if (! defined $chunk_data_length) {
    print STDERR "Can't parse WAV header.\n";
    return undef;
  }

  if (defined $forgelen) {
    if ($forgelen > 0.00001) {
      my ($sec) = int($forgelen);
      my ($msec) = ($forgelen - (0.0 + int($forgelen))) * 1000;
      my ($bps) = ($sample_rate * $channel_count * int(($sample_bit_length + 7) / 8));
      my ($len) = int(($sec * $bps) + (($msec * $bps) / 1000));
      my ($blocklen);
      my ($residual);
      if (($sample_rate == 44100) &&
	  ($channel_count == 2) &&
	  ($sample_bit_length == 16)) {
	# It's probably a CD track so let's round it to next cd block limit (2352 bytes).
	$blocklen = 2352;
      } else {
	# Pad it at least to next valid sample border.
	$blocklen = ($channel_count * int(($sample_bit_length + 7) / 8))
      }
      $residual = $len % $blocklen;

      if ($residual != 0) {
	my ($pad) = $blocklen - $residual;
	$len += $pad;
      }
      $hdr = forge_wav_hdr($hdr, $len);
    } else {
      $hdr = forge_continuous_wav_hdr($hdr);
    }
  }

  if (($off > 0) && ($off < length($hdr))) {
    $hdr = substr($hdr, $off, (length($hdr) - $off));
  } elsif ($off == length($hdr)) {
    $hdr = undef;
  } elsif ($off > length($hdr)) {
    $hdr = undef;
    $off -= length($hdr);
    if (! sysseek($f, $off, 1)) {
      # It's not seekable.  Then just read until desired offset.
      while ($off > 0) {
	my ($rl) = $off > $buftry ? $buftry : $off;
	$buflen = sysread($f, $buf, $rl);
	if ($buflen < 1) {
	  return undef;
	}
	$off -= $buflen;
      }
    }
  }

  if (defined $hdr) {
    if (syswrite (STDOUT, $hdr) != length($hdr)) {
      print STDERR "Write failed.\n";
      return undef;
    }
  }

  while (1) {
    $buflen = sysread($f, $buf, $buftry);
    if ($buflen < 1) {
      return 1;
    }
    if (syswrite (STDOUT, $buf) != $buflen) {
      print STDERR "Write failed.\n";
      return undef;
    }
  }
}

sub read_wav_hdr
{
  my ($f) = shift;
  my ($hdr) = undef;
  my ($format_data_length);

  if (sysread($f, $hdr, 44) != 44) {
    print STDERR "Can't read WAV header.\n";
    return undef;
  }

  return undef
    if ((substr($hdr,  0, 4) ne "RIFF"));

  return undef
    if ((substr($hdr,  8, 4) ne "WAVE"));

  return undef
    if ((substr($hdr, 12, 4) ne "fmt "));

  $format_data_length = (((((((ord(substr($hdr, 19, 1))) * 256) +
			     (ord(substr($hdr, 18, 1)))) * 256) +
			   (ord(substr($hdr, 17, 1)))) * 256) +
			 (ord(substr($hdr, 16, 1))));

  return undef
    if (($format_data_length < 16) || ($format_data_length > 256));

  if ($format_data_length > 16) {
    my ($hdr_cont);
    my ($cont_len) = $format_data_length - 16;
    if (sysread($f, $hdr_cont, $cont_len) != $cont_len) {
      print STDERR "Can't read WAV header.\n";
      return undef;
    }
    $hdr = $hdr . $hdr_cont;
  }

  # FFMpeg creates sometimes WAVs with fmt chunk longer than 16 bytes.
  # This is weird, since at least iTunes can't play them.  Let's chop
  # extra bytes away from the fmt chunk and make the WAV header look
  # like everything else in the world seems to like it.
  if ($format_data_length > 16) {
    my ($chunk_data_length,
	$format_data_length,
	$compression_code,
	$channel_count,
	$sample_rate,
	$sample_bit_length,
	$data_length) = parse_wav_hdr($hdr);
    $chunk_data_length = $chunk_data_length - ($format_data_length - 16);
    $hdr = (substr($hdr,  0, 4) .
	    chr($chunk_data_len % 256) .
	    chr((int($chunk_data_len / 256)) % 256) .
	    chr((int($chunk_data_len / (256 * 256))) % 256) .
	    chr((int($chunk_data_len / (256 * 256 * 256))) % 256) .
	    substr($hdr,  8, 8) .
	    chr(16) .
	    chr(0) .
	    chr(0) .
	    chr(0) .
	    substr($hdr,  20, 16) .
	    substr($hdr, 20 + $format_data_length, length($hdr) - (20 + $format_data_length)));
  }

  return $hdr;
}

sub parse_wav_hdr
{
  my ($hdr) = shift;
  if (length($hdr) < 44) {
    return undef;
  }

  my ($chunk_data_length);
  my ($format_data_length);
  my ($compression_code);
  my ($channel_count);
  my ($sample_rate);
  my ($sample_bit_length);
  my ($data_length);

  return undef
    if ((substr($hdr,  0, 4) ne "RIFF"));

  $chunk_data_length = (((((((ord(substr($hdr, 7, 1))) * 256) +
			    (ord(substr($hdr, 6, 1)))) * 256) +
			  (ord(substr($hdr, 5, 1)))) * 256) +
			(ord(substr($hdr, 4, 1))));

  return undef
    if ((substr($hdr,  8, 4) ne "WAVE"));

  return undef
    if ((substr($hdr, 12, 4) ne "fmt "));

  $format_data_length = (((((((ord(substr($hdr, 19, 1))) * 256) +
			     (ord(substr($hdr, 18, 1)))) * 256) +
			   (ord(substr($hdr, 17, 1)))) * 256) +
			 (ord(substr($hdr, 16, 1))));

  return undef
    if ($format_data_length < 16);

  $compression_code = (((ord(substr($hdr, 21, 1))) * 256) +
		       (ord(substr($hdr, 20, 1))));

  return undef
    if ($compression_code != 1);

  $channel_count = (((ord(substr($hdr, 23, 1))) * 256) +
		    (ord(substr($hdr, 22, 1))));

  return undef
    if ($channel_count < 1);

  $sample_rate = (((((((ord(substr($hdr, 27, 1))) * 256) +
		      (ord(substr($hdr, 26, 1)))) * 256) +
		    (ord(substr($hdr, 25, 1)))) * 256) +
		  (ord(substr($hdr, 24, 1))));

  $sample_bit_length = (((ord(substr($hdr, 35, 1))) * 256) +
			(ord(substr($hdr, 34, 1))));

  return undef
    if ((substr($hdr, $format_data_length + 20, 4) ne "data"));

  $data_length = (((((((ord(substr($hdr, $format_data_length + 24 + 3, 1))) * 256) +
		      (ord(substr($hdr, $format_data_length + 24 + 2, 1)))) * 256) +
		    (ord(substr($hdr, $format_data_length + 24 + 1, 1)))) * 256) +
		  (ord(substr($hdr, $format_data_length + 24 + 0, 1))));

  return ($chunk_data_length,
	  $format_data_length,
	  $compression_code,
	  $channel_count,
	  $sample_rate,
	  $sample_bit_length,
	  $data_length);
}

sub forge_wav_hdr
{
  my ($hdr) = shift;
  my ($data_len) = shift;
  my ($chunk_data_len) = $data_len + 36;

  return (substr($hdr,  0, 4) .
	  chr($chunk_data_len % 256) .
	  chr((int($chunk_data_len / 256)) % 256) .
	  chr((int($chunk_data_len / (256 * 256))) % 256) .
	  chr((int($chunk_data_len / (256 * 256 * 256))) % 256) .
	  substr($hdr,  8, length($hdr) - 8 - 4) .
	  chr($data_len % 256) .
	  chr((int($data_len / 256)) % 256) .
	  chr((int($data_len / (256 * 256))) % 256) .
	  chr((int($data_len / (256 * 256 * 256))) % 256));
}

sub forge_continuous_wav_hdr
{
  my ($hdr) = shift;

  return (substr($hdr,  0, 4) .
	  chr(0xff) .
	  chr(0xff) .
	  chr(0xff) .
	  chr(0xff) .
	  substr($hdr,  8, length($hdr) - 8 - 4) .
	  chr(0xff - length($hdr) - 8) .
	  chr(0xff) .
	  chr(0xff) .
	  chr(0xff));
}

sub passthru_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($tf) = tmp_file();
  my ($f) = undef;

  if (! open($f, "< $fn")) {
    return undef;
  }
  wav_loop($f, $off, undef); # Ignore $forgelen
  close($f);
}

sub flac_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($r) = undef;
  my ($w) = undef;
  my ($pid);

  $pid = open2($r, $w, 'flac', '--silent', '--decode', '--stdout', "$fn");
  wav_loop($r, $off, undef); # Ignore $forgelen
  close($r);
  close($w);
  if (waitpid($pid, WNOHANG) <= 0) {
    kill(SIGKILL, $pid);
    waitpid($pid, 0);
  }
}

sub ffmpeg_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($r) = undef;
  my ($w) = undef;
  my ($pid);

  $pid = open2($r, $w, 'ffmpeg', '-vn', '-i', "$fn", '-f', 'wav', '-');
  wav_loop($r, $off, $forgelen);
  close($r);
  close($w);
  if (waitpid($pid, WNOHANG) <= 0) {
    kill(SIGKILL, $pid);
    waitpid($pid, 0);
  }
}

sub mplayer_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($tf) = tmp_file();
  my ($w) = undef;
  my ($f) = undef;
  my ($pid);
  my ($i);
  my ($started) = undef;
  my ($waited) = undef;
  my ($ready) = undef;
  my ($prebuf) = 2 * 1024 * 1024;
  unlink($tf);
  $pid = open3('>&STDERR', $w, '>&STDERR',
	       'mplayer',
	       '-quiet', '-really-quiet',
	       '-vo', 'null',
	       '-nomouseinput', '-nojoystick', '-nolirc',
	       '-noconsolecontrols', '-nortc',
	       '-ao', "pcm:file=$tf", "$fn");

  # Wait just to see, if the decoding starts.
  # If the resulting wav is less than 512 bytes,
  # This will fail, but who cares.
  foreach $i (0.05, 0.1, 0.2, 0.4, 0.8, 1.6, 3.2) {
    my ($s);
    my ($wp);
    select(undef, undef, undef, $i);
    $wp = waitpid($pid, WNOHANG);
    ($_, $_, $_, $_, $_, $_, $_, $s, $_, $_, $_, $_, $_) = stat("$tf");
    if (defined $s && ($s > 512)) {
      $started = 1;
    }
    if ($wp > 0) {
      $waited = 1;
    }
    if ($started || $waited) {
      last;
    }
  }
  if (! $started) {
    kill(SIGKILL, $pid);
    unlink($tf);
  }

  # Loop here until we got data beyond the offset point
  # or the process terminates.
  my ($sleep) = 0.025;
  while (! $waited) {
    my ($s);
    my ($wp);
    if ($sleep < 0.5) {
      $sleep *= 2.0;
    }
    select(undef, undef, undef, $sleep);
    $wp = waitpid($pid, WNOHANG);
    ($_, $_, $_, $_, $_, $_, $_, $s, $_, $_, $_, $_, $_) = stat("$tf");
    if (defined $s && ($s > ($off + $prebuf))) {
      $ready = 1;
    }
    if ($wp > 0) {
      $waited = 1;
    }
    if ($ready || $waited) {
      last;
    }
  }

  # Stream.
  if (open($f, "< $tf")) {
    wav_loop($f, $off, $forgelen);
    close($f);
  }
  if (! $waited) {
    waitpid($pid, 0);
  }
  close($w);
  unlink($tf);
}

sub mpeg4_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($f) = undef;
  my ($hdr) = undef;
  my ($r) = undef;

  if (open($f, "< $fn")) {
    my ($rl) = sysread($f, $hdr, 512);
    close($f);
    if ($rl != 512) {
      return undef;
    }
  } else {
    return undef;
  }

  #
  # This detection is really rudimentary, but seems to
  # do the job for now.
  #
  if (index($hdr, "Halac") >= 0) {
    $r = alac_proc($fn, $off, $forgelen);
  } else {
    $r = ffmpeg_proc($fn, $off, $forgelen);
  }

  return $r;
}

sub alac_proc
{
  my ($fn) = shift;
  my ($off) = shift;
  my ($forgelen) = shift;
  my ($r) = undef;
  my ($w) = undef;
  my ($pid);

  $pid = open2($r, $w, 'alac', "$fn");
  wav_loop($r, $off, undef); # Ignore $forgelen
  close($r);
  close($w);
  if (waitpid($pid, WNOHANG) <= 0) {
    kill(SIGKILL, $pid);
    waitpid($pid, 0);
  }
}
