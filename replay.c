
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define __USE_BASETYPE__ 1
#include <system_includes.h>

#include "util.h"
#include "replay.h"
#include "tables.h"
#include "gui.h"
#include "undo.h"

extern BOOL quitting;
extern int32 defnotejump, definotejump;

APTR rp_mempool = NULL;

#ifndef __SDL_WRAPPER__
#define FREQ 48000
extern uint32 gui_tick_sig;
struct Task    *rp_maintask    = NULL;
struct Process *rp_subtask     = NULL;
int32           rp_subtask_sig = -1;
struct MsgPort *rp_msgport     = NULL;
uint32          rp_sigs        = 0;

uint32          rp_freq        = FREQ;
float64         rp_freqf       = FREQ;
uint32          rp_audiobuflen;
int8           *rp_audiobuffer[2]  = { NULL, NULL };

struct MsgPort    *rp_replyport = NULL;
#endif

struct rp_command *rp_mainmsg   = NULL;

struct List    *rp_tunelist    = NULL;

struct ahx_tune *rp_curtune    = NULL;

struct SignalSemaphore *rp_list_ss = NULL;

enum {
  STS_IDLE = 0,
  STS_PLAYNOTE,
  STS_PLAYPOS,
  STS_PLAYSONG,
  STS_RECORDING,
  STS_CALCULATING,
  STS_DEADED,
  STS_PLAYROW
};

uint32 rp_state = STS_IDLE;
#ifdef __SDL_WRAPPER__
volatile uint32 rp_state_ack = STS_IDLE;
#endif
extern int32 pref_defstereo;

#ifndef __SDL_WRAPPER__
// Libraries and interfaces
struct Library  *AHIBase = NULL;
struct AHIIFace *IAHI = NULL;

// AHI stuff
struct MsgPort    *ahi_mp;
struct AHIRequest *ahi_io[2] = { NULL, NULL };
int32              ahi_dev = -1;
#else
#define FREQ 48000
#ifdef __linux__
#define OUTPUT_LEN ((FREQ/50)*2) /* Linux can't cope with buffer being too small, so we sacrifice GUI responsiveness... */
#else
#define OUTPUT_LEN (FREQ/50)
#endif
static struct ahx_tune *rp_playtune = NULL;
int16 rp_audiobuffer[OUTPUT_LEN*2];
uint32 rp_audiobuflen = OUTPUT_LEN*2;
#endif

/*
** Waves
*/

int8 waves[WAVES_SIZE];

uint32 panning_left[256], panning_right[256];
/*
static inline void clr_l( uint32 *src, uint32 longs)
{
  do {
    *src++ = 0;
    longs--;
  } while (longs > 0);
}
*/
void rp_GenPanningTables( void )
{
  uint32 i;
  float64 aa, ab;

  // Sine based panning table
  aa = (3.14159265f*2.0f)/4.0f;   // Quarter of the way through the sinewave == top peak
  ab = 0.0f;                      // Start of the climb from zero

  for( i=0; i<256; i++ )
  {
    panning_left[i]  = (uint32)(sin(aa)*255.0f);
    panning_right[i] = (uint32)(sin(ab)*255.0f);
    
    aa += (3.14159265*2.0f/4.0f)/256.0f;
    ab += (3.14159265*2.0f/4.0f)/256.0f;
  }
  panning_left[255] = 0;
  panning_right[0] = 0;

/*
  // Original panning table (linear panning == poo)
  for( i=0; i<256; i++ )
  {
    // Compensate for -128 to 127 range (ideally we want -128 to 128)

    if( i < 64 )
      panning_left[i] = 256-i;
    else
      panning_left[i] = 255-i;

    if( i > 191 )
      panning_right[i] = i+1;
    else
      panning_right[i] = i;
    
    if( panning_left[i] == 128 ) panning_right[i] = 128;
    if( panning_right[i] == 128 ) panning_left[i] = 128;
  }
*/
}

void rp_GenSawtooth( int8 *buf, uint32 len )
{
  uint32 i;
  int32  val, add;
  
  add = 256 / (len-1);
  val = -128;
  
  for( i=0; i<len; i++, val += add )
    *buf++ = (int8)val;  
}

void rp_GenTriangle( int8 *buf, uint32 len )
{
  uint32 i;
  int32  d2, d5, d1, d4;
  int32  val;
  const int8   *buf2;
  
  d2  = len;
  d5  = len >> 2;
  d1  = 128/d5;
  d4  = -(d2 >> 1);
  val = 0;
  
  for( i=0; i<d5; i++ )
  {
    *buf++ = val;
    val += d1;
  }
  *buf++ = 0x7f;

  if( d5 != 1 )
  {
    val = 128;
    for( i=0; i<d5-1; i++ )
    {
      val -= d1;
      *buf++ = val;
    }
  }
  
  buf2 = buf + d4;
  for( i=0; i<d5*2; i++ )
  {
    int8 c;
    
    c = *buf2++;
    if( c == 0x7f )
      c = 0x80;
    else
      c = -c;
    
    *buf++ = c;
  }
}

void rp_GenSquare( int8 *buf )
{
  uint32 i, j;
  
  for( i=1; i<=0x20; i++ )
  {
    for( j=0; j<(0x40-i)*2; j++ )
      *buf++ = 0x80;
    for( j=0; j<i*2; j++ )
      *buf++ = 0x7f;
  }
}

static inline int32 clipshifted8(int32 in)
{
  int16 top = (int16)(in >> 16);
  if (top > 127) in = 127 << 16;
  else if (top < -128) in = -(128 << 16);
  return in;
}

void rp_GenFilterWaves( const int8 *buf, int8 *lowbuf, int8 *highbuf )
{


  const int16 * mid_table = &filter_thing[0];
  const int16 * low_table = &filter_thing[1395];

  int32 freq;
  int32 i;

  for( i=0, freq = 25; i<31; i++, freq += 9 )
  {
    uint32 wv;
    const int8  *a0 = buf;

    for( wv=0; wv<6+6+0x20+1; wv++ )
    {
      int32 in, fre, high, mid, low;
      uint32  j;

      mid  = *mid_table++ << 8;
      low = *low_table++ << 8;

      for( j=0; j<=lentab[wv]; j++ )
      {
        in   = a0[j] << 16;
        high = clipshifted8( in - mid - low );
        fre  = (high >> 8) * freq;
        mid  = clipshifted8(mid + fre);
        fre  = (mid  >> 8) * freq;
        low  = clipshifted8(low + fre);
        *highbuf++ = high >> 16;
        *lowbuf++  = low  >> 16;
      }
      a0 += lentab[wv]+1;
    }
  }
}
        
void rp_GenWhiteNoise( int8 *buf, uint32 len )
{
  uint32 ays;

  ays = 0x41595321;

  do {
    uint16 ax, bx;
    int8 s;

    s = ays;

    if( ays & 0x100 )
    {
      s = 0x7f;

      if( ays & 0x8000 )
        s = 0x80;
    }

    *buf++ = s;
    len--;

    ays = (ays >> 5) | (ays << 27);
    ays = (ays & 0xffffff00) | ((ays & 0xff) ^ 0x9a);
    bx  = ays;
    ays = (ays << 2) | (ays >> 30);
    ax  = ays;
    bx  += ax;
    ax  ^= bx;
    ays  = (ays & 0xffff0000) | ax;
    ays  = (ays >> 3) | (ays << 29);
  } while( len );
}

void rp_clear_instrument( struct ahx_instrument *ins )
{
  int32 j;

  ins->ins_Name[0]          = 0;
  ins->ins_Volume           = 64;
  ins->ins_WaveLength       = 3;
  ins->ins_FilterLowerLimit = 1;
  ins->ins_FilterUpperLimit = 1;
  ins->ins_FilterSpeed      = 0;
  ins->ins_SquareLowerLimit = 1;
  ins->ins_SquareUpperLimit = 1;
  ins->ins_SquareSpeed      = 0;
  ins->ins_VibratoDelay     = 0;
  ins->ins_VibratoSpeed     = 0;
  ins->ins_VibratoDepth     = 0;
  ins->ins_HardCutRelease   = 0;
  ins->ins_HardCutReleaseFrames = 0;
  ins->ins_Envelope.aFrames = 1;
  ins->ins_Envelope.aVolume = 0;
  ins->ins_Envelope.dFrames = 1;
  ins->ins_Envelope.dVolume = 0;
  ins->ins_Envelope.sFrames = 1;
  ins->ins_Envelope.rFrames = 1;
  ins->ins_Envelope.rVolume = 0;
  ins->ins_PList.pls_Speed  = 0;
  ins->ins_PList.pls_Length = 0;
  for( j=0; j<256; j++ )
  {
    ins->ins_PList.pls_Entries[j].ple_Note       = 0;
    ins->ins_PList.pls_Entries[j].ple_Waveform   = 0;
    ins->ins_PList.pls_Entries[j].ple_Fixed      = 0;
    ins->ins_PList.pls_Entries[j].ple_FX[0]      = 0;
    ins->ins_PList.pls_Entries[j].ple_FXParam[0] = 0;
    ins->ins_PList.pls_Entries[j].ple_FX[1]      = 0;
    ins->ins_PList.pls_Entries[j].ple_FXParam[1] = 0;
  }
  ins->ins_ptop  = 0;
  ins->ins_pcurx = 0;
  ins->ins_pcury = 0;
}

void rp_clear_tune( struct ahx_tune *at )
{
  int32 i, j;
  int32 defgain[] = { 71, 72, 76, 85, 100 };

  if( at == NULL ) return;
  if( at == rp_curtune ) rp_stop();

  free_undolists( at );

  at->at_Name[0]         = 0;
  at->at_Time            = 0;
  at->at_ExactTime       = 0;
  at->at_LoopDetector    = 0;
  at->at_SongNum         = 0;
  at->at_Frequency       = FREQ;
  at->at_FreqF           = (float64)FREQ;
  at->at_WaveformTab[0]  = &waves[WO_TRIANGLE_04];
  at->at_WaveformTab[1]  = &waves[WO_SAWTOOTH_04];
  at->at_WaveformTab[3]  = &waves[WO_WHITENOISE];
  at->at_Restart         = 0;
  at->at_PositionNr      = 1;
  at->at_SpeedMultiplier = 1;
  at->at_TrackLength     = 64;
  at->at_TrackNr         = 0;
  at->at_InstrumentNr    = 0;
  at->at_SubsongNr       = 0;
  at->at_PosJump         = 0;
  at->at_PlayingTime     = 0;
  at->at_Tempo           = 6;
  at->at_PosNr           = 0;
  at->at_NextPosNr       = -1;
  at->at_StepWaitFrames  = 0;
  at->at_NoteNr          = 0;
  at->at_PosJumpNote     = 0;
  at->at_GetNewPosition  = 0;
  at->at_PatternBreak    = 0;
  at->at_SongEndReached  = 0;
  at->at_posed_curs      = 0;
  at->at_tracked_curs    = 0;
  at->at_curins          = 1;
  at->at_drumpadnote     = 25;
  at->at_drumpadmode     = FALSE;
  at->at_topins          = 1;
  at->at_topinsb         = 1;
  at->at_curlch          = 0;
  at->at_Channels        = 4;
  at->at_baseins         = 0;
  at->at_curpanel        = 0;
  at->at_modified        = FALSE;
  at->at_defstereo       = pref_defstereo;
  at->at_defpanleft      = stereopan_left[pref_defstereo];
  at->at_defpanright     = stereopan_right[pref_defstereo];
  at->at_mixgain         = (defgain[pref_defstereo]*256)/100;
  at->at_mixgainP        = defgain[pref_defstereo];

  at->at_cbmarktrack     = -1;
  at->at_cbmarkstartnote = -1;
  at->at_cbmarkendnote   = -1;
  at->at_cbmarkleftx     = 0;
  at->at_cbmarkrightx    = 0;
  
  at->at_cbpmarklft      = -1;
  at->at_cbpmarkmarklft  = -1;
  
  at->at_notejump        = defnotejump;
  at->at_inotejump       = definotejump;

  at->at_doing           = D_IDLE;
  at->at_editing         = E_TRACK;
  at->at_idoing          = D_IDLE;
  
  at->at_rempos[0] =  0;
  at->at_rempos[1] = 16;
  at->at_rempos[2] = 32;
  at->at_rempos[3] = 48;
  at->at_rempos[4] = 63;
  
  for( i=0; i<MAX_CHANNELS; i++ )
  {
    at->at_Voices[i].vc_VUMeter    = 0;
    at->at_Voices[i].vc_SetTrackOn = 1;
  }
  
  for( i=0; i<MAX_CHANNELS; i+=4 )
  {
    at->at_Voices[i+0].vc_Pan          = at->at_defpanleft;
    at->at_Voices[i+0].vc_SetPan       = at->at_defpanleft;
    at->at_Voices[i+0].vc_PanMultLeft  = panning_left[at->at_defpanleft];
    at->at_Voices[i+0].vc_PanMultRight = panning_right[at->at_defpanleft];
    at->at_Voices[i+1].vc_Pan          = at->at_defpanright;
    at->at_Voices[i+1].vc_SetPan       = at->at_defpanright;
    at->at_Voices[i+1].vc_PanMultLeft  = panning_left[at->at_defpanright];
    at->at_Voices[i+1].vc_PanMultRight = panning_right[at->at_defpanright];
    at->at_Voices[i+2].vc_Pan          = at->at_defpanright;
    at->at_Voices[i+2].vc_SetPan       = at->at_defpanright;
    at->at_Voices[i+2].vc_PanMultLeft  = panning_left[at->at_defpanright];
    at->at_Voices[i+2].vc_PanMultRight = panning_right[at->at_defpanright];
    at->at_Voices[i+3].vc_Pan          = at->at_defpanleft;
    at->at_Voices[i+3].vc_SetPan       = at->at_defpanleft;
    at->at_Voices[i+3].vc_PanMultLeft  = panning_left[at->at_defpanleft];
    at->at_Voices[i+3].vc_PanMultRight = panning_right[at->at_defpanleft];
  }

  for( i=0; i<256; i++ )
  {
    at->at_Subsongs[i] = 0;

    for( j=0; j<64; j++ )
    {
      at->at_Tracks[i][j].stp_Note       = 0;
      at->at_Tracks[i][j].stp_Instrument = 0;
      at->at_Tracks[i][j].stp_FX         = 0;
      at->at_Tracks[i][j].stp_FXParam    = 0;
      at->at_Tracks[i][j].stp_FXb        = 0;
      at->at_Tracks[i][j].stp_FXbParam   = 0;
    }
  }
  
  for( i=0; i<1000; i++ )
  {
    for( j=0; j<MAX_CHANNELS; j++ )
    {
      at->at_Positions[i].pos_Track[j]     = 0;
      at->at_Positions[i].pos_Transpose[j] = 0;
    }
  }
  

  for( i=0; i<64; i++ )
    rp_clear_instrument( &at->at_Instruments[i] );

  at->at_ticks = at->at_secs = at->at_mins = at->at_hours = 0;
}

void rp_reset_some_shit( struct ahx_tune *at )
{
  uint32 i;

  at->at_ticks = at->at_secs = at->at_mins = at->at_hours = 0;

  for( i=0; i<MAX_CHANNELS; i++ )
  {
    at->at_Voices[i].vc_Delta=1;
    at->at_Voices[i].vc_OverrideTranspose = 1000; // 1.5
    at->at_Voices[i].vc_SamplePos=at->at_Voices[i].vc_Track=at->at_Voices[i].vc_Transpose=at->at_Voices[i].vc_NextTrack = at->at_Voices[i].vc_NextTranspose = 0;
    at->at_Voices[i].vc_ADSRVolume=at->at_Voices[i].vc_InstrPeriod=at->at_Voices[i].vc_TrackPeriod=at->at_Voices[i].vc_VibratoPeriod=at->at_Voices[i].vc_NoteMaxVolume=at->at_Voices[i].vc_PerfSubVolume=at->at_Voices[i].vc_TrackMasterVolume=0;
    at->at_Voices[i].vc_NewWaveform=at->at_Voices[i].vc_Waveform=at->at_Voices[i].vc_PlantSquare=at->at_Voices[i].vc_PlantPeriod=at->at_Voices[i].vc_IgnoreSquare=0;
    at->at_Voices[i].vc_TrackOn=at->at_Voices[i].vc_FixedNote=at->at_Voices[i].vc_VolumeSlideUp=at->at_Voices[i].vc_VolumeSlideDown=at->at_Voices[i].vc_HardCut=at->at_Voices[i].vc_HardCutRelease=at->at_Voices[i].vc_HardCutReleaseF=0;
    at->at_Voices[i].vc_PeriodSlideSpeed=at->at_Voices[i].vc_PeriodSlidePeriod=at->at_Voices[i].vc_PeriodSlideLimit=at->at_Voices[i].vc_PeriodSlideOn=at->at_Voices[i].vc_PeriodSlideWithLimit=0;
    at->at_Voices[i].vc_PeriodPerfSlideSpeed=at->at_Voices[i].vc_PeriodPerfSlidePeriod=at->at_Voices[i].vc_PeriodPerfSlideOn=at->at_Voices[i].vc_VibratoDelay=at->at_Voices[i].vc_VibratoCurrent=at->at_Voices[i].vc_VibratoDepth=at->at_Voices[i].vc_VibratoSpeed=0;
    at->at_Voices[i].vc_SquareOn=at->at_Voices[i].vc_SquareInit=at->at_Voices[i].vc_SquareLowerLimit=at->at_Voices[i].vc_SquareUpperLimit=at->at_Voices[i].vc_SquarePos=at->at_Voices[i].vc_SquareSign=at->at_Voices[i].vc_SquareSlidingIn=at->at_Voices[i].vc_SquareReverse=0;
    at->at_Voices[i].vc_FilterOn=at->at_Voices[i].vc_FilterInit=at->at_Voices[i].vc_FilterLowerLimit=at->at_Voices[i].vc_FilterUpperLimit=at->at_Voices[i].vc_FilterPos=at->at_Voices[i].vc_FilterSign=at->at_Voices[i].vc_FilterSpeed=at->at_Voices[i].vc_FilterSlidingIn=at->at_Voices[i].vc_IgnoreFilter=0;
    at->at_Voices[i].vc_PerfCurrent=at->at_Voices[i].vc_PerfSpeed=at->at_Voices[i].vc_WaveLength=at->at_Voices[i].vc_NoteDelayOn=at->at_Voices[i].vc_NoteCutOn=0;
    at->at_Voices[i].vc_AudioPeriod=at->at_Voices[i].vc_AudioVolume=at->at_Voices[i].vc_VoiceVolume=at->at_Voices[i].vc_VoicePeriod=at->at_Voices[i].vc_VoiceNum=at->at_Voices[i].vc_WNRandom=0;
    at->at_Voices[i].vc_SquareWait=at->at_Voices[i].vc_FilterWait=at->at_Voices[i].vc_PerfWait=at->at_Voices[i].vc_NoteDelayWait=at->at_Voices[i].vc_NoteCutWait=0;
    at->at_Voices[i].vc_PerfList=0;
    at->at_Voices[i].vc_RingSamplePos=at->at_Voices[i].vc_RingDelta=at->at_Voices[i].vc_RingPlantPeriod=at->at_Voices[i].vc_RingAudioPeriod=at->at_Voices[i].vc_RingNewWaveform=at->at_Voices[i].vc_RingWaveform=at->at_Voices[i].vc_RingFixedPeriod=at->at_Voices[i].vc_RingBasePeriod=0;

    at->at_Voices[i].vc_RingMixSource = NULL;
    at->at_Voices[i].vc_RingAudioSource = NULL;

    memset( &at->at_Voices[i].vc_SquareTempBuffer,0,0x80);
    memset( &at->at_Voices[i].vc_ADSR,0,sizeof(struct ahx_envelope));
    memset( &at->at_Voices[i].vc_VoiceBuffer,0,0x281);
    memset( &at->at_Voices[i].vc_RingVoiceBuffer,0,0x281);
  }
  
  for( i=0; i<MAX_CHANNELS; i++ )
  {
    at->at_Voices[i].vc_WNRandom          = 0x280;
    at->at_Voices[i].vc_VoiceNum          = i;
    at->at_Voices[i].vc_TrackMasterVolume = 0x40;
    at->at_Voices[i].vc_TrackOn           = at->at_Voices[i].vc_SetTrackOn;
    at->at_Voices[i].vc_MixSource         = at->at_Voices[i].vc_VoiceBuffer;
  }
}

