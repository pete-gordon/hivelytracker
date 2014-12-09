
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <system_includes.h>

#include "util.h"
#include "replay.h"
#include "gui.h"
#include "undo.h"

extern struct ahx_tune       *curtune;
extern struct textbox         tbx[];
extern int32                  pref_maxundobuf;

extern struct ahx_instrument *perf_lastinst;
extern int16                  insls_lastcuri;
extern int16                  inslsb_lastcuri;
extern int16                  posed_lastposnr;
extern int16                  trked_lastposnr;
extern struct rawbm mainbm;

struct textbox *show_tbox;
uint32 mubsizes[] = { 0, 262144, 524288, 1048576, 2097152, 4194304 };

/*****************************************************/

void free_undolists( struct ahx_tune *at )
{
  struct undonode *tn, *nn;

  // Clear undo system lists  
  tn = (struct undonode *)IExec->GetHead(at->at_undolist);
  while( tn )
  {
    nn = (struct undonode *)IExec->GetSucc(&tn->un_ln);
    IExec->Remove( (struct Node *)tn );
    IExec->FreeSysObject( ASOT_NODE, tn );
    tn = nn;
  }

  tn = (struct undonode *)IExec->GetHead(at->at_redolist);
  while( tn )
  {
    nn = (struct undonode *)IExec->GetSucc(&tn->un_ln);
    IExec->Remove( (struct Node *)tn );
    IExec->FreeSysObject( ASOT_NODE, tn );
    tn = nn;
  }

  at->at_undomem = 0;  
}

void clear_redolist( struct ahx_tune *at )
{
  struct undonode *tn, *nn;
  
  tn = (struct undonode *)IExec->GetHead(at->at_redolist);
  while( tn )
  {
    nn = (struct undonode *)IExec->GetSucc(&tn->un_ln);
    at->at_undomem -= tn->un_size;
    IExec->Remove( (struct Node *)tn );
    IExec->FreeSysObject( ASOT_NODE, tn );
    tn = nn;
  }
}

void trim_undolist( struct ahx_tune *at )
{
  struct undonode *un;

  if( mubsizes[pref_maxundobuf] == 0 ) return;

  while( at->at_undomem > mubsizes[pref_maxundobuf] )
  {
    un = (struct undonode *)IExec->RemHead( at->at_undolist );
    if( !un )
    {
      // WTF?
      clear_redolist( at );
      at->at_undomem = 0;
      return;
    }
  
    at->at_undomem -= un->un_size;
    IExec->FreeSysObject( ASOT_NODE, un );
  }
}

struct undonode *alloc_undonode( struct ahx_tune *at, uint32 size )
{
  struct undonode *un;

  un = (struct undonode *)allocnode(size);
  if( !un ) return NULL;

  un->un_ln.ln_Succ = NULL;
  un->un_ln.ln_Pred = NULL;
  un->un_size = size;
  at->at_undomem += size;
  trim_undolist( at );
  return un;  
}

/*****************************************************/

void setbefore_posregion( struct ahx_tune *at, int32 left, int32 pos, int32 chans, int32 rows )
{
  int32 size;
  int32 x, y, i;
  
  size = chans*rows*2;
  
  at->at_rem_pbleft  = left;
  at->at_rem_pbpos   = pos;
  at->at_rem_pbchans = chans;
  at->at_rem_pbrows  = rows;
  
  i=0;
  for( y=pos; y<(pos+rows); y++ )
  {
    for( x=left; x<(left+chans); x++ )
    {
      at->at_rem_posbuf[i++] = at->at_Positions[y].pos_Track[x];
      at->at_rem_posbuf[i++] = at->at_Positions[y].pos_Transpose[x];
    }
  } 
}

void setafter_posregion( struct ahx_tune *at, int32 left, int32 pos, int32 chans, int32 rows )
{
  struct undonode *un;
  struct udat_pos_region *ud;
  int32 x,y,i,size;

  // Ensure we're talking about the same region
  if( ( left  != at->at_rem_pbleft ) ||
      ( pos   != at->at_rem_pbpos ) ||
      ( chans != at->at_rem_pbchans ) ||
      ( rows  != at->at_rem_pbrows ) )
    return;

  size = chans*rows*2;

  un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_pos_region )+(size*2) );
  if( !un ) return;

  ud         = (struct udat_pos_region *)&un[1];
  ud->before = (uint8 *)&ud[1];
  ud->after  = (uint8 *)&ud->before[size];
  
  un->un_type       = UNT_POS_REGION;
  un->un_data       = ud;
  ud->left          = left;
  ud->pos           = pos;
  ud->chans         = chans;
  ud->rows          = rows;
  ud->posnr         = at->at_PosNr;
  ud->posed_curs    = at->at_posed_curs;
  ud->curlch        = at->at_curlch;
  
  i=0;
  for( y=pos; y<(pos+rows); y++ )
  {
    for( x=left; x<(left+chans); x++ )
    {
      ud->before[i]  = at->at_rem_posbuf[i];
      ud->after[i++] = at->at_Positions[y].pos_Track[x];
      ud->before[i]  = at->at_rem_posbuf[i];
      ud->after[i++] = at->at_Positions[y].pos_Transpose[x];
    }
  }

  IExec->AddTail( at->at_undolist, (struct Node *)un );
  clear_redolist( at );  
}

