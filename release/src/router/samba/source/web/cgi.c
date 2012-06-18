/* 
   some simple CGI helper routines
   Copyright (C) Andrew Tridgell 1997-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include "includes.h"
#include "smb.h"

#define MAX_VARIABLES 10000

/* set the expiry on fixed pages */
#define EXPIRY_TIME (60*60*24*7)

#define CGI_LOGGING 0

#ifdef DEBUG_COMMENTS
extern void print_title(char *fmt, ...);
#endif

struct var {
	char *name;
	char *value;
};

static struct var variables[MAX_VARIABLES];
static int num_variables;
static int content_length;
static int request_post;
static char *query_string;
static char *baseurl;
static char *pathinfo;
static char *C_user;
static BOOL inetd_server;
static BOOL got_request;

static void unescape(char *buf)
{
	char *p=buf;

	while ((p=strchr(p,'+')))
		*p = ' ';

	p = buf;

	while (p && *p && (p=strchr(p,'%'))) {
		int c1 = p[1];
		int c2 = p[2];

		if (c1 >= '0' && c1 <= '9')
			c1 = c1 - '0';
		else if (c1 >= 'A' && c1 <= 'F')
			c1 = 10 + c1 - 'A';
		else if (c1 >= 'a' && c1 <= 'f')
			c1 = 10 + c1 - 'a';
		else {p++; continue;}

		if (c2 >= '0' && c2 <= '9')
			c2 = c2 - '0';
		else if (c2 >= 'A' && c2 <= 'F')
			c2 = 10 + c2 - 'A';
		else if (c2 >= 'a' && c2 <= 'f')
			c2 = 10 + c2 - 'a';
		else {p++; continue;}
			
		*p = (c1<<4) | c2;

		memcpy(p+1, p+3, strlen(p+3)+1);
		p++;
	}
}


static char *grab_line(FILE *f, int *cl)
{
	char *ret;
	int i = 0;
	int len = 1024;

	ret = (char *)malloc(len);
	if (!ret) return NULL;
	

	while ((*cl)) {
		int c = fgetc(f);
		(*cl)--;

		if (c == EOF) {
			(*cl) = 0;
			break;
		}
		
		if (c == '\r') continue;

		if (strchr("\n&", c)) break;

		ret[i++] = c;

		if (i == len-1) {
			char *ret2;
			ret2 = (char *)realloc(ret, len*2);
			if (!ret2) return ret;
			len *= 2;
			ret = ret2;
		}
	}
	

	ret[i] = 0;
	return ret;
}

/***************************************************************************
  load all the variables passed to the CGI program. May have multiple variables
  with the same name and the same or different values. Takes a file parameter
  for simulating CGI invocation eg loading saved preferences.
  ***************************************************************************/
void cgi_load_variables(FILE *f1)
{
	FILE *f = f1;
	static char *line;
	char *p, *s, *tok;
	int len;

#ifdef DEBUG_COMMENTS
	char dummy[100]="";
	print_title(dummy);
	printf("<!== Start dump in cgi_load_variables() %s ==>\n",__FILE__);
#endif

	if (!f1) {
		f = stdin;
		if (!content_length) {
			p = getenv("CONTENT_LENGTH");
			len = p?atoi(p):0;
		} else {
			len = content_length;
		}
	} else {
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
	}


	if (len > 0 && 
	    (f1 || request_post ||
	     ((s=getenv("REQUEST_METHOD")) && 
	      strcasecmp(s,"POST")==0))) {
		while (len && (line=grab_line(f, &len))) {
			p = strchr(line,'=');
			if (!p) continue;
			
			*p = 0;
			
			variables[num_variables].name = strdup(line);
			variables[num_variables].value = strdup(p+1);

			free(line);
			
			if (!variables[num_variables].name || 
			    !variables[num_variables].value)
				continue;

			unescape(variables[num_variables].value);
			unescape(variables[num_variables].name);

#ifdef DEBUG_COMMENTS
			printf("<!== POST var %s has value \"%s\"  ==>\n",
			       variables[num_variables].name,
			       variables[num_variables].value);
#endif
			
			num_variables++;
			if (num_variables == MAX_VARIABLES) break;
		}
	}

	if (f1) {
#ifdef DEBUG_COMMENTS
	        printf("<!== End dump in cgi_load_variables() ==>\n"); 
#endif
		return;
	}

	fclose(stdin);
	(void)open("/dev/null", O_RDWR);

	if ((s=query_string) || (s=getenv("QUERY_STRING"))) {
		for (tok=strtok(s,"&;");tok;tok=strtok(NULL,"&;")) {
			p = strchr(tok,'=');
			if (!p) continue;
			
			*p = 0;
			
			variables[num_variables].name = strdup(tok);
			variables[num_variables].value = strdup(p+1);

			if (!variables[num_variables].name || 
			    !variables[num_variables].value)
				continue;

			unescape(variables[num_variables].value);
			unescape(variables[num_variables].name);

#ifdef DEBUG_COMMENTS
                        printf("<!== Commandline var %s has value \"%s\"  ==>\n",
                               variables[num_variables].name,
                               variables[num_variables].value);
#endif						
			num_variables++;
			if (num_variables == MAX_VARIABLES) break;
		}

	}
#ifdef DEBUG_COMMENTS
        printf("<!== End dump in cgi_load_variables() ==>\n");   
#endif
}


