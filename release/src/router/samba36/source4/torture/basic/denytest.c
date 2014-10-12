/* 
   Unix SMB/CIFS implementation.
   SMB torture tester - deny mode scanning functions
   Copyright (C) Andrew Tridgell 2001
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "libcli/security/security.h"
#include "torture/util.h"
#include "cxd_known.h"

extern int torture_failures;

#define CHECK_MAX_FAILURES(label) do { if (++failures >= torture_failures) goto label; } while (0)

enum deny_result {A_0=0, A_X=1, A_R=2, A_W=3, A_RW=5};

static const char *denystr(int denymode)
{
	const struct {
		int v;
		const char *name; 
	} deny_modes[] = {
		{DENY_DOS, "DENY_DOS"},
		{DENY_ALL, "DENY_ALL"},
		{DENY_WRITE, "DENY_WRITE"},
		{DENY_READ, "DENY_READ"},
		{DENY_NONE, "DENY_NONE"},
		{DENY_FCB, "DENY_FCB"},
		{-1, NULL}};
	int i;
	for (i=0;deny_modes[i].name;i++) {
		if (deny_modes[i].v == denymode) return deny_modes[i].name;
	}
	return "DENY_XXX";
}

static const char *openstr(int mode)
{
	const struct {
		int v;
		const char *name; 
	} open_modes[] = {
		{O_RDWR, "O_RDWR"},
		{O_RDONLY, "O_RDONLY"},
		{O_WRONLY, "O_WRONLY"},
		{-1, NULL}};
	int i;
	for (i=0;open_modes[i].name;i++) {
		if (open_modes[i].v == mode) return open_modes[i].name;
	}
	return "O_XXX";
}

static const char *resultstr(enum deny_result res)
{
	const struct {
		enum deny_result res;
		const char *name; 
	} results[] = {
		{A_X, "X"},
		{A_0, "-"},
		{A_R, "R"},
		{A_W, "W"},
		{A_RW,"RW"}};
	int i;
	for (i=0;ARRAY_SIZE(results);i++) {
		if (results[i].res == res) return results[i].name;
	}
	return "*";
}

static const struct {
	int isexe;
	int mode1, deny1;
	int mode2, deny2;
	enum deny_result result;
} denytable2[] = {
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_R},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_W},
{0, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_W},
{0, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_RW},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_W},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_RW},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_R},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_W},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_0}
};


static const struct {
	int isexe;
	int mode1, deny1;
	int mode2, deny2;
	enum deny_result result;
} denytable1[] = {
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{1, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{1, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{1,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{1,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{1, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{1, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{1, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{1, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{1, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_RW},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_DOS,     A_RW},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_DOS,     A_W},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_DOS,      O_RDWR,   DENY_FCB,     A_RW},
{0, O_WRONLY,   DENY_DOS,    O_RDONLY,   DENY_FCB,     A_RW},
{0, O_WRONLY,   DENY_DOS,    O_WRONLY,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_ALL,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,   DENY_ALL,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_READ,     A_R},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY, DENY_WRITE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY, DENY_WRITE,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0,   O_RDWR,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_W},
{0, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_RDONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_READ,     A_W},
{0, O_WRONLY,  DENY_READ,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_WRONLY,  DENY_READ,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_READ,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0,   O_RDWR,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_RW},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_W},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_RDONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_RDONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_DOS,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_READ,     A_RW},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_READ,     A_R},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_READ,     A_W},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,  DENY_NONE,     A_RW},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,  DENY_NONE,     A_R},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,  DENY_NONE,     A_W},
{0, O_WRONLY,  DENY_NONE,      O_RDWR,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_RDONLY,   DENY_FCB,     A_0},
{0, O_WRONLY,  DENY_NONE,    O_WRONLY,   DENY_FCB,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0,   O_RDWR,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{0,   O_RDWR,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_RDONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{0, O_RDONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{0, O_RDONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_DOS,     A_RW},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_DOS,     A_R},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_DOS,     A_W},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_ALL,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY, DENY_WRITE,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_READ,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,  DENY_NONE,     A_0},
{0, O_WRONLY,   DENY_FCB,      O_RDWR,   DENY_FCB,     A_RW},
{0, O_WRONLY,   DENY_FCB,    O_RDONLY,   DENY_FCB,     A_RW},
{0, O_WRONLY,   DENY_FCB,    O_WRONLY,   DENY_FCB,     A_RW}
};


static void progress_bar(struct torture_context *tctx, unsigned int i, unsigned int total)
{
	if (torture_setting_bool(tctx, "progress", true)) {
		torture_comment(tctx, "%5d/%5d\r", i, total);
		fflush(stdout);
	}
}

/*
  this produces a matrix of deny mode behaviour for 1 connection
 */
bool torture_denytest1(struct torture_context *tctx, 
		       struct smbcli_state *cli1)
{
	int fnum1, fnum2;
	int i;
	bool correct = true;
	struct timespec tv, tv_start;
	const char *fnames[2] = {"\\denytest1.dat", "\\denytest1.exe"};
	int failures=0;

	torture_comment(tctx, "Testing deny modes with 1 connection\n");

	for (i=0;i<2;i++) {
		smbcli_unlink(cli1->tree, fnames[i]);
		fnum1 = smbcli_open(cli1->tree, fnames[i], O_RDWR|O_CREAT, DENY_NONE);
		smbcli_write(cli1->tree, fnum1, 0, fnames[i], 0, strlen(fnames[i]));
		smbcli_close(cli1->tree, fnum1);
	}

	torture_comment(tctx, "Testing %d entries\n", (int)ARRAY_SIZE(denytable1));

	clock_gettime_mono(&tv_start);

	for (i=0; i<ARRAY_SIZE(denytable1); i++) {
		enum deny_result res;
		const char *fname = fnames[denytable1[i].isexe];

		progress_bar(tctx, i, ARRAY_SIZE(denytable1));

		if (!torture_setting_bool(tctx, "deny_fcb_support", true) &&
		    (denytable1[i].deny1 == DENY_FCB ||
			denytable1[i].deny2 == DENY_FCB))
			continue;

		if (!torture_setting_bool(tctx, "deny_dos_support", true) &&
		    (denytable1[i].deny1 == DENY_DOS ||
			denytable1[i].deny2 == DENY_DOS))
			continue;

		fnum1 = smbcli_open(cli1->tree, fname, 
				 denytable1[i].mode1,
				 denytable1[i].deny1);
		fnum2 = smbcli_open(cli1->tree, fname, 
				 denytable1[i].mode2,
				 denytable1[i].deny2);

		if (fnum1 == -1) {
			res = A_X;
		} else if (fnum2 == -1) {
			res = A_0;
		} else {
			uint8_t x = 1;
			res = A_0;
			if (smbcli_read(cli1->tree, fnum2, &x, 0, 1) == 1) {
				res += A_R;
			}
			if (smbcli_write(cli1->tree, fnum2, 0, &x, 0, 1) == 1) {
				res += A_W;
			}
		}

		if (torture_setting_bool(tctx, "showall", false) || 
			res != denytable1[i].result) {
			int64_t tdif;
			clock_gettime_mono(&tv);
			tdif = nsec_time_diff(&tv, &tv_start);
			tdif /= 1000000;
			torture_comment(tctx, "%lld: %s %8s %10s    %8s %10s    %s (correct=%s)\n",
			       (long long)tdif,
			       fname,
			       denystr(denytable1[i].deny1),
			       openstr(denytable1[i].mode1),
			       denystr(denytable1[i].deny2),
			       openstr(denytable1[i].mode2),
			       resultstr(res),
			       resultstr(denytable1[i].result));
		}

		if (res != denytable1[i].result) {
			correct = false;
			CHECK_MAX_FAILURES(failed);
		}

		smbcli_close(cli1->tree, fnum1);
		smbcli_close(cli1->tree, fnum2);
	}

failed:
	for (i=0;i<2;i++) {
		smbcli_unlink(cli1->tree, fnames[i]);
	}
		
