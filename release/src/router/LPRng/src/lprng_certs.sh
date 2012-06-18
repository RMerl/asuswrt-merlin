#!/bin/sh
# Set up LPRng Certificate Authority
# Set up LPRng Server Certificate
# Set up LPRng Client Certificates
#
#  Requires: OpenSSL  (http://www.openssl.org)
#  Based on openssl-0.9.6c, but probably works with an
#
#  This script was stolen^H^H^H^H^H^H derived from the CA.sh script in the
#  ssleay/openssl distribution, and the cca.sh and sign.sh scripts in the
#  mod_ssl 2.8.8 distribution.
#  As noted:
##  CCA -- Trivial Client CA management for testing purposes
##  Copyright (c) 1998-2001 Ralf S. Engelschall, All Rights Reserved.
##  sign.sh -- Sign a SSL Certificate Request (CSR)
##  Copyright (c) 1998-2001 Ralf S. Engelschall, All Rights Reserved. 
#
# The LPRng SSL authentication uses the same general setup as does Apache
# and other systems:
#
#  $sysconfdir default: /etc/lpd/
#       - a directory where you have the SSL stuff
#   SSL_CA_FILE "/etc/lpd/ssl.ca/ca.crt"
#       ${sysconfdir}/lpd/ssl.ca/ca.crt
#       - CA file, i.e. - one file with lots of certs,
#         but we are going to assume that this is our signing cert
#   SSL_CA_KEY "/etc/lpd/ssl.ca/${CA_KEY}"
#       ${sysconfdir}/lpd/ssl.ca/${CA_KEY}
#       - private key for CA file, used to sign certs
#   SSL_SERVER_CERT "/etc/lpd/ssl.server/server.cert"
#       ${sysconfdir}/lpd/ssl.cert/server.cert
#       - server cert file
#   SSL_SERVER_PASSWORD_FILE "/etc/lpd/ssl.server/server.pwd"
#       ${sysconfdir}/lpd/ssl.cert/server.pwd
#       - server private key password in this file
#
#
#  Use:
#    lprng_cert init                 - sets up directory structure
#    lprng_cert newca                - creates CA certs
#    lprng_cert gen                  - generates certificates
#    lprng_cert verify path_to_cert  - verifies certificates
#    lprng_cert defaults             - sets/examines defaults
#    lprng_cert encrypt path_to_key  - encrypts/decrypts key file
#    lprng_cert index                - make certificate index files
#
# we force the location of the CERTS to be whereever we want
# no redeeming social value to this stuff;

#   some optional terminal sequences
case $TERM in
    xterm|xterm*|vt220|vt220*)
        T_MD=`echo dummy | awk '{ printf("%c%c%c%c", 27, 91, 49, 109); }'`
        T_ME=`echo dummy | awk '{ printf("%c%c%c", 27, 91, 109); }'`
        ;;
    vt100|vt100*)
        T_MD=`echo dummy | awk '{ printf("%c%c%c%c%c%c", 27, 91, 49, 109, 0, 0); }'`
        T_ME=`echo dummy | awk '{ printf("%c%c%c%c%c", 27, 91, 109, 0, 0); }'`
        ;;
    default)
        T_MD=''
        T_ME=''
        ;;
esac

# announce what you are doing
cat <<EOF
${T_MD}lprng_certs -- LPRng SSL Certificate Management${T_ME}
Copyright (c) 2002 Patrick Powell
Based on CCA by Ralf S. Engelschall
(Copyright (c) 1998-2001 Ralf S. Engelschall, All Rights Reserved.)

EOF

#----------------- FUNCTIONS -----------------------

# general purpose failure function
usage() {
	cat >&2 <<EOF
${T_MD}usage: $0 [--TEMP=dir] option${T_ME}
${T_MD}  init             - make directory structure${T_ME}
${T_MD}  newca            - make new root CA and default values for certs${T_ME}
${T_MD}  defaults         - set new default values for certs${T_ME}
${T_MD}  encrypt keyfile  - set or change password on private key file${T_ME}
${T_MD}  gen              - generate user, server, or signing cert${T_ME}
${T_MD}  verify [cert]    - verify cert file${T_ME}
${T_MD}  index [dir]      - make certificate index files in directory dir${T_ME}
${T_MD}  --TEMP=dir       - specify directory for test operations${T_ME}

EOF
	rm -f ${CFG}
    exit 1
}

