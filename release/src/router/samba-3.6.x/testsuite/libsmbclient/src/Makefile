CC	=	gcc
CFLAGS	=	-Wall -W -O2 -g -I../../../source3/include
LFLAGS	=	-L../../../source3/bin

LIBS=	-L/usr/lib -lsmbclient -ltalloc -ltdb -lwbclient
INCPATH= -I. -I/usr/include -I./include
BIN_DIR=bin

SUB_DIRS=init fstat open unlink chown close opendir closedir rename lseek lseekdir stat \
		getdents creat read readdir mkdir rmdir write chmod open_print_job list_print_jobs \
		print_file telldir unlink_print_job

MAKEDIR = || exec false; \
	if test -d "$$dir"; then :; else \
	echo mkdir "$$dir"; \
	mkdir -p "$$dir" >/dev/null 2>&1 || \
	test -d "$$dir" || \
	mkdir "$$dir" || \
	exec false; fi || exec false

G_INIT		=	$(BIN_DIR)/init_1 \
			$(BIN_DIR)/init_2 \
			$(BIN_DIR)/init_3 \
			$(BIN_DIR)/init_4 

G_FSTAT		= 	$(BIN_DIR)/fstat_1 \
			$(BIN_DIR)/fstat_2 \
			$(BIN_DIR)/fstat_3 \
			$(BIN_DIR)/fstat_4 \
			$(BIN_DIR)/fstat_5 \
			$(BIN_DIR)/fstat_6

G_OPEN		= 	$(BIN_DIR)/open_1 \
			$(BIN_DIR)/open_2 \
			$(BIN_DIR)/open_3 \
			$(BIN_DIR)/open_4 \
			$(BIN_DIR)/open_5

G_UNLINK	= 	$(BIN_DIR)/unlink_1  \
			$(BIN_DIR)/unlink_2  \
			$(BIN_DIR)/unlink_3  \
			$(BIN_DIR)/unlink_4  \
			$(BIN_DIR)/unlink_5  \
			$(BIN_DIR)/unlink_6  \
			$(BIN_DIR)/unlink_7  \
			$(BIN_DIR)/unlink_8  \
			$(BIN_DIR)/unlink_9  \
			$(BIN_DIR)/unlink_10 \
			$(BIN_DIR)/unlink_11 \
			$(BIN_DIR)/unlink_12


G_CLOSE		= 	$(BIN_DIR)/close_1 \
			$(BIN_DIR)/close_2

G_OPENDIR	= 	$(BIN_DIR)/opendir_1 \
			$(BIN_DIR)/opendir_2 \
			$(BIN_DIR)/opendir_3 \
			$(BIN_DIR)/opendir_4 

G_CLOSEDIR	= 	$(BIN_DIR)/closedir_1 \
			$(BIN_DIR)/closedir_2 \
			$(BIN_DIR)/closedir_3 \
			$(BIN_DIR)/closedir_4 

G_RENAME	= 	$(BIN_DIR)/rename_1 \
			$(BIN_DIR)/rename_2 \
			$(BIN_DIR)/rename_3 \
			$(BIN_DIR)/rename_4 \
			$(BIN_DIR)/rename_5 \
			$(BIN_DIR)/rename_6 \
			$(BIN_DIR)/rename_7 \
			$(BIN_DIR)/rename_8 \
			$(BIN_DIR)/rename_9 \
			$(BIN_DIR)/rename_10 \
			$(BIN_DIR)/rename_11 \
			$(BIN_DIR)/rename_12 \
			$(BIN_DIR)/rename_13 \
			$(BIN_DIR)/rename_14

G_LSEEK		= 	$(BIN_DIR)/lseek_1 \
			$(BIN_DIR)/lseek_2 \
			$(BIN_DIR)/lseek_3 \
			$(BIN_DIR)/lseek_4 \
			$(BIN_DIR)/lseek_5 \
			$(BIN_DIR)/lseek_6 \
			$(BIN_DIR)/lseek_7 \
			$(BIN_DIR)/lseek_8

G_LSEEKDIR	= 	$(BIN_DIR)/lseekdir_1 \
			$(BIN_DIR)/lseekdir_2 \
			$(BIN_DIR)/lseekdir_3 \
			$(BIN_DIR)/lseekdir_4 \
			$(BIN_DIR)/lseekdir_5 \
			$(BIN_DIR)/lseekdir_6

G_STAT		= 	$(BIN_DIR)/stat_1 \
			$(BIN_DIR)/stat_2 \
			$(BIN_DIR)/stat_3 \
			$(BIN_DIR)/stat_4 \
			$(BIN_DIR)/stat_5 \
			$(BIN_DIR)/stat_6 \
			$(BIN_DIR)/stat_k

