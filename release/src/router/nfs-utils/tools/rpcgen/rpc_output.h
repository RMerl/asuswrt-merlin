/*
 * rpc_output.h
 *
 * Declarations for output functions
 *
 */

#ifndef RPCGEN_NEW_OUTPUT_H
#define RPCGEN_NEW_OUTPUT_H

void	write_msg_out(void);
int	nullproc(proc_list *);
void	printarglist(proc_list *, char *, char *);
void	pdeclaration(char *, declaration *, int, char *);

#endif /* RPCGEN_NEW_OUTPUT_H */
