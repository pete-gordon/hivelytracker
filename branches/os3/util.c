#include <stdio.h>

#include <system_includes.h>

struct Node *allocnode(int32 size)
{
#ifndef __SDL_WRAPPER__
  return AllocVec(size, MEMF_ANY);
#else
  struct Node *ptr = malloc(size);
  if (ptr) memset(ptr, 0, sizeof(struct Node));
  return ptr;
#endif
}
