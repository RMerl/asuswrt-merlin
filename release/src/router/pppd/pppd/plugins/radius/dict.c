/*
 * $Id: dict.c,v 1.1 2004/11/14 07:26:26 paulus Exp $
 *
 * Copyright (C) 2002 Roaring Penguin Software Inc.
 *
 * Copyright (C) 1995,1996,1997 Lars Fenneberg
 *
 * Copyright 1992 Livingston Enterprises, Inc.
 *
 * Copyright 1992,1993, 1994,1995 The Regents of the University of Michigan
 * and Merit Network, Inc. All Rights Reserved
 *
 * See the file COPYRIGHT for the respective terms and conditions.
 * If the file is missing contact me at lf@elemental.net
 * and I'll send you a copy.
 *
 */

#include <includes.h>
#include <radiusclient.h>

static DICT_ATTR *dictionary_attributes = NULL;
static DICT_VALUE *dictionary_values = NULL;
static VENDOR_DICT *vendor_dictionaries = NULL;

/*
 * Function: rc_read_dictionary
 *
 * Purpose: Initialize the dictionary.  Read all ATTRIBUTES into
 *	    the dictionary_attributes list.  Read all VALUES into
 *	    the dictionary_values list.  Construct VENDOR dictionaries
 *          as required.
 *
 */

