#include <objc/Object.h>

@interface Decode: Object
{
}
- multipleDef;
- (const char *) myDescription;
@end

@implementation Decode

- multipleDef
{
  printf("method multipleDef\n");
  return self;
}

- (const char *) myDescription
{
  return "Decode gdb test object";
}

@end

int
multipleDef()
{
  printf("function multipleDef\n");
  return 0;
}

int main (int argc, const char *argv[])
{
  id obj;
  obj = [Decode new];
  multipleDef();
  [obj multipleDef];
  return 0;
}

const char *_NSPrintForDebugger(id object)
{
  /* This is not really what _NSPrintForDebugger should do, but it
     is a simple test if gdb can call this function */
  if (object && [object respondsTo: @selector(myDescription)])
    return [object myDescription];

  return NULL;
}