	torture_comment(tctx, "finshed denytest1 (%d failures)\n", failures);
	return correct;
}


/*
  this produces a matrix of deny mode behaviour with 2 connections
 */
bool torture_denytest2(struct torture_context *tctx, 
		       struct smbcli_state *cli1, 
		       struct smbcli_state *cli2)
{
	int fnum1, fnum2;
	int i;
	bool correct = true;
	const char *fnames[2] = {"\\denytest2.dat", "\\denytest2.exe"};
	struct timespec tv, tv_start;
	int failures=0;

	for (i=0;i<2;i++) {
		smbcli_unlink(cli1->tree, fnames[i]);
		fnum1 = smbcli_open(cli1->tree, fnames[i], O_RDWR|O_CREAT, DENY_NONE);
		smbcli_write(cli1->tree, fnum1, 0, fnames[i], 0, strlen(fnames[i]));
		smbcli_close(cli1->tree, fnum1);
	}

	clock_gettime_mono(&tv_start);

	for (i=0; i<ARRAY_SIZE(denytable2); i++) {
		enum deny_result res;
		const char *fname = fnames[denytable2[i].isexe];

		progress_bar(tctx, i, ARRAY_SIZE(denytable1));

		if (!torture_setting_bool(tctx, "deny_fcb_support", true) &&
		    (denytable1[i].deny1 == DENY_FCB ||
			denytable1[i].deny2 == DENY_FCB))
			continue;

		if (!torture_setting_bool(tctx, "deny_dos_support", true) &&
		    (denytable1[i].deny1 == DENY_DOS ||
			denytable1[i].deny2 == DENY_DOS))
			continue;

		fnum1 = smbcli_open(cli1->tree, fname, 
				 denytable2[i].mode1,
				 denytable2[i].deny1);
		fnum2 = smbcli_open(cli2->tree, fname, 
				 denytable2[i].mode2,
				 denytable2[i].deny2);

		if (fnum1 == -1) {
			res = A_X;
		} else if (fnum2 == -1) {
			res = A_0;
		} else {
			uint8_t x = 1;
			res = A_0;
			if (smbcli_read(cli2->tree, fnum2, &x, 0, 1) == 1) {
				res += A_R;
			}
			if (smbcli_write(cli2->tree, fnum2, 0, &x, 0, 1) == 1) {
				res += A_W;
			}
		}

		if (torture_setting_bool(tctx, "showall", false) || 
			res != denytable2[i].result) {
			int64_t tdif;
			clock_gettime_mono(&tv);
			tdif = nsec_time_diff(&tv, &tv_start);
			tdif /= 1000000;
			torture_comment(tctx, "%lld: %s %8s %10s    %8s %10s    %s (correct=%s)\n",
			       (long long)tdif,
			       fname,
			       denystr(denytable2[i].deny1),
			       openstr(denytable2[i].mode1),
			       denystr(denytable2[i].deny2),
			       openstr(denytable2[i].mode2),
			       resultstr(res),
			       resultstr(denytable2[i].result));
		}

		if (res != denytable2[i].result) {
			correct = false;
			CHECK_MAX_FAILURES(failed);
		}

		smbcli_close(cli1->tree, fnum1);
		smbcli_close(cli2->tree, fnum2);
	}

failed:		
	for (i=0;i<2;i++) {
		smbcli_unlink(cli1->tree, fnames[i]);
	}

	torture_comment(tctx, "finshed denytest2 (%d failures)\n", failures);
	return correct;
}



/*
   simple test harness for playing with deny modes
 */
bool torture_denytest3(struct torture_context *tctx, 
		       struct smbcli_state *cli1,
		       struct smbcli_state *cli2)
{
	int fnum1, fnum2;
	const char *fname;

	fname = "\\deny_dos1.dat";

	smbcli_unlink(cli1->tree, fname);
	fnum1 = smbcli_open(cli1->tree, fname, O_CREAT|O_TRUNC|O_WRONLY, DENY_DOS);
	fnum2 = smbcli_open(cli1->tree, fname, O_CREAT|O_TRUNC|O_WRONLY, DENY_DOS);
	if (fnum1 != -1) smbcli_close(cli1->tree, fnum1);
	if (fnum2 != -1) smbcli_close(cli1->tree, fnum2);
	smbcli_unlink(cli1->tree, fname);
	torture_comment(tctx, "fnum1=%d fnum2=%d\n", fnum1, fnum2);


	fname = "\\deny_dos2.dat";

	smbcli_unlink(cli1->tree, fname);
	fnum1 = smbcli_open(cli1->tree, fname, O_CREAT|O_TRUNC|O_WRONLY, DENY_DOS);
	fnum2 = smbcli_open(cli2->tree, fname, O_CREAT|O_TRUNC|O_WRONLY, DENY_DOS);
	if (fnum1 != -1) smbcli_close(cli1->tree, fnum1);
	if (fnum2 != -1) smbcli_close(cli2->tree, fnum2);
	smbcli_unlink(cli1->tree, fname);
	torture_comment(tctx, "fnum1=%d fnum2=%d\n", fnum1, fnum2);

	return true;
}

struct bit_value {
	uint32_t value;
	const char *name;
};

static uint32_t map_bits(const struct bit_value *bv, int b, int nbits)
{
	int i;
	uint32_t ret = 0;
	for (i=0;i<nbits;i++) {
		if (b & (1<<i)) {
			ret |= bv[i].value;
		}
	}
	return ret;
}

