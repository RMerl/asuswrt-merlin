/**
    eMail is a command line SMTP client.

    Copyright (C) 2001 - 2008 email by Dean Jones
    Software supplied and written by http://www.cleancode.org

    This file is part of eMail.

    eMail is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    eMail is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with eMail; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**/
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "email.h"
#include "addr_parse.h"
#include "utils.h"
#include "addy_book.h"
#include "error.h"

/*
 * Entry is a structure in which we will hold a parsed
 * string from the address book.  
 * 
 * e_gors == "group" for group "single" for single and NULL for neither
 * e_name == Name of entry
 * e_addr == Addresses found with entry 
 *
 */

typedef struct {
	char *e_gors;
	char *e_name;
	char *e_addr;
} ENTRY;

/**
 * Frees an ENTRY structure if it needs to be feed 
**/
static void
freeEntry(ENTRY *en)
{
	xfree(en->e_gors);
	xfree(en->e_name);
	xfree(en->e_addr);
}


static void
separateDestr(void *ptr)
{
	xfree(ptr);
}

static void
addrDestr(void *ptr)
{
	struct addr *a = (struct addr *)ptr;
	if (a) {
		xfree(a->name);
		xfree(a->email);
		xfree(a);
	}
}

	
/** 
 * Seperate will take a string of command separated fields
 * and with separate them into a linked list for return 
 * by the function.
**/
static dlist
separate(char *string)
{
	char *next;
	int counter;
	dlist ret = dlInit(separateDestr);

	counter = 0;
	next = strtok(string, ",");
	while (next) {
		/* don't let our counter get over 99 address' */
		if (counter == 99) {
			break;
		}

		/**
		 * make sure that next isn't too large, if it is, don't store it
		 * get next element and move on...
		 */
		if (strlen(next) > MAXBUF) {
			next = strtok(NULL, ",");
			continue;
		}

		/* Get rid of white spaces */
		while (*next == ' ' || *next == '\t') {
			next++;
		}

		dlInsertTop(ret, xstrdup(next));
		next = strtok(NULL, ",");
		counter++;
	}
	return ret;
}

/**
 * Makes sure that all values are available for storage in
 * the ENTRY structure.  
 *
 * If NONE of the values are ready, it returns -1 to let 
 * the caller know that this is not an ERROR and should 
 * continue.
 *
 * If some of the values aren't available, something fishy
 * went on and we'll return ERROR
 *
 * Otherwise, store the values in the ENTRY struct.
**/
static int
makeEntry(ENTRY *en, const char *gors, const char *name, const char *addr)
{
	if ((*gors == '\0') && (*name == '\0') && (*addr == '\0')) {
		return -1;            /* No error */
	}
	if ((*gors == '\0') || (*name == '\0') || (*addr == '\0')) {
		return ERROR;
	}
	en->e_gors = xstrdup(gors);
	en->e_name = xstrdup(name);
	en->e_addr = xstrdup(addr);
	return 0;
}

/**
 * Parses the address book file in search of the correct
 * entry.  If the entry desired is found, then it is stored
 * in the ENTRY structure that is passed.
**/
static int
getEntry(ENTRY *en, char *ent, FILE *book)
{
	int ch, line=1;
	dstrbuf *ptr, *gors;
	dstrbuf *name, *addr;

	assert(book != NULL);
	rewind(book); /* start at top of file */

	gors = DSB_NEW;
	name = DSB_NEW;
	addr = DSB_NEW;
	ptr = gors;
	en->e_gors = en->e_name = en->e_addr = NULL;

	while ((ch = fgetc(book)) != EOF) {
		switch (ch) {
		case '#':
			while ((ch = fgetc(book)) != '\n')
				;
			break;
		case '\\':
			ch = fgetc(book);
			if (ch == '\r') {
				ch = fgetc(book);
			}
			if (ch != '\n') {
				dsbCatChar(ptr, ch);
			}
			line++;
			ch = 0;
			break;
		case '\'':
			if (copyUpTo(ptr, ch, book) == '\n') {
				ch = line;
				goto exit;
			}
			break;
		case '"':
			if (copyUpTo(ptr, ch, book) == '\n') {
				ch = line;
				goto exit;
			}
			break;
		case ':':
			if (name->len != 0) {
				ch = line;
				goto exit;
			}
			ptr = name;
			break;
		case '=':
			if (addr->len != 0) {
				ch = line;
				goto exit;
			}
			ptr = addr;
			break;
		case ' ':
			/* Nothing for spaces */
			break;
		case '\t':
			/* Nothing for tabs */
			break;
		case '\r':
			/* Ignore */
			break;
		case '\n':
			/* Handle newlines below */
			break;
		default:
			dsbCatChar(ptr, ch);
			break;
		}

		if (ch == '\n') {
			if (strcasecmp(name->str, ent) == 0) {
				if (makeEntry(en, gors->str, name->str, addr->str) 
					== ERROR) {
					ch = line;
					goto exit;
				}
				ch = 0;
				goto exit;
			}

			/* Let's keep looking ! */
			dsbDestroy(gors);
			dsbDestroy(name);
			dsbDestroy(addr);
			gors = DSB_NEW;
			name = DSB_NEW;
			addr = DSB_NEW;
			ptr = gors;
			line++;
		}
	}

exit:
	dsbDestroy(gors);
	dsbDestroy(name);
	dsbDestroy(addr);

	/* Possible EOF or Line number */
	return ch;
}


