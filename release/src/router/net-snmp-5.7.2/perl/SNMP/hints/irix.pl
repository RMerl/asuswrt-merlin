# uses libelf for nlist on irix (>= 5.3?)
$self->{LIBS} .= ' -lelf';