/***************************************************************************
  find a variable passed via CGI
  Doesn't quite do what you think in the case of POST text variables, because
  if they exist they might have a value of "" or even " ", depending on the 
  browser. Also doesn't allow for variables[] containing multiple variables
  with the same name and the same or different values.
  ***************************************************************************/
char *cgi_variable(char *name)
{
	int i;

	for (i=0;i<num_variables;i++)
		if (strcmp(variables[i].name, name) == 0)
			return variables[i].value;
	return NULL;
}

/***************************************************************************
tell a browser about a fatal error in the http processing
  ***************************************************************************/
static void cgi_setup_error(char *err, char *header, char *info)
{
	if (!got_request) {
		/* damn browsers don't like getting cut off before they give a request */
		char line[1024];
		while (fgets(line, sizeof(line)-1, stdin)) {
			if (strncasecmp(line,"GET ", 4)==0 || 
			    strncasecmp(line,"POST ", 5)==0 ||
			    strncasecmp(line,"PUT ", 4)==0) {
				break;
			}
		}
	}

	printf("HTTP/1.0 %s\r\n%sConnection: close\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>%s</TITLE></HEAD><BODY><H1>%s</H1>%s<p></BODY></HTML>\r\n\r\n", err, header, err, err, info);
	fclose(stdin);
	fclose(stdout);
	exit(0);
}


/***************************************************************************
tell a browser about a fatal authentication error
  ***************************************************************************/
static void cgi_auth_error(void)
{
	if (inetd_server) {
                cgi_setup_error("401 Authorization Required", 
                                "WWW-Authenticate: Basic realm=\"SWAT\"\r\n",
                                "You must be authenticated to use this service");
	} else {
		printf("Content-Type: text/html\r\n");

		printf("\r\n<HTML><HEAD><TITLE>SWAT</TITLE></HEAD>\n");
		printf("<BODY><H1>Installation Error</H1>\n");
		printf("SWAT must be installed via inetd. It cannot be run as a CGI script<p>\n");
		printf("</BODY></HTML>\r\n");
	}
	exit(0);
}


/***************************************************************************
decode a base64 string in-place - simple and slow algorithm
  ***************************************************************************/
static void base64_decode(char *s)
{
	char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int bit_offset, byte_offset, idx, i, n;
	unsigned char *d = (unsigned char *)s;
	char *p;

	n=i=0;

	while (*s && (p=strchr(b64,*s))) {
		idx = (int)(p - b64);
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		d[byte_offset] &= ~((1<<(8-bit_offset))-1);
		if (bit_offset < 3) {
			d[byte_offset] |= (idx << (2-bit_offset));
			n = byte_offset+1;
		} else {
			d[byte_offset] |= (idx >> (bit_offset-2));
			d[byte_offset+1] = 0;
			d[byte_offset+1] |= (idx << (8-(bit_offset-2))) & 0xFF;
			n = byte_offset+2;
		}
		s++; i++;
	}
	/* null terminate */
	d[n] = 0;
}


