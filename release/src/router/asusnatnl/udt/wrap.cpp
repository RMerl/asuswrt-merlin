#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <iostream>
#include "udt.h"
#include "ccc.h"

#include "wrap.h"
#include "UDPBlast.h"

using namespace std;

void udt_startup(int inst_id)
{
    UDT::startup(inst_id);
}

void udt_cleanup()
{
    // use this function to release the UDT library
    UDT::cleanup();
}

int udt_socket()
{
    struct addrinfo hints, *local;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_socktype = SOCK_DGRAM;

    if (0 != getaddrinfo(NULL, "59000", &hints, &local)) {
	cout << "incorrect network address.\n" << endl;
	return 0;
    }

    UDTSOCKET fd = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    //UDT::setsockopt(fd, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
    //UDT::setsockopt(fd, 0, UDT_MSS, new int(9000), sizeof(int));
   UDT::setsockopt(fd, 0, UDT_SNDBUF, new int(1460*64), sizeof(int));
   UDT::setsockopt(fd, 0, UDT_RCVBUF, new int(1460*64), sizeof(int));

    // Windows UDP issue
    // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
    #ifdef WIN32
	//UDT::setsockopt(fd, 0, UDT_MSS, new int(1052), sizeof(int));
	//UDT::setsockopt(fd, 0, UDT_MSS, new int(1500), sizeof(int));
	UDT::setsockopt(fd, 0, UDT_MSS, new int(1456), sizeof(int));
    #endif

	//UDT::setsockopt(fd, 0, UDT_LINGER, new int(1), sizeof(int));

	UDT::setsockopt(fd, 1, UDT_RCVSYN, new bool(true), sizeof(bool));
	UDT::setsockopt(fd, 1, UDT_SNDSYN, new bool(true), sizeof(bool));

    // for rendezvous connection, enable the code below
    UDT::setsockopt(fd, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
    //if (UDT::ERROR == UDT::bind(fd, local->ai_addr, local->ai_addrlen)) {
	//cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
	//return 0;
    //}

    freeaddrinfo(local);

    return fd;
}

int udt_bind(void *call)
{
   return UDT::bind_stream(call);
}

int udt_connect(void *call)
{
   return UDT::connect_stream(call);
}

int udt_close(void *call) 
{
   return UDT::close_stream(call);
}

int udt_close(int u)
{
   return UDT::close(u);
}

int udt_recv(int u, char* buf, int len, int flags)
{
   return UDT::recv(u, buf, len, flags);	
}

int udt_send(int u, const char* buf, int len, int flags) 
{
   return UDT::send(u, buf, len, flags);	
}

#if 0
int udt_select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* timeout)
{
	return UDT::select(nfds, readfds, writefds, exceptfds, timeout);

}
#endif

int appclient(int argc, char* argv[])
{
   if ((3 != argc) || (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_ip server_port" << endl;
      return 0;
   }

   // use this function to initialize the UDT library
   UDT::startup(0);

   struct addrinfo hints, *local, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   //hints.ai_socktype = SOCK_DGRAM;

   if (0 != getaddrinfo(NULL, "9000", &hints, &local))
   {
      cout << "incorrect network address.\n" << endl;
      return 0;
   }

   UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

   // UDT Options
   //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
   //UDT::setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
   //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
   //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));

   // Windows UDP issue
   // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold
   #ifdef WIN32
      UDT::setsockopt(client, 0, UDT_MSS, new int(1052), sizeof(int));
   #endif

   // for rendezvous connection, enable the code below
   /*
   UDT::setsockopt(client, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
   if (UDT::ERROR == UDT::bind(client, local->ai_addr, local->ai_addrlen))
   {
      cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }
   */

   freeaddrinfo(local);

   if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
   {
      cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
      return 0;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return 0;
   }

   freeaddrinfo(peer);

#if 0
   // using CC method
   CUDPBlast* cchandle = NULL;
   int temp;
   UDT::getsockopt(client, 0, UDT_CC, &cchandle, &temp);
   if (NULL != cchandle)
      cchandle->setRate(500);
#endif

   int size = 100000;
   char* data = new char[size];

   #ifndef WIN32
      //pthread_create(new pthread_t, NULL, monitor, &client);
   #else
      //CreateThread(NULL, 0, monitor, &client, 0, NULL);
   #endif

   for (int i = 0; i < 1000000; i ++)
   {
      int ssize = 0;
      int ss;
      while (ssize < size)
      {
         if (UDT::ERROR == (ss = UDT::send(client, data + ssize, size - ssize, 0)))
         {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
         }

         ssize += ss;
      }

      if (ssize < size)
         break;
   }

   UDT::close(client);

   delete [] data;

   // use this function to release the UDT library
   UDT::cleanup();

   return 1;
}