void setbefore_track( struct ahx_tune *at, int32 trk )
{
  int32 i;

  for( i=0; i<64; i++ )
    at->at_rem_track[i] = at->at_Tracks[trk][i];
}

void setafter_track( struct ahx_tune *at, int32 trk )
{
  struct undonode *un;
  struct udat_track *ud;
  int32 i;

  un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_track ) );
  if( !un ) return;

  ud = (struct udat_track *)&un[1];
  un->un_ln.ln_Succ = NULL;
  un->un_ln.ln_Pred = NULL;
  un->un_type       = UNT_TRACK;
  un->un_data       = ud;
  ud->track         = trk;
  ud->posnr         = at->at_PosNr;
  ud->notenr        = at->at_NoteNr;
  ud->tracked_curs  = at->at_tracked_curs;
  ud->curlch        = at->at_curlch;

  for( i=0; i<64; i++ )
  {
    ud->before[i] = at->at_rem_track[i];
    ud->after[i]  = at->at_Tracks[trk][i];
  }

  IExec->AddTail( at->at_undolist, (struct Node *)un );
  clear_redolist( at );
}

void setbefore_string( struct ahx_tune *at, TEXT *ptr )
{
  if( ptr == NULL ) return;
  strcpy( at->at_rem_string, ptr );
}

void setafter_string( int32 which, struct ahx_tune *at, TEXT *ptr )
{
  struct undonode *un;
  struct udat_string *ud;

  if( ptr == NULL ) return;

  un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_string )+strlen( ptr )+strlen( at->at_rem_string )+2 );
  if( !un ) return;

  ud = (struct udat_string *)&un[1];
  ud->before = (TEXT *)&ud[1];
  ud->after  = &ud->before[strlen(at->at_rem_string)+1];

  un->un_ln.ln_Succ = NULL;
  un->un_ln.ln_Pred = NULL;
  un->un_type       = which;
  un->un_data       = ud;
  ud->ptr           = ptr;

  strcpy( ud->before, at->at_rem_string );
  strcpy( ud->after,  ptr );

  switch( which )
  {
    case UNT_STRING_INSNAME:
      ud->extra = at->at_curins;
      break;
  }

  IExec->AddTail( at->at_undolist, (struct Node *)un );
  clear_redolist( at );
}

void setbefore_plist( struct ahx_tune *at, struct ahx_plsentry *ptr )
{
  int32 i;
  
  for( i=0; i<256; i++ )
    at->at_rem_pls_Entries[i] = ptr[i];
}

void setafter_plist( struct ahx_tune *at, struct ahx_plsentry *ptr )
{
  struct undonode *un;
  struct udat_whole_plist *ud;
  int32 i;

  un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_whole_plist ) );
  if( !un ) return;

  ud = (struct udat_whole_plist *)&un[1];
  un->un_ln.ln_Succ = NULL;
  un->un_ln.ln_Pred = NULL;
  un->un_type       = UNT_PLE;
  un->un_data       = ud;
  ud->ptr           = ptr;
  ud->insnum        = at->at_curins;

  for( i=0; i<256; i++ )
  {
    ud->before[i] = at->at_rem_pls_Entries[i];
    ud->after[i]  = ptr[i];
  }

  IExec->AddTail( at->at_undolist, (struct Node *)un );
  clear_redolist( at );
}

/*****************************************************/

// Simple modifications
void modify_env_w( struct ahx_tune *at, struct ahx_envelope *ptr, uint32 field, int16 new )
{
  struct undonode *un;
  struct udat_env_w *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_env_w *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_env_w *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_env_w ) );
    if( un )
    {
      ud = (struct udat_env_w *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->insnum        = at->at_curins;
      ud->after         = new;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_ENV_AFRAMES:
      if( rembef ) ud->before = ptr->aFrames;
      ptr->aFrames = new;
      break;
    
    case UNT_ENV_AVOLUME:
      if( rembef ) ud->before = ptr->aVolume;
      ptr->aVolume = new;
      break;
    
    case UNT_ENV_DFRAMES:
      if( rembef ) ud->before = ptr->dFrames;
      ptr->dFrames = new;
      break;
    
    case UNT_ENV_DVOLUME:
      if( rembef ) ud->before = ptr->dVolume;
      ptr->dVolume = new;
      break;
    
    case UNT_ENV_SFRAMES:
      if( rembef ) ud->before = ptr->sFrames;
      ptr->sFrames = new;
      break;
    
    case UNT_ENV_RFRAMES:
      if( rembef ) ud->before = ptr->rFrames;
      ptr->rFrames = new;
      break;
    
    case UNT_ENV_RVOLUME:
      if( rembef ) ud->before = ptr->rVolume;
      ptr->rVolume = new;
      break;
  }
}