static const char *bit_string(TALLOC_CTX *mem_ctx, const struct bit_value *bv, int b, int nbits)
{
	char *ret = NULL;
	int i;
	for (i=0;i<nbits;i++) {
		if (b & (1<<i)) {
			if (ret == NULL) {
				ret = talloc_asprintf(mem_ctx, "%s", bv[i].name);
			} else {
				ret = talloc_asprintf_append_buffer(ret, " | %s", bv[i].name);
			}
		}
	}
	if (ret == NULL) ret = talloc_strdup(mem_ctx, "(NONE)");
	return ret;
}


/*
  determine if two opens conflict
*/
static NTSTATUS predict_share_conflict(uint32_t sa1, uint32_t am1, uint32_t sa2, uint32_t am2,
				       bool read_for_execute, enum deny_result *res)
{
#define CHECK_MASK(am, sa, right, share) do { \
	if (((am) & (right)) && !((sa) & (share))) { \
		*res = A_0; \
		return NT_STATUS_SHARING_VIOLATION; \
	}} while (0)

	*res = A_0;
	if (am2 & (SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA)) {
		*res += A_W;
	}
	if (am2 & SEC_FILE_READ_DATA) {
		*res += A_R;
	} else if ((am2 & SEC_FILE_EXECUTE) && read_for_execute) {
		*res += A_R;
	}

	/* if either open involves no read.write or delete access then
	   it can't conflict */
	if (!(am1 & (SEC_FILE_WRITE_DATA | 
		     SEC_FILE_APPEND_DATA |
		     SEC_FILE_READ_DATA | 
		     SEC_FILE_EXECUTE | 
		     SEC_STD_DELETE))) {
		return NT_STATUS_OK;
	}
	if (!(am2 & (SEC_FILE_WRITE_DATA | 
		     SEC_FILE_APPEND_DATA |
		     SEC_FILE_READ_DATA | 
		     SEC_FILE_EXECUTE | 
		     SEC_STD_DELETE))) {
		return NT_STATUS_OK;
	}

	/* check the basic share access */
	CHECK_MASK(am1, sa2, 
		   SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA, 
		   NTCREATEX_SHARE_ACCESS_WRITE);
	CHECK_MASK(am2, sa1, 
		   SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA, 
		   NTCREATEX_SHARE_ACCESS_WRITE);

	CHECK_MASK(am1, sa2, 
		   SEC_FILE_READ_DATA | SEC_FILE_EXECUTE, 
		   NTCREATEX_SHARE_ACCESS_READ);
	CHECK_MASK(am2, sa1, 
		   SEC_FILE_READ_DATA | SEC_FILE_EXECUTE, 
		   NTCREATEX_SHARE_ACCESS_READ);

	CHECK_MASK(am1, sa2, 
		   SEC_STD_DELETE, 
		   NTCREATEX_SHARE_ACCESS_DELETE);
	CHECK_MASK(am2, sa1, 
		   SEC_STD_DELETE, 
		   NTCREATEX_SHARE_ACCESS_DELETE);

	return NT_STATUS_OK;
}

/*
  a denytest for ntcreatex
 */
static bool torture_ntdenytest(struct torture_context *tctx, 
			       struct smbcli_state *cli1,
			       struct smbcli_state *cli2, int client)
{
	const struct bit_value share_access_bits[] = {
		{ NTCREATEX_SHARE_ACCESS_READ,   "S_R" },
		{ NTCREATEX_SHARE_ACCESS_WRITE,  "S_W" },
		{ NTCREATEX_SHARE_ACCESS_DELETE, "S_D" }
	};
	const struct bit_value access_mask_bits[] = {
		{ SEC_FILE_READ_DATA,        "R_DATA" },
		{ SEC_FILE_WRITE_DATA,       "W_DATA" },
		{ SEC_FILE_READ_ATTRIBUTE,   "R_ATTR" },
		{ SEC_FILE_WRITE_ATTRIBUTE,  "W_ATTR" },
		{ SEC_FILE_READ_EA,          "R_EAS " },
		{ SEC_FILE_WRITE_EA,         "W_EAS " },
		{ SEC_FILE_APPEND_DATA,      "A_DATA" },
		{ SEC_FILE_EXECUTE,          "EXEC  " }
	};
	int fnum1;
	int i;
	bool correct = true;
	struct timespec tv, tv_start;
	const char *fname;
	int nbits1 = ARRAY_SIZE(share_access_bits);
	int nbits2 = ARRAY_SIZE(access_mask_bits);
	union smb_open io1, io2;
	extern int torture_numops;
	int failures = 0;
	uint8_t buf[1];

	torture_comment(tctx, "format: server correct\n");

	ZERO_STRUCT(buf);

	fname = talloc_asprintf(cli1, "\\ntdeny_%d.dll", client);

	smbcli_unlink(cli1->tree, fname);
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	smbcli_write(cli1->tree, fnum1, 0, buf, 0, sizeof(buf));
	smbcli_close(cli1->tree, fnum1);

	clock_gettime_mono(&tv_start);

	io1.ntcreatex.level = RAW_OPEN_NTCREATEX;
	io1.ntcreatex.in.root_fid.fnum = 0;
	io1.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io1.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io1.ntcreatex.in.file_attr = 0;
	io1.ntcreatex.in.alloc_size = 0;
	io1.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	io1.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io1.ntcreatex.in.security_flags = 0;
	io1.ntcreatex.in.fname = fname;
	io2 = io1;

	torture_comment(tctx, "Testing %d entries on %s\n", torture_numops, fname);

