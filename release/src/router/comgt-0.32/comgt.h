/***************************************************************************
 *  comgt.h - 3G/GPRS datacard management utility header file
 *
 *  Copyright (C) 2003  Paul Hardwick <paul@peck.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***************************************************************************/

/***************************************************************************
* $Id: comgt.h,v 1.1 2008-12-04 12:29:16 michael Exp $
****************************************************************************/
#define COMGT_VERSION "0.32"
#define YEARS "2005,2006"

char _default_code[] =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    send \"AT+CFUN=1^m\"\n\
    waitquiet 1 0.2\n\
  :start\n\
    flash 0.1\n\
    send \"AT+CPIN?^m\"\n\
    waitfor 30 \"SIM PUK\",\"SIM PIN\",\"READY\",\"ERROR\",\"ERR\"\n\
    if % = -1 goto error\n\
    if % = 0 goto ready\n\
    if % = 1 goto getpin\n\
    if % = 2 goto ready\n\
    if % = 3 goto error\n\
    if % = 4 goto error\n\
  :error\n\
    print $s,\" ***SIM ERROR***\n\"\n\
    print \"Check device port configuration.\nCheck SIM is inserted\nTest SIM in a mobile phone?\n\"\n\
    exit 1\n\
  :getpin\n\
    #handle case where original Vodafone 3G generates wrong response\n\
    waitfor 1 \"2\"\n\
    if % = 0 goto ready\n\
    print \"\nEnter PIN number: \"\n\
    input $x\n\
    let a=len($x)\n\
    if a<>5 goto getpin\n\
    let c=0\n\
  :test\n\
    let $c=$mid($x,c,1)\n\
    if $c<\"0\" goto getpin\n\
    if $c>\"9\" goto getpin\n\
    inc c\n\
    if c<4 goto test\n\
    let a=val($x)\n\
    if a<0 goto getpin\n\
    if a>9999 goto getpin\n\
    let $c=$left($x,4)\n\
  :enterpin\n\
    send \"AT+CPIN=\\\"\"\n\
    send $c\n\
    send \"\\\"^m\"\n\
    waitfor 20 \"OK\",\"ERR\"\n\
    if % = -1 goto timeerror\n\
    if % = 0 goto cont\n\
    if % = 1 goto pinerror\n\
  :pinerror\n\
  :timeerror\n\
    print \"ERROR entering PIN code\n\"\n\
    print \"Caution! - entering the wrong PIN code three times will lock the SIM\n\"\n\
    exit 1\n\
  :ready\n\
    print \"SIM ready\n\"\n\
  :cont\n\
    print \"Waiting for Registration..(120 sec max)\"\n\
    let c = 0\n\
  :waitreg\n\
    send \"AT+CREG?^m\"\n\
    waitfor 2 \"+CREG: 0,1\",\"+CREG: 0,5\"\n\
    if % = -1 goto regagain\n\
    if % = 0 goto homereg\n\
    if % = 1 goto roamreg\n\
  :regagain\n\
    if c > 120 goto regtimeout\n\
    let c=c+2\n\
    print \".\"\n\
    goto waitreg\n\
  :regtimeout\n\
    print \"\nFailed to register\n\"\n\
    exit 1\n\
  :homereg\n\
    print \"\nRegistered on Home network: \"\n\
    goto registered\n\
  :roamreg\n\
    print \"\nRegistered on Roaming network: \"\n\
    goto registered\n\
  :registered\n\
    waitquiet 1 0.1\n\
    send \"AT+COPS?^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let a=len($s)\n\
    let b=a-12\n\
    if b < 1 goto regtimeout\n\
    let $c=$right($s,b)\n\
    print $c,\"\n\"\n\
    let c=0\n\
  :signal\n\
    waitquiet 1 0.1\n\
    send \"AT+CSQ^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let a=len($s)\n\
    let a=a-6\n\
    let $s=$right($s,a)\n\
    if $s <> \"0,0\" goto sigcont\n\
    if c > 3 goto sigexit\n\
    let c=c+1\n\
    pause 1\n\
    goto signal\n\
  :sigexit\n\
    print \"Signal strength measure 0,0 too low!\"\n\
    exit 1\n\
  :sigcont\n\
    print \"Signal Quality:\",$s,\"\\n\"\n\
    waitquiet 1 0.1\n";