G_GETDENTS	= 	$(BIN_DIR)/getdents_1 \
			$(BIN_DIR)/getdents_2 \
			$(BIN_DIR)/getdents_3 \
			$(BIN_DIR)/getdents_4 \
			$(BIN_DIR)/getdents_5 

G_CREAT		= 	$(BIN_DIR)/creat_1 \
			$(BIN_DIR)/creat_2 \
			$(BIN_DIR)/creat_3

G_READ		= 	$(BIN_DIR)/read_1 \
			$(BIN_DIR)/read_2 \
			$(BIN_DIR)/read_3 \
			$(BIN_DIR)/read_4 \
			$(BIN_DIR)/read_5 \
			$(BIN_DIR)/read_6 \
			$(BIN_DIR)/read_7 \
			$(BIN_DIR)/read_8 \
			$(BIN_DIR)/read_9 \
			$(BIN_DIR)/read_10 \
			$(BIN_DIR)/read_11 \
			$(BIN_DIR)/read_12 \
			$(BIN_DIR)/read_13

G_MKDIR		= 	$(BIN_DIR)/mkdir_1 \
			$(BIN_DIR)/mkdir_2 \
			$(BIN_DIR)/mkdir_3 \
			$(BIN_DIR)/mkdir_4

G_RMDIR		= 	$(BIN_DIR)/rmdir_1 \
			$(BIN_DIR)/rmdir_2 \
			$(BIN_DIR)/rmdir_3 \
			$(BIN_DIR)/rmdir_4 \
			$(BIN_DIR)/rmdir_5 \
			$(BIN_DIR)/rmdir_6 

G_READDIR	=	$(BIN_DIR)/readdir_1 \
			$(BIN_DIR)/readdir_2 \
			$(BIN_DIR)/readdir_3 \
			$(BIN_DIR)/readdir_4 \
			$(BIN_DIR)/readdir_5 

G_WRITE		= 	$(BIN_DIR)/write_1 \
			$(BIN_DIR)/write_2 \
			$(BIN_DIR)/write_3 \
			$(BIN_DIR)/write_4 \
			$(BIN_DIR)/write_5 \
			$(BIN_DIR)/write_6 \
			$(BIN_DIR)/write_7 \
			$(BIN_DIR)/write_8 \
			$(BIN_DIR)/write_9 \
			$(BIN_DIR)/write_10 \
			$(BIN_DIR)/write_11 \
			$(BIN_DIR)/write_12 \
			$(BIN_DIR)/write_13 

G_TELLDIR	= 	$(BIN_DIR)/telldir_1 \
			$(BIN_DIR)/telldir_2 \
			$(BIN_DIR)/telldir_3 \
			$(BIN_DIR)/telldir_4 \
			$(BIN_DIR)/telldir_5

G_CHMOD		= 	$(BIN_DIR)/chmod_1

G_CHOWN		= 	$(BIN_DIR)/chown_1

G_PRINT_FILE	=	$(BIN_DIR)/print_file_1 \
			$(BIN_DIR)/print_file_2 \
			$(BIN_DIR)/print_file_3 \
			$(BIN_DIR)/print_file_4

G_OPEN_PRINT_JOB	=	$(BIN_DIR)/open_print_job_1 \
				$(BIN_DIR)/open_print_job_2

G_LIST_PRINT_JOBS	=	$(BIN_DIR)/list_print_jobs_1 \
				$(BIN_DIR)/list_print_jobs_2 \
				$(BIN_DIR)/list_print_jobs_3 \
				$(BIN_DIR)/list_print_jobs_4 \
				$(BIN_DIR)/list_print_jobs_5 \
				$(BIN_DIR)/list_print_jobs_6 \
				$(BIN_DIR)/list_print_jobs_7 

G_UNLINK_PRINT_JOB	=	$(BIN_DIR)/unlink_print_job_1 \
				$(BIN_DIR)/unlink_print_job_2 \
				$(BIN_DIR)/unlink_print_job_3 \
				$(BIN_DIR)/unlink_print_job_4 \
				$(BIN_DIR)/unlink_print_job_5 

GROUPS=	$(G_CHMOD) $(G_CHOWN) $(G_CLOSE) $(G_CLOSEDIR) $(G_CREAT) $(G_FSTAT)\
	$(G_GETDENTS) $(G_INIT) $(G_LIST_PRINT_JOBS) $(G_LSEEK) $(G_LSEEKDIR) \
	$(G_MKDIR) $(G_OPEN) $(G_OPENDIR) $(G_OPEN_PRINT_JOB) $(G_PRINT_FILE)\
	$(G_READ) $(G_READDIR) $(G_RENAME) $(G_RMDIR) $(G_STAT) $(G_TELLDIR) $(G_UNLINK) \
	$(G_UNLINK_PRINT_JOB) $(G_WRITE)

