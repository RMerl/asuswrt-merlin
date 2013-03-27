open SPARC, "syscallent.h" || die "no puedo abrir el de la sparc";
open ALPHA, "../alpha/syscallent.h" || die "no puedo abrir el de la alpha";
open PC, "../i386/syscallent.h" || die "no puedo abrir PC\n";

while (<SPARC>) {
    chop;
    ($i1, $i2, $i3, $syscall, $syscall_name) = split;
    $strn[$index]   = $syscall_name;
    $name[$index++] = $syscall;
}

while (<ALPHA>){
    if (/\{/) {
	($i1, $n, $pr, $syscall) = split;
	$par{$syscall} = $n;
	$prr{$syscall} = $pr;
    }
}

while (<PC>){
    if (/\{/) {
	($i1, $n, $pr, $syscall) = split;
	$par{$syscall} = $n;
	$prr{$syscall} = $pr;
    }
}

print "missing \n";

for ($i = 0; $i < $index; $i++){
    $x = $name[$i];
    $y = $strn[$i];
    $n = $par{$x};
    $p = $prr{$x};
    $j++;
    print "\t{ $n\t$p\t$x\t$y },\t /* $j */\n";
}