/***************************************************************************
handle a http authentication line
  ***************************************************************************/
static BOOL cgi_handle_authorization(char *line)
{
	char *p, *user, *user_pass;
	struct passwd *pass = NULL;
	BOOL ret = False;

	if (strncasecmp(line,"Basic ", 6)) {
		cgi_setup_error("401 Bad Authorization", "", 
				"Only basic authorization is understood");
		return False;
	}
	line += 6;
	while (line[0] == ' ') line++;
	base64_decode(line);
	if (!(p=strchr(line,':'))) {
		/*
		 * Always give the same error so a cracker
		 * cannot tell why we fail.
		 */
		cgi_setup_error("401 Bad Authorization", "", 
				"username/password must be supplied");
		return False;
	}
	*p = 0;
	user = line;
	user_pass = p+1;

	/*
	 * Try and get the user from the UNIX password file.
	 */

	if(!(pass = Get_Pwnam(user,False))) {
		/*
		 * Always give the same error so a cracker
		 * cannot tell why we fail.
		 */
		cgi_setup_error("401 Bad Authorization", "",
				"username/password must be supplied");
		return False;
	}

	/*
	 * Validate the password they have given.
	 */

	if((ret = pass_check(user, user_pass, strlen(user_pass), NULL, NULL)) == True) {

		/*
		 * Password was ok.
		 */

		if(pass->pw_uid != 0) {
			/*
			 * We have not authenticated as root,
			 * become the user *permanently*.
			 */
			become_user_permanently(pass->pw_uid, pass->pw_gid);
		}

		/* Save the users name */
		C_user = strdup(user);
	}

	return ret;
}

/***************************************************************************
is this root?
  ***************************************************************************/
BOOL am_root(void)
{
	if (geteuid() == 0) {
		return( True);
	} else {
		return( False);
	}
}

/***************************************************************************
return a ptr to the users name
  ***************************************************************************/
char *cgi_user_name(void)
{
        return(C_user);
}


/***************************************************************************
handle a file download
  ***************************************************************************/
static void cgi_download(char *file)
{
	SMB_STRUCT_STAT st;
	char buf[1024];
	int fd, l, i;
	char *p;

	/* sanitise the filename */
	for (i=0;file[i];i++) {
		if (!isalnum((int)file[i]) && !strchr("/.-_", file[i])) {
			cgi_setup_error("404 File Not Found","",
					"Illegal character in filename");
		}
	}

	if (!file_exist(file, &st)) {
		cgi_setup_error("404 File Not Found","",
				"The requested file was not found");
	}
	fd = sys_open(file,O_RDONLY,0);
	if (fd == -1) {
		cgi_setup_error("404 File Not Found","",
				"The requested file was not found");
	}
	printf("HTTP/1.0 200 OK\r\n");
	if ((p=strrchr(file,'.'))) {
		if (strcmp(p,".gif")==0) {
			printf("Content-Type: image/gif\r\n");
		} else if (strcmp(p,".jpg")==0) {
			printf("Content-Type: image/jpeg\r\n");
		} else {
			printf("Content-Type: text/html\r\n");
		}
	}
	printf("Expires: %s\r\n", http_timestring(time(NULL)+EXPIRY_TIME));

	printf("Content-Length: %d\r\n\r\n", (int)st.st_size);
	while ((l=read(fd,buf,sizeof(buf)))>0) {
		fwrite(buf, 1, l, stdout);
	}
	close(fd);
	exit(0);
}


/***************************************************************************
setup the cgi framework, handling the possability that this program is either
run as a true cgi program by a web browser or is itself a mini web server
  ***************************************************************************/