void modify_ins_b( struct ahx_tune *at, struct ahx_instrument *ptr, uint32 field, uint8 new )
{
  struct undonode *un;
  struct udat_ins_b *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_ins_b *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_ins_b *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_ins_b ) );
    if( un )
    {
      ud = (struct udat_ins_b *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->insnum        = at->at_curins;
      ud->after         = new;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_INS_VOLUME:
      if( rembef ) ud->before = ptr->ins_Volume;
      ptr->ins_Volume = new;
      break;

    case UNT_INS_WAVELENGTH:
      if( rembef ) ud->before = ptr->ins_WaveLength;
      ptr->ins_WaveLength = new;
      break;

    case UNT_INS_FILTERLOWERLIMIT:
      if( rembef ) ud->before = ptr->ins_FilterLowerLimit;
      ptr->ins_FilterLowerLimit = new;
      break;
    
    case UNT_INS_FILTERUPPERLIMIT:
      if( rembef ) ud->before = ptr->ins_FilterUpperLimit;
      ptr->ins_FilterUpperLimit = new;
      break;
    
    case UNT_INS_FILTERSPEED:
      if( rembef ) ud->before = ptr->ins_FilterSpeed;
      ptr->ins_FilterSpeed = new;
      break;

    case UNT_INS_SQUARELOWERLIMIT:
      if( rembef ) ud->before = ptr->ins_SquareLowerLimit;
      ptr->ins_SquareLowerLimit = new;
      break;
    
    case UNT_INS_SQUAREUPPERLIMIT:
      if( rembef ) ud->before = ptr->ins_SquareUpperLimit;
      ptr->ins_SquareUpperLimit = new;
      break;
    
    case UNT_INS_SQUARESPEED:
      if( rembef ) ud->before = ptr->ins_SquareSpeed;
      ptr->ins_SquareSpeed = new;
      break;

    case UNT_INS_VIBRATODELAY:
      if( rembef ) ud->before = ptr->ins_VibratoDelay;
      ptr->ins_VibratoDelay = new;
      break;
    
    case UNT_INS_VIBRATOSPEED:
      if( rembef ) ud->before = ptr->ins_VibratoSpeed;
      ptr->ins_VibratoSpeed = new;
      break;

    case UNT_INS_VIBRATODEPTH:
      if( rembef ) ud->before = ptr->ins_VibratoDepth;
      ptr->ins_VibratoDepth = new;
      break;
    
    case UNT_INS_HARDCUTRELEASE:
      if( rembef ) ud->before = ptr->ins_HardCutRelease;
      ptr->ins_HardCutRelease = new;
      break;

    case UNT_INS_HARDCUTRELEASEFRAMES:
      if( rembef ) ud->before = ptr->ins_HardCutReleaseFrames;
      ptr->ins_HardCutReleaseFrames = new;
      break;
  }
}

void modify_ple_b( struct ahx_tune *at, struct ahx_instrument *ins, struct ahx_plsentry *ptr, uint32 field, int8 new )
{
  struct undonode *un;
  struct udat_ple_b *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_ple_b *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_ple_b *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_ple_b ) );
    if( un )
    {
      ud = (struct udat_ple_b *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->pins          = ins;
      ud->insnum        = at->at_curins;
      ud->after         = new;
      ud->ptop          = ins->ins_ptop;
      ud->pcurx         = ins->ins_pcurx;
      ud->pcury         = ins->ins_pcury;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_PLE_NOTE:
      if( rembef ) ud->before = ptr->ple_Note;
      ptr->ple_Note = new;
      break;
    
    case UNT_PLE_WAVEFORM:
      if( rembef ) ud->before = ptr->ple_Waveform;
      ptr->ple_Waveform = new;
      break;
      
    case UNT_PLE_FX0:
      if( rembef ) ud->before = ptr->ple_FX[0];
      ptr->ple_FX[0] = new;
      break;

    case UNT_PLE_FX1:
      if( rembef ) ud->before = ptr->ple_FX[1];
      ptr->ple_FX[1] = new;
      break;

    case UNT_PLE_FXPARAM0:
      if( rembef ) ud->before = ptr->ple_FXParam[0];
      ptr->ple_FXParam[0] = new;
      break;

    case UNT_PLE_FXPARAM1:
      if( rembef ) ud->before = ptr->ple_FXParam[1];
      ptr->ple_FXParam[1] = new;
      break;
  }
}

void modify_ple_w( struct ahx_tune *at, struct ahx_instrument *ins, struct ahx_plsentry *ptr, uint32 field, int16 new )
{
  struct undonode *un;
  struct udat_ple_w *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_ple_w *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_ple_w *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_ple_w ) );
    if( un )
    {
      ud = (struct udat_ple_w *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->pins          = ins;
      ud->insnum        = at->at_curins;
      ud->after         = new;
      ud->ptop          = ins->ins_ptop;
      ud->pcurx         = ins->ins_pcurx;
      ud->pcury         = ins->ins_pcury;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_PLE_FIXED:
      if( rembef ) ud->before = ptr->ple_Fixed;
      ptr->ple_Fixed = new;
      break;    
  }
}