char _info_code[]  =
    "print \"##### Wireless WAN Modem Configuration #####\\n\"\n\
    opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 2 0.5\n\
  :name\n\
    print \"Product text:\n====\n\"\n\
    send \"ATi^m\"\n\
    get 1 \" ^m\" $s\n\
  :more\n\
    get 1 \"^m\" $s\n\
    let x=len($s)\n\
    if $s=\"OK\n\" goto manf\n\
    #dump\n\
    if x<1 goto manf\n\
    if $s=\"\n\" goto more\n\
    print $s\n\
    goto more\n\
  :manf\n\
    print \"\n====\n\"\n\
    waitquiet 2 0.1\n\
    print \"Manufacturer:           \"\n\
    send \"AT+cgmi^m\"\n\
    get 1 \"^m\" $s\n\
    get 1 \"^m\" $s\n\
    let x=len($s)\n\
    if x<1 goto imei_serial\n\
    dec x\n\
    let $s=$right($s,x)\n\
    print $s,\"\\n\"\n\
  :imei_serial\n\
    waitquiet 5 0.1\n\
    print \"IMEI and Serial Number: \"\n\
    send \"AT+GSN^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let x=len($s)\n\
    dec x\n\
    let $s=$right($s,x)\n\
    print ,$s,\"\\n\"\n\
  :firmware\n\
    waitquiet 5 0.1\n\
    print \"Manufacturer\'s Revision: \"\n\
    send \"AT+GMR^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let x=len($s)\n\
    dec x\n\
    dec x\n\
    let $s=$left($s,x)\n\
    print $s,\"\\n\"\n\
  :hardware\n\
    waitquiet 5 0.1\n\
    print \"Hardware Revision:      \"\n\
    send \"AT_OHWV^m\"\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    print $s,\"\\n\"\n\
  :networklock\n\
    waitquiet 5 0.1\n\
    print \"Network Locked:         \"\n\
    send \"AT+clck=\\\"PN\\\",2^m\"\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    print $s,\"\\n\"\n\
    waitquiet 5 0.1\n\
  :customized\n\
    print \"Customisation:          \"\n\
    send \"AT_ocst^m\"\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    get 2 \",^m\" $s\n\
    print $s,\"\\n\"\n\
  :bandsettings\n\
    waitquiet 5 0.1\n\
    print \"Band settings:          \"\n\
    send \"AT_OSBM?^m\"\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    if $s=\"4\" print \"Europe 900/1800MHz \"\n\
    if $s=\"5\" print \"USA 900/1900MHz \"\n\
    print \"(\",$s,\")\\n\" \n\
  :apn\n\
    waitquiet 5 0.1\n\
    print \"APN:                    \"\n\
    send \"AT+CGDCONT?^m\"\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    get 2 \" ^m\" $s\n\
    print $s,\"\\n\" \n\
    waitquiet 5 0.1  \n\
    print \"##### END #####\\n\"" ;


char _USA_code[]=
    "print \"##### Band Change to USA operation #####\\n\"\n\
    opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 2 0.5\n\
    send \"AT_OSBM=5^m\"\n\
    waitfor 10 \"OK\",\"ERR\"\n\
    if % = -1 goto timeout\n\
    if % = 0 goto cont\n\
    if % = 1 goto error\n\
  :timeout\n\
    print \"Timeout Error communicating with device.\n\"\n\
    exit 1\n\
  :error\n\
    print \"Error response from device.\n\"\n\
    exit 1\n\
  :cont\n\
    print \"Complete\\n\"\n";

