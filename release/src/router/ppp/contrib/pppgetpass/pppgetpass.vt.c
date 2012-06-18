#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <termios.h>
#include <sys/vt.h>

static int console_owner(uid_t, int);

int main(int argc, char **argv)
{
  int console;
  uid_t uid;
  struct vt_stat origstate;
  int openvtnum;
  char openvtname[256];
  int openvt;
  gid_t gid;
  int chowned;
  FILE *fp;
  struct termios t;
  char pass[256], *nl;
  int outfd, passlen;
  ssize_t wrote;
  console=open("/dev/console", O_RDWR);

  uid=getuid();
  gid=getgid();
  seteuid(uid);

  openlog(argv[0], LOG_PID, LOG_DAEMON);

  if(argc!=4) {
    syslog(LOG_WARNING, "Usage error");
    return 1;
  }

  if(console<0) {
    syslog(LOG_ERR, "open(/dev/console): %m");
    return 1;
  }

  if(ioctl(console, VT_GETSTATE, &origstate)<0) {
    syslog(LOG_ERR, "VT_GETSTATE: %m");
    return 1;
  }

  if(uid) {
    if(!console_owner(uid, origstate.v_active)) {
      int i;
      for(i=0;i<64;++i) {
        if(i!=origstate.v_active && console_owner(uid, i))
          break;
      }
      if(i==64) {
        syslog(LOG_WARNING, "run by uid %lu not at console", (unsigned long)uid);
        return 1;
      }
    }
  }

  if(ioctl(console, VT_OPENQRY, &openvtnum)<0) {
    syslog(LOG_ERR, "VT_OPENQRY: %m");
    return 1;
  }
  if(openvtnum==-1) {
    syslog(LOG_ERR, "No free VTs");
    return 1;
  }

  snprintf(openvtname, sizeof openvtname, "/dev/tty%d", openvtnum);
  seteuid(0);
  openvt=open(openvtname, O_RDWR);
  if(openvt<0) {
    seteuid(uid);
    syslog(LOG_ERR, "open(%s): %m", openvtname);
    return 1;
  }

  chowned=fchown(openvt, uid, gid);
  if(chowned<0) {
    seteuid(uid);
    syslog(LOG_ERR, "fchown(%s): %m", openvtname);
    return 1;
  }

  close(console);

  if(ioctl(openvt, VT_ACTIVATE, openvtnum)<0) {
    seteuid(uid);
    syslog(LOG_ERR, "VT_ACTIVATE(%d): %m", openvtnum);
    return 1;
  }

  while(ioctl(openvt, VT_WAITACTIVE, openvtnum)<0) {
    if(errno!=EINTR) {
      ioctl(openvt, VT_ACTIVATE, origstate.v_active);
      seteuid(uid);
      syslog(LOG_ERR, "VT_WAITACTIVE(%d): %m", openvtnum);
      return 1;
    }
  }

  seteuid(uid);
  fp=fdopen(openvt, "r+");
  if(!fp) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    syslog(LOG_ERR, "fdopen(%s): %m", openvtname);
    return 1;
  }

  if(tcgetattr(openvt, &t)<0) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    syslog(LOG_ERR, "tcgetattr(%s): %m", openvtname);
    return 1;
  }
  t.c_lflag &= ~ECHO;
  if(tcsetattr(openvt, TCSANOW, &t)<0) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    syslog(LOG_ERR, "tcsetattr(%s): %m", openvtname);
    return 1;
  }

  if(fprintf(fp, "\033[2J\033[H")<0) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    syslog(LOG_ERR, "write error on %s: %m", openvtname);
    return 1;
  }
  if(argv[1][0] && argv[2][0]) {
    if(fprintf(fp, "Password for PPP client %s on server %s: ", argv[1], argv[2])<0) {
      seteuid(0);
      ioctl(openvt, VT_ACTIVATE, origstate.v_active);
      seteuid(uid);
      syslog(LOG_ERR, "write error on %s: %m", openvtname);
      return 1;
    }
  } else if(argv[1][0] && !argv[2][0]) {
    if(fprintf(fp, "Password for PPP client %s: ", argv[1])<0) {
      syslog(LOG_ERR, "write error on %s: %m", openvtname);
      seteuid(0);
      ioctl(openvt, VT_ACTIVATE, origstate.v_active);
      seteuid(uid);
      return 1;
    }
  } else if(!argv[1][0] && argv[2][0]) {
    if(fprintf(fp, "Password for PPP on server %s: ", argv[2])<0) {
      seteuid(0);
      ioctl(openvt, VT_ACTIVATE, origstate.v_active);
      seteuid(uid);
      syslog(LOG_ERR, "write error on %s: %m", openvtname);
      return 1;
    }
  } else {
    if(fprintf(fp, "Enter PPP password: ")<0) {
      seteuid(0);
      ioctl(openvt, VT_ACTIVATE, origstate.v_active);
      seteuid(uid);
      syslog(LOG_ERR, "write error on %s: %m", openvtname);
      return 1;
    }
  }

  if(!fgets(pass, sizeof pass, fp)) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    if(ferror(fp)) {
      syslog(LOG_ERR, "read error on %s: %m", openvtname);
    }
    return 1;
  }
  if((nl=strchr(pass, '\n'))) 
    *nl=0;
  passlen=strlen(pass);
  
  outfd=atoi(argv[3]);
  if((wrote=write(outfd, pass, passlen))!=passlen) {
    seteuid(0);
    ioctl(openvt, VT_ACTIVATE, origstate.v_active);
    seteuid(uid);
    if(wrote<0)
      syslog(LOG_ERR, "write error on outpipe: %m");
    else
      syslog(LOG_ERR, "short write on outpipe");
    return 1;
  }

  seteuid(0);
  ioctl(openvt, VT_ACTIVATE, origstate.v_active);
  seteuid(uid);
  return 0;
}

static int console_owner(uid_t uid, int cons)
{
  char name[256];
  struct stat st;
  snprintf(name, sizeof name, "/dev/tty%d", cons);
  if(stat(name, &st)<0) {
    if(errno!=ENOENT)
      syslog(LOG_ERR, "stat(%s): %m", name);
    return 0;
  }
  return uid==st.st_uid;
}