BOOL rp_init_subsong( struct ahx_tune *at, uint32 nr )
{
  uint32 PosNr, i;

  if( nr > at->at_SubsongNr )
    return FALSE;

  at->at_SongNum = nr;
  
  PosNr = 0;
  if( nr ) PosNr = at->at_Subsongs[nr-1];
  
  at->at_PosNr          = PosNr;
  at->at_PosJump        = 0;
  at->at_PatternBreak   = 0;
  at->at_NoteNr         = 0;
  at->at_PosJumpNote    = 0;
  at->at_Tempo          = 6;
  at->at_StepWaitFrames	= 0;
  at->at_GetNewPosition = 1;
  at->at_SongEndReached = 0;
  at->at_PlayingTime    = 0;
  at->at_curss          = nr;
  
  for( i=0; i<MAX_CHANNELS; i+=4 )
  {
    at->at_Voices[i+0].vc_Pan          = at->at_defpanleft;
    at->at_Voices[i+0].vc_SetPan       = at->at_defpanleft;
    at->at_Voices[i+0].vc_PanMultLeft  = panning_left[at->at_defpanleft];
    at->at_Voices[i+0].vc_PanMultRight = panning_right[at->at_defpanleft];
    at->at_Voices[i+1].vc_Pan          = at->at_defpanright;
    at->at_Voices[i+1].vc_SetPan       = at->at_defpanright;
    at->at_Voices[i+1].vc_PanMultLeft  = panning_left[at->at_defpanright];
    at->at_Voices[i+1].vc_PanMultRight = panning_right[at->at_defpanright];
    at->at_Voices[i+2].vc_Pan          = at->at_defpanright;
    at->at_Voices[i+2].vc_SetPan       = at->at_defpanright;
    at->at_Voices[i+2].vc_PanMultLeft  = panning_left[at->at_defpanright];
    at->at_Voices[i+2].vc_PanMultRight = panning_right[at->at_defpanright];
    at->at_Voices[i+3].vc_Pan          = at->at_defpanleft;
    at->at_Voices[i+3].vc_SetPan       = at->at_defpanleft;
    at->at_Voices[i+3].vc_PanMultLeft  = panning_left[at->at_defpanleft];
    at->at_Voices[i+3].vc_PanMultRight = panning_right[at->at_defpanleft];
  }

  rp_reset_some_shit( at );
  
  return TRUE;
}

struct ahx_tune *rp_new_tune( BOOL addtolist )
{
  struct ahx_tune *at;
  
  at = (struct ahx_tune *)allocnode(sizeof(struct ahx_tune));
  if( !at )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return NULL;
  }
  
  at->at_undolist = IExec->AllocSysObjectTags( ASOT_LIST, TAG_DONE );
  if( !at->at_undolist )
  {
    IExec->FreeSysObject( ASOT_NODE, at );
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return NULL;
  }

  at->at_redolist = IExec->AllocSysObjectTags( ASOT_LIST, TAG_DONE );
  if( !at->at_redolist )
  {
    IExec->FreeSysObject( ASOT_LIST, at->at_undolist );
    IExec->FreeSysObject( ASOT_NODE, at );
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return NULL;
  }
  
  at->at_undomem         = 0;

  at->at_ln.ln_Succ      = NULL;
  at->at_ln.ln_Pred      = NULL;
  at->at_ln.ln_Name      = &at->at_Name[0];
  at->at_ln.ln_Pri       = 0;
  at->at_ln.ln_Type      = NT_USER;

  rp_clear_tune( at );

  if( addtolist )
  {
    IExec->ObtainSemaphore( rp_list_ss );
    IExec->AddTail( rp_tunelist, (struct Node *)at );
    IExec->ReleaseSemaphore( rp_list_ss );
  }
  
  rp_init_subsong( at, 0 );
  return at;  
}

void rp_free_tune( struct ahx_tune *at )
{
  IExec->ObtainSemaphore( rp_list_ss );
  IExec->Remove( (struct Node *)at );

  free_undolists( at );

  IExec->FreeSysObject( ASOT_LIST, at->at_undolist );
  IExec->FreeSysObject( ASOT_LIST, at->at_redolist );
  IExec->FreeSysObject( ASOT_NODE, at );
  IExec->ReleaseSemaphore( rp_list_ss );
}

void rp_free_all_tunes( void )
{	
  struct ahx_tune *at, *nat;
  
  IExec->ObtainSemaphore( rp_list_ss );
  at = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
  while( at )
  {
    nat = (struct ahx_tune *)IExec->GetSucc(&at->at_ln);
    rp_free_tune( at );
    at = nat;
  }
  IExec->ReleaseSemaphore( rp_list_ss );
}

void rp_save_hvl_ins( const TEXT *name, struct ahx_instrument *ins )
{
  FILE *fh;
  int8 *buf, *bptr;
  int32 buflen, i;

  buflen = 26+ins->ins_PList.pls_Length*5;
  buf = IExec->AllocPooled( rp_mempool, buflen );
  if( !buf )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return;
  }

  for( i=0; i<buflen; i++ ) buf[i] = 0;
  
  strcpy( (TEXT *)buf, "HVLI" );
     
  buf[4] = ins->ins_Volume;
  buf[5] = (ins->ins_FilterSpeed&0x1f)<<3;
  buf[16] = (ins->ins_FilterSpeed&0x20)<<2;
  buf[5] |= ins->ins_WaveLength&0x07;
   
  buf[6]  = ins->ins_Envelope.aFrames;
  buf[7]  = ins->ins_Envelope.aVolume;
  buf[8]  = ins->ins_Envelope.dFrames;
  buf[9]  = ins->ins_Envelope.dVolume;
  buf[10] = ins->ins_Envelope.sFrames;
  buf[11] = ins->ins_Envelope.rFrames;
  buf[12] = ins->ins_Envelope.rVolume;
    
  buf[16] |= ins->ins_FilterLowerLimit&0x7f;
  buf[17] = ins->ins_VibratoDelay;
  buf[18] = (ins->ins_HardCutReleaseFrames&0x07)<<4;
  buf[18] |= (ins->ins_HardCutRelease&0x1)<<7;
  buf[18] |= ins->ins_VibratoDepth&0x0f;
  buf[19] = ins->ins_VibratoSpeed;
  buf[20] = ins->ins_SquareLowerLimit;
  buf[21] = ins->ins_SquareUpperLimit;
  buf[22] = ins->ins_SquareSpeed;
  buf[23] = ins->ins_FilterUpperLimit&0x3f;
  buf[24] = ins->ins_PList.pls_Speed;
  buf[25] = ins->ins_PList.pls_Length;
    
  bptr = &buf[26];
    
  for( i=0; i<ins->ins_PList.pls_Length; i++ )
  {
    bptr[0]  = ins->ins_PList.pls_Entries[i].ple_FX[0]&0xf;
    bptr[1]  = (ins->ins_PList.pls_Entries[i].ple_FX[1]&0xf)<<3;
    bptr[1] |= ins->ins_PList.pls_Entries[i].ple_Waveform&7;
    bptr[2]  = (ins->ins_PList.pls_Entries[i].ple_Fixed&1)<<6;
    bptr[2] |= ins->ins_PList.pls_Entries[i].ple_Note&0x3f;
    bptr[3]  = ins->ins_PList.pls_Entries[i].ple_FXParam[0];
    bptr[4]  = ins->ins_PList.pls_Entries[i].ple_FXParam[1];
    bptr += 5;
  }

  fh = fopen( name, "wb" );
  if( !fh )
  {
    printf( "unable to open file\n" );
    IExec->FreePooled( rp_mempool, buf, buflen );
    return;
  }
  
  fwrite( buf, buflen, 1, fh );
  fwrite( ins->ins_Name, strlen( ins->ins_Name )+1, 1, fh );
  fclose( fh );
  IExec->FreePooled( rp_mempool, buf, buflen );
}

void rp_save_ins( const TEXT *name, struct ahx_tune *at, int32 in )
{
  FILE *fh;
  int8 *buf, *bptr;
  int32 buflen, i, k, l;
  struct ahx_instrument *ins;
  BOOL saveahxi;
#ifdef __SDL_WRAPPER__
  char mkname[4096];
#endif
  
  if( at == NULL ) return;

#ifdef __SDL_WRAPPER__
  // Enforce file extension since windows likes them so much
  // (although let them use a prefix if they want to keep it oldskool)
  if (strncasecmp(name, "ins.", 4) != 0)
  {
    // No prefix...
    i = strlen(name);
    if ((i < 4) || (strcasecmp(&name[i-4], ".ins") != 0))
    {
      // No extension
      strncpy(mkname, name, 4096);
      strncat(mkname, ".ins", 4096);
      mkname[4095] = 0;
      name = mkname;
    }
  }
#endif

  ins = &at->at_Instruments[in];

  saveahxi = TRUE;

  // Have to save as HVL instrument?
  for( i=0; i<ins->ins_PList.pls_Length; i++ )
  {
    if( ( ins->ins_PList.pls_Entries[i].ple_FX[0] > 5 ) &&
        ( ins->ins_PList.pls_Entries[i].ple_FX[0] != 12 ) &&
        ( ins->ins_PList.pls_Entries[i].ple_FX[0] != 15 ) )
      saveahxi = FALSE;
    if( ( ins->ins_PList.pls_Entries[i].ple_FX[1] > 5 ) &&
        ( ins->ins_PList.pls_Entries[i].ple_FX[1] != 12 ) &&
        ( ins->ins_PList.pls_Entries[i].ple_FX[1] != 15 ) )
      saveahxi = FALSE;
  }
  
  if( saveahxi == FALSE )
  {
    rp_save_hvl_ins( name, ins );
    return;
  }

  buflen = 26+ins->ins_PList.pls_Length*4;
  buf = IExec->AllocPooled( rp_mempool, buflen );
  if( !buf )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return;
  }
  
  for( i=0; i<buflen; i++ ) buf[i] = 0;
  
  strcpy( (TEXT *)buf, "THXI" );

  buf[4] = ins->ins_Volume;
  buf[5] = (ins->ins_FilterSpeed&0x1f)<<3;
  buf[16] = (ins->ins_FilterSpeed&0x20)<<2;
  buf[5] |= ins->ins_WaveLength&0x07;
   
  buf[6]  = ins->ins_Envelope.aFrames;
  buf[7]  = ins->ins_Envelope.aVolume;
  buf[8]  = ins->ins_Envelope.dFrames;
  buf[9]  = ins->ins_Envelope.dVolume;
  buf[10] = ins->ins_Envelope.sFrames;
  buf[11] = ins->ins_Envelope.rFrames;
  buf[12] = ins->ins_Envelope.rVolume;
    
  buf[16] |= ins->ins_FilterLowerLimit&0x7f;
  buf[17] = ins->ins_VibratoDelay;
  buf[18] = (ins->ins_HardCutReleaseFrames&0x07)<<4;
  buf[18] |= (ins->ins_HardCutRelease&0x1)<<7;
  buf[18] |= ins->ins_VibratoDepth&0x0f;
  buf[19] = ins->ins_VibratoSpeed;
  buf[20] = ins->ins_SquareLowerLimit;
  buf[21] = ins->ins_SquareUpperLimit;
  buf[22] = ins->ins_SquareSpeed;
  buf[23] = ins->ins_FilterUpperLimit&0x3f;
  buf[24] = ins->ins_PList.pls_Speed;
  buf[25] = ins->ins_PList.pls_Length;
    
  bptr = &buf[26];
    
  for( i=0; i<ins->ins_PList.pls_Length; i++ )
  {
    // Convert to AHX format (this means that HVL instruments could have more FX! w00t!)
    k = ins->ins_PList.pls_Entries[i].ple_FX[1];
    if( k == 12 ) k = 6;
    if( k == 15 ) k = 7;
    l = ins->ins_PList.pls_Entries[i].ple_FX[0];
    if( l == 12 ) l = 6;
    if( l == 15 ) l = 7;
    bptr[0] = (k&7)<<5;
    bptr[0] |= (l&7)<<2;
    bptr[0] |= (ins->ins_PList.pls_Entries[i].ple_Waveform&6)>>1;
    bptr[1] = (ins->ins_PList.pls_Entries[i].ple_Waveform&1)<<7;
    bptr[1] |= (ins->ins_PList.pls_Entries[i].ple_Fixed&1)<<6;
    bptr[1] |= ins->ins_PList.pls_Entries[i].ple_Note&0x3f;
    bptr[2] = ins->ins_PList.pls_Entries[i].ple_FXParam[0];
    bptr[3] = ins->ins_PList.pls_Entries[i].ple_FXParam[1];
    bptr += 4;
  }

  fh = fopen( name, "wb" );
  if( !fh )
  {
    printf( "unable to open file\n" );
    IExec->FreePooled( rp_mempool, buf, buflen );
    return;
  }
  
  fwrite( buf, buflen, 1, fh );
  fwrite( ins->ins_Name, strlen( ins->ins_Name )+1, 1, fh );
  fclose( fh );
  IExec->FreePooled( rp_mempool, buf, buflen );
}

void rp_load_ins( const TEXT *name, struct ahx_tune *at, int32 in )
{
  FILE *fh;
  uint8  *buf;
  const uint8 *bptr;
  uint32  buflen, i, k, l;
  struct ahx_instrument *ni;
  BOOL ahxi;
  
  if( at == NULL ) return;

  fh = fopen(name, "rb");
  if( !fh )
  {
    printf( "Can't open file\n" );
    return;
  }

  fseek(fh, 0, SEEK_END);
  buflen = ftell(fh);
  fseek(fh, 0, SEEK_SET);

  buf = IExec->AllocPooled( rp_mempool, buflen );
  if( !buf )
  {
    fclose(fh);
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return;
  }
  
  if( fread( buf, buflen, 1, fh ) != 1 )
  {
    IExec->FreePooled( rp_mempool, buf, buflen );
    fclose(fh);
    printf( "Unable to read from file!\n" );
    return;
  }
  fclose(fh);
  
  ahxi = TRUE;
  
  if( ( buf[0] == 'H' ) &&
      ( buf[1] == 'V' ) &&
      ( buf[2] == 'L' ) &&
      ( buf[3] == 'I' ) )
    ahxi = FALSE;
  
  if( ( ahxi == TRUE ) &&
      ( ( buf[0] != 'T' ) ||
        ( buf[1] != 'H' ) ||
        ( buf[2] != 'X' ) ||
        ( buf[3] != 'I' ) ) )
  {
    IExec->FreePooled( rp_mempool, buf, buflen );
    printf( "Invalid file\n" );
    return;
  }
  
  ni = &at->at_Instruments[in];

  ni->ins_Volume      = buf[4];
  ni->ins_FilterSpeed = ((buf[5]>>3)&0x1f)|((buf[16]>>2)&0x20);
  ni->ins_WaveLength  = buf[5]&0x07;
   
  ni->ins_Envelope.aFrames = buf[6];
  ni->ins_Envelope.aVolume = buf[7];
  ni->ins_Envelope.dFrames = buf[8];
  ni->ins_Envelope.dVolume = buf[9];
  ni->ins_Envelope.sFrames = buf[10];
  ni->ins_Envelope.rFrames = buf[11];
  ni->ins_Envelope.rVolume = buf[12];
    
  ni->ins_FilterLowerLimit     = buf[16]&0x7f;
  ni->ins_VibratoDelay         = buf[17];
  ni->ins_HardCutReleaseFrames = (buf[18]>>4)&0x07;
  ni->ins_HardCutRelease       = buf[18]&0x80?1:0;
  ni->ins_VibratoDepth         = buf[18]&0x0f;
  ni->ins_VibratoSpeed         = buf[19];
  ni->ins_SquareLowerLimit     = buf[20];
  ni->ins_SquareUpperLimit     = buf[21];
  ni->ins_SquareSpeed          = buf[22];
  ni->ins_FilterUpperLimit     = buf[23]&0x3f;
  ni->ins_PList.pls_Speed      = buf[24];
  ni->ins_PList.pls_Length     = buf[25];
    
  bptr = &buf[26];

  if( ahxi )
  {    
    for( i=0; i<ni->ins_PList.pls_Length; i++ )
    {
      k = (bptr[0]>>5)&7;
      if( k == 6 ) k = 12;
      if( k == 7 ) k = 15;
      l = (bptr[0]>>2)&7;
      if( l == 6 ) l = 12;
      if( l == 7 ) l = 15;
      ni->ins_PList.pls_Entries[i].ple_FX[1]      = k;
      ni->ins_PList.pls_Entries[i].ple_FX[0]      = l;
      ni->ins_PList.pls_Entries[i].ple_Waveform   = ((bptr[0]<<1)&6) | (bptr[1]>>7);
      ni->ins_PList.pls_Entries[i].ple_Fixed      = (bptr[1]>>6)&1;
      ni->ins_PList.pls_Entries[i].ple_Note       = bptr[1]&0x3f;
      ni->ins_PList.pls_Entries[i].ple_FXParam[0] = bptr[2];
      ni->ins_PList.pls_Entries[i].ple_FXParam[1] = bptr[3];
      bptr += 4;
    }
  } else {
    for( i=0; i<ni->ins_PList.pls_Length; i++ )
    {
      ni->ins_PList.pls_Entries[i].ple_FX[0] = bptr[0]&0xf;
      ni->ins_PList.pls_Entries[i].ple_FX[1] = (bptr[1]>>3)&0xf;
      ni->ins_PList.pls_Entries[i].ple_Waveform = bptr[1]&7;
      ni->ins_PList.pls_Entries[i].ple_Fixed = (bptr[2]>>6)&1;
      ni->ins_PList.pls_Entries[i].ple_Note  = bptr[2]&0x3f;
      ni->ins_PList.pls_Entries[i].ple_FXParam[0] = bptr[3];
      ni->ins_PList.pls_Entries[i].ple_FXParam[1] = bptr[4];
      bptr += 5;
    }
  }
  
  while( i < 256 )
  {
    ni->ins_PList.pls_Entries[i].ple_FX[0] = 0;
    ni->ins_PList.pls_Entries[i].ple_FX[1] = 0;
    ni->ins_PList.pls_Entries[i].ple_FXParam[0] = 0;
    ni->ins_PList.pls_Entries[i].ple_FXParam[1] = 0;
    ni->ins_PList.pls_Entries[i].ple_Waveform = 0;
    ni->ins_PList.pls_Entries[i].ple_Fixed = 0;
    ni->ins_PList.pls_Entries[i].ple_Note = 0;
    i++;
  }

  ni->ins_ptop = 0;
  ni->ins_pcurx = 0;
  ni->ins_pcury = 0;
  
  strncpy( ni->ins_Name, (TEXT *)bptr, 128 );

//  printf( "Loaded '%s'!\n", ni->ins_Name );
}

