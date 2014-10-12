/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief  Check the idmap database.
 * @author Gregor Beck <gb@sernet.de>
 * @date   Mar 2011
 */

#ifndef NET_IDMAP_CHECK_H
#define NET_IDMAP_CHECK_H

#include <stdbool.h>

struct net_context;

struct check_options {
	bool test;
	bool verbose;
	bool lock;
	bool automatic;
	bool force;
	bool repair;
};

int net_idmap_check_db(const char* db, const struct check_options* opts);

#endif /* NET_IDMAP_CHECK_H */

/*Local Variables:*/
/*mode: c*/
/*End:*/
