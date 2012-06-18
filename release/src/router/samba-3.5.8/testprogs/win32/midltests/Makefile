INCLUDES=-I
CFLAGS=$(INCLUDES) -Zi -D_WIN32_WINNT=0x502
LIBS=rpcrt4.lib

all: midltests.exe

clean:
	del *~ *.obj *.exe midltests.h midltests_s.c midltests_c.c

#MIDL_ARGS=/target NT50
MIDL_ARGS=/Os /prefix client cli_ /prefix server srv_ /prefix switch swi_
midltests.h midltests_s.c midltests_c.c: midltests.idl midltests.acf
	midl $(MIDL_ARGS) /acf midltests.acf midltests.idl

MIDLTESTS_OBJ = midltests.obj midltests_s_m.obj midltests_c_m.obj midltests_marshall.obj utils.obj
midltests.exe: $(MIDLTESTS_OBJ)
	$(CC) -o midltests.exe $(MIDLTESTS_OBJ) $(LIBS)

midltests.obj: midltests.h midltests.idl
midltests_c_m.c: midltests_c.c
midltests_s_m.c: midltests_s.c

midltests.obj: midltests.h midltests.idl midltests.c
midltests_s_m.obj: midltests_marshall.h midltests_s.c midltests_s_m.c
midltests_c_m.obj: midltests_marshall.h midltests_c.c midltests_c_m.c
midltests_marshall.obj: midltests.h midltests_marshall.c
utils.obj: midltests.h utils.c
