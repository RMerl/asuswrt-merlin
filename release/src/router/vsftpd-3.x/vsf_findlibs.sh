#!/bin/sh
# Cheesy hacky location of additional link libraries.

locate_library() { [ ! "$1*" = "`echo $1*`" ]; }
find_func() { egrep $1 $2 >/dev/null; }

if find_func hosts_access tcpwrap.o; then
  echo "-lwrap";
  locate_library /lib/libnsl.so && echo "-lnsl";
  locate_library /lib64/libnsl.so && echo "-lnsl";
fi

# Look for PAM (done weirdly due to distribution bugs (e.g. Debian) or the
# crypt library.
if find_func pam_start sysdeputil.o; then
  locate_library /lib/libpam.so.0 && echo "/lib/libpam.so.0";
  locate_library /usr/lib/libpam.so && echo "-lpam";
  locate_library /usr/lib64/libpam.so && echo "-lpam";
  locate_library /lib/x86_64-linux-gnu/libpam.so.0 && echo "-lpam";
  # HP-UX ends shared libraries with .sl
  locate_library /usr/lib/libpam.sl && echo "-lpam";
  # AIX ends shared libraries with .a
  locate_library /usr/lib/libpam.a && echo "-lpam";
else
  locate_library /lib/libcrypt.so && echo "-lcrypt";
  locate_library /usr/lib/libcrypt.so && echo "-lcrypt";
  locate_library /usr/lib64/libcrypt.so && echo "-lcrypt";
  locate_library /lib/x86_64-linux-gnu/libcrypt.so && echo "-lcrypt";
fi

# Look for the dynamic linker library. Needed by older RedHat when
# you link in PAM
locate_library /lib/libdl.so && echo "-ldl";

# Look for libsocket. Solaris needs this.
locate_library /lib/libsocket.so && echo "-lsocket";

# Look for libnsl. Solaris needs this.
locate_library /lib/libnsl.so && echo "-lnsl";

# Look for libresolv. Solaris needs this.
locate_library /lib/libresolv.so && echo "-lresolv";

# Look for libutil. Older FreeBSD need this for setproctitle().
locate_library /usr/lib/libutil.so && echo "-lutil";

# For older HP-UX...
locate_library /usr/lib/libsec.sl && echo "-lsec";

# Look for libcap (capabilities)
if locate_library /lib/libcap.so.1; then
  echo "/lib/libcap.so.1";
elif locate_library /lib/libcap.so.2; then
  echo "/lib/libcap.so.2";
else
  locate_library /usr/lib/libcap.so && echo "-lcap";
  locate_library /lib/libcap.so && echo "-lcap";
  locate_library /lib64/libcap.so && echo "-lcap";
fi

# Solaris needs this for nanosleep()..
locate_library /lib/libposix4.so && echo "-lposix4";
locate_library /usr/lib/libposix4.so && echo "-lposix4";

# Tru64 (nanosleep)
locate_library /usr/shlib/librt.so && echo "-lrt";

# Solaris sendfile
locate_library /usr/lib/libsendfile.so && echo "-lsendfile";

# OpenSSL
if find_func SSL_library_init ssl.o; then
  echo "-lssl -lcrypto";
fi

exit 0;

