#include "testutils.h"
#include "buffer.h"

void
test_main(void)
{
  struct nettle_buffer buffer;
  uint8_t s[5];
  
  nettle_buffer_init(&buffer);
  ASSERT(nettle_buffer_write(&buffer, LDATA("foo")));
  
  ASSERT(NETTLE_BUFFER_PUTC(&buffer, 'x'));

  ASSERT(buffer.size == 4);
  ASSERT(buffer.alloc >= 4);
  ASSERT(MEMEQ(4, buffer.contents, "foox"));

  nettle_buffer_clear(&buffer);
  
  nettle_buffer_init_size(&buffer, sizeof(s), s);
  ASSERT(buffer.alloc == sizeof(s));
  ASSERT(nettle_buffer_write(&buffer, LDATA("foo")));
  ASSERT(buffer.size == 3);

  ASSERT(!nettle_buffer_write(&buffer, LDATA("bar")));
}
