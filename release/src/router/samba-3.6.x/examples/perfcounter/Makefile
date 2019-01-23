#
# Copyright (C) Marcin Krzysztof Porwit    2005
#  
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
# 

SAMBA_SRC_DIR=../../source3
TDB_SRC_DIR=$(SAMBA_SRC_DIR)/../lib/tdb

CFLAGS = -g -I$(SAMBA_SRC_DIR) -I$(SAMBA_SRC_DIR)/include -I$(TDB_SRC_DIR)/include -I../../
CC = gcc

PROGS = perfcount
TDB_OBJ = $(TDB_SRC_DIR)/common/tdb.o $(TDB_SRC_DIR)/common/dump.o \
	$(TDB_SRC_DIR)/common/error.o $(TDB_SRC_DIR)/common/freelist.o \
	$(TDB_SRC_DIR)/common/io.o $(TDB_SRC_DIR)/common/lock.o \
	$(TDB_SRC_DIR)/common/open.o $(TDB_SRC_DIR)/common/transaction.o \
	$(TDB_SRC_DIR)/common/traverse.o
PERF_WRITER_OBJ = perf_writer.o perf_writer_mem.o perf_writer_util.o perf_writer_cpu.o perf_writer_process.o perf_writer_disk.o

default: $(PROGS)

perfcount: $(PERF_WRITER_OBJ)
	$(CC) $(CFLAGS) -o perfcount $(PERF_WRITER_OBJ) $(TDB_OBJ)

clean:
	rm -f $(PROGS) *.o *~ *% core