int rc_read_dictionary (char *filename)
{
	FILE           *dictfd;
	char            dummystr[AUTH_ID_LEN];
	char            namestr[AUTH_ID_LEN];
	char            valstr[AUTH_ID_LEN];
	char            attrstr[AUTH_ID_LEN];
	char            typestr[AUTH_ID_LEN];
	char            vendorstr[AUTH_ID_LEN];
	int             line_no;
	DICT_ATTR      *attr;
	DICT_VALUE     *dval;
	VENDOR_DICT    *vdict;
	char            buffer[256];
	int             value;
	int             type;
	int             n;
	int             retcode;
	if ((dictfd = fopen (filename, "r")) == (FILE *) NULL)
	{
		error( "rc_read_dictionary: couldn't open dictionary %s: %s",
				filename, strerror(errno));
		return (-1);
	}

	line_no = 0;
	retcode = 0;
	while (fgets (buffer, sizeof (buffer), dictfd) != (char *) NULL)
	{
		line_no++;

		/* Skip empty space */
		if (*buffer == '#' || *buffer == '\0' || *buffer == '\n')
		{
			continue;
		}

		if (strncmp (buffer, "VENDOR", 6) == 0) {
		    /* Read the VENDOR line */
		    if (sscanf(buffer, "%s%s%d", dummystr, namestr, &value) != 3) {
			error("rc_read_dictionary: invalid vendor on line %d of dictionary %s",
			      line_no, filename);
			retcode = -1;
			break;
		    }
		    /* Validate entry */
		    if (strlen (namestr) > NAME_LENGTH) {
			error("rc_read_dictionary: invalid name length on line %d of dictionary %s",
			      line_no, filename);
			retcode = -1;
			break;
		    }
		    /* Create new vendor entry */
		    vdict = (VENDOR_DICT *) malloc (sizeof (VENDOR_DICT));
		    if (!vdict) {
			novm("rc_read_dictionary");
			retcode = -1;
			break;
		    }
		    strcpy(vdict->vendorname, namestr);
		    vdict->vendorcode = value;
		    vdict->attributes = NULL;
		    vdict->next = vendor_dictionaries;
		    vendor_dictionaries = vdict;
		}
		else if (strncmp (buffer, "ATTRIBUTE", 9) == 0)
		{

			/* Read the ATTRIBUTE line.  It is one of:
			 * ATTRIBUTE attr_name attr_val type         OR
			 * ATTRIBUTE attr_name attr_val type vendor  */
			vendorstr[0] = 0;
			n = sscanf(buffer, "%s%s%s%s%s", dummystr, namestr, valstr, typestr, vendorstr);
			if (n != 4 && n != 5)
			{
				error("rc_read_dictionary: invalid attribute on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			/*
			 * Validate all entries
			 */
			if (strlen (namestr) > NAME_LENGTH)
			{
				error("rc_read_dictionary: invalid name length on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			if (strlen (vendorstr) > NAME_LENGTH)
			{
				error("rc_read_dictionary: invalid name length on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			if (!isdigit (*valstr))
			{
				error("rc_read_dictionary: invalid value on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}
			value = atoi (valstr);

			if (strcmp (typestr, "string") == 0)
			{
				type = PW_TYPE_STRING;
			}
			else if (strcmp (typestr, "integer") == 0)
			{
				type = PW_TYPE_INTEGER;
			}
			else if (strcmp (typestr, "ipaddr") == 0)
			{
				type = PW_TYPE_IPADDR;
			}
			else if (strcmp (typestr, "date") == 0)
			{
				type = PW_TYPE_DATE;
			}
			else
			{
				error("rc_read_dictionary: invalid type on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			/* Search for vendor if supplied */
			if (*vendorstr) {
			    vdict = rc_dict_findvendor(vendorstr);
			    if (!vdict) {
				    error("rc_read_dictionary: unknown vendor on line %d of dictionary %s",
					  line_no, filename);
				    retcode = -1;
				    break;
			    }
			} else {
			    vdict = NULL;
			}
			/* Create a new attribute for the list */
			if ((attr =
				(DICT_ATTR *) malloc (sizeof (DICT_ATTR)))
							== (DICT_ATTR *) NULL)
			{
				novm("rc_read_dictionary");
				retcode = -1;
				break;
			}
			strcpy (attr->name, namestr);
			if (vdict) {
			    attr->vendorcode = vdict->vendorcode;
			} else {
			    attr->vendorcode = VENDOR_NONE;
			}
			attr->value = value;
			attr->type = type;

			/* Insert it into the list */
			if (vdict) {
			    attr->next = vdict->attributes;
			    vdict->attributes = attr;
			} else {
			    attr->next = dictionary_attributes;
			    dictionary_attributes = attr;
			}
		}
		else if (strncmp (buffer, "VALUE", 5) == 0)
		{
			/* Read the VALUE line */
			if (sscanf (buffer, "%s%s%s%s", dummystr, attrstr,
				    namestr, valstr) != 4)
			{
				error("rc_read_dictionary: invalid value entry on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			/*
			 * Validate all entries
			 */
			if (strlen (attrstr) > NAME_LENGTH)
			{
				error("rc_read_dictionary: invalid attribute length on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			if (strlen (namestr) > NAME_LENGTH)
			{
				error("rc_read_dictionary: invalid name length on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}

			if (!isdigit (*valstr))
			{
				error("rc_read_dictionary: invalid value on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}
			value = atoi (valstr);

			/* Create a new VALUE entry for the list */
			if ((dval =
				(DICT_VALUE *) malloc (sizeof (DICT_VALUE)))
							== (DICT_VALUE *) NULL)
			{
				novm("rc_read_dictionary");
				retcode = -1;
				break;
			}
			strcpy (dval->attrname, attrstr);
			strcpy (dval->name, namestr);
			dval->value = value;

			/* Insert it into the list */
			dval->next = dictionary_values;
			dictionary_values = dval;
		}
		else if (strncmp (buffer, "INCLUDE", 7) == 0)
		{
			/* Read the INCLUDE line */
			if (sscanf (buffer, "%s%s", dummystr, namestr) != 2)
			{
				error("rc_read_dictionary: invalid include entry on line %d of dictionary %s",
				      line_no, filename);
				retcode = -1;
				break;
			}
			if (rc_read_dictionary(namestr) == -1)
			{
				retcode = -1;
				break;
			}
		}
	}
	fclose (dictfd);
	return retcode;
}

/*
 * Function: rc_dict_getattr
 *
 * Purpose: Return the full attribute structure based on the
 *	    attribute id number and vendor code.  If vendor code is VENDOR_NONE,
 *          non-vendor-specific attributes are used
 *
 */

DICT_ATTR *rc_dict_getattr (int attribute, int vendor)
{
	DICT_ATTR      *attr;
	VENDOR_DICT    *dict;

	if (vendor == VENDOR_NONE) {
	    attr = dictionary_attributes;
	    while (attr != (DICT_ATTR *) NULL) {
		if (attr->value == attribute) {
		    return (attr);
		}
		attr = attr->next;
	    }
	} else {
	    dict = rc_dict_getvendor(vendor);
	    if (!dict) {
		return NULL;
	    }
	    attr = dict->attributes;
	    while (attr) {
		if (attr->value == attribute) {
		    return attr;
		}
		attr = attr->next;
	    }
	}
	return NULL;
}

/*
 * Function: rc_dict_findattr
 *
 * Purpose: Return the full attribute structure based on the
 *	    attribute name.
 *
 */

DICT_ATTR *rc_dict_findattr (char *attrname)
{
	DICT_ATTR      *attr;
	VENDOR_DICT    *dict;

	attr = dictionary_attributes;
	while (attr != (DICT_ATTR *) NULL)
	{
		if (strcasecmp (attr->name, attrname) == 0)
		{
			return (attr);
		}
		attr = attr->next;
	}

	/* Search vendor-specific dictionaries */
	dict = vendor_dictionaries;
	while (dict) {
	    attr = dict->attributes;
	    while (attr) {
		if (strcasecmp (attr->name, attrname) == 0) {
		    return (attr);
		}
		attr = attr->next;
	    }
	    dict = dict->next;
	}
	return ((DICT_ATTR *) NULL);
}


/*
 * Function: rc_dict_findval
 *
 * Purpose: Return the full value structure based on the
 *         value name.
 *
 */

DICT_VALUE *rc_dict_findval (char *valname)
{
	DICT_VALUE     *val;

	val = dictionary_values;
	while (val != (DICT_VALUE *) NULL)
	{
		if (strcasecmp (val->name, valname) == 0)
		{
			return (val);
		}
		val = val->next;
	}
	return ((DICT_VALUE *) NULL);
}

/*
 * Function: dict_getval
 *
 * Purpose: Return the full value structure based on the
 *          actual value and the associated attribute name.
 *
 */

DICT_VALUE * rc_dict_getval (UINT4 value, char *attrname)
{
	DICT_VALUE     *val;

	val = dictionary_values;
	while (val != (DICT_VALUE *) NULL)
	{
		if (strcmp (val->attrname, attrname) == 0 &&
				val->value == value)
		{
			return (val);
		}
		val = val->next;
	}
	return ((DICT_VALUE *) NULL);
}

/*
 * Function: rc_dict_findvendor
 *
 * Purpose: Return the vendor's dictionary given the vendor name.
 *
 */
VENDOR_DICT * rc_dict_findvendor (char *vendorname)
{
    VENDOR_DICT *dict;

    dict = vendor_dictionaries;
    while (dict) {
	if (!strcmp(vendorname, dict->vendorname)) {
	    return dict;
	}
	dict = dict->next;
    }
    return NULL;
}

/*
 * Function: rc_dict_getvendor
 *
 * Purpose: Return the vendor's dictionary given the vendor ID
 *
 */
VENDOR_DICT * rc_dict_getvendor (int id)
{
    VENDOR_DICT *dict;

    dict = vendor_dictionaries;
    while (dict) {
	if (id == dict->vendorcode) {
	    return dict;
	}
	dict = dict->next;
    }
    return NULL;
}