void modify_pls_w( struct ahx_tune *at, struct ahx_plist *ptr, uint32 field, int16 new )
{
  struct undonode *un;
  struct udat_pls_w *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_pls_w *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_pls_w *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_pls_w ) );
    if( un )
    {
      ud = (struct udat_pls_w *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->insnum        = at->at_curins;
      ud->after         = new;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_PLS_SPEED:
      if( rembef ) ud->before = ptr->pls_Speed;
      ptr->pls_Speed = new;
      break;

    case UNT_PLS_LENGTH:
      if( rembef ) ud->before = ptr->pls_Length;
      ptr->pls_Length = new;
      break;
  }
}

void modify_stp_b( struct ahx_tune *at, struct ahx_step *ptr, uint32 field, int8 new )
{
  struct undonode *un;
  struct udat_stp_b *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_stp_b *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_stp_b *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_stp_b ) );
    if( un )
    {
      ud = (struct udat_stp_b *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->after         = new;
      ud->posnr         = at->at_PosNr;
      ud->notenr         = at->at_NoteNr;
      ud->tracked_curs  = at->at_tracked_curs;
      ud->curlch        = at->at_curlch;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_STP_NOTE:
      if( rembef ) ud->before = ptr->stp_Note;
      ptr->stp_Note = new;
      break;
    
    case UNT_STP_INSTRUMENT:
      if( rembef ) ud->before = ptr->stp_Instrument;
      ptr->stp_Instrument = new;
      break;
      
    case UNT_STP_FX:
      if( rembef ) ud->before = ptr->stp_FX;
      ptr->stp_FX = new;
      break;

    case UNT_STP_FXB:
      if( rembef ) ud->before = ptr->stp_FXb;
      ptr->stp_FXb = new;
      break;

    case UNT_STP_FXPARAM:
      if( rembef ) ud->before = ptr->stp_FXParam;
      ptr->stp_FXParam = new;
      break;

    case UNT_STP_FXBPARAM:
      if( rembef ) ud->before = ptr->stp_FXbParam;
      ptr->stp_FXbParam = new;
      break;
  }
}

void modify_stp_w( struct ahx_tune *at, struct ahx_step *ptr, uint32 field, int16 new )
{
  struct undonode *un;
  struct udat_stp_w *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_stp_w *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_stp_w *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_stp_w ) );
    if( un )
    {
      ud = (struct udat_stp_w *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->after         = new;
      ud->posnr         = at->at_PosNr;
      ud->notenr         = at->at_NoteNr;
      ud->tracked_curs  = at->at_tracked_curs;
      ud->curlch        = at->at_curlch;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_STP_NOTEANDINS:
      if( rembef ) ud->before = (ptr->stp_Note<<8)|(ptr->stp_Instrument);
      ptr->stp_Note = new>>8;
      ptr->stp_Instrument = new & 0xff;
      break;    

    case UNT_STP_FXANDPARAM:
      if( rembef ) ud->before = (ptr->stp_FX<<8)|(ptr->stp_FXParam);
      ptr->stp_FX = new>>8;
      ptr->stp_FXParam = new & 0xff;
      break;    

    case UNT_STP_FXBANDPARAM:
      if( rembef ) ud->before = (ptr->stp_FXb<<8)|(ptr->stp_FXbParam);
      ptr->stp_FXb = new>>8;
      ptr->stp_FXbParam = new & 0xff;
      break;    
  }
}

void modify_pos_b( struct ahx_tune *at, struct ahx_position *ptr, int32 chan, uint32 field, int8 new )
{
  struct undonode *un;
  struct udat_pos_b *ud = NULL;
  BOOL rembef;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) &&
      ( ((struct udat_pos_b *)un->un_data)->chan == chan ) &&
      ( ((struct udat_pos_b *)un->un_data)->ptr == ptr ) )
  {
    ud = (struct udat_pos_b *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_pos_b ) );
    if( un )
    {
      ud = (struct udat_pos_b *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->ptr           = ptr;
      ud->chan          = chan;
      ud->after         = new;
      ud->posnr         = at->at_PosNr;
      ud->posed_curs    = at->at_posed_curs;
      ud->curlch        = at->at_curlch;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_POS_TRACK:
      if( rembef ) ud->before = ptr->pos_Track[chan];
      ptr->pos_Track[chan] = new;
      break;

    case UNT_POS_TRANS:
      if( rembef ) ud->before = ptr->pos_Transpose[chan];
      ptr->pos_Transpose[chan] = new;
      break;
  }
}