	for (i=0;i<torture_numops;i++) {
		NTSTATUS status1, status2, status2_p;
		int64_t tdif;
		TALLOC_CTX *mem_ctx = talloc_new(NULL);
		enum deny_result res, res2;
		int b_sa1 = random() & ((1<<nbits1)-1);
		int b_am1 = random() & ((1<<nbits2)-1);
		int b_sa2 = random() & ((1<<nbits1)-1);
		int b_am2 = random() & ((1<<nbits2)-1);
		bool read_for_execute;

		progress_bar(tctx, i, torture_numops);
		
		io1.ntcreatex.in.share_access = map_bits(share_access_bits, b_sa1, nbits1);
		io1.ntcreatex.in.access_mask  = map_bits(access_mask_bits,  b_am1, nbits2);
		
		io2.ntcreatex.in.share_access = map_bits(share_access_bits, b_sa2, nbits1);
		io2.ntcreatex.in.access_mask  = map_bits(access_mask_bits,  b_am2, nbits2);

		status1 = smb_raw_open(cli1->tree, mem_ctx, &io1);
		status2 = smb_raw_open(cli2->tree, mem_ctx, &io2);

		if (random() % 2 == 0) {
			read_for_execute = true;
		} else {
			read_for_execute = false;
		}
		
		if (!NT_STATUS_IS_OK(status1)) {
			res = A_X;
		} else if (!NT_STATUS_IS_OK(status2)) {
			res = A_0;
		} else {
			union smb_read r;
			NTSTATUS status;

			/* we can't use smbcli_read() as we need to
			   set read_for_execute */
			r.readx.level = RAW_READ_READX;
			r.readx.in.file.fnum = io2.ntcreatex.out.file.fnum;
			r.readx.in.offset = 0;
			r.readx.in.mincnt = sizeof(buf);
			r.readx.in.maxcnt = sizeof(buf);
			r.readx.in.remaining = 0;
			r.readx.in.read_for_execute = read_for_execute;
			r.readx.out.data = buf;

			res = A_0;
			status = smb_raw_read(cli2->tree, &r);
			if (NT_STATUS_IS_OK(status)) {
				res += A_R;
			}
			if (smbcli_write(cli2->tree, io2.ntcreatex.out.file.fnum,
					 0, buf, 0, sizeof(buf)) >= 1) {
				res += A_W;
			}
		}
		
		if (NT_STATUS_IS_OK(status1)) {
			smbcli_close(cli1->tree, io1.ntcreatex.out.file.fnum);
		}
		if (NT_STATUS_IS_OK(status2)) {
			smbcli_close(cli2->tree, io2.ntcreatex.out.file.fnum);
		}
		
		status2_p = predict_share_conflict(io1.ntcreatex.in.share_access,
						   io1.ntcreatex.in.access_mask,
						   io2.ntcreatex.in.share_access,
						   io2.ntcreatex.in.access_mask, 
						   read_for_execute,
						   &res2);
		
		clock_gettime_mono(&tv);
		tdif = nsec_time_diff(&tv, &tv_start);
		tdif /= 1000000;
		if (torture_setting_bool(tctx, "showall", false) || 
		    !NT_STATUS_EQUAL(status2, status2_p) ||
		    res != res2) {
			torture_comment(tctx, "\n%-20s %-70s\n%-20s %-70s %4s %4s  %s/%s\n",
			       bit_string(mem_ctx, share_access_bits, b_sa1, nbits1),
			       bit_string(mem_ctx, access_mask_bits,  b_am1, nbits2),
			       bit_string(mem_ctx, share_access_bits, b_sa2, nbits1),
			       bit_string(mem_ctx, access_mask_bits,  b_am2, nbits2),
			       resultstr(res),
			       resultstr(res2),
			       nt_errstr(status2),
			       nt_errstr(status2_p));
			fflush(stdout);
		}
		
		if (res != res2 ||
		    !NT_STATUS_EQUAL(status2, status2_p)) {
			CHECK_MAX_FAILURES(failed);
			correct = false;
		}
		
		talloc_free(mem_ctx);
	}

failed:
	smbcli_unlink(cli1->tree, fname);
	
	torture_comment(tctx, "finshed ntdenytest (%d failures)\n", failures);
	return correct;
}



/*
  a denytest for ntcreatex
 */
bool torture_ntdenytest1(struct torture_context *tctx, 
			 struct smbcli_state *cli, int client)
{
	extern int torture_seed;

	srandom(torture_seed + client);

	torture_comment(tctx, "starting ntdenytest1 client %d\n", client);

	return torture_ntdenytest(tctx, cli, cli, client);
}

/*
  a denytest for ntcreatex
 */
bool torture_ntdenytest2(struct torture_context *torture, 
			 struct smbcli_state *cli1,
			 struct smbcli_state *cli2)
{
	return torture_ntdenytest(torture, cli1, cli2, 0);
}

#define COMPARE_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		failed = true; \
	}} while (0)

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
		      "(%s) wrong value for %s  0x%x - should be 0x%x\n", \
		       __location__, #v, (int)(v), (int)correct); \
		ret = false; \
	}} while (0)

/*
  test sharing of handles with DENY_DOS on a single connection
*/
bool torture_denydos_sharing(struct torture_context *tctx, 
			     struct smbcli_state *cli)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = "\\torture_denydos.txt";
	NTSTATUS status;
	int fnum1 = -1, fnum2 = -1;
	bool ret = true;
	union smb_setfileinfo sfinfo;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(cli);

	torture_comment(tctx, "Checking DENY_DOS shared handle semantics\n");
	smbcli_unlink(cli->tree, fname);

	io.openx.level = RAW_OPEN_OPENX;
	io.openx.in.fname = fname;
	io.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_DOS;
	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	io.openx.in.search_attrs = 0;
	io.openx.in.file_attrs = 0;
	io.openx.in.write_time = 0;
	io.openx.in.size = 0;
	io.openx.in.timeout = 0;

	torture_comment(tctx, "openx twice with RDWR/DENY_DOS\n");
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.openx.out.file.fnum;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.openx.out.file.fnum;

	torture_comment(tctx, "fnum1=%d fnum2=%d\n", fnum1, fnum2);

	sfinfo.generic.level = RAW_SFILEINFO_POSITION_INFORMATION;
	sfinfo.position_information.in.file.fnum = fnum1;
	sfinfo.position_information.in.position = 1000;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "two handles should be same file handle\n");
	finfo.position_information.level = RAW_FILEINFO_POSITION_INFORMATION;
	finfo.position_information.in.file.fnum = fnum1;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(finfo.position_information.out.position, 1000);

	finfo.position_information.in.file.fnum = fnum2;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(finfo.position_information.out.position, 1000);


	smbcli_close(cli->tree, fnum1);
	smbcli_close(cli->tree, fnum2);

	torture_comment(tctx, "openx twice with RDWR/DENY_NONE\n");
	io.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | OPENX_MODE_DENY_NONE;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.openx.out.file.fnum;

	io.openx.in.open_func = OPENX_OPEN_FUNC_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.openx.out.file.fnum;

	torture_comment(tctx, "fnum1=%d fnum2=%d\n", fnum1, fnum2);

	torture_comment(tctx, "two handles should be separate\n");
	sfinfo.generic.level = RAW_SFILEINFO_POSITION_INFORMATION;
	sfinfo.position_information.in.file.fnum = fnum1;
	sfinfo.position_information.in.position = 1000;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	finfo.position_information.level = RAW_FILEINFO_POSITION_INFORMATION;
	finfo.position_information.in.file.fnum = fnum1;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(finfo.position_information.out.position, 1000);

	finfo.position_information.in.file.fnum = fnum2;
	status = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(finfo.position_information.out.position, 0);

done:
	smbcli_close(cli->tree, fnum1);
	smbcli_close(cli->tree, fnum2);
	smbcli_unlink(cli->tree, fname);

	return ret;
}

#define CXD_MATCHES(_cxd, i)                                            \
	((cxd_known[i].cxd_test == (_cxd)->cxd_test) &&                 \
	 (cxd_known[i].cxd_flags == (_cxd)->cxd_flags) &&               \
	 (cxd_known[i].cxd_access1 == (_cxd)->cxd_access1) &&           \
	 (cxd_known[i].cxd_sharemode1 == (_cxd)->cxd_sharemode1) &&     \
	 (cxd_known[i].cxd_access2 == (_cxd)->cxd_access2) &&           \
	 (cxd_known[i].cxd_sharemode2 == (_cxd)->cxd_sharemode2))

