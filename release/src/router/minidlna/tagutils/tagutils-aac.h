//=========================================================================
// FILENAME	: tagutils-aac.h
// DESCRIPTION	: AAC metadata reader
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

static int _get_aactags(char *file, struct song_metadata *psong);
static int _get_aacfileinfo(char *file, struct song_metadata *psong);
static off_t _aac_lookforatom(FILE *aac_fp, char *atom_path, unsigned int *atom_length);