/**
 * Add an email to the list.  Make sure it is formated properly.
 * After formated properly, list_insert() it 
**/
static void
insertEntry(dlist to, const char *name, const char *addr)
{
	struct addr *newaddr = xmalloc(sizeof(struct addr));
	if (validateEmail(addr) == ERROR) {
		warning("Email address '%s' is invalid. Skipping...\n", addr);
		return;
	}
	if (*name != '\0' && strcmp(name, "") != 0) { 
		newaddr->name = xstrdup(name);
	}
	newaddr->email = xstrdup(addr);
	dlInsertTop(to, newaddr);
}

/**
 * A wrapper for insertEntry for when we seem to have just an address 
 * (ie not an address book entry.)
 */
static void
insertAddrEntry(dlist to, const char *addr)
{
	dstrbuf *name = DSB_NEW;
	dstrbuf *email = DSB_NEW;
	if (parseAddr(addr, name, email) == ERROR) {
		warning("Email address %s is incorrectly formatted. Skipping...\n", addr);
	} else {
		char *stripped = stripEmailName(name->str);
		insertEntry(to, stripped, email->str);
	}
	dsbDestroy(name);
	dsbDestroy(email);
}

/**
 * This function checks each address in tmp and formats it
 * properly and copies it over to the new list.
**/
static void
checkAndCopy(dlist to, dlist from)
{
	char *next=NULL;
	while ((next=(char *)dlGetNext(from)) != NULL) {
		insertAddrEntry(to, next);
	}
}

static int addEntry(dlist to, ENTRY *en, FILE *book);

/**
 * Loops through the linked list and calls Get_entry for
 * each name in the linked list.  It will add each entry into
 * the 'to' linked lists when an appropriate match is found
 * from the address book.
**/
static int
checkAddrBook(dlist to, dlist from, FILE *book)
{
	ENTRY en;
	int retval;
	char *next=NULL;

	/* Go through list from, resolving to list curr */
	while ((next=(char *)dlGetNext(from)) != NULL) {
		retval = getEntry(&en, next, book);
		if (retval > 0) {
			fatal("Address book incorrectly formated on line %d\n", retval);
			return ERROR;
		} else if (retval == EOF) {
			insertAddrEntry(to, next);
		} else {
			if (addEntry(to, &en, book) == ERROR) {
				return ERROR;
			}
		}
		freeEntry(&en);
	}
	return SUCCESS; 
}

/**
 * Add an entry to the linked lists.
 * If entry is a group, we must separate the people inside of 
 * the group and re-call check_addr_book for those entries.
**/
static int
storeGroup(dlist to, ENTRY *en, FILE *book)
{
	static int set_group;
	dlist tmp=NULL;

	if (set_group) {
		fatal("You can't define groups within groups!\n");
		return ERROR;
	}

	tmp = separate(en->e_addr);
	set_group = 1;
	checkAddrBook(to, tmp, book);
	set_group = 0;
	dlDestroy(tmp);
	return 0;
}

/**
 * Wrapper for adding an entry.  Will call Store_group
 * or Store_single, or Store_email if the entry is 
 * one of the above.
**/
static int
addEntry(dlist to, ENTRY *en, FILE *book)
{
	if (strcmp(en->e_gors, "group") == 0) {
		if (storeGroup(to, en, book) == ERROR) {
			return ERROR;
		}
	} else {
		insertEntry(to, en->e_name, en->e_addr);
	}
	return 0;
}


/** 
 * GetNames just calls the correct functions to 
 * split up the names passed to email and check
 * them against the address book and it will return 
 * a linked list 'list_t' with the correct separate names.
**/
dlist
getNames(char *string)
{
	FILE *book;
	char *addr_book;
	dlist tmp = NULL;
	dstrbuf *bpath=NULL;
	dlist ret = dlInit(addrDestr);

	addr_book = getConfValue("ADDRESS_BOOK");

	/* Read in and hash the address book */
	if (!(tmp = separate(string))) {
		return NULL;
	}

	if (!addr_book) {
		checkAndCopy(ret, tmp);
	} else {
		bpath = expandPath(addr_book);
		book = fopen(bpath->str, "r");
		if (!book) {
			fatal("Can't open address book: '%s'\n", bpath->str);
			dlDestroy(ret);
			dlDestroy(tmp);
			dsbDestroy(bpath);
			return NULL;
		}
		dsbDestroy(bpath);
		checkAddrBook(ret, tmp, book);
		fclose(book);
	}

	dlDestroy(tmp);
	return ret;
}