uint32 rp_ahx_test( const struct ahx_tune *at )
{
  int32 i, j;
  int32 hvlfeats;
  const struct ahx_instrument *in;
  const struct ahx_step       *sp;

  // First, check if the tune adheres to AHX limitations
  hvlfeats = 0;
  
  // >4 channels?
  if( at->at_Channels > 4 ) hvlfeats |= SWF_MANYCHANS;
  
  // Check instruments only use 1, 2, 3, 4, 5, C & F commands
  for( i=1; i<64; i++ )
  {
    in = &at->at_Instruments[i];
    for( j=0; j<in->ins_PList.pls_Length; j++ )
    {
      switch( in->ins_PList.pls_Entries[j].ple_FX[0] )
      {
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xa:
        case 0xb:
        case 0xd:
        case 0xe:
          hvlfeats |= SWF_NEWINSCMD;
          break;
      }

      switch( in->ins_PList.pls_Entries[j].ple_FX[1] )
      {
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
        case 0xa:
        case 0xb:
        case 0xd:
        case 0xe:
          hvlfeats |= SWF_NEWINSCMD;
          break;
      }
        
      if( hvlfeats & SWF_NEWINSCMD ) break;
    }
  }
  
  // Check for double or new commands
  for( i=0; i<256; i++ )
  {
    for( j=0; j<at->at_TrackLength; j++ )
    {
      sp = &at->at_Tracks[i][j];
      
      if( ( sp->stp_FX == 7 ) || ( sp->stp_FXb == 7 ) )
        hvlfeats |= SWF_PANCMD;
      
      if( ((sp->stp_FX==0xe)&&((sp->stp_FXParam&0xf0)==0xf0)) ||
          ((sp->stp_FXb==0xe)&&((sp->stp_FXParam&0xf0)==0xf0)) )
        hvlfeats |= SWF_EFXCMD;
      
      if( (( sp->stp_FX != 0 )  || ( sp->stp_FXParam != 0 )) &&
          (( sp->stp_FXb != 0 ) || ( sp->stp_FXbParam != 0 )) )
        hvlfeats |= SWF_DOUBLECMD;
    }
    
    if( (hvlfeats & (SWF_PANCMD|SWF_EFXCMD|SWF_DOUBLECMD)) == (SWF_PANCMD|SWF_EFXCMD|SWF_DOUBLECMD) )
      break;
  }
  
  return hvlfeats;
}

void rp_save_hvl( const TEXT *name, struct ahx_tune *at )
{
  FILE *fh;
  int32 i, j, k, tbl;
  int32 minver;
  uint8 emptytrk;
  uint8 *tbf;
  const struct ahx_instrument *in;
#ifdef __SDL_WRAPPER__
  char mkname[4096];
#endif
  
  if( at == NULL ) return;

#ifdef __SDL_WRAPPER__
  // Enforce file extension since windows likes them so much
  // (although let them use a prefix if they want to keep it oldskool)
  if (strncasecmp(name, "hvl.", 4) != 0)
  {
    // No prefix...
    i = strlen(name);
    if ((i < 4) || (strcasecmp(&name[i-4], ".hvl") != 0))
    {
      // No extension
      strncpy(mkname, name, 4096);
      strncat(mkname, ".hvl", 4096);
      mkname[4095] = 0;
      name = mkname;
    }
  }
#endif

  // Calculate TrackNr
  at->at_TrackNr = 0;
  for( i=0; i<at->at_PositionNr; i++ )
    for( j=0; j<at->at_Channels; j++ )
      if( at->at_Positions[i].pos_Track[j] > at->at_TrackNr )
        at->at_TrackNr = at->at_Positions[i].pos_Track[j];
  if( at->at_TrackNr == 0 ) at->at_TrackNr = 1;

  // Calculate InstrumentNr
  at->at_InstrumentNr = 0;
  for( i=1; i<64; i++ )
    if( at->at_Instruments[i].ins_PList.pls_Length > 0 )
      at->at_InstrumentNr = i;
  
  emptytrk = 0x80;
  for( i=0; i<at->at_TrackLength; i++ )
    if( ( at->at_Tracks[0][i].stp_Note != 0 ) ||
        ( at->at_Tracks[0][i].stp_Instrument != 0 ) ||
        ( at->at_Tracks[0][i].stp_FX != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXParam != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXb != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXbParam != 0 ) )
      emptytrk = 0;
  
  // Calculate names offset
  tbl  = 16;
  tbl += at->at_SubsongNr * 2;
  tbl += at->at_PositionNr * at->at_Channels * 2;
  
  minver = 0;
  for( i=0; i<=at->at_TrackNr; i++ )
  {
    if( ( emptytrk != 0 ) && ( i == 0 ) )
      continue;

    k = 0;
    for( j=0; j<at->at_TrackLength; j++ )
    {
      if( ( at->at_Tracks[i][j].stp_FX == 0xe ) &&
          ( (at->at_Tracks[i][j].stp_FXParam&0xf0) == 0xf0 ) &&
          ( minver < 1 ) )
        minver = 1;
      if( ( at->at_Tracks[i][j].stp_FXb == 0xe ) &&
          ( (at->at_Tracks[i][j].stp_FXbParam&0xf0) == 0xf0 ) &&
          ( minver < 1 ) )
        minver = 1;

      if( ( at->at_Tracks[i][j].stp_Note == 0 ) &&
          ( at->at_Tracks[i][j].stp_Instrument == 0 ) &&
          ( at->at_Tracks[i][j].stp_FX == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXParam == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXb == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXbParam == 0 ) )
        k++;
      else
        k+=5;
    }
    tbl += k;
  }
  
  for( i=1; i<=at->at_InstrumentNr; i++ )
    tbl += 22 + at->at_Instruments[i].ins_PList.pls_Length*5;

  tbf = IExec->AllocPooled( rp_mempool, tbl );
  if( !tbf )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return;
  }

  fh = fopen( name, "wb" );
  if( !fh )
  {
    IExec->FreePooled( rp_mempool, tbf, tbl );
    return;
  }
  
  strcpy( (TEXT *)tbf, "HVL" );

  tbf[3]  = minver;
  tbf[4]  =  (tbl >> 8)&0xff;
  tbf[5]  =  tbl&0xff;
  tbf[6]   = (at->at_PositionNr>>8);
  tbf[6]  |= ((at->at_SpeedMultiplier-1)&3)<<5;
  tbf[6]  |= emptytrk;
  tbf[7]   = at->at_PositionNr & 0xff;
  tbf[8]   = (at->at_Channels-4)<<2;
  tbf[8]  |= (at->at_Restart>>8)&0x3;
  tbf[9]   = at->at_Restart&0xff;
  tbf[10]  = at->at_TrackLength;
  tbf[11]  = at->at_TrackNr;
  tbf[12]  = at->at_InstrumentNr;
  tbf[13]  = at->at_SubsongNr;
  tbf[14] =  at->at_mixgainP;
  tbf[15] =  at->at_defstereo;
  fwrite( tbf, 16, 1, fh );

  // Subsongs
  if( at->at_SubsongNr > 0 )
  {
    for( i=0, k=0; i<at->at_SubsongNr; i++ )
    {
      tbf[k++] = at->at_Subsongs[i]>>8;
      tbf[k++] = at->at_Subsongs[i]&0xff;
    }
    fwrite( tbf, k, 1, fh );
  }
  
  // Position list
  for( i=0, k=0; i<at->at_PositionNr; i++ )
  {
    for( j=0; j<at->at_Channels; j++ )
    {
      tbf[k++] = at->at_Positions[i].pos_Track[j];
      tbf[k++] = at->at_Positions[i].pos_Transpose[j];
    }
  }
  fwrite( tbf, k, 1, fh );
  
  // Tracks
  for( i=0, k=0; i<=at->at_TrackNr; i++ )
  {
    if( ( emptytrk != 0 ) && ( i == 0 ) )
      continue;
    
    for( j=0; j<at->at_TrackLength; j++ )
    {
      if( ( at->at_Tracks[i][j].stp_Note == 0 ) &&
          ( at->at_Tracks[i][j].stp_Instrument == 0 ) &&
          ( at->at_Tracks[i][j].stp_FX == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXParam == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXb == 0 ) &&
          ( at->at_Tracks[i][j].stp_FXbParam == 0 ) )
      {
        tbf[k++] = 0x3f;
        continue;
      }
    
      tbf[k++]  = at->at_Tracks[i][j].stp_Note;
      tbf[k++]  = at->at_Tracks[i][j].stp_Instrument;
      tbf[k]    = (at->at_Tracks[i][j].stp_FX&0xf)<<4;
      tbf[k++] |= (at->at_Tracks[i][j].stp_FXb&0xf);
      tbf[k++]  = at->at_Tracks[i][j].stp_FXParam;
      tbf[k++]  = at->at_Tracks[i][j].stp_FXbParam;
    }
  }
  fwrite( tbf, k, 1, fh );

  // Instruments
  for( i=1; i<=at->at_InstrumentNr; i++ )
  {
    in = &at->at_Instruments[i];
    k = 0;
    tbf[0]   = in->ins_Volume;
    tbf[1]   = (in->ins_FilterSpeed&0x1f)<<3;
    tbf[1]  |= in->ins_WaveLength&0x7;
    tbf[2]   = in->ins_Envelope.aFrames;
    tbf[3]   = in->ins_Envelope.aVolume;
    tbf[4]   = in->ins_Envelope.dFrames;
    tbf[5]   = in->ins_Envelope.dVolume;
    tbf[6]   = in->ins_Envelope.sFrames;
    tbf[7]   = in->ins_Envelope.rFrames;
    tbf[8]   = in->ins_Envelope.rVolume;
    tbf[9]   = 0;
    tbf[10]  = 0;
    tbf[11]  = 0;
    tbf[12]  = (in->ins_FilterSpeed&0x20)<<2;
    tbf[12] |= in->ins_FilterLowerLimit&0x7f;
    tbf[13]  = in->ins_VibratoDelay;
    tbf[14]  = (in->ins_HardCutReleaseFrames&0x07)<<4;
    tbf[14] |= (in->ins_HardCutRelease&1)<<7;
    tbf[14] |= in->ins_VibratoDepth&0x0f;
    tbf[15]  = in->ins_VibratoSpeed;
    tbf[16]  = in->ins_SquareLowerLimit;
    tbf[17]  = in->ins_SquareUpperLimit;
    tbf[18]  = in->ins_SquareSpeed;
    tbf[19]  = in->ins_FilterUpperLimit&0x3f;
    tbf[20]  = in->ins_PList.pls_Speed;
    tbf[21]  = in->ins_PList.pls_Length;
    fwrite( tbf, 22, 1, fh );
     
    for( j=0, k=0; j<in->ins_PList.pls_Length; j++ )
    {
      tbf[k++] = in->ins_PList.pls_Entries[j].ple_FX[0]&0xf;
      tbf[k]    = (in->ins_PList.pls_Entries[j].ple_FX[1]&0xf)<<3;
      tbf[k++] |= in->ins_PList.pls_Entries[j].ple_Waveform&7;
      tbf[k]    = (in->ins_PList.pls_Entries[j].ple_Fixed&1)<<6;
      tbf[k++] |= in->ins_PList.pls_Entries[j].ple_Note&0x3f;
      tbf[k++]  = in->ins_PList.pls_Entries[j].ple_FXParam[0];
      tbf[k++]  = in->ins_PList.pls_Entries[j].ple_FXParam[1];
    }
    fwrite( tbf, k, 1, fh );  
  }
  
  fwrite( at->at_Name, strlen( at->at_Name )+1, 1, fh );
  for( i=1; i<=at->at_InstrumentNr; i++ )
    fwrite( at->at_Instruments[i].ins_Name, strlen( at->at_Instruments[i].ins_Name )+1, 1, fh );

  fclose( fh );
  IExec->FreePooled( rp_mempool, tbf, tbl );
}

void rp_save_ahx( const TEXT *name, struct ahx_tune *at )
{
  FILE *fh;
  int32 i, j, k, l, m, tbl;
  uint8 emptytrk;
  uint8 *tbf;
  const struct ahx_instrument *in;
#ifdef __SDL_WRAPPER__
  char mkname[4096];
#endif

  if( at == NULL ) return;

#ifdef __SDL_WRAPPER__
  // Enforce file extension since windows likes them so much
  // (although let them use a prefix if they want to keep it oldskool)
  if ((strncasecmp(name, "ahx.", 4) != 0) &&
      (strncasecmp(name, "thx.", 4) != 0))
  {
    // No prefix...
    i = strlen(name);
    if ((i < 4) ||
        ((strcasecmp(&name[i-4], ".ahx") != 0) &&
         (strcasecmp(&name[i-4], ".thx") != 0)))
    {
      // No extension
      strncpy(mkname, name, 4096);
      strncat(mkname, ".ahx", 4096);
      mkname[4095] = 0;
      name = mkname;
    }
  }
#endif

  // Calculate TrackNr
  at->at_TrackNr = 0;
  for( i=0; i<at->at_PositionNr; i++ )
    for( j=0; j<4; j++ )
      if( at->at_Positions[i].pos_Track[j] > at->at_TrackNr )
        at->at_TrackNr = at->at_Positions[i].pos_Track[j];
  if( at->at_TrackNr == 0 ) at->at_TrackNr = 1;

  // Calculate InstrumentNr
  at->at_InstrumentNr = 0;
  for( i=1; i<64; i++ )
    if( at->at_Instruments[i].ins_PList.pls_Length > 0 )
      at->at_InstrumentNr = i;
  
  emptytrk = 0x80;
  for( i=0; i<at->at_TrackLength; i++ )
    if( ( at->at_Tracks[0][i].stp_Note != 0 ) ||
        ( at->at_Tracks[0][i].stp_Instrument != 0 ) ||
        ( at->at_Tracks[0][i].stp_FX != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXParam != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXb != 0 ) ||
        ( at->at_Tracks[0][i].stp_FXbParam != 0 ) )
      emptytrk = 0;
 
  // Calculate names offset
  tbl  = 14;
  tbl += at->at_SubsongNr * 2;
  tbl += at->at_PositionNr * 8;
  tbl += (at->at_TrackNr+1) * at->at_TrackLength * 3;
  
  for( i=1; i<=at->at_InstrumentNr; i++ )
    tbl += 22 + at->at_Instruments[i].ins_PList.pls_Length*4;
  
  if( emptytrk != 0 ) tbl -= at->at_TrackLength * 3;
  
  tbf = IExec->AllocPooled( rp_mempool, tbl );
  if( !tbf )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return;
  }
  
  fh = fopen( name, "wb" );
  if( !fh )
  {
    IExec->FreePooled( rp_mempool, tbf, tbl );
    return;
  }
  
  strcpy( (TEXT *)tbf, "THX\1" );
  
  tbf[4]  =  (tbl >> 8)&0xff;
  tbf[5]  =  tbl&0xff;
  tbf[6]  =  (at->at_PositionNr>>8);
  tbf[6]  |= ((at->at_SpeedMultiplier-1)&3)<<5;
  tbf[6]  |= emptytrk;
  tbf[7]  =  at->at_PositionNr & 0xff;
  tbf[8]  =  at->at_Restart>>8;
  tbf[9]  =  at->at_Restart&0xff;
  tbf[10] =  at->at_TrackLength;
  tbf[11] =  at->at_TrackNr;
  tbf[12] =  at->at_InstrumentNr;
  tbf[13] =  at->at_SubsongNr;
  fwrite( tbf, 14, 1, fh );
  
  // Subsongs
  if( at->at_SubsongNr > 0 )
  {
    for( i=0, k=0; i<at->at_SubsongNr; i++ )
    {
      tbf[k++]     = at->at_Subsongs[i]>>8;
      tbf[k++] = at->at_Subsongs[i]&0xff;
    }
    fwrite( tbf, k, 1, fh );
  }
  
  // Position list
  for( i=0, k=0; i<at->at_PositionNr; i++ )
  {
    for( j=0; j<4; j++ )
    {
      tbf[k++] = at->at_Positions[i].pos_Track[j];
      tbf[k++] = at->at_Positions[i].pos_Transpose[j];
    }
  }
  fwrite( tbf, k, 1, fh );
  
  // Tracks
  for( i=0, k=0; i<=at->at_TrackNr; i++ )
  {
    if( ( emptytrk != 0 ) && ( i == 0 ) )
      continue;
    
    for( j=0; j<at->at_TrackLength; j++ )
    {
      tbf[k]    = (at->at_Tracks[i][j].stp_Note&0x3f)<<2;
      tbf[k++] |= (at->at_Tracks[i][j].stp_Instrument>>4)&0x3;
      tbf[k]    = (at->at_Tracks[i][j].stp_Instrument&0xf)<<4;
      
      if( ( at->at_Tracks[i][j].stp_FX != 0 ) ||
          ( at->at_Tracks[i][j].stp_FXParam != 0 ) )
      {
        tbf[k++] |= at->at_Tracks[i][j].stp_FX & 0xf;
        tbf[k++]  = at->at_Tracks[i][j].stp_FXParam;
      } else {
        tbf[k++] |= at->at_Tracks[i][j].stp_FXb & 0xf;
        tbf[k++]  = at->at_Tracks[i][j].stp_FXbParam;
      }      
    }
  }
  fwrite( tbf, k, 1, fh );

  // Instruments
  for( i=1; i<=at->at_InstrumentNr; i++ )
  {
    in = &at->at_Instruments[i];
    k = 0;
    tbf[0]   = in->ins_Volume;
    tbf[1]   = (in->ins_FilterSpeed&0x1f)<<3;
    tbf[1]  |= in->ins_WaveLength&0x7;
    tbf[2]   = in->ins_Envelope.aFrames;
    tbf[3]   = in->ins_Envelope.aVolume;
    tbf[4]   = in->ins_Envelope.dFrames;
    tbf[5]   = in->ins_Envelope.dVolume;
    tbf[6]   = in->ins_Envelope.sFrames;
    tbf[7]   = in->ins_Envelope.rFrames;
    tbf[8]   = in->ins_Envelope.rVolume;
    tbf[9]   = 0;
    tbf[10]  = 0;
    tbf[11]  = 0;
    tbf[12]  = (in->ins_FilterSpeed&0x20)<<2;
    tbf[12] |= in->ins_FilterLowerLimit&0x7f;
    tbf[13]  = in->ins_VibratoDelay;
    tbf[14]  = (in->ins_HardCutReleaseFrames&0x07)<<4;
    tbf[14] |= (in->ins_HardCutRelease&1)<<7;
    tbf[14] |= in->ins_VibratoDepth&0x0f;
    tbf[15]  = in->ins_VibratoSpeed;
    tbf[16]  = in->ins_SquareLowerLimit;
    tbf[17]  = in->ins_SquareUpperLimit;
    tbf[18]  = in->ins_SquareSpeed;
    tbf[19]  = in->ins_FilterUpperLimit&0x3f;
    tbf[20]  = in->ins_PList.pls_Speed;
    tbf[21]  = in->ins_PList.pls_Length;
    fwrite( tbf, 22, 1, fh );
     
    for( j=0, k=0; j<in->ins_PList.pls_Length; j++ )
    {
      l = in->ins_PList.pls_Entries[j].ple_FX[1];
      if( l == 12 ) l = 6;
      if( l == 15 ) l = 7;
      m = in->ins_PList.pls_Entries[j].ple_FX[0];
      if( m == 12 ) m = 6;
      if( m == 15 ) m = 7;
      
      tbf[k]    = (l&7)<<5;
      tbf[k]   |= (m&7)<<2;
      tbf[k++] |= (in->ins_PList.pls_Entries[j].ple_Waveform&6)>>1;
      tbf[k]    = (in->ins_PList.pls_Entries[j].ple_Waveform&1)<<7;
      tbf[k]   |= (in->ins_PList.pls_Entries[j].ple_Fixed&1)<<6;
      tbf[k++] |= in->ins_PList.pls_Entries[j].ple_Note&0x3f;
      tbf[k++]  = in->ins_PList.pls_Entries[j].ple_FXParam[0];
      tbf[k++]  = in->ins_PList.pls_Entries[j].ple_FXParam[1];
    }
    fwrite( tbf, k, 1, fh );  
  }
  
  fwrite( at->at_Name, strlen( at->at_Name )+1, 1, fh );
  for( i=1; i<=at->at_InstrumentNr; i++ )
    fwrite( at->at_Instruments[i].ins_Name, strlen( at->at_Instruments[i].ins_Name )+1, 1, fh );

  fclose( fh );
  IExec->FreePooled( rp_mempool, tbf, tbl );
}

struct ahx_tune *rp_load_ahx( struct ahx_tune *at, uint8 *buf, uint32 buflen, BOOL passed )
{
  const uint8  *bptr;
  const TEXT   *nptr;
  uint32  i, j, k, l;