void modify_tune_b( struct ahx_tune *at, uint32 field, int8 new )
{
  struct undonode *un;
  struct udat_tune_b *ud = NULL;
  BOOL rembef;

  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) )
  {
    ud = (struct udat_tune_b *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_tune_b ) );
    if( un )
    {
      ud = (struct udat_tune_b *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->after         = new;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_TRACKLEN:
      if( rembef ) ud->before = at->at_TrackLength;
      at->at_TrackLength = new;
      break;

    case UNT_CHANNELS:
      if( rembef ) ud->before = at->at_Channels;
      at->at_Channels = new;
      break;

    case UNT_MIXGAIN:
      if( rembef ) ud->before = at->at_mixgainP;
      at->at_mixgainP = new;
      break;

    case UNT_SPMUL:
      if( rembef ) ud->before = at->at_SpeedMultiplier;
      at->at_SpeedMultiplier = new;
      break;
  }
}

void modify_tune_w( struct ahx_tune *at, uint32 field, int16 new )
{
  struct undonode *un;
  struct udat_tune_w *ud = NULL;
  BOOL rembef;

  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == field ) )
  {
    ud = (struct udat_tune_w *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_tune_w ) );
    if( un )
    {
      ud = (struct udat_tune_w *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = field;
      un->un_data       = ud;
      ud->after         = new;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }

  switch( field )
  {
    case UNT_POSITIONNR:
      if( rembef ) ud->before = at->at_PositionNr;
      at->at_PositionNr = new;
      break;

    case UNT_RESTART:
      if( rembef ) ud->before = at->at_Restart;
      at->at_Restart = new;
      break;

    case UNT_SUBSONGS:
      if( rembef ) ud->before = at->at_SubsongNr;
      at->at_SubsongNr = new;
      break;
  }
}

void modify_sspos( struct ahx_tune *at, int16 new )
{
  struct undonode *un;
  struct udat_subsongpos *ud;
  BOOL rembef;
  
  if( at->at_curss == 0 ) return;
  
  rembef = FALSE;
  
  // Check for strings of the same action
  un = (struct undonode *)IExec->GetTail( at->at_undolist );
  if( ( un != NULL ) &&
      ( un->un_type == UNT_SSPOS ) &&
      ( ((struct udat_subsongpos *)un->un_data)->subsong == at->at_curss ) )
  {
    ud = (struct udat_subsongpos *)un->un_data;
    ud->after = new;
  } else {
    un = alloc_undonode( at, sizeof( struct undonode )+sizeof( struct udat_subsongpos ) );
    if( un )
    {
      ud = (struct udat_subsongpos *)&un[1];
      un->un_ln.ln_Succ = NULL;
      un->un_ln.ln_Pred = NULL;
      un->un_type       = UNT_SSPOS;
      un->un_data       = ud;
      ud->after         = new;
      ud->subsong       = at->at_curss;

      IExec->AddTail( at->at_undolist, (struct Node *)un );
      clear_redolist( at );
      rembef = TRUE;
    }
  }
  
  if( rembef ) ud->before = at->at_Subsongs[at->at_curss-1];
  at->at_Subsongs[at->at_curss-1] = new;
}

/*****************************************************/

void show_changed( struct ahx_tune *at, int32 wpanel, BOOL forceall )
{
  if( at != curtune ) return;

  if( wpanel == -1 ) return;

  if( wpanel != at->at_curpanel )
  {
    // Switch panel and render whole thing
    at->at_curpanel = wpanel;
    gui_render_tunepanel( TRUE );
    return;
  }

  // Try and be semi-smart about re-rendering parts of the panel
  switch( wpanel )
  {
    case PN_TRACKER:
      gui_render_tracker( forceall );
      gui_render_inslist( forceall );
      break;

    case PN_INSED:
      gui_render_perf( at, &at->at_Instruments[at->at_curins], forceall );
      gui_render_inslistb( forceall );
      break;
  }    

  if( show_tbox )
  {
    gui_render_tbox( &mainbm, show_tbox );
    gui_render_tabs();
  }

  gui_set_various_things( at );
}