# ask - prompt with name and default:
#  ask "1. ..." "XY" var
ask() {
	eval v="\$$2"
	echo -n "$n. $1 [default '$v'] "
	read VAR
	if [ "$VAR" = "" ] ; then
		VAR="$v";
	fi
	eval $2=\"$VAR\"
	export $2
	names="$names $2"
	n=`expr $n + 1`
}

#
# set and/or get the new values for defaults
#

defaults() {
	finished=0
	while [ "$finished" = "0" ] ; do
		names=
		n=1
		export n
		ask "Country Name             (2 letter code, C)    " C_val
		ask "State or Province Name   (full name, ST)       " ST_val
		ask "Locality Name            (eg, city, L)         " L_val
		ask "Organization Name        (eg, company, O)      " O_val
		ask "Organizational Unit Name for CA     (eg, section, OU)"  OU_ca_val
		ask "Organizational Unit Name for Signer (eg, section, OU)"  OU_signer_val
		ask "Organizational Unit Name for Server (eg, section, OU)"  OU_server_val
		ask "Organizational Unit Name for User   (eg, section, OU)"  OU_user_val
		ask "Common Name for CA       (eg, CA name, CN)     "  CN_ca_val
		ask "Common Name for Signer   (eg, signer name, CN) "  CN_signer_val
		ask "Common Name for Server   (eg, server name, CN) "  CN_server_val
		ask "Common Name for User     (eg, user name, CN)   "  CN_user_val
		ask "Email Address            (eg, name@FQDN, Email)"  Email_val
		ask "CA Certificate Validity in days                "  Validity_ca_val
		ask "Signer Certificate Validity in days            "  Validity_signer_val
		ask "Server Certificate Validity in days            "  Validity_server_val
		ask "User Certificate Validity in days              "  Validity_user_val
		ask "Signer Certificate Path  (blank indicates CA signs)"  Signer_cert_path
		ask "Signer Private Key File (blank indicates key in cert file)"  Signer_key_path
		ask "Created Certificates Directory (blank indicates default $CA_USER_CERTS)   "  Cert_dir
		ask "Revoked Certificates File (blank indicates default $CA_CRL_FILE)   "  Revoke_file
		for i in $names ; do
			eval v="\$$i"
			echo $i $v
		done
		echo -n "save new values? [No/yes/again (No default)] "
		read VAR
		case "$VAR" in
			[yY]* )
				cp /dev/null ${CA_DEFAULTS}
				for i in $names ; do
					eval v="\$$i"
					echo $i=\"$v\" >>${CA_DEFAULTS}
				done
				finished=1;
				break;
				;;
			[nN]* | "" )
				finished=1;
				break;
				;;
			*) ;;
		esac
	done
}

#
# encrypt the certificate key
#
encrypt() {
	cat <<EOF
______________________________________________________________________

${T_MD}${STEP}Enrypting RSA private key $1 with a pass phrase for security${T_ME}

The contents of the certificate key file (the generated private key)
should be echo kept secret, especially so if it is used to sign
Certificates or for User authentication.
  SSL experts strongly recommend you to encrypt the key file with
a Triple-DES cipher and a Pass Phrase.  When using LPRng, you provide
the password via a file specified by the LPR_SSL_PASSWORD
environent variable, or in the ${HOME}/.lpr/client.pwd file.
The LPD server uses the ssl_server_password_file option to specify
the location of a file containing the password.

EOF
	
	if [ "$1" = "" -o ! -f $1 ] ; then
		echo "The file '$1' does not exist! Cannot encrypt it";
		exit 1;
	fi
	while [ 1 ]; do
		echo -n "Encrypt the private key now? [Y/n]: "
		read VAR
		case "$VAR" in
			[yY] | "" ) VAR=YES; break;;
			[nN] ) VAR=NO; break;;
		esac
	done
	if [ "$VAR" = YES ]; then
		while [ 1 ] ; do
		(umask 077; ${OPENSSL} rsa -des3 -in ${1} -out ${1}.enc)
			if [ $? -ne 0 ] ; then
				echo "Failed to encrypt RSA private key" 1>&2
				echo "You may need to renter the password" 1>&2
		else
				break;
			fi
		done
		(umask 077; cp ${1}.enc ${1}; rm -f ${1}.enc )
		echo "Fine, you're using an encrypted private key to sign CERTS."
		ENCRYPTED=YES
	else
		echo "Warning, you're using an unencrypted private key for signing CERTS."
		echo "Make sure that the key is VERY well protected"
	fi
}