  at->at_PositionNr      = ((buf[6]&0x0f)<<8)|buf[7];
  at->at_Restart         = (buf[8]<<8)|buf[9];
  at->at_SpeedMultiplier = ((buf[6]>>5)&3)+1;
  at->at_TrackLength     = buf[10];
  at->at_TrackNr         = buf[11];
  at->at_InstrumentNr    = buf[12];
  at->at_SubsongNr       = buf[13];
  
  if( at->at_Restart >= at->at_PositionNr )
    at->at_Restart = at->at_PositionNr-1;

  // Do some validation  
  if( ( at->at_PositionNr > 1000 ) ||
      ( at->at_TrackLength > 64 ) ||
      ( at->at_InstrumentNr > 64 ) )
  {
    printf( "%d,%d,%d\n", at->at_PositionNr,
                          at->at_TrackLength,
                          at->at_InstrumentNr );
    if( passed )
      rp_clear_tune( at );
    else
      rp_free_tune( at );
    IExec->FreePooled( rp_mempool, buf, buflen );
    printf( "Invalid file.\n" );
    return NULL;
  }

  strncpy( at->at_Name, (TEXT *)&buf[(buf[4]<<8)|buf[5]], 128 );
  nptr = (TEXT *)&buf[((buf[4]<<8)|buf[5])+strlen( at->at_Name )+1];

  bptr = &buf[14];
  
  // Subsongs
  for( i=0; i<at->at_SubsongNr; i++ )
  {
    at->at_Subsongs[i] = (bptr[0]<<8)|bptr[1];
    bptr += 2;
  }
  
  // Position list
  for( i=0; i<at->at_PositionNr; i++ )
  {
    for( j=0; j<4; j++ )
    {
      at->at_Positions[i].pos_Track[j]     = *bptr++;
      at->at_Positions[i].pos_Transpose[j] = *(int8 *)bptr++;
    }
  }
  
  // Tracks
  for( i=0; i<=at->at_TrackNr; i++ )
  {
    // rp_new_tune() clears it anyway
    if( ( ( buf[6]&0x80 ) == 0x80 ) && ( i == 0 ) )
      continue;
    
    for( j=0; j<at->at_TrackLength; j++ )
    {
      at->at_Tracks[i][j].stp_Note       = (bptr[0]>>2)&0x3f;
      at->at_Tracks[i][j].stp_Instrument = ((bptr[0]&0x3)<<4) | (bptr[1]>>4);
      at->at_Tracks[i][j].stp_FX         = bptr[1]&0xf;
      at->at_Tracks[i][j].stp_FXParam    = bptr[2];
      at->at_Tracks[i][j].stp_FXb        = 0;
      at->at_Tracks[i][j].stp_FXbParam   = 0;
      bptr += 3;
    }
  }
  
  // Instruments
  for( i=1; i<=at->at_InstrumentNr; i++ )
  {
    if( nptr < (TEXT *)(buf+buflen) )
    {
      strncpy( at->at_Instruments[i].ins_Name, nptr, 128 );
      nptr += strlen( nptr )+1;
    } else {
      at->at_Instruments[i].ins_Name[0] = 0;
    }
    
    at->at_Instruments[i].ins_Volume      = bptr[0];
    at->at_Instruments[i].ins_FilterSpeed = ((bptr[1]>>3)&0x1f)|((bptr[12]>>2)&0x20);
    at->at_Instruments[i].ins_WaveLength  = bptr[1]&0x07;

    at->at_Instruments[i].ins_Envelope.aFrames = bptr[2];
    at->at_Instruments[i].ins_Envelope.aVolume = bptr[3];
    at->at_Instruments[i].ins_Envelope.dFrames = bptr[4];
    at->at_Instruments[i].ins_Envelope.dVolume = bptr[5];
    at->at_Instruments[i].ins_Envelope.sFrames = bptr[6];
    at->at_Instruments[i].ins_Envelope.rFrames = bptr[7];
    at->at_Instruments[i].ins_Envelope.rVolume = bptr[8];

    at->at_Instruments[i].ins_FilterLowerLimit     = bptr[12]&0x7f;
    at->at_Instruments[i].ins_VibratoDelay         = bptr[13];
    at->at_Instruments[i].ins_HardCutReleaseFrames = (bptr[14]>>4)&0x07;
    at->at_Instruments[i].ins_HardCutRelease       = bptr[14]&0x80?1:0;
    at->at_Instruments[i].ins_VibratoDepth         = bptr[14]&0x0f;
    at->at_Instruments[i].ins_VibratoSpeed         = bptr[15];
    at->at_Instruments[i].ins_SquareLowerLimit     = bptr[16];
    at->at_Instruments[i].ins_SquareUpperLimit     = bptr[17];
    at->at_Instruments[i].ins_SquareSpeed          = bptr[18];
    at->at_Instruments[i].ins_FilterUpperLimit     = bptr[19]&0x3f;
    at->at_Instruments[i].ins_PList.pls_Speed      = bptr[20];
    at->at_Instruments[i].ins_PList.pls_Length     = bptr[21];

    bptr += 22;
    for( j=0; j<at->at_Instruments[i].ins_PList.pls_Length; j++ )
    {
      k = (bptr[0]>>5)&7;
      if( k == 6 ) k = 12;
      if( k == 7 ) k = 15;
      l = (bptr[0]>>2)&7;
      if( l == 6 ) l = 12;
      if( l == 7 ) l = 15;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1]      = k;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0]      = l;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform   = ((bptr[0]<<1)&6) | (bptr[1]>>7);
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed      = (bptr[1]>>6)&1;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Note       = bptr[1]&0x3f;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = bptr[2];
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = bptr[3];
      
      // 1.6: Strip "toggle filter" commands if the module is
      //      version 0 (pre-filters). This is what AHX also does.
      if( ( buf[3] == 0 ) && ( l == 4 ) && ( (bptr[2]&0xf0) != 0 ) )
        at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] &= 0x0f;
      if( ( buf[3] == 0 ) && ( k == 4 ) && ( (bptr[3]&0xf0) != 0 ) )
        at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] &= 0x0f;
      
      bptr += 4;
    }
    
    while( j < 256 )
    {
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Note = 0;
      j++;
    }
    at->at_Instruments[i].ins_ptop = 0;
    at->at_Instruments[i].ins_pcurx = 0;
    at->at_Instruments[i].ins_pcury = 0;
  }
  
  if( !passed )
  {
    IExec->ObtainSemaphore( rp_list_ss );
    IExec->AddTail( rp_tunelist, (struct Node *)at );
    IExec->ReleaseSemaphore( rp_list_ss );
  }

  rp_init_subsong( at, 0 );
  IExec->FreePooled( rp_mempool, buf, buflen );
  return at;
}

struct ahx_tune *rp_mod_import( struct ahx_tune *at, uint8 *buf, uint32 buflen, BOOL passed )
{ 
  int32 i, j, k, l;
  int32 cv_pos, cv_row, cv_chan;

  rp_clear_tune( at );
  
  if( strncmp( (TEXT *)&buf[1080], "M.K.", 4 ) != 0 )
  {
    if( passed )
      rp_clear_tune( at );
    else
      rp_free_tune( at );
    IExec->FreePooled( rp_mempool, buf, buflen );
    printf( "Invalid file.\n" );
    return NULL;
  }

  for( i=0; i<20; i++ )
  {
    if( buf[i] == 0 ) break;
    at->at_Name[i] = buf[i];
  }
  at->at_Name[i] = 0;
  
  for( k=1,j=20; k<32; k++,j+=30 )
  {
    for( i=0; i<22; i++ )
    {
      if( buf[j+i] == 0 ) break;
      at->at_Instruments[k].ins_Name[i] = buf[j+i];
    }
    at->at_Instruments[k].ins_Name[i] = 0;
    
    if( ((buf[j+22]<<8)|(buf[j+23])) != 0 )
    {
      at->at_Instruments[k].ins_Volume               = 64;
      at->at_Instruments[k].ins_WaveLength           = 5;
      at->at_Instruments[k].ins_FilterLowerLimit     = 1;
      at->at_Instruments[k].ins_FilterUpperLimit     = 31;
      at->at_Instruments[k].ins_FilterSpeed          = 0;
      at->at_Instruments[k].ins_SquareLowerLimit     = 32;
      at->at_Instruments[k].ins_SquareUpperLimit     = 63;
      at->at_Instruments[k].ins_SquareSpeed          = 1;
      at->at_Instruments[k].ins_VibratoDelay         = 0;
      at->at_Instruments[k].ins_VibratoDepth         = 0;
      at->at_Instruments[k].ins_VibratoSpeed         = 0;
      at->at_Instruments[k].ins_HardCutRelease       = 0;
      at->at_Instruments[k].ins_HardCutReleaseFrames = 0;
      at->at_Instruments[k].ins_Envelope.aFrames     = 1;
      at->at_Instruments[k].ins_Envelope.aVolume     = 64;
      at->at_Instruments[k].ins_Envelope.dFrames     = 1;
      at->at_Instruments[k].ins_Envelope.dVolume     = 64;
      at->at_Instruments[k].ins_Envelope.sFrames     = 1;
      at->at_Instruments[k].ins_Envelope.rFrames     = 1;
      at->at_Instruments[k].ins_Envelope.rVolume     = 64;
      at->at_Instruments[k].ins_Envelope.aFrames     = 1;
      at->at_Instruments[k].ins_PList.pls_Speed      = 1;
      at->at_Instruments[k].ins_PList.pls_Length     = 1;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_Note       = 1;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_Waveform   = 2;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_Fixed      = 0;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_FX[0]      = 0;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_FXParam[0] = 0;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_FX[1]      = 0;
      at->at_Instruments[k].ins_PList.pls_Entries[0].ple_FXParam[1] = 0;
    }
  }
  
  at->at_PositionNr = buf[950];
  k=0;
  for( i=0; i<128; i++ )
  {
    if( i < at->at_PositionNr )
    {
      at->at_Positions[i].pos_Track[0] = buf[i+952]*4;
      at->at_Positions[i].pos_Track[1] = buf[i+952]*4+1;
      at->at_Positions[i].pos_Track[2] = buf[i+952]*4+2;
      at->at_Positions[i].pos_Track[3] = buf[i+952]*4+3;
    }
    if( buf[i+952] > k )
      k = buf[i+952];
  }
  
  i = 1084;
  for( cv_pos=0; cv_pos<=k; cv_pos++ )
  {
    for( cv_row=0; cv_row<64; cv_row++ )
    {
      for( cv_chan=0; cv_chan<4; cv_chan++ )
      {
        l = ((buf[i]&0xf)<<8)|buf[i+1];
        for( j=0; j<37; j++ ) if( pt_import_period_tab[j] == l ) break;
        if( j < 37 )
          at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_Note = j;
        at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_Instrument = (buf[i]&0xf0)|((buf[i+2]>>4)&0xf);
        l = buf[i+2]&0xf;
        switch( l )
        {
          case 0x1:
          case 0x2:
          case 0x3:
          case 0x5:
          case 0xa:
          case 0xb:
          case 0xc:
          case 0xd:
            at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FX         = l;
            at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FXParam    = buf[i+3];
            break;
          
          case 0xe:
            switch( (buf[i+3]>>4)&0xf )
            {
              case 0x1:
              case 0x2:
              case 0xa:
              case 0xb:
              case 0xc:
              case 0xd:
                at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FX         = l;
                at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FXParam    = buf[i+3];
                break;
            }
            break;

          case 0xf:
            if( buf[i+3] >= 0x20 ) break;
            at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FX         = l;
            at->at_Tracks[cv_pos*4+cv_chan][cv_row].stp_FXParam    = buf[i+3];
            break;
        }

        i+=4;
      }
    }
  }

  if( !passed )
  {
    IExec->ObtainSemaphore( rp_list_ss );
    IExec->AddTail( rp_tunelist, (struct Node *)at );
    IExec->ReleaseSemaphore( rp_list_ss );
  }

  rp_init_subsong( at, 0 );
  IExec->FreePooled( rp_mempool, buf, buflen );
  return at;
}

struct ahx_tune *rp_load_tune( const TEXT *name, struct ahx_tune *at )
{
  uint8  *buf;
  const uint8  *bptr;
  const TEXT   *nptr;
  uint32  buflen, i, j;
  FILE   *fh;
  BOOL    passed;

  if( at )
  {
    rp_clear_tune( at );
    passed = TRUE;
  } else {
    at = rp_new_tune( FALSE );
    if( !at )
    {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
      return NULL;
    }
    passed = FALSE;
  }

  fh = fopen(name, "rb");
  if (!fh)
  {
    if( !passed  ) rp_free_tune( at );
    printf( "Can't open file\n" );
    return NULL;
  }

  fseek(fh, 0, SEEK_END);
  buflen = ftell(fh);
  fseek(fh, 0, SEEK_SET);

  buf = IExec->AllocPooled( rp_mempool, buflen );
  if( !buf )
  {
    if( !passed  ) rp_free_tune( at );
    fclose(fh);
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return NULL;
  }
  
  if( fread( buf, buflen, 1, fh ) != 1 )
  {
    if( !passed  ) rp_free_tune( at );
    fclose(fh);
    printf( "Unable to read from file!\n" );
    return NULL;
  }
  fclose(fh);

  if( ( buf[0] == 'T' ) &&
      ( buf[1] == 'H' ) &&
      ( buf[2] == 'X' ) &&
      ( buf[3] < 3 ) )
    return rp_load_ahx( at, buf, buflen, passed );

  if( ( buf[0] != 'H' ) ||
      ( buf[1] != 'V' ) ||
      ( buf[2] != 'L' ) ||
      ( buf[3] > 1 ) )
    return rp_mod_import( at, buf, buflen, passed );

  at->at_PositionNr      = ((buf[6]&0x0f)<<8)|buf[7];
  at->at_Channels        = (buf[8]>>2)+4;
  at->at_Restart         = ((buf[8]&3)<<8)|buf[9];
  at->at_SpeedMultiplier = ((buf[6]>>5)&3)+1;
  at->at_TrackLength     = buf[10];
  at->at_TrackNr         = buf[11];
  at->at_InstrumentNr    = buf[12];
  at->at_SubsongNr       = buf[13];
  at->at_mixgainP        = buf[14];
  at->at_mixgain         = (at->at_mixgainP<<8)/100;
  at->at_defstereo       = buf[15];
  at->at_defpanleft      = stereopan_left[at->at_defstereo];
  at->at_defpanright     = stereopan_right[at->at_defstereo];
  
  if( at->at_Restart >= at->at_PositionNr )
    at->at_Restart = at->at_PositionNr-1;

  // Do some validation  
  if( ( at->at_PositionNr > 1000 ) ||
      ( at->at_TrackLength > 64 ) ||
      ( at->at_InstrumentNr > 64 ) )
  {
    printf( "%d,%d,%d\n", at->at_PositionNr,
                          at->at_TrackLength,
                          at->at_InstrumentNr );
    if( passed )
      rp_clear_tune( at );
    else
      rp_free_tune( at );
    IExec->FreePooled( rp_mempool, buf, buflen );
    printf( "Invalid file.\n" );
    return NULL;
  }

  strncpy( at->at_Name, (TEXT *)&buf[(buf[4]<<8)|buf[5]], 128 );
  nptr = (TEXT *)&buf[((buf[4]<<8)|buf[5])+strlen( at->at_Name )+1];

  bptr = &buf[16];
  
  // Subsongs
  for( i=0; i<at->at_SubsongNr; i++ )
  {
    at->at_Subsongs[i] = (bptr[0]<<8)|bptr[1];
    bptr += 2;
  }
  
  // Position list
  for( i=0; i<at->at_PositionNr; i++ )
  {
    for( j=0; j<at->at_Channels; j++ )
    {
      if( i < 1000 )
      {
        at->at_Positions[i].pos_Track[j]     = *bptr++;
        at->at_Positions[i].pos_Transpose[j] = *(int8 *)bptr++;
      } else {
        bptr++;
        bptr++;
      }
    }
  }
  
  // Tracks
  for( i=0; i<=at->at_TrackNr; i++ )
  {
    // rp_new_tune() clears it anyway
    if( ( ( buf[6]&0x80 ) == 0x80 ) && ( i == 0 ) )
      continue;

    for( j=0; j<at->at_TrackLength; j++ )
    {
      if( bptr[0] == 0x3f )
      {
        at->at_Tracks[i][j].stp_Note       = 0;
        at->at_Tracks[i][j].stp_Instrument = 0;
        at->at_Tracks[i][j].stp_FX         = 0;
        at->at_Tracks[i][j].stp_FXParam    = 0;
        at->at_Tracks[i][j].stp_FXb        = 0;
        at->at_Tracks[i][j].stp_FXbParam   = 0;
        bptr++;
        continue;
      }
      
      at->at_Tracks[i][j].stp_Note       = bptr[0];
      at->at_Tracks[i][j].stp_Instrument = bptr[1];
      at->at_Tracks[i][j].stp_FX         = bptr[2]>>4;
      at->at_Tracks[i][j].stp_FXParam    = bptr[3];
      at->at_Tracks[i][j].stp_FXb        = bptr[2]&0xf;
      at->at_Tracks[i][j].stp_FXbParam   = bptr[4];
      
      // Strip out EFx commands from older modules
      // since they did nothing until HVL1
      if( buf[3] < 1 )
      {
        if( ((at->at_Tracks[i][j].stp_FX==0xe)&&((at->at_Tracks[i][j].stp_FXParam&0xf0)==0xf0)) ||
            ((at->at_Tracks[i][j].stp_FXb==0xe)&&((at->at_Tracks[i][j].stp_FXParam&0xf0)==0xf0)) )
        {
          at->at_Tracks[i][j].stp_FX = 0;
          at->at_Tracks[i][j].stp_FXParam = 0;
        }          
      }

      bptr += 5;
    }
  }

  // Instruments
  for( i=1; i<=at->at_InstrumentNr; i++ )
  {
    if( nptr < (TEXT *)(buf+buflen) )
    {
      strncpy( at->at_Instruments[i].ins_Name, nptr, 128 );
      nptr += strlen( nptr )+1;
    } else {
      at->at_Instruments[i].ins_Name[0] = 0;
    }
    
    at->at_Instruments[i].ins_Volume      = bptr[0];
    at->at_Instruments[i].ins_FilterSpeed = ((bptr[1]>>3)&0x1f)|((bptr[12]>>2)&0x20);
    at->at_Instruments[i].ins_WaveLength  = bptr[1]&0x07;

    at->at_Instruments[i].ins_Envelope.aFrames = bptr[2];
    at->at_Instruments[i].ins_Envelope.aVolume = bptr[3];
    at->at_Instruments[i].ins_Envelope.dFrames = bptr[4];
    at->at_Instruments[i].ins_Envelope.dVolume = bptr[5];
    at->at_Instruments[i].ins_Envelope.sFrames = bptr[6];
    at->at_Instruments[i].ins_Envelope.rFrames = bptr[7];
    at->at_Instruments[i].ins_Envelope.rVolume = bptr[8];
    
    at->at_Instruments[i].ins_FilterLowerLimit     = bptr[12]&0x7f;
    at->at_Instruments[i].ins_VibratoDelay         = bptr[13];
    at->at_Instruments[i].ins_HardCutReleaseFrames = (bptr[14]>>4)&0x07;
    at->at_Instruments[i].ins_HardCutRelease       = bptr[14]&0x80?1:0;
    at->at_Instruments[i].ins_VibratoDepth         = bptr[14]&0x0f;
    at->at_Instruments[i].ins_VibratoSpeed         = bptr[15];
    at->at_Instruments[i].ins_SquareLowerLimit     = bptr[16];
    at->at_Instruments[i].ins_SquareUpperLimit     = bptr[17];
    at->at_Instruments[i].ins_SquareSpeed          = bptr[18];
    at->at_Instruments[i].ins_FilterUpperLimit     = bptr[19]&0x3f;
    at->at_Instruments[i].ins_PList.pls_Speed      = bptr[20];
    at->at_Instruments[i].ins_PList.pls_Length     = bptr[21];
    
    bptr += 22;
    for( j=0; j<at->at_Instruments[i].ins_PList.pls_Length; j++ )
    {
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0] = bptr[0]&0xf;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1] = (bptr[1]>>3)&0xf;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform = bptr[1]&7;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed = (bptr[2]>>6)&1;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Note  = bptr[2]&0x3f;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = bptr[3];
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = bptr[4];
      bptr += 5;
    }
    
    while( j < 256 )
    {
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[0] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FX[1] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[0] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_FXParam[1] = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Waveform = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Fixed = 0;
      at->at_Instruments[i].ins_PList.pls_Entries[j].ple_Note = 0;
      j++;
    }
    at->at_Instruments[i].ins_ptop = 0;
    at->at_Instruments[i].ins_pcurx = 0;
    at->at_Instruments[i].ins_pcury = 0;
  }
  
  if( !passed )
  {
    IExec->ObtainSemaphore( rp_list_ss );
    IExec->AddTail( rp_tunelist, (struct Node *)at );
    IExec->ReleaseSemaphore( rp_list_ss );
  }

  rp_init_subsong( at, 0 );
  IExec->FreePooled( rp_mempool, buf, buflen );
  return at;
}

