INCLUDES=-I 
CFLAGS=$(INCLUDES) -Zi -nologo
LIBS=rpcrt4.lib

all: client server

clean:
	del *~ *.obj client server rpcecho_c.c rpcecho_s.c rpcecho.h

rpcecho.h rpcecho_s.c rpcecho_c.c: rpcecho.idl rpcecho.acf
	midl rpcecho.idl

client: client.obj rpcecho_c.obj utils.obj
	$(CC) $(CFLAGS) -o client client.obj rpcecho_c.obj utils.obj $(LIBS)

server: server.obj rpcecho_s.obj utils.obj
	$(CC) $(CFLAGS) -o server server.obj rpcecho_s.obj utils.obj $(LIBS)

client.obj: rpcecho.h client.c
server.obj: rpcecho.h server.c
rpcecho_c.obj: rpcecho.h rpcecho_c.c
rpcecho_s.obj: rpcecho.h rpcecho_s.c
utils.obj: rpcecho.h utils.c
