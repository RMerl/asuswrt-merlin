#include <objc/Object.h>

@interface NonDebug: Object
{
}
@end
@interface NonDebug2: Object
{
}
@end

@implementation NonDebug

- someMethod
{
  printf("method someMethod\n");
  return self;
}

@end
@implementation NonDebug2

- someMethod
{
  printf("method2 someMethod\n");
  return self;
}

@end


int main (int argc, const char *argv[])
{
  id obj;
  obj = [NonDebug new];
  [obj someMethod];
  return 0;
}
