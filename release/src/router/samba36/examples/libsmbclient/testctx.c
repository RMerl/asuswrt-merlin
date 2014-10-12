#include <libsmbclient.h>
#include <stdlib.h>
#include <stdio.h>

void create_and_destroy_context (void)
{
  int i;
  SMBCCTX *ctx;
  ctx = smbc_new_context ();
  /* Both should do the same thing */
  smbc_setOptionDebugToStderr(ctx, 1);
  smbc_option_set(ctx, "debug_to_stderr", 1);
  smbc_setDebug(ctx, 1);
  i = smbc_getDebug(ctx);
  if (i != 1) { 
	  printf("smbc_getDebug() did not return debug level set\n");
	  exit(1);
  }
  if (!smbc_getOptionDebugToStderr(ctx)) {
	  printf("smbc_setOptionDebugToStderr() did not stick\n");
	  exit(1);
  }
  smbc_init_context (ctx);
  smbc_free_context (ctx, 1);
}

int main (int argc, char **argv)
{
  create_and_destroy_context ();
  create_and_destroy_context ();
  return 0;
}
