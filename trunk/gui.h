
struct rawbm
{
  uint16  w;
  uint16  h;
  int     fpen, bpen;
  int     findex;
  BOOL    jam2;
  BOOL    fpenset, bpenset, fontset;
#ifndef __SDL_WRAPPER__
  struct RastPort rp;
  struct BitMap   *bm;
  int     baseline;
#else
  struct SDL_Surface *srf;
  SDL_Color fsc, bsc;
  TTF_Font *font;
  int     offx, offy;
#endif
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
BOOL gui_maybe_quit( void );
void gui_shutdown( void );
void gui_handler( uint32 gotsigs );
int32 gui_req( uint32 img, const TEXT *title, const TEXT *reqtxt, const TEXT *buttons );
void gui_render_tunepanel( BOOL force );
void gui_render_tracker( BOOL force );
void gui_render_perf( struct ahx_tune *at, struct ahx_instrument *ins, BOOL force );
void gui_set_various_things( struct ahx_tune *at );
void gui_render_inslistb( BOOL force );
void gui_render_inslist( BOOL force );
void gui_render_tbox( struct rawbm *bm, struct textbox *tb );
void gui_render_tabs( void );
void gui_render_vumeters( void );
void gui_render_wavemeter( void );
void gui_render_everything( void );
BOOL gui_restart( void );

BOOL make_image( struct rawbm *bm, uint16 w, uint16 h );
BOOL open_image( const TEXT *name, struct rawbm *bm );

void set_fcol(struct rawbm *bm, uint32 col);
void fillrect_xy(struct rawbm *bm, int x, int y, int x2, int y2);
void bm_to_bm(const struct rawbm *src, int sx, int sy, struct rawbm *dest, int dx, int dy, int w, int h);


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

#define FONT_FIX 0
#define FONT_SFX 1
#define FONT_PRP 2
