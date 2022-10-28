
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>

#include <exec/types.h>
#include <exec/exectags.h>

#include <proto/exec.h>
#include <proto/dos.h>

#define __USE_BASETYPE__ 1

#include <proto/ahi.h>

#include "hvl_replay.h"

uint32          audiobuflen;
int8           *audiobuffer[2]  = { NULL, NULL };

// Libraries and interfaces
struct Library  *AHIBase = NULL;
struct AHIIFace *IAHI = NULL;

// AHI stuff
struct MsgPort    *ahi_mp;
struct AHIRequest *ahi_io[2] = { NULL, NULL };
int32              ahi_dev = -1;
uint32             ahisig = 0;
struct AHIRequest *prev_req = NULL;

BOOL   need_wait[2];
uint32 nextbuf;

#define FREQ 48000

BOOL init( void )
{
  uint32 i;

  audiobuflen = FREQ * sizeof( uint16 ) * 2 / 50;

  audiobuffer[0] = IExec->AllocVec( audiobuflen * 2, MEMF_ANY );
  if( audiobuffer[0] == NULL )
  {
    printf( "Out of memory!\n" );
    return FALSE;
  }

  audiobuffer[1] = &audiobuffer[0][audiobuflen];
  
  for( i=0; i<audiobuflen; i++ )
    audiobuffer[0][i] = 0;  

  ahi_mp = IExec->CreateMsgPort();
  if( !ahi_mp )
  {
    printf( "Unable to create message port!\n" );
    return FALSE;
  }

  ahi_io[0] = (struct AHIRequest *)IExec->CreateIORequest( ahi_mp, sizeof( struct AHIRequest ) );
  if( ahi_io[0] == NULL ) return FALSE;

  ahi_io[0]->ahir_Version = 4;
  
  ahi_dev = IExec->OpenDevice( AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)ahi_io[0], 0 ); 
  if( ahi_dev == -1 ) return FALSE;
  
  AHIBase = (struct Library *)ahi_io[0]->ahir_Std.io_Device;
  IAHI = (struct AHIIFace *)IExec->GetInterface( AHIBase, "main", 1, NULL );
  if( !IAHI ) return FALSE;
  
  ahi_io[1] = IExec->AllocVec( sizeof( struct AHIRequest ), MEMF_ANY );
  if( ahi_io[1] == NULL ) return FALSE;
  
  IExec->CopyMem( ahi_io[0], ahi_io[1], sizeof( struct AHIRequest ) );

  ahisig  = 1L<<ahi_mp->mp_SigBit;
  
  return TRUE;
}

void shut( void )
{
  if( ahi_io[1] )     IExec->FreeVec( ahi_io[1] );
  if( IAHI )          IExec->DropInterface( (struct Interface *)IAHI );
  if( ahi_dev != -1 )
  {
    IExec->CloseDevice( (struct IORequest *)ahi_io[0] );
    IExec->DeleteIORequest( (struct IORequest *)ahi_io[0] );
  }
  if( ahi_mp )        IExec->DeleteMsgPort( ahi_mp );
  if( audiobuffer[0] ) IExec->FreeVec( audiobuffer[0] );
}

void mix_and_play( struct hvl_tune *ht )
{
 
  if( need_wait[nextbuf] )
    IExec->WaitIO( (struct IORequest *)ahi_io[nextbuf] );

  hvl_DecodeFrame( ht, audiobuffer[nextbuf], audiobuffer[nextbuf]+sizeof( int16 ), 4 );

  ahi_io[nextbuf]->ahir_Std.io_Command = CMD_WRITE;
  ahi_io[nextbuf]->ahir_Std.io_Data    = audiobuffer[nextbuf];
  ahi_io[nextbuf]->ahir_Std.io_Length  = audiobuflen;
  ahi_io[nextbuf]->ahir_Std.io_Offset  = 0;
  ahi_io[nextbuf]->ahir_Type           = AHIST_S16S;
  ahi_io[nextbuf]->ahir_Frequency      = FREQ;
  ahi_io[nextbuf]->ahir_Volume         = 0x10000;
  ahi_io[nextbuf]->ahir_Position       = 0x8000;
  ahi_io[nextbuf]->ahir_Link           = prev_req;
  IExec->SendIO( (struct IORequest *)ahi_io[nextbuf] );
            
  prev_req = ahi_io[nextbuf];
  need_wait[nextbuf] = TRUE;

  nextbuf ^= 1;  
  
  if( ( need_wait[nextbuf] == TRUE ) &&
      ( IExec->CheckIO( (struct IORequest *)ahi_io[nextbuf] ) != NULL ) )
  {
    IExec->WaitIO( (struct IORequest *)ahi_io[nextbuf] );
    need_wait[nextbuf] = FALSE;
  }
            
  if( need_wait[nextbuf] == FALSE )
  {
    hvl_DecodeFrame( ht, audiobuffer[nextbuf], audiobuffer[nextbuf]+sizeof( int16 ), 4 );

    ahi_io[nextbuf]->ahir_Std.io_Command = CMD_WRITE;
    ahi_io[nextbuf]->ahir_Std.io_Data    = audiobuffer[nextbuf];
    ahi_io[nextbuf]->ahir_Std.io_Length  = audiobuflen;
    ahi_io[nextbuf]->ahir_Std.io_Offset  = 0;
    ahi_io[nextbuf]->ahir_Type           = AHIST_S16S;
    ahi_io[nextbuf]->ahir_Frequency      = FREQ;
    ahi_io[nextbuf]->ahir_Volume         = 0x10000;
    ahi_io[nextbuf]->ahir_Position       = 0x8000;
    ahi_io[nextbuf]->ahir_Link           = prev_req;
    IExec->SendIO( (struct IORequest *)ahi_io[nextbuf] );

    prev_req = ahi_io[nextbuf];
    need_wait[nextbuf] = TRUE;
    nextbuf ^= 1;  
  }
}

int main( int argc, char *argv[] )
{
  struct hvl_tune *tune;

  if( argc < 2 )
  {
    printf( "Usage: play_hvl <tune>\n" );
    return 0;
  }

  if( init() )
  {
    hvl_InitReplayer();
    tune = hvl_LoadTune( argv[1], FREQ, 2 );
    if( tune )
    {
      BOOL done;
      uint32 gotsigs;
      
      hvl_InitSubsong( tune, 0 );

      nextbuf = 0;
      mix_and_play( tune );

      done = FALSE;
      
      while( !done )
      {
        gotsigs = IExec->Wait( ahisig | SIGBREAKF_CTRL_C );
      
        if( gotsigs & ahisig ) mix_and_play( tune );
        if( gotsigs & SIGBREAKF_CTRL_C ) done = TRUE;
      }
      hvl_FreeTune( tune );
    }
  }
  shut();
  return 0;
}
