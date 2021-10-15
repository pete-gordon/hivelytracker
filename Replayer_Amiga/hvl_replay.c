#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <exec/types.h>
#include <exec/exectags.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "hvl_replay.h"
#include "hvl_tables.h"

#define HVL_MALLOC(size) IExec->AllocVec( size, MEMF_ANY );
#define HVL_FREE(ptr)    IExec->FreeVec( ptr )
#define memcpy(d,s,l) Exec->CopyMem((void *)(s), (void *)(d), l)

#include "../common/hvl_replay.c"

struct hvl_tune *hvl_LoadTune( CONST TEXT *name, uint32 freq, uint32 defstereo )
{
  struct hvl_tune *ht;
  uint8  *buf;
  uint32  buflen;
  BPTR    lock, fh;
  struct  FileInfoBlock *fib;
  struct  hvl_plsentry *ple;

  fib = IDOS->AllocDosObjectTags( DOS_FIB, TAG_DONE );
  if( !fib )
  {
    printf( "Out of memory\n" );
    return NULL;
  }

  lock = IDOS->Lock( name, ACCESS_READ );
  if( !lock )
  {
    IDOS->FreeDosObject( DOS_FIB, fib );
    printf( "Unable to find '%s'\n", name );
    return NULL;
  }

  IDOS->Examine( lock, fib );
  if( !FIB_IS_FILE( fib ) )
  {
    IDOS->UnLock( lock );
    IDOS->FreeDosObject( DOS_FIB, fib );
    printf( "Bad file!\n" );
    return NULL;
  }

  buflen = fib->fib_Size;
  buf = IExec->AllocVec( buflen, MEMF_ANY );
  if( !buf )
  {
    IDOS->UnLock( lock );
    IDOS->FreeDosObject( DOS_FIB, fib );
    printf( "Out of memory!\n" );
    return NULL;
  }

  fh = IDOS->OpenFromLock( lock );
  if( !fh )
  {
    IExec->FreeVec( buf );
    IDOS->UnLock( lock );
    IDOS->FreeDosObject( DOS_FIB, fib );
    printf( "Can't open file\n" );
    return NULL;
  }

  if( IDOS->Read( fh, buf, buflen ) != buflen )
  {
    IExec->FreeVec( buf );
    IDOS->Close( fh );
    IDOS->FreeDosObject( DOS_FIB, fib );
    printf( "Unable to read from file!\n" );
    return NULL;
  }
  IDOS->Close( fh );
  IDOS->FreeDosObject( DOS_FIB, fib );

  ht = hvl_load( buf, buflen, defstereo, freq );
  IExec->FreeVec( buf );
  return ht;
}
