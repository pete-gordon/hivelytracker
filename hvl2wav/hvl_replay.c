#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "types.h"
#include <string.h>

#include "hvl_replay.h"
#include "hvl_tables.h"

#define HVL_MALLOC(size) malloc( size )
#define HVL_FREE(ptr)    free( ptr )
#define CONST const
#include "../common/hvl_replay.c"

struct hvl_tune *hvl_LoadTune( CONST TEXT *name, uint32 freq, uint32 defstereo )
{
  struct hvl_tune *ht;
  uint8  *buf;
  uint32 buflen;
  FILE *f;

  f = fopen( name, "rb" );
  if( !f )
  {
    printf( "Cannot open file\n" );
    return NULL;
  }
  fseek( f, 0, SEEK_END );
  buflen = ftell( f );
  rewind( f );
  buf = malloc( buflen );
  if( !buf )
  {
    fclose( f );
    printf( "Out of memory!\n" );
    return NULL;
  }

  if( fread( buf, 1, buflen, f ) != buflen )
  {
    free( buf );
    fclose( f );
    printf( "Unable to read from file!\n" );
    return NULL;
  }
  fclose( f );

  ht = hvl_load( buf, buflen, defstereo, freq );
  free (ht);
  return ht;
}

static int32 hvl_mix_findloudest( struct hvl_tune *ht, uint32 samples )
{
  const int8   *src[MAX_CHANNELS];
  const int8   *rsrc[MAX_CHANNELS];
        uint32  delta[MAX_CHANNELS];
        uint32  rdelta[MAX_CHANNELS];
        int32   vol[MAX_CHANNELS];
        uint32  pos[MAX_CHANNELS];
        uint32  rpos[MAX_CHANNELS];
        uint32  cnt;
        int32   panl[MAX_CHANNELS];
        int32   panr[MAX_CHANNELS];
        int32   a=0, b=0, j;
        uint32  loud;
        uint32  i, chans, loops;

  loud = 0;

  chans = ht->ht_Channels;
  for( i=0; i<chans; i++ )
  {
    delta[i] = ht->ht_Voices[i].vc_Delta;
    vol[i]   = ht->ht_Voices[i].vc_VoiceVolume;
    pos[i]   = ht->ht_Voices[i].vc_SamplePos;
    src[i]   = ht->ht_Voices[i].vc_MixSource;
    panl[i]  = ht->ht_Voices[i].vc_PanMultLeft;
    panr[i]  = ht->ht_Voices[i].vc_PanMultRight;
    /* Ring Modulation */
    rdelta[i]= ht->ht_Voices[i].vc_RingDelta;
    rpos[i]  = ht->ht_Voices[i].vc_RingSamplePos;
    rsrc[i]  = ht->ht_Voices[i].vc_RingMixSource;
  }

  do
  {
    loops = samples;
    for( i=0; i<chans; i++ )
    {
      if( pos[i] >= (0x280 << 16)) pos[i] -= 0x280<<16;
      cnt = ((0x280<<16) - pos[i] - 1) / delta[i] + 1;
      if( cnt < loops ) loops = cnt;

      if( rsrc[i] )
      {
        if( rpos[i] >= (0x280<<16)) rpos[i] -= 0x280<<16;
        cnt = ((0x280<<16) - rpos[i] - 1) / rdelta[i] + 1;
        if( cnt < loops ) loops = cnt;
      }
    }

    samples -= loops;

    // Inner loop
    do
    {
      a=0;
      b=0;
      for( i=0; i<chans; i++ )
      {
        if( rsrc[i] )
        {
          /* Ring Modulation */
          j = ((src[i][pos[i]>>16]*rsrc[i][rpos[i]>>16])>>7)*vol[i];
          rpos[i] += rdelta[i];
        } else {
          j = src[i][pos[i]>>16]*vol[i];
        }
        a += (j * panl[i]) >> 7;
        b += (j * panr[i]) >> 7;
        pos[i] += delta[i];
      }

//      a = (a*ht->ht_mixgain)>>8;
//      b = (b*ht->ht_mixgain)>>8;
      a = abs( a );
      b = abs( b );

      if( a > loud ) loud = a;
      if( b > loud ) loud = b;

      loops--;
    } while( loops > 0 );
  } while( samples > 0 );

  for( i=0; i<chans; i++ )
  {
    ht->ht_Voices[i].vc_SamplePos = pos[i];
    ht->ht_Voices[i].vc_RingSamplePos = rpos[i];
  }

  return loud;
}

int32 hvl_FindLoudest( struct hvl_tune *ht, int32 maxframes, BOOL usesongend )
{
  uint32 rsamp, rloop;
  uint32 samples, loops, loud, n;
  int32 frm;

  rsamp = ht->ht_Frequency/50/ht->ht_SpeedMultiplier;
  rloop = ht->ht_SpeedMultiplier;

  loud = 0;

  ht->ht_SongEndReached = 0;

  frm = 0;
  while( frm < maxframes )
  {
    if( ( usesongend ) && ( ht->ht_SongEndReached ) )
      break;

    samples = rsamp;
    loops   = rloop;

    do
    {
      hvl_play_irq( ht );
      n = hvl_mix_findloudest( ht, samples );
      if( n > loud ) loud = n;
      loops--;
    } while( loops );

    frm++;
  }

  return loud;
}