# help for signing
shelp() {
	cat <<EOF
You can select the type of Certificate based on the Netscape Model:
USER: can only send jobs to server
SERVER: can accept jobs and forward them.  Needs to act as both
  client and server
SIGNING AUTHORITY: creates a certificate to be used for signing
  other certificates.  Note: you can select certificate to be used
  for signing.
EOF
}

# set values of various files based on defaults
#

set_values () {
	CA_RND=${CA_DIR}/ca.rnd
	CA_SER=${CA_DIR}/ca.ser
	CA_CSR=${CA_DIR}/ca.csr
	CA_DEFAULTS=${CA_DIR}/ca.defaults
	if [ "$Cert_dir" != "" ] ; then
		CA_USER_CERTS=$Cert_dir
	fi
	if [ "$Revoke_file" != "" ] ; then
		CA_CRL_FILE=$Revoke_file
	fi
}

index_certs() {
	( cd $1;
	#set -x
	echo "Indexing " `pwd`
	rm -f *.[0-9]*
	for file in *.crt; do
		if [ ".`grep SKIPME $file`" != . ]; then
			echo dummy | awk '{ printf("%-15s ... Skipped\n", file); }' "file=$file";
		else
			n=0;
			while [ 1 ]; do
				hash="`${OPENSSL} x509 -noout -hash <$file`";
				if [ -r "$hash.$n" ]; then
					n=`expr $n + 1`;
				else
					echo dummy | awk '{ printf("%-15s ... %s\n", file, hash); }' "file=$file" "hash=$hash.$n";
					ln -s $file $hash.$n;
					break;
				fi;
			done;
		fi;
	done
	)
}

make_cfg() {
    cat >${CFG} <<EOT
[ req ]
default_bits                    = 1024
distinguished_name              = req_DN
RANDFILE                        = ${CA_RND}
[ req_DN ]
countryName                     = "1. Country Name            (2 letter code, C) "
countryName_default             = ${C_val}
countryName_min                 = 2
countryName_max                 = 2
stateOrProvinceName             = "2. State or Province Name     (full name, ST) "
stateOrProvinceName_default     = ${ST_val}
localityName                    = "3. Locality Name                (eg, city, L) "
localityName_default            = ${L_val}
0.organizationName              = "4. Organization Name         (eg, company, 0) "
0.organizationName_default      = ${O_val}
organizationalUnitName          = "5. Organizational Unit Name (eg, section, OU) "
organizationalUnitName_default  = ${OU_val}
commonName                      = "6. Common Name           (eg, $TYPE name, CN) "
commonName_max                  = 64
commonName_default              = ${CN_val}
emailAddress                    = "7. Email Address       (eg, name@FQDN, Email) "
emailAddress_max                = 40
emailAddress_default            = ${Email_val}
EOT
}

init_ca_dirs() {
    # if directories do not exist then setup the directory
	for i in ${CA_DIR} ${CA_USER_CERTS} ;  do
		d=$i;
		if [ "$d" != "" -a ! -d "${d}" ] ; then
			# create the directory hierarchy
			echo "creating $d"
			mkdir -p "${d}" 
			if [ ! -d "${d}" ] ; then
				echo "cannot create directory $d";
				exit 1
			fi
		fi
	done
}

#--------------------- START OF MAIN BODY -----------------

if [ "$#" = 0 ] ; then usage; fi

# set default values


CFG=/tmp/$$.sslcfg

OPENSSL=@OPENSSL@
CA_KEY=@SSL_CA_KEY@
CA_CERT=@SSL_CA_FILE@
CA_USER_CERTS=@SSL_CERTS_DIR@
CA_CRL_FILE=@SSL_CRL_FILE@
SSL_SERVER_CERT=@SSL_SERVER_CERT@

C_val="XY"
ST_val="Snake Desert"
L_val="Snake Town"
O_val="Snake Oil, Ltd"
OU_ca_val="CA"
OU_signer_val="Signer"
OU_server_val="Server"
OU_user_val="User"
CN_ca_val="Snake Oil CA"
CN_server_val="PrintServer Name"
CN_signer_val="Signer Name"
CN_user_val="John Q. User"
Validity_ca_val=365
Validity_signer_val=365
Validity_user_val=365
Validity_server_val=365
Email_val="name@snakeoil.dom"

