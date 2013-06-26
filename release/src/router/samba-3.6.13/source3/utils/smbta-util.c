/*
   smbta-util: tool for controlling encryption with
	vfs_smb_traffic_analyzer
   Copyright (C) 2010 Holger Hetterich <hhetter@novell.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "secrets.h"

static void delete_key(void);


static void help(void)
{
printf("-h 		print this help message.\n");
printf("-f <file>	install the key from a file and activate\n");
printf("		encryption.\n");
printf("-g <file>	generate a key, save it to a file, and activate encryption.\n");
printf("-u		uninstall a key, and deactivate encryption.\n");
printf("-c <file>	create a file from an installed key.\n");
printf("-s		check if a key is installed, and print the key to stdout.\n");
printf("\n");
}

static void check_key(void)
{	size_t size;
	char *akey;
	if (!secrets_init()) {
		printf("Error opening secrets database.");
		exit(1);
        }
	akey = (char *) secrets_fetch("smb_traffic_analyzer_key", &size);
	if (akey != NULL) {
		printf("A key is installed: %s\n",akey);
		printf("Encryption activated.\n");
		free(akey);
		exit(0);
	} else printf("No key is installed.\n");
	exit(1);
}

static void create_keyfile(char *filename, char *key)
{
	FILE *keyfile;
	keyfile = fopen(filename, "w");
	if (keyfile == NULL) {
		printf("error creating the keyfile!\n");
		exit(1);
	}
	fprintf(keyfile, "%s", key);
	fclose(keyfile);
	printf("File '%s' has been created.\n", filename);
}

/**
 * Load a key from a file. The caller has to free the
 * returned string.
 */
static void load_key_from_file(char *filename, char *key)
{
	FILE *keyfile;
	int l;
	keyfile = fopen(filename, "r");
	if (keyfile == NULL) {
		printf("Error opening the keyfile!\n");
		exit(1);
	}
	l = fscanf(keyfile, "%s", key);
	if (strlen(key) != 16) {
		printf("Key file in wrong format\n");
		fclose(keyfile);
		exit(1);
	}
	fclose(keyfile);
}

static void create_file_from_key(char *filename)
{
	size_t size;
	char *akey = (char *) secrets_fetch("smb_traffic_analyzer_key", &size);
	if (akey == NULL) {
		printf("No key is installed! Can't create file.\n");
		exit(1);
	}
	create_keyfile(filename, akey);
	free(akey);
}

/**
 * Generate a random key. The user has to free the returned
 * string.
 */
static void generate_key(char *key)
{
	int f;
	srand( (unsigned)time( NULL ) );
	for ( f = 0; f < 16; f++) {
		*(key+f) = (rand() % 128) +32;
	}
	*(key+16)='\0';
	printf("Random key generated.\n");
}

static void create_new_key_and_activate( char *filename )
{
	char key[17] = {0};

	if (!secrets_init()) {
		printf("Error opening secrets database.");
		exit(1);
	}

	generate_key(key);
	delete_key();
	secrets_store("smb_traffic_analyzer_key", key, strlen(key)+1 );
	printf("Key installed, encryption activated.\n");
	create_file_from_key(filename);
}

static void delete_key(void)
{
	size_t size;
	char *akey = (char *) secrets_fetch("smb_traffic_analyzer_key", &size);
	if (akey != NULL) {
		free(akey);
		secrets_delete("smb_traffic_analyzer_key");
		printf("Removed installed key. Encryption deactivated.\n");
	} else {
	printf("No key is installed.\n");
	}
}


static void load_key_from_file_and_activate( char *filename)
{
	char key[17] = {0};
	char *akey;
	size_t size;
	load_key_from_file(filename, key);
	printf("Loaded key from %s.\n",filename);
	akey = (char *) secrets_fetch("smb_traffic_analyzer_key", &size);
	if (akey != NULL) {
		printf("Removing the old key.\n");
		delete_key();
		SAFE_FREE(akey);
	}
	printf("Installing the key from file %s\n",filename);
	secrets_store("smb_traffic_analyzer_key", key, strlen(key)+1);
}

static void process_arguments(int argc, char **argv)
{
	char co;
	while ((co = getopt(argc, argv, "hf:g:uc:s")) != EOF) {
		switch(co) {
		case 'h':
			help();
			exit(0);
		case 's':
			check_key();
			break;
		case 'g':
			create_new_key_and_activate(optarg);
			break;
		case 'u':
			delete_key();
			break;
		case 'c':
			create_file_from_key(optarg);
			break;
		case 'f':
			load_key_from_file_and_activate(optarg);
			break;
		default:
			help();
			break;
		}
	}
}

int main(int argc, char **argv)
{
	sec_init();
	load_case_tables();

	if (!lp_load_initial_only(get_dyn_CONFIGFILE())) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n",
						get_dyn_CONFIGFILE());
	exit(1);
	}

	if (argc == 1) {
		help();
		exit(1);
	}

	process_arguments(argc, argv);
	exit(0);
}