void rp_process_stepfx_1( struct ahx_tune *at, struct ahx_voice *voice, int32 FX, int32 FXParam )
{
  switch( FX )
  {
    case 0x0:  // Position Jump HI
      if( ((FXParam&0x0f) > 0) && ((FXParam&0x0f) <= 9) )
        at->at_PosJump = FXParam & 0xf;
      break;

    case 0x5:  // Volume Slide + Tone Portamento
    case 0xa:  // Volume Slide
      voice->vc_VolumeSlideDown = FXParam & 0x0f;
      voice->vc_VolumeSlideUp   = FXParam >> 4;
      break;
    
    case 0x7:  // Panning
      if( FXParam > 127 )
        FXParam -= 256;
      voice->vc_Pan          = (FXParam+128);
      voice->vc_SetPan       = (FXParam+128);
      voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
      voice->vc_PanMultRight = panning_right[voice->vc_Pan];
      break;
    
    case 0xb: // Position jump
      at->at_PosJump      = at->at_PosJump*100 + (FXParam & 0x0f) + (FXParam >> 4)*10;
      at->at_PatternBreak = 1;
      if( at->at_PosJump <= at->at_PosNr )
        at->at_SongEndReached = 1;
      break;
    
    case 0xd: // Pattern break
      at->at_PosJump      = at->at_PosNr+1;
      at->at_PosJumpNote  = (FXParam & 0x0f) + (FXParam>>4)*10;
      at->at_PatternBreak = 1;
      if( at->at_PosJumpNote >  at->at_TrackLength )
        at->at_PosJumpNote = 0;
      break;
    
    case 0xe: // Extended commands
      switch( FXParam >> 4 )
      {
        case 0xc: // Note cut
          if( (FXParam & 0x0f) < at->at_Tempo )
          {
            voice->vc_NoteCutWait = FXParam & 0x0f;
            if( voice->vc_NoteCutWait )
            {
              voice->vc_NoteCutOn      = 1;
              voice->vc_HardCutRelease = 0;
            }
          }
          break;

        // 1.6: case for 0xd removed
      }
      break;
    
    case 0xf: // Speed
      at->at_Tempo = FXParam;
      if( FXParam == 0 )
        at->at_SongEndReached = 1;
      break;
  }  
}

void rp_process_stepfx_2( struct ahx_tune *at, struct ahx_voice *voice, int32 FX, int32 FXParam, int32 *Note )
{
  switch( FX )
  {
    case 0x9: // Set squarewave offset
      voice->vc_SquarePos    = FXParam >> (5 - voice->vc_WaveLength);
//      voice->vc_PlantSquare  = 1;
      voice->vc_IgnoreSquare = 1;
      break;
    
    case 0x3: // Tone portamento
      if( FXParam != 0 ) voice->vc_PeriodSlideSpeed = FXParam;
    case 0x5: // Tone portamento + volume slide
      
      if( *Note )
      {
        int32 new, diff;

        new   = period_tab[*Note];
        diff  = period_tab[voice->vc_TrackPeriod];
        diff -= new;
        new   = diff + voice->vc_PeriodSlidePeriod;
        
        if( new )
          voice->vc_PeriodSlideLimit = -diff;
      }
      voice->vc_PeriodSlideOn        = 1;
      voice->vc_PeriodSlideWithLimit = 1;
      *Note = 0;
      break;      
  } 
}

void rp_process_stepfx_3( struct ahx_tune *at, struct ahx_voice *voice, int32 FX, int32 FXParam )
{
  int32 i;
  
  switch( FX )
  {
    case 0x01: // Portamento up (period slide down)
      voice->vc_PeriodSlideSpeed     = -FXParam;
      voice->vc_PeriodSlideOn        = 1;
      voice->vc_PeriodSlideWithLimit = 0;
      break;
    case 0x02: // Portamento down
      voice->vc_PeriodSlideSpeed     = FXParam;
      voice->vc_PeriodSlideOn        = 1;
      voice->vc_PeriodSlideWithLimit = 0;
      break;
    case 0x04: // Filter override
      if( ( FXParam == 0 ) || ( FXParam == 0x40 ) ) break;
      if( FXParam < 0x40 )
      {
        voice->vc_IgnoreFilter = FXParam;
        break;
      }
      if( FXParam > 0x7f ) break;
      voice->vc_FilterPos = FXParam - 0x40;
      break;
    case 0x0c: // Volume
      FXParam &= 0xff;
      if( FXParam <= 0x40 )
      {
        voice->vc_NoteMaxVolume = FXParam;
        break;
      }
      
      if( (FXParam -= 0x50) < 0 ) break; // 1.6

      if( FXParam <= 0x40 )
      {
        for( i=0; i<at->at_Channels; i++ )
          at->at_Voices[i].vc_TrackMasterVolume = FXParam;
        break;
      }
      
      if( (FXParam -= 0xa0-0x50) < 0 ) break; // 1.6

      if( FXParam <= 0x40 )
        voice->vc_TrackMasterVolume = FXParam;
      break;

    case 0xe: // Extended commands;
      switch( FXParam >> 4 )
      {
        case 0x1: // Fineslide up
          voice->vc_PeriodSlidePeriod -= (FXParam & 0x0f);
          voice->vc_PlantPeriod = 1;
          break;
        
        case 0x2: // Fineslide down
          voice->vc_PeriodSlidePeriod += (FXParam & 0x0f);
          voice->vc_PlantPeriod = 1;
          break;
        
        case 0x4: // Vibrato control
          voice->vc_VibratoDepth = FXParam & 0x0f;
          break;
        
        case 0x0a: // Fine volume up
          voice->vc_NoteMaxVolume += FXParam & 0x0f;
          
          if( voice->vc_NoteMaxVolume > 0x40 )
            voice->vc_NoteMaxVolume = 0x40;
          break;
        
        case 0x0b: // Fine volume down
          voice->vc_NoteMaxVolume -= FXParam & 0x0f;
          
          if( voice->vc_NoteMaxVolume < 0 )
            voice->vc_NoteMaxVolume = 0;
          break;
        
        case 0x0f: // Miscellaneous (1.5+)
          switch( FXParam & 0x0f )
          {
            case 1:  // Preserve track transpose (1.5+)
              voice->vc_OverrideTranspose = voice->vc_Transpose;
              break;
          }
          break;
      } 
      break;
  }
}

void rp_play_instrument( struct ahx_tune *at, int32 vcnum, int32 instr, int32 period )
{
  struct ahx_voice *voice;
  struct ahx_instrument *Ins;
  int16  SquareLower, SquareUpper, d6, d3, d4;

  if( at->at_Instruments[instr].ins_PList.pls_Length == 0 )
    return;
  
  if( period == 0 ) return;
  if( vcnum >= at->at_Channels ) return;
  
  voice = &at->at_Voices[vcnum];
  Ins   = &at->at_Instruments[instr];

  voice->vc_Pan          = voice->vc_SetPan;
  voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
  voice->vc_PanMultRight = panning_right[voice->vc_Pan];

  voice->vc_TrackOn          = voice->vc_SetTrackOn;  
  voice->vc_Transpose        = 0;
  voice->vc_OverrideTranspose = 1000;  // 1.5

  voice->vc_PeriodSlideSpeed = voice->vc_PeriodSlidePeriod = voice->vc_PeriodSlideLimit = 0;

  voice->vc_PerfSubVolume    = 0x40;
  voice->vc_ADSRVolume       = 0;
  voice->vc_Instrument       = Ins;
  voice->vc_SamplePos        = 0;
    
  voice->vc_ADSR.aFrames     = Ins->ins_Envelope.aFrames;
  voice->vc_ADSR.aVolume     = Ins->ins_Envelope.aVolume*256/voice->vc_ADSR.aFrames;
  voice->vc_ADSR.dFrames     = Ins->ins_Envelope.dFrames;
  voice->vc_ADSR.dVolume     = (Ins->ins_Envelope.dVolume-Ins->ins_Envelope.aVolume)*256/voice->vc_ADSR.dFrames;
  voice->vc_ADSR.sFrames     = Ins->ins_Envelope.sFrames;
  voice->vc_ADSR.rFrames     = Ins->ins_Envelope.rFrames;
  voice->vc_ADSR.rVolume     = (Ins->ins_Envelope.rVolume-Ins->ins_Envelope.dVolume)*256/voice->vc_ADSR.rFrames;
    
  voice->vc_WaveLength       = Ins->ins_WaveLength;
  voice->vc_NoteMaxVolume    = Ins->ins_Volume;
    
  voice->vc_VibratoCurrent   = 0;
  voice->vc_VibratoDelay     = Ins->ins_VibratoDelay;
  voice->vc_VibratoDepth     = Ins->ins_VibratoDepth;
  voice->vc_VibratoSpeed     = Ins->ins_VibratoSpeed;
  voice->vc_VibratoPeriod    = 0;
    
  voice->vc_HardCutRelease   = Ins->ins_HardCutRelease;
  voice->vc_HardCut          = Ins->ins_HardCutReleaseFrames;
    
  voice->vc_IgnoreSquare = voice->vc_SquareSlidingIn = 0;
  voice->vc_SquareWait   = voice->vc_SquareOn        = 0;
    
  SquareLower = Ins->ins_SquareLowerLimit >> (5 - voice->vc_WaveLength);
  SquareUpper = Ins->ins_SquareUpperLimit >> (5 - voice->vc_WaveLength);
    
  if( SquareUpper < SquareLower )
  {
    int16 t = SquareUpper;
    SquareUpper = SquareLower;
    SquareLower = t;
  }
    
  voice->vc_SquareUpperLimit = SquareUpper;
  voice->vc_SquareLowerLimit = SquareLower;
  
  voice->vc_IgnoreFilter    = voice->vc_FilterWait = voice->vc_FilterOn = 0;
  voice->vc_FilterSlidingIn = 0;

  d6 = Ins->ins_FilterSpeed;
  d3 = Ins->ins_FilterLowerLimit;
  d4 = Ins->ins_FilterUpperLimit;
   
  if( d3 & 0x80 ) d6 |= 0x20;
  if( d4 & 0x80 ) d6 |= 0x40;
    
  voice->vc_FilterSpeed = d6;
  d3 &= ~0x80;
  d4 &= ~0x80;
    
  if( d3 > d4 )
  {
    int16 t = d3;
    d3 = d4;
    d4 = t;
  }
    
  voice->vc_FilterUpperLimit = d4;
  voice->vc_FilterLowerLimit = d3;
  voice->vc_FilterPos        = 32;
  
  voice->vc_PerfWait  = voice->vc_PerfCurrent = 0;
  voice->vc_PerfSpeed = Ins->ins_PList.pls_Speed;
  voice->vc_PerfList  = &voice->vc_Instrument->ins_PList;

  voice->vc_TrackPeriod = period;
  voice->vc_InstrPeriod = period;
  voice->vc_PlantPeriod = 1;

  voice->vc_RingMixSource   = NULL;   // No ring modulation
  voice->vc_RingSamplePos   = 0;
  voice->vc_RingPlantPeriod = 0;
  voice->vc_RingNewWaveform = 0;
}

void rp_process_step( struct ahx_tune *at, struct ahx_voice *voice )
{
  int32  Note, Instr, donenotedel;
  struct ahx_step *Step;
  
  if( voice->vc_TrackOn == 0 )
    return;
  
  if( ( rp_state == STS_PLAYNOTE ) ||
      ( rp_state == STS_PLAYROW ) ) return;

  voice->vc_VolumeSlideUp = voice->vc_VolumeSlideDown = 0;
  
  Step = &at->at_Tracks[at->at_Positions[at->at_PosNr].pos_Track[voice->vc_VoiceNum]][at->at_NoteNr];
  
  Note    = Step->stp_Note;
  Instr   = Step->stp_Instrument;

  // --------- 1.6: from here --------------
  
  donenotedel = 0;

  // Do notedelay here
  if( ((Step->stp_FX&0xf)==0xe) && ((Step->stp_FXParam&0xf0)==0xd0) )
  {
    if( voice->vc_NoteDelayOn )
    {
      voice->vc_NoteDelayOn = 0;
      donenotedel = 1;
    } else {
      if( (Step->stp_FXParam&0x0f) < at->at_Tempo )
      {
        voice->vc_NoteDelayWait = Step->stp_FXParam & 0x0f;
        if( voice->vc_NoteDelayWait )
        {
          voice->vc_NoteDelayOn = 1;
          return;
        }
      }
    }
  }

  if( (donenotedel==0) && ((Step->stp_FXb&0xf)==0xe) && ((Step->stp_FXbParam&0xf0)==0xd0) )
  {
    if( voice->vc_NoteDelayOn )
    {
      voice->vc_NoteDelayOn = 0;
    } else {
      if( (Step->stp_FXbParam&0x0f) < at->at_Tempo )
      {
        voice->vc_NoteDelayWait = Step->stp_FXbParam & 0x0f;
        if( voice->vc_NoteDelayWait )
        {
          voice->vc_NoteDelayOn = 1;
          return;
        }
      }
    }
  }
  
  // ---------- 1.6: to here -------------

  if( Note ) voice->vc_OverrideTranspose = 1000; // 1.5

  rp_process_stepfx_1( at, voice, Step->stp_FX&0xf,  Step->stp_FXParam );  
  rp_process_stepfx_1( at, voice, Step->stp_FXb&0xf, Step->stp_FXbParam );
  
  if( Instr )
  {
    struct ahx_instrument *Ins;
    int16  SquareLower, SquareUpper, d6, d3, d4;
    
    /* 1.4: Reset panning to last set position */
    voice->vc_Pan          = voice->vc_SetPan;
    voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
    voice->vc_PanMultRight = panning_right[voice->vc_Pan];

    voice->vc_PeriodSlideSpeed = voice->vc_PeriodSlidePeriod = voice->vc_PeriodSlideLimit = 0;

    voice->vc_PerfSubVolume    = 0x40;
    voice->vc_ADSRVolume       = 0;
    voice->vc_Instrument       = Ins = &at->at_Instruments[Instr];
    voice->vc_SamplePos        = 0;
    
    voice->vc_ADSR.aFrames     = Ins->ins_Envelope.aFrames;
	voice->vc_ADSR.aVolume     = voice->vc_ADSR.aFrames ? Ins->ins_Envelope.aVolume*256/voice->vc_ADSR.aFrames : Ins->ins_Envelope.aVolume * 256; // XXX
    voice->vc_ADSR.dFrames     = Ins->ins_Envelope.dFrames;
	voice->vc_ADSR.dVolume     = voice->vc_ADSR.dFrames ? (Ins->ins_Envelope.dVolume-Ins->ins_Envelope.aVolume)*256/voice->vc_ADSR.dFrames : Ins->ins_Envelope.dVolume * 256; // XXX
    voice->vc_ADSR.sFrames     = Ins->ins_Envelope.sFrames;
    voice->vc_ADSR.rFrames     = Ins->ins_Envelope.rFrames;
	voice->vc_ADSR.rVolume     = voice->vc_ADSR.rFrames ? (Ins->ins_Envelope.rVolume-Ins->ins_Envelope.dVolume)*256/voice->vc_ADSR.rFrames : Ins->ins_Envelope.rVolume * 256; // XXX
    
    voice->vc_WaveLength       = Ins->ins_WaveLength;
    voice->vc_NoteMaxVolume    = Ins->ins_Volume;
    
    voice->vc_VibratoCurrent   = 0;
    voice->vc_VibratoDelay     = Ins->ins_VibratoDelay;
    voice->vc_VibratoDepth     = Ins->ins_VibratoDepth;
    voice->vc_VibratoSpeed     = Ins->ins_VibratoSpeed;
    voice->vc_VibratoPeriod    = 0;
    
    voice->vc_HardCutRelease   = Ins->ins_HardCutRelease;
    voice->vc_HardCut          = Ins->ins_HardCutReleaseFrames;
    
    voice->vc_IgnoreSquare = voice->vc_SquareSlidingIn = 0;
    voice->vc_SquareWait   = voice->vc_SquareOn        = 0;
    
    SquareLower = Ins->ins_SquareLowerLimit >> (5 - voice->vc_WaveLength);
    SquareUpper = Ins->ins_SquareUpperLimit >> (5 - voice->vc_WaveLength);
    
    if( SquareUpper < SquareLower )
    {
      int16 t = SquareUpper;
      SquareUpper = SquareLower;
      SquareLower = t;
    }
    
    voice->vc_SquareUpperLimit = SquareUpper;
    voice->vc_SquareLowerLimit = SquareLower;
    
    voice->vc_IgnoreFilter    = voice->vc_FilterWait = voice->vc_FilterOn = 0;
    voice->vc_FilterSlidingIn = 0;

    d6 = Ins->ins_FilterSpeed;
    d3 = Ins->ins_FilterLowerLimit;
    d4 = Ins->ins_FilterUpperLimit;
    
    if( d3 & 0x80 ) d6 |= 0x20;
    if( d4 & 0x80 ) d6 |= 0x40;
    
    voice->vc_FilterSpeed = d6;
    d3 &= ~0x80;
    d4 &= ~0x80;
    
    if( d3 > d4 )
    {
      int16 t = d3;
      d3 = d4;
      d4 = t;
    }
    
    voice->vc_FilterUpperLimit = d4;
    voice->vc_FilterLowerLimit = d3;
    voice->vc_FilterPos        = 32;
    
    voice->vc_PerfWait  = voice->vc_PerfCurrent = 0;
    voice->vc_PerfSpeed = Ins->ins_PList.pls_Speed;
    voice->vc_PerfList  = &voice->vc_Instrument->ins_PList;
    
    voice->vc_RingMixSource   = NULL;   // No ring modulation
    voice->vc_RingSamplePos   = 0;
    voice->vc_RingPlantPeriod = 0;
    voice->vc_RingNewWaveform = 0;
  }
  
  voice->vc_PeriodSlideOn = 0;
  
  rp_process_stepfx_2( at, voice, Step->stp_FX&0xf,  Step->stp_FXParam,  &Note );  
  rp_process_stepfx_2( at, voice, Step->stp_FXb&0xf, Step->stp_FXbParam, &Note );

  if( Note )
  {
    voice->vc_TrackPeriod = Note;
    voice->vc_PlantPeriod = 1;
  }
  
  rp_process_stepfx_3( at, voice, Step->stp_FX&0xf,  Step->stp_FXParam );  
  rp_process_stepfx_3( at, voice, Step->stp_FXb&0xf, Step->stp_FXbParam );  
}

