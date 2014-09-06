# uses kstat on solaris for get_uptime
$self->{LIBS} .= ' -lkstat';
# add -ldb if you are getting the vsnprintf not found error
# $self->{LIBS} .= ' -lkstat -ldb';