static int cxd_find_known(struct createx_data *cxd)
{
	static int i = -1;

	/* Optimization for tests which we don't have results saved for. */
	if ((cxd->cxd_test == CXD_TEST_CREATEX_ACCESS_EXHAUSTIVE) ||
	    (cxd->cxd_test == CXD_TEST_CREATEX_SHAREMODE_EXTENDED))
		return -1;

	/* Optimization: If our cxd_known table is too large, it hurts test
	 * performance to search through the entire table each time. If the
	 * caller can pass in the previous result, we can try the next entry.
	 * This works if results are taken directly from the same code. */
	i++;
	if ((i >= 0) && (i < sizeof(cxd_known) / sizeof(cxd_known[0])) &&
	    CXD_MATCHES(cxd, i))
		return i;

	for (i = 0; i < (sizeof(cxd_known) / sizeof(cxd_known[0])); i++) {
		if (CXD_MATCHES(cxd, i))
			return i;
	}

	return -1;
}

#define CREATEX_NAME "\\createx_dir"

static bool createx_make_dir(struct torture_context *tctx,
    struct smbcli_tree *tree, TALLOC_CTX *mem_ctx, const char *fname)
{
	bool ret = true;
	NTSTATUS status;

	status = smbcli_mkdir(tree, fname);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	return ret;
}

static bool createx_make_file(struct torture_context *tctx,
    struct smbcli_tree *tree, TALLOC_CTX *mem_ctx, const char *fname)
{
	union smb_open open_parms;
	bool ret = true;
	NTSTATUS status;

	ZERO_STRUCT(open_parms);
	open_parms.generic.level = RAW_OPEN_NTCREATEX;
	open_parms.ntcreatex.in.flags = 0;
	open_parms.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	open_parms.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	open_parms.ntcreatex.in.share_access = 0;
	open_parms.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	open_parms.ntcreatex.in.create_options = 0;
	open_parms.ntcreatex.in.fname = fname;

	status = smb_raw_open(tree, mem_ctx, &open_parms);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_close(tree, open_parms.ntcreatex.out.file.fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

 done:
	return ret;
}

static void createx_fill_dir(union smb_open *open_parms, int accessmode,
    int sharemode, const char *fname)
{
	ZERO_STRUCTP(open_parms);
	open_parms->generic.level = RAW_OPEN_NTCREATEX;
	open_parms->ntcreatex.in.flags = 0;
	open_parms->ntcreatex.in.access_mask = accessmode;
	open_parms->ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	open_parms->ntcreatex.in.share_access = sharemode;
	open_parms->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	open_parms->ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	open_parms->ntcreatex.in.fname = fname;
}

static void createx_fill_file(union smb_open *open_parms, int accessmode,
    int sharemode, const char *fname)
{
	ZERO_STRUCTP(open_parms);
	open_parms->generic.level = RAW_OPEN_NTCREATEX;
	open_parms->ntcreatex.in.flags = 0;
	open_parms->ntcreatex.in.access_mask = accessmode;
	open_parms->ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	open_parms->ntcreatex.in.share_access = sharemode;
	open_parms->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	open_parms->ntcreatex.in.create_options = 0;
	open_parms->ntcreatex.in.fname = fname;
	open_parms->ntcreatex.in.root_fid.fnum = 0;
}

static int data_file_fd = -1;

#define KNOWN   "known"
#define CHILD   "child"
static bool createx_test_dir(struct torture_context *tctx,
    struct smbcli_tree *tree, int fnum, TALLOC_CTX *mem_ctx, NTSTATUS *result)
{
	bool ret = true;
	NTSTATUS status;
	union smb_open open_parms;

	/* bypass original handle to guarantee creation */
	ZERO_STRUCT(open_parms);
	open_parms.generic.level = RAW_OPEN_NTCREATEX;
	open_parms.ntcreatex.in.flags = 0;
	open_parms.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	open_parms.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	open_parms.ntcreatex.in.share_access = 0;
	open_parms.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	open_parms.ntcreatex.in.create_options = 0;
	open_parms.ntcreatex.in.fname = CREATEX_NAME "\\" KNOWN;

	status = smb_raw_open(tree, mem_ctx, &open_parms);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(tree, open_parms.ntcreatex.out.file.fnum);

	result[CXD_DIR_ENUMERATE] = NT_STATUS_OK;

	/* try to create a child */
	ZERO_STRUCT(open_parms);
	open_parms.generic.level = RAW_OPEN_NTCREATEX;
	open_parms.ntcreatex.in.flags = 0;
	open_parms.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	open_parms.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	open_parms.ntcreatex.in.share_access = 0;
	open_parms.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	open_parms.ntcreatex.in.create_options = 0;
	open_parms.ntcreatex.in.fname = CHILD;
	open_parms.ntcreatex.in.root_fid.fnum = fnum;

	result[CXD_DIR_CREATE_CHILD] =
	    smb_raw_open(tree, mem_ctx, &open_parms);
	smbcli_close(tree, open_parms.ntcreatex.out.file.fnum);

	/* try to traverse dir to known good file */
	ZERO_STRUCT(open_parms);
	open_parms.generic.level = RAW_OPEN_NTCREATEX;
	open_parms.ntcreatex.in.flags = 0;
	open_parms.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	open_parms.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	open_parms.ntcreatex.in.share_access = 0;
	open_parms.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	open_parms.ntcreatex.in.create_options = 0;
	open_parms.ntcreatex.in.fname = KNOWN;
	open_parms.ntcreatex.in.root_fid.fnum = fnum;

	result[CXD_DIR_TRAVERSE] =
	    smb_raw_open(tree, mem_ctx, &open_parms);


	smbcli_close(tree, open_parms.ntcreatex.out.file.fnum);
	smbcli_unlink(tree, CREATEX_NAME "\\" KNOWN);
	smbcli_unlink(tree, CREATEX_NAME "\\" CHILD);

 done:
	return ret;
}

static bool createx_test_file(struct torture_context *tctx,
    struct smbcli_tree *tree, int fnum, TALLOC_CTX *mem_ctx, NTSTATUS *result)
{
	union smb_read rd;
	union smb_write wr;
	char buf[256] = "";

	memset(&rd, 0, sizeof(rd));
	rd.readx.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = sizeof(buf);
	rd.readx.in.maxcnt = sizeof(buf);
	rd.readx.out.data = (uint8_t *)buf;

	result[CXD_FILE_READ] = smb_raw_read(tree, &rd);

	memset(&wr, 0, sizeof(wr));
	wr.writex.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.count = sizeof(buf);
	wr.writex.in.data = (uint8_t *)buf;

	result[CXD_FILE_WRITE] = smb_raw_write(tree, &wr);