void rp_plist_command_parse( struct ahx_tune *at, struct ahx_voice *voice, int32 FX, int32 FXParam )
{
  switch( FX )
  {
    case 0:
      if( ( FXParam > 0 ) && ( FXParam < 0x40 ) )
      {
        if( voice->vc_IgnoreFilter )
        {
          voice->vc_FilterPos    = voice->vc_IgnoreFilter;
          voice->vc_IgnoreFilter = 0;
        } else {
          voice->vc_FilterPos    = FXParam;
        }
        voice->vc_NewWaveform = 1;
      }
      break;

    case 1:
      voice->vc_PeriodPerfSlideSpeed = FXParam;
      voice->vc_PeriodPerfSlideOn    = 1;
      break;

    case 2:
      voice->vc_PeriodPerfSlideSpeed = -FXParam;
      voice->vc_PeriodPerfSlideOn    = 1;
      break;
    
    case 3:
      if( voice->vc_IgnoreSquare == 0 )
        voice->vc_SquarePos = FXParam >> (5-voice->vc_WaveLength);
      else
        voice->vc_IgnoreSquare = 0;
      break;
    
    case 4:
      if( FXParam == 0 )
      {
        voice->vc_SquareInit = (voice->vc_SquareOn ^= 1);
        voice->vc_SquareSign = 1;
      } else {

        if( FXParam & 0x0f )
        {
          voice->vc_SquareInit = (voice->vc_SquareOn ^= 1);
          voice->vc_SquareSign = 1;
          if(( FXParam & 0x0f ) == 0x0f )
            voice->vc_SquareSign = -1;
        }
        
        if( FXParam & 0xf0 )
        {
          voice->vc_FilterInit = (voice->vc_FilterOn ^= 1);
          voice->vc_FilterSign = 1;
          if(( FXParam & 0xf0 ) == 0xf0 )
            voice->vc_FilterSign = -1;
        }
      }
      break;
    
    case 5:
      voice->vc_PerfCurrent = FXParam;
      break;
    
    case 7:
      // Ring modulate with triangle
      if(( FXParam >= 1 ) && ( FXParam <= 0x3C ))
      {
        voice->vc_RingBasePeriod = FXParam;
        voice->vc_RingFixedPeriod = 1;
      } else if(( FXParam >= 0x81 ) && ( FXParam <= 0xBC )) {
        voice->vc_RingBasePeriod = FXParam-0x80;
        voice->vc_RingFixedPeriod = 0;
      } else {
        voice->vc_RingBasePeriod = 0;
        voice->vc_RingFixedPeriod = 0;
        voice->vc_RingNewWaveform = 0;
        voice->vc_RingAudioSource = NULL; // turn it off
        voice->vc_RingMixSource   = NULL;
        break;
      }    
      voice->vc_RingWaveform    = 0;
      voice->vc_RingNewWaveform = 1;
      voice->vc_RingPlantPeriod = 1;
      break;
    
    case 8:  // Ring modulate with sawtooth
      if(( FXParam >= 1 ) && ( FXParam <= 0x3C ))
      {
        voice->vc_RingBasePeriod = FXParam;
        voice->vc_RingFixedPeriod = 1;
      } else if(( FXParam >= 0x81 ) && ( FXParam <= 0xBC )) {
        voice->vc_RingBasePeriod = FXParam-0x80;
        voice->vc_RingFixedPeriod = 0;
      } else {
        voice->vc_RingBasePeriod = 0;
        voice->vc_RingFixedPeriod = 0;
        voice->vc_RingNewWaveform = 0;
        voice->vc_RingAudioSource = NULL;
        voice->vc_RingMixSource   = NULL;
        break;
      }

      voice->vc_RingWaveform    = 1;
      voice->vc_RingNewWaveform = 1;
      voice->vc_RingPlantPeriod = 1;
      break;

    case 9:    
      if( FXParam > 127 )
        FXParam -= 256;
      voice->vc_Pan          = (FXParam+128);
      voice->vc_PanMultLeft  = panning_left[voice->vc_Pan];
      voice->vc_PanMultRight = panning_right[voice->vc_Pan];
      break;

    case 12:
      if( FXParam <= 0x40 )
      {
        voice->vc_NoteMaxVolume = FXParam;
        break;
      }
      
      if( (FXParam -= 0x50) < 0 ) break;

      if( FXParam <= 0x40 )
      {
        voice->vc_PerfSubVolume = FXParam;
        break;
      }
      
      if( (FXParam -= 0xa0-0x50) < 0 ) break;
      
      if( FXParam <= 0x40 )
        voice->vc_TrackMasterVolume = FXParam;
      break;
    
    case 15:
      voice->vc_PerfSpeed = voice->vc_PerfWait = FXParam;
      break;
  } 
}

void rp_process_frame( struct ahx_tune *at, struct ahx_voice *voice )
{
  static CONST uint8 Offsets[] = {0x00,0x04,0x04+0x08,0x04+0x08+0x10,0x04+0x08+0x10+0x20,0x04+0x08+0x10+0x20+0x40};

  if( voice->vc_TrackOn == 0 )
    return;

  if( rp_state != STS_PLAYNOTE )
  {  
    if( voice->vc_NoteDelayOn )
    {
      if( voice->vc_NoteDelayWait <= 0 )
        rp_process_step( at, voice );
      else
        voice->vc_NoteDelayWait--;
    }
  
    if( voice->vc_HardCut )
    {
      int32 nextinst;
    
      if( at->at_NoteNr+1 < at->at_TrackLength )
        nextinst = at->at_Tracks[voice->vc_Track][at->at_NoteNr+1].stp_Instrument;
      else
        nextinst = at->at_Tracks[voice->vc_NextTrack][0].stp_Instrument;
    
      if( nextinst )
      {
        int32 d1;
      
        d1 = at->at_Tempo - voice->vc_HardCut;
      
        if( d1 < 0 ) d1 = 0;
      
        if( !voice->vc_NoteCutOn )
        {
          voice->vc_NoteCutOn       = 1;
          voice->vc_NoteCutWait     = d1;
          voice->vc_HardCutReleaseF = -(d1-at->at_Tempo);
        } else {
          voice->vc_HardCut = 0;
        }
      }
    }
    
    if( voice->vc_NoteCutOn )
    {
      if( voice->vc_NoteCutWait <= 0 )
      {
        voice->vc_NoteCutOn = 0;
        
        if( voice->vc_HardCutRelease )
        {
          voice->vc_ADSR.rVolume = -(voice->vc_ADSRVolume - (voice->vc_Instrument->ins_Envelope.rVolume << 8)) / voice->vc_HardCutReleaseF;
          voice->vc_ADSR.rFrames = voice->vc_HardCutReleaseF;
          voice->vc_ADSR.aFrames = voice->vc_ADSR.dFrames = voice->vc_ADSR.sFrames = 0;
        } else {
          voice->vc_NoteMaxVolume = 0;
        }
      } else {
        voice->vc_NoteCutWait--;
      }
    }
  }
    
  // ADSR envelope
  if( voice->vc_ADSR.aFrames )
  {
    voice->vc_ADSRVolume += voice->vc_ADSR.aVolume;
      
    if( --voice->vc_ADSR.aFrames <= 0 )
      voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.aVolume << 8;

  } else if( voice->vc_ADSR.dFrames ) {
    
    voice->vc_ADSRVolume += voice->vc_ADSR.dVolume;
      
    if( --voice->vc_ADSR.dFrames <= 0 )
      voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.dVolume << 8;
    
  } else if( voice->vc_ADSR.sFrames ) {
    
    voice->vc_ADSR.sFrames--;
    
  } else if( voice->vc_ADSR.rFrames ) {
    
    voice->vc_ADSRVolume += voice->vc_ADSR.rVolume;
    
    if( --voice->vc_ADSR.rFrames <= 0 )
      voice->vc_ADSRVolume = voice->vc_Instrument->ins_Envelope.rVolume << 8;
  }

  if( ( rp_state != STS_PLAYNOTE ) &&
      ( rp_state != STS_PLAYROW ) )
  {
    // VolumeSlide
    voice->vc_NoteMaxVolume = voice->vc_NoteMaxVolume + voice->vc_VolumeSlideUp - voice->vc_VolumeSlideDown;

    if( voice->vc_NoteMaxVolume < 0 )
      voice->vc_NoteMaxVolume = 0;
    else if( voice->vc_NoteMaxVolume > 0x40 )
      voice->vc_NoteMaxVolume = 0x40;

    // Portamento
    if( voice->vc_PeriodSlideOn )
    {
      if( voice->vc_PeriodSlideWithLimit )
      {
        int32  d0, d2;
      
        d0 = voice->vc_PeriodSlidePeriod - voice->vc_PeriodSlideLimit;
        d2 = voice->vc_PeriodSlideSpeed;
      
        if( d0 > 0 )
          d2 = -d2;
      
        if( d0 )
        {
          int32 d3;
         
          d3 = (d0 + d2) ^ d0;
        
          if( d3 >= 0 )
            d0 = voice->vc_PeriodSlidePeriod + d2;
          else
            d0 = voice->vc_PeriodSlideLimit;
        
          voice->vc_PeriodSlidePeriod = d0;
          voice->vc_PlantPeriod = 1;
        }
      } else {
        voice->vc_PeriodSlidePeriod += voice->vc_PeriodSlideSpeed;
        voice->vc_PlantPeriod = 1;
      }
    }
  }
  
  // Vibrato
  if( voice->vc_VibratoDepth )
  {
    if( voice->vc_VibratoDelay <= 0 )
    {
      voice->vc_VibratoPeriod = (vib_tab[voice->vc_VibratoCurrent] * voice->vc_VibratoDepth) >> 7;
      voice->vc_PlantPeriod = 1;
      voice->vc_VibratoCurrent = (voice->vc_VibratoCurrent + voice->vc_VibratoSpeed) & 0x3f;
    } else {
      voice->vc_VibratoDelay--;
    }
  }
  
  // PList
  if( voice->vc_PerfList != 0 )
  {
    if( voice->vc_Instrument && voice->vc_PerfCurrent < voice->vc_Instrument->ins_PList.pls_Length )
    {
      int signedOverflow = (voice->vc_PerfWait == 128);
      
      voice->vc_PerfWait--;
      if( signedOverflow || (int8)voice->vc_PerfWait <= 0 )
      {
        uint32 i;
        int32 cur;
        
        cur = voice->vc_PerfCurrent++;
        voice->vc_PerfWait = voice->vc_PerfSpeed;
        
        if( voice->vc_PerfList->pls_Entries[cur].ple_Waveform )
        {
          voice->vc_Waveform             = voice->vc_PerfList->pls_Entries[cur].ple_Waveform-1;
          voice->vc_NewWaveform          = 1;
          voice->vc_PeriodPerfSlideSpeed = voice->vc_PeriodPerfSlidePeriod = 0;
        }
        
        // Holdwave
        voice->vc_PeriodPerfSlideOn = 0;
        
        for( i=0; i<2; i++ )
          rp_plist_command_parse( at, voice, voice->vc_PerfList->pls_Entries[cur].ple_FX[i]&0xff, voice->vc_PerfList->pls_Entries[cur].ple_FXParam[i]&0xff );
        
        // GetNote
        if( voice->vc_PerfList->pls_Entries[cur].ple_Note )
        {
          voice->vc_InstrPeriod = voice->vc_PerfList->pls_Entries[cur].ple_Note;
          voice->vc_PlantPeriod = 1;
          voice->vc_FixedNote   = voice->vc_PerfList->pls_Entries[cur].ple_Fixed;
        }
      }
    } else {
      if( voice->vc_PerfWait )
        voice->vc_PerfWait--;
      else
        voice->vc_PeriodPerfSlideSpeed = 0;
    }
  }
  
  // PerfPortamento
  if( voice->vc_PeriodPerfSlideOn )
  {
    voice->vc_PeriodPerfSlidePeriod -= voice->vc_PeriodPerfSlideSpeed;
    
    if( voice->vc_PeriodPerfSlidePeriod )
      voice->vc_PlantPeriod = 1;
  }
  
  if( voice->vc_Waveform == 3-1 && voice->vc_SquareOn )
  {
    if( --voice->vc_SquareWait <= 0 )
    {
      int32 d1, d2, d3;
      
      d1 = voice->vc_SquareLowerLimit;
      d2 = voice->vc_SquareUpperLimit;
      d3 = voice->vc_SquarePos;
      
      if( voice->vc_SquareInit )
      {
        voice->vc_SquareInit = 0;
        
        if( d3 <= d1 )
        {
          voice->vc_SquareSlidingIn = 1;
          voice->vc_SquareSign = 1;
        } else if( d3 >= d2 ) {
          voice->vc_SquareSlidingIn = 1;
          voice->vc_SquareSign = -1;
        }
      }
      
      // NoSquareInit
      if( d1 == d3 || d2 == d3 )
      {
        if( voice->vc_SquareSlidingIn )
          voice->vc_SquareSlidingIn = 0;
        else
          voice->vc_SquareSign = -voice->vc_SquareSign;
      }
      
      d3 += voice->vc_SquareSign;
      voice->vc_SquarePos   = d3;
      voice->vc_PlantSquare = 1;
      voice->vc_SquareWait  = voice->vc_Instrument->ins_SquareSpeed;
    }
  }
  
  if( voice->vc_FilterOn && --voice->vc_FilterWait <= 0 )
  {
    uint32 i, FMax;
    int32 d1, d2, d3;
    
    d1 = voice->vc_FilterLowerLimit;
    d2 = voice->vc_FilterUpperLimit;
    d3 = voice->vc_FilterPos;
    
    if( voice->vc_FilterInit )
    {
      voice->vc_FilterInit = 0;
      if( d3 <= d1 )
      {
        voice->vc_FilterSlidingIn = 1;
        voice->vc_FilterSign      = 1;
      } else if( d3 >= d2 ) {
        voice->vc_FilterSlidingIn = 1;
        voice->vc_FilterSign      = -1;
      }
    }
    
    // NoFilterInit
    FMax = (voice->vc_FilterSpeed < 4) ? (5-voice->vc_FilterSpeed) : 1;

    for( i=0; i<FMax; i++ )
    {
      if( ( d1 == d3 ) || ( d2 == d3 ) )
      {
        if( voice->vc_FilterSlidingIn )
          voice->vc_FilterSlidingIn = 0;
        else
          voice->vc_FilterSign = -voice->vc_FilterSign;
      }
      d3 += voice->vc_FilterSign;
    }
    
    if( d3 < 1 )  d3 = 1;
    if( d3 > 63 ) d3 = 63;
    voice->vc_FilterPos   = d3;
    voice->vc_NewWaveform = 1;
    voice->vc_FilterWait  = voice->vc_FilterSpeed - 3;
    
    if( voice->vc_FilterWait < 1 )
      voice->vc_FilterWait = 1;
  }

  if( voice->vc_Waveform == 3-1 || voice->vc_PlantSquare )
  {
    // CalcSquare
    uint32  i;
    int32   Delta;
    int8   *SquarePtr;
    int32  X;
    
    SquarePtr = &waves[WO_SQUARES+(voice->vc_FilterPos-0x20)*(0xfc+0xfc+0x80*0x1f+0x80+0x280*3)];
    X = voice->vc_SquarePos << (5 - voice->vc_WaveLength);
    
    if( X > 0x20 )
    {
      X = 0x40 - X;
      voice->vc_SquareReverse = 1;
    }
    
    // OkDownSquare
    if( X > 0 )
      SquarePtr += (X-1) << 7;
    
    Delta = 32 >> voice->vc_WaveLength;
    at->at_WaveformTab[2] = voice->vc_SquareTempBuffer;
    
    for( i=0; i<(1<<voice->vc_WaveLength)*4; i++ )
    {
      voice->vc_SquareTempBuffer[i] = *SquarePtr;
      SquarePtr += Delta;
    }
    
    voice->vc_NewWaveform = 1;
    voice->vc_Waveform    = 3-1;
    voice->vc_PlantSquare = 0;
  }
  
  if( voice->vc_Waveform == 4-1 )
    voice->vc_NewWaveform = 1;
  
  if( voice->vc_RingNewWaveform )
  {
    int8 *rasrc;
    
    if( voice->vc_RingWaveform > 1 ) voice->vc_RingWaveform = 1;
    
    rasrc = at->at_WaveformTab[voice->vc_RingWaveform];
    rasrc += Offsets[voice->vc_WaveLength];
    
    voice->vc_RingAudioSource = rasrc;
  }    
        
  
  if( voice->vc_NewWaveform )
  {
    int8 *AudioSource;

    AudioSource = at->at_WaveformTab[voice->vc_Waveform];
    
    if( voice->vc_Waveform != 3-1 )
      AudioSource += (voice->vc_FilterPos-0x20)*(0xfc+0xfc+0x80*0x1f+0x80+0x280*3);

    if( voice->vc_Waveform < 3-1)
    {
      // GetWLWaveformlor2
      AudioSource += Offsets[voice->vc_WaveLength];
    }

    if( voice->vc_Waveform == 4-1 )
    {
      // AddRandomMoving
      AudioSource += ( voice->vc_WNRandom & (2*0x280-1) ) & ~1;
      // GoOnRandom
      voice->vc_WNRandom += 2239384;
      voice->vc_WNRandom  = ((((voice->vc_WNRandom >> 8) | (voice->vc_WNRandom << 24)) + 782323) ^ 75) - 6735;
    }

    voice->vc_AudioSource = AudioSource;
  }
  
  // Ring modulation period calculation
  if( voice->vc_RingAudioSource )
  {
    voice->vc_RingAudioPeriod = voice->vc_RingBasePeriod;
  
    if( !(voice->vc_RingFixedPeriod) )
    {
      if( voice->vc_OverrideTranspose != 1000 ) // 1.5
        voice->vc_RingAudioPeriod += voice->vc_OverrideTranspose + voice->vc_TrackPeriod - 1;
      else
        voice->vc_RingAudioPeriod += voice->vc_Transpose + voice->vc_TrackPeriod - 1;
    }
  
    if( voice->vc_RingAudioPeriod > 5*12 )
      voice->vc_RingAudioPeriod = 5*12;
  
    if( voice->vc_RingAudioPeriod < 0 )
      voice->vc_RingAudioPeriod = 0;
  
    voice->vc_RingAudioPeriod = period_tab[voice->vc_RingAudioPeriod];

    if( !(voice->vc_RingFixedPeriod) )
      voice->vc_RingAudioPeriod += voice->vc_PeriodSlidePeriod;

    voice->vc_RingAudioPeriod += voice->vc_PeriodPerfSlidePeriod + voice->vc_VibratoPeriod;

    if( voice->vc_RingAudioPeriod > 0x0d60 )
      voice->vc_RingAudioPeriod = 0x0d60;

    if( voice->vc_RingAudioPeriod < 0x0071 )
      voice->vc_RingAudioPeriod = 0x0071;
  }
  
  // Normal period calculation
  voice->vc_AudioPeriod = voice->vc_InstrPeriod;
  
  if( !(voice->vc_FixedNote) )
  {
    if( voice->vc_OverrideTranspose != 1000 ) // 1.5
      voice->vc_AudioPeriod += voice->vc_OverrideTranspose + voice->vc_TrackPeriod - 1;
    else
      voice->vc_AudioPeriod += voice->vc_Transpose + voice->vc_TrackPeriod - 1;
  }
    
  if( voice->vc_AudioPeriod > 5*12 )
    voice->vc_AudioPeriod = 5*12;
  
  if( voice->vc_AudioPeriod < 0 )
    voice->vc_AudioPeriod = 0;
  
  voice->vc_AudioPeriod = period_tab[voice->vc_AudioPeriod];
  
  if( !(voice->vc_FixedNote) )
    voice->vc_AudioPeriod += voice->vc_PeriodSlidePeriod;

  voice->vc_AudioPeriod += voice->vc_PeriodPerfSlidePeriod + voice->vc_VibratoPeriod;    

  if( voice->vc_AudioPeriod > 0x0d60 )
    voice->vc_AudioPeriod = 0x0d60;

  if( voice->vc_AudioPeriod < 0x0071 )
    voice->vc_AudioPeriod = 0x0071;
  
  voice->vc_AudioVolume = (((((((voice->vc_ADSRVolume >> 8) * voice->vc_NoteMaxVolume) >> 6) * voice->vc_PerfSubVolume) >> 6) * voice->vc_TrackMasterVolume) >> 6);
}