char _Europe_code[]  =
    "print \"##### Band Change to European operation #####\\n\"\n\
    opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 2 0.5\n\
    send \"AT_OSBM=4^m\"\n\
    waitfor 10 \"OK\",\"ERR\"\n\
    if % = -1 goto timeout\n\
    if % = 0 goto cont\n\
    if % = 1 goto error\n\
  :timeout\n\
    print \"Timeout Error communicating with device.\n\"\n\
    exit 1\n\
  :error\n\
    print \"Error response from device.\n\"\n\
    exit 1\n\
  :cont\n\
    print \"Complete\\n\"\n";

char _sig_code[]  =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 2 0.5\n\
    let c=0\n\
  :signal\n\
    waitquiet 1 0.1\n\
    send \"AT+CSQ^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let a=len($s)\n\
    let a=a-6\n\
    let $s=$right($s,a)\n\
    if $s <> \"0,0\" goto sigcont\n\
    if c > 3 goto sigexit\n\
    let c=c+1\n\
    pause 1\n\
    goto signal\n\
  :sigexit\n\
    print \"Signal strength measure 0,0 too low!\"\n\
    exit 1\n\
  :sigcont\n\
    print \"Signal Quality:\",$s,\"\\n\"\n\
    waitquiet 1 0.1\n\
    exit 0\n";

char _reg_code[]  =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 2 0.5\n\
    print \"Waiting for Registration\"\n\
    let c = 0\n\
  :waitreg\n\
    send \"AT+CREG?^m\"\n\
    waitfor 2 \"+CREG: 0,1\",\"+CREG: 0,5\"\n\
    if % = -1 goto regagain\n\
    if % = 0 goto homereg\n\
    if % = 1 goto roamreg\n\
  :regagain\n\
    if c > 120 goto regtimeout\n\
    let c=c+2\n\
    print \".\"\n\
    goto waitreg\n\
  :regtimeout\n\
    print \"\nFailed to register\n\"\n\
    exit 1\n\
  :homereg\n\
    print \"\nRegistered on Home network: \"\n\
    goto registered\n\
  :roamreg\n\
    print \"\nRegistered on Roaming network: \"\n\
    goto registered\n\
  :registered\n\
    waitquiet 1 0.1\n\
    send \"AT+COPS?^m\"\n\
    get 2 \"^m\" $s\n\
    get 2 \"^m\" $s\n\
    let a=len($s)\n\
    let b=a-12\n\
    if b < 1 goto regtimeout\n\
    let $c=$right($s,b)\n\
    print $c,\"\n\"\n";

char _3G2G_mode_code[] =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 1 0.2\n\
    send \"AT_OPSYS=3^m\"\n\
    waitfor 10 \"OK\",\"ERR\"\n\
    if % = -1 goto timeout\n\
    if % = 0 goto cont\n\
    if % = 1 goto error\n\
  :timeout\n\
    print \"Timeout Error communicating with device.\n\"\n\
    exit 1\n\
  :error\n\
    print \"Error response from device.\n\"\n\
    exit 1\n\
  :cont\n\
    print \"Set 3G preferred mode\\n\"\n";


char _3G_mode_code[] =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 1 0.2\n\
    send \"AT_OPSYS=1^m\"\n\
    waitfor 10 \"OK\",\"ERR\"\n\
    if % = -1 goto timeout\n\
    if % = 0 goto cont\n\
    if % = 1 goto error\n\
  :timeout\n\
    print \"Timeout Error communicating with device.\n\"\n\
    exit 1\n\
  :error\n\
    print \"Error response from device.\n\"\n\
    exit 1\n\
  :cont\n\
    print \"Set 3G only mode\\n\"\n";

