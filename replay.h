
// Woohoo!
#define MAX_CHANNELS 16

// Some handy constants. Thanks eightbitbubsy.
#define AMIGA_PAL_XTAL            28375160
#define AMIGA_NTSC_XTAL           28636360
#define AMIGA_CPU_PAL_CLK         ((AMIGA_PAL_XTAL / 4))
#define AMIGA_CPU_NTSC_CLK        ((AMIGA_NTSC_XTAL / 4))
#define AMIGA_CIA_PAL_CLK         ((AMIGA_CPU_PAL_CLK / 10))
#define AMIGA_CIA_NTSC_CLK        ((AMIGA_CPU_NTSC_CLK / 10))
#define AMIGA_PAULA_PAL_CLK       ((AMIGA_CPU_PAL_CLK / 2))
#define AMIGA_PAULA_NTSC_CLK      ((AMIGA_CPU_NTSC_CLK / 2))

#define Period2Freq(period) ((AMIGA_PAULA_PAL_CLK * 65536.f) / (period))
//#define Period2Freq(period) (3579545.25f / (period))

struct ahx_envelope
{
  int16 aFrames, aVolume;
  int16 dFrames, dVolume;
  int16 sFrames;
  int16 rFrames, rVolume;
  int16 pad;
};

struct ahx_plsentry
{
  uint8 ple_Note;
  uint8 ple_Waveform;
  int16 ple_Fixed;
  int8  ple_FX[2];
  int8  ple_FXParam[2];
};

struct ahx_plist
{
  int16               pls_Speed;
  int16               pls_Length;
  struct ahx_plsentry pls_Entries[256];
};

struct ahx_instrument
{
  TEXT                ins_Name[128];
  uint8               ins_Volume;
  uint8               ins_WaveLength;
  uint8               ins_FilterLowerLimit;
  uint8               ins_FilterUpperLimit;
  uint8               ins_FilterSpeed;
  uint8               ins_SquareLowerLimit;
  uint8               ins_SquareUpperLimit;
  uint8               ins_SquareSpeed;
  uint8               ins_VibratoDelay;
  uint8               ins_VibratoSpeed;
  uint8               ins_VibratoDepth;
  uint8               ins_HardCutRelease;
  uint8               ins_HardCutReleaseFrames;
  struct ahx_envelope ins_Envelope;
  struct ahx_plist    ins_PList;
  int32               ins_ptop;
  int32               ins_pcurx;
  int32               ins_pcury;
};

struct ahx_position
{
  uint8 pos_Track[MAX_CHANNELS];
  int8  pos_Transpose[MAX_CHANNELS];
};

struct ahx_step
{
  uint8 stp_Note;
  uint8 stp_Instrument;
  uint8 stp_FX;
  uint8 stp_FXParam;
  uint8 stp_FXb;
  uint8 stp_FXbParam;
};

