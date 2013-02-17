

LIBS	= 

NFSOBJS = ../nfs/exports.o ../nfs/rmtab.o ../nfs/xio.o ../nfs/closeall.o \
	  ../nfs/rpcmisc.o ../nfs/rpcdispatch.o ../nfs/xlog.o ../nfs/xcommon.o ../nfs/wildmat.o \
	  ../nfs/nfssvc.o ../nfs/nfsclient.o ../nfs/nfsexport.o ../nfs/getfh.o ../nfs/nfsctl.o \
	  ../nfs/rpc_socket.o ../nfs/svc_socket.o ../nfs/cacheio.o ../nfs/getport.o
EXPOBJS = ../export/mount_clnt.o ../export/mount_xdr.o ../export/client.o ../export/export.o ../export/hostname.o \
	  ../export/nfsctl.o ../export/rmtab.o ../export/xtab.o
MSCOBJS = ../misc/from_local.o ../misc/mountpoint.o

all install::	$(LIBS) libnfs.so
	@:

clean distclean::
	rm -f $(LIBS)

lib%.a:
	ln -sf ../$*/$@ .

libnfs.so: $(NFSOBJS) $(EXPOBJS) $(MSCOBJS)
	$(CC) -shared -nostdlib -fPIC -o $@ $^
