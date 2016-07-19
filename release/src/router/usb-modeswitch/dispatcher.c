/*
 * Copyright (c) 2011-2016 Josua Dietze, usb_modeswitch version 2.3.0
 * Contains code under
 * Copyright (c) 2010 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <jim.h>

/* RAW is defined to the complete Tcl code in that file: */
#include "usb_modeswitch.string"

#define MAX_ARGSIZE 1024

int main(int argc, char **argv)
{
	int i, retval;
	char arg[MAX_ARGSIZE];
	char arglist[MAX_ARGSIZE]; 

	Jim_Interp *interp;

	interp = NULL;
	arglist[0] = '\0';

	for (i=1; i<argc; i++) {
		if (strlen(argv[i]) > MAX_ARGSIZE-4) {
			printf("Argument #%d extends maximum size, skip it\n", i);
			continue;
		}
		if ( (strlen(arglist) + strlen(argv[i])) > MAX_ARGSIZE-4) {
			printf("Argument #%d would extend maximum list size, skip it\n", i);
			continue;
		}
		sprintf(arg,"{%s} ",argv[i]);
		strncat(arglist,arg,MAX_ARGSIZE-1);
	}

	char code[sizeof(RAW) + sizeof(arglist) + 28];
	sprintf(code, "set argv {%s}\nset argc %d\n", arglist, argc-1);
	strncat(code, RAW, sizeof(RAW));

	/* Create Jim instance */
	interp = Jim_CreateInterp();
	assert(interp != NULL && "Could not create interpreter!");

	/* Register base commands, actually implementing Tcl */
	Jim_RegisterCoreCommands(interp);

	/* Initialize any static extensions */
	Jim_InitStaticExtensions(interp);

	/* Evaluate the script that's now in "code" */
	retval = Jim_Eval(interp, code);
	if (retval < 0)
		printf("Evaluation returned error %d\n", retval);

	/* Free the interpreter */
	Jim_FreeInterp(interp);
	return (EXIT_SUCCESS);
}