case "$1" in
	--TEMP=* )
		s=`expr "$1" : "--TEMP=\(.*\)"`
		CA_DIR="$s/ssl.ca"
		CA_CERT="$s/ssl.ca/ca.crt"
		CA_KEY="$s/ssl.ca/ca.key"
		CA_USER_CERTS="$s/ssl.certs"
		CA_CRL_FILE="$s/certs.crl"
		DIRS=""
		shift
		;;
esac

if [ "$CA_DIR" = "" ] ; then
	CA_DIR=`dirname ${CA_CERT}`
fi
CA_DEFAULTS=${CA_DIR}/ca.defaults

# if you have a defaults file, then read it
if [ -f "${CA_DEFAULTS}" ] ; then
. ${CA_DEFAULTS}
fi
set_values

#   find some random files
#   (do not use /dev/random here, because this device 
#   doesn't work as expected on all platforms)
randfiles=''
for file in /var/log/messages /var/adm/messages \
            /kernel /vmunix /vmlinuz \
            /etc/hosts /etc/resolv.conf; do
    if [ -f $file ]; then
        if [ ".$randfiles" = . ]; then
            randfiles="$file"
        else
            randfiles="${randfiles}:$file"
        fi
    fi
done

case $1 in
init )
    # if directories do not exist then setup the directory
	init_ca_dirs
	;;

newca)
    # if directories do not exist then setup the directory
	init_ca_dirs
	if [ -f ${CA_CERT} ] ; then
		while [ 1 ] ; do
			echo -n "WARNING: ${CA_CERT} already exists! Do you want to overwrite it? [N/y]"
			read VAR
			case "$VAR" in
			[Yy]* ) break ;;
			* ) exit 1
			esac
		done
	fi
    echo "${T_MD}INITIALIZATION - SET DEFAULTS in ${CA_DEFAULTS} ${T_ME}"
    echo ""
	if [ -f ${CA_DEFAULTS} ] ; then touch ${CA_DEFAULTS} ; fi
	defaults
	set_values
    echo "${T_MD}Generating custom Certificate Authority (CA)${T_ME}"
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 1: Generating RSA private key for CA (1024 bit)${T_ME}"
    cp /dev/null ${CA_RND}
    echo '02' >${CA_SER}
    if [ ".$randfiles" != . ]; then
        ${OPENSSL} genrsa -rand $randfiles -out ${CA_KEY} 1024
    else
        ${OPENSSL} genrsa -out ${CA_KEY} 1024
    fi
    if [ $? -ne 0 ]; then
        echo "$0: Error: Failed to generate RSA private key" 1>&2
		rm -f ${CFG}
        exit 1
    fi
	TYPE="ca"
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 2: Generating X.509 certificate signing request for CA${T_ME}"
	eval CN_val=\"\$CN_${TYPE}_val\"
	eval OU_val=\"\$OU_${TYPE}_val\"
	eval Validity_val=\"\$Validity_${TYPE}_val\"
	make_cfg
	#cat ${CFG}
    ${OPENSSL} req -config ${CFG} -new -key ${CA_KEY} -out ${CA_CSR}
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate certificate signing request" 1>&2
		rm -f ${CFG}
        exit 1
    fi
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 3: Generating X.509 certificate for CA signed by itself${T_ME}"
    cat >${CFG} <<EOT
extensions = x509v3
[ x509v3 ]
subjectAltName   = email:copy
basicConstraints = CA:true,pathlen:0
nsComment        = "lprng_cert generated custom CA certificate"
# nsCertType       = sslCA,emailCA,objsign,email ---  we will leave this out, see openssl/doc/openssl.txt
nsCertType=     sslCA,emailCA,objsign,email
EOT
    ${OPENSSL} x509 -extfile ${CFG} -req -days ${Validity_ca_val} -signkey ${CA_KEY} -in ${CA_CSR} -out ${CA_CERT}
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate self-signed CA certificate" 1>&2
		rm -f ${CFG}
        exit 1
    fi
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}RESULT:${T_ME}"
    ${OPENSSL} verify ${CA_CERT}
    if [ $? -ne 0 ]; then
        echo "Error: Failed to verify resulting X.509 certificate" 1>&2
		rm -f ${CFG}
        exit 1
    fi
	STEP="STEP 4. " encrypt ${CA_KEY}
    echo "______________________________________________________________________"
    echo ""

	if [ "$ENCRYPTED" = YES ] ; then
		echo "${T_MD}STEP 5: Combine CERT and KEY file${T_ME}"
		echo "Generate single CERT and KEY file? [N/y] ";
		read VAR
		case "$VAR" in
			[yY]* )
				(umask 077;
				cat ${CA_KEY} ${CA_CERT} \
					> ${CA_CERT}.combined
				mv ${CA_CERT}.combined ${CA_CERT}
				rm ${CA_KEY}
				)
				CA_KEY="${CA_CERT}"
				;;
		esac
	fi
	index_certs ${CA_DIR}

	echo ""
	echo "Use the following commands to examine the CERT and KEY files:" 
    echo "   openssl x509 -text -in ${CA_CERT}"
    echo "   openssl rsa -text -in ${CA_KEY}"
	;;

