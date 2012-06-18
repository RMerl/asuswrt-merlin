#!/usr/bin/perl
while (1)
{
	exit 0 if read(STDIN,$c,1) == 0;
	last if ($cl eq "\031" && $c eq "\001");
	$cl = $c;
}
kill 'STOP',$$;
exit 0