void rp_set_audio( struct ahx_voice *voice, float64 freqf )
{
  if( voice->vc_TrackOn == 0 )
  {
    voice->vc_VoiceVolume = 0;
    return;
  }
  
  voice->vc_VoiceVolume = voice->vc_AudioVolume;
  
  if( voice->vc_PlantPeriod )
  {
    float64 freq2;
    uint32  delta;
    
    voice->vc_PlantPeriod = 0;
    voice->vc_VoicePeriod = voice->vc_AudioPeriod;
    
    freq2 = Period2Freq( voice->vc_AudioPeriod );
    delta = (uint32)(freq2 / freqf);

    if( delta > (0x280<<16) ) delta -= (0x280<<16);
    if( delta == 0 ) delta = 1;
    voice->vc_Delta = delta;
  }
  
  if( voice->vc_NewWaveform )
  {
    int8 *src;
    
    src = voice->vc_AudioSource;

    if( voice->vc_Waveform == 4-1 )
    {
      IExec->CopyMem((void *)src, &voice->vc_VoiceBuffer[0], 0x280);
    } else {
      uint32 i, WaveLoops;

      WaveLoops = (1 << (5 - voice->vc_WaveLength)) * 5;

      for( i=0; i<WaveLoops; i++ )
        IExec->CopyMem((void *)src, &voice->vc_VoiceBuffer[i*4*(1<<voice->vc_WaveLength)], 4*(1<<voice->vc_WaveLength));
    }

    voice->vc_VoiceBuffer[0x280] = voice->vc_VoiceBuffer[0];
    voice->vc_MixSource          = voice->vc_VoiceBuffer;
  }

  /* Ring Modulation */
  if( voice->vc_RingPlantPeriod )
  {
    float64 freq2;
    uint32  delta;
    
    voice->vc_RingPlantPeriod = 0;
    freq2 = Period2Freq( voice->vc_RingAudioPeriod );
    delta = (uint32)(freq2 / freqf);
    
    if( delta > (0x280<<16) ) delta -= (0x280<<16);
    if( delta == 0 ) delta = 1;
    voice->vc_RingDelta = delta;
  }
  
  if( voice->vc_RingNewWaveform )
  {
    int8 *src;
    uint32 i, WaveLoops;
    
    src = voice->vc_RingAudioSource;

    WaveLoops = (1 << (5 - voice->vc_WaveLength)) * 5;

    for( i=0; i<WaveLoops; i++ )
      IExec->CopyMem((void *)src, &voice->vc_RingVoiceBuffer[i*4*(1<<voice->vc_WaveLength)], 4*(1<<voice->vc_WaveLength));

    voice->vc_RingVoiceBuffer[0x280] = voice->vc_RingVoiceBuffer[0];
    voice->vc_RingMixSource          = voice->vc_RingVoiceBuffer;
  }
}

void rp_play_irq( struct ahx_tune *at )
{
  uint32 i;
  
  if( ( rp_state != STS_PLAYNOTE ) &&
      ( rp_state != STS_PLAYROW ) )
  {
    if( at->at_stopnextrow > 0 )
    {
      if( ( at->at_GetNewPosition ) ||
          ( at->at_StepWaitFrames <= 1 ) )
      {
        at->at_stopnextrow--;
        if( at->at_stopnextrow == 0 )
          rp_state = STS_PLAYROW;
      }
    }
  }

  if( ( rp_state != STS_PLAYNOTE ) &&
      ( rp_state != STS_PLAYROW ) )
  {
    if( at->at_StepWaitFrames == 0 )
    {
      if( at->at_GetNewPosition )
      {
        int32 nextpos = (at->at_PosNr+1==at->at_PositionNr)?0:(at->at_PosNr+1);

        for( i=0; i<at->at_Channels; i++ )
        {
          at->at_Voices[i].vc_Track         = at->at_Positions[at->at_PosNr].pos_Track[i];
          at->at_Voices[i].vc_Transpose     = at->at_Positions[at->at_PosNr].pos_Transpose[i];
          at->at_Voices[i].vc_NextTrack     = at->at_Positions[nextpos].pos_Track[i];
          at->at_Voices[i].vc_NextTranspose = at->at_Positions[nextpos].pos_Transpose[i];
        }
        at->at_GetNewPosition = 0;
      }
    
      for( i=0; i<at->at_Channels; i++ )
        rp_process_step( at, &at->at_Voices[i] );
    
      at->at_StepWaitFrames = at->at_Tempo;
    }
  }
  
  for( i=0; i<at->at_Channels; i++ )
    rp_process_frame( at, &at->at_Voices[i] );

  if( ( rp_state != STS_PLAYNOTE ) &&
      ( rp_state != STS_PLAYROW ) )
  {
    at->at_PlayingTime++;
    if( --at->at_StepWaitFrames == 0 )
    {
      if( !at->at_PatternBreak )
      {
        at->at_NoteNr++;
        if( at->at_NoteNr >= at->at_TrackLength )
        {
          at->at_PosJump      = at->at_PosNr+1;
          at->at_PosJumpNote  = 0;
          at->at_PatternBreak = 1;
        }
      }
    
      if( at->at_PatternBreak )
      {
        at->at_PatternBreak = 0;
        
        // Manual override?
        if( at->at_NextPosNr != -1 )
        {
          at->at_PosNr       = at->at_NextPosNr;
          at->at_NextPosNr   = -1;
          at->at_NoteNr      = 0;
        } else {
          if( rp_state != STS_PLAYPOS )
          {
            at->at_PosNr        = at->at_PosJump;
            at->at_NoteNr       = at->at_PosJumpNote;
            if( at->at_PosNr == at->at_PositionNr )
            {
              at->at_SongEndReached = 1;
              at->at_PosNr          = at->at_Restart;
            }
          } else {
            at->at_NoteNr = 0;
          }
        }
        at->at_PosJumpNote  = 0;
        at->at_PosJump      = 0;

        at->at_GetNewPosition = 1;
      }
    }
  }

  for( i=0; i<at->at_Channels; i++ )
    rp_set_audio( &at->at_Voices[i], (float64)FREQ );
  
}

int32 rp_mix_findloudest( struct ahx_tune *at, uint32 samples )
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
  
  chans = at->at_Channels;
  for( i=0; i<chans; i++ )
  {
    delta[i] = at->at_Voices[i].vc_Delta;
    vol[i]   = at->at_Voices[i].vc_VoiceVolume;
    pos[i]   = at->at_Voices[i].vc_SamplePos;
    src[i]   = at->at_Voices[i].vc_MixSource;
    panl[i]  = at->at_Voices[i].vc_PanMultLeft;
    panr[i]  = at->at_Voices[i].vc_PanMultRight;
    /* Ring Modulation */
    rdelta[i]= at->at_Voices[i].vc_RingDelta;
    rpos[i]  = at->at_Voices[i].vc_RingSamplePos;
    rsrc[i]  = at->at_Voices[i].vc_RingMixSource;
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
      
//      a = (a*at->at_mixgain)>>8;
//      b = (b*at->at_mixgain)>>8;
      a = abs( a );
      b = abs( b );
      
      if( a > loud ) loud = a;
      if( b > loud ) loud = b;
      
      loops--;
    } while( loops > 0 );
  } while( samples > 0 );

  for( i=0; i<chans; i++ )
  {
    at->at_Voices[i].vc_SamplePos = pos[i];
    at->at_Voices[i].vc_RingSamplePos = rpos[i];
  }
    
  return loud;
}

void rp_mixchunk( struct ahx_tune *at, uint32 samples, int8 *buf1, int8 *buf2, int32 bufmod )
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
  uint32  vu[MAX_CHANNELS];
  int32   a=0, b=0, j;
  uint32  i, chans, loops;
  
  chans = at->at_Channels;
  for( i=0; i<chans; i++ )
  {
    delta[i] = at->at_Voices[i].vc_Delta;
    vol[i]   = at->at_Voices[i].vc_VoiceVolume;
    pos[i]   = at->at_Voices[i].vc_SamplePos;
    src[i]   = at->at_Voices[i].vc_MixSource;
    panl[i]  = at->at_Voices[i].vc_PanMultLeft;
    panr[i]  = at->at_Voices[i].vc_PanMultRight;
    
    /* Ring Modulation */
    rdelta[i]= at->at_Voices[i].vc_RingDelta;
    rpos[i]  = at->at_Voices[i].vc_RingSamplePos;
    rsrc[i]  = at->at_Voices[i].vc_RingMixSource;
    
    vu[i] = 0;
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
        
        if( abs( j ) > vu[i] ) vu[i] = abs( j );

        a += (j * panl[i]) >> 7;
        b += (j * panr[i]) >> 7;
        pos[i] += delta[i];
      }
      
      a = (a*at->at_mixgain)>>8;
      b = (b*at->at_mixgain)>>8;
      
      *(int16 *)buf1 = a;
      *(int16 *)buf2 = b;
      
      loops--;
      
      buf1 += bufmod;
      buf2 += bufmod;
    } while( loops > 0 );
  } while( samples > 0 );

  for( i=0; i<chans; i++ )
  {
    at->at_Voices[i].vc_SamplePos = pos[i];
    at->at_Voices[i].vc_RingSamplePos = rpos[i];
    at->at_Voices[i].vc_VUMeter = vu[i];
  }
}


void rp_decode_frame( struct ahx_tune *at, int8 *buf1, int8 *buf2, int32 bufmod )
{
  uint32 samples, loops;
  
  samples = FREQ/50/at->at_SpeedMultiplier;
  loops   = at->at_SpeedMultiplier;
  
  do
  {
    rp_play_irq( at );
    rp_mixchunk( at, samples, buf1, buf2, bufmod );
    buf1 += samples * bufmod;
    buf2 += samples * bufmod;
    loops--;
  } while( loops );
  
  if( ( rp_state == STS_PLAYSONG ) ||
      ( rp_state == STS_PLAYPOS ) )
  {
    if( (at->at_ticks=(at->at_ticks+1)%50) == 0 )
      if( (at->at_secs =(at->at_secs +1)%60) == 0 )
        if( (at->at_mins =(at->at_mins +1)%60) == 0 )
          at->at_hours++;
  }

  // Update the gui!
  rp_curtune = at;
#ifndef __SDL_WRAPPER__
  IExec->Signal( rp_maintask, gui_tick_sig );
#else
  {
    SDL_Event     event;
    SDL_UserEvent userevent;

    userevent.type  = SDL_USEREVENT;
    userevent.code  = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = userevent;
    
    SDL_PushEvent( &event );
  }
#endif
}

// You'd better not be playing this bastard!
int32 rp_find_loudest( struct ahx_tune *at )
{
  uint32 rsamp, rloop;
  uint32 samples, loops, loud, n, i;

  rsamp = FREQ/50/at->at_SpeedMultiplier;
  rloop = at->at_SpeedMultiplier;

  loud = 0;
  
  for( i=0; i<=at->at_SubsongNr; i++ )
  {
    rp_init_subsong( at, i );
  
    at->at_SongEndReached = 0;
    
    while( at->at_SongEndReached == 0 )
    {
      samples = rsamp;
      loops   = rloop;

//      if( IExec->SetSignal( 0L, SIGBREAKF_CTRL_C ) & SIGBREAKF_CTRL_C )
//        break;
      do
      {
        rp_play_irq( at );
        n = rp_mix_findloudest( at, samples );
        if( n > loud ) loud = n;
        loops--;
      } while( loops );
    }
  }
  
  return loud;
}

#ifndef __SDL_WRAPPER__
BOOL rp_alloc_buffers( void )
{
  int32 i;

  if( rp_audiobuffer[0] ) IExec->FreePooled( rp_mempool, rp_audiobuffer[0], rp_audiobuflen * 2 );
  
  rp_freqf = (float64)FREQ;
  rp_audiobuflen = FREQ * sizeof( uint16 ) * 2 / 50;

  rp_audiobuffer[0] = IExec->AllocPooled( rp_mempool, rp_audiobuflen * 2 );
  if( rp_audiobuffer[0] == NULL ) return FALSE;

  rp_audiobuffer[1] = &rp_audiobuffer[0][rp_audiobuflen];
  
  for( i=0; i<rp_audiobuflen; i++ )
    rp_audiobuffer[0][i] = 0;
  
  return TRUE;
}

BOOL rp_subtask_init( void )
{
  if( !rp_alloc_buffers() ) return FALSE;

  rp_msgport = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
  if( !rp_msgport ) return FALSE;
  
  ahi_mp = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
  if( !ahi_mp ) return FALSE;
  
  ahi_io[0] = (struct AHIRequest *)IExec->AllocSysObjectTags(ASOT_IOREQUEST,
    ASOIOR_ReplyPort, ahi_mp,
    ASOIOR_Size,      sizeof( struct AHIRequest ),
    TAG_DONE);
  if( ahi_io[0] == NULL ) return FALSE;

  ahi_io[0]->ahir_Version = 4;
  
  ahi_dev = IExec->OpenDevice( AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)ahi_io[0], 0 ); 
  if( ahi_dev == -1 ) return FALSE;
  
  AHIBase = (struct Library *)ahi_io[0]->ahir_Std.io_Device;
  IAHI = (struct AHIIFace *)IExec->GetInterface( AHIBase, "main", 1, NULL );
  if( !IAHI ) return FALSE;
  
  ahi_io[1] = IExec->AllocSysObjectTags(ASOT_IOREQUEST, ASOIOR_Size, sizeof( struct AHIRequest ), ASOIOR_Duplicate, ahi_io[0], TAG_DONE );
  if( ahi_io[1] == NULL ) return FALSE;
  
  return TRUE;
}

void rp_subtask_shut( void )
{
  if( IAHI )          IExec->DropInterface( (struct Interface *)IAHI );
  if( ahi_dev != -1 )
  {
    IExec->CloseDevice( (struct IORequest *)ahi_io[0] );
    IExec->FreeSysObject( ASOT_IOREQUEST, ahi_io[0] );
  }
  if( ahi_io[1] )     IExec->FreeSysObject( ASOT_IOREQUEST, ahi_io[1] );
  if( ahi_mp )        IExec->FreeSysObject( ASOT_PORT, ahi_mp );
  if( rp_msgport )
  {
    struct Message *msg;
    while( ( msg = IExec->GetMsg( rp_msgport ) ) ) IExec->ReplyMsg( msg );
    IExec->FreeSysObject( ASOT_PORT, rp_msgport );
    rp_msgport = NULL;
  }
  if( rp_audiobuffer[0] ) IExec->FreePooled( rp_mempool, rp_audiobuffer[0], rp_audiobuflen * 2 );
}

void rp_mix_and_play_sounds( struct ahx_tune *at, BOOL *need_wait, struct AHIRequest **prev_req, uint32 *nextbuf )
{
  int32 rp_nextbuf;
  
  rp_nextbuf = *nextbuf;
  
  if( need_wait[rp_nextbuf] )
    IExec->WaitIO( (struct IORequest *)ahi_io[rp_nextbuf] );

  rp_decode_frame( at, rp_audiobuffer[rp_nextbuf], rp_audiobuffer[rp_nextbuf]+sizeof( int16 ), 4 );

  ahi_io[rp_nextbuf]->ahir_Std.io_Command = CMD_WRITE;
  ahi_io[rp_nextbuf]->ahir_Std.io_Data    = rp_audiobuffer[rp_nextbuf];
  ahi_io[rp_nextbuf]->ahir_Std.io_Length  = rp_audiobuflen;
  ahi_io[rp_nextbuf]->ahir_Std.io_Offset  = 0;
  ahi_io[rp_nextbuf]->ahir_Type           = AHIST_S16S;
  ahi_io[rp_nextbuf]->ahir_Frequency      = rp_freq;
  ahi_io[rp_nextbuf]->ahir_Volume         = 0x10000;
  ahi_io[rp_nextbuf]->ahir_Position       = 0x8000;
  ahi_io[rp_nextbuf]->ahir_Link           = *prev_req;
  IExec->SendIO( (struct IORequest *)ahi_io[rp_nextbuf] );
            
  *prev_req = ahi_io[rp_nextbuf];
  need_wait[rp_nextbuf] = TRUE;

  rp_nextbuf ^= 1;  
  
  if( ( need_wait[rp_nextbuf] == TRUE ) &&
      ( IExec->CheckIO( (struct IORequest *)ahi_io[rp_nextbuf] ) != NULL ) )
  {
    IExec->WaitIO( (struct IORequest *)ahi_io[rp_nextbuf] );
    need_wait[rp_nextbuf] = FALSE;
  }
            
  if( need_wait[rp_nextbuf] == FALSE )
  {
    rp_decode_frame( at, rp_audiobuffer[rp_nextbuf], rp_audiobuffer[rp_nextbuf]+sizeof( int16 ), 4 );

    ahi_io[rp_nextbuf]->ahir_Std.io_Command = CMD_WRITE;
    ahi_io[rp_nextbuf]->ahir_Std.io_Data    = rp_audiobuffer[rp_nextbuf];
    ahi_io[rp_nextbuf]->ahir_Std.io_Length  = rp_audiobuflen;
    ahi_io[rp_nextbuf]->ahir_Std.io_Offset  = 0;
    ahi_io[rp_nextbuf]->ahir_Type           = AHIST_S16S;
    ahi_io[rp_nextbuf]->ahir_Frequency      = rp_freq;
    ahi_io[rp_nextbuf]->ahir_Volume         = 0x10000;
    ahi_io[rp_nextbuf]->ahir_Position       = 0x8000;
    ahi_io[rp_nextbuf]->ahir_Link           = *prev_req;
    IExec->SendIO( (struct IORequest *)ahi_io[rp_nextbuf] );

    *prev_req = ahi_io[rp_nextbuf];
    need_wait[rp_nextbuf] = TRUE;
    rp_nextbuf ^= 1;  
  }
  *nextbuf = rp_nextbuf;
}