	memset(&rd, 0, sizeof(rd));
	rd.readx.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = sizeof(buf);
	rd.readx.in.maxcnt = sizeof(buf);
	rd.readx.in.read_for_execute = 1;
	rd.readx.out.data = (uint8_t *)buf;

	result[CXD_FILE_EXECUTE] = smb_raw_read(tree, &rd);

	return true;
}

/* TODO When redirecting stdout to a file, the progress bar really screws up
 * the output. Could use a switch "--noprogress", or direct the progress bar to
 * stderr? No other solution? */
static void createx_progress_bar(struct torture_context *tctx, unsigned int i,
    unsigned int total, unsigned int skipped)
{
	if (torture_setting_bool(tctx, "progress", true)) {
		torture_comment(tctx, "%5d/%5d (%d skipped)\r", i, total,
		    skipped);
		fflush(stdout);
	}
}

static bool torture_createx_specific(struct torture_context *tctx, struct
    smbcli_state *cli, struct smbcli_state *cli2, TALLOC_CTX *mem_ctx, struct
    createx_data *cxd, int estimated_count)
{
	static int call_count = 1;
	static int unskipped_call_count = 1;
	const char *fname = CREATEX_NAME;
	int fnum = -1, fnum2 = -1, res, i;
	union smb_open open_parms1, open_parms2;
	bool ret = true;
	bool is_dir = cxd->cxd_flags & CXD_FLAGS_DIRECTORY;
	NTSTATUS *result = &cxd->cxd_result[0];
	NTSTATUS *result2 = &cxd->cxd_result2[0];
	bool found = false, failed = false;

	bool (*make_func)(struct torture_context *,
	    struct smbcli_tree *, TALLOC_CTX *, const char *);
	void (*fill_func)(union smb_open *, int, int, const char *);
	bool (*test_func)(struct torture_context *,
	    struct smbcli_tree *, int, TALLOC_CTX *, NTSTATUS *);
	NTSTATUS (*destroy_func)(struct smbcli_tree *, const char *);

	if (is_dir) {
		make_func = createx_make_dir;
		fill_func = createx_fill_dir;
		test_func = createx_test_dir;
		destroy_func = smbcli_rmdir;
	} else {
		make_func = createx_make_file;
		fill_func = createx_fill_file;
		test_func = createx_test_file;
		destroy_func = smbcli_unlink;
	}

	/* Skip all SACL related tests. */
	if ((!torture_setting_bool(tctx, "sacl_support", true)) &&
	    ((cxd->cxd_access1 & SEC_FLAG_SYSTEM_SECURITY) ||
	     (cxd->cxd_access2 & SEC_FLAG_SYSTEM_SECURITY)))
		goto done;

	if (cxd->cxd_flags & CXD_FLAGS_MAKE_BEFORE_CREATEX) {
		ret = make_func(tctx, cli->tree, mem_ctx, fname);
		if (!ret) {
			torture_result(tctx, TORTURE_FAIL,
				"Initial creation failed\n");
			goto done;
		}
	}

	/* Initialize. */
	fill_func(&open_parms1, cxd->cxd_access1, cxd->cxd_sharemode1, fname);

	if (cxd->cxd_test == CXD_TEST_CREATEX_SHAREMODE) {
		fill_func(&open_parms2, cxd->cxd_access2, cxd->cxd_sharemode2,
		    fname);
	}

	for (i = CXD_CREATEX + 1; i < CXD_MAX; i++) {
		result[i] = NT_STATUS_UNSUCCESSFUL;
		result2[i] = NT_STATUS_UNSUCCESSFUL;
	}

	/* Perform open(s). */
	result[CXD_CREATEX] = smb_raw_open(cli->tree, mem_ctx, &open_parms1);
	if (NT_STATUS_IS_OK(result[CXD_CREATEX])) {
		fnum = open_parms1.ntcreatex.out.file.fnum;
		ret = test_func(tctx, cli->tree, fnum, mem_ctx, result);
		smbcli_close(cli->tree, fnum);
	}

	if (cxd->cxd_test == CXD_TEST_CREATEX_SHAREMODE) {
		result2[CXD_CREATEX] = smb_raw_open(cli2->tree, mem_ctx,
		    &open_parms2);
		if (NT_STATUS_IS_OK(result2[CXD_CREATEX])) {
			fnum2 = open_parms2.ntcreatex.out.file.fnum;
			ret = test_func(tctx, cli2->tree, fnum2, mem_ctx,
			    result2);
			smbcli_close(cli2->tree, fnum2);
		}
	}

	if (data_file_fd >= 0) {
		found = true;
		res = write(data_file_fd, &cxd, sizeof(cxd));
		if (res != sizeof(cxd)) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s): write failed: %s!",
				__location__, strerror(errno));
			ret = false;
		}
	} else if ((res = cxd_find_known(cxd)) >= 0) {
		found = true;
		for (i = 0; i < CXD_MAX; i++) {
			/* Note: COMPARE_STATUS will set the "failed" bool. */
			COMPARE_STATUS(result[i], cxd_known[res].cxd_result[i]);
			if (i == 0 && !NT_STATUS_IS_OK(result[i]))
				break;

			if (cxd->cxd_test == CXD_TEST_CREATEX_SHAREMODE) {
				COMPARE_STATUS(result2[i],
				    cxd_known[res].cxd_result2[i]);
				if (i == 0 && !NT_STATUS_IS_OK(result2[i]))
					break;
			}
		}
	}

	/* We print if its not in the "cxd_known" list or if we fail. */
	if (!found || failed) {
		torture_comment(tctx,
		    "  { .cxd_test = %d, .cxd_flags = %#3x, "
		    ".cxd_access1 = %#10x, .cxd_sharemode1=%1x, "
		    ".cxd_access2=%#10x, .cxd_sharemode2=%1x, "
		    ".cxd_result = { ", cxd->cxd_test, cxd->cxd_flags,
		    cxd->cxd_access1, cxd->cxd_sharemode1, cxd->cxd_access2,
		    cxd->cxd_sharemode2);
		for (i = 0; i < CXD_MAX; i++) {
			torture_comment(tctx, "%s, ", nt_errstr(result[i]));
			if (i == 0 && !NT_STATUS_IS_OK(result[i]))
				break;
		}
		torture_comment(tctx, "}");
		if (cxd->cxd_test == CXD_TEST_CREATEX_SHAREMODE) {
			torture_comment(tctx, ", .cxd_result2 = { ");
			for (i = 0; i < CXD_MAX; i++) {
				torture_comment(tctx, "%s, ",
						nt_errstr(result2[i]));
				if (i == 0 && !NT_STATUS_IS_OK(result2[i]))
					break;
			}
			torture_comment(tctx, "}");
		}
		torture_comment(tctx, "}, \n");
	} else {
		createx_progress_bar(tctx, call_count, estimated_count,
		    call_count - unskipped_call_count);
	}
	/* Count tests that we didn't skip. */
	unskipped_call_count++;
 done:
	call_count++;

	destroy_func(cli->tree, fname);
	return ret;
}

