#ifndef ATALK_SERVER_IPC_H
#define ATALK_SERVER_IPC_H

#include <atalk/server_child.h>
#include <atalk/globals.h>

/* Remember to add IPC commands to server_ipc.c:ipc_cmd_str[] */
#define IPC_DISCOLDSESSION   0
#define IPC_GETSESSION       1
#define IPC_STATE            2  /* pass AFP session state */
#define IPC_VOLUMES          3  /* pass list of open volumes */

extern int ipc_server_read(server_child_t *children, int fd);
extern int ipc_child_write(int fd, uint16_t command, int len, void *token);
extern int ipc_child_state(AFPObj *obj, uint16_t state);

#endif /* IPC_GETSESSION_LOGIN */
