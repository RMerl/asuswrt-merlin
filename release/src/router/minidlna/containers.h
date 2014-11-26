/* MiniDLNA media server
 * Copyright (C) 2014  NETGEAR
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */

struct magic_container_s {
	const char *name;
	const char *objectid_match;
	const char **objectid;
	const char *objectid_sql;
	const char *parentid_sql;
	const char *refid_sql;
	const char *child_count;
	const char *where;
	const char *orderby;
	int max_count;
	int required_flags;
};

extern struct magic_container_s magic_containers[];

struct magic_container_s *in_magic_container(const char *id, int flags, const char **real_id);
struct magic_container_s *check_magic_container(const char *id, int flags);
