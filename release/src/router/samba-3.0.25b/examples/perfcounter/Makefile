#
# Copyright (C) Marcin Krzysztof Porwit    2005
#  
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# 

SAMBA_SRC_DIR=../../source
TDB_SRC_DIR=$(SAMBA_SRC_DIR)/tdb

CFLAGS = -g -I$(SAMBA_SRC_DIR)/include -I$(TDB_SRC_DIR)
CC = gcc

PROGS = perfcount
TDB_OBJ = $(TDB_SRC_DIR)/tdb.o $(TDB_SRC_DIR)/spinlock.o $(TDB_SRC_DIR)/tdbback.o
PERF_WRITER_OBJ = perf_writer.o perf_writer_mem.o perf_writer_util.o perf_writer_cpu.o perf_writer_process.o perf_writer_disk.o

default: $(PROGS)

$(TDB_OBJ):
	cd $(TDB_SRC_DIR) && make

perfcount: $(PERF_WRITER_OBJ) $(TDB_OBJ)
	$(CC) $(CFLAGS) -o perfcount $(PERF_WRITER_OBJ) $(TDB_OBJ)

clean:
	rm -f $(PROGS) *.o *~ *% core