void undo( struct ahx_tune *at )
{
  struct undonode         *un;
  struct udat_env_w       *env_w;
  struct udat_whole_plist *whole_plist;
  struct udat_ple_b       *ple_b;
  struct udat_ple_w       *ple_w;
  struct udat_pls_w       *pls_w;
  struct udat_ins_b       *ins_b;
  struct udat_track       *rtrk;
  struct udat_stp_b       *stp_b;
  struct udat_stp_w       *stp_w;
  struct udat_string      *rstr;
  struct udat_pos_b       *pos_b;
  struct udat_pos_region  *pos_r;
  int32 i, wpanel, x, y;
  BOOL forceall = FALSE;

  wpanel = -1;
  show_tbox = NULL;

  // Get last thing in the undo list
  un = (struct undonode *)IExec->RemTail( at->at_undolist );
  if( !un ) return;

  // Undo it!
  switch( un->un_type )
  {
    case UNT_POSITIONNR:
      at->at_PositionNr = ((struct udat_tune_w *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_RESTART:
      at->at_Restart = ((struct udat_tune_w *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_TRACKLEN:
      at->at_TrackLength = ((struct udat_tune_b *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_SUBSONGS:
      at->at_SubsongNr = ((struct udat_tune_w *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_SSPOS:
      at->at_curss = ((struct udat_subsongpos *)un->un_data)->subsong;
      at->at_Subsongs[at->at_curss-1] = ((struct udat_tune_b *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_CHANNELS:
      at->at_Channels = ((struct udat_tune_b *)un->un_data)->before;
      at->at_curlch = 0;
      wpanel = PN_TRACKER;
      forceall = TRUE;
      break;

    case UNT_MIXGAIN:
      at->at_mixgainP = ((struct udat_tune_b *)un->un_data)->before;
      at->at_mixgain = (at->at_mixgainP*256)/100;
      wpanel = PN_TRACKER;
      break;

    case UNT_SPMUL:
      at->at_SpeedMultiplier = ((struct udat_tune_b *)un->un_data)->before;
      wpanel = PN_TRACKER;
      break;

    case UNT_ENV_AFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aFrames = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_AVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aVolume = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_DFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->dFrames = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_DVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aVolume = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_SFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->sFrames = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_RFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->rFrames = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_RVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->rVolume = env_w->before;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_PLE:
      whole_plist = (struct udat_whole_plist *)un->un_data;
      for( i=0; i<256; i++ ) whole_plist->ptr[i] = whole_plist->before[i];
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = whole_plist->insnum;
      break;

    case UNT_PLE_NOTE:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_Note = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_WAVEFORM:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_Waveform = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FIXED:
      ple_w = (struct udat_ple_w *)un->un_data;
      ple_w->ptr->ple_Fixed = ple_w->before;
      ple_w->pins->ins_ptop  = ple_w->ptop;
      ple_w->pins->ins_pcurx = ple_w->pcurx;
      ple_w->pins->ins_pcury = ple_w->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_w->insnum;
      break;

    case UNT_PLE_FX0:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FX[0] = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FX1:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FX[1] = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FXPARAM0:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FXParam[0] = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FXPARAM1:  
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FXParam[1] = ple_b->before;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLS_SPEED:
      pls_w = (struct udat_pls_w *)un->un_data;
      pls_w->ptr->pls_Speed = pls_w->before;
      wpanel = PN_INSED;
      at->at_curins = pls_w->insnum;
      break;

    case UNT_PLS_LENGTH:
      pls_w = (struct udat_pls_w *)un->un_data;
      pls_w->ptr->pls_Length = pls_w->before;
      wpanel = PN_INSED;
      at->at_curins = pls_w->insnum;
      break;
  
    case UNT_INS_VOLUME:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_Volume = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_WAVELENGTH:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_WaveLength = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERLOWERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterLowerLimit = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERUPPERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterUpperLimit = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERSPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterSpeed = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUARELOWERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareLowerLimit = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUAREUPPERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareUpperLimit = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUARESPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareSpeed = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATODELAY:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoDelay = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATOSPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoSpeed = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATODEPTH:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoDepth = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_HARDCUTRELEASE:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_HardCutRelease = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_HARDCUTRELEASEFRAMES:    
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_HardCutReleaseFrames = ins_b->before;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;
    
    case UNT_STRING_INSNAME:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->before );
      show_tbox = &tbx[TB_INSNAME];
      wpanel = PN_INSED;
      at->at_curins = rstr->extra;
      inslsb_lastcuri = -1;
      break;
    
    case UNT_STRING_INSNAME2:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->before );
      wpanel = PN_TRACKER;
      insls_lastcuri = -1;
      break;
    
    case UNT_STRING_SONGNAME:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->before );
      show_tbox = &tbx[TB_SONGNAME];
      wpanel = PN_TRACKER;
      break;
      
    case UNT_TRACK:
      rtrk = (struct udat_track *)un->un_data;
      for( i=0; i<64; i++ )
        at->at_Tracks[rtrk->track][i] = rtrk->before[i];
      at->at_PosNr        = rtrk->posnr;
      at->at_NoteNr       = rtrk->notenr;
      at->at_tracked_curs = rtrk->tracked_curs;
      at->at_curlch       = rtrk->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_NOTEANDINS:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_Note = (stp_w->before>>8);
      stp_w->ptr->stp_Instrument = stp_w->before & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXANDPARAM:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_FX      = (stp_w->before>>8);
      stp_w->ptr->stp_FXParam = stp_w->before & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXBANDPARAM:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_FXb      = (stp_w->before>>8);
      stp_w->ptr->stp_FXbParam = stp_w->before & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_NOTE:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_Note = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_INSTRUMENT:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_Instrument = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FX:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FX = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXPARAM:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXParam = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXB:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXb = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXBPARAM:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXbParam = stp_b->before;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_TRACK:
      pos_b = (struct udat_pos_b *)un->un_data;
      pos_b->ptr->pos_Track[pos_b->chan] = pos_b->before;
      at->at_PosNr        = pos_b->posnr;
      at->at_posed_curs   = pos_b->posed_curs;
      at->at_curlch       = pos_b->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_TRANS:
      pos_b = (struct udat_pos_b *)un->un_data;
      pos_b->ptr->pos_Transpose[pos_b->chan] = pos_b->before;
      at->at_PosNr        = pos_b->posnr;
      at->at_posed_curs   = pos_b->posed_curs;
      at->at_curlch       = pos_b->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_REGION:
      pos_r = (struct udat_pos_region *)un->un_data;

      i=0;
      for( y=pos_r->pos; y<(pos_r->pos+pos_r->rows); y++ )
      {
        for( x=pos_r->left; x<(pos_r->left+pos_r->chans); x++ )
        {
          at->at_Positions[y].pos_Track[x]     = pos_r->before[i++];
          at->at_Positions[y].pos_Transpose[x] = pos_r->before[i++];
        }
      }
      at->at_PosNr        = pos_r->posnr;
      at->at_posed_curs   = pos_r->posed_curs;
      at->at_curlch       = pos_r->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;
  }
  
  at->at_modified = TRUE;

  // Place it into the redo list
  IExec->AddTail( at->at_redolist, (struct Node *)un );

  show_changed( at, wpanel, forceall );
}

void redo( struct ahx_tune *at )
{
  struct undonode         *un;
  struct udat_env_w       *env_w;
  struct udat_whole_plist *whole_plist;
  struct udat_ple_b       *ple_b;
  struct udat_ple_w       *ple_w;
  struct udat_pls_w       *pls_w;
  struct udat_ins_b       *ins_b;
  struct udat_track       *rtrk;
  struct udat_stp_b       *stp_b;
  struct udat_stp_w       *stp_w;
  struct udat_string      *rstr;
  struct udat_pos_b       *pos_b;
  struct udat_pos_region  *pos_r;
  int32 i, wpanel, x, y;
  BOOL forceall = FALSE;

  wpanel = -1;
  show_tbox = NULL;

  // Get last thing in the redo list
  un = (struct undonode *)IExec->RemTail( at->at_redolist );
  if( !un ) return;

  // Redo it!
  switch( un->un_type )
  {
    case UNT_POSITIONNR:
      at->at_PositionNr = ((struct udat_tune_w *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_RESTART:
      at->at_Restart = ((struct udat_tune_w *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_TRACKLEN:
      at->at_TrackLength = ((struct udat_tune_b *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_SUBSONGS:
      at->at_SubsongNr = ((struct udat_tune_w *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_SSPOS:
      at->at_curss = ((struct udat_subsongpos *)un->un_data)->subsong;
      at->at_Subsongs[at->at_curss-1] = ((struct udat_tune_b *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_CHANNELS:
      at->at_Channels = ((struct udat_tune_b *)un->un_data)->after;
      at->at_curlch = 0;
      wpanel = PN_TRACKER;
      forceall = TRUE;
      break;

    case UNT_MIXGAIN:
      at->at_mixgainP = ((struct udat_tune_b *)un->un_data)->after;
      at->at_mixgain = (at->at_mixgainP*256)/100;
      wpanel = PN_TRACKER;
      break;

    case UNT_SPMUL:
      at->at_SpeedMultiplier = ((struct udat_tune_b *)un->un_data)->after;
      wpanel = PN_TRACKER;
      break;

    case UNT_ENV_AFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aFrames = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_AVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aVolume = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_DFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->dFrames = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_DVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->aVolume = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_SFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->sFrames = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_RFRAMES:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->rFrames = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_ENV_RVOLUME:
      env_w = (struct udat_env_w *)un->un_data;
      env_w->ptr->rVolume = env_w->after;
      wpanel = PN_INSED;
      at->at_curins = env_w->insnum;
      break;

    case UNT_PLE:
      whole_plist = (struct udat_whole_plist *)un->un_data;
      for( i=0; i<256; i++ ) whole_plist->ptr[i] = whole_plist->after[i];
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = whole_plist->insnum;
      break;

    case UNT_PLE_NOTE:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_Note = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_WAVEFORM:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_Waveform = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FIXED:
      ple_w = (struct udat_ple_w *)un->un_data;
      ple_w->ptr->ple_Fixed = ple_w->after;
      ple_w->pins->ins_ptop  = ple_w->ptop;
      ple_w->pins->ins_pcurx = ple_w->pcurx;
      ple_w->pins->ins_pcury = ple_w->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_w->insnum;
      break;

    case UNT_PLE_FX0:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FX[0] = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FX1:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FX[1] = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FXPARAM0:
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FXParam[0] = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLE_FXPARAM1:  
      ple_b = (struct udat_ple_b *)un->un_data;
      ple_b->ptr->ple_FXParam[1] = ple_b->after;
      ple_b->pins->ins_ptop  = ple_b->ptop;
      ple_b->pins->ins_pcurx = ple_b->pcurx;
      ple_b->pins->ins_pcury = ple_b->pcury;
      wpanel = PN_INSED;
      perf_lastinst = NULL;
      at->at_curins = ple_b->insnum;
      break;

    case UNT_PLS_SPEED:
      pls_w = (struct udat_pls_w *)un->un_data;
      pls_w->ptr->pls_Speed = pls_w->after;
      wpanel = PN_INSED;
      at->at_curins = pls_w->insnum;
      break;

    case UNT_PLS_LENGTH:
      pls_w = (struct udat_pls_w *)un->un_data;
      pls_w->ptr->pls_Length = pls_w->after;
      wpanel = PN_INSED;
      at->at_curins = pls_w->insnum;
      break;
  
    case UNT_INS_VOLUME:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_Volume = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_WAVELENGTH:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_WaveLength = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERLOWERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterLowerLimit = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERUPPERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterUpperLimit = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_FILTERSPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_FilterSpeed = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUARELOWERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareLowerLimit = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUAREUPPERLIMIT:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareUpperLimit = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_SQUARESPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_SquareSpeed = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATODELAY:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoDelay = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATOSPEED:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoSpeed = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_VIBRATODEPTH:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_VibratoDepth = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_HARDCUTRELEASE:
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_HardCutRelease = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;

    case UNT_INS_HARDCUTRELEASEFRAMES:    
      ins_b = (struct udat_ins_b *)un->un_data;
      ins_b->ptr->ins_HardCutReleaseFrames = ins_b->after;
      wpanel = PN_INSED;
      at->at_curins = ins_b->insnum;
      break;
    
    case UNT_STRING_INSNAME:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->after );
      show_tbox = &tbx[TB_INSNAME];
      wpanel = PN_INSED;
      at->at_curins = rstr->extra;
      inslsb_lastcuri = -1;
      break;
    
    case UNT_STRING_INSNAME2:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->after );
      wpanel = PN_TRACKER;
      insls_lastcuri = -1;
      break;
    
    case UNT_STRING_SONGNAME:
      rstr = (struct udat_string *)un->un_data;
      strcpy( rstr->ptr, rstr->after );
      show_tbox = &tbx[TB_SONGNAME];
      wpanel = PN_TRACKER;
      break;
      
    case UNT_TRACK:
      rtrk = (struct udat_track *)un->un_data;
      for( i=0; i<64; i++ )
        at->at_Tracks[rtrk->track][i] = rtrk->after[i];
      at->at_PosNr        = rtrk->posnr;
      at->at_NoteNr       = rtrk->notenr;
      at->at_tracked_curs = rtrk->tracked_curs;
      at->at_curlch       = rtrk->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_NOTEANDINS:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_Note = (stp_w->after>>8);
      stp_w->ptr->stp_Instrument = stp_w->after & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXANDPARAM:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_FX      = (stp_w->after>>8);
      stp_w->ptr->stp_FXParam = stp_w->after & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXBANDPARAM:
      stp_w = (struct udat_stp_w *)un->un_data;
      stp_w->ptr->stp_FXb      = (stp_w->after>>8);
      stp_w->ptr->stp_FXbParam = stp_w->after & 0xff;
      at->at_PosNr        = stp_w->posnr;
      at->at_NoteNr       = stp_w->notenr;
      at->at_tracked_curs = stp_w->tracked_curs;
      at->at_curlch       = stp_w->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_NOTE:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_Note = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_INSTRUMENT:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_Instrument = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FX:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FX = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXPARAM:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXParam = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXB:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXb = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_STP_FXBPARAM:
      stp_b = (struct udat_stp_b *)un->un_data;
      stp_b->ptr->stp_FXbParam = stp_b->after;
      at->at_PosNr        = stp_b->posnr;
      at->at_NoteNr       = stp_b->notenr;
      at->at_tracked_curs = stp_b->tracked_curs;
      at->at_curlch       = stp_b->curlch;
      wpanel = PN_TRACKER;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_TRACK:
      pos_b = (struct udat_pos_b *)un->un_data;
      pos_b->ptr->pos_Track[pos_b->chan] = pos_b->after;
      at->at_PosNr        = pos_b->posnr;
      at->at_posed_curs   = pos_b->posed_curs;
      at->at_curlch       = pos_b->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_TRANS:
      pos_b = (struct udat_pos_b *)un->un_data;
      pos_b->ptr->pos_Transpose[pos_b->chan] = pos_b->after;
      at->at_PosNr        = pos_b->posnr;
      at->at_posed_curs   = pos_b->posed_curs;
      at->at_curlch       = pos_b->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;

    case UNT_POS_REGION:
      pos_r = (struct udat_pos_region *)un->un_data;

      i=0;
      for( y=pos_r->pos; y<(pos_r->pos+pos_r->rows); y++ )
      {
        for( x=pos_r->left; x<(pos_r->left+pos_r->chans); x++ )
        {
          at->at_Positions[y].pos_Track[x]     = pos_r->after[i++];
          at->at_Positions[y].pos_Transpose[x] = pos_r->after[i++];
        }
      }
      at->at_PosNr        = pos_r->posnr;
      at->at_posed_curs   = pos_r->posed_curs;
      at->at_curlch       = pos_r->curlch;
      wpanel = PN_TRACKER;
      posed_lastposnr = 1000;
      trked_lastposnr = 1000;
      break;
  }
  
  at->at_modified = TRUE;

  // Place it back into the undo list
  IExec->AddTail( at->at_undolist, (struct Node *)un );

  show_changed( at, wpanel, forceall );
}