void cgi_setup(char *rootdir, int auth_required)
{
	BOOL authenticated = False;
	char line[1024];
	char *url=NULL;
	char *p;
#if CGI_LOGGING
	FILE *f;
#endif

	if (chdir(rootdir)) {
		cgi_setup_error("400 Server Error", "",
				"chdir failed - the server is not configured correctly");
	}

	/* maybe we are running under a web server */
	if (getenv("CONTENT_LENGTH") || getenv("REQUEST_METHOD")) {
		if (auth_required) {
			cgi_auth_error();
		}
		return;
	}

	inetd_server = True;

	if (!check_access(1, lp_hostsallow(-1), lp_hostsdeny(-1))) {
		cgi_setup_error("400 Server Error", "",
				"Samba is configured to deny access from this client\n<br>Check your \"hosts allow\" and \"hosts deny\" options in smb.conf ");
	}

#if CGI_LOGGING
	f = sys_fopen("/tmp/cgi.log", "a");
	if (f) fprintf(f,"\n[Date: %s   %s (%s)]\n", 
		       http_timestring(time(NULL)),
		       client_name(1), client_addr(1));
#endif

	/* we are a mini-web server. We need to read the request from stdin
	   and handle authentication etc */
	while (fgets(line, sizeof(line)-1, stdin)) {
#if CGI_LOGGING
		if (f) fputs(line, f);
#endif
		if (line[0] == '\r' || line[0] == '\n') break;
		if (strncasecmp(line,"GET ", 4)==0) {
			got_request = True;
			url = strdup(&line[4]);
		} else if (strncasecmp(line,"POST ", 5)==0) {
			got_request = True;
			request_post = 1;
			url = strdup(&line[5]);
		} else if (strncasecmp(line,"PUT ", 4)==0) {
			got_request = True;
			cgi_setup_error("400 Bad Request", "",
					"This server does not accept PUT requests");
		} else if (strncasecmp(line,"Authorization: ", 15)==0) {
			authenticated = cgi_handle_authorization(&line[15]);
		} else if (strncasecmp(line,"Content-Length: ", 16)==0) {
			content_length = atoi(&line[16]);
		}
		/* ignore all other requests! */
	}
#if CGI_LOGGING
	if (f) fclose(f);
#endif

	if (auth_required && !authenticated) {
		cgi_auth_error();
	}

	if (!url) {
		cgi_setup_error("400 Bad Request", "",
				"You must specify a GET or POST request");
	}

	/* trim the URL */
	if ((p = strchr(url,' ')) || (p=strchr(url,'\t'))) {
		*p = 0;
	}
	while (*url && strchr("\r\n",url[strlen(url)-1])) {
		url[strlen(url)-1] = 0;
	}

	/* anything following a ? in the URL is part of the query string */
	if ((p=strchr(url,'?'))) {
		query_string = p+1;
		*p = 0;
	}

	string_sub(url, "/swat/", "", 0);

	if (url[0] != '/' && strstr(url,"..")==0 && file_exist(url, NULL)) {
		cgi_download(url);
	}

	printf("HTTP/1.0 200 OK\r\nConnection: close\r\n");
	printf("Date: %s\r\n", http_timestring(time(NULL)));
	baseurl = "";
	pathinfo = url+1;
}


/***************************************************************************
return the current pages URL
  ***************************************************************************/
char *cgi_baseurl(void)
{
	if (inetd_server) {
		return baseurl;
	}
	return getenv("SCRIPT_NAME");
}

/***************************************************************************
return the current pages path info
  ***************************************************************************/
char *cgi_pathinfo(void)
{
	char *r;
	if (inetd_server) {
		return pathinfo;
	}
	r = getenv("PATH_INFO");
	if (!r) return "";
	if (*r == '/') r++;
	return r;
}

/***************************************************************************
return the hostname of the client
  ***************************************************************************/
char *cgi_remote_host(void)
{
	if (inetd_server) {
		return client_name(1);
	}
	return getenv("REMOTE_HOST");
}

/***************************************************************************
return the hostname of the client
  ***************************************************************************/
char *cgi_remote_addr(void)
{
	if (inetd_server) {
		return client_addr(1);
	}
	return getenv("REMOTE_ADDR");
}


/***************************************************************************
return True if the request was a POST
  ***************************************************************************/
BOOL cgi_waspost(void)
{
	if (inetd_server) {
		return request_post;
	}
	return strequal(getenv("REQUEST_METHOD"), "POST");
}