void rp_subtask_main( void )
{
  uint32 gotsigs;
  uint32 rp_msgsig, rp_ahisig;
  uint32 rp_nextbuf, i;
  BOOL need_wait[2];
  struct ahx_tune *rp_tune;
  struct rp_command *msg;
  
  if( rp_subtask_init() )
  {
    rp_state   = STS_IDLE;
    rp_tune    = NULL;
    rp_msgsig  = 1L<<rp_msgport->mp_SigBit;
    rp_ahisig  = 1L<<ahi_mp->mp_SigBit;
    rp_nextbuf = 0;
    need_wait[0] = FALSE;
    need_wait[1] = FALSE;
    struct AHIRequest *prev_req = NULL;
    
    IExec->Signal( rp_maintask, (1L<<rp_subtask_sig) );

//    IExec->DebugPrintF( "Replayer subtask, ready for action!\n" );

    while( 1 )
    {
      gotsigs = IExec->Wait( rp_msgsig | rp_ahisig | SIGBREAKF_CTRL_C );
      
      if( gotsigs & rp_msgsig )
      {
        while( ( msg = (struct rp_command *)IExec->GetMsg( rp_msgport ) ) )
        {
//          IExec->DebugPrintF( "Got command %ld, %ld, %08lX\n", msg->rpc_Command, msg->rpc_Data, msg->rpc_Tune );
          switch( msg->rpc_Command )
          {
            case RPC_STOP:
              if( need_wait[0] ) IExec->WaitIO( (struct IORequest *)ahi_io[0] );
              if( need_wait[1] ) IExec->WaitIO( (struct IORequest *)ahi_io[1] );
              need_wait[0] = need_wait[1] = FALSE;
              if( rp_tune )
              {
                for( i=0; i<MAX_CHANNELS; i++ )
                {
                  rp_tune->at_Voices[i].vc_TrackOn = 0;
                  rp_tune->at_Voices[i].vc_VUMeter = 0;
                }
                if( rp_tune->at_NextPosNr != -1 )
                {
                  rp_tune->at_PosNr     = rp_tune->at_NextPosNr;
                  rp_tune->at_NextPosNr = -1;
                  IExec->Signal( rp_maintask, gui_tick_sig );                  
                }
                rp_tune = NULL;
              }   
              rp_state = STS_IDLE;
              break;

            case RPC_PLAY_NOTE:
              if( rp_tune != msg->rpc_Tune )
              {
                if( need_wait[0] ) IExec->WaitIO( (struct IORequest *)ahi_io[0] );
                if( need_wait[1] ) IExec->WaitIO( (struct IORequest *)ahi_io[1] );
                need_wait[0] = need_wait[1] = FALSE;
                rp_tune = msg->rpc_Tune;
              }
              
              if( rp_tune != NULL )
              {
                rp_play_instrument( rp_tune, msg->rpc_Data2>>8, msg->rpc_Data, msg->rpc_Data2&0xff );
                gotsigs |= rp_ahisig;
                rp_state = STS_PLAYNOTE;
              }
              break;
            
            case RPC_PLAY_ROW:
              if( msg->rpc_Tune != NULL )
              {
                struct ahx_tune *ttune;
                ttune = rp_tune;
                rp_tune = msg->rpc_Tune;
                rp_tune->at_stopnextrow    = 3;
                rp_tune->at_GetNewPosition = 1;
                rp_tune->at_ticks = rp_tune->at_secs = rp_tune->at_mins = rp_tune->at_hours = 0;
                if( ( rp_tune != ttune ) || ( rp_state != STS_PLAYROW ) )
                  rp_reset_some_shit( rp_tune );
                gotsigs |= rp_ahisig;
                rp_state = STS_PLAYPOS;
              }
              break;

            case RPC_CONT_POS:
            case RPC_PLAY_POS:
              if( rp_tune )
              {
                if( rp_tune->at_doing == D_PLAYING )
                  rp_tune->at_doing = D_IDLE;
              }
            
              rp_tune = msg->rpc_Tune;
              
              if( rp_tune != NULL )
              {
                rp_tune->at_stopnextrow = 0;
                if( msg->rpc_Command == RPC_PLAY_POS )
                  rp_tune->at_NoteNr = 0;

                rp_tune->at_PosJumpNote    = 0;
                rp_tune->at_StepWaitFrames = 0;
                rp_tune->at_GetNewPosition = 1;
                rp_tune->at_ticks = rp_tune->at_secs = rp_tune->at_mins = rp_tune->at_hours = 0;
                rp_reset_some_shit( rp_tune );
                gotsigs |= rp_ahisig;
                rp_state = STS_PLAYPOS;
              }
              break;

            case RPC_CONT_SONG:
            case RPC_PLAY_SONG:
              if( rp_tune )
              {
                if( rp_tune->at_doing == D_PLAYING )
                  rp_tune->at_doing = D_IDLE;
              }

              rp_tune = msg->rpc_Tune;
              if( rp_tune != NULL )
              {
                rp_tune->at_stopnextrow = 0;
                if( msg->rpc_Command == RPC_PLAY_SONG )
                  rp_tune->at_NoteNr         = 0;

                rp_tune->at_PosJumpNote    = 0;
                rp_tune->at_StepWaitFrames = 0;
                rp_tune->at_GetNewPosition = 1;
                rp_tune->at_ticks = rp_tune->at_secs = rp_tune->at_mins = rp_tune->at_hours = 0;
                rp_reset_some_shit( rp_tune );
              }

              if( rp_tune )
              {
                rp_state = STS_PLAYSONG;
                gotsigs |= rp_ahisig;
              } else {
                rp_state = STS_IDLE;
              }

              msg->rpc_Tune = rp_tune;
              break;
          }
               
          IExec->ReplyMsg( (struct Message *)msg );
        }
      }
      
      if( gotsigs & SIGBREAKF_CTRL_C ) break;
      
      if( gotsigs & rp_ahisig )
      {
        BOOL anychan;
        switch( rp_state )
        {
          case STS_CALCULATING:
          case STS_IDLE:
            prev_req = NULL;
            break;
          case STS_PLAYNOTE:
          case STS_PLAYROW:
            if( rp_tune == NULL )
            {
              rp_state = STS_IDLE;
              break;
            }

            anychan = FALSE;
            for( i=0; i<rp_tune->at_Channels; i++ )
              if( rp_tune->at_Voices[i].vc_TrackOn )
              {
                anychan = TRUE;
                break;
              }
            
            if( anychan == FALSE )
            {
              rp_state = STS_IDLE;
              break;
            }

            rp_mix_and_play_sounds( rp_tune, &need_wait[0], &prev_req, &rp_nextbuf );
            break;
            
          case STS_PLAYPOS:
          case STS_PLAYSONG:
            if( rp_tune == NULL )
            {
              rp_state = STS_IDLE;
              break;
            }

            rp_mix_and_play_sounds( rp_tune, &need_wait[0], &prev_req, &rp_nextbuf );

            break;
          default:
            // Unknown state?!
            rp_state = STS_IDLE;
        }
      }
    }
  }
  rp_subtask_shut();
  rp_state = STS_DEADED;
  IExec->Signal( rp_maintask, (1L<<rp_subtask_sig) );
}
#else
void do_the_music( void *dummy, int8 *stream, int length )
{
  if((rp_state != STS_IDLE) && (rp_playtune))
  {
    int i;

    for (i=0; i<(length/2); i+=((FREQ/50)*2))
      rp_decode_frame( rp_playtune, (int8*)&rp_audiobuffer[i], (int8*)&rp_audiobuffer[i+1], 4 );
    memcpy(stream, rp_audiobuffer, length);
  }
  else
  {
    if (stream && length)
      memset(stream, 0, length);
  }

  rp_state_ack = rp_state;
}
#endif

BOOL rp_init( void )
{
#ifdef __SDL_WRAPPER__
  SDL_AudioSpec wanted;
#endif

#ifndef __SDL_WRAPPER__
  rp_maintask = IExec->FindTask( NULL );

  rp_subtask_sig = IExec->AllocSignal( -1 );
  if( rp_subtask_sig == -1 )
  {
    printf( "Unable to allocate signal\n" );
    return FALSE;
  }

  rp_mempool = IExec->AllocSysObjectTags( ASOT_MEMPOOL,
    ASOPOOL_Puddle,    8192,
    ASOPOOL_Threshold, 8000,
    ASOPOOL_Protected, TRUE,
    TAG_DONE );
  if( !rp_mempool )
  {
    printf( "Unable to create memory pool\n" );
    return FALSE;
  }
#endif

  rp_tunelist = IExec->AllocSysObjectTags( ASOT_LIST, TAG_DONE );
  if( !rp_tunelist )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }
  
  rp_list_ss = IExec->AllocSysObjectTags( ASOT_SEMAPHORE, TAG_DONE );
  if( !rp_list_ss )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }

#ifndef __SDL_WRAPPER__
  rp_replyport = IExec->AllocSysObject(ASOT_PORT, TAG_DONE);
  if( !rp_replyport )
  {
    printf( "Unable to create a message port\n" );
    return FALSE;
  }
  
  rp_mainmsg = IExec->AllocSysObjectTags( ASOT_MESSAGE,
    ASOMSG_Size,      sizeof( struct rp_command ),
    ASOMSG_ReplyPort, rp_replyport,
    TAG_DONE );
#else
  rp_mainmsg = malloc(sizeof(struct rp_command));
#endif

  if( !rp_mainmsg )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }

  rp_GenPanningTables();
  rp_GenSawtooth( &waves[WO_SAWTOOTH_04], 0x04 );
  rp_GenSawtooth( &waves[WO_SAWTOOTH_08], 0x08 );
  rp_GenSawtooth( &waves[WO_SAWTOOTH_10], 0x10 );
  rp_GenSawtooth( &waves[WO_SAWTOOTH_20], 0x20 );
  rp_GenSawtooth( &waves[WO_SAWTOOTH_40], 0x40 );
  rp_GenSawtooth( &waves[WO_SAWTOOTH_80], 0x80 );
  rp_GenTriangle( &waves[WO_TRIANGLE_04], 0x04 );
  rp_GenTriangle( &waves[WO_TRIANGLE_08], 0x08 );
  rp_GenTriangle( &waves[WO_TRIANGLE_10], 0x10 );
  rp_GenTriangle( &waves[WO_TRIANGLE_20], 0x20 );
  rp_GenTriangle( &waves[WO_TRIANGLE_40], 0x40 );
  rp_GenTriangle( &waves[WO_TRIANGLE_80], 0x80 );
  rp_GenSquare( &waves[WO_SQUARES] );
  rp_GenWhiteNoise( &waves[WO_WHITENOISE], WHITENOISELEN );
  rp_GenFilterWaves( &waves[WO_TRIANGLE_04], &waves[WO_LOWPASSES], &waves[WO_HIGHPASSES] );

#ifndef __SDL_WRAPPER__
  rp_subtask = IDOS->CreateNewProcTags( NP_Name,     "ahx2006 Player Task",
                                        NP_Entry,    rp_subtask_main,
                                        NP_Priority, 30,
                                        NP_Child,    TRUE,
                                        TAG_DONE );
  if( rp_subtask == NULL )
  {
    printf( "Unable to create audio task\n" );
    return FALSE;
  }
  
  IExec->Wait( (1L<<rp_subtask_sig) );
  if( rp_state == STS_DEADED )
  {
    printf( "Unable to initialise audio\n" );
    rp_subtask = NULL;
    return FALSE;
  }

  rp_sigs = ((1L<<rp_subtask_sig)|(1L<<rp_replyport->mp_SigBit));
#endif

  rp_new_tune( TRUE );  

#ifdef __SDL_WRAPPER__
  wanted.freq = FREQ; 
  wanted.format = AUDIO_S16SYS; 
  wanted.channels = 2; /* 1 = mono, 2 = stereo */
  wanted.samples = OUTPUT_LEN; // HIVELY_LEN;

  wanted.callback = (void*)do_the_music;
  wanted.userdata = 0;

  if( SDL_OpenAudio( &wanted, NULL ) < 0 )
  {
    printf( "SDL says: %s\n", SDL_GetError() );
    return FALSE;
  }

  SDL_PauseAudio(0);
#endif
  return TRUE;
}

void rp_shutdown( void )
{
  if( rp_tunelist )
  {
    rp_free_all_tunes();
    IExec->FreeSysObject( ASOT_LIST, rp_tunelist );
  }
  
  if( rp_list_ss ) IExec->FreeSysObject( ASOT_SEMAPHORE, rp_list_ss );

#ifndef __SDL_WRAPPER__
  if( rp_subtask )
  {
    IExec->Signal( (struct Task *)rp_subtask, SIGBREAKF_CTRL_C );
    IExec->Wait( (1L<<rp_subtask_sig) );
  }
  
  if( rp_mainmsg )   IExec->FreeSysObject( ASOT_MESSAGE, rp_mainmsg );
  
  if( rp_replyport ) IExec->FreeSysObject( ASOT_PORT, rp_replyport );

  if( rp_mempool )  IExec->FreeSysObject( ASOT_MEMPOOL, rp_mempool );  

  if( rp_subtask_sig != -1 ) IExec->FreeSignal( rp_subtask_sig );
#else
  SDL_PauseAudio(1);
  if( rp_mainmsg )   free(rp_mainmsg);
#endif
}

#ifndef __SDL_WRAPPER__
void rp_handler( uint32 gotsigs )
{
  // audio subtask has quit!
  if( gotsigs & (1L<<rp_subtask_sig) )
  {
    rp_subtask = NULL;
    quitting = TRUE;
    return;
  }
}
#endif

#ifdef __SDL_WRAPPER__
static void set_player_state(uint32 newstate)
{
  rp_state = newstate;
  while (rp_state_ack != newstate)
  {
    usleep(1000);
  }
}
#endif

void rp_send_command( struct rp_command *cmd )
{
#ifndef __SDL_WRAPPER__
  uint32 wsigs, gsigs;
  
  wsigs = (1L<<rp_replyport->mp_SigBit)|(1L<<rp_subtask_sig);
  
  IExec->PutMsg( rp_msgport, (struct Message *)rp_mainmsg );

  gsigs = IExec->Wait( wsigs );
  
  if( gsigs & (1L<<rp_replyport->mp_SigBit))
    IExec->GetMsg( rp_replyport );
  
  if( gsigs & (1L<<rp_subtask_sig) )
  {
    // If we got this signal, the subtask has
    // quit and we won't get a reply ever anyway. (== shit)
    quitting = TRUE;
  }
#else
  int i;
  switch (cmd->rpc_Command)
  {
    case RPC_CONT_POS:
    case RPC_PLAY_POS:
    case RPC_CONT_SONG:
    case RPC_PLAY_SONG:
      set_player_state(STS_IDLE);
      if( rp_playtune )
      {
        if( rp_playtune->at_doing == D_PLAYING )
          rp_playtune->at_doing = D_IDLE;
      }

      rp_playtune = cmd->rpc_Tune;
      if( rp_playtune != NULL )
      {
        rp_playtune->at_stopnextrow = 0;
        if( cmd->rpc_Command == RPC_PLAY_SONG )
          rp_playtune->at_NoteNr         = 0;

        rp_playtune->at_PosJumpNote    = 0;
        rp_playtune->at_StepWaitFrames = 0;
        rp_playtune->at_GetNewPosition = 1;
        rp_playtune->at_ticks = rp_playtune->at_secs = rp_playtune->at_mins = rp_playtune->at_hours = 0;
        rp_reset_some_shit( rp_playtune );
      }
      break;

    case RPC_PLAY_ROW:
      if( cmd->rpc_Tune != NULL )
      {
        if (cmd->rpc_Tune != rp_playtune)
          set_player_state(STS_IDLE);

        ;
        cmd->rpc_Tune->at_stopnextrow    = 3;
        cmd->rpc_Tune->at_GetNewPosition = 1;
        cmd->rpc_Tune->at_ticks = cmd->rpc_Tune->at_secs = cmd->rpc_Tune->at_mins = cmd->rpc_Tune->at_hours = 0;
        if( ( rp_playtune != cmd->rpc_Tune ) || ( rp_state != STS_PLAYROW ) )
        {
          rp_reset_some_shit( cmd->rpc_Tune );
          rp_playtune = cmd->rpc_Tune;
        }
        rp_state = STS_PLAYPOS;
      }
      break;

    case RPC_PLAY_NOTE:
      if( rp_playtune != cmd->rpc_Tune )
      {
        set_player_state(STS_IDLE);
        rp_playtune = cmd->rpc_Tune;
      }
      
      if( rp_playtune != NULL )
      {
        rp_play_instrument( rp_playtune, cmd->rpc_Data2>>8, cmd->rpc_Data, cmd->rpc_Data2&0xff );
        rp_state = STS_PLAYNOTE;
      }
      break;
    
    case RPC_STOP:
      set_player_state(STS_IDLE);
      if( rp_playtune )
      {
        for( i=0; i<MAX_CHANNELS; i++ )
        {
          rp_playtune->at_Voices[i].vc_TrackOn = 0;
          rp_playtune->at_Voices[i].vc_VUMeter = 0;
        }
        if( rp_playtune->at_NextPosNr != -1 )
        {
          SDL_Event     event;
          SDL_UserEvent userevent;

          rp_playtune->at_PosNr     = rp_playtune->at_NextPosNr;
          rp_playtune->at_NextPosNr = -1;

          userevent.type  = SDL_USEREVENT;
          userevent.code  = 0;
          userevent.data1 = NULL;
          userevent.data2 = NULL;
          
          event.type = SDL_USEREVENT;
          event.user = userevent;
          
          SDL_PushEvent( &event );
        }
        rp_playtune = NULL;
      }   
      break;
  }

  switch (cmd->rpc_Command)
  {
    case RPC_CONT_POS:
    case RPC_PLAY_POS:
      rp_state = STS_PLAYPOS;
      break;

    case RPC_CONT_SONG:
    case RPC_PLAY_SONG:
      if( rp_playtune )
      {
        rp_state = STS_PLAYSONG;
      } else {
        rp_state = STS_IDLE;
      }

      cmd->rpc_Tune = rp_playtune;
      rp_curtune = rp_playtune;
      break;
  }
#endif
}

BOOL rp_play_song( struct ahx_tune *at, uint32 subsong, BOOL cont )
{
  rp_mainmsg->rpc_Command = cont ? RPC_CONT_SONG : RPC_PLAY_SONG;
  rp_mainmsg->rpc_Tune    = at;
  rp_mainmsg->rpc_Data    = subsong;
  rp_mainmsg->rpc_Data2   = 0;

  rp_send_command( rp_mainmsg );
  
  return (rp_mainmsg->rpc_Tune == at);
}

BOOL rp_play_pos( struct ahx_tune *at, BOOL cont )
{
  rp_mainmsg->rpc_Command = cont ? RPC_CONT_POS : RPC_PLAY_POS;
  rp_mainmsg->rpc_Tune    = at;
  rp_mainmsg->rpc_Data    = 0;
  rp_mainmsg->rpc_Data2   = 0;

  rp_send_command( rp_mainmsg );
  
  return (rp_mainmsg->rpc_Tune == at);
}

BOOL rp_play_row( struct ahx_tune *at )
{
  rp_mainmsg->rpc_Command = RPC_PLAY_ROW;
  rp_mainmsg->rpc_Tune    = at;
  rp_mainmsg->rpc_Data    = 0;
  rp_mainmsg->rpc_Data2   = 0;

  rp_send_command( rp_mainmsg );
  
  return (rp_mainmsg->rpc_Tune == at);
}

BOOL rp_play_note( struct ahx_tune *at, int32 inst, int32 note, int32 channel )
{
  if( ( note < 1 ) || ( note > 60 ) )
    return TRUE;

  rp_mainmsg->rpc_Command = RPC_PLAY_NOTE;
  rp_mainmsg->rpc_Tune    = at;
  rp_mainmsg->rpc_Data    = inst;
  rp_mainmsg->rpc_Data2   = (channel<<8)|note;

  rp_send_command( rp_mainmsg );
  
  return (rp_mainmsg->rpc_Tune == at);
}

void rp_stop( void )
{
  rp_mainmsg->rpc_Command = RPC_STOP;
  rp_mainmsg->rpc_Tune    = NULL;
  rp_mainmsg->rpc_Data    = 0;
  rp_mainmsg->rpc_Data2   = 0;

  rp_send_command( rp_mainmsg );

#ifdef __SDL_WRAPPER__
  memset(rp_audiobuffer, 0, sizeof(rp_audiobuffer));
#endif
}

void rp_zap_tracks( struct ahx_tune * at )
{
  uint32 i, j;

  if( !at ) return;
  if( at == rp_curtune ) rp_stop();
  
  for( i=0; i<256; i++ )
    for( j=0; j<64; j++ )
    {
      at->at_Tracks[i][j].stp_Note       = 0;
      at->at_Tracks[i][j].stp_Instrument = 0;
      at->at_Tracks[i][j].stp_FX         = 0;
      at->at_Tracks[i][j].stp_FXParam    = 0;
      at->at_Tracks[i][j].stp_FXb        = 0;
      at->at_Tracks[i][j].stp_FXbParam   = 0;
    }  
}

void rp_zap_positions( struct ahx_tune *at )
{
  uint32 i, j;

  if( !at ) return;
  if( at == rp_curtune ) rp_stop();
  
  for( i=0; i<1000; i++ )
  {
    for( j=0; j<MAX_CHANNELS; j++ )
    {
      at->at_Positions[i].pos_Track[j]     = 0;
      at->at_Positions[i].pos_Transpose[j] = 0;
    }
  }

  for( i=0; i<256; i++ )
    at->at_Subsongs[i] = 0;
  
  at->at_SubsongNr = 0;
  at->at_curss     = 0;
  at->at_PosNr     = 0;
  at->at_Restart   = 0;
}

void rp_zap_instruments( struct ahx_tune *at )
{
  uint32 i;

  if( !at ) return;
  if( at == rp_curtune ) rp_stop();

  for( i=0; i<64; i++ )
    rp_clear_instrument( &at->at_Instruments[i] );
}