char _2G_mode_code[] =
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 1 0.2\n\
    send \"AT_OPSYS=0^m\"\n\
    waitfor 10 \"OK\",\"ERR\"\n\
    if % = -1 goto timeout\n\
    if % = 0 goto cont\n\
    if % = 1 goto error\n\
  :timeout\n\
    print \"Timeout Error communicating with device.\n\"\n\
    exit 1\n\
  :error\n\
    print \"Error response from device.\n\"\n\
    exit 1\n\
  :cont\n\
    print \"Set 2G only mode\\n\"\n";

char _GTEDGE_code[] =
    "opengt\n\
    set com 57600n81\n\
    send \"AT+CFUN=1^m\"\n\
    waitquiet 5 0.2";

char _SETPIN_code[]=
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 1 0.2\n\
  :start\n\
    flash 0.1\n\
    send \"AT+CPIN?^m\"\n\
    waitfor 30 \"SIM PUK\",\"SIM PIN\",\"READY\",\"ERROR\",\"ERR\"\n\
    if % = -1 goto error\n\
    if % = 0 goto ready\n\
    if % = 1 goto getpin\n\
    if % = 2 goto ready\n\
    if % = 3 goto error\n\
    if % = 4 goto error\n\
  :error\n\
    print $s,\" ***SIM ERROR***\n\"\n\
    print \"Check device port configuration.\nCheck SIM is inserted\nTest SIM in a mobile phone?\n\"\n\
    exit 1\n\
  :getpin\n\
    #handle case where original Vodafone 3G generates wrong response\n\
    let $x=$env(\"COMGTPIN\")\n\
    let a=len($x)\n\
    if a=0 goto pinenverr\n\
    if a<>4 goto pinerror\n\
    let c=0\n\
  :test\n\
    let $c=$mid($x,c,1)\n\
    if $c<\"0\" goto pinerror\n\
    if $c>\"9\" goto pinerror\n\
    inc c\n\
    if c<4 goto test\n\
    let a=val($x)\n\
    if a<0 goto pinerror\n\
    if a>9999 goto pinerror\n\
    let $c=$left($x,4)\n\
  :enterpin\n\
    send \"AT+CPIN=\\\"\"\n\
    send $c\n\
    send \"\\\"^m\"\n\
    waitfor 20 \"OK\",\"ERR\"\n\
    if % = -1 goto timeerror\n\
    if % = 0 goto ready\n\
    if % = 1 goto pinerror\n\
  :pinenverr\n\
    print \"ERROR: The COMGTPIN env variable is not set\n\"\n\
    exit 1\n\
  :pinerror\n\
    print \"ERROR: PIN code must be 4 decimal digits only\n\"\n\
    print \"Caution! - entering the wrong PIN code three times will lock the SIM\n\"\n\
    exit 1\n\
  :timeerror\n\
    print \"ERROR: timeout, device did not respond to PIN command entry.\n\"\n\
    exit 1\n\
  :ready\n\
    print \"SIM ready\n\"\n";

char _SETAPN_code[]=
    "opengt\n\
    set com 115200n81\n\
    set senddelay 0.05\n\
    waitquiet 1 0.2\n\
  :start\n\
    flash 0.1\n\
  :getapn\n\
    let $x=$env(\"COMGTAPN\")\n\
    let a=len($x)\n\
    if a=0 goto apnerror\n\
    if a>32 goto apnerror\n\
  :enterapn\n\
    send \"AT+CGDCONT=1,\\\"IP\\\",\\\"\n\
    send $c\n\
    send \"\\\"^m\"\n\
    waitfor 20 \"OK\",\"ERR\"\n\
    if % = -1 goto timeerror\n\
    if % = 0 goto OK\n\
    if % = 1 goto apnerror\n\
  :apnerror\n\
    print \"ERROR entering APN\n\"\n\
    print \"The COMGTAPN env variable is not set\n\"\n\
    exit 1\n\
  :timeerror\n\
    print \"ERROR entering APN\n\"\n\
    print \"The device timeout.\n\"\n\
    exit 1\n\
  :OK\n\
    exit 0\n";