struct ahx_voice
{
  int16                  vc_Track;
  int16                  vc_NextTrack;
  int16                  vc_Transpose;
  int16                  vc_NextTranspose;
  int16                  vc_OverrideTranspose;
  int32                  vc_ADSRVolume;
  struct ahx_envelope    vc_ADSR;
  struct ahx_instrument *vc_Instrument;
  uint32                 vc_SamplePos;
  uint32                 vc_Delta;
  uint16                 vc_InstrPeriod;
  uint16                 vc_TrackPeriod;
  uint16                 vc_VibratoPeriod;
  uint16                 vc_WaveLength;
  int16                  vc_NoteMaxVolume;
  uint16                 vc_PerfSubVolume;
  uint8                  vc_NewWaveform;
  uint8                  vc_Waveform;
  uint8                  vc_PlantPeriod;
  uint8                  vc_VoiceVolume;
  uint8                  vc_PlantSquare;
  uint8                  vc_IgnoreSquare;
  uint8                  vc_FixedNote;
  int16                  vc_VolumeSlideUp;
  int16                  vc_VolumeSlideDown;
  int16                  vc_HardCut;
  uint8                  vc_HardCutRelease;
  int16                  vc_HardCutReleaseF;
  uint8                  vc_PeriodSlideOn;
  int16                  vc_PeriodSlideSpeed;
  int16                  vc_PeriodSlidePeriod;
  int16                  vc_PeriodSlideLimit;
  int16                  vc_PeriodSlideWithLimit;
  int16                  vc_PeriodPerfSlideSpeed;
  int16                  vc_PeriodPerfSlidePeriod;
  uint8                  vc_PeriodPerfSlideOn;
  int16                  vc_VibratoDelay;
  int16                  vc_VibratoSpeed;
  int16                  vc_VibratoCurrent;
  int16                  vc_VibratoDepth;
  int16                  vc_SquareOn;
  int16                  vc_SquareInit;
  int16                  vc_SquareWait;
  int16                  vc_SquareLowerLimit;
  int16                  vc_SquareUpperLimit;
  int16                  vc_SquarePos;
  int16                  vc_SquareSign;
  int16                  vc_SquareSlidingIn;
  int16                  vc_SquareReverse;
  uint8                  vc_FilterOn;
  uint8                  vc_FilterInit;
  int16                  vc_FilterWait;
  int16                  vc_FilterSpeed;
  int16                  vc_FilterUpperLimit;
  int16                  vc_FilterLowerLimit;
  int16                  vc_FilterPos;
  int16                  vc_FilterSign;
  int16                  vc_FilterSlidingIn;
  int16                  vc_IgnoreFilter;
  int16                  vc_PerfCurrent;
  int16                  vc_PerfSpeed;
  int16                  vc_PerfWait;
  struct ahx_plist      *vc_PerfList;
  int8                  *vc_AudioPointer;
  int8                  *vc_AudioSource;
  uint8                  vc_NoteDelayOn;
  uint8                  vc_NoteCutOn;
  int16                  vc_NoteDelayWait;
  int16                  vc_NoteCutWait;
  int16                  vc_AudioPeriod;
  int16                  vc_AudioVolume;
  int32                  vc_WNRandom;
  int8                  *vc_MixSource;
  int8                   vc_SquareTempBuffer[0x80];
  int8                   vc_VoiceBuffer[0x282*4];
  uint8                  vc_VoiceNum;
  uint8                  vc_TrackMasterVolume;
  uint8                  vc_TrackOn;
  uint8                  vc_SetTrackOn;
  int16                  vc_VoicePeriod;
  uint32                 vc_Pan;
  uint32                 vc_SetPan;
  uint32                 vc_PanMultLeft;
  uint32                 vc_PanMultRight;
  int32                  vc_VUMeter;

  /* Ring modulation! */
  uint32                 vc_RingSamplePos;
  uint32                 vc_RingDelta;
  int8                  *vc_RingMixSource;
  uint8                  vc_RingPlantPeriod;
  int16                  vc_RingInstrPeriod;
  int16                  vc_RingBasePeriod;
  int16                  vc_RingAudioPeriod;
  int8                  *vc_RingAudioSource;
  uint8                  vc_RingNewWaveform;
  uint8                  vc_RingWaveform;
  uint8                  vc_RingFixedPeriod;
  int8                   vc_RingVoiceBuffer[0x282*4];
};

// Store all tunes in a list so we can have more than
// one loaded at once (= tabs!)
struct ahx_tune
{
  struct Node            at_ln;

  struct List           *at_undolist;
  struct List           *at_redolist;
  uint32                 at_undomem;

/****
***** Anything that can't just be copied from one ahx_tune structure
***** into another with a single CopyMem call should be above at_Name
****/

  TEXT                   at_Name[128];
  uint32                 at_Time;
  uint32                 at_ExactTime;
  uint16                 at_LoopDetector;
  uint16                 at_SongNum;
  uint32                 at_Frequency;
  float64                at_FreqF;
  int8                  *at_WaveformTab[MAX_CHANNELS];
  uint16                 at_Restart;
  uint16                 at_PositionNr;
  uint8                  at_SpeedMultiplier;
  uint8                  at_TrackLength;
  uint8                  at_TrackNr;
  uint8                  at_InstrumentNr;
  uint8                  at_SubsongNr;
  uint16                 at_PosJump;
  uint32                 at_PlayingTime;
  int16                  at_Tempo;
  int16                  at_PosNr;
  int16                  at_NextPosNr;
  uint16                 at_StepWaitFrames;
  int16                  at_NoteNr;
  uint16                 at_PosJumpNote;
  uint8                  at_GetNewPosition;
  uint8                  at_PatternBreak;
  uint8                  at_SongEndReached;
  uint8                  at_Stereo;
  uint16                 at_Subsongs[256];
  uint16                 at_Channels;
  struct ahx_position    at_Positions[1000];
  struct ahx_step        at_Tracks[256][64];
  struct ahx_instrument  at_Instruments[64];
  struct ahx_voice       at_Voices[MAX_CHANNELS];
  int32                  at_posed_curs;
  int32                  at_tracked_curs;
  int32                  at_curins;
  int32                  at_drumpadnote;
  BOOL                   at_drumpadmode;
  int16                  at_topins;
  int16                  at_topinsb;
  int32                  at_curss;
  int32                  at_curlch;
  int32                  at_baseins;
  int32                  at_curpanel;
  BOOL                   at_modified;
  int32                  at_mixgain;
  int32                  at_mixgainP;

