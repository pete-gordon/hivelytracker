/*
** HVL2WAV
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "replay.h"

uint32 max_frames = 10 * 60 * 50;
uint32 framelen;
uint32 mix_freq = 44100;
uint32 subsong = 0, smplen = 0;
uint32 stereosep = 0;
BOOL autogain = FALSE;
BOOL use_songend = TRUE;
char *filename = NULL;
char *outname = NULL;
char wavname[1020];
char tmpname[1024];
int8 *mixbuf = NULL;
struct hvl_tune *ht = NULL;
FILE *of = NULL;
FILE *tf = NULL;

void write32( FILE *f, uint32 n )
{
#ifdef _BIG_ENDIAN_
  fprintf( f, "%c%c%c%c", (int)(n&0xff), (int)((n>>8)&0xff), (int)((n>>16)&0xff), (int)((n>>24)&0xff) );
#else
  fwrite( &n, 4, 1, f );
#endif
}

void write16( FILE *f, uint16 n )
{
#ifdef _BIG_ENDIAN_
  fprintf( f, "%c%c", n&0xff, (n>>8)&0xff );
#else
  fwrite( &n, 2, 1, f );
#endif
}

int main( int argc, char *argv[] )
{
  int i, j;
  int32 l;
  uint32 frm;

  for( i=1; i<argc; i++ )
  {
    if( argv[i][0] == '-' )
    {
      switch( argv[i][1] )
      {
        case 't':
          max_frames = atoi( &argv[i][2] ) * 50;
          j = 2;
          while( ( argv[i][j] >= '0' ) && ( argv[i][j] <= '9' ) ) j++;
          if( argv[i][j] == ':' )
          {
            max_frames *= 60;
            max_frames += (atoi( &argv[i][j+1] )*50);
          }

          if( max_frames == 0 )
          {
            printf( "Invalid timeout\n" );
            return 0;
          }

          use_songend = FALSE;
          break;
      
        case 'f':
          mix_freq = atoi( &argv[i][2] );
          if( ( mix_freq < 8000 ) || ( mix_freq > 48000 ) )
          {
            printf( "Frequency should be between 8000 and 48000\n" );
            return 0;
          }
          break;
        
        case 's':
          subsong = atoi( &argv[i][2] );
          break;
        
        case 'a':
          autogain = TRUE;
          break;
        
        case 'o':
          outname = &argv[i][2];
          if( outname[0] == 0 )
          {
            printf( "Invalid output name\n" );
            return 0;
          }
          break;
        
        case 'x':
          stereosep = atoi( &argv[i][2] );
          if( stereosep > 4 ) stereosep = 4;
          break;
      }
    } else {
      if( !filename ) filename = argv[i];
    }
  }

  if( !filename )
  {
    printf( "HVL2WAV 1.2, replayer v1.8\n" );
    printf( "By Xeron/IRIS (pete@petergordon.org.uk)\n" );
    printf( "\n" );
    printf( "Usage: hvl2wav [options] <filename>\n" );
    printf( "\n" );
    printf( "Options:\n" );
    printf( "  -ofile Output to 'file'\n" );
    printf( "  -tm:ss Timeout after m minutes and ss seconds\n" );
    printf( "         (overrides songend detection, defaults to 10:00\n" );
    printf( "  -fn    Set frequency, where n is 8000 to 48000\n" );
    printf( "  -sn    Use subsong n\n" );
    printf( "  -a     Calculate optimal gain before converting\n" );
    printf( "  -xn    Set stereo seperation (ahx only, no effect on HVL files)\n" );
    printf( "            0 = 0%% (Mono)\n" );
    printf( "            1 = 25%%\n" );
    printf( "            2 = 50%%\n" );
    printf( "            3 = 75%%\n" );
    printf( "            4 = 100%% (Paula)\n" );
    return 0;
  }

  framelen = (mix_freq*2*2)/50;
  mixbuf = malloc( framelen );
  if( !mixbuf )
  {
    printf( "Out of memory :-(\n" );
    return 0;
  }

  hvl_InitReplayer();

  ht = hvl_LoadTune( filename, mix_freq, stereosep );
  if( !ht )
  {
    free( mixbuf );
    printf( "Unable to open '%s'\n", filename );
    return 0;
  }

  if( !hvl_InitSubsong( ht, subsong ) )
  {
    hvl_FreeTune( ht );
    free( mixbuf );
    printf( "Unable to initialise subsong %ld\n", subsong );
    return 0;
  }

  if( autogain )
  {
    printf( "Calculating optimal gain...\n" );
    l = hvl_FindLoudest( ht, max_frames, use_songend );
    if( l > 0 )
      l = 3276700/l;
    else
      l = 100;
    
    if( l > 200 ) l = 200;
    ht->ht_mixgain = (l*256)/100;

    hvl_InitSubsong( ht, subsong );
    ht->ht_SongEndReached = 0;
  }

  printf( "Converting tune...\n" );

  if( outname )
  {
    strncpy( wavname, outname, 1020 );
  } else {
    if( ( strlen( filename ) > 4 ) &&
        ( ( strncasecmp( filename, "hvl.", 4 ) == 0 ) ||
          ( strncasecmp( filename, "ahx.", 4 ) == 0 ) ) )
      strncpy( wavname, &filename[4], 1020 );
    else
      strncpy( wavname, filename, 1020 );

    if( ( strlen( wavname ) > 4 ) &&
        ( ( strcasecmp( &wavname[strlen(wavname)-4], ".hvl" ) == 0 ) ||
          ( strcasecmp( &wavname[strlen(wavname)-4], ".ahx" ) == 0 ) ) )
      wavname[strlen(wavname)-4] = 0;
    strcat( wavname, ".wav" );
  }

  strcpy( tmpname, wavname );
  strcat( tmpname, ".tmp" );

  tf = fopen( tmpname, "wb" );
  if( !tf )
  {
    hvl_FreeTune( ht );
    free( mixbuf );
    printf( "Unable to open '%s' for output\n", tmpname );
    return 0;
  }

  frm = 0;
  smplen = 0;
  while( frm < max_frames )
  {
    if( ( use_songend ) && ( ht->ht_SongEndReached ) )
      break;

    hvl_DecodeFrame( ht, mixbuf, &mixbuf[2], 4 );

#ifdef _BIG_ENDIAN_
    // The replayer generates audio samples in the same
    // endian mode as the CPU. So on big endian systems
    // we have to swap it all to stupid endian for the
    // WAV format.
    for( i=0; i<framelen; i+=2 )
    {
      int8 k;
      k = mixbuf[i]; mixbuf[i] = mixbuf[i+1]; mixbuf[i+1] = k;
    }
#endif

    fwrite( mixbuf, framelen, 1, tf );
    smplen += framelen;
    frm++;
  }

  fclose( tf );

  of = fopen( wavname, "wb" );
  if( !of )
  {
    hvl_FreeTune( ht );
    free( mixbuf );
    unlink( tmpname );
    printf( "Unable to open '%s' for output\n", wavname );
    return 0;
  }

  tf = fopen( tmpname, "rb" );
  if( !tf )
  {
    fclose( of );
    unlink( wavname );
    unlink( tmpname );
    hvl_FreeTune( ht );
    free( mixbuf );
    printf( "Unable to open '%s' for input\n", tmpname );
    return 0;
  }

  fprintf( of, "RIFF" );
  write32( of, smplen+36 );
  fprintf( of, "WAVE" );
  fprintf( of, "fmt " );
  write32( of, 16 );
  write16( of, 1 );
  write16( of, 2 );
  write32( of, mix_freq );
  write32( of, mix_freq*4 );
  write16( of, 4 );
  write16( of, 16 );
  fprintf( of, "data" );
  write32( of, smplen );

  for( i=0; i<smplen; i+=framelen )
  {
    fread( mixbuf, framelen, 1, tf );
    fwrite( mixbuf, framelen, 1, of );
  }

  fclose( of );
  fclose( tf );
  unlink( tmpname );
  hvl_FreeTune( ht );
  free( mixbuf );
  printf( "ALL DONE!\n" );
  return 0;
}