uint32_t sec_access_bit_groups[] = {
	SEC_RIGHTS_FILE_READ,
	SEC_RIGHTS_FILE_WRITE,
	SEC_RIGHTS_FILE_EXECUTE
};
#define NUM_ACCESS_GROUPS     (sizeof(sec_access_bit_groups) / sizeof(uint32_t))
#define ACCESS_GROUPS_COUNT   ((1 << NUM_ACCESS_GROUPS))
#define BITSINBYTE 8

/* Note: See NTCREATEX_SHARE_ACCESS_{NONE,READ,WRITE,DELETE} for share mode
 * declarations. */
#define NUM_SHAREMODE_PERMUTATIONS 8

/**
 * NTCREATEX and SHARE MODE test.
 *
 * Open with combinations of (access_mode, share_mode).
 *  - Check status
 * Open 2nd time with combination of (access_mode2, share_mode2).
 *  - Check status
 * Perform operations to verify?
 *  - Read
 *  - Write
 *  - Delete
 */
bool torture_createx_sharemodes(struct torture_context *tctx,
				struct smbcli_state *cli,
				struct smbcli_state *cli2,
				bool dir,
				bool extended)
{
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	int i, j, est;
	int gp1, gp2; /* group permuters */
	struct createx_data cxd = {0};
	int num_access_bits1 = sizeof(cxd.cxd_access1) * BITSINBYTE;
	int num_access_bits2 = sizeof(cxd.cxd_access2) * BITSINBYTE;

	mem_ctx = talloc_init("createx_sharemodes");
	if (!mem_ctx)
		return false;

	if (!torture_setting_bool(tctx, "sacl_support", true))
		torture_warning(tctx, "Skipping SACL related tests!\n");

	cxd.cxd_test = extended ? CXD_TEST_CREATEX_SHAREMODE_EXTENDED :
	    CXD_TEST_CREATEX_SHAREMODE;
	cxd.cxd_flags = dir ? CXD_FLAGS_DIRECTORY: 0;

	/* HACK for progress bar: figure out estimated count. */
	est = (NUM_SHAREMODE_PERMUTATIONS * NUM_SHAREMODE_PERMUTATIONS) *
	    ((ACCESS_GROUPS_COUNT * ACCESS_GROUPS_COUNT) +
	     (extended ? num_access_bits1 * num_access_bits2 : 0));

	/* Blank slate. */
	smbcli_deltree(cli->tree, CREATEX_NAME);
	smbcli_unlink(cli->tree, CREATEX_NAME);

	/* Choose 2 random share modes. */
	for (cxd.cxd_sharemode1 = 0;
	     cxd.cxd_sharemode1 < NUM_SHAREMODE_PERMUTATIONS;
	     cxd.cxd_sharemode1++) {
		for (cxd.cxd_sharemode2 = 0;
		     cxd.cxd_sharemode2 < NUM_SHAREMODE_PERMUTATIONS;
		     cxd.cxd_sharemode2++) {

			/* Permutate through our access_bit_groups. */
			for (gp1 = 0; gp1 < ACCESS_GROUPS_COUNT; gp1++) {
				for (gp2 = 0; gp2 < ACCESS_GROUPS_COUNT; gp2++)
				{
					cxd.cxd_access1 = cxd.cxd_access2 = 0;

					for (i = 0; i < NUM_ACCESS_GROUPS; i++)
					{
						cxd.cxd_access1 |=
						    (gp1 & (1 << i)) ?
						    sec_access_bit_groups[i]:0;
						cxd.cxd_access2 |=
						    (gp2 & (1 << i)) ?
						    sec_access_bit_groups[i]:0;
					}

					torture_createx_specific(tctx, cli,
					   cli2, mem_ctx, &cxd, est);
				}
			}

			/* Only do the single access bits on an extended run. */
			if (!extended)
				continue;

			for (i = 0; i < num_access_bits1; i++) {
				for (j = 0; j < num_access_bits2; j++) {
					cxd.cxd_access1 = 1ull << i;
					cxd.cxd_access2 = 1ull << j;

					torture_createx_specific(tctx, cli,
					    cli2, mem_ctx, &cxd, est);
				}
			}
		}
	}
	torture_comment(tctx, "\n");

	talloc_free(mem_ctx);
	return ret;
}

bool torture_createx_sharemodes_file(struct torture_context *tctx,
    struct smbcli_state *cli, struct smbcli_state *cli2)
{
	return torture_createx_sharemodes(tctx, cli, cli2, false, false);
}

bool torture_createx_sharemodes_dir(struct torture_context *tctx,
    struct smbcli_state *cli, struct smbcli_state *cli2)
{
	return torture_createx_sharemodes(tctx, cli, cli2, true, false);
}

bool torture_createx_access(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	uint32_t group_permuter;
	uint32_t i;
	struct createx_data cxd = {0};
	int est;
	int num_access_bits = sizeof(cxd.cxd_access1) * BITSINBYTE;

	mem_ctx = talloc_init("createx_dir");
	if (!mem_ctx)
		return false;

	if (!torture_setting_bool(tctx, "sacl_support", true))
		torture_warning(tctx, "Skipping SACL related tests!\n");

	cxd.cxd_test = CXD_TEST_CREATEX_ACCESS;

	/* HACK for progress bar: figure out estimated count. */
	est = CXD_FLAGS_COUNT * (ACCESS_GROUPS_COUNT + (num_access_bits * 3));

	/* Blank slate. */
	smbcli_deltree(cli->tree, CREATEX_NAME);
	smbcli_unlink(cli->tree, CREATEX_NAME);

	for (cxd.cxd_flags = 0; cxd.cxd_flags <= CXD_FLAGS_MASK;
	     cxd.cxd_flags++) {
		/**
		 * This implements a basic permutation of all elements of
		 * 'bit_group'.  group_permuter is a bit field representing
		 * which groups to turn on.
		*/
		for (group_permuter = 0; group_permuter < (1 <<
			NUM_ACCESS_GROUPS); group_permuter++) {
			for (i = 0, cxd.cxd_access1 = 0;
			     i < NUM_ACCESS_GROUPS; i++) {
				cxd.cxd_access1 |= (group_permuter & (1 << i))
				    ? sec_access_bit_groups[i] : 0;
			}

			torture_createx_specific(tctx, cli, NULL, mem_ctx,
			    &cxd, est);
		}
		for (i = 0; i < num_access_bits; i++) {
			/* And now run through the single access bits. */
			cxd.cxd_access1 = 1 << i;
			torture_createx_specific(tctx, cli, NULL, mem_ctx,
			    &cxd, est);

			/* Does SEC_FLAG_MAXIMUM_ALLOWED override? */
			cxd.cxd_access1 |= SEC_FLAG_MAXIMUM_ALLOWED;
			torture_createx_specific(tctx, cli, NULL, mem_ctx,
			    &cxd, est);

			/* What about SEC_FLAG_SYSTEM_SECURITY? */
			cxd.cxd_access1 |= SEC_FLAG_SYSTEM_SECURITY;
			torture_createx_specific(tctx, cli, NULL, mem_ctx,
			    &cxd, est);
		}
	}

	talloc_free(mem_ctx);
	return ret;
}

