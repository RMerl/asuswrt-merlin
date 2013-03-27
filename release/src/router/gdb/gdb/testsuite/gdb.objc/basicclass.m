#include <objc/Object.h>

@interface BasicClass: Object
{
  id object;
}
+ newWithArg: arg;
- doIt;
- takeArg: arg;
- printHi;
- (int) printNumber: (int)number;
- (const char *) myDescription;
@end

@interface BasicClass (Private)
- hiddenMethod;
@end

@implementation BasicClass
+ newWithArg: arg
{
  id obj = [self new];
  [obj takeArg: arg];
  return obj;
}

- doIt
{
  return self;
}

- takeArg: arg
{
  object = arg;
  [self hiddenMethod];
  return self;
}

- printHi
{
  printf("Hi\n");
  return self;
}

- (int) printNumber: (int)number
{
  printf("%d\n", number);
  return number+1;
}

- (const char *) myDescription
{
  return "BasicClass gdb test object";
}

@end

@implementation BasicClass (Private)
- hiddenMethod
{
  return self;
}
@end

int main (int argc, const char *argv[])
{
  id obj;
  obj = [BasicClass new];
  [obj takeArg: obj];
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
