/*
** HivelyTracker Windows Commandline Replayer
**
** Just quickly bodged together as an example.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stddef.h>

#include <windows.h>

#include "hvl_replay.h"

     
HWAVEOUT     hWaveOut = INVALID_HANDLE_VALUE; /* Device handle */
WAVEFORMATEX wfx;
LPSTR        audblock;
char audiobuffer[3][((44100*2*2)/50)];

struct hvl_tune *ht = NULL;

BOOL init( char *name )
{
  MMRESULT result;

  wfx.nSamplesPerSec  = 44100;
  wfx.wBitsPerSample  = 16;
  wfx.nChannels       = 2;

  wfx.cbSize          = 0;
  wfx.wFormatTag      = WAVE_FORMAT_PCM;
  wfx.nBlockAlign     = (wfx.wBitsPerSample >> 3) * wfx.nChannels;
  wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

  hvl_InitReplayer();

  ht = hvl_LoadTune( name, 44100, 0 );
  if( !ht ) return FALSE;

  if( waveOutOpen( &hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL ) != MMSYSERR_NOERROR )
  {
    printf( "Unable to open waveout\n" );
    return FALSE;
  }

  return TRUE;  
}

void shut( void )
{
  if( ht ) hvl_FreeTune( ht );
  if( hWaveOut != INVALID_HANDLE_VALUE ) waveOutClose( hWaveOut );
}

int main(int argc, char *argv[])
{
  WAVEHDR header[3];
  int nextbuf = 0;
  sigset_t base_mask, waiting_mask;

  printf( "Hively Tracker command line player 1.6\n" );  

  if( argc < 2 )
  {
    printf( "Usage: play_hvl <tune>\n" );
    return 0;
  }

  if( init( argv[1] ) )
  {
    int i;

    printf( "Tune: '%s'\n", ht->ht_Name );
    printf( "Instruments:\n" );
    for( i=1; i<=ht->ht_InstrumentNr; i++ )
      printf( "%02ld: %s\n", i, ht->ht_Instruments[i].ins_Name );

    memset( &header[0], 0, sizeof( WAVEHDR ) );
    memset( &header[1], 0, sizeof( WAVEHDR ) );
    memset( &header[2], 0, sizeof( WAVEHDR ) );

    header[0].dwBufferLength = ((44100*2*2)/50);
    header[0].lpData         = (LPSTR)audiobuffer[0];

    header[1].dwBufferLength = ((44100*2*2)/50);
    header[1].lpData         = (LPSTR)audiobuffer[1];

    header[2].dwBufferLength = ((44100*2*2)/50);
    header[2].lpData         = (LPSTR)audiobuffer[2];

    hvl_DecodeFrame( ht, audiobuffer[nextbuf], audiobuffer[nextbuf]+2, 4 );
    waveOutPrepareHeader( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
    waveOutWrite( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
    nextbuf = (nextbuf+1)%3;
    hvl_DecodeFrame( ht, audiobuffer[nextbuf], audiobuffer[nextbuf]+2, 4 );
    waveOutPrepareHeader( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
    waveOutWrite( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
    nextbuf = (nextbuf+1)%3;

    for(;;)
    {
      hvl_DecodeFrame( ht, audiobuffer[nextbuf], audiobuffer[nextbuf]+2, 4 );
      waveOutPrepareHeader( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
      waveOutWrite( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) );
      nextbuf = (nextbuf+1)%3;
     
      // Don't do this in your own player or plugin :-)
      while( waveOutUnprepareHeader( hWaveOut, &header[nextbuf], sizeof( WAVEHDR ) ) == WAVERR_STILLPLAYING ) ;
    }

  }
  shut();

  return 0;
}
