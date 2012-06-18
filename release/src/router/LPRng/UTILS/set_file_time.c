/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 The evil 'set file time' function for testing file removal
  Use:
    setfiletime Age file*
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
 
     extern char *optarg;
     extern int optind;
     extern int optopt;
     extern int opterr;
     extern int optreset;

char *Name = "???";

char *msg[] = {
	"%s: setfiletime AGE file*\n",
	"  AGE  is fK, where f amount (fraction)",
	"       K is amount - S/s sec, M/m min, H/h hour, D/d day",
	0
};
void usage(void)
{
	int i;
	for( i = 0; msg[i]; ++i ){
		if( i == 0 ) fprintf( stderr, msg[i], Name );
		else fprintf( stderr, "%s\n", msg[i] );
	}
	exit(1);
}


int main( int argc, char **argv )
{
	int c;
	int age;
	float fage;
	char *s, *t;
	struct timeval tval[2];
	setlinebuf(stdout);
	if( argv[0] ) Name = argv[0];
	while( (c = getopt( argc, argv, "=" ) ) != EOF ){
		switch( c ){
			case '=': usage(); break;
			default: usage(); break;
		}
	}
	if( argc - optind == 0 ) usage();
	s = argv[optind++];
	t = 0;
	fage = strtod(s,&t);
	printf("age '%f' (%s)\n", fage, s );
	if( !t ) usage();
	if( !fage ) usage();
	switch( t[0] ){
		case 's': case 'S': break;
		case 'm': case 'M': fage *= 60; break;
		case 'h': case 'H': fage *= 60*60; break;
		case 'd': case 'D': fage *= 24*60*60; break;
	}
	printf( "back '%f' sec\n", fage );
	if( gettimeofday( &tval[0], 0 ) ){
		perror( "gettimeofday" ); exit(1);
	}
	printf( "now '%d'\n", (int)tval[0].tv_sec );
	tval[0].tv_sec -= (int)fage;
	tval[1] = tval[0];
	while( optind < argc ){
		s = argv[optind++];
		printf( "setting '%s' back %d\n", s, (int)fage );
		if( utimes( s, tval ) ){
			perror( "utimes" ); exit(1);
		}
	}
	return(0);
}
