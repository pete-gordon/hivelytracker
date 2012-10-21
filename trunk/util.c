#include <stdio.h>

#include <system_includes.h>

#ifndef __SDL_WRAPPER__
/*
** A small routine to open a library and get its default
** interface, and report errors if anything fails.
*/
BOOL getLibIFace( struct Library **libbase, TEXT *libname, uint32 version, void *ifaceptr )
{
  struct Interface **ifptr = (struct Interface **)ifaceptr;

  *libbase = IExec->OpenLibrary( libname, version );
  if( *libbase == NULL )
  {
    printf( "Unable to open '%s' version %ld\n", libname, version );
    return FALSE;
  }

  *ifptr = IExec->GetInterface( *libbase, "main", 1, NULL );
  if( *ifptr == NULL )
  {
    printf( "Unable to get the main interface for '%s'\n", libname );
    return FALSE;
  }

  return TRUE;
}
#endif

struct Node *allocnode(int32 size)
{
#ifndef __SDL_WRAPPER__
  return IExec->AllocSysObjectTags(ASOT_NODE, ASONODE_Size, size, TAG_DONE);
#else
  struct Node *ptr = malloc(size);
  if (ptr) memset(ptr, 0, sizeof(struct Node));
  return ptr;
#endif
}
