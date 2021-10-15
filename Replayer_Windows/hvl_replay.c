#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hvl_replay.h"
#include "hvl_tables.h"

#define HVL_MALLOC(size) malloc( size )
#define HVL_FREE(ptr)    free( ptr )
#include "../common/hvl_replay.c"

struct hvl_tune *hvl_LoadTune( CONST TEXT *name, uint32 freq, uint32 defstereo )
{
  struct hvl_tune *ht = NULL;
  uint8  *buf;
  uint32  buflen;
  FILE *fh;

  fh = fopen( name, "rb" );
  if( !fh )
  {
    printf( "Can't open file\n" );
    return NULL;
  }

  fseek( fh, 0, SEEK_END );
  buflen = ftell( fh );
  fseek( fh, 0, SEEK_SET );

  buf = malloc( buflen );
  if( !buf )
  {
    fclose( fh );
    printf( "Out of memory!\n" );
    return NULL;
  }

  if( fread( buf, 1, buflen, fh ) != buflen )
  {
    fclose( fh );
    free( buf );
    printf( "Unable to read from file!\n" );
    return NULL;
  }
  fclose( fh );

  ht = hvl_load( buf, buflen, freq, defstereo );
  free( buf );
  return ht;
}