.c.o:
	@echo Compiling $*.c
	@$(CC) -c $(CFLAGS) -o $@ $(INCPATH) $<


all:	$(GROUPS)	

init:			$(G_INIT)
fstat:			$(G_FSTAT)
open:			$(G_OPEN)
unlink:			$(G_UNLINK)
chown:			$(G_CHOWN)
close:			$(G_CLOSE)
opendir:		$(G_OPENDIR)
closedir:		$(G_CLOSEDIR)
rename:			$(G_RENAME)
readdir:		$(G_READDIR)
lseek:			$(G_LSEEK)
lseekdir:		$(G_LSEEKDIR)
stat:			$(G_STAT)
getdents:		$(G_GETDENTS)
creat:			$(G_CREAT)
read:			$(G_READ)
mkdir:			$(G_MKDIR)
rmdir:			$(G_RMDIR)
write:			$(G_WRITE)
chmod:			$(G_CHMOD)
telldir:		$(G_TELLDIR)
print_file:		$(G_PRINT_FILE)
open_print_job:		$(G_OPEN_PRINT_JOB)
list_print_jobs:	$(G_LIST_PRINT_JOBS)
unlink_print_job:	$(G_UNLINK_PRINT_JOB)


$(BIN_DIR)/testsmbc:	testsmbc.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ testsmbc.o $(INCPATH) $(LIBS)

$(BIN_DIR)/init_1:	init/init_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ init/init_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/init_2:	init/init_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ init/init_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/init_3:	init/init_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ init/init_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/init_4:	init/init_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ init/init_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_1:	fstat/fstat_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_2:	fstat/fstat_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_3:	fstat/fstat_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_4:	fstat/fstat_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_5:	fstat/fstat_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/fstat_6:	fstat/fstat_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ fstat/fstat_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_1:	open/open_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ open/open_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_2:	open/open_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ open/open_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_3:	open/open_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ open/open_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_4:	open/open_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ open/open_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_5:	open/open_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ open/open_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_1:	unlink/unlink_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_2:	unlink/unlink_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_3:	unlink/unlink_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_4:	unlink/unlink_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_5:	unlink/unlink_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_6:	unlink/unlink_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_7:	unlink/unlink_7.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_8:	unlink/unlink_8.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_8.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_9:	unlink/unlink_9.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_9.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_10:	unlink/unlink_10.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_10.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_11:	unlink/unlink_11.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_11.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_12:	unlink/unlink_12.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ unlink/unlink_12.o $(INCPATH) $(LIBS)

$(BIN_DIR)/chown_1:	chown/chown_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ chown/chown_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/close_1:	close/close_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ close/close_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/close_2:	close/close_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ close/close_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/opendir_1:	opendir/opendir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ opendir/opendir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/opendir_2:	opendir/opendir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ opendir/opendir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/opendir_3:	opendir/opendir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ opendir/opendir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/opendir_4:	opendir/opendir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ opendir/opendir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/closedir_1:	closedir/closedir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ closedir/closedir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/closedir_2:	closedir/closedir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ closedir/closedir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/closedir_3:	closedir/closedir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ closedir/closedir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/closedir_4:	closedir/closedir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ closedir/closedir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_1:	rename/rename_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_2:	rename/rename_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_3:	rename/rename_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_4:	rename/rename_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_5:	rename/rename_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_6:	rename/rename_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_7:	rename/rename_7.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_8:	rename/rename_8.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_8.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_9:	rename/rename_9.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_9.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_10:	rename/rename_10.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_10.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_11:	rename/rename_11.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_11.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_12:	rename/rename_12.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_12.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_13:	rename/rename_13.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_13.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rename_14:	rename/rename_14.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rename/rename_14.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_1:	lseek/lseek_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_2:	lseek/lseek_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_3:	lseek/lseek_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_4:	lseek/lseek_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_5:	lseek/lseek_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_6:	lseek/lseek_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_7:	lseek/lseek_7.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseek_8:	lseek/lseek_8.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseek/lseek_8.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_1:	lseekdir/lseekdir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_2:	lseekdir/lseekdir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_3:	lseekdir/lseekdir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_4:	lseekdir/lseekdir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_5:	lseekdir/lseekdir_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/lseekdir_6:	lseekdir/lseekdir_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ lseekdir/lseekdir_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_1:	stat/stat_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_2:	stat/stat_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_3:	stat/stat_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_4:	stat/stat_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_5:	stat/stat_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_6:	stat/stat_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/stat_k:	stat/stat_k.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ stat/stat_k.o $(INCPATH) $(LIBS)