defaults )
	echo "______________________________________________________________________"
	echo ""
	echo "${T_MD}${STEP}Setting Default Values${T_ME}"
	defaults;
	;;

encrypt )
	shift
	if [ "$1" = "" ] ; then usage; fi;
	if [ ! -f "$1" ] ; then useage; fi;
	sed -n -e '/BEGIN.*PRIVATE KEY/,/END.*PRIVATE KEY/p' $1 >/tmp/$$.key
	sed -e '/BEGIN.*PRIVATE KEY/,/END.*PRIVATE KEY/d' $1 >/tmp/$$.crt
	STEP="" encrypt /tmp/$$.key
	status=$?
	echo STATUS $status
	if [ $status = 0 ] ; then
		mv $1 $1.orig
		cat /tmp/$$.crt /tmp/$$.key >$1
	fi
    ;;

gen )
    echo "${T_MD}CERTIFICATE GENERATION${T_ME}"
	COMBINE="Y/n"

	while [ 1 ] ; do
		echo -n "What type of certificate? User/Server/Signing Authority/Help? [u/s/a/H] "
		read VAR
		case "$VAR" in
			[uU] ) TYPE=user; nsCertType="client,email";
					if [ "$Signer_cert_path" != "" ] ; then
						CA_CERT=$Signer_cert_path;
						CA_KEY=$Signer_key_path;
					fi
					break;;
			[sS] ) TYPE=server; nsCertType="client,server,email";
					if [ "$Signer_cert_path" != "" ] ; then
						CA_CERT=$Signer_cert_path;
						CA_KEY=$Signer_key_path;
					fi
					break;;
			[Aa] ) TYPE=signer; nsCertType="objsign";
					CA_USER_CERTS=$CA_DIR;
					CA_SER=$CA_DIR/ca.ser
					COMBINE="N/y"
					break;;
			*) shelp; ;;
		esac
	done

	while [ 1 ] ; do
		echo -n "Create in '$CA_USER_CERTS' [return for yes, or specify directory] "
		read VAR
		case "$VAR" in
			"" ) break ;;
			*) 	CA_USER_CERTS=$VAR;
				CA_SER=$VAR/ca.ser
			;;
		esac
	done
	
	if [ ! -d ${CA_USER_CERTS} ] ; then
		mkdir -p ${CA_USER_CERTS}
	fi
	if [ ! -f $CA_SER ] ; then
		echo 1 > $CA_SER;
	fi

	user="$TYPE-`cat ${CA_SER}`"
	while [ 1 ] ; do
		echo -n "CERT name '$user'? [return for yes, or specify name] "
		read VAR
		case "$VAR" in
			"" ) break ;;
			*) 	user="$VAR"
			;;
		esac
	done

	echo "Creating $user in $CA_USER_CERTS" 

	CERT=$CA_CERT;
	KEY=$CA_KEY;
	while [ 1 ] ; do
		if [ ! -f "$CERT" ] ; then
			d=`dirname $CERT`;
			if [ "$d" = "." -o "$d" = "" ] ; then d="${CA_DIR}/"; fi
			if [ -f $d$CERT ] ; then CERT=$d$CERT; fi
			if [ -f $d${CERT}.crt ] ; then CERT=$d${CERT}.crt; fi
		fi
		echo -n "Sign with Certificate '$CERT' [return for yes, ? for list, or specify cert file] "
		read VAR
		case "$VAR" in
			"" )
				if [ ! -f "$CERT" ] ; then
					echo "cert file '$CERT' does not exist" 
					continue
				fi
				break;
				;;
			"?" )
				d=`dirname $CERT`;
				echo "Possible CERTS in directory '$d' are:"
				ls $d/*.crt 2>/dev/null
				;;
			*)
			if [ ! -f "$VAR" ] ; then
				if [ -f "$VAR.crt" ] ; then
					VAR="$VAR.crt"
				fi
				d=`dirname $CERT`;
				v=
				for i in `ls $d/*.crt 2>/dev/null | grep $VAR` ; do
					echo "Match Found $i"
					if [ "$v" = "" ] ; then v="$i" ; else v="$v $i"; fi
				done
				if [ "$v" = "" ] ; then
					echo "Possible CERTS in directory '$d' are:"
					ls $d/*.crt 2>/dev/null
				elif [ -f "$v" ] ; then
					VAR=$v
				else
					continue
				fi
			fi
			CERT="$VAR"; KEY="$VAR"
			;;
		esac
	done

	if [ "`grep 'PRIVATE KEY' $CERT`" != "" ] ; then
		echo Private key in $CERT
		KEY=$CERT
	else
		while [ 1 ] ; do
			echo -n "Private key file '$KEY' [return for yes, or specify path to key file] "
			read VAR
			case "$VAR" in
				"" )
					if [ ! -f "$KEY" ] ; then
						echo "file '$KEY' does not exist" 
						continue
					fi
					break;
					;;
				*)
				if [ ! -f "$VAR" ] ; then
					echo "file '$VAR' does not exist" 
					d=`dirname $CERT`;
					if [ -f $d/$VAR ] ; then
						VAR=$d/$VAR;
						echo "Trying $VAR"
					else
						continue
					fi
				fi
				KEY="$VAR"
				;;
			esac
		done
	fi

    echo ""
    echo "${T_MD}Generating ${TYPE} Certificate [$user] ${T_ME}"
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 1: Generating RSA private key for ${TYPE} (1024 bit)${T_ME}"
    if [ ".$randfiles" != . ]; then
        ${OPENSSL} genrsa -rand $randfiles -out ${CA_USER_CERTS}/$user.key 1024
    else
        ${OPENSSL} genrsa -out ${CA_USER_CERTS}/$user.key 1024
    fi
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate RSA private key" 1>&2
		rm -f ${CFG}
        exit 1
    fi
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 2: Generating X.509 certificate signing request for ${TYPE}${T_ME}"
	eval CN_val=\"\$CN_${TYPE}_val\"
	eval OU_val=\"\$OU_${TYPE}_val\"
	eval Validity_val=\"\$Validity_${TYPE}_val\"
	make_cfg
    ${OPENSSL} req -config ${CFG} -new -key ${CA_USER_CERTS}/$user.key -out ${CA_USER_CERTS}/$user.csr
    if [ $? -ne 0 ]; then
        echo "Error: Failed to generate certificate signing request" 1>&2
		rm -f ${CFG}
        exit 1
    fi
	while [ 1 ] ; do
		echo -n "User Certificate Validity in days  [default $Validity_val] "
		read VAR
		case "$VAR" in
			"" ) VAR=$Validity_val;;
		esac
		v=`echo ${VAR} | sed -e 's/[0-9]//g'`
		if [ "$v" != "" -o "$VAR" = "" ] ; then
			echo "Must be integer value"
		else
			Validity_val=$VAR;
			break;
		fi
	done
    rm -f ${CFG}
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}STEP 3: Generating X.509 certificate signed by ${CERT}${T_ME}"
    cat >${CFG} <<EOT
extensions = x509v3
[ x509v3 ]
subjectAltName   = email:copy
basicConstraints = CA:false,pathlen:0
nsComment        = "lprng_cert generated $TYPE certificate"
# You can really go to town here, but remeber, the server certs
#  must be used for both client AND server operation, so it gets
#  a bit tricky specifying the type of cert
# for end users
# nsCertType       = client,email ---  we will leave this out, see openssl/doc/openssl.txt
# for servers
# nsCertType       = server,client,email ---  we will leave this out, see openssl/doc/openssl.txt
nsCertType= ${nsCertType}
EOT
	while [ 1 ] ; do
		${OPENSSL} x509 -extfile ${CFG} -days ${Validity_val} \
		-CAserial ${CA_SER} -CA ${CERT} -CAkey ${KEY} \
		-in ${CA_USER_CERTS}/$user.csr -req -out ${CA_USER_CERTS}/$user.crt
		if [ $? -ne 0 ] ; then
			echo "Error: Failed to generate X.509 certificate" 1>&2
			echo "try again? [Y/n] "
			read VAR
			case "$VAR" in
				[Yy]* | "" ) ;;
				* )
					rm -f ${CFG}
					exit 1
				;;
			esac
		else
			break;
		fi
	done
#    caname="`${OPENSSL} x509 -noout -text -in ${CERT} |\
#             grep Subject: | sed -e 's;.*CN=;;' -e 's;/Em.*;;'`"
#    username="`${OPENSSL} x509 -noout -text -in ${CA_USER_CERTS}/$user.crt |\
#               grep Subject: | sed -e 's;.*CN=;;' -e 's;/Em.*;;'`"
#    echo "Assembling PKCS#12 package"
#    ${OPENSSL} pkcs12 -export -in ${CA_USER_CERTS}/$user.crt \
#		-inkey ${CA_USER_CERTS}/$user.key -certfile ${CERT} \
#		 -name "$username" -caname "$caname" -out ${CA_USER_CERTS}/$user.p12
    echo "______________________________________________________________________"
    echo ""
    echo "${T_MD}RESULT:${T_ME}"
    ${OPENSSL} verify -CApath ${CA_DIR} -CAfile ${CERT} ${CA_USER_CERTS}/$user.crt
    if [ $? -ne 0 ]; then
        echo ": Failed to verify resulting X.509 certificate" 1>&2
		rm -f ${CFG}
        exit 1
    fi

	STEP="STEP 4. " encrypt  ${CA_USER_CERTS}/$user.key
    echo "______________________________________________________________________"
    echo ""

	ofile=key 
	if [ "$ENCRYPTED" = YES ] ; then
		echo "${T_MD}STEP 5: Combine CERT and KEY file${T_ME}"
		echo "Generate single CERT and KEY file? [$COMBINE] ";
		read VAR
		case "$VAR" in
			[yY]* | "" )
				if [ "$VAR" = "" -a "$COMBINE" = "N/y" ] ; then
					break;
				fi
				(umask 077;
				cat ${CA_USER_CERTS}/$user.key ${CA_USER_CERTS}/$user.crt \
					> ${CA_USER_CERTS}/$user.crt.combined
				mv ${CA_USER_CERTS}/$user.crt.combined \
					${CA_USER_CERTS}/$user.crt
				rm ${CA_USER_CERTS}/$user.key
				)
				ofile=crt
				;;
		esac
	fi

	if [ "${TYPE}" = "signer" ] ; then
		index_certs ${CA_USER_CERTS}
	fi

	echo ""
	echo "Use the following commands to examine the CERT and KEY files:" 
    echo "   openssl x509 -text -in ${CA_USER_CERTS}/$user.crt"
    echo "   openssl rsa -text -in ${CA_USER_CERTS}/$user.$ofile"

	;;

verify )
	shift
	if [ $# = 0 ] ; then
		v=`ls ${CA_USER_CERTS}/*.crt 2>/dev/null`
	else
		for i in $* ; do
			if [ -f $i ] ; then
				v="$v $i"
			elif [ -d $i ] ; then
				for j in $i/*.crt ; do
					v="$v $j"
				done
			fi
		done
	fi
	for i in $v ; do
		if [ -f $i ] ; then
			v=$i;
		elif [ -f $CA_USER_CERTS/$i ] ; then
			v=$CA_USER_CERTS/$i;
		elif [ -f $CA_USER_CERTS/$i.crt ] ; then
			v=$CA_USER_CERTS/$i.crt;
		elif [ -f $CA_DIR/$i ] ; then
			v=$CA_DIR/$i;
		elif [ -f $CA_DIR/$i.crt ] ; then
			v=$CA_DIR/$i.crt;
		else
			echo "cannot find $i in $CA_USER_CERTS or $CA_DIR";
			continue
		fi
		echo "cert: $v"
		${OPENSSL} x509 -in $v -subject -issuer -noout
	    ${OPENSSL} verify -CApath ${CA_DIR} $v
	done
	;;

index )
	shift
	if [ "$1" != "" ] ; then CA_DIR="$1" ; fi
	index_certs ${CA_DIR}
	;;


*)
    echo "Unknown request '$*'";
	usage;
	rm -f ${CFG}
    exit 1
    ;;
esac
rm -f ${CFG}
exit $RET
