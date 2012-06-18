#include <libsmbclient.h>

void create_and_destroy_context (void)
{
  SMBCCTX *ctx;
  ctx = smbc_new_context ();
  smbc_init_context (ctx);

  smbc_free_context (ctx, 1);
}

int main (int argc, char **argv)
{
  create_and_destroy_context ();
  create_and_destroy_context ();
  return 0;
}