$(BIN_DIR)/getdents_1:	getdents/getdents_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ getdents/getdents_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/getdents_2:	getdents/getdents_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ getdents/getdents_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/getdents_3:	getdents/getdents_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ getdents/getdents_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/getdents_4:	getdents/getdents_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ getdents/getdents_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/getdents_5:	getdents/getdents_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ getdents/getdents_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/creat_1:	creat/creat_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ creat/creat_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/creat_2:	creat/creat_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ creat/creat_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/creat_3:	creat/creat_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ creat/creat_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/mkdir_1:	mkdir/mkdir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ mkdir/mkdir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/mkdir_2:	mkdir/mkdir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ mkdir/mkdir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/mkdir_3:	mkdir/mkdir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ mkdir/mkdir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/mkdir_4:	mkdir/mkdir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ mkdir/mkdir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/readdir_1:	readdir/readdir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ readdir/readdir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/readdir_2:	readdir/readdir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ readdir/readdir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/readdir_3:	readdir/readdir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ readdir/readdir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/readdir_4:	readdir/readdir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ readdir/readdir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/readdir_5:	readdir/readdir_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ readdir/readdir_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_1:	rmdir/rmdir_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_2:	rmdir/rmdir_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_3:	rmdir/rmdir_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_4:	rmdir/rmdir_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_5:	rmdir/rmdir_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/rmdir_6:	rmdir/rmdir_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ rmdir/rmdir_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_1:	write/write_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_2:	write/write_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_3:	write/write_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_4:	write/write_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_5:	write/write_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_6:	write/write_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_7:	write/write_7.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_8:	write/write_8.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_8.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_9:	write/write_9.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_9.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_10:	write/write_10.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_10.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_11:	write/write_11.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_11.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_12:	write/write_12.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_12.o $(INCPATH) $(LIBS)

$(BIN_DIR)/write_13:	write/write_13.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ write/write_13.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_1:	read/read_1.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_2:	read/read_2.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_3:	read/read_3.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_4:	read/read_4.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_5:	read/read_5.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_6:	read/read_6.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_7:	read/read_7.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_8:	read/read_8.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_8.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_9:	read/read_9.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_9.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_10:	read/read_10.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_10.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_11:	read/read_11.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_11.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_12:	read/read_12.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_12.o $(INCPATH) $(LIBS)

$(BIN_DIR)/read_13:	read/read_13.o
	@echo Linking $@
	@$(CC) $(LFLAGS) -o $@ read/read_13.o $(INCPATH) $(LIBS)

$(BIN_DIR)/chmod_1:	chmod/chmod_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ chmod/chmod_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/telldir_1:	telldir/telldir_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ telldir/telldir_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/telldir_2:	telldir/telldir_2.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ telldir/telldir_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/telldir_3:	telldir/telldir_3.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ telldir/telldir_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/telldir_4:	telldir/telldir_4.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ telldir/telldir_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/telldir_5:	telldir/telldir_5.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ telldir/telldir_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/print_file_1:	print_file/print_file_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ print_file/print_file_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/print_file_2:	print_file/print_file_2.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ print_file/print_file_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/print_file_3:	print_file/print_file_3.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ print_file/print_file_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/print_file_4:	print_file/print_file_4.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ print_file/print_file_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_print_job_1:	open_print_job/open_print_job_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ open_print_job/open_print_job_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/open_print_job_2:	open_print_job/open_print_job_2.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ open_print_job/open_print_job_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_1:	list_print_jobs/list_print_jobs_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_2:	list_print_jobs/list_print_jobs_2.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_3:	list_print_jobs/list_print_jobs_3.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_4:	list_print_jobs/list_print_jobs_4.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_5:	list_print_jobs/list_print_jobs_5.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_5.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_6:	list_print_jobs/list_print_jobs_6.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_6.o $(INCPATH) $(LIBS)

$(BIN_DIR)/list_print_jobs_7:	list_print_jobs/list_print_jobs_7.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ list_print_jobs/list_print_jobs_7.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_print_job_1:	unlink_print_job/unlink_print_job_1.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ unlink_print_job/unlink_print_job_1.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_print_job_2:	unlink_print_job/unlink_print_job_2.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ unlink_print_job/unlink_print_job_2.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_print_job_3:	unlink_print_job/unlink_print_job_3.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ unlink_print_job/unlink_print_job_3.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_print_job_4:	unlink_print_job/unlink_print_job_4.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ unlink_print_job/unlink_print_job_4.o $(INCPATH) $(LIBS)

$(BIN_DIR)/unlink_print_job_5:	unlink_print_job/unlink_print_job_5.o
	@echo Linking $@
	@dir=$(BIN_DIR) $(MAKEDIR)@
	@$(CC) $(LFLAGS) -o $@ unlink_print_job/unlink_print_job_5.o $(INCPATH) $(LIBS)


clean:
	@for i in $(SUB_DIRS); do \
		 rm -f $$i/*.o; \
	done

	@rm -f $(GROUPS)
	@rm -rf $(BIN_DIR)
	@echo "Done"

