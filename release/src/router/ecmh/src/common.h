/*****************************************************
 ecmh - Easy Cast du Multi Hub - Common Functions
******************************************************
 $Author: fuzzel $
 $Id: common.h,v 1.2 2005/02/09 17:58:06 fuzzel Exp $
 $Date: 2005/02/09 17:58:06 $
*****************************************************/

void dolog(int level, const char *fmt, ...);
int huprunning(void);
void savepid(void);
void cleanpid(int i);