#define ACCESS_KNOWN_MASK 0xF31F01FFull

bool torture_createx_access_exhaustive(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	char *data_file;
	TALLOC_CTX *mem_ctx;
	bool ret = true, first;
	uint32_t i;
	struct createx_data cxd = {0};

	mem_ctx = talloc_init("createx_dir");
	if (!mem_ctx)
		return false;

	if (!torture_setting_bool(tctx, "sacl_support", true))
		torture_warning(tctx, "Skipping SACL related tests!\n");

	data_file = getenv("CREATEX_DATA");
	if (data_file) {
		data_file_fd = open(data_file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if (data_file_fd < 0) {
			torture_result(tctx, TORTURE_FAIL,
				"(%s): data file open failedu: %s!",
				__location__, strerror(errno));
			ret = false;
			goto done;
		}
	}

	/* Blank slate. */
	smbcli_deltree(cli->tree, CREATEX_NAME);
	smbcli_unlink(cli->tree, CREATEX_NAME);

	cxd.cxd_test = CXD_TEST_CREATEX_ACCESS_EXHAUSTIVE;

	for (cxd.cxd_flags = 0; cxd.cxd_flags <= CXD_FLAGS_MASK;
	     cxd.cxd_flags++) {
		for (i = 0, first = true; (i != 0) || first; first = false,
		     i = ((i | ~ACCESS_KNOWN_MASK) + 1) & ACCESS_KNOWN_MASK) {
			cxd.cxd_access1 = i;
			ret = torture_createx_specific(tctx, cli, NULL,
			    mem_ctx, &cxd, 0);
			if (!ret)
				break;
		}
	}

	close(data_file_fd);
	data_file_fd = -1;

 done:
	talloc_free(mem_ctx);
	return ret;
}

#define MAXIMUM_ALLOWED_FILE    "torture_maximum_allowed"
bool torture_maximum_allowed(struct torture_context *tctx,
    struct smbcli_state *cli)
{
	struct security_descriptor *sd, *sd_orig;
	union smb_open io;
	static TALLOC_CTX *mem_ctx;
	int fnum, i;
	bool ret = true;
	NTSTATUS status;
	union smb_fileinfo q;
	const char *owner_sid;
	bool has_restore_privilege, has_backup_privilege;

	mem_ctx = talloc_init("torture_maximum_allowed");

	if (!torture_setting_bool(tctx, "sacl_support", true))
		torture_warning(tctx, "Skipping SACL related tests!\n");

	sd = security_descriptor_dacl_create(mem_ctx,
	    0, NULL, NULL,
	    SID_NT_AUTHENTICATED_USERS,
	    SEC_ACE_TYPE_ACCESS_ALLOWED,
	    SEC_RIGHTS_FILE_READ,
	    0, NULL);

	/* Blank slate */
	smbcli_unlink(cli->tree, MAXIMUM_ALLOWED_FILE);

	/* create initial file with restrictive SD */
	memset(&io, 0, sizeof(io));
	io.generic.level = RAW_OPEN_NTTRANS_CREATE;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.fname = MAXIMUM_ALLOWED_FILE;
	io.ntcreatex.in.sec_desc = sd;

	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* the correct answers for this test depends on whether the
	   user has restore privileges. To find that out we first need
	   to know our SID - get it from the owner_sid of the file we
	   just created */
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.fnum = fnum;
	q.query_secdesc.in.secinfo_flags = SECINFO_DACL | SECINFO_OWNER;
	status = smb_raw_fileinfo(cli->tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd_orig = q.query_secdesc.out.sd;

	owner_sid = dom_sid_string(tctx, sd_orig->owner_sid);

	status = torture_check_privilege(cli, 
					 owner_sid, 
					 sec_privilege_name(SEC_PRIV_RESTORE));
	has_restore_privilege = NT_STATUS_IS_OK(status);
	torture_comment(tctx, "Checked SEC_PRIV_RESTORE for %s - %s\n", 
			owner_sid,
			has_restore_privilege?"Yes":"No");

	status = torture_check_privilege(cli, 
					 owner_sid, 
					 sec_privilege_name(SEC_PRIV_BACKUP));
	has_backup_privilege = NT_STATUS_IS_OK(status);
	torture_comment(tctx, "Checked SEC_PRIV_BACKUP for %s - %s\n", 
			owner_sid,
			has_backup_privilege?"Yes":"No");

	smbcli_close(cli->tree, fnum);

	for (i = 0; i < 32; i++) {
		uint32_t mask = SEC_FLAG_MAXIMUM_ALLOWED | (1u << i);
		uint32_t ok_mask = SEC_RIGHTS_FILE_READ | SEC_GENERIC_READ | 
			SEC_STD_DELETE | SEC_STD_WRITE_DAC;

		if (has_restore_privilege) {
			ok_mask |= SEC_RIGHTS_PRIV_RESTORE;
		}
		if (has_backup_privilege) {
			ok_mask |= SEC_RIGHTS_PRIV_BACKUP;
		}

		/* Skip all SACL related tests. */
		if ((!torture_setting_bool(tctx, "sacl_support", true)) &&
		    (mask & SEC_FLAG_SYSTEM_SECURITY))
			continue;

		memset(&io, 0, sizeof(io));
		io.generic.level = RAW_OPEN_NTTRANS_CREATE;
		io.ntcreatex.in.access_mask = mask;
		io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		io.ntcreatex.in.impersonation =
		    NTCREATEX_IMPERSONATION_ANONYMOUS;
		io.ntcreatex.in.fname = MAXIMUM_ALLOWED_FILE;

		status = smb_raw_open(cli->tree, mem_ctx, &io);
		if (mask & ok_mask ||
		    mask == SEC_FLAG_MAXIMUM_ALLOWED) {
			CHECK_STATUS(status, NT_STATUS_OK);
		} else {
			if (mask & SEC_FLAG_SYSTEM_SECURITY) {
				CHECK_STATUS(status, NT_STATUS_PRIVILEGE_NOT_HELD);
			} else {
				CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
			}
		}

		fnum = io.ntcreatex.out.file.fnum;

		smbcli_close(cli->tree, fnum);
	}

 done:
	smbcli_unlink(cli->tree, MAXIMUM_ALLOWED_FILE);
	return ret;
}