  // Tracked mark
  int16                  at_cbmarkmarknote;
  int16                  at_cbmarkmarkx;
  int16                  at_cbmarkcurnote;
  int16                  at_cbmarkcurx;
  int16                  at_cbmarktrack;
  int16                  at_cbmarkstartnote;
  int16                  at_cbmarkendnote;
  int16                  at_cbmarkleftx;
  int16                  at_cbmarkrightx;
  int16                  at_cbmarkbits;

  // Posed mark
  int16                  at_cbpmarkmarkpos;
  int16                  at_cbpmarkmarklft;
  int16                  at_cbpmarklft;
  int16                  at_cbpmarktop;
  int16                  at_cbpmarkrgt;
  int16                  at_cbpmarkbot;
  int16                  at_cbpmarklftcol;
  int16                  at_cbpmarkrgtcol;

  int16                  at_notejump;
  int16                  at_inotejump;
  int16                  at_rempos[5];
  int32                  at_defstereo;
  int32                  at_defpanleft;
  int32                  at_defpanright;
  uint8                  at_ticks;
  uint8                  at_secs;
  uint8                  at_mins;
  uint8                  at_hours;

  int32                  at_doing;
  int32                  at_editing;
  int32                  at_idoing;

  // Undo stuff
  struct ahx_plsentry    at_rem_pls_Entries[256];
  TEXT                   at_rem_string[128];
  struct ahx_step        at_rem_track[64];
  uint8                  at_rem_posbuf[1000*MAX_CHANNELS*2];
  int32                  at_rem_pbleft, at_rem_pbpos;
  int32                  at_rem_pbchans, at_rem_pbrows;

  uint8                  at_stopnextrow;
};

enum {
  RPC_STOP,
  RPC_PLAY_SONG,
  RPC_PLAY_POS,
  RPC_PLAY_NOTE,
  RPC_CONT_SONG,
  RPC_CONT_POS,
  RPC_PLAY_ROW
};

struct rp_command
{
#ifndef __SDL_WRAPPER__
  struct Message   rpc_Message;
#endif
  uint16           rpc_Command;
  uint16           rpc_Data;
  uint16           rpc_Data2;
  struct ahx_tune *rpc_Tune;
};

struct ahx_tune *rp_load_tune( const TEXT *name, struct ahx_tune *at );
BOOL rp_init( void );
void rp_shutdown( void );
void rp_handler( uint32 gotsigs );
void rp_stop( void );
struct ahx_tune *rp_new_tune( BOOL addtolist );
void rp_free_tune( struct ahx_tune *at );
void rp_clear_tune( struct ahx_tune *at );
BOOL rp_play_note( struct ahx_tune *at, int32 inst, int32 note, int32 channel );
void rp_load_ins( const TEXT *name, struct ahx_tune *at, int32 in );
void rp_save_ins( const TEXT *name, struct ahx_tune *at, int32 in );
BOOL rp_play_song( struct ahx_tune *at, uint32 subsong, BOOL cont );
BOOL rp_play_pos( struct ahx_tune *at, BOOL cont );
BOOL rp_init_subsong( struct ahx_tune *at, uint32 nr );
uint32 rp_ahx_test( const struct ahx_tune *at );
void rp_save_ahx( const TEXT *name, struct ahx_tune *at );
void rp_save_hvl( const TEXT *name, struct ahx_tune *at );
int32 rp_find_loudest( struct ahx_tune *at );
void rp_clear_instrument( struct ahx_instrument *ins );
void rp_zap_tracks( struct ahx_tune * at );
void rp_zap_positions( struct ahx_tune *at );
void rp_zap_instruments( struct ahx_tune *at );
BOOL rp_play_row( struct ahx_tune *at );

#define SWB_DOUBLECMD 0
#define SWF_DOUBLECMD (1L<<SWB_DOUBLECMD)
#define SWB_NEWINSCMD 1
#define SWF_NEWINSCMD (1L<<SWB_NEWINSCMD)
#define SWB_MANYCHANS 2
#define SWF_MANYCHANS (1L<<SWB_MANYCHANS)
#define SWB_PANCMD 3
#define SWF_PANCMD (1L<<SWB_PANCMD)
#define SWB_EFXCMD 4
#define SWF_EFXCMD (1L<<SWB_EFXCMD)
