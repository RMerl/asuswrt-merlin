#ifndef __SMBREADLINE_H__
#define __SMBREADLINE_H__

char *smb_readline(const char *prompt, void (*callback)(void), 
		   char **(completion_fn)(const char *text, int start, int end));
const char *smb_readline_get_line_buffer(void);
void smb_readline_ca_char(char c);

#endif /* __SMBREADLINE_H__ */
