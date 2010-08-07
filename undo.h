
enum
{
  UNT_POSITIONNR = 0,
  UNT_RESTART,
  UNT_TRACKLEN,
  UNT_SUBSONGS,
  UNT_SSPOS,
  UNT_CHANNELS,
  UNT_MIXGAIN,
  UNT_SPMUL,

  UNT_ENV_AFRAMES,
  UNT_ENV_AVOLUME,
  UNT_ENV_DFRAMES,
  UNT_ENV_DVOLUME,
  UNT_ENV_SFRAMES,
  UNT_ENV_RFRAMES,
  UNT_ENV_RVOLUME,

  UNT_PLE,

  UNT_PLE_NOTE,
  UNT_PLE_WAVEFORM,
  UNT_PLE_FIXED,
  UNT_PLE_FX0,
  UNT_PLE_FX1,
  UNT_PLE_FXPARAM0,
  UNT_PLE_FXPARAM1,  

  UNT_PLS_SPEED,
  UNT_PLS_LENGTH,

  UNT_INS_VOLUME,
  UNT_INS_WAVELENGTH,
  UNT_INS_FILTERLOWERLIMIT,
  UNT_INS_FILTERUPPERLIMIT,
  UNT_INS_FILTERSPEED,
  UNT_INS_SQUARELOWERLIMIT,
  UNT_INS_SQUAREUPPERLIMIT,
  UNT_INS_SQUARESPEED,
  UNT_INS_VIBRATODELAY,
  UNT_INS_VIBRATOSPEED,
  UNT_INS_VIBRATODEPTH,
  UNT_INS_HARDCUTRELEASE,
  UNT_INS_HARDCUTRELEASEFRAMES,    

  UNT_TRACK,

  UNT_STP_NOTE,
  UNT_STP_INSTRUMENT,
  UNT_STP_NOTEANDINS,
  UNT_STP_FX,
  UNT_STP_FXPARAM,
  UNT_STP_FXANDPARAM,
  UNT_STP_FXB,
  UNT_STP_FXBPARAM,
  UNT_STP_FXBANDPARAM,
  
  UNT_POS_TRACK,
  UNT_POS_TRANS,
  UNT_POS_REGION,
  
  // Strings are treated differently
  UNT_STRING_SONGNAME,
  UNT_STRING_INSNAME,
  UNT_STRING_INSNAME2
};

struct undonode
{
  struct Node  un_ln;
  uint32       un_size;
  uint16       un_type;
  void        *un_data;
};

struct udat_env_w
{
  struct ahx_envelope *ptr;
  int32 insnum;
  int16 before;
  int16 after;
};

struct udat_ple_w
{
  struct ahx_plsentry *ptr;
  struct ahx_instrument *pins;
  int32 insnum;
  int32 pcurx, pcury, ptop;
  int16 before;
  int16 after;
};

struct udat_ple_b
{
  struct ahx_plsentry *ptr;
  struct ahx_instrument *pins;
  int32 insnum;
  int32 pcurx, pcury, ptop;
  int8 before;
  int8 after;
};

struct udat_pls_w
{
  struct ahx_plist *ptr;
  int32 insnum;
  int16 before;
  int16 after;
};

struct udat_ins_b
{
  struct ahx_instrument *ptr;
  int32 insnum;
  uint8 before;
  uint8 after;
};

struct udat_stp_b
{
  struct ahx_step *ptr;
  uint16 posnr;
  uint8  notenr;
  int32  tracked_curs;
  int32  curlch;
  uint8  before;
  uint8  after;
};

struct udat_stp_w
{
  struct ahx_step *ptr;
  uint16 posnr;
  uint8  notenr;
  int32  tracked_curs;
  int32  curlch;
  uint16  before;
  uint16  after;
};

struct udat_track
{
  int32 track;
  uint16 posnr;
  uint8  notenr;
  int32  tracked_curs;
  int32  curlch;
  struct ahx_step before[64];
  struct ahx_step after[64];
};

struct udat_pos_b
{
  struct ahx_position *ptr;
  int32 chan;
  uint16 posnr;
  int32 posed_curs;
  int32 curlch;
  uint8 before;
  uint8 after;
};

struct udat_pos_region
{
  uint16 posnr;
  int32 posed_curs;
  int32 curlch;
  int32 left,  pos;
  int32 chans, rows;
  uint8 *before;
  uint8 *after;
};

struct udat_whole_plist
{
  struct ahx_plsentry *ptr;
  int32 insnum;
  struct ahx_plsentry before[256];
  struct ahx_plsentry after[256];
};

struct udat_string
{
  TEXT *ptr;
  int32 extra;
  TEXT *before;
  TEXT *after;
};

struct udat_tune_b
{
  uint8 before;
  uint8 after;
};

struct udat_tune_w
{
  int16 before;
  int16 after;
};

struct udat_subsongpos
{
  int32 subsong;
  uint16 before;
  uint16 after;
};

void setbefore_posregion( struct ahx_tune *at, int32 left, int32 pos, int32 chans, int32 rows );
void setafter_posregion( struct ahx_tune *at, int32 left, int32 pos, int32 chans, int32 rows );
void setbefore_track( struct ahx_tune *at, int32 trk );
void setafter_track( struct ahx_tune *at, int32 trk );
void setbefore_plist( struct ahx_tune *at, struct ahx_plsentry *ptr );
void setafter_plist( struct ahx_tune *at, struct ahx_plsentry *ptr );
void setbefore_string( struct ahx_tune *at, TEXT *ptr );
void setafter_string( int32 which, struct ahx_tune *at, TEXT *ptr );

void modify_env_w( struct ahx_tune *at, struct ahx_envelope *ptr, uint32 field, int16 new );
void modify_ins_b( struct ahx_tune *at, struct ahx_instrument *ptr, uint32 field, uint8 new );
void modify_ple_b( struct ahx_tune *at, struct ahx_instrument *ins, struct ahx_plsentry *ptr, uint32 field, int8 new );
void modify_ple_w( struct ahx_tune *at, struct ahx_instrument *ins, struct ahx_plsentry *ptr, uint32 field, int16 new );
void modify_pls_w( struct ahx_tune *at, struct ahx_plist *ptr, uint32 field, int16 new );
void modify_stp_b( struct ahx_tune *at, struct ahx_step *ptr, uint32 field, int8 new );
void modify_stp_w( struct ahx_tune *at, struct ahx_step *ptr, uint32 field, int16 new );
void modify_pos_b( struct ahx_tune *at, struct ahx_position *ptr, int32 chan, uint32 field, int8 new );
void modify_tune_b( struct ahx_tune *at, uint32 field, int8 new );
void modify_tune_w( struct ahx_tune *at, uint32 field, int16 new );
void modify_sspos( struct ahx_tune *at, int16 new );

void undo( struct ahx_tune *at );
void redo( struct ahx_tune *at );

void free_undolists( struct ahx_tune *at );
