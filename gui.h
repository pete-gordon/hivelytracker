
struct rawbm
{
  uint16  w;
  uint16  h;
  struct RastPort rp;
  struct BitMap   *bm;
};

struct textbox
{
  int16  x, y;
  int16  w;
  TEXT  *content;
  int32  maxlen;
  int16  flags;
  int32  spos;
  int32  cpos;
  int16  inpanel;
  struct rawbm bm;
};

void gui_pre_init( void );
BOOL gui_init( void );
void gui_shutdown( void );
void gui_handler( uint32 gotsigs );
int32 gui_req( uint32 img, TEXT *title, TEXT *reqtxt, TEXT *buttons );
void gui_render_tunepanel( BOOL force );
void gui_render_tracker( BOOL force );
void gui_render_perf( struct ahx_tune *at, struct ahx_instrument *ins, BOOL force );
void gui_set_various_things( struct ahx_tune *at );
void gui_render_inslistb( BOOL force );
void gui_render_inslist( BOOL force );
void gui_render_tbox( struct RastPort *rp, struct textbox *tb );
void gui_render_tabs( void );

BOOL make_image( struct rawbm *bm, uint16 w, uint16 h );
BOOL open_image( TEXT *name, struct rawbm *bm );

enum
{
  D_IDLE = 0,
  D_EDITING,
  D_PLAYING,
  D_RECORDING
};

enum
{
  E_POS = 0,
  E_TRACK
};

enum
{
  PN_TRACKER = 0,
  PN_INSED,
  PN_END
};

enum
{
  TB_SONGNAME = 0,
  TB_INSNAME,
  TB_INSNAME2,
  TB_END
};
