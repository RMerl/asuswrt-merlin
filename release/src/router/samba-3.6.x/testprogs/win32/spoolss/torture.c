/*
   Unix SMB/CIFS implementation.
   SMB torture UI functions

   Copyright (C) Jelmer Vernooij 2006

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

#include "testspoolss.h"
#include "torture.h"

/****************************************************************************
****************************************************************************/

void torture_warning(struct torture_context *context, const char *comment, ...)
{
	va_list ap;
	char tmp[1024];

#if 0
	if (!context->results->ui_ops->warning)
		return;
#endif

	va_start(ap, comment);
	if (vsprintf(tmp, comment, ap) == -1) {
		return;
	}
	va_end(ap);

	fprintf(stderr, "WARNING: %s\n", tmp);
#if 0
	context->results->ui_ops->warning(context, tmp);

	free(tmp);
#endif
}

/****************************************************************************
****************************************************************************/

void torture_result(struct torture_context *context,
		    enum torture_result result, const char *fmt, ...)
{
	va_list ap;
	char tmp[1024];

	va_start(ap, fmt);

	if (context->last_reason) {
		torture_warning(context, "%s", context->last_reason);
		free(context->last_reason);
		context->last_reason = NULL;
	}

	context->last_result = result;
	if (vsprintf(tmp, fmt, ap) == -1) {
		return;
	}
	context->last_reason = malloc(sizeof(tmp));
	if (!context->last_reason) {
		return;
	}
	memcpy(context->last_reason, tmp, sizeof(tmp));

	va_end(ap);
}

/****************************************************************************
****************************************************************************/

void torture_comment(struct torture_context *context, const char *comment, ...)
{
	va_list ap;
	char tmp[1024];
#if 0
	if (!context->results->ui_ops->comment)
		return;
#endif
	va_start(ap, comment);
	if (vsprintf(tmp, comment, ap) == -1) {
		return;
	}
	va_end(ap);

#if 0
	context->results->ui_ops->comment(context, tmp);
#endif
	fprintf(stdout, "%s\n", tmp);

#if 0
	free(tmp);
#endif
}