char _help_code[]  =\
    "print \"comgt Usage: \"\n\
    print \"comgt [options] [built in script]'|[external script]\n\n\"\n\
    print \"Built in scripts -\n\"\n\
    print \"  comgt [default]           Checks SIM status (requests PIN if required),\n\"\n\
    print \"                           registration and signal strength reported by\n\"\n\
    print \"                           datacard.\n\"\n\
    print \"  comgt info                Display configuration of datacard.\n\"\n\
    print \"  comgt sig                 Report Signal strength.\n\"\n\
    print \"  comgt reg                 Report Registration status.\n\"\n\
    print \"  comgt PIN                 Set PIN using COMGTPIN env variable.\n\"\n\
    print \"  comgt APN                 Set APN using COMGTAPN env variable.\n\"\n\
    print \"\n  Valid for GlobeTrotter GPRS only:\n\"\n\
    print \"      comgt USA                 Switch to 900/1900 MHz band for USA operation.\n\"\n\
    print \"      comgt Europe              Switch to 900/1800 MHz band for Europen operation.\n\"\n\
    print \"\n  Valid for GlobeTrotter EDGE and Combo EDGE only:\n\"\n\
    print \"      comgt GTEDGE             Switch on radio interface.\n\"\n\
    print \"\n  Valid for Vodafone 3G, GlobeTrotter Fusion, GT Max :\n\"\n\
    print \"      comgt 2G             2G networks only.\n\"\n\
    print \"      comgt 3G             3G networks only\n\"\n\
    print \"      comgt 3G2G           3G network preferred\n\"\n\
    print \"\nCommand line options (must be before script name) - \\n\"\n\
    print \"  -d device                Use alternative device. e.g -d /dev/ttyUSB0\n\"\n\
    print \"  -e                       Turn on communication echo.\n\"\n\
    print \"  -h                       Help.\n\"\n\
    print \"  -V                       Version.\n\"\n\
    print \"  -s                       Don't run internal 'default' script before running\n\"\n\
    print \"                           external script.\n\"\n\
    print \"  -t=\"\\n\"                    Set alternative line terminator (default=\"\\n\").\n\"\n\
    print \"  -v                       Verbose mode. Print lots of trace info.\n\"\n\
    print \"  -x                       Speed exchange. 115200 replaced by 57600.\n\"\n\
    print \"                           ***used for GlobeTrotter EDGE and Combo EDGE***\n\"\n\
    print \"NOTES:\ncomgt assumes that the GlobeTrotter device is /dev/modem (create a logical link\n\"\n\
    print \"to actual device or use -d switch). Unless you use the '-s' switch comgt will\n\"\n\
    print \"run the internal 'default' script first before running an external script file.\n\"\n" ;




char *get_code(char* name){
  if(strlen(name)==0) return(_default_code);
  if (strcmp(name,"default")==0) return (_default_code);
  if (strcmp(name,"help")==0) return (_help_code);
  if (strcmp(name,"info")==0) return (_info_code);
  if (strcmp(name,"USA")==0) return (_USA_code);
  if (strcmp(name,"Europe")==0) return (_Europe_code);
  if (strcmp(name,"sig")==0) return (_sig_code);
  if (strcmp(name,"reg")==0) return (_reg_code);
  if (strcmp(name,"GTEDGE")==0) return (_GTEDGE_code);
  if (strcmp(name,"2G")==0) return (_2G_mode_code);
  if (strcmp(name,"3G")==0) return (_3G_mode_code);
  if (strcmp(name,"3G2G")==0) return (_3G2G_mode_code);
  if (strcmp(name,"PIN")==0) return (_SETPIN_code);
  if (strcmp(name,"APN")==0) return (_SETAPN_code);
  return(NULL);
}

