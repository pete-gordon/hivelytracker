/*
** Abandon all hope all ye who enter here
** (consider this fair warning)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __SDL_WRAPPER__
    #include <unistd.h>
#endif

#include <system_includes.h>

#include "replay.h"
#include "gui.h"
#include "util.h"
#include "about.h"
#include "undo.h"
#ifdef __APPLE__
char *osxGetPrefsPath();
char *osxGetResourcesPath(char *, const char*);
#endif

#ifndef __SDL_WRAPPER__
struct Library *IntuitionBase = NULL;
struct Library *P96Base       = NULL;
struct Library *GfxBase       = NULL;
struct Library *DataTypesBase = NULL;
struct Library *DiskfontBase  = NULL;
struct Library *AslBase       = NULL;
struct Library *KeymapBase    = NULL;
struct Library *RequesterBase = NULL;

struct IntuitionIFace *IIntuition = NULL;
struct P96IFace       *IP96       = NULL;
struct GraphicsIFace  *IGraphics  = NULL;
struct DataTypesIFace *IDataTypes = NULL;
struct DiskfontIFace  *IDiskfont  = NULL;
struct AslIFace       *IAsl       = NULL;
struct KeymapIFace    *IKeymap    = NULL;
struct RequesterIFace *IRequester = NULL;

struct InputEvent iev;

struct Window *mainwin = NULL;
struct Window *prefwin = NULL;
struct Screen *scr = NULL;

struct Gadget gad_drag;
struct Gadget gad_drag2;
struct Gadget gad_depth;
#else
struct SDL_Surface *ssrf = NULL;
BOOL needaflip = FALSE;
extern int srfdepth;
extern SDL_Event event;
extern int16 rp_audiobuffer[];
extern uint32 rp_audiobuflen;
extern BOOL aboutwin_open;
#endif

BOOL gui_savethem;

#ifndef __SDL_WRAPPER__
int32  gui_tick_signum = -1;
uint32 gui_tick_sig = 0;

uint32 mainwin_sig = 0;
uint32 prefwin_sig = 0;
uint32 gui_sigs    = 0;
extern int8   *rp_audiobuffer[];
#endif

int16 pw_x = -1, pw_y = -1;
int16 pw_bl = 0, pw_bt = 0;

int32  basekey = 24;

uint8  *cbpbuf = NULL;
uint32  cbpblen = 0;
uint16  cbplcol = 0;
uint16  cbprcol = 0;
uint16  cbpchns = 0;
uint16  cbprows = 0;

extern uint32  rp_audiobuflen;

#define TRACKED_X 3
#define TRACKED_Y 324
#define TRACKED_W 740
#define TRACKED_H 272
#define TRACKED_MX (TRACKED_W-1)
#define TRACKED_MY (TRACKED_H-1)

#define POSED_X 298
#define POSED_Y 190
#define POSED_W 316
#define POSED_H 98
#define POSED_MX (POSED_W-1)
#define POSED_MY (POSED_H-1)

#define INSLIST_X 622
#define INSLIST_Y 152
#define INSLIST_W 168
#define INSLIST_H 136
#define INSLIST_MX (INSLIST_W-1)
#define INSLIST_MY (INSLIST_H-1)

#define PERF_X 291
#define PERF_Y 144
#define PERF_W 146
#define PERF_H 438
#define PERF_MX (PERF_W-1)
#define PERF_MY (PERF_H-1)

#define INSLSTB_X 474
#define INSLSTB_Y 144
#define INSLSTB_W 168
#define INSLSTB_H 438
#define INSLSTB_MX (INSLSTB_W-1)
#define INSLSTB_MY (INSLSTB_H-1)

int32 qual;
BOOL qualRMB = FALSE;

//BOOL qualShift=FALSE, qualCtrl=FALSE, qualRMB=FALSE, qualAlt, qualAmiga;

extern struct List *rp_tunelist;
extern struct SignalSemaphore *rp_list_ss;

struct ahx_tune *curtune = NULL;
extern struct ahx_tune *rp_curtune;

struct ahx_instrument *perf_lastinst = NULL;
int32 perf_laststep = -1;
int32 perf_lastcx = -1;
int32 perf_lastcy = -1;
int32 perf_lastplen = -1;

struct ahx_tune *tmr_lasttune = NULL;
uint32 tmr_lasttime = 0;

int32 vum_last[MAX_CHANNELS];
BOOL vum_needclr = 0;
int32 wm_count = 0;

int32 pref_defstereo  = 4;
int32 pref_maxundobuf = 2;
BOOL  pref_fullscr    = FALSE;
BOOL  pref_blankzeros = FALSE;

BOOL  pref_dorestart = FALSE;
BOOL  pref_oldfullscr;
TEXT  pref_oldskindir[512];
#ifdef __SDL_WRAPPER__
BOOL  pref_rctrlplaypos = FALSE;
#endif

BOOL fullscr = FALSE;

extern BOOL quitting;

enum
{
  PAL_BACK = 0,
  PAL_BARDARK,
  PAL_BARMID,
  PAL_BARLIGHT,
  PAL_TEXT,
  PAL_DISABLEDTEXT,
  PAL_WAVEMETER,
  PAL_CURSNORM,
  PAL_CURSEDIT,
  PAL_BTNTEXT,
  PAL_BTNSHADOW,
  PAL_FKEYHIGHLIGHT,
  PAL_POSEDCHIND,
  PAL_TABTEXT,
  PAL_TABSHADOW,
  PAL_END
};

uint32 pal[] = { 0x000000, 0x500000, 0x780000, 0xff5555, 0xffffff, 0x808080, 0xff88ff, 0xffffff, 0xffff88, 0xffffff, 0x000000, 0xffffff, 0xffffff, 0xffffff, 0x000000 };
#ifdef __SDL_WRAPPER__
uint32 mappal[sizeof(pal)/sizeof(uint32)];
#endif

const TEXT *notenames[] = { "---",
  "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
  "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
  "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
  "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
  "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5" };

const int32 posed_xoff[] = {  4*7,  5*7,  6*7,   8*7,  9*7,
                       11*7, 12*7, 13*7,  15*7, 16*7,
                       18*7, 19*7, 20*7,  22*7, 23*7,

                       25*7, 26*7, 27*7,  29*7, 30*7,
                       32*7, 33*7, 34*7,  36*7, 37*7,
                       39*7, 40*7, 41*7,  43*7, 44*7 };

const int32 tracked_xoff[] = {  3*8,   7*8,  8*8,  10*8, 11*8, 12*8,  14*8, 15*8, 16*8,
                         18*8,  22*8, 23*8,  25*8, 26*8, 27*8,  29*8, 30*8, 31*8,
                         33*8,  37*8, 38*8,  40*8, 41*8, 42*8,  44*8, 45*8, 46*8,

                         48*8,  52*8, 53*8,  55*8, 56*8, 57*8,  59*8, 60*8, 61*8,
                         63*8,  67*8, 68*8,  70*8, 71*8, 72*8,  74*8, 75*8, 76*8,
                         78*8,  82*8, 83*8,  85*8, 86*8, 87*8,  89*8, 90*8, 91*8 };

const int32 perf_xoff[] = { 4*8, 9*8, 11*8, 12*8, 13*8, 15*8, 16*8, 17*8 };

struct czone
{
  int16 x;
  int16 y;
  int16 w;
  int16 h;
};

#define NBB_ENABLED 0
#define NBF_ENABLED (1L<<NBB_ENABLED)
#define NBB_WAVELEN 1
#define NBF_WAVELEN (1L<<NBB_WAVELEN)
#define NBB_UDDURINGPLAY 2
#define NBF_UDDURINGPLAY (1L<<NBB_UDDURINGPLAY)
#define NBB_ONOFF 3
#define NBF_ONOFF (1L<<NBB_ONOFF)

struct numberbox
{
  int16  x, y;
  int16  w, h;
  TEXT   fmt[8];
  int32  max, min;
  int32  cnum;
  uint16 flags;
  int16  pressed;
};


#define TBB_ENABLED 0
#define TBF_ENABLED (1L<<TBB_ENABLED)
#define TBB_ACTIVE 1
#define TBF_ACTIVE (1L<<TBB_ACTIVE)
#define TBB_VISIBLE 2
#define TBF_VISIBLE (1L<<TBB_VISIBLE)

struct prefcycle
{
  int16   x, y;
  int32   copt;
  int32   numopts;
  int32   zone;
  TEXT  **options;
};

struct popup
{
  int16  x, y;
};

enum
{
  NB_POSNR = 0,
  NB_LENGTH,
  NB_RESPOS,
  NB_TRKLEN,
  NB_SSNR,
  NB_CURSS,
  NB_SSPOS,
  NB_CHANS,
  NB_MIXG,
  NB_SPEEDMULT,
  NB_DRUMPADMODE,
  NB_END
};

enum
{
  INB_INS = 0,
  INB_VOL,
  INB_WAVELEN,
  INB_ATTACK,
  INB_AVOL,
  INB_DECAY,
  INB_DVOL,
  INB_SUSTAIN,
  INB_RELEASE,
  INB_RVOL,
  INB_VIBDELAY,
  INB_VIBDEPTH,
  INB_VIBSPEED,
  INB_SQRLOWER,
  INB_SQRUPPER,
  INB_SQRSPEED,
  INB_FLTLOWER,
  INB_FLTUPPER,
  INB_FLTSPEED,
  INB_PERFSPEED,
  INB_PERFLEN,
  INB_HARDCUT,
  INB_RELCUT,
  INB_END
};

enum
{
  BBA_NOTHING = 0,
  BBA_PLAYPOS,
  BBA_PLAYSONG,
  BBA_STOP,
  BBA_ZAP,
  BBA_NEWTAB,
  BBA_CLONETAB,
  BBA_LOADTUNE,
  BBA_SAVEAHX,
  BBA_SAVEHVL,
  BBA_LOADINS,
  BBA_SAVEINS,
  BBA_CONTSONG,
  BBA_CONTPOS,
  BBA_INSEDIT,
  BBA_PREFS,
  BBA_AUTOGAIN,
  BBA_ABOUT,
  
  BBA_CUTTRK,
  BBA_COPYTRK,
  BBA_PASTE,
  BBA_PASTECMDS,
  BBA_NOTEUP,
  BBA_NOTEDN,
  BBA_SCRLUP,
  BBA_SCRLDN,
  BBA_REVERSE,
  
  BBA_UNDO,
  BBA_REDO,
  
  BBA_OPTIMISE,
  BBA_OPTIMISE_MORE,

  BBA_ZAP_SONG,
  BBA_ZAP_TRACKS,
  BBA_ZAP_POSNS,
  BBA_ZAP_INSTRS,
  BBA_ZAP_ALL,
  BBA_BACK,
  
  BBA_END
};

enum
{
  PTB_SONGDIR = 0,
  PTB_INSTDIR,
  PTB_SKINDIR,
  PTB_END
};

enum
{
  PP_SONGDIR = 0,
  PP_INSTDIR,
  PP_SKINDIR,
  PP_END
};

struct buttonbank
{
  int16  x, y;
  int32  zone;
  TEXT  *name;
  int32  action;
  int32  raction;
  int32  xo;
};

enum
{
  BM_LOGO = 0,
  BM_TAB_AREA,
  BM_TAB_LEFT,
  BM_TAB_MID,
  BM_TAB_RIGHT,
  BM_TAB_TEXT,
  BM_ITAB_LEFT,
  BM_ITAB_MID,
  BM_ITAB_RIGHT,
  BM_ITAB_TEXT,
  BM_BUTBANKR,
  BM_BUTBANKP,
  BM_BG_TRACKER,
  BM_BG_INSED,
  BM_PLUSMINUS,
  BM_POSED,
  BM_TRACKED,
  BM_TRACKBAR,
  BM_PERF,
  BM_VUMETER,
  BM_WAVEMETERS,
  BM_DEPTH,
  BM_PRF_BG,
  BM_PRF_CYCLE,
  BM_INSLIST,
  BM_INSLISTB,
  BM_TRKBANKR,
  BM_TRKBANKP,
  BM_CHANMUTE,
  BM_BLANK,
  BM_DIRPOPUP,
  BM_END
};

enum
{
  PC_WMODE = 0,
  PC_DEFSTEREO,
  PC_BLANKZERO,
  PC_MAXUNDOBUF,
  PC_END
};

struct tunetab
{
  struct ahx_tune *tune;
  int32            zone;
};

TEXT fixfontname[256];
TEXT sfxfontname[256];
TEXT prpfontname[256];

TEXT skinext[32];

BOOL tabtextback = FALSE;
BOOL tabtextshad = TRUE;
BOOL posedadvance = TRUE;
int32 defnotejump = 1, definotejump = 1;

#ifndef __SDL_WRAPPER__
struct TextAttr fixfontattr = { fixfontname, 16, 0, 0 };
struct TextAttr sfxfontattr = { sfxfontname, 14, 0, 0 };
struct TextAttr prpfontattr = { prpfontname, 16, 0, 0 };

struct TextFont *fixfont = NULL;
struct TextFont *sfxfont = NULL;
struct TextFont *prpfont = NULL;
#else
TTF_Font *fixttf = NULL;
TTF_Font *sfxttf = NULL;
TTF_Font *prpttf = NULL;
BOOL prefwin_open = FALSE;
#endif

#define MAX_ZONES 100
#define MAX_PZONES 20

int32 last_tbox = -1;

struct rawbm      mainbm, prefbm;
struct rawbm      bitmaps[BM_END];
struct czone      zones[MAX_ZONES], pzones[MAX_PZONES];
struct numberbox  trk_nb[NB_END], ins_nb[INB_END];
struct textbox    tbx[TB_END], ptb[PTB_END];
struct textbox   *etbx = NULL, *ptbx = NULL;
struct popup      pp[PP_END];
struct tunetab    ttab[8];
struct buttonbank bbank[16];
struct buttonbank tbank[12];
struct prefcycle  pcyc[PC_END];

TEXT *pc_wmode_opts[] = { "Windowed", "Fullscreen", NULL };
TEXT *pc_defst_opts[] = { "0% (mono)", "25%", "50%", "75%", "100% (paula)", NULL };
TEXT *pc_bzero_opts[] = { "No", "Yes", NULL };
TEXT *pc_mundo_opts[] = { "Unlimited", "256Kb", "512Kb", "1Mb", "2Mb", "4Mb", NULL };

struct ahx_tune *posed_lasttune  = NULL;
int16            posed_lastposnr = 1000;
int16            posed_lastlchan = 0;
int16            posed_lastchans = 0;

struct ahx_tune *trked_lasttune   = NULL;
int16            trked_lastposnr  = 1000;
int16            trked_lastnotenr = 65;
int16            trked_lastchans  = 4;
int16            trked_lastlchan  = 0;

struct ahx_tune *insls_lasttune   = NULL;
int16            insls_lastcuri   = -1;
int16            insls_lasttopi   = -1;

struct ahx_tune *inslsb_lasttune   = NULL;
int16            inslsb_lastcuri   = -1;
int16            inslsb_lasttopi   = -1;

int32 numzones = 0;
int32 numpzones = 0;
int32 cz_lmb, cz_plmb;
int32 cz_rmb, cz_prmb;

int32 whichcyc = -1;
int32 whichpop = -1;

int32 zn_close    = -1;
int32 zn_scrdep   = -1;
int32 zn_mute[6];

#ifndef __SDL_WRAPPER__
struct FileRequester *ins_lreq;
struct FileRequester *ins_sreq;
struct FileRequester *sng_lreq;
struct FileRequester *sng_sreq;
struct FileRequester *dir_req;
#endif

#define CBB_NOTES 0
#define CBF_NOTES (1L<<CBB_NOTES)
#define CBB_CMD1 1
#define CBF_CMD1 (1L<<CBB_CMD1)
#define CBB_CMD2 2
#define CBF_CMD2 (1L<<CBB_CMD2)

struct ahx_step cb_content[64];
int32 cb_contains = 0;
int32 cb_length = 0;

TEXT songdir[512];
TEXT instdir[512];
TEXT skindir[512];

#ifdef __SDL_WRAPPER__
TEXT remsongdir[512];
TEXT reminstdir[512];

#ifdef WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

void setpathpart(char *path, const char *file)
{
  int i;

  i = strlen(file);
  while (i>0)
  {
    if (file[i] == PATHSEP) break;
    i--;
  }

  if (i>511) return;

  strcpy(path, file);
  path[i] = 0;
}
#endif

BOOL isws( TEXT c )
{
  if( ( c == 9 ) || ( c == 32 ) ) return TRUE;
  return FALSE;
}

BOOL isnum( TEXT c )
{
  if( ( c >= '0' ) && ( c <= '9' ) ) return TRUE;
  return FALSE;
}

BOOL ishex( TEXT c )
{
  if( ( c >= '0' ) && ( c <= '9' ) ) return TRUE;
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return TRUE;
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return TRUE;
  return FALSE;
}

#ifndef __SDL_WRAPPER__
int32 gui_req( uint32 img, const TEXT *title, const TEXT *reqtxt, const TEXT *buttons )
{
  Object *req_obj;
  int32 n;

  req_obj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
    REQ_TitleText,  title,
    REQ_BodyText,   reqtxt,
    REQ_GadgetText, buttons,
    REQ_Image,      img,
    REQ_WrapBorder, 40,
    TAG_DONE );
    
  if( !req_obj ) return 1;

  n = IIntuition->IDoMethod( req_obj, RM_OPENREQ, NULL, NULL, scr );
  IIntuition->DisposeObject( req_obj );
  return n;
}
#endif

void set_fpen(struct rawbm *bm, int pen)
{
  if ((bm->fpenset) && (bm->fpen == pen))
    return;

#ifndef __SDL_WRAPPER__
  IGraphics->SetRPAttrs( &bm->rp, RPTAG_APenColor, pal[pen]|0xff000000, TAG_DONE );
#else
  bm->fsc.r = pal[pen]>>16;
  bm->fsc.g = pal[pen]>>8;
  bm->fsc.b = pal[pen];
#endif

  bm->fpen = pen;
  bm->fpenset = TRUE;
}

void set_fcol(struct rawbm *bm, uint32 col)
{
#ifndef __SDL_WRAPPER__
  IGraphics->SetRPAttrs( &bm->rp, RPTAG_APenColor, col|0xff000000, TAG_DONE );
#else
  bm->fsc.r = col>>16;
  bm->fsc.g = col>>8;
  bm->fsc.b = col;
#endif

  bm->fpenset = FALSE;
}

void set_bpen(struct rawbm *bm, int pen)
{
  if ((bm->bpenset) && (bm->bpen == pen))
    return;

#ifndef __SDL_WRAPPER__
  IGraphics->SetRPAttrs( &bm->rp, RPTAG_BPenColor, pal[pen]|0xff000000, TAG_DONE );
#else
  bm->bsc.r = pal[pen]>>16;
  bm->bsc.g = pal[pen]>>8;
  bm->bsc.b = pal[pen];
#endif

  bm->bpen = pen;
  bm->bpenset = TRUE;
}

void set_pens(struct rawbm *bm, int fpen, int bpen)
{
  set_fpen(bm, fpen);
  set_bpen(bm, bpen);
}

void fillrect_xy(struct rawbm *bm, int x, int y, int x2, int y2)
{
#ifndef __SDL_WRAPPER__
  IGraphics->RectFill( &bm->rp, x, y, x2, y2 );
#else
  SDL_Rect rect = { .x = x+bm->offx, .y = y+bm->offy, .w = (x2-x)+1, .h = (y2-y)+1 };
  Uint32 col;

  if (bm->fpenset)
    col = mappal[bm->fpen];
  else
#ifdef __APPLE__
    // Work-around for SDL bug on OSX
    col = SDL_MapRGB(bm->srf->format, bm->fsc.b, bm->fsc.g, bm->fsc.r) >> 8;
#else
    col = SDL_MapRGB(bm->srf->format, bm->fsc.r, bm->fsc.g, bm->fsc.b);
#endif
  SDL_FillRect(bm->srf, &rect, col);
#endif
}

void bm_to_bm(const struct rawbm *src, int sx, int sy, struct rawbm *dest, int dx, int dy, int w, int h)
{
#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort(src->bm, sx, sy, &dest->rp, dx, dy, w, h, 0x0C0);
#else
  SDL_Rect srect = { .x = sx+src->offx, .y = sy+src->offy, .w = w, .h = h };
  SDL_Rect drect = { .x = dx+dest->offx, .y = dy+dest->offy, .w = w, .h = h };
  SDL_BlitSurface(src->srf, &srect, dest->srf, &drect);
  if (dest == &mainbm) needaflip = TRUE;
#endif
}

void set_font(struct rawbm *bm, int findex, BOOL jam2)
{
  if ((bm->fontset) && (bm->findex == findex) && (bm->jam2 == jam2))
    return;

#ifndef __SDL_WRAPPER__
  struct TextFont *thefont;
  switch (findex)
  {
    case FONT_SFX: thefont = sfxfont; break;
    case FONT_PRP: thefont = prpfont; break;
    default:       thefont = fixfont; break;
  }
   
  IGraphics->SetRPAttrs( &bm->rp, RPTAG_DrMd, jam2 ? JAM2 : JAM1, RPTAG_Font, thefont, TAG_DONE );
  bm->baseline = thefont->tf_Baseline;
#else
  switch (findex)
  {
    case FONT_SFX: bm->font = sfxttf; break;
    case FONT_PRP: bm->font = prpttf; break;
    default:       bm->font = fixttf; break;
  }
#endif

  bm->findex  = findex;
  bm->jam2    = jam2;
  bm->fontset = TRUE;
}

void printstr(struct rawbm *bm, const char *str, int x, int y)
{
#ifndef __SDL_WRAPPER__
  IGraphics->Move(&bm->rp, x, y+bm->baseline);
  IGraphics->Text(&bm->rp, str, strlen(str));
#else
  SDL_Surface *srf;
  SDL_Rect rect = { .x = x+bm->offx, .y = y+bm->offy };
  if (bm->jam2)
  {
    srf = TTF_RenderText_Shaded(bm->font, str, bm->fsc, bm->bsc);
  }
  else
  {
    srf = TTF_RenderText_Blended(bm->font, str, bm->fsc);
  }
  if (!srf) return;
  SDL_BlitSurface(srf, NULL, bm->srf, &rect);
  SDL_FreeSurface(srf);
#endif
}

void printstrlen(struct rawbm *bm, const char *str, int len, int x, int y)
{
#ifndef __SDL_WRAPPER__
  IGraphics->Move(&bm->rp, x, y+bm->baseline);
  IGraphics->Text(&bm->rp, str, len);
#else
  char tmp[len+1];
  SDL_Surface *srf;
  SDL_Rect rect = { .x = x+bm->offx, .y = y+bm->offy };
  strncpy(tmp, str, len);
  tmp[len] = 0;
  if (bm->jam2)
  {
    srf = TTF_RenderText_Shaded(bm->font, tmp, bm->fsc, bm->bsc);
  }
  else
  {
    srf = TTF_RenderText_Blended(bm->font, tmp, bm->fsc);
  }
  if (!srf) return;
  SDL_BlitSurface(srf, NULL, bm->srf, &rect);
  SDL_FreeSurface(srf);
#endif
}

int textfit( struct rawbm *bm, const char *str, int w )
{
#ifndef __SDL_WRAPPER__
  struct TextExtent te;
  return IGraphics->TextFit( &bm->rp, str, strlen(str), &te, NULL, 1, w, 24 );
#else
  int tw, th, c;
  char tmp[512];
  strncpy(tmp, str, 512);
  tmp[511] = 0;

  c = strlen(tmp);
  while (c>0)
  {
    TTF_SizeText(bm->font, tmp, &tw, &th);
    if (tw <= w) break;
    tmp[--c] = 0;
  }
  return c;
#endif
}

int32 gui_add_zone( struct czone *zn, int32 *numzn, int32 maxz, int16 x, int16 y, int16 w, int16 h )
{
  int32 fz;

  for( fz=0; fz<(*numzn); fz++ )
    if( zn[fz].x == -1 ) break;

  if( fz == *numzn )
  {
    if( (*numzn) == maxz )
      return -1;
    (*numzn)++;
  }

  zn[fz].x = x;
  zn[fz].y = y;
  zn[fz].w = w;
  zn[fz].h = h;
  
//  printf( "z: %ld = %d,%d,%d,%d\n", fz, x, y, w, h );

  return fz;
}

void gui_set_nbox( struct numberbox *nb, int16 x, int16 y, int16 w, int16 h, int32 min, int32 max, int32 cnum, const TEXT *fmt, uint16 flags )
{
  nb->x = x;
  nb->y = y;
  nb->w = w;
  nb->h = h;
  nb->min = min;
  nb->max = max;
  nb->cnum = cnum;
  nb->flags = flags;
  nb->pressed = 0;
      
  if( fmt == NULL )
    strcpy( nb->fmt, "%d" );
  else
    strcpy( nb->fmt, fmt );
}

void gui_zap_zone( struct czone *zn, int32 *numzn, int32 max, int32 *zone )
{
  int32 tz;
  
  tz = *zone;
  *zone = -1;
  
  if( ( tz < 0 ) || ( tz >= max ) )
    return;

  zn[tz].x = -1;
  
  if( (*numzn) == 0 ) return;

  while( (*numzn) > 0 )
  {
    if( zn[(*numzn)-1].x != -1 ) break;
    (*numzn)--;
  }
}

int32 gui_get_zone( struct czone *zn, int32 numz, int16 x, int16 y )
{
  int32 i;
  
  for( i=0; i<numz; i++ )
  {
    if( zn[i].x != -1 )
    {
      if( ( x >= zn[i].x ) &&
          ( y >= zn[i].y ) &&
          ( x < ( zn[i].x+zn[i].w ) ) &&
          ( y < ( zn[i].y+zn[i].h ) ) )
        return i;
    }
  }
 
  return -1;
}

BOOL make_image( struct rawbm *bm, uint16 w, uint16 h )
{
  bm->w = w;
  bm->h = h;
  bm->fontset = FALSE;
  bm->fpenset = FALSE;
  bm->bpenset = FALSE;
#ifndef __SDL_WRAPPER__
  IGraphics->InitRastPort( &bm->rp );
  bm->bm = IGraphics->AllocBitMap( bm->w, bm->h, 8, 0, mainwin->RPort->BitMap );
  if( !bm->bm )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }
  bm->rp.BitMap = bm->bm;
#else
  bm->offx = 0;
  bm->offy = 0;
  bm->srf = SDL_CreateRGBSurface(SRFTYPE, w, h, srfdepth, 0, 0, 0, 0);
  if( !bm->srf )
  {
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }
#endif
  return TRUE;
}

BOOL open_image( const TEXT *name, struct rawbm *bm )
{
#ifndef __SDL_WRAPPER__
  uint8 *data;
  Object *io = NULL;
  struct BitMapHeader *bmh;
  struct RenderInfo ri;
  TEXT tmp[1024];
  
  strcpy( tmp, skindir );
  IDOS->AddPart( tmp, name, 1024 );
  strcat( tmp, skinext );
  
  io = IDataTypes->NewDTObject( tmp,
    DTA_SourceType,        DTST_FILE,
    DTA_GroupID,           GID_PICTURE,
    PDTA_FreeSourceBitMap, TRUE,
    PDTA_DestMode,         PMODE_V43,
    TAG_DONE );

  if( !io )
  {
    printf( "Error opening '%s'\n", tmp );
    return FALSE;
  }
  
  if( IDataTypes->GetDTAttrs( io,
    PDTA_BitMapHeader,     &bmh,
    TAG_DONE ) == 0 )
  {
    IDataTypes->DisposeDTObject( io );
    printf( "Unable to get information about '%s'\n", tmp );
    return FALSE;
  }
  
  bm->w      = bmh->bmh_Width;
  bm->h      = bmh->bmh_Height;
  
  IGraphics->InitRastPort( &bm->rp );
  bm->bm = IGraphics->AllocBitMap( bm->w, bm->h, 8, 0, mainwin->RPort->BitMap );
  if( !bm->bm )
  {
    IDataTypes->DisposeDTObject( io );
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }
  
  bm->rp.BitMap = bm->bm;
  
  data   = IExec->AllocVecTags( bm->w*bm->h*4, TAG_DONE );
  if( !data )
  {
    IDataTypes->DisposeDTObject( io );
    IGraphics->FreeBitMap( bm->bm );
    printf( "Out of memory @ %s:%d\n", __FILE__, __LINE__ );
    return FALSE;
  }
  
  IIntuition->IDoMethod( io, PDTM_READPIXELARRAY, data, PBPAFMT_ARGB, bm->w<<2, 0, 0, bm->w,bm->h );

  ri.Memory      = data;
  ri.BytesPerRow = bm->w<<2;
  ri.RGBFormat   = RGBFB_A8R8G8B8;
  IP96->p96WritePixelArray( &ri, 0, 0, &bm->rp, 0, 0, bm->w, bm->h );  
  
  IExec->FreeVec( data );
  IDataTypes->DisposeDTObject( io );
#else
  SDL_Surface *tmpsrf;
  TEXT tmp[1024];
  
#ifdef __APPLE__
  if (strncmp(skindir, "Skins/", 6) == 0)
    osxGetResourcesPath(tmp, skindir);
#else

  strncpy( tmp, skindir, 1024 );
#endif
  strncat( tmp, "/", 1024 );
  strncat( tmp, name, 1024 );
  strncat( tmp, skinext, 1024 );

  tmpsrf = IMG_Load(tmp);
  if (!tmpsrf)
  {
    printf("Error loading '%s': %s\n", tmp, IMG_GetError());
    return FALSE;
  }

  if (!make_image(bm, tmpsrf->w, tmpsrf->h))
  {
    SDL_FreeSurface(tmpsrf);
    return FALSE;
  }
  /* Convert to the screen surface format */
  SDL_BlitSurface(tmpsrf, NULL, bm->srf, NULL);
  SDL_FreeSurface(tmpsrf);
#endif
  return TRUE;
}

BOOL gui_set_tbox( struct textbox *tb, int16 x, int16 y, int16 w, TEXT *content, int32 maxlen, int16 flags, int16 inpanel )
{
  tb->x = x;
  tb->y = y;
  tb->w = w;
  tb->content = content;
  tb->maxlen = maxlen;
  tb->flags = flags;
  tb->spos = 0;
  tb->cpos = 0;
  tb->inpanel = inpanel;

  if( !make_image( &tb->bm, w, 16 ) ) return FALSE;

  set_font(&tb->bm, FONT_FIX, FALSE);
  return TRUE;
}

void put_partbitmap( uint32 bm, uint16 bx, uint16 by, uint16 x, uint16 y, uint16 w, uint16 h )
{
#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort( bitmaps[bm].bm, bx, by, mainwin->RPort, x, y, w, h, 0x0C0 );
#else
  SDL_Rect srect = { .x = bx+bitmaps[bm].offx, .y = by+bitmaps[bm].offy, .w = w, .h = h };
  SDL_Rect drect = { .x = x , .y =  y, .w = w, .h = h };
  SDL_BlitSurface(bitmaps[bm].srf, &srect, ssrf, &drect);
  needaflip = TRUE;
#endif
}

void put_bitmap( uint32 bm, uint16 x, uint16 y, uint16 w, uint16 h )
{
#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort( bitmaps[bm].bm, 0, 0, mainwin->RPort, x, y, w, h, 0x0C0 );
#else
  SDL_Rect srect = { .x = bitmaps[bm].offx, .y = bitmaps[bm].offy, .w = w, .h = h };
  SDL_Rect drect = { .x = x, .y = y, .w = w, .h = h };
  SDL_BlitSurface(bitmaps[bm].srf, &srect, ssrf, &drect);
  needaflip = TRUE;
#endif
}

void put_ppartbitmap( uint32 bm, uint16 bx, uint16 by, uint16 x, uint16 y, uint16 w, uint16 h )
{
#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort( bitmaps[bm].bm, bx, by, prefwin->RPort, x+pw_bl, y+pw_bt, w, h, 0x0C0 );
#else
  SDL_Rect srect = { .x = bx+bitmaps[bm].offx, .y = by+bitmaps[bm].offy, .w = w, .h = h };
  SDL_Rect drect = { .x = x+4 , .y =  y+4, .w = w, .h = h };
  SDL_BlitSurface(bitmaps[bm].srf, &srect, prefbm.srf, &drect);
#endif
}

void put_pbitmap( uint32 bm, uint16 x, uint16 y, uint16 w, uint16 h )
{
#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort( bitmaps[bm].bm, 0, 0, prefwin->RPort, x+pw_bl, y+pw_bt, w, h, 0x0C0 );
#else
  SDL_Rect srect = { .x = bitmaps[bm].offx, .y = bitmaps[bm].offy, .w = w, .h = h };
  SDL_Rect drect = { .x = x+4, .y = y+4, .w = w, .h = h };
  SDL_BlitSurface(bitmaps[bm].srf, &srect, prefbm.srf, &drect);
  needaflip = TRUE;
#endif
}

void gui_render_nbox( struct ahx_tune *at, int32 panel, struct numberbox *nb )
{
  TEXT pbuf[32];

  if( nb->cnum < nb->min ) nb->cnum = nb->min;
  if( nb->cnum > nb->max ) nb->cnum = nb->max;
  
  if( panel != at->at_curpanel )
    return;

  set_fpen(&mainbm, PAL_BACK);
  fillrect_xy(&mainbm, nb->x, nb->y, nb->x+nb->w-25, nb->y+nb->h-1);

  set_font(&mainbm, FONT_FIX, FALSE);
  
  if( ( nb->flags & NBF_ENABLED ) != 0 )
    set_fpen(&mainbm, PAL_TEXT);
  else
    set_fpen(&mainbm, PAL_DISABLEDTEXT);

  switch( nb->flags & (NBF_WAVELEN|NBF_ONOFF) )
  {
    case NBF_WAVELEN:
      sprintf( pbuf, nb->fmt, (4L<<nb->cnum) );
      break;
    
    case NBF_ONOFF:
      if( nb->cnum )
        strcpy( pbuf, "On " );
      else
        strcpy( pbuf, "Off" );
      break;
    
    default:
      sprintf( pbuf, nb->fmt, nb->cnum );
  }
  printstr(&mainbm, pbuf, nb->x+4, nb->y);

  put_partbitmap( BM_PLUSMINUS, 0, 0, nb->x+nb->w-28, nb->y-1, 28, 19 );
  nb->pressed = 0;
}

void gui_render_tbox( struct rawbm *bm, struct textbox *tb )
{
  int16 panel;
  int32 w;
  
  if( ( tb->flags & TBF_VISIBLE ) == 0 ) return;

  if( bm == &mainbm )
  {
    panel = PN_TRACKER;
    if( curtune )
      panel = curtune->at_curpanel;
  
    if( panel != tb->inpanel ) return;
  }

  set_fpen(&tb->bm, PAL_BACK);
  fillrect_xy(&tb->bm, 0, 0, tb->w-1, 15);

  if( tb->content == NULL )
  {
    bm_to_bm(&tb->bm, 0, 0, bm, tb->x, tb->y, tb->w, 16);
    return;
  }
  
  if( tb->cpos < 0 ) tb->cpos = 0;
  
  if( tb->cpos > strlen( tb->content ) )
    tb->cpos = strlen( tb->content );

  if( tb->cpos < tb->spos ) tb->spos = tb->cpos;
  if( tb->cpos >= (tb->spos + (tb->w>>3)) )
    tb->spos = tb->cpos - ((tb->w>>3)-1);
  
  w = strlen( &tb->content[tb->spos] );
  if( w > (tb->w>>3) ) w = (tb->w>>3);
  
  if( tb->flags & TBF_ACTIVE )
  {
    set_fpen(&tb->bm, PAL_BARLIGHT);
    fillrect_xy(&tb->bm, (tb->cpos-tb->spos)*8, 0, (tb->cpos-tb->spos)*8+7, 15);
  }

  if( tb->flags & TBF_ENABLED )
    set_fpen(&tb->bm, PAL_TEXT);
  else
    set_fpen(&tb->bm, PAL_DISABLEDTEXT);

  printstrlen(&tb->bm, &tb->content[tb->spos], w, 0, 0);

  bm_to_bm(&tb->bm, 0, 0, bm, tb->x, tb->y, tb->w, 16);
}

void gui_render_perf( struct ahx_tune *at, struct ahx_instrument *ins, BOOL force )
{
  TEXT pbuf[32];
  int32 i, j, k, n;
  int32 cx, cy, mx, my;

  if( ins == NULL )
  {
    set_fpen(&mainbm, PAL_BACK);
    fillrect_xy(&mainbm, PERF_X, PERF_Y, PERF_MX, PERF_MY);
    perf_lastinst = NULL;
    return;
  }

  if( ins->ins_pcury < 0 ) ins->ins_pcury = 0;
  if( ins->ins_pcury > 254 ) ins->ins_pcury = 254;

  if( ins->ins_pcurx < 0 ) ins->ins_pcurx = 0;
  if( ins->ins_pcurx > 7 ) ins->ins_pcurx = 7;
  
  if( ins->ins_pcury < ins->ins_ptop ) ins->ins_ptop = ins->ins_pcury;
  if( ins->ins_pcury >= ins->ins_ptop+(PERF_H>>4) )
    ins->ins_ptop = ins->ins_pcury-((PERF_H>>4)-1);

  if( ins->ins_ptop < 0 ) ins->ins_ptop = 0;
  if( ins->ins_ptop > 255-(PERF_H>>4) ) ins->ins_ptop = (255-(PERF_H>>4));

  if( ( force == FALSE ) &&
      ( ins == perf_lastinst ) &&
      ( ins->ins_ptop == perf_laststep ) &&
      ( ins->ins_pcurx == perf_lastcx ) &&
      ( ins->ins_pcury == perf_lastcy ) &&
      ( ins->ins_PList.pls_Length == perf_lastplen ) )
    return;

  perf_lastinst = ins;
  perf_laststep = ins->ins_ptop;
  perf_lastcx   = ins->ins_pcurx;
  perf_lastcy   = ins->ins_pcury;
  perf_lastplen = ins->ins_PList.pls_Length;
  
  set_fpen(&bitmaps[BM_PERF], PAL_BACK);
  fillrect_xy(&bitmaps[BM_PERF], 0, 0, PERF_MX, PERF_MY);
  
  set_pens(&bitmaps[BM_PERF], PAL_TEXT, PAL_BACK);
  
  if( ins->ins_ptop >= ins->ins_PList.pls_Length )
    set_fpen(&bitmaps[BM_PERF], PAL_DISABLEDTEXT);

  k = ins->ins_pcury - ins->ins_ptop;
  
  for( i=0, j=ins->ins_ptop; i<(PERF_H>>4); i++, j++ )
  {
    if( j > 254 ) break;

    if( j == ins->ins_pcury )
      set_bpen(&bitmaps[BM_PERF], PAL_BARMID);
    if( j == ins->ins_pcury+1 )
      set_bpen(&bitmaps[BM_PERF], PAL_BACK);
    
    if( j == ins->ins_PList.pls_Length )
      set_fpen(&bitmaps[BM_PERF], PAL_DISABLEDTEXT);

    n = ins->ins_PList.pls_Entries[j].ple_Note;
    if( n > 60 ) n = 0;
    
    sprintf( pbuf, "%03d %s%c %01d %01X%02X %01X%02X ",
      (int)j,
      notenames[n],
      ins->ins_PList.pls_Entries[j].ple_Fixed ? '*' : ' ',
      ins->ins_PList.pls_Entries[j].ple_Waveform,
      ins->ins_PList.pls_Entries[j].ple_FX[0]&0xf,
      ins->ins_PList.pls_Entries[j].ple_FXParam[0]&0xff,
      ins->ins_PList.pls_Entries[j].ple_FX[1]&0xf,
      ins->ins_PList.pls_Entries[j].ple_FXParam[1]&0xff );

    printstr(&bitmaps[BM_PERF], pbuf, 0, (i*16)+4);
  }

  if( k >= 0 )
  {
    set_fpen(&bitmaps[BM_PERF], PAL_BARLIGHT);
    fillrect_xy(&bitmaps[BM_PERF], 0, k*16+3, PERF_MX, k*16+3);
  }
  
  if( k < ((PERF_H>>4)-1) )
  {
    set_fpen(&bitmaps[BM_PERF], PAL_BARDARK);
    fillrect_xy(&bitmaps[BM_PERF], 0, k*16+20, PERF_MX, k*16+20);
  }

  set_fpen(&bitmaps[BM_PERF], PAL_CURSNORM);
  fillrect_xy(&bitmaps[BM_PERF],  3*8+4, 0,  3*8+4, PERF_MY);
  fillrect_xy(&bitmaps[BM_PERF],  8*8+4, 0,  8*8+4, PERF_MY);
  fillrect_xy(&bitmaps[BM_PERF], 10*8+4, 0, 10*8+4, PERF_MY);
  fillrect_xy(&bitmaps[BM_PERF], 14*8+4, 0, 14*8+4, PERF_MY);

  if( at->at_idoing == D_EDITING )
    set_fpen(&bitmaps[BM_PERF], PAL_CURSEDIT);
  else
    set_fpen(&bitmaps[BM_PERF], PAL_CURSNORM);
    
  cx = perf_xoff[ins->ins_pcurx]-2;
  cy = (k<<4)+2;
  mx = cx+8+3;
  my = cy+16+3;

  if( ins->ins_pcurx == 0 )
    mx = cx+32+3;
    
  fillrect_xy(&bitmaps[BM_PERF], cx, cy, mx, cy+1);
  fillrect_xy(&bitmaps[BM_PERF], cx, my-1, mx, my);
  fillrect_xy(&bitmaps[BM_PERF], cx, cy, cx+1, my);
  fillrect_xy(&bitmaps[BM_PERF], mx-1, cy, mx, my);

  put_bitmap( BM_PERF, PERF_X, PERF_Y, PERF_W, PERF_H );
}

void gui_render_inslistb( BOOL force )
{
  struct ahx_tune *at;
  struct ahx_instrument *in;
  int16 curi;
  int32 i, j, k;
  TEXT tmp[32];
  
  curi = 0;
  
  at = curtune;
  if( at )
  {
    if( at->at_curpanel != PN_INSED )
      return;
    
    curi = at->at_curins;
  }
  
  if( ( inslsb_lastcuri == curi ) &&
      ( inslsb_lasttune == at ) &&
      ( inslsb_lasttopi == at->at_topinsb ) &&
      ( force == FALSE ) )
    return;

  set_fpen(&bitmaps[BM_INSLISTB], PAL_BACK);
  fillrect_xy(&bitmaps[BM_INSLISTB], 0, 0, INSLSTB_MX, INSLSTB_MY);

  if( at )
  {
    if( curi < at->at_topinsb )     at->at_topinsb = curi;
    if( curi > (at->at_topinsb+((INSLSTB_H>>4)-1)) ) at->at_topinsb = curi-((INSLSTB_H>>4)-1);
    
    if( at->at_topinsb < 1 ) at->at_topinsb = 1;

    set_pens(&bitmaps[BM_INSLISTB], PAL_TEXT, PAL_BACK);
    for( i=0, j=at->at_topinsb; i<(INSLSTB_H>>4); i++, j++ )
    {
      sprintf( tmp, "%02d                   ", (int)j );
      in = &at->at_Instruments[j];

      for( k=0; k<18; k++ )
      {
        if( in->ins_Name[k] == 0 ) break;
        tmp[k+3] = in->ins_Name[k];
      }

      if( j == curi )
        set_bpen(&bitmaps[BM_INSLISTB], PAL_BARMID);
      if( j == curi+1 )
        set_bpen(&bitmaps[BM_INSLISTB], PAL_BACK);

      printstrlen(&bitmaps[BM_INSLISTB], tmp, 24, 0, i*16+4);
    }

    if( curi >= at->at_topinsb )
    {
      k = (curi-at->at_topinsb)*16+4;
      set_fpen(&bitmaps[BM_INSLISTB], PAL_BARLIGHT);
      fillrect_xy(&bitmaps[BM_INSLISTB], 0, k-1, INSLSTB_MX, k-1);
      set_fpen(&bitmaps[BM_INSLISTB], PAL_BARDARK);
      fillrect_xy(&bitmaps[BM_INSLISTB], 0, k+16, INSLSTB_MX, k+16);
    }

    set_fpen(&bitmaps[BM_INSLISTB], PAL_TEXT);
    fillrect_xy(&bitmaps[BM_INSLISTB], 20, 0, 20, INSLSTB_MY);
  }
  
  put_bitmap( BM_INSLISTB, INSLSTB_X, INSLSTB_Y, INSLSTB_W, INSLSTB_H );
      
  inslsb_lastcuri = curi;
  inslsb_lasttune = at;
  inslsb_lasttopi = at->at_topinsb;
}

void gui_check_nb( struct ahx_tune *at, uint32 nb, int32 val )
{
  if( trk_nb[nb].cnum != val )
  {
    trk_nb[nb].cnum = val;
    gui_render_nbox( at, PN_TRACKER, &trk_nb[nb] );
  }
}

void gui_check_inb( struct ahx_tune *at, uint32 nb, int32 val )
{
  if( ins_nb[nb].cnum != val )
  {
    ins_nb[nb].cnum = val;
    gui_render_nbox( at, PN_INSED, &ins_nb[nb] );
  }
}

void gui_set_various_things( struct ahx_tune *at )
{
  int32 i;

  if( at->at_NextPosNr != -1 )
    gui_check_nb( at, NB_POSNR, at->at_NextPosNr );
  else
    gui_check_nb( at, NB_POSNR, at->at_PosNr );

  gui_check_nb( at, NB_LENGTH, at->at_PositionNr );
  gui_check_nb( at, NB_TRKLEN, at->at_TrackLength );
  gui_check_nb( at, NB_RESPOS, at->at_Restart );
  gui_check_nb( at, NB_SSNR,   at->at_SubsongNr );
  
  if( at->at_SubsongNr > 0 )
  {
    if( ( trk_nb[NB_CURSS].cnum != at->at_curss ) || ( ( trk_nb[NB_CURSS].flags & NBF_ENABLED ) == 0 ) )
    {
      trk_nb[NB_CURSS].flags |= NBF_ENABLED;
      trk_nb[NB_CURSS].cnum = at->at_curss;
      gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_CURSS] );
    }
  } else {
    if( ( trk_nb[NB_CURSS].flags & NBF_ENABLED ) != 0 )
    {
      trk_nb[NB_CURSS].flags &= ~NBF_ENABLED;
      trk_nb[NB_CURSS].cnum = 0;
      gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_CURSS] );
    }
  }

  if( at->at_curss > 0 )
  {
    if( ( trk_nb[NB_SSPOS].cnum != at->at_Subsongs[at->at_curss-1] ) ||
        ( ( trk_nb[NB_SSPOS].flags & NBF_ENABLED ) == 0 ) )
    {
      if( at->at_SubsongNr > 0 )
      {
        trk_nb[NB_SSPOS].flags |= NBF_ENABLED;
        trk_nb[NB_SSPOS].cnum = at->at_Subsongs[at->at_curss-1];
        gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_SSPOS] );
      } else {
        trk_nb[NB_SSPOS].flags &= ~NBF_ENABLED;
        trk_nb[NB_SSPOS].cnum = 0;
        gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_SSPOS] );
      }
    }
  } else {
    if( ( trk_nb[NB_SSPOS].flags & NBF_ENABLED ) != 0 )
    {
      trk_nb[NB_SSPOS].cnum = 0;
      trk_nb[NB_SSPOS].flags &= ~NBF_ENABLED;
      gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_SSPOS] );
    }
  }

  gui_check_nb( at, NB_CHANS,       at->at_Channels );  
  gui_check_nb( at, NB_MIXG,        at->at_mixgainP );
  gui_check_nb( at, NB_SPEEDMULT,   at->at_SpeedMultiplier );
  gui_check_nb( at, NB_DRUMPADMODE, at->at_drumpadmode );

  if( ( at != NULL ) && ( ins_nb[INB_INS].cnum != at->at_curins ) )
  {
    ins_nb[INB_INS].cnum = at->at_curins;
    gui_render_nbox( at, PN_INSED, &ins_nb[INB_INS] );
  }
  
  if( ( at == NULL ) || ( at->at_curins == 0 ) )
  {
    for( i=1; i<INB_END; i++ )
    {
      ins_nb[i].cnum = ins_nb[i].min;
      ins_nb[i].flags &= ~NBF_ENABLED;
      gui_render_nbox( at, PN_INSED, &ins_nb[i] );
    }
  } else {
    struct ahx_instrument *ip;
    
    ip = &at->at_Instruments[at->at_curins];

    for( i=1; i<INB_END; i++ )
    {
      ins_nb[i].flags |= NBF_ENABLED;
      gui_render_nbox( at, PN_INSED, &ins_nb[i] );
    }
    
    gui_check_inb( at, INB_VOL,     ip->ins_Volume );
    gui_check_inb( at, INB_WAVELEN, ip->ins_WaveLength );
    
    switch( ip->ins_WaveLength )
    {
      case 0:
        i = 0x20;
        break; 
      case 1:
        i = 0x10;
        break;
      case 2:
        i = 0x08;
        break;
      case 3:
        i = 0x04;
        break;
      case 4:
        i = 0x02;
        break;
      default:
        i = 0x01;
        break;
    }
    
    ins_nb[INB_SQRLOWER].min = i;
    ins_nb[INB_SQRUPPER].min = i;

    if( ip->ins_SquareLowerLimit < i )
      ip->ins_SquareLowerLimit = i;
    if( ip->ins_SquareUpperLimit < i )
      ip->ins_SquareUpperLimit = i;

    gui_check_inb( at, INB_ATTACK,    ip->ins_Envelope.aFrames );
    gui_check_inb( at, INB_AVOL,      ip->ins_Envelope.aVolume );
    gui_check_inb( at, INB_DECAY,     ip->ins_Envelope.dFrames );
    gui_check_inb( at, INB_DVOL,      ip->ins_Envelope.dVolume );
    gui_check_inb( at, INB_SUSTAIN,   ip->ins_Envelope.sFrames );
    gui_check_inb( at, INB_RELEASE,   ip->ins_Envelope.rFrames );
    gui_check_inb( at, INB_RVOL,      ip->ins_Envelope.rVolume );
    gui_check_inb( at, INB_VIBDELAY,  ip->ins_VibratoDelay );
    gui_check_inb( at, INB_VIBDEPTH,  ip->ins_VibratoDepth );
    gui_check_inb( at, INB_VIBSPEED,  ip->ins_VibratoSpeed );
    gui_check_inb( at, INB_SQRLOWER,  ip->ins_SquareLowerLimit );
    gui_check_inb( at, INB_SQRUPPER,  ip->ins_SquareUpperLimit );
    gui_check_inb( at, INB_SQRSPEED,  ip->ins_SquareSpeed );
    gui_check_inb( at, INB_FLTLOWER,  ip->ins_FilterLowerLimit );
    gui_check_inb( at, INB_FLTUPPER,  ip->ins_FilterUpperLimit );
    gui_check_inb( at, INB_FLTSPEED,  ip->ins_FilterSpeed );
    gui_check_inb( at, INB_PERFSPEED, ip->ins_PList.pls_Speed );
    gui_check_inb( at, INB_PERFLEN,   ip->ins_PList.pls_Length );
    gui_check_inb( at, INB_HARDCUT,   ip->ins_HardCutReleaseFrames );
    gui_check_inb( at, INB_RELCUT,    ip->ins_HardCutRelease );
  }
  
  if( at == NULL )
  {
    if( tbx[TB_SONGNAME].content != NULL )
    {
      tbx[TB_SONGNAME].content = NULL;
      gui_render_tbox( &mainbm, &tbx[TB_SONGNAME] );
    }
  } else {
    if( tbx[TB_SONGNAME].content != at->at_Name )
    {
      tbx[TB_SONGNAME].content = at->at_Name;
      tbx[TB_SONGNAME].spos = 0;
      tbx[TB_SONGNAME].cpos = 0;
      gui_render_tbox( &mainbm, &tbx[TB_SONGNAME] );
    }
  }

  if( at == NULL )
  {
    if( tbx[TB_INSNAME].content != NULL )
    {
      tbx[TB_INSNAME].content = NULL;
      gui_render_tbox( &mainbm, &tbx[TB_INSNAME] );
    }
  } else {
    if( tbx[TB_INSNAME].content != at->at_Instruments[at->at_curins].ins_Name )
    {
      tbx[TB_INSNAME].content = at->at_Instruments[at->at_curins].ins_Name;
      tbx[TB_INSNAME].spos = 0;
      tbx[TB_INSNAME].cpos = 0;
      gui_render_tbox( &mainbm, &tbx[TB_INSNAME] );
    }
  }
  
  if( ( curtune ) && ( curtune->at_curpanel == PN_INSED ) )
  {
    gui_render_perf( curtune, &curtune->at_Instruments[curtune->at_curins], FALSE );
    gui_render_inslistb( FALSE );
  }
}

void gui_setup_trkbbank( int32 which )
{
  int32 i;
  
  // Blank the bank!
  for( i=0; i<12; i++ ) 
  {
    tbank[i].x       = 744;
    tbank[i].y       = 120+200 + i*23;
    tbank[i].name    = NULL;
    tbank[i].action  = BBA_NOTHING;
    tbank[i].raction = BBA_NOTHING;
  }
  
  switch( which )
  {
    case 0:
      tbank[0].name   = "Cut";
      tbank[0].action = BBA_CUTTRK;
      tbank[0].raction= BBA_CUTTRK;
      tbank[1].name   = "Copy";
      tbank[1].action = BBA_COPYTRK;
      tbank[1].raction= BBA_COPYTRK;
      tbank[2].name   = "Paste";
      tbank[2].action = BBA_PASTE;
      tbank[2].raction= BBA_PASTE;
      tbank[3].name   = "Nt. Up";
      tbank[3].action = BBA_NOTEUP;
      tbank[4].name   = "Nt. Dn";
      tbank[4].action = BBA_NOTEDN;
      tbank[5].name   = "Scl. Up";
      tbank[5].action = BBA_SCRLUP;
      tbank[6].name   = "Scl. Dn";
      tbank[6].action = BBA_SCRLDN;
      tbank[7].name   = "Rvrs.";
      tbank[7].action = BBA_REVERSE;
      break;
  }

  set_font(&mainbm, FONT_PRP, FALSE);

  for( i=0; i<11; i++ )
  {
    if( tbank[i].name != NULL )
    {
#ifndef __SDL_WRAPPER__
      tbank[i].xo = 26 - ( IGraphics->TextLength( &mainbm.rp, tbank[i].name, strlen( tbank[i].name ) ) >> 1 );
#else
      int w, h;
      TTF_SizeText(mainbm.font, tbank[i].name, &w, &h);
      tbank[i].xo = 26 - (w/2);
#endif
    }
  }  
}

void gui_setup_buttonbank( int32 which )
{
  int32 i;

  // Blank the bank!
  for( i=0; i<16; i++ )
  {
    bbank[i].x       = 256 + (i&3)*80;
    bbank[i].y       = (i>>2)*24;
    bbank[i].name    = NULL;
    bbank[i].action  = BBA_NOTHING;
    bbank[i].raction = BBA_NOTHING;
  }
  
  switch( which )
  {
    case 0:
      bbank[0].name    = "New Tab";
      bbank[0].action  = BBA_NEWTAB;
      bbank[0].raction = BBA_CLONETAB;
      bbank[1].name    = "Load Mod";
      bbank[1].action  = BBA_LOADTUNE;
      bbank[2].name    = "Save AHX";
      bbank[2].action  = BBA_SAVEAHX;
      bbank[3].name    = "Save HVL";
      bbank[3].action  = BBA_SAVEHVL;
      bbank[4].name    = "Load Ins";
      bbank[4].action  = BBA_LOADINS;
      bbank[5].name    = "Save Ins";
      bbank[5].action  = BBA_SAVEINS;
      bbank[6].name    = "Ins Edit";
      bbank[6].action  = BBA_INSEDIT;
      bbank[7].name    = "Prefs";
      bbank[7].action  = BBA_PREFS;
      bbank[8].name    = "Autogain";
      bbank[8].action  = BBA_AUTOGAIN;
      bbank[9].name    = "Undo";
      bbank[9].action  = BBA_UNDO;
      bbank[9].raction = BBA_REDO;
      bbank[10].name   = "Optim";
      bbank[10].action = BBA_OPTIMISE;
      bbank[10].raction= BBA_OPTIMISE_MORE;
      bbank[11].name   = "About";
      bbank[11].action = BBA_ABOUT;
      bbank[12].name   = "Play Song";
      bbank[12].action = BBA_PLAYSONG;
      bbank[12].raction= BBA_CONTSONG;
      bbank[13].name   = "Play Pos";
      bbank[13].action = BBA_PLAYPOS;
      bbank[13].raction= BBA_CONTPOS;
      bbank[14].name   = "Stop";
      bbank[14].action = BBA_STOP;
      bbank[14].raction= BBA_STOP;
      bbank[15].name   = "Zap";
      bbank[15].action = BBA_ZAP;
      break;

#ifdef __SDL_WRAPPER__
    case 1:
      bbank[0].name    = "Zap song";
      bbank[0].action  = BBA_ZAP_SONG;
      bbank[1].name    = "Zap Tracks";
      bbank[1].action  = BBA_ZAP_TRACKS;
      bbank[2].name    = "Zap Posns";
      bbank[2].action  = BBA_ZAP_POSNS;
      bbank[3].name    = "Zap Instrs";
      bbank[3].action  = BBA_ZAP_INSTRS;
      bbank[12].name   = "Zap All";
      bbank[12].action = BBA_ZAP_ALL;
      bbank[15].name   = "Cancel";
      bbank[15].action = BBA_BACK;
      break;
#endif
  }

  set_font(&mainbm, FONT_PRP, FALSE);

  for( i=0; i<16; i++ )
  {
    if( bbank[i].name != NULL )
    {
#ifndef __SDL_WRAPPER__
      bbank[i].xo = 40 - ( IGraphics->TextLength( &mainbm.rp, bbank[i].name, strlen( bbank[i].name ) ) >> 1 );
#else
      int w, h;
      TTF_SizeText(mainbm.font, bbank[i].name, &w, &h);
      bbank[i].xo = 40 - (w/2);
#endif
    } 
  }  
}

void gui_render_bbank_texts( struct buttonbank *bbnk, int32 nb )
{
  int32 i;

  set_font(&mainbm, FONT_PRP, FALSE);
  set_fpen(&mainbm, PAL_BTNSHADOW);

  for( i=0; i<nb; i++ )
  {
    if( bbnk[i].name != NULL )
    {
      printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo+1, bbnk[i].y+5);
    }
  }

  set_fpen(&mainbm, PAL_BTNTEXT);

  for( i=0; i<nb; i++ )
  {
    if( bbnk[i].name != NULL )
    {
      printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo, bbnk[i].y+4);
    }
  }
}

void gui_render_main_buttonbank( void )
{
  put_bitmap( BM_BUTBANKR, 256, 0, 4*80, 4*24 );
  gui_render_bbank_texts( &bbank[0], 16 );
}

void gui_render_track_buttonbank( void )
{
  put_bitmap( BM_TRKBANKR, 744, 120+200, 52, 12*23 );
  gui_render_bbank_texts( &tbank[0], 12 );
}

void gui_render_tabs( void )
{
  int32 ntabs, rtabs, tabw, tabsw, i, j, k, l, m, n, o, tabh;
  struct ahx_tune *at;
  const TEXT *tname;

  put_bitmap( BM_TAB_AREA, 0, 96, 800, 24 );
  
  tabh = 24;
  if( bitmaps[BM_TAB_MID].h >= 26 )
  {
    tabh = 26;
    l = PN_TRACKER;
    if( curtune )
      l = curtune->at_curpanel;

    switch( l )
    {
      case PN_INSED:
        put_bitmap( BM_BG_INSED, 0, 120, 800, 2 );
        break;
      default:
        put_bitmap( BM_BG_TRACKER, 0, 120, 800, 2 );
        break;
    }
  }

  for( i=0; i<8; i++ )
  {
    ttab[i].tune = NULL;
    gui_zap_zone( &zones[0], &numzones, MAX_ZONES, &ttab[i].zone );
  }

  IExec->ObtainSemaphore( rp_list_ss );

  ntabs = 0;
  at = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
  while( at )
  {
    ntabs++;
    at = (struct ahx_tune *)IExec->GetSucc(&at->at_ln);
  }
  
  if( ntabs == 0 )
  {
    curtune = NULL;
    IExec->ReleaseSemaphore( rp_list_ss );
    return;
  }
  
  if( curtune == NULL )
    curtune = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
  
  // Ensure phantom tab!
  rtabs = ntabs;
  if( rtabs < 2 ) rtabs = 2;
  if( rtabs > 8 ) rtabs = 8;
  
  // Space per tab
  tabsw = (800<<8) / rtabs;
  
  // Actual tab width
  tabw  = (tabsw>>8)&~15;
  if( tabw == (tabsw>>8) ) tabw -= 8;
  
  // Tab offset
  k = ((tabsw>>1)-(tabw<<7));

  set_font(&mainbm, FONT_PRP, FALSE);

  // Draw them!
  at = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
  for( i=0, j=k; i<rtabs; i++, j+=tabsw )
  {
    if( i >= ntabs ) break;
    
    ttab[i].tune = at;
    ttab[i].zone = gui_add_zone( &zones[0], &numzones, MAX_ZONES, (j>>8), 96, tabw, 24 );

    if( at == curtune )
    {
      l = BM_TAB_LEFT;
      o = BM_TAB_TEXT;
      m = tabh;
    } else {
      l = BM_ITAB_LEFT;
      o = BM_ITAB_TEXT;
      m = 24;
    }
    
    tname = at->at_Name[0] ? at->at_Name : "[Untitled]";

#ifndef __SDL_WRAPPER__
    n = IGraphics->TextLength( &mainbm.rp, tname, strlen( tname ) );
#else
    TTF_SizeText(mainbm.font, tname, &n, &k);
#endif
    if( !tabtextback ) o = l+1;

    put_bitmap( l+0, j>>8, 96, 20, m );
    for( k=0; k<(tabw-36); k+=32 )
    {
      put_bitmap( o, (j>>8)+20+k, 96, (tabw-36)-k < 32 ? (tabw-36)-k : 32, m );
      if( k > n ) o = l+1;
    }
    put_bitmap( l+2, (j>>8)+tabw-16, 96, 16, m );
    
    k = textfit(&mainbm, tname, tabw-32);

    if( tabtextshad )
    {    
      set_fpen(&mainbm, PAL_TABSHADOW);
      printstrlen(&mainbm, tname, k, (j>>8)+25, 100);
    }

    set_fpen(&mainbm, PAL_TABTEXT);
    printstrlen(&mainbm, tname, k, (j>>8)+24, 99);
    at = (struct ahx_tune *)IExec->GetSucc(&at->at_ln);
  }
  
  IExec->ReleaseSemaphore( rp_list_ss );
}

void gui_render_wavemeter( void )
{
  int32 i, v, off, delta;
  int16 *ba;

  set_fpen(&bitmaps[BM_WAVEMETERS], PAL_BACK);
  fillrect_xy(&bitmaps[BM_WAVEMETERS], 9,  7, 181+9, 37+ 7);
  fillrect_xy(&bitmaps[BM_WAVEMETERS], 9, 49, 181+9, 37+49);

  set_fpen(&bitmaps[BM_WAVEMETERS], PAL_WAVEMETER);

#ifndef __SDL_WRAPPER__
  ba = (int16 *)rp_audiobuffer[0];
#else
  ba = rp_audiobuffer;
#endif
  delta = rp_audiobuflen / 728;
  if( delta == 0 ) delta = 1;
  delta <<= 1;
  
  for( i=9, off=0; i<(9+182); i++, off += delta )
  {
    v = (ba[off] >> 10) + 16 + 10;
    if( v < (0+10) ) v = 0+10;
    if( v > (31+10) ) v = 31+10;
#ifndef __SDL_WRAPPER__
    if( i == 9 )
      IGraphics->Move( &bitmaps[BM_WAVEMETERS].rp, 9, v );
    else
      IGraphics->Draw( &bitmaps[BM_WAVEMETERS].rp, i, v );
#else
     fillrect_xy(&bitmaps[BM_WAVEMETERS], i, v, i, v);
#endif
  } 

  ba++;

  for( i=9, off=0; i<(9+182); i++, off += delta )
  {
    v = (ba[off] >> 10) + 16 + 52;
    if( v < (0+52) ) v = 0+52;
    if( v > (31+52) ) v = 31+52;
#ifndef __SDL_WRAPPER__
    if( i == 9 )
      IGraphics->Move( &bitmaps[BM_WAVEMETERS].rp, 9, v );
    else
      IGraphics->Draw( &bitmaps[BM_WAVEMETERS].rp, i, v );
#else
     fillrect_xy(&bitmaps[BM_WAVEMETERS], i, v, i, v);
#endif
  } 
  put_bitmap( BM_WAVEMETERS, 576, 0, 200, 96 );
}

void gui_render_vumeters( void )
{
  int32 lch, dch, v, i;

  if( curtune == NULL ) return;
  
  if( curtune->at_curpanel != PN_TRACKER )
    return;
  
  dch = curtune->at_Channels;
  if( dch > 6 ) dch = 6;
  lch = curtune->at_curlch;
  
  for( i=0; i<dch; i++ )
  {
    v = curtune->at_Voices[i+lch].vc_VUMeter >> 7;
    if( v > 64 ) v = 64;
    
    if( ( vum_needclr ) && ( v < vum_last[i+lch] ) && ( v < 64 ) )
    {
#ifndef __SDL_WRAPPER__
      IGraphics->BltBitMapRastPort( bitmaps[BM_TRACKED].bm, 64+i*120, 64, mainwin->RPort, TRACKED_X+64+i*120, TRACKED_Y+64, 32, 64-v, 0x0C0 );
#else
      SDL_Rect srect = { .x = 64+i*120, .y = 64, .w = 32, .h = 64-v };
      SDL_Rect drect = { .x = TRACKED_X+64+i*120, .y = TRACKED_Y+64, .w = 32, .h = 64-v };
      SDL_BlitSurface( bitmaps[BM_TRACKED].srf, &srect, ssrf, &drect );
      needaflip = TRUE;
#endif
    }
    
    if( v > 0 )
      put_bitmap( BM_VUMETER, TRACKED_X+64+i*120, (TRACKED_Y+128)-v, 32, v );
    
    vum_last[i+lch] = v;
  }
  
  vum_needclr = TRUE;
}

void gui_render_tracked( BOOL force )
{
  int16 posnr=0, notenr=0, trklen=0, trkmax=0;
  struct ahx_tune *at;
  struct ahx_step *stp;
  int32 i, j, k, l, bw, lch, dch, ech;
  BOOL needrestore, hrestore;
  TEXT pbuf[256];
  
  i = PN_TRACKER;
  if( curtune )
    i = curtune->at_curpanel;
  
  if( i != PN_TRACKER ) return;
  
  bw = TRACKED_W;
  lch = dch = 0;
  
  at = curtune;
  if( at )
  {
    if( at->at_cbmarktrack != -1 )
    {
      if( ( at->at_doing == D_EDITING ) || ( at->at_doing == D_IDLE ) )
      {
        // Do the full-on draw
        force = TRUE;
        at->at_cbmarkcurnote = at->at_NoteNr;
        at->at_cbmarkcurx    = at->at_tracked_curs%9;
        
        if( at->at_cbmarkmarknote > at->at_cbmarkcurnote )
        {
          at->at_cbmarkstartnote = at->at_cbmarkcurnote;
          at->at_cbmarkendnote   = at->at_cbmarkmarknote;
        } else {
          at->at_cbmarkstartnote = at->at_cbmarkmarknote;
          at->at_cbmarkendnote   = at->at_cbmarkcurnote;
        }

        i = at->at_tracked_curs/9+at->at_curlch;
        j = at->at_Positions[at->at_PosNr].pos_Track[i];

        if( j == at->at_cbmarktrack )
        {
          if( at->at_cbmarkmarkx > at->at_cbmarkcurx )
          {
            at->at_cbmarkleftx  = at->at_cbmarkcurx;
            at->at_cbmarkrightx = at->at_cbmarkmarkx;
          } else {
            at->at_cbmarkleftx  = at->at_cbmarkmarkx;
            at->at_cbmarkrightx = at->at_cbmarkcurx;
          }
        }
    
        at->at_cbmarkbits = 0;
        if( at->at_cbmarkleftx < 3 ) at->at_cbmarkbits |= CBF_NOTES;
        if( ( at->at_cbmarkleftx < 6 ) && ( at->at_cbmarkrightx > 2 ) ) at->at_cbmarkbits |= CBF_CMD1;
        if( at->at_cbmarkrightx > 5 ) at->at_cbmarkbits |= CBF_CMD2;
      } else {
        // Cancel if not editing or idle
        at->at_cbmarktrack = -1;
      }
    }
  
    posnr  = at->at_PosNr;
    notenr = at->at_NoteNr;
    trklen = at->at_TrackLength;
    trkmax = trklen-1;
    dch    = at->at_Channels;
    if( dch > 6 ) dch = 6;
    lch    = at->at_curlch;
  }
  
  if( force == FALSE )
  {
    if( ( at     == trked_lasttune ) &&
        ( posnr  == trked_lastposnr ) &&
        ( notenr == trked_lastnotenr ) &&
        ( lch    == trked_lastlchan ) &&
        ( dch    == trked_lastchans ) )
      return;
  }
  
  ech = dch + lch;
  
  // Optimise for scrolling?
  if( ( at == trked_lasttune ) && 
      ( posnr == trked_lastposnr ) &&
      ( notenr == trked_lastnotenr+1 ) &&
      ( lch == trked_lastlchan ) &&
      ( dch == trked_lastchans ) &&
      ( force == FALSE ) &&
      ( at != NULL ) )
  {
    bw = (3*8)+(dch*15*8)-1;
    if( bw > TRACKED_W ) bw = TRACKED_W;
#ifndef __SDL_WRAPPER__
    IGraphics->ScrollRaster( &bitmaps[BM_TRACKED].rp, 0, 16, 0, 0, bw, TRACKED_MY );
#else
    {
      SDL_Rect srect = { .x = 0, .y = 16, .w = bw, .h = TRACKED_H-16 };
      SDL_Rect drect = { .x = 0, .y = 0, .w = bw, .h = TRACKED_H-16 };
      SDL_BlitSurface(bitmaps[BM_TRACKED].srf, &srect, bitmaps[BM_TRACKED].srf, &drect);
    }
#endif

    set_fpen(&bitmaps[BM_TRACKED], PAL_BACK);
    fillrect_xy(&bitmaps[BM_TRACKED], 0, 256, TRACKED_MX, TRACKED_MY );
    fillrect_xy(&bitmaps[BM_TRACKED], 0, 7*16-2, TRACKED_MX, 7*16-1 );
//    IGraphics->Move( &bitmaps[BM_TRACKED].rp, 0, 7*16-1 );
//    IGraphics->Draw( &bitmaps[BM_TRACKED].rp, bw, 7*16-1 );

    hrestore = FALSE;
    set_fpen(&bitmaps[BM_TRACKED], PAL_TEXT);
    for( i=notenr-1, j=(7*16); i<notenr+3; i++, j+=16 )
    {
      if( i == notenr+2 )
      {
        i = notenr+8;
        j = (16*16);
      }
      if( i == notenr )
        set_bpen(&bitmaps[BM_TRACKED], PAL_BARMID);
      if( i == notenr+1 )
        set_bpen(&bitmaps[BM_TRACKED], PAL_BACK);
      
      if( ( i == at->at_rempos[0] ) ||
          ( i == at->at_rempos[1] ) ||
          ( i == at->at_rempos[2] ) ||
          ( i == at->at_rempos[3] ) ||
          ( i == at->at_rempos[4] ) )
      {
        set_fpen(&bitmaps[BM_TRACKED], PAL_FKEYHIGHLIGHT);
        hrestore = TRUE;
      } else {
        if( hrestore )
        {
          set_fpen(&bitmaps[BM_TRACKED], PAL_TEXT);
          hrestore = FALSE;
        }
      }
     
      if( ( i >= 0 ) && ( i < trklen ) )
      {
        sprintf( pbuf, "%02d ", (int)i );
        for( k=lch, l=0; k<ech; k++, l++ )
        {
          stp = &at->at_Tracks[at->at_Positions[posnr].pos_Track[k]&0xff][i];
          if( stp->stp_Instrument )
          {
            sprintf( &pbuf[l*15+3], "%s %02d %1X%02X %1X%02X ",
              notenames[stp->stp_Note],
              stp->stp_Instrument,
              stp->stp_FX,
              stp->stp_FXParam&0xff,
              stp->stp_FXb,
              stp->stp_FXbParam&0xff );
          } else {
            sprintf( &pbuf[l*15+3], "%s    %1X%02X %1X%02X ",
              notenames[stp->stp_Note],
              stp->stp_FX,
              stp->stp_FXParam&0xff,
              stp->stp_FXb,
              stp->stp_FXbParam&0xff );
          }

          if( pref_blankzeros )
          {
            if( ( stp->stp_FX == 0 ) && ( stp->stp_FXParam == 0 ) )
            {
              pbuf[l*15+10] = ' ';
              pbuf[l*15+11] = ' ';
              pbuf[l*15+12] = ' ';
            }

            if( ( stp->stp_FXb == 0 ) && ( stp->stp_FXbParam == 0 ) )
            {
              pbuf[l*15+14] = ' ';
              pbuf[l*15+15] = ' ';
              pbuf[l*15+16] = ' ';
            }
          }
        }
        printstr(&bitmaps[BM_TRACKED], pbuf, 0, j);
      }
    }

  } else {
  
    // Not optimised for scrolling
  
    if( ( !at ) || ( force ) )
    {
      set_fpen(&bitmaps[BM_TRACKED], PAL_BACK);
      fillrect_xy(&bitmaps[BM_TRACKED], 0, 0, TRACKED_MX, TRACKED_MY);
      set_fpen(&bitmaps[BM_TRACKED], PAL_BARMID);
      fillrect_xy(&bitmaps[BM_TRACKED], 0, 8*16, TRACKED_MX, 8*16+15);
    }

    if( at )
    {  
      if( ( force ) || ( lch != trked_lastlchan ) || ( posnr != trked_lastposnr ) )
      {
        set_font(&bitmaps[BM_TRACKBAR], FONT_PRP, FALSE);
        bm_to_bm(&bitmaps[BM_BG_TRACKER], 20+TRACKED_X, TRACKED_Y-142, &bitmaps[BM_TRACKBAR], 0, 0, 768, 19);

        for( j=0, k=lch, l=0; l<6; j+=15*8, k++, l++ )
        { 
          if( k < ech )
          {
            sprintf( pbuf, "Track %d", (int)k+1 );
            set_fpen(&bitmaps[BM_TRACKBAR], PAL_BTNSHADOW);
            printstr(&bitmaps[BM_TRACKBAR], pbuf, j+1, 2);
            set_fpen(&bitmaps[BM_TRACKBAR], PAL_BTNTEXT);
            printstr(&bitmaps[BM_TRACKBAR], pbuf, j, 1);
          }
        } 

        set_font(&bitmaps[BM_TRACKBAR], FONT_FIX, TRUE);
        set_pens(&bitmaps[BM_TRACKBAR], PAL_TEXT, PAL_BACK);
        for( j=11*8-2, k=lch, l=0; l<6; j+=15*8, k++, l++ )
        { 
          if( k < ech )
          {
            if( at->at_Voices[k].vc_SetTrackOn )
              bm_to_bm(&bitmaps[BM_CHANMUTE], 0, 0, &bitmaps[BM_TRACKBAR], j-20, 0, 14, 19);
            else
              bm_to_bm(&bitmaps[BM_CHANMUTE], 0, 19, &bitmaps[BM_TRACKBAR], j-20, 0, 14, 19);
            sprintf( pbuf, "%03d", at->at_Positions[posnr].pos_Track[k] );
            printstr(&bitmaps[BM_TRACKBAR], pbuf, j, 1);
          }
        } 
        put_bitmap( BM_TRACKBAR, 20+TRACKED_X, TRACKED_Y-22, TRACKED_W-24, 19 );
      }
    
      if( notenr < 8 )
      {
        set_fpen(&bitmaps[BM_TRACKED], PAL_BACK);
        fillrect_xy(&bitmaps[BM_TRACKED], 0, 0, TRACKED_MX, ((8-notenr)*16)-1);
      }
  
      if( notenr > trkmax-8 )
      {
        set_fpen(&bitmaps[BM_TRACKED], PAL_BACK);
        fillrect_xy(&bitmaps[BM_TRACKED], 0, ((trklen-notenr)+8)*16, TRACKED_MX, TRACKED_MY);
      }
      
      hrestore = FALSE;

      set_fpen(&bitmaps[BM_TRACKED], PAL_TEXT);
      for( i=notenr-8, j=0; i<notenr+9; i++, j+=16 )
      {
        if( i == notenr )
          set_bpen(&bitmaps[BM_TRACKED], PAL_BARMID);
        if( i == notenr+1 )
          set_bpen(&bitmaps[BM_TRACKED], PAL_BACK);
   
        if( ( i == at->at_rempos[0] ) ||
            ( i == at->at_rempos[1] ) ||
            ( i == at->at_rempos[2] ) ||
            ( i == at->at_rempos[3] ) ||
            ( i == at->at_rempos[4] ) )
        {
          set_fpen(&bitmaps[BM_TRACKED], PAL_FKEYHIGHLIGHT);
          hrestore = TRUE;
        } else {
          if( hrestore )
          {
            set_fpen(&bitmaps[BM_TRACKED], PAL_TEXT);
            hrestore = FALSE;
          }
        }

        if( ( i >= 0 ) && ( i < trklen ) )
        {
          sprintf( pbuf, "%02d ", (int)i );
          for( k=lch, l=0; k<ech; k++, l++ )
          {
            stp = &at->at_Tracks[at->at_Positions[posnr].pos_Track[k]&0xff][i];
            if( stp->stp_Instrument )
            {
              sprintf( &pbuf[l*15+3], "%s %02d %1X%02X %1X%02X ",
                notenames[stp->stp_Note],
                stp->stp_Instrument,
                stp->stp_FX,
                stp->stp_FXParam&0xff,
                stp->stp_FXb,
                stp->stp_FXbParam&0xff );
            } else {
              sprintf( &pbuf[l*15+3], "%s    %1X%02X %1X%02X ",
                notenames[stp->stp_Note],
                stp->stp_FX,
                stp->stp_FXParam&0xff,
                stp->stp_FXb,
                stp->stp_FXbParam&0xff );
            }

            if( pref_blankzeros )
            {
              if( ( stp->stp_FX == 0 ) && ( stp->stp_FXParam == 0 ) )
              {
                pbuf[l*15+10] = ' ';
                pbuf[l*15+11] = ' ';
                pbuf[l*15+12] = ' ';
              }

              if( ( stp->stp_FXb == 0 ) && ( stp->stp_FXbParam == 0 ) )
              {
                pbuf[l*15+14] = ' ';
                pbuf[l*15+15] = ' ';
                pbuf[l*15+16] = ' ';
              }
            }
          }
          printstr(&bitmaps[BM_TRACKED], pbuf, 0, j);
          
          if( at->at_cbmarktrack != -1 )
          {
            needrestore = FALSE;
            
            l = at->at_tracked_curs/9;
            k = l+lch;
            
            // Render any marked blocks
            if( ( k >= lch ) && ( k<ech ) )
            {
              if( ( i >= at->at_cbmarkstartnote ) &&
                  ( i <= at->at_cbmarkendnote ) &&
                  ( at->at_Positions[posnr].pos_Track[k] == at->at_cbmarktrack ) )
              {
                stp = &at->at_Tracks[at->at_Positions[posnr].pos_Track[k]&0xff][i];

                if( needrestore == FALSE )
                {
                  set_pens(&bitmaps[BM_TRACKED], PAL_BACK, PAL_TEXT);
                  needrestore = TRUE;
                }
                
                if( stp->stp_Instrument )
                {
                  sprintf( pbuf, "%s %02d %1X%02X %1X%02X",
                    notenames[stp->stp_Note],
                    stp->stp_Instrument,
                    stp->stp_FX,
                    stp->stp_FXParam&0xff,
                    stp->stp_FXb,
                    stp->stp_FXbParam&0xff );
                } else {
                  sprintf( pbuf, "%s    %1X%02X %1X%02X",
                    notenames[stp->stp_Note],
                    stp->stp_FX,
                    stp->stp_FXParam&0xff,
                    stp->stp_FXb,
                    stp->stp_FXbParam&0xff );
                }

                switch( at->at_cbmarkbits )
                {
                  case CBF_NOTES:
                    printstrlen(&bitmaps[BM_TRACKED], pbuf, 6, 24+(l*120), j );
                    break;
                  case CBF_NOTES|CBF_CMD1:
                    printstrlen(&bitmaps[BM_TRACKED], pbuf, 10, 24+(l*120), j );
                    break;
                  case CBF_NOTES|CBF_CMD1|CBF_CMD2:  // everything
                    printstr(&bitmaps[BM_TRACKED], pbuf, 24+(l*120), j );
                    break;
                  case CBF_CMD1:
                    printstrlen(&bitmaps[BM_TRACKED], &pbuf[7], 3, 24+(7*8)+(l*120), j );
                    break;
                  case CBF_CMD1|CBF_CMD2:
                    printstrlen(&bitmaps[BM_TRACKED], &pbuf[7], 7, 24+(7*8)+(l*120), j );
                    break;
                  case CBF_CMD2:
                    printstrlen(&bitmaps[BM_TRACKED], &pbuf[11], 3, 24+(11*8)+(l*120), j );
                    break;
                }
              }
            }
            if( needrestore )
            {
              set_pens(&bitmaps[BM_TRACKED], PAL_TEXT, PAL_BACK);
            }
          }
        }
      }
    }
  }

  trked_lasttune   = at;
  trked_lastposnr  = posnr;
  trked_lastnotenr = notenr;
  trked_lastchans  = dch;
  trked_lastlchan  = lch;

  set_fpen(&bitmaps[BM_TRACKED], PAL_BARMID);
  for( i=0; i<6*120; i+=120 )
  {
    fillrect_xy(&bitmaps[BM_TRACKED], i+24+28, 0, i+24+28, TRACKED_MY);
    fillrect_xy(&bitmaps[BM_TRACKED], i+24+52, 0, i+24+52, TRACKED_MY);
    fillrect_xy(&bitmaps[BM_TRACKED], i+24+84, 0, i+24+84, TRACKED_MY);
  }

  set_fpen(&bitmaps[BM_TRACKED], PAL_BARLIGHT);
  fillrect_xy(&bitmaps[BM_TRACKED], 0, 8*16-1, TRACKED_MX, 8*16-1);
  set_fpen(&bitmaps[BM_TRACKED], PAL_BARDARK);
  fillrect_xy(&bitmaps[BM_TRACKED], 0, 9*16-1, TRACKED_MX, 9*16-1);
  set_fpen(&bitmaps[BM_TRACKED], PAL_TEXT);
  fillrect_xy(&bitmaps[BM_TRACKED], 20, 0, 20, TRACKED_MY);
  
  for( i=120; i<6*120; i+=120 )
  {
    fillrect_xy(&bitmaps[BM_TRACKED], 20+i, 0, 20+i, TRACKED_MY);
  }
  
  if( at->at_editing == E_TRACK )
  {
    int32 cx, cy, mx, my;

    if( at->at_doing == D_EDITING )
      set_fpen(&bitmaps[BM_TRACKED], PAL_CURSEDIT);
    else
      set_fpen(&bitmaps[BM_TRACKED], PAL_CURSNORM);
    
    cx = tracked_xoff[at->at_tracked_curs]-2;
    cy = 8*16-2;
    mx = cx+8+3;
    my = cy+16+3;

    if( ( at->at_tracked_curs % 9 ) == 0 )
      mx = cx+24+3;
    
    fillrect_xy(&bitmaps[BM_TRACKED], cx, cy, mx, cy+1);
    fillrect_xy(&bitmaps[BM_TRACKED], cx, my-1, mx, my);
    fillrect_xy(&bitmaps[BM_TRACKED], cx, cy, cx+1, my);
    fillrect_xy(&bitmaps[BM_TRACKED], mx-1, cy, mx, my);
  }

  put_bitmap( BM_TRACKED, TRACKED_X, TRACKED_Y, bw, TRACKED_H );
  vum_needclr = FALSE;
}

void gui_render_inslist( BOOL force )
{
  struct ahx_tune *at;
  struct ahx_instrument *in;
  int16 curi;
  int32 i, j, k;
  TEXT tmp[32];
  
  curi = 0;
  
  at = curtune;
  if( at )
  {
    if( at->at_curpanel != PN_TRACKER )
      return;
    
    curi = at->at_curins;
  }
  
  if( ( insls_lastcuri == curi ) &&
      ( insls_lasttune == at ) &&
      ( insls_lasttopi == at->at_topins ) &&
      ( force == FALSE ) )
    return;

  set_fpen(&bitmaps[BM_INSLIST], PAL_BACK);
  fillrect_xy(&bitmaps[BM_INSLIST], 0, 0, INSLIST_MX, INSLIST_MY);

  if( at )
  {
    if( curi < at->at_topins )     at->at_topins = curi;
    if( curi > (at->at_topins+8) ) at->at_topins = curi-8;
    
    if( at->at_topins < 1 ) at->at_topins = 1;

    set_pens(&bitmaps[BM_INSLIST], PAL_TEXT, PAL_BACK);
    for( i=0, j=at->at_topins; i<9; i++, j++ )
    {
      sprintf( tmp, "%02d                      ", (int)j );
      in = &at->at_Instruments[j];

      for( k=0; k<21; k++ )
      {
        if( in->ins_Name[k] == 0 ) break;
        tmp[k+3] = in->ins_Name[k];
      }

      if( j == curi )
        set_bpen(&bitmaps[BM_INSLIST], PAL_BARMID);
      if( j == curi+1 )
        set_bpen(&bitmaps[BM_INSLIST], PAL_BACK);

      printstrlen(&bitmaps[BM_INSLIST], tmp, 24, 0, i*14+5);
    }

    if( curi >= at->at_topins )
    {
      k = (curi-at->at_topins)*14+5;
      set_fpen(&bitmaps[BM_INSLIST], PAL_BARLIGHT);
      fillrect_xy(&bitmaps[BM_INSLIST], 0, k-1, INSLIST_MX, k-1);
      set_fpen(&bitmaps[BM_INSLIST], PAL_BARDARK);
      fillrect_xy(&bitmaps[BM_INSLIST], 0, k+14, INSLIST_MX, k+14);
    }

    set_fpen(&bitmaps[BM_INSLIST], PAL_TEXT);
    fillrect_xy(&bitmaps[BM_INSLIST], 17, 0, 17, INSLIST_MY);
  }
  
  put_bitmap( BM_INSLIST, INSLIST_X, INSLIST_Y, INSLIST_W, INSLIST_H );
  insls_lastcuri = curi;
  insls_lasttune = at;
  insls_lasttopi = at->at_topins;
}

void gui_render_posed( BOOL force )
{
  struct ahx_tune *at;
  int16 posnr=0;
  int32 i, j, k, l, m, lch=0, dch, ech;
  TEXT pbuf[256];
  
  at = curtune;
  dch = 0;
  if( at )
  {
    if( at->at_curpanel != PN_TRACKER )
      return;

    if( at->at_NextPosNr != -1 )
      posnr = at->at_NextPosNr;
    else
      posnr = at->at_PosNr;
    lch = at->at_curlch;
    dch = at->at_Channels;
    if( dch > 6 ) dch = 6;
  }
  
  ech = lch + dch;

  if( force == FALSE )
  {
    if( ( at == posed_lasttune ) &&
        ( posnr == posed_lastposnr ) &&
        ( lch == posed_lastlchan ) &&
        ( dch == posed_lastchans ) )
      return;
  }
  
  if( posed_lastposnr != posnr )
  {
    trk_nb[NB_POSNR].cnum = posnr;
    gui_render_nbox( at, PN_TRACKER, &trk_nb[NB_POSNR] );
  }

  posed_lasttune  = at;
  posed_lastposnr = posnr;
  posed_lastlchan = lch;
  posed_lastchans = dch;
  
  if( ( !at ) || ( force ) )
  {
    set_fpen(&bitmaps[BM_POSED], PAL_BACK);
    fillrect_xy(&bitmaps[BM_POSED], 0, 0, POSED_MX, POSED_MY);
    set_fpen(&bitmaps[BM_POSED], PAL_BARMID);
    fillrect_xy( &bitmaps[BM_POSED], 0, 3*14, POSED_MX, 3*14+13 );
  }
  
  if( at )
  {
    if( posnr < 3 )
    {
      set_fpen(&bitmaps[BM_POSED], PAL_BACK);
      fillrect_xy( &bitmaps[BM_POSED], 0, 0, POSED_MX, ((3-posnr)*14)-1 );
    }
  
    if( posnr > 996 )
    {
      set_fpen(&bitmaps[BM_POSED], PAL_BACK);
      fillrect_xy( &bitmaps[BM_POSED], 0, (1003-posnr)*14, POSED_MX, POSED_MY );
    }

    set_fpen(&bitmaps[BM_POSED], PAL_TEXT);
    for( i=posnr-3, j=0; i<posnr+4; i++, j+=14 )
    {
      if( i == posnr )
        set_bpen(&bitmaps[BM_POSED], PAL_BARMID);
      if( i == posnr+1 )
        set_bpen(&bitmaps[BM_POSED], PAL_BACK);
     
      if( ( i >= 0 ) && ( i < 1000 ) )
      {
        sprintf( pbuf, "%03d ", (int)i );
        for( k=lch, l=0; k<ech; k++, l++ )
          sprintf( &pbuf[l*7+4], "%03d %02X ",
            at->at_Positions[i].pos_Track[k]&0xff,
            at->at_Positions[i].pos_Transpose[k]&0xff );
        printstr(&bitmaps[BM_POSED], pbuf, 0, j);
      }
    }
    
    // Any marked areas?
    if( at->at_cbpmarkmarklft != -1 )
    {
      BOOL showit;
      
      at->at_cbpmarktop    = at->at_cbpmarkmarkpos;
      at->at_cbpmarklft    = at->at_cbpmarkmarklft;
      at->at_cbpmarkbot    = at->at_PosNr;
      at->at_cbpmarkrgt    = (((at->at_posed_curs/5)+lch)<<1)|((at->at_posed_curs%5)>2?1:0);
      
      if( at->at_cbpmarktop > at->at_cbpmarkbot )
      {
        i                 = at->at_cbpmarktop;
        at->at_cbpmarktop = at->at_cbpmarkbot;
        at->at_cbpmarkbot = i;
      }

      if( at->at_cbpmarklft > at->at_cbpmarkrgt )
      {
        i                 = at->at_cbpmarklft;
        at->at_cbpmarklft = at->at_cbpmarkrgt;
        at->at_cbpmarkrgt = i;
      }
      
      at->at_cbpmarklftcol = at->at_cbpmarklft & 1;
      at->at_cbpmarkrgtcol = at->at_cbpmarkrgt & 1;
      at->at_cbpmarklft    >>= 1;
      at->at_cbpmarkrgt    >>= 1; 

      showit = TRUE;      
      if( ( at->at_cbpmarkrgt < lch ) ||
          ( at->at_cbpmarklft >= ech ) ||
          ( at->at_cbpmarkbot < (posnr-3) ) ||
          ( at->at_cbpmarktop > (posnr+3) ) )
        showit = FALSE;
      
      if( showit )
      {
        int32 kl, kr, it, ib;
        
        kl = at->at_cbpmarklft>lch ? at->at_cbpmarklft : lch;
        kr = at->at_cbpmarkrgt<(ech-1) ? at->at_cbpmarkrgt : (ech-1);
        it = at->at_cbpmarktop>(posnr-3) ? at->at_cbpmarktop : (posnr-3);
        ib = at->at_cbpmarkbot<(posnr+3) ? at->at_cbpmarkbot : (posnr+3);
        
        set_pens(&bitmaps[BM_POSED], PAL_BACK, PAL_TEXT);
        
        pbuf[0]=0;

        // Nice! or... more accurately Confusing!
        for( i=it, j=14*(it-(posnr-3));
             i<=ib;
             i++, j+=14 )
        {
          
          for( m=0, k=kl, l=0; k<=kr; k++, l++ )
          {
            if( ( k>kl ) || ( at->at_cbpmarklftcol == 0 ) )
            {
              sprintf( &pbuf[m], "%03d ", at->at_Positions[i].pos_Track[k]&0xff );
              m+=4;
            }
            if( ( k<kr ) || ( at->at_cbpmarkrgtcol != 0 ) )
            {
              sprintf( &pbuf[m], "%02X ", at->at_Positions[i].pos_Transpose[k]&0xff );
              m+=3;
            }
          }
          pbuf[m-1] = 0;
          printstr(&bitmaps[BM_POSED], pbuf, ((kl-lch)*49)+(at->at_cbpmarklftcol*28)+28, j);
        }
      }
    }

    set_pens(&bitmaps[BM_POSED], PAL_POSEDCHIND, PAL_BARMID);
    for( k=lch, l=0; k<ech; k++, l++ )
    {
      sprintf( pbuf, "%d ", (int)k+1 );
      printstr(&bitmaps[BM_POSED], pbuf, l*49+25, 0);
    }      
    set_bpen(&bitmaps[BM_POSED], PAL_BACK);
  }

  set_fpen(&bitmaps[BM_POSED], PAL_BARLIGHT);
  fillrect_xy(&bitmaps[BM_POSED], 0, 3*14-1, POSED_MX, 3*14-1);
  set_fpen(&bitmaps[BM_POSED], PAL_BARDARK);
  fillrect_xy(&bitmaps[BM_POSED], 0, 4*14, POSED_MX, 4*14);

  set_fpen(&bitmaps[BM_POSED], PAL_TEXT);
  fillrect_xy(&bitmaps[BM_POSED], 24, 0, 24, POSED_MY);
  
  for( i=1; i<6; i++ )
  {
    fillrect_xy(&bitmaps[BM_POSED], 24+i*49, 0, 24+i*49, POSED_MY);
  }

  if( at->at_editing == E_POS )
  {
    int32 cx, cy, mx, my;
    
    cx = posed_xoff[at->at_posed_curs]-2;
    cy = 3*14-2;
    mx = cx+7+3;
    my = cy+14+3;

    fillrect_xy(&bitmaps[BM_POSED], cx, cy, mx, cy+1 );
    fillrect_xy(&bitmaps[BM_POSED], cx, my-1, mx, my );
    fillrect_xy(&bitmaps[BM_POSED], cx, cy, cx+1, my );
    fillrect_xy(&bitmaps[BM_POSED], mx-1, cy, mx, my );
  }
  
  put_bitmap( BM_POSED, POSED_X, POSED_Y, POSED_W, POSED_H );
}

void gui_render_tracker( BOOL force )
{
  int32 panel, i;

  if( curtune == NULL )
    panel = PN_TRACKER;
  else
    panel = curtune->at_curpanel;
  
  if( panel != PN_TRACKER ) return;

  gui_render_posed( force );
  gui_render_tracked( force );
  gui_render_inslist( force );
  gui_set_various_things( curtune );
  for( i=0; i<TB_END; i++ )
    gui_render_tbox( &mainbm, &tbx[i] );  
}

void gui_render_timer( BOOL force )
{
  TEXT tbuf[128];
  if( curtune == NULL ) return;
  if( curtune->at_curpanel != PN_TRACKER ) return;

  if( !force )
  {
    if( ( tmr_lasttune == curtune ) &&
        ( tmr_lasttime == ((curtune->at_hours<<16)|(curtune->at_mins<<8)|curtune->at_secs) ) )
      return;
  }
  
  tmr_lasttune = curtune;
  tmr_lasttime = (curtune->at_hours<<16)|(curtune->at_mins<<8)|curtune->at_secs;

  sprintf( tbuf, "%02d:%02d:%02d", curtune->at_hours, curtune->at_mins, curtune->at_secs );  

  set_font(&mainbm, FONT_FIX, FALSE);
  set_fpen(&mainbm, PAL_BACK);
  fillrect_xy(&mainbm, 546, 132, 546+65, 132+15 );
  set_fpen(&mainbm, PAL_TEXT);
  printstr(&mainbm, tbuf, 546, 132);
}

void gui_render_tunepanel( BOOL force )
{
  int32 panel, i;

  if( curtune == NULL )
    panel = PN_TRACKER;
  else
    panel = curtune->at_curpanel;
        
  switch( panel )
  {
    case PN_TRACKER:
      if( force )
      {
        put_bitmap( BM_BG_TRACKER, 0, 120, 800, 480 );
        gui_render_track_buttonbank();
        gui_render_tabs();
        for( i=0; i<NB_END; i++ )
          gui_render_nbox( curtune, PN_TRACKER, &trk_nb[i] );
      }
      gui_render_timer( force );
      gui_render_tracker( force );
      break;
    
    case PN_INSED:
      if( force )
      {
        put_bitmap( BM_BG_INSED, 0, 120, 800, 480 );
        gui_render_tabs();
        gui_render_perf( curtune, curtune ? &curtune->at_Instruments[curtune->at_curins] : NULL, force );
        for( i=0; i<INB_END; i++ )
          gui_render_nbox( curtune, PN_INSED, &ins_nb[i] );
        for( i=0; i<TB_END; i++ )
          gui_render_tbox( &mainbm, &tbx[i] );  
        gui_render_inslistb( force );
      }
      break;
  }
  
}

void gui_set_rempos( struct ahx_tune *at )
{
  at->at_rempos[0] = 0;
  at->at_rempos[1] = at->at_TrackLength>>2;
  at->at_rempos[2] = at->at_TrackLength>>1;
  at->at_rempos[3] = (at->at_TrackLength*3)/4;
  at->at_rempos[4] = at->at_TrackLength-1;
}

void gui_update_from_nbox( int32 panel, struct numberbox *bp, int32 i )
{
  struct ahx_tune *at;
  struct ahx_instrument *ip;
  int32 j;
  
  if( bp[i].cnum < bp[i].min ) bp[i].cnum = bp[i].min;
  if( bp[i].cnum > bp[i].max ) bp[i].cnum = bp[i].max;

  at = curtune;
  if( at == NULL ) return;

  switch( at->at_curpanel )
  {
    case PN_TRACKER:
      switch( i )
      {
        case NB_POSNR:
          if( at->at_doing == D_PLAYING )
            at->at_NextPosNr = bp[i].cnum;
          else
            at->at_PosNr = bp[i].cnum;
          gui_render_tracker( FALSE );
          return;
        case NB_LENGTH:
          modify_tune_w( at, UNT_POSITIONNR, bp[i].cnum );
          break;
        case NB_RESPOS:
          modify_tune_w( at, UNT_RESTART, bp[i].cnum );
          break;
        case NB_TRKLEN:
          j = bp[NB_TRKLEN].cnum;
          if( j < 1 ) j = 1;
          modify_tune_b( at, UNT_TRACKLEN, j );
          if( at->at_NoteNr >= at->at_TrackLength )
            at->at_NoteNr = at->at_TrackLength-1;
          gui_set_rempos( curtune );
          trked_lastposnr = 8000;
          gui_render_tracker( FALSE );
          break;
        case NB_SSNR:
          modify_tune_w( at, UNT_SUBSONGS, bp[NB_SSNR].cnum );
          gui_set_various_things( at );
          break;
        case NB_CURSS:
          at->at_curss = bp[NB_CURSS].cnum;
          rp_init_subsong( at, bp[NB_CURSS].cnum );
          gui_set_various_things( at );
          gui_render_tracker( FALSE );
          break;
        case NB_SSPOS:
          if( at->at_curss > 0 )
            modify_sspos( at, bp[NB_SSPOS].cnum );
          break;
        case NB_CHANS:
          rp_stop();
          gui_render_tracked( TRUE );  // Kill the VUMeters
          gui_render_wavemeter();
          at->at_doing = D_IDLE;
          j = bp[NB_CHANS].cnum;
          if( j < 4 ) j = 4;
          if( j > MAX_CHANNELS ) j = MAX_CHANNELS;
          modify_tune_b( at, UNT_CHANNELS, j );
          at->at_curlch = 0;
          at->at_tracked_curs = 0;
          at->at_posed_curs = 0;
          gui_render_tracker( TRUE );
          break;

        case NB_MIXG:
          j = bp[NB_MIXG].cnum;
          if( j < 0 ) j = 0;
          if( j > 200 ) j = 200;
          modify_tune_b( at, UNT_MIXGAIN, j );
          at->at_mixgain = (j*256)/100;
          break;
        
        case NB_SPEEDMULT:
          j = bp[NB_SPEEDMULT].cnum;
          if( j < 1 ) j = 1;
          if( j > 4 ) j = 4;
          modify_tune_b( at, UNT_SPMUL, j );
          break;
        
        case NB_DRUMPADMODE:
          j = bp[NB_DRUMPADMODE].cnum;
          if( j < 0 ) j = 0;
          if( j > 1 ) j = 1;
          at->at_drumpadmode = j;
          break;
      }

      gui_render_nbox( at, PN_TRACKER, &bp[i] );
      break;

    case PN_INSED:
      ip = &at->at_Instruments[at->at_curins];
      switch( i )
      {
        case INB_INS:
          at->at_curins = bp[i].cnum;
          gui_render_nbox( at, PN_INSED, &bp[i] );
          gui_set_various_things( at );
          return;
        case INB_VOL:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_VOLUME, bp[i].cnum );
          break;
        case INB_WAVELEN:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_WAVELENGTH, bp[i].cnum );

          switch( ip->ins_WaveLength )
          {
            case 0:
              j = 0x20;
              break; 
            case 1:
              j = 0x10;
              break;
            case 2:
              j = 0x08;
              break;
            case 3:
              j = 0x04;
              break;
            case 4:
              j = 0x02;
              break;
            default:
              j = 0x01;
              break;
          }
    
          ins_nb[INB_SQRLOWER].min = j;
          ins_nb[INB_SQRUPPER].min = j;

          if( ip->ins_SquareLowerLimit < j )
            ip->ins_SquareLowerLimit = j;
          if( ip->ins_SquareUpperLimit < j )
            ip->ins_SquareUpperLimit = j;
          
          ins_nb[INB_SQRLOWER].cnum = ip->ins_SquareLowerLimit;
          ins_nb[INB_SQRUPPER].cnum = ip->ins_SquareUpperLimit;

          gui_render_nbox( at, PN_INSED, &bp[INB_SQRLOWER] );
          gui_render_nbox( at, PN_INSED, &bp[INB_SQRUPPER] );
          break;
        case INB_ATTACK:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_AFRAMES, bp[i].cnum );
          break;
        case INB_AVOL:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_AVOLUME, bp[i].cnum );
          break;
        case INB_DECAY:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_DFRAMES, bp[i].cnum );
          break;
        case INB_DVOL:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_DVOLUME, bp[i].cnum );
          break;
        case INB_SUSTAIN:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_SFRAMES, bp[i].cnum );
          break;
        case INB_RELEASE:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_RFRAMES, bp[i].cnum );
          break;
        case INB_RVOL:
          if( ip == NULL ) break;
          modify_env_w( at, &ip->ins_Envelope, UNT_ENV_RVOLUME, bp[i].cnum );
          break;
        case INB_VIBDELAY:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_VIBRATODELAY, bp[i].cnum );
          break;
        case INB_VIBDEPTH:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_VIBRATODEPTH, bp[i].cnum );
          break;
        case INB_VIBSPEED:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_VIBRATOSPEED, bp[i].cnum );
          break;
        case INB_SQRLOWER:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_SQUARELOWERLIMIT, bp[i].cnum );
          break;
        case INB_SQRUPPER:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_SQUAREUPPERLIMIT, bp[i].cnum );
          break;
        case INB_SQRSPEED:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_SQUARESPEED, bp[i].cnum );
          break;
        case INB_FLTLOWER:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_FILTERLOWERLIMIT, bp[i].cnum );
          break;
        case INB_FLTUPPER:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_FILTERUPPERLIMIT, bp[i].cnum );
          break;
        case INB_FLTSPEED:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_FILTERSPEED, bp[i].cnum );
          break;
        case INB_PERFSPEED:
          if( ip == NULL ) break;
          modify_pls_w( at, &ip->ins_PList, UNT_PLS_SPEED, bp[i].cnum );
          break;
        case INB_PERFLEN:
          if( ip == NULL ) break;
          modify_pls_w( at, &ip->ins_PList, UNT_PLS_LENGTH, bp[i].cnum );
          gui_render_perf( curtune, ip, FALSE );
          break;
        case INB_HARDCUT:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_HARDCUTRELEASEFRAMES, bp[i].cnum );          
          break;
        case INB_RELCUT:
          if( ip == NULL ) break;
          modify_ins_b( at, ip, UNT_INS_HARDCUTRELEASE, bp[i].cnum );          
          break;
      }

      gui_render_nbox( at, PN_INSED, &bp[i] );
      break;
  }
}

void gui_set_popup( int32 pn, int16 x, int16 y )
{
  pp[pn].x = x;
  pp[pn].y = y;
}

void gui_render_popup( int32 pn, BOOL pressed )
{
  if( pressed )
    put_ppartbitmap( BM_DIRPOPUP, 0, 18, pp[pn].x, pp[pn].y, 20, 18 );
  else
    put_pbitmap( BM_DIRPOPUP, pp[pn].x, pp[pn].y, 20, 18 );
}

void gui_set_pcycle( int32 cn, int16 x, int16 y, int32 cur, TEXT **opts )
{
  int32 i;

  for( i=0; opts[i] != NULL; i++ ) ;

  if( cur >= i ) cur = 0;

  pcyc[cn].x = x;
  pcyc[cn].y = y;
  pcyc[cn].numopts = i;
  pcyc[cn].copt = cur;
  pcyc[cn].options = opts;
  pcyc[cn].zone = gui_add_zone( &pzones[0], &numpzones, MAX_PZONES, x, y, 160, 24 );
}

void gui_render_pcycle( int32 cn, BOOL pressed )
{
  int w, tx;
  const TEXT *ctxt;

  if( pressed )
    put_ppartbitmap( BM_PRF_CYCLE, 0, 24, pcyc[cn].x, pcyc[cn].y, 158, 24 );
  else
    put_pbitmap( BM_PRF_CYCLE, pcyc[cn].x, pcyc[cn].y, 158, 24 );
  

  ctxt = pcyc[cn].options[pcyc[cn].copt];
  if( ctxt == NULL ) return;

  set_font(&prefbm, FONT_PRP, FALSE);

#ifndef __SDL_WRAPPER__
  w = IGraphics->TextLength( &prefbm.rp, ctxt, strlen( ctxt ) );
#else
  TTF_SizeText(prefbm.font, ctxt, &w, &tx);
#endif
  tx = pcyc[cn].x+pw_bl+(79-(w>>1));

  set_fpen(&prefbm, PAL_BTNSHADOW);
  printstr(&prefbm, ctxt, tx+1, pcyc[cn].y+pw_bt+5);
  set_fpen(&prefbm, PAL_BTNTEXT);
  printstr(&prefbm, ctxt, tx, pcyc[cn].y+pw_bt+4);

#if defined(WIN32) || defined(__APPLE__)
  if (cn == 0)
  {
    set_fpen(&prefbm, PAL_BACK);
    for (w=0; w<24; w+=3)
      fillrect_xy(&prefbm, pcyc[0].x, pcyc[0].y+w, pcyc[0].x+157, pcyc[0].y+w);
  }
#endif
}

void gui_render_prefs( void )
{
  int32 i;
  
  bm_to_bm( &bitmaps[BM_PRF_BG], 0, 0, &prefbm, pw_bl, pw_bt, 400, 300);
  
  for( i=0; i<PC_END; i++ )
    gui_render_pcycle( i, FALSE );

  for( i=0; i<PTB_END; i++ )
    gui_render_tbox( &prefbm, &ptb[i] );  
  
  for( i=0; i<PP_END; i++ )
    gui_render_popup( i, FALSE );

#ifdef __SDL_WRAPPER__
  set_fpen(&prefbm, PAL_BTNSHADOW);
  printstr(&prefbm, "ESC to close", 5, 281);
  set_fpen(&prefbm, PAL_BTNTEXT);
  printstr(&prefbm, "ESC to close", 4, 280);
  bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
}

void gui_open_prefs( void )
{
  pref_dorestart = FALSE;
  pref_oldfullscr = pref_fullscr;
  strcpy( pref_oldskindir, skindir );

#ifndef __SDL_WRAPPER__
  if( pw_x == -1 ) pw_x = mainwin->LeftEdge + 150;
  if( pw_y == -1 ) pw_y = mainwin->TopEdge + 100;

  prefwin = IIntuition->OpenWindowTags( NULL,
    WA_Left,        pw_x,
    WA_Top,         pw_y,
    WA_InnerWidth,  400,
    WA_InnerHeight, 300,
    WA_Title,       "Preferences",
    WA_ScreenTitle, "HivelyTracker (c)2022 IRIS & Up Rough! - http://www.uprough.net - http://www.hivelytracker.co.uk",
    WA_RMBTrap,     TRUE,
    WA_IDCMP,       IDCMP_CLOSEWINDOW|IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_GADGETUP|IDCMP_EXTENDEDMOUSE,
    WA_Activate,    TRUE,
    WA_DragBar,     TRUE,
    WA_CloseGadget, TRUE,
    WA_DepthGadget, TRUE,
    fullscr ? WA_CustomScreen : TAG_IGNORE, scr,
    TAG_DONE );
  if( !prefwin )
  {
    printf( "Unable to open prefs window!\n" );
    prefwin_sig = 0;
    return;
  }

  memset(&prefbm, 0, sizeof(struct rawbm));
  memcpy(&prefbm.rp, prefwin->RPort, sizeof(struct RastPort));
  prefbm.bm = prefwin->RPort->BitMap;
  
  pw_bl = prefwin->BorderLeft;
  pw_bt = prefwin->BorderTop;
  
#else
  pw_bl = 0;
  pw_bt = 0;
#endif

  set_font(&prefbm, FONT_PRP, FALSE);

  gui_set_pcycle( PC_WMODE,     220, 8     , pref_fullscr ? 1 : 0, pc_wmode_opts );
  gui_set_pcycle( PC_DEFSTEREO, 220, 8+24*1, pref_defstereo,  pc_defst_opts );
  gui_set_pcycle( PC_BLANKZERO, 220, 8+24*2, pref_blankzeros, pc_bzero_opts ); 

  gui_set_tbox( &ptb[PTB_SONGDIR], 220+pw_bl, 8+24*3+4+pw_bt, 136, songdir, 511, TBF_ENABLED|TBF_VISIBLE, 0 );
  gui_set_tbox( &ptb[PTB_INSTDIR], 220+pw_bl, 8+24*4+4+pw_bt, 136, instdir, 511, TBF_ENABLED|TBF_VISIBLE, 0 );
  gui_set_tbox( &ptb[PTB_SKINDIR], 220+pw_bl, 8+24*5+4+pw_bt, 136, skindir, 511, TBF_ENABLED|TBF_VISIBLE, 0 );

  gui_set_popup( PP_SONGDIR, 360, 8+24*3+3 );
  gui_set_popup( PP_INSTDIR, 360, 8+24*4+3 );
  gui_set_popup( PP_SKINDIR, 360, 8+24*5+3 );


  gui_set_pcycle( PC_MAXUNDOBUF, 220, 8+24*6, pref_maxundobuf, pc_mundo_opts );

  gui_render_prefs();

#ifndef __SDL_WRAPPER__
  prefwin_sig = (1L<<prefwin->UserPort->mp_SigBit);
  gui_sigs |= prefwin_sig;
#else
  prefwin_open = TRUE;
#endif
}

void gui_close_prefs( void )
{
  int32 i;

  for( i=0; i<MAX_PZONES; i++ )
    pzones[i].x = -1;
  numpzones = 0;

#ifndef __SDL_WRAPPER__
  if( prefwin )
  {
    pw_x = prefwin->LeftEdge;
    pw_y = prefwin->TopEdge;
    IIntuition->CloseWindow( prefwin );
  }
  
  prefwin = NULL;
  gui_sigs &= ~prefwin_sig;
  prefwin_sig = 0;
  memset(&prefbm.rp, 0, sizeof(struct RastPort));
  prefbm.bm = NULL;

  for( i=0; i<PTB_END; i++ )
  {
    if( ptb[i].bm.bm ) IGraphics->FreeBitMap( ptb[i].bm.bm );
    ptb[i].bm.bm = NULL;
  }
#else
  prefwin_open = FALSE;
  aboutwin_open = FALSE;

  for( i=0; i<PTB_END; i++ )
  {
    if( ptb[i].bm.srf ) SDL_FreeSurface( ptb[i].bm.srf );
    ptb[i].bm.srf = NULL;
  }

  gui_render_everything();
#endif

  if( ( pref_oldfullscr != pref_fullscr ) ||
      ( strcmp( pref_oldskindir, skindir ) != 0 ) )
    pref_dorestart = TRUE;
}

TEXT *gui_encode_pstr( TEXT *out, const TEXT *in )
{
  int32 ic, oc;
  
  ic=oc=0;
  while( in[ic] )
  {
    if(( in[ic] == 39 ) || ( in[ic] == '\\' )) out[oc++] = '\\';
    out[oc++] = in[ic++];
  }
  
  out[oc] = 0;
  return out;
}

BOOL gui_decode_pstr( const TEXT *tag, TEXT *buf )
{
  int32 ic, oc;

  ic=oc=0;

  if( strncmp( buf, tag, strlen( tag ) ) != 0 )
    return FALSE;
  
  ic = strlen( tag );

  // Skip whitespace
  while( isws( buf[ic] ) ) ic++;
  
  // equals?
  if( buf[ic] != '=' ) return FALSE;
  
  ic++;
  while( isws( buf[ic] ) ) ic++;

  // Starts with a quote?  
  if( buf[ic] != 39 ) return FALSE;
  
  ic++;
  while( buf[ic] )
  {
    // Escaped char?
    if( ( buf[ic] == '\\' ) && ( buf[ic+1] != 0 ) )
    {
      buf[oc++] = buf[ic+1];
      ic += 2;
      continue;
    }
    
    // Terminating quote?
    if( buf[ic] == 39 )
    {
      buf[oc] = 0;
      return TRUE;
    }
    
    // Carry on
    buf[oc++] = buf[ic++];
  }
  
  // Missing quote
  buf[oc] = 0;
  return FALSE;
}

BOOL gui_decode_num( const TEXT *tag, const TEXT *buf, int32 *val )
{
  uint32 ic;

  *val = 0;

  if( strncmp( buf, tag, strlen( tag ) ) != 0 )
    return FALSE;
  
  ic = strlen( tag );
  while( isws( buf[ic] ) ) ic++;
  
  if( buf[ic] != '=' ) return FALSE;
  ic++;
  
  while( isws( buf[ic] ) ) ic++;

  *val = atoi( &buf[ic] );
  return TRUE;
}

void gui_loadskinsettings( void )
{
  FILE *f;
  TEXT tmp[1024];
  int32 pen, col, i;
  
  tabtextback = FALSE;
  tabtextshad = TRUE;

#ifdef __APPLE__
  if (strncmp(skindir, "Skins/", 6) == 0)
    osxGetResourcesPath(tmp, skindir);
#else
  strncpy( tmp, skindir, 1024 );
#endif

#ifndef __SDL_WRAPPER__
  IDOS->AddPart( tmp, "Settings_os4", 1024 );
#else
  strncat( tmp, "/Settings_os4", 1024 );
#endif
  
  f = fopen( tmp, "r" );
  if( !f ) return;
  
  pen = 0;
  while( fgets(tmp, 1024, f) )
  {
    if( gui_decode_pstr( "skinext", tmp ) )
    {
      strncpy( skinext, tmp, 32 );
      continue;
    }

    if( gui_decode_pstr( "font1", tmp ) )
    {
      #ifdef __SDL_WRAPPER__
#ifdef PREFIX
	strcpy( prpfontname, PREFIX "/share/hivelytracker/ttf/");
#else
        strcpy( prpfontname, skindir ); // // // Set path to skin directory
        strcat( prpfontname, "/" );     // // //
#endif
        strcat( prpfontname, tmp );     // // // Append font filename
        
        // If font cannot be opened/found, load default
        if ( access(prpfontname, F_OK ) == -1 )
        {
            strcpy( prpfontname,
#ifdef PREFIX
	                         PREFIX "/share/hivelytracker/"
#endif
                                 "ttf/DejaVuSans.ttf" );
        }
      #endif
      continue;
    }
    
    if( gui_decode_pstr( "font2", tmp ) )
    {
      #ifdef __SDL_WRAPPER__
#ifdef PREFIX
	strcpy( fixfontname, PREFIX "/share/hivelytracker/ttf/");
#else
        strcpy( fixfontname, skindir ); // // //
        strcat( fixfontname, "/" );     // // //
#endif
        strcat( fixfontname, tmp );     // // //
        
        if ( access(fixfontname, F_OK ) == -1 )
        {
            strcpy( fixfontname,
#ifdef PREFIX
	                         PREFIX "/share/hivelytracker/"
#endif
                                 "ttf/DejaVuSansMono.ttf" );
        }
      #endif
      continue;
    }

    if( gui_decode_pstr( "font3", tmp ) )
    {
      #ifdef __SDL_WRAPPER__
#ifdef PREFIX
	strcpy( sfxfontname, PREFIX "/share/hivelytracker/ttf/");
#else
        strcpy( sfxfontname, skindir ); // // //
        strcat( sfxfontname, "/" );     // // //
#endif
        strcat( sfxfontname, tmp );     // // //
        
        if ( access(sfxfontname, F_OK ) == -1 )
        {
            strcpy( sfxfontname,
#ifdef PREFIX
	                         PREFIX "/share/hivelytracker/"
#endif
                                 "ttf/DejaVuSansMono.ttf" );
        }
      #endif
      continue;
    }

    if( gui_decode_pstr( "tabtextback", tmp ) )
    {
      if( strcasecmp( tmp, "Yes" ) == 0 )
        tabtextback = TRUE;
      else
        tabtextback = FALSE;
      continue;
    }

    if( gui_decode_pstr( "tabtextshad", tmp ) )
    {
      if( strcasecmp( tmp, "Yes" ) == 0 )
        tabtextshad = TRUE;
      else
        tabtextshad = FALSE;
      continue;
    }

    col = 0;
    if( ishex( tmp[0] ) )
    {
      // No more pens
      if( pen >= PAL_END ) continue;

      i=0;
      while( ishex( tmp[i] ) )
      {
        col <<= 4;
        if(( tmp[i] >= '0' ) && ( tmp[i] <= '9' )) col += tmp[i]-'0';
        if(( tmp[i] >= 'A' ) && ( tmp[i] <= 'F' )) col += tmp[i]-('A'-10);
        if(( tmp[i] >= 'a' ) && ( tmp[i] <= 'f' )) col += tmp[i]-('a'-10);
        i++;
      }

      pal[pen] = col;
#ifdef __SDL_WRAPPER__
#ifdef __APPLE__
      // Work-around for SDL bug on OSX
      mappal[pen] = SDL_MapRGB(ssrf->format, col&0xff, (col>>8)&0xff, (col>>16)&0xff) >> 8;
#else
      mappal[pen] = SDL_MapRGB(ssrf->format, (col>>16)&0xff, (col>>8)&0xff, col&0xff);
#endif
#endif
      // Just in case the skin doesn't specify
      // tab colours
      if( pen == PAL_BTNTEXT )   pal[PAL_TABTEXT]   = col;
      if( pen == PAL_BTNSHADOW ) pal[PAL_TABSHADOW] = col;

      pen++;
    }    
  }
  
  fclose(f);
}

void gui_load_prefs( void )
{
  FILE *f;
  int32 i;
  TEXT tmp[256];
  
#ifdef __APPLE__
  f = fopen( osxGetPrefsPath(), "r");
#else
  f = fopen( "ht.prefs", "r" );
#endif
  if( !f ) return;
  
  while( fgets(tmp, 256, f) )
  {
#ifdef __SDL_WRAPPER__
    if( gui_decode_num( "rctrlplaypos", tmp, &i ) )
    {
#ifdef __WIN32__ /* On Win32 RALT generates phantom RCTRL presses that stop this being workable */
      pref_rctrlplaypos = FALSE;
#else
      pref_rctrlplaypos = (i!=0);
#endif
      continue;
    }
#endif

    if( gui_decode_num( "display", tmp, &i ) )
    {
      pref_fullscr = (i!=0);
      continue;
    }
    
    if( gui_decode_num( "defstereo", tmp, &i ) )
    {
      if( i < 0 ) i = 0;
      if( i > 4 ) i = 4;
      pref_defstereo = i;
      continue;
    }
    
    if( gui_decode_num( "blankzero", tmp, &i ) )
    {
      pref_blankzeros = (i!=0);
      continue;
    }
    
    if( gui_decode_num( "maxundobf", tmp, &i ) )
    {
      if( i < 0 ) i = 0;
      if( i > 5 ) i = 5;
      pref_maxundobuf = i;
      continue;
    }

    if( gui_decode_pstr( "songdir", tmp ) )
    {
      strcpy( songdir, tmp );
      continue;
    }
    
    if( gui_decode_pstr( "instdir", tmp ) )
    {
      strcpy( instdir, tmp );
      continue;
    }
    
    if( gui_decode_pstr( "skindir", tmp ) )
    {
      strcpy( skindir, tmp );
      continue;
    }

    if( gui_decode_num( "posedadvance", tmp, &i ) )
    {
      posedadvance = (i!=0);
      continue;
    }

    if( gui_decode_num( "notejump", tmp, &i ) )
    {
      if( i < 0 ) i = 0;
      if( i > 9 ) i = 9;
      defnotejump = i;
      continue;
    }

    if( gui_decode_num( "inotejump", tmp, &i ) )
    {
      if( i < 0 ) i = 0;
      if( i > 9 ) i = 9;
      definotejump = i;
      continue;
    }
  }

  fclose(f);

#if defined(WIN32) || defined(APPLE)
  pref_fullscr = FALSE;
#endif
}

void gui_pre_init( void )
{
  uint32 i;

#ifndef __SDL_WRAPPER__
  for( i=0; i<BM_END; i++ ) bitmaps[i].bm = NULL;
  for( i=0; i<TB_END; i++ ) tbx[i].bm.bm = NULL;
#else
  for( i=0; i<BM_END; i++ ) bitmaps[i].srf = NULL;
  for( i=0; i<TB_END; i++ ) tbx[i].bm.srf = NULL;
#endif

  strcpy( songdir, "Songs" );
  strcpy( instdir, "Instruments" );
  strcpy( skindir,
#ifdef PREFIX
                   PREFIX
                   "/share/hivelytracker/"
#endif
                   "Skins/SIDMonster-Light" );

  gui_load_prefs();

  // Remove me later ...
  if ( songdir[0] == 0 ) strcpy(songdir, "Songs");
  if ( instdir[0] == 0 ) strcpy(instdir, "Instruments");

#ifdef __SDL_WRAPPER__
  strncpy( remsongdir, songdir, 512 );
  strncpy( reminstdir, instdir, 512 );
#endif

  memset(&prefbm, 0, sizeof(prefbm));
}

void gui_save_prefs( void )
{
  FILE *f;
  TEXT tmp[1024];
  
#ifdef __APPLE__
  f = fopen( osxGetPrefsPath(), "w" );
#else
  f = fopen( "ht.prefs", "w" );
#endif
  if( !f ) return;
  
  fprintf( f, "display = %d\n",   (int)pref_fullscr );
  fprintf( f, "defstereo = %d\n", (int)pref_defstereo );
  fprintf( f, "blankzero = %d\n", (int)pref_blankzeros );
  fprintf( f, "maxundobf = %d\n", (int)pref_maxundobuf );
  fprintf( f, "songdir = '%s'\n",  gui_encode_pstr( tmp, songdir ) );
  fprintf( f, "instdir = '%s'\n",  gui_encode_pstr( tmp, instdir ) );
  fprintf( f, "skindir = '%s'\n",  gui_encode_pstr( tmp, skindir ) );
  fprintf( f, "posedadvance = %d\n", (int)posedadvance );
  fprintf( f, "notejump = %d\n",  (int)defnotejump );
  fprintf( f, "inotejump = %d\n",  (int)definotejump );
#ifdef __SDL_WRAPPER__
  fprintf( f, "rctrlplaypos = %d\n", (int)pref_rctrlplaypos);
#endif
  fclose( f );
}

BOOL gui_open_skin_images( void )
{
  gui_loadskinsettings();

  if( !open_image( "logo",        &bitmaps[BM_LOGO] ) ) return FALSE;
  if( !open_image( "tab_area",    &bitmaps[BM_TAB_AREA] ) )    return FALSE;
  if( !open_image( "tab_left",    &bitmaps[BM_TAB_LEFT] ) )    return FALSE;
  if( !open_image( "tab_mid",     &bitmaps[BM_TAB_MID] ) )     return FALSE;
  if( !open_image( "tab_right",   &bitmaps[BM_TAB_RIGHT] ) )   return FALSE;
  if( !open_image( "itab_left",   &bitmaps[BM_ITAB_LEFT] ) )   return FALSE;
  if( !open_image( "itab_mid",    &bitmaps[BM_ITAB_MID] ) )    return FALSE;
  if( !open_image( "itab_right",  &bitmaps[BM_ITAB_RIGHT] ) )  return FALSE;

  if( tabtextback )
  {
    if( !open_image( "itab_text",   &bitmaps[BM_ITAB_TEXT] ) )   return FALSE;
    if( !open_image( "tab_text",    &bitmaps[BM_TAB_TEXT] ) )    return FALSE;
  }

  if( !open_image( "butbankr",    &bitmaps[BM_BUTBANKR] ) )    return FALSE;
  if( !open_image( "butbankp",    &bitmaps[BM_BUTBANKP] ) )    return FALSE;
  if( !open_image( "bg_tracker",  &bitmaps[BM_BG_TRACKER] ) )  return FALSE;
  if( !open_image( "bg_insed",    &bitmaps[BM_BG_INSED] ) )    return FALSE;
  if( !open_image( "plusminus",   &bitmaps[BM_PLUSMINUS] ) )   return FALSE;
  if( !open_image( "vumeter",     &bitmaps[BM_VUMETER] ) )     return FALSE;
  if( !open_image( "depth",       &bitmaps[BM_DEPTH] ) )       return FALSE;
  if( !open_image( "blank",       &bitmaps[BM_BLANK] ) )       return FALSE;
  if( !open_image( "wavemeter",   &bitmaps[BM_WAVEMETERS] ) )  return FALSE;
  if( !open_image( "prefs_os4",   &bitmaps[BM_PRF_BG] ) )      return FALSE;
  if( !open_image( "prefs_cycle", &bitmaps[BM_PRF_CYCLE] ) )   return FALSE;
  if( !open_image( "trkbankr",    &bitmaps[BM_TRKBANKR] ) )    return FALSE;
  if( !open_image( "trkbankp",    &bitmaps[BM_TRKBANKP] ) )    return FALSE;
  if( !open_image( "chanmute",    &bitmaps[BM_CHANMUTE] ) )    return FALSE;
  if( !open_image( "dir_popup",   &bitmaps[BM_DIRPOPUP] ) )    return FALSE;

  return TRUE;
}

BOOL gui_open( void )
{
  int32 i;

  fullscr = pref_fullscr;

#ifndef __SDL_WRAPPER__
  // Make our drag gadget
  gad_drag.NextGadget    = &gad_drag2;
  gad_drag.LeftEdge      = 24;
  gad_drag.TopEdge       = 0;
  gad_drag.Width         = 232;
  gad_drag.Height        = 24;
  gad_drag.Flags         = GFLG_GADGHNONE;
  gad_drag.Activation    = 0;
  gad_drag.GadgetType    = GTYP_WDRAGGING;
  gad_drag.GadgetRender  = NULL;
  gad_drag.SelectRender  = NULL;
  gad_drag.GadgetText    = NULL;
  gad_drag.MutualExclude = 0;
  gad_drag.SpecialInfo   = NULL;
  gad_drag.GadgetID      = 0;
  gad_drag.UserData      = NULL;

  gad_drag2.NextGadget    = &gad_depth;
  gad_drag2.LeftEdge      = 0;
  gad_drag2.TopEdge       = 24;
  gad_drag2.Width         = 256;
  gad_drag2.Height        = 72;
  gad_drag2.Flags         = GFLG_GADGHNONE;
  gad_drag2.Activation    = 0;
  gad_drag2.GadgetType    = GTYP_WDRAGGING;
  gad_drag2.GadgetRender  = NULL;
  gad_drag2.SelectRender  = NULL;
  gad_drag2.GadgetText    = NULL;
  gad_drag2.MutualExclude = 0;
  gad_drag2.SpecialInfo   = NULL;
  gad_drag2.GadgetID      = 0;
  gad_drag2.UserData      = NULL;

  // Make our depth gadget
  gad_depth.NextGadget    = NULL;
  gad_depth.LeftEdge      = 776;
  gad_depth.TopEdge       = 0;
  gad_depth.Width         = 24;
  gad_depth.Height        = 24;
  gad_depth.Flags         = 0;
  gad_depth.Activation    = 0;
  gad_depth.GadgetType    = GTYP_WDEPTH;
  gad_depth.GadgetRender  = NULL;
  gad_depth.SelectRender  = NULL;
  gad_depth.GadgetText    = NULL;
  gad_depth.MutualExclude = 0;
  gad_depth.SpecialInfo   = NULL;
  gad_depth.GadgetID      = 0;
  gad_depth.UserData      = NULL;

  if( fullscr )
  {
    uint32 modeid, depth;

    modeid = IP96->p96BestModeIDTags( P96BIDTAG_NominalWidth,   800,
                                      P96BIDTAG_NominalHeight,  600,
                                      P96BIDTAG_FormatsAllowed, RGBFF_R8G8B8|
                                                                RGBFF_B8G8R8|
                                                                RGBFF_A8R8G8B8|
                                                                RGBFF_A8B8G8R8|
                                                                RGBFF_R8G8B8A8|
                                                                RGBFF_B8G8R8A8|
                                                                RGBFF_R5G6B5|
                                                                RGBFF_R5G6B5PC|
                                                                RGBFF_R5G5B5|
                                                                RGBFF_R5G5B5PC|
                                                                RGBFF_B5G6R5PC|
                                                                RGBFF_B5G5R5PC,
                                      TAG_DONE );
    if( modeid == INVALID_ID )
    {
      printf( "Unable to get a suitable screenmode. Falling back to windowed mode...\n" );
      fullscr = FALSE;
    } else {
    
      depth = IP96->p96GetModeIDAttr( modeid, P96IDA_DEPTH );
    
      scr = IIntuition->OpenScreenTags( NULL,
        SA_Width,     800,
        SA_Height,    600,
        SA_Depth,     depth,
        SA_Title,     "HivelyTracker",
        SA_ShowTitle, FALSE,
        SA_DisplayID, modeid,
        SA_Pens,      -1,
        TAG_DONE );
      if( !scr )
      {
        printf( "Unable to open screen. Falling back to windowed mode...\n" );
        fullscr = FALSE;
      }
    }
  }

  mainwin = IIntuition->OpenWindowTags( NULL,
    WA_Width,       800,
    WA_Height,      600,
    WA_Borderless,  TRUE,
    WA_ScreenTitle, "HivelyTracker (c)2022 IRIS & Up Rough! - http://www.uprough.net - http://www.hivelytracker.co.uk",
    WA_RMBTrap,     TRUE,
    WA_IDCMP,       IDCMP_MOUSEBUTTONS|IDCMP_RAWKEY|IDCMP_GADGETUP|IDCMP_EXTENDEDMOUSE,
    WA_Activate,    TRUE,
    WA_Gadgets,     fullscr ? NULL : &gad_drag,
    fullscr ? WA_CustomScreen : TAG_IGNORE, scr,
    WA_Backdrop,    fullscr,
    TAG_DONE );
  if( !mainwin )
  {
    printf( "Unable to open main window!\n" );
    return FALSE;
  }

#endif // __SDL_WRAPPER__

  memset(&mainbm, 0, sizeof(mainbm));
  mainbm.w = 800;
  mainbm.h = 600;
#ifndef __SDL_WRAPPER__
  memcpy(&mainbm.rp, mainwin->RPort, sizeof(struct RastPort));
  mainbm.bm = mainwin->RPort->BitMap;
#else
  mainbm.srf = ssrf;
#endif

  prefbm.w = 400;
  prefbm.h = 300;

  if( !gui_open_skin_images() )
  {
    printf( "Error loading skin. Reverting to SIDMonster-Light...\n" );
    strcpy( skindir, "Skins/SIDMonster-Light" );
    if( !gui_open_skin_images() ) return FALSE;
  }

  if( !make_image( &bitmaps[BM_POSED],      POSED_W+8, POSED_H ) )  return FALSE;
  if( !make_image( &bitmaps[BM_TRACKED],    TRACKED_W+8, TRACKED_H ) ) return FALSE;
  if( !make_image( &bitmaps[BM_TRACKBAR],   768, 20 ) )  return FALSE;
  if( !make_image( &bitmaps[BM_PERF],       PERF_W, PERF_H ) ) return FALSE;
  if( !make_image( &bitmaps[BM_INSLIST],    INSLIST_W, INSLIST_H ) ) return FALSE;
  if( !make_image( &bitmaps[BM_INSLISTB],   INSLSTB_W, INSLSTB_H ) ) return FALSE;
#ifdef __SDL_WRAPPER__
  if( !make_image( &prefbm, 408, 308 ) ) return FALSE;
  prefbm.offx = 4;
  prefbm.offy = 4;
#endif

#ifndef __SDL_WRAPPER__
  fixfont = IDiskfont->OpenDiskFont( &fixfontattr );
  if( !fixfont )
  {
    printf( "Unable to open '%s'\n", fixfontattr.ta_Name );
    return FALSE;
  }

  sfxfont = IDiskfont->OpenDiskFont( &sfxfontattr );
  if( !sfxfont )
  {
    printf( "Unable to open '%s'\n", sfxfontattr.ta_Name );
    return FALSE;
  }

  prpfont = IDiskfont->OpenDiskFont( &prpfontattr );
  if( !prpfont )
  {
    printf( "Unable to open '%s'\n", prpfontattr.ta_Name );
    return FALSE;
  }
#else
  fixttf = TTF_OpenFont(fixfontname, 14);
  if( !fixttf )
  {
    printf( "Unable to open '%s'\n", fixfontname );
    return FALSE;
  }

  sfxttf = TTF_OpenFont(sfxfontname, 12);
  if( !sfxttf )
  {
    printf( "Unable to open '%s'\n", sfxfontname );
    return FALSE;
  }

  prpttf = TTF_OpenFont(prpfontname, 14);
  if( !prpttf )
  {
    printf( "Unable to open '%s'\n", prpfontname );
    return FALSE;
  }
#endif

  set_font(&bitmaps[BM_POSED], FONT_SFX, TRUE);
  set_pens(&bitmaps[BM_POSED], PAL_BACK, PAL_BACK);
  fillrect_xy(&bitmaps[BM_POSED], 0, 0, POSED_MX, POSED_MY);
  set_font(&bitmaps[BM_TRACKED], FONT_FIX, TRUE);
  set_pens(&bitmaps[BM_TRACKED], PAL_BACK, PAL_BACK);
  fillrect_xy(&bitmaps[BM_TRACKED], 0, 0, TRACKED_MX, TRACKED_MY);
  set_font(&bitmaps[BM_PERF], FONT_FIX, TRUE);
  set_pens(&bitmaps[BM_PERF], PAL_BACK, PAL_BACK);
  set_font(&bitmaps[BM_INSLIST], FONT_FIX, TRUE);
  set_pens(&bitmaps[BM_INSLIST], PAL_BACK, PAL_BACK);
  fillrect_xy(&bitmaps[BM_INSLIST], 0, 0, INSLIST_MX, TRACKED_MY);
  set_font(&bitmaps[BM_INSLISTB], FONT_FIX, TRUE);
  set_pens(&bitmaps[BM_INSLISTB], PAL_BACK, PAL_BACK);
  fillrect_xy(&bitmaps[BM_INSLISTB], 0, 0, INSLIST_MX, TRACKED_MY);

  for( i=0; i<8; i++ )
  {
    ttab[i].tune = NULL;
    ttab[i].zone = -1;
  }

  zn_close  = gui_add_zone( &zones[0], &numzones, MAX_ZONES, 0, 0, 24, 24 );

  if( fullscr )
    zn_scrdep = gui_add_zone( &zones[0], &numzones, MAX_ZONES, 776, 0, 24, 24 );

  for( i=0; i<6; i++ )
    zn_mute[i] = gui_add_zone( &zones[0], &numzones, MAX_ZONES, TRACKED_X+20+66+i*120, TRACKED_Y-22, 14, 19 );

#ifndef __SDL_WRAPPER__
  mainwin_sig = (1L<<mainwin->UserPort->mp_SigBit);
  gui_sigs    = mainwin_sig | gui_tick_sig;
#endif

  for( i=0; i<16; i++ )
    bbank[i].zone = gui_add_zone( &zones[0], &numzones, MAX_ZONES, (i&3)*80+256, (i>>2)*24, 80, 24 );
  for( i=0; i<12; i++ )
    tbank[i].zone = gui_add_zone( &zones[0], &numzones, MAX_ZONES, 744, 120+200+i*23, 52, 23 );
  
  posed_lasttune  = NULL;
  posed_lastposnr = -1;

  gui_set_nbox( &trk_nb[NB_POSNR],        75, 132,      58, 16, 0, 999, 0, "%03d", NBF_ENABLED|NBF_UDDURINGPLAY );
  gui_set_nbox( &trk_nb[NB_LENGTH],       75, 132+20*1, 58, 16, 1, 999, 0, "%03d", NBF_ENABLED|NBF_UDDURINGPLAY );
  gui_set_nbox( &trk_nb[NB_RESPOS],       75, 132+20*2, 58, 16, 0, 999, 0, "%03d", NBF_ENABLED|NBF_UDDURINGPLAY );
  gui_set_nbox( &trk_nb[NB_TRKLEN],       75, 132+20*3, 58, 16, 1,  64, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &trk_nb[NB_SSNR],         75, 132+20*4, 58, 16, 0, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &trk_nb[NB_CURSS],        75, 132+20*5, 58, 16, 0, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &trk_nb[NB_SSPOS],        75, 132+20*6, 58, 16, 0, 999, 0, "%03d", NBF_ENABLED );

  gui_set_nbox( &trk_nb[NB_CHANS],       219, 132,      58, 16, 4, MAX_CHANNELS, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &trk_nb[NB_MIXG],        219, 132+20*1, 58, 16, 0, 200, 0, "%03d", NBF_ENABLED|NBF_UDDURINGPLAY );
  gui_set_nbox( &trk_nb[NB_SPEEDMULT],   219, 132+20*2, 58, 16, 1,   4, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &trk_nb[NB_DRUMPADMODE], 219, 132+20*3, 58, 16, 0,   1, 0,      "", NBF_ENABLED|NBF_ONOFF|NBF_UDDURINGPLAY );

  gui_set_nbox( &ins_nb[INB_INS],     9,  128,      58, 16, 0,  63, 0, " %02d", NBF_ENABLED|NBF_UDDURINGPLAY );

  gui_set_nbox( &ins_nb[INB_VOL],     72, 142+20*1, 58, 16, 0,  64, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_WAVELEN], 72, 142+20*2, 58, 16, 0,   5, 0, " %02X", NBF_ENABLED|NBF_WAVELEN );

  gui_set_nbox( &ins_nb[INB_ATTACK],  72, 160+20*3, 58, 16, 1, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_AVOL],    72, 160+20*4, 58, 16, 0,  64, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_DECAY],   72, 160+20*5, 58, 16, 1, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_DVOL],    72, 160+20*6, 58, 16, 0,  64, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_SUSTAIN], 72, 160+20*7, 58, 16, 1, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_RELEASE], 72, 160+20*8, 58, 16, 1, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_RVOL],    72, 160+20*9, 58, 16, 0,  64, 0, " %02d", NBF_ENABLED );
  
  gui_set_nbox( &ins_nb[INB_VIBDELAY],72, 178+20*10, 58, 16, 0, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_VIBDEPTH],72, 178+20*11, 58, 16, 0,  15, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_VIBSPEED],72, 178+20*12, 58, 16, 0,  63, 0, " %02d", NBF_ENABLED );

  gui_set_nbox( &ins_nb[INB_SQRLOWER],72, 196+20*13, 58, 16, 1,  63, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_SQRUPPER],72, 196+20*14, 58, 16, 1,  63, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_SQRSPEED],72, 196+20*15, 58, 16, 0, 127, 0, "%03d", NBF_ENABLED );

  gui_set_nbox( &ins_nb[INB_FLTLOWER],72, 214+20*16, 58, 16, 1,  63, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_FLTUPPER],72, 214+20*17, 58, 16, 1,  63, 0, " %02d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_FLTSPEED],72, 214+20*18, 58, 16, 0, 127, 0, "%03d", NBF_ENABLED );

  gui_set_nbox( &ins_nb[INB_PERFSPEED], 208, 142+20*1, 58, 16, 0, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_PERFLEN],   208, 142+20*2, 58, 16, 0, 255, 0, "%03d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_HARDCUT],   208, 142+20*3, 58, 16, 0,   7, 0, "  %01d", NBF_ENABLED );
  gui_set_nbox( &ins_nb[INB_RELCUT],    208, 142+20*4, 58, 16, 0,   1, 0, "  %01d", NBF_ENABLED|NBF_ONOFF );
  
  gui_set_tbox( &tbx[TB_SONGNAME], 298, 152, POSED_W, NULL, 127, TBF_ENABLED|TBF_VISIBLE, PN_TRACKER );
  gui_set_tbox( &tbx[TB_INSNAME],  115, 128, 148, NULL, 127, TBF_ENABLED|TBF_VISIBLE, PN_INSED );
  gui_set_tbox( &tbx[TB_INSNAME2], INSLIST_X+17, INSLIST_Y, INSLIST_W-17, NULL, 127, TBF_ENABLED, PN_TRACKER );
  
  gui_setup_trkbbank( 0 );
  gui_setup_buttonbank( 0 );
  if( IExec->GetHead(rp_tunelist) )
    gui_set_various_things( (struct ahx_tune *)IExec->GetHead(rp_tunelist) );
  gui_render_everything();
  gui_render_tunepanel( TRUE );

  cz_lmb = -1;
  cz_rmb = -1;
  
  return TRUE;
}

BOOL gui_init( void )
{ 
  gui_savethem = FALSE;

#ifndef __SDL_WRAPPER__
  iev.ie_Class    = IECLASS_RAWKEY;
  iev.ie_SubClass = 0;
  strcpy( fixfontname, "Bitstream Vera Sans Mono.font" );
  strcpy( sfxfontname, "Bitstream Vera Sans Mono.font" );
  strcpy( prpfontname, "Bitstream Vera Sans.font" );
#else
#ifdef __APPLE__
  osxGetResourcesPath(fixfontname, "DejaVuSansMono.ttf");
  osxGetResourcesPath(sfxfontname, "DejaVuSansMono.ttf");
  osxGetResourcesPath(prpfontname, "DejaVuSans.ttf");
#elif defined(__HAIKU__)
  strcpy( fixfontname, "/boot/system/data/fonts/ttfonts/DejaVuSansMono.ttf" );
  strcpy( sfxfontname, "/boot/system/data/fonts/ttfonts/DejaVuSansMono.ttf" );
  strcpy( prpfontname, "/boot/system/data/fonts/ttfonts/DejaVuSans.ttf" );
#else
  strcpy( fixfontname,
#ifdef PREFIX
                       PREFIX
                       "/share/hivelytracker/"
#endif
                       "ttf/DejaVuSansMono.ttf" );
  strcpy( sfxfontname,
#ifdef PREFIX
                       PREFIX
                       "/share/hivelytracker/"
#endif
                       "ttf/DejaVuSansMono.ttf" );
  strcpy( prpfontname,
#ifdef PREFIX
                       PREFIX
                       "/share/hivelytracker/"
#endif
                       "ttf/DejaVuSans.ttf" );
#endif  
#endif  

  strcpy( skinext,     ".png" );

#ifndef __SDL_WRAPPER__
  if( !getLibIFace( &IntuitionBase, "intuition.library",    51, &IIntuition ) ) return FALSE;
  if( !getLibIFace( &GfxBase,       "graphics.library",     51, &IGraphics ) )  return FALSE;
  if( !getLibIFace( &P96Base,       "Picasso96API.library",  2, &IP96) )        return FALSE;
  if( !getLibIFace( &DataTypesBase, "datatypes.library",    51, &IDataTypes ) ) return FALSE;
  if( !getLibIFace( &DiskfontBase,  "diskfont.library",     51, &IDiskfont ) )  return FALSE;
  if( !getLibIFace( &AslBase,       "asl.library",          51, &IAsl) )        return FALSE;
  if( !getLibIFace( &KeymapBase,    "keymap.library",       51, &IKeymap) )     return FALSE;
  if( !getLibIFace( &RequesterBase, "requester.class",      51, &IRequester ) ) return FALSE;

  gui_tick_signum = IExec->AllocSignal( -1 );
  if( gui_tick_signum == -1 )
  {
    printf( "Unable to allocate signal\n" );
    return FALSE;
  }
  gui_tick_sig = (1L<<gui_tick_signum);

  ins_lreq = IAsl->AllocAslRequestTags( ASL_FileRequest,
               ASLFR_TitleText,     "Load Instrument",
               ASLFR_InitialDrawer, instdir,
               ASLFR_DoSaveMode,    FALSE,
               ASLFR_RejectIcons,   TRUE,
               TAG_DONE );
  if( !ins_lreq )
  {
    printf( "Error allocating Asl request\n" );
    return FALSE;
  }

  ins_sreq = IAsl->AllocAslRequestTags( ASL_FileRequest,
               ASLFR_TitleText,     "Save Instrument",
               ASLFR_InitialDrawer, instdir,
               ASLFR_DoSaveMode,    TRUE,
               ASLFR_RejectIcons,   TRUE,
               TAG_DONE );
  if( !ins_sreq )
  {
    printf( "Error allocating Asl request\n" );
    return FALSE;
  }

  sng_lreq = IAsl->AllocAslRequestTags( ASL_FileRequest,
               ASLFR_TitleText,     "Load Song",
               ASLFR_InitialDrawer, songdir,
               ASLFR_DoSaveMode,    FALSE,
               ASLFR_RejectIcons,   TRUE,
               TAG_DONE );
  if( !sng_lreq )
  {
    printf( "Error allocating Asl request\n" );
    return FALSE;
  }

  sng_sreq = IAsl->AllocAslRequestTags( ASL_FileRequest,
               ASLFR_TitleText,     songdir,
               ASLFR_InitialDrawer, "Songs",
               ASLFR_DoSaveMode,    TRUE,
               ASLFR_RejectIcons,   TRUE,
               TAG_DONE );
  if( !sng_sreq )
  {
    printf( "Error allocating Asl request\n" );
    return FALSE;
  }

  dir_req  = IAsl->AllocAslRequestTags( ASL_FileRequest,
               ASLFR_DoSaveMode,    FALSE,
               ASLFR_DrawersOnly,   TRUE,
               TAG_DONE );
  if( !ins_lreq )
  {
    printf( "Error allocating Asl request\n" );
    return FALSE;
  }
#endif

  if( !gui_open() ) return FALSE;

  gui_savethem = TRUE;

  return TRUE;
}

void gui_press_bbank( struct buttonbank *bbnk, int32 nb, int32 z, int32 button )
{
  int32 i;

  for( i=0; i<nb; i++ )
    if( z == bbnk[i].zone )
      break;
  
  if( i == nb )
    return;

  switch( button )
  {
    case 0:
      if( bbnk[i].action == BBA_NOTHING )
        return;
      break;
    case 1:
      if( bbnk[i].raction == BBA_NOTHING )
        return;
      break;
  }
  
  // Gahhh. I suck. But i can't be bothered to fix
  // this now.
  if( bbnk == &bbank[0] )
    put_partbitmap( BM_BUTBANKP, (i&3)*80, (i>>2)*24, (i&3)*80+256, (i>>2)*24, 80, 24 );
  if( bbnk == &tbank[0] )
    put_partbitmap( BM_TRKBANKP, 0, i*23, 744, 120+200+i*23, 52, 23 );

  if( bbnk[i].name == NULL ) return;

  set_font(&mainbm, FONT_PRP, FALSE);
  set_fpen(&mainbm, PAL_BTNSHADOW);
  printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo+1, bbnk[i].y+5);
  set_fpen(&mainbm, PAL_BTNTEXT);
  printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo, bbnk[i].y+4);
}

void gui_release_bbank( struct buttonbank *bbnk, int32 nb, int32 z, int32 button )
{
  int32 i;

  for( i=0; i<nb; i++ )
    if( z == bbnk[i].zone )
      break;
  
  if( i == nb )
    return;

  switch( button )
  {
    case 0:  
      if( bbnk[i].action == BBA_NOTHING )
        return;
      break;
    case 1:
      if( bbnk[i].raction == BBA_NOTHING )
        return;
      break;
  }

  if( bbnk == &bbank[0] )
    put_partbitmap( BM_BUTBANKR, (i&3)*80, (i>>2)*24, (i&3)*80+256, (i>>2)*24, 80, 24 );
  if( bbnk == &tbank[0] )
    put_partbitmap( BM_TRKBANKR, 0, i*23, 744, 120+200+i*23, 52, 23 );

  if( bbnk[i].name == NULL ) return;

  set_font(&mainbm, FONT_PRP, FALSE);
  set_fpen(&mainbm, PAL_BTNSHADOW);
  printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo+1, bbnk[i].y+5);
  set_fpen(&mainbm, PAL_BTNTEXT);
  printstr(&mainbm, bbnk[i].name, bbnk[i].x+bbnk[i].xo, bbnk[i].y+4);
}

void gui_copyposregion( struct ahx_tune *at, BOOL cutting )
{
  int32 i, j, size;
  uint8 *ptr;
  
  // Calculate the size of the region to copy
  size = ((at->at_cbpmarkrgt-at->at_cbpmarklft)+1) *    /* Channels */
         ((at->at_cbpmarkbot-at->at_cbpmarktop)+1) *    /* Rows */
         2;

  if( cbpbuf == NULL )
  {
    cbpbuf  = IExec->AllocVecTags( size, TAG_DONE );
    if( cbpbuf == NULL ) return;
    cbpblen = size;
  }
  
  if( cbpblen < size )
  {
    IExec->FreeVec( cbpbuf );
    cbpblen = 0;
    cbpbuf  = IExec->AllocVecTags( size, TAG_DONE );
    if( cbpbuf == NULL ) return;
    cbpblen = size;
  }

  cbplcol = at->at_cbpmarklftcol;
  cbprcol = at->at_cbpmarkrgtcol;
  cbpchns = (at->at_cbpmarkrgt-at->at_cbpmarklft)+1;
  cbprows = (at->at_cbpmarkbot-at->at_cbpmarktop)+1;

  if( cutting ) setbefore_posregion( at, at->at_cbpmarklft, at->at_cbpmarktop, cbpchns, cbprows );

  ptr = cbpbuf;
  for( i=at->at_cbpmarktop; i<=at->at_cbpmarkbot; i++ )
  {
    for( j=at->at_cbpmarklft; j<=at->at_cbpmarkrgt; j++ )
    {
      *(ptr++) = at->at_Positions[i].pos_Track[j];
      *(ptr++) = at->at_Positions[i].pos_Transpose[j];
      if( cutting )
      {
        if(( j>at->at_cbpmarklft ) || ( cbplcol == 0 )) at->at_Positions[i].pos_Track[j]     = 0;
        if(( j<at->at_cbpmarkrgt ) || ( cbprcol != 0 )) at->at_Positions[i].pos_Transpose[j] = 0;
      }
    }
  }  

  if( cutting ) setafter_posregion( at, at->at_cbpmarklft, at->at_cbpmarktop, cbpchns, cbprows );
}

void gui_pasteposregion( struct ahx_tune *at )
{
  int32 i, j, pos, lch;
  uint8 *ptr;

  if( cbpbuf == NULL ) return;
  if( ( cbpchns == 0 ) || ( cbprows == 0 ) ) return;
  
  pos = at->at_PosNr;
  lch = at->at_posed_curs/5 + at->at_curlch;

  setbefore_posregion( at, lch, pos, cbpchns, cbprows );
  
  ptr = cbpbuf;
  for( i=0; i<cbprows; i++ )
  {
    if( (i+pos) > 999 ) break;
    for( j=0; j<cbpchns; j++ )
    {
      if( (lch+j) >= at->at_Channels ) break;
      if( ( j > 0 ) || ( cbplcol == 0 ) )
        at->at_Positions[pos+i].pos_Track[lch+j] = *(ptr++);
      else
        ptr++;
      if( ( j < (cbpchns-1) ) || ( cbprcol == 1 ) )
        at->at_Positions[pos+i].pos_Transpose[lch+j] = *(ptr++);
      else
        ptr++;
    }
  }
  
  setafter_posregion( at, lch, pos, cbpchns, cbprows );

  at->at_modified = TRUE;
}

void gui_copyregion( int16 track, int16 start, int16 end, int16 toclip, BOOL cutting )
{
  int32 i;

  for( i=start; i<=end; i++ )
  {
    if( toclip & CBF_NOTES )
    {
      cb_content[i-start].stp_Note       = curtune->at_Tracks[track][i].stp_Note;
      cb_content[i-start].stp_Instrument = curtune->at_Tracks[track][i].stp_Instrument;
      if( cutting )
      {
        curtune->at_Tracks[track][i].stp_Note       = 0;
        curtune->at_Tracks[track][i].stp_Instrument = 0;
      }
    }
        
    if( toclip & CBF_CMD1 )
    {
      cb_content[i-start].stp_FX         = curtune->at_Tracks[track][i].stp_FX;
      cb_content[i-start].stp_FXParam    = curtune->at_Tracks[track][i].stp_FXParam;
      if( cutting )
      {
        curtune->at_Tracks[track][i].stp_FX         = 0;
        curtune->at_Tracks[track][i].stp_FXParam    = 0;
      }
    }

    if( toclip & CBF_CMD2 )
    {
      cb_content[i-start].stp_FXb        = curtune->at_Tracks[track][i].stp_FXb;
      cb_content[i-start].stp_FXbParam   = curtune->at_Tracks[track][i].stp_FXbParam;
      if( cutting )
      {
        curtune->at_Tracks[track][i].stp_FXb        = 0;
        curtune->at_Tracks[track][i].stp_FXbParam   = 0;
      }
    }
  }
      
  cb_contains = toclip;
  cb_length   = (end-start)+1;
}

void gui_paste( struct ahx_tune *at, int16 track, int16 pastemask )
{
  int32 i;
  
  if( at == NULL ) return;
  if( track == -1 ) return;
  if( cb_length == 0 ) return;
  
  pastemask &= cb_contains;  // Only the bits in both
  if( pastemask == 0 ) return;

  for( i=0; i<cb_length; i++ )
  {
    if( (i+at->at_NoteNr) >= at->at_TrackLength )
      break;
        
    if( pastemask & CBF_NOTES )
    {
      at->at_Tracks[track][i+at->at_NoteNr].stp_Note       = cb_content[i].stp_Note;
      at->at_Tracks[track][i+at->at_NoteNr].stp_Instrument = cb_content[i].stp_Instrument;
    }
        
    if( pastemask & CBF_CMD1 )
    {
      at->at_Tracks[track][i+at->at_NoteNr].stp_FX         = cb_content[i].stp_FX;
      at->at_Tracks[track][i+at->at_NoteNr].stp_FXParam    = cb_content[i].stp_FXParam;
    }

    if( pastemask & CBF_CMD2 )
    {
      at->at_Tracks[track][i+at->at_NoteNr].stp_FXb        = cb_content[i].stp_FXb;
      at->at_Tracks[track][i+at->at_NoteNr].stp_FXbParam   = cb_content[i].stp_FXbParam;
    }
  }
      
  at->at_NoteNr = (at->at_NoteNr+i)%at->at_TrackLength;
  at->at_modified = TRUE;
}

BOOL optimise( struct ahx_tune *from, struct ahx_tune *to, BOOL optimise_more )
{
  int32 op_pos, op_chan, op_track, op_trans;
  int32 thistrans, i, j, k;
  int32 imap[64], icare[64], ins, topins;
  BOOL usedzero, carenote;
  
  for( i=0; i<63; i++ )
    imap[i] = 0;
  
  IExec->CopyMem( from->at_Name, to->at_Name, 128 );
  strncat( to->at_Name, "[opt]", 128 );
  to->at_Name[127]       = 0;
  to->at_Restart         = from->at_Restart;
  to->at_PositionNr      = from->at_PositionNr;
  to->at_SpeedMultiplier = from->at_SpeedMultiplier;
  to->at_TrackLength     = from->at_TrackLength;
  to->at_SubsongNr       = from->at_SubsongNr;
  to->at_Stereo          = from->at_Stereo;
  IExec->CopyMem( from->at_Subsongs, to->at_Subsongs, 256*2 );
  to->at_Channels        = from->at_Channels;
  to->at_mixgain         = from->at_mixgain;
  to->at_mixgainP        = from->at_mixgainP;

  topins = 1;
  usedzero = FALSE;
  carenote = TRUE;

  // Loop through all the positions
  for( op_pos=0; op_pos<from->at_PositionNr; op_pos++ )
  {
    // Loop through all the channels
    for( op_chan=0; op_chan<from->at_Channels; op_chan++ )
    {
      op_track = from->at_Positions[op_pos].pos_Track[op_chan];
      op_trans = from->at_Positions[op_pos].pos_Transpose[op_chan];

      // Find a track already present that matches
      // this one
      for( i=0; i<to->at_TrackNr; i++ )
      {
        thistrans = optimise_more ? 10000 : 0;
        for( j=0; j<from->at_TrackLength; j++ )
        {
          // Map the instrument
          ins = from->at_Tracks[op_track][j].stp_Instrument;
          if( ins > 0 )
          {
            if( imap[ins] == 0 )
            {
              to->at_Instruments[topins] = from->at_Instruments[ins];
              imap[ins] = topins++;
              
              icare[ins] = FALSE;
              for( k=0; k<from->at_Instruments[ins].ins_PList.pls_Length; k++ )
              {
                if( ( from->at_Instruments[ins].ins_PList.pls_Entries[k].ple_Note != 0 ) &&
                    ( from->at_Instruments[ins].ins_PList.pls_Entries[k].ple_Fixed == 0 ) )
                {
                  icare[ins] = TRUE;
                  break;
                }
              }
            }
            carenote = icare[ins];
            ins = imap[ins];
          }
          
          // Compare it
          if( to->at_Tracks[i][j].stp_Instrument != ins ) break;
          if( to->at_Tracks[i][j].stp_FX         != from->at_Tracks[op_track][j].stp_FX      )  break;
          if( to->at_Tracks[i][j].stp_FXParam    != from->at_Tracks[op_track][j].stp_FXParam )  break;
          if( to->at_Tracks[i][j].stp_FXb        != from->at_Tracks[op_track][j].stp_FXb      ) break;
          if( to->at_Tracks[i][j].stp_FXbParam   != from->at_Tracks[op_track][j].stp_FXbParam ) break;
          
          if( from->at_Tracks[op_track][j].stp_Note != 0 )
          {
            if( to->at_Tracks[i][j].stp_Note == 0 ) break;
            
            if( ( ins == 0 ) || ( carenote ) )
            {
              if( thistrans != 10000 )
              {
                if( (to->at_Tracks[i][j].stp_Note-from->at_Tracks[op_track][j].stp_Note) != thistrans )
                  break;
              } else {
                thistrans = to->at_Tracks[i][j].stp_Note - from->at_Tracks[op_track][j].stp_Note;
                if( ( (thistrans+op_trans) < -128 ) ||
                    ( (thistrans+op_trans) > 127 ) )
                  break;
              }
            }
          } else {
            if( to->at_Tracks[i][j].stp_Note != 0 ) break;
          } 
        }
        
        // Found a match?
        if( j == from->at_TrackLength )
          break;
      }
      
      if( i == to->at_TrackNr )
      {
        for( j=0; j<from->at_TrackLength; j++ )
        {
          // Map the instrument
          ins = from->at_Tracks[op_track][j].stp_Instrument;
          if( ins > 0 )
          {
            if( imap[ins] == 0 )
            {
              to->at_Instruments[topins] = from->at_Instruments[ins];
              imap[ins] = topins++;

              icare[ins] = FALSE;
              for( k=0; k<from->at_Instruments[ins].ins_PList.pls_Length; k++ )
              {
                if( ( from->at_Instruments[ins].ins_PList.pls_Entries[k].ple_Note != 0 ) &&
                    ( from->at_Instruments[ins].ins_PList.pls_Entries[k].ple_Fixed == 0 ) )
                {
                  icare[ins] = TRUE;
                  break;
                }
              }

            }
            ins = imap[ins];
          }
          
          to->at_Tracks[i][j].stp_Instrument = ins;
          to->at_Tracks[i][j].stp_Note       = from->at_Tracks[op_track][j].stp_Note;
          to->at_Tracks[i][j].stp_FX         = from->at_Tracks[op_track][j].stp_FX;
          to->at_Tracks[i][j].stp_FXParam    = from->at_Tracks[op_track][j].stp_FXParam;
          to->at_Tracks[i][j].stp_FXb        = from->at_Tracks[op_track][j].stp_FXb;
          to->at_Tracks[i][j].stp_FXbParam   = from->at_Tracks[op_track][j].stp_FXbParam;
        }
        to->at_TrackNr++;
        thistrans = 0;
      }
      
      if( thistrans == 10000 ) thistrans = 0;
      
      to->at_Positions[op_pos].pos_Track[op_chan]     = i;
      to->at_Positions[op_pos].pos_Transpose[op_chan] = op_trans-thistrans;
      
      if( i == 0 ) usedzero = TRUE;
    } 
  }
  
  return usedzero;
}

BOOL gui_check_bbank( struct buttonbank *bbnk, int32 nb, int32 z, int32 button )
{
  int32 i, j, b, l;
  int32 chan, track;
#ifndef __SDL_WRAPPER__
  BPTR lock, olddir;
#else
  char *gfname;
#endif
  BOOL ok, optimise_more;
  struct ahx_tune *at;
  struct ahx_step tstp;
  TEXT tmp[256];
  BOOL cutting;
  int32 toclip;
  
  for( i=0; i<nb; i++ )
    if( z == bbnk[i].zone )
      break;
  
  if( i == nb )
    return FALSE;
  
  if( button == 0 )
    b = bbnk[i].action;
  else
    b = bbnk[i].raction;
  
  chan = track = -1;
  if( curtune )
  {
    chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
    track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
  }
  
  cutting = FALSE;
  toclip = 0;
  optimise_more = FALSE;
  
  switch( b )
  {
    case BBA_ABOUT:
      about_toggle();
      break;
    
    case BBA_ZAP:
#ifndef __SDL_WRAPPER__
      switch( gui_req( REQIMAGE_QUESTION, "Zap!", "What would you like to Zap?", "Everything|Song|Tracks|Positions|Instruments|Oops! Nothing!" ) )
      {
        case 1: // Everything
          rp_clear_tune( curtune );
          gui_render_tunepanel( TRUE );
          break;
        
        case 2: // Song
          rp_zap_tracks( curtune );
          rp_zap_positions( curtune );
          free_undolists( curtune );
          gui_render_tunepanel( TRUE );
          break;
        
        case 3: // Tracks
          rp_zap_tracks( curtune );
          free_undolists( curtune );
          gui_render_tunepanel( TRUE );
          break;
        
        case 4: // Positions
          rp_zap_positions( curtune );
          free_undolists( curtune );
          gui_render_tunepanel( TRUE );
          break;
        
        case 5:  // Instruments
          rp_zap_instruments( curtune );
          free_undolists( curtune );
          gui_render_tunepanel( TRUE );
          break; 
      }
#else
      gui_setup_buttonbank( 1 );
      gui_render_main_buttonbank();
#endif
      break;

    case BBA_ZAP_SONG:
      rp_zap_tracks( curtune );
      rp_zap_positions( curtune );
      free_undolists( curtune );
      gui_render_tunepanel( TRUE );
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break;

    case BBA_ZAP_TRACKS:
      rp_zap_tracks( curtune );
      free_undolists( curtune );
      gui_render_tunepanel( TRUE );
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break;

    case BBA_ZAP_POSNS:
      rp_zap_positions( curtune );
      free_undolists( curtune );
      gui_render_tunepanel( TRUE );
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break;
        
    case BBA_ZAP_INSTRS:
      rp_zap_instruments( curtune );
      free_undolists( curtune );
      gui_render_tunepanel( TRUE );
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break; 

    case BBA_ZAP_ALL:
      rp_clear_tune( curtune );
      gui_render_tunepanel( TRUE );
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break;

    case BBA_BACK:
      gui_setup_buttonbank( 0 );
      gui_render_main_buttonbank();
      break;

    case BBA_UNDO:
      rp_stop();
      curtune->at_doing = D_IDLE;
      undo( curtune );
      gui_render_wavemeter();
      break;

    case BBA_REDO:
      rp_stop();
      curtune->at_doing = D_IDLE;
      redo( curtune );
      gui_render_wavemeter();
      break;

    case BBA_AUTOGAIN:
      rp_stop();
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;
      
      for( j=0; j<curtune->at_Channels; j++ )
      {
        curtune->at_Voices[j].vc_SetTrackOn = 1;
        curtune->at_Voices[j].vc_TrackOn = 1;
      }
      
      bbank[i].name = "Wait...";
      gui_render_main_buttonbank();
      
      l = rp_find_loudest( curtune );
      if( l > 0 )
        l = 3276700/l;
      else
        l = 100;
      
      if( l > 200 ) l = 200;
      modify_tune_b( curtune, UNT_MIXGAIN, l );
      curtune->at_mixgain  = (l*256)/100;
      
      rp_init_subsong( curtune, 0 );
      
      gui_set_various_things( curtune );
      gui_render_posed( FALSE );
      gui_render_tracked( TRUE );

      bbank[i].name = "Autogain";
      gui_render_main_buttonbank();

      break;
      
    case BBA_INSEDIT:
      curtune->at_idoing = D_IDLE;
      if( curtune->at_curpanel == PN_TRACKER )
        curtune->at_curpanel = PN_INSED;
      else
        curtune->at_curpanel = PN_TRACKER;
      
      gui_render_tunepanel( TRUE );
      break;
      
    case BBA_NEWTAB:
      at = rp_new_tune( TRUE );
      if( at )
      {
        curtune = at;
        gui_set_various_things( at );
        gui_render_tabs();
        gui_render_tunepanel( TRUE );
      }
      break;
    
    case BBA_OPTIMISE_MORE:
      optimise_more = TRUE;
    case BBA_OPTIMISE:
      at = rp_new_tune( TRUE );
      if( at )
      {
        at->at_TrackNr = 1; // Try and use track 0 as the empty one
        if( !optimise( curtune, at, optimise_more ) )
        {
          rp_clear_tune( at );
          at->at_TrackNr = 0;
          optimise( curtune, at, optimise_more );
        }
        
        at->at_doing    = D_IDLE;
        at->at_editing  = E_TRACK;
        at->at_idoing   = D_IDLE;
        curtune = at;
        gui_set_various_things( at );
        gui_render_tabs();
        gui_render_tunepanel( TRUE );        
      }
      break;
    
    case BBA_CLONETAB:
      at = rp_new_tune( TRUE );
      if( at )
      {
        IExec->CopyMem( &curtune->at_Name[0], &at->at_Name[0], sizeof( struct ahx_tune ) - offsetof( struct ahx_tune, at_Name ) );
        at->at_doing    = D_IDLE;
        at->at_editing  = E_TRACK;
        at->at_idoing   = D_IDLE;
        curtune = at;
        gui_set_various_things( at );
        gui_render_tabs();
        gui_render_tunepanel( TRUE );
      }
      break;

    case BBA_LOADINS:
      if( curtune == NULL ) break;
      
      if( curtune->at_curins == 0 )
        curtune->at_curins = 1;

      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;

#ifndef __SDL_WRAPPER__
      ok = IAsl->AslRequestTags( ins_lreq,
        ASLFR_Window,      mainwin,
        ASLFR_SleepWindow, TRUE,
        TAG_DONE );
      if( !ok ) break;
      
      lock = IDOS->Lock( ins_lreq->fr_Drawer, ACCESS_READ );
      olddir = IDOS->SetCurrentDir( lock );
      
      rp_load_ins( ins_lreq->fr_File, curtune, curtune->at_curins );
    
      IDOS->SetCurrentDir( olddir );
      IDOS->UnLock( lock );
#else
      if (!(gfname = filerequester("Load instrument", reminstdir, "", FR_INSLOAD))) break;
      setpathpart(reminstdir, gfname);
      rp_load_ins( gfname, curtune, curtune->at_curins );
      free(gfname);
#endif
      gui_set_various_things( curtune );
      if( curtune->at_curpanel == PN_TRACKER )
      {
        gui_render_inslist( TRUE );
      } else {
        gui_render_perf( curtune, &curtune->at_Instruments[curtune->at_curins], TRUE );
        gui_render_tbox( &mainbm, &tbx[TB_INSNAME] );
        gui_render_inslistb( TRUE );
      }
      break;

    case BBA_SAVEAHX:
      if( curtune == NULL ) break;
      
      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;
    
      i = rp_ahx_test( curtune );
      if( i != 0 )
      {
        TEXT wtxt[4096];
    
        sprintf( wtxt, "'%s' uses the following HivelyTracker specific features:\n\n", curtune->at_Name );
    
        if( i & SWF_MANYCHANS ) strcat( wtxt, "* More than 4 channels\n" );
        if( i & SWF_DOUBLECMD ) strcat( wtxt, "* Notes with 2 commands\n" );
        if( i & SWF_NEWINSCMD ) strcat( wtxt, "* Instruments with commands other than 1,2,3,4,5,C & F\n" );
        if( i & SWF_PANCMD )    strcat( wtxt, "* 7xx panning command\n" );
        if( i & SWF_EFXCMD )    strcat( wtxt, "* EFx command\n" );
    
        strcat( wtxt, "\nSaving as AHX would strip these out. Are you sure?" );

        if( gui_req( REQIMAGE_WARNING, "HivelyTracker", wtxt, "_OK|_Noooo!" ) == 0 )
          break;
      }
      
      strcpy( tmp, "AHX." );
      strcat( tmp, curtune->at_Name );

#ifndef __SDL_WRAPPER__
      ok = IAsl->AslRequestTags( sng_sreq,
        ASLFR_Window,      mainwin,
        ASLFR_InitialFile, tmp,
        ASLFR_SleepWindow, TRUE,
        TAG_DONE );
      if( !ok ) break;
      
      lock = IDOS->Lock( sng_sreq->fr_Drawer, ACCESS_READ );
      olddir = IDOS->SetCurrentDir( lock );
      
      rp_save_ahx( sng_sreq->fr_File, curtune );
      
      if( i == 0 )
        curtune->at_modified = FALSE;
      
      IDOS->SetCurrentDir( olddir );
      IDOS->UnLock( lock );
#else
      if (!(gfname = filerequester("Save AHX module", remsongdir, "", FR_AHXSAVE))) break;
      setpathpart(remsongdir, gfname);
      rp_save_ahx( gfname, curtune );
      free(gfname);
      if( i == 0 )
        curtune->at_modified = FALSE;
#endif
      break;

    case BBA_SAVEHVL:
      if( curtune == NULL ) break;
      
      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;
    
      strcpy( tmp, "HVL." );
      strcat( tmp, curtune->at_Name );

#ifndef __SDL_WRAPPER__
      ok = IAsl->AslRequestTags( sng_sreq,
        ASLFR_Window,      mainwin,
        ASLFR_InitialFile, tmp,
        ASLFR_SleepWindow, TRUE,
        TAG_DONE );
      if( !ok ) break;
      
      lock = IDOS->Lock( sng_sreq->fr_Drawer, ACCESS_READ );
      olddir = IDOS->SetCurrentDir( lock );
      
      rp_save_hvl( sng_sreq->fr_File, curtune );
      
      curtune->at_modified = FALSE;
      
      IDOS->SetCurrentDir( olddir );
      IDOS->UnLock( lock );
#else
      if (!(gfname = filerequester("Save HVL module", remsongdir, "", FR_HVLSAVE))) break;
      setpathpart(remsongdir, gfname);
      rp_save_hvl( gfname, curtune );
      free(gfname);
      curtune->at_modified = FALSE;
#endif
      break;

    case BBA_SAVEINS:
      if( curtune == NULL ) break;

      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;
    
      strcpy( tmp, "INS." );
      strcat( tmp, curtune->at_Instruments[curtune->at_curins].ins_Name );

#ifndef __SDL_WRAPPER__
      ok = IAsl->AslRequestTags( ins_sreq,
        ASLFR_Window,      mainwin,
        ASLFR_InitialFile, tmp,
        ASLFR_SleepWindow, TRUE,
        TAG_DONE );
      if( !ok ) break;
      
      lock = IDOS->Lock( ins_sreq->fr_Drawer, ACCESS_READ );
      olddir = IDOS->SetCurrentDir( lock );
      
      rp_save_ins( ins_sreq->fr_File, curtune, curtune->at_curins );
      gui_set_various_things( curtune );
      
      IDOS->SetCurrentDir( olddir );
      IDOS->UnLock( lock );
#else
      if (!(gfname = filerequester("Save instrument", reminstdir, "", FR_INSSAVE))) break;
      setpathpart(reminstdir, gfname);
      rp_save_ins( gfname, curtune, curtune->at_curins );
      free(gfname);
      gui_set_various_things( curtune );
#endif
      break;

    case BBA_LOADTUNE:
      if( ( curtune != NULL ) && ( curtune->at_modified ) )
        if( gui_req( REQIMAGE_QUESTION, "HivelyTracker", "This song has been modified. Continue?", "_Yep!|Arrgh.. _No!" ) == 0 )
          break;

#ifndef __SDL_WRAPPER__
      ok = IAsl->AslRequestTags( sng_lreq,
        ASLFR_Window,      mainwin,
        ASLFR_SleepWindow, TRUE,
        TAG_DONE );
      if( !ok ) break;
#else
      if (!(gfname = filerequester("Load song", remsongdir, "", FR_MODLOAD))) break;
      setpathpart(remsongdir, gfname);
#endif      
      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      if( curtune ) curtune->at_doing = D_IDLE;

#ifndef __SDL_WRAPPER__
      lock = IDOS->Lock( sng_lreq->fr_Drawer, ACCESS_READ );
      olddir = IDOS->SetCurrentDir( lock );

      at = rp_load_tune( sng_lreq->fr_File, curtune );
      if( at ) curtune = at;
      
      IDOS->SetCurrentDir( olddir );
      IDOS->UnLock( lock );
#else
      at = rp_load_tune( gfname, curtune );
      free( gfname );
      if( at ) curtune = at;
#endif
      if( at ) gui_set_rempos( at );
      gui_render_tabs();
      gui_render_tunepanel( TRUE );
      break;

    case BBA_CONTPOS:    
    case BBA_PLAYPOS:
      if( curtune == NULL )
        break;
      
      curtune->at_doing = D_IDLE;
      if( rp_play_pos( curtune, b == BBA_CONTPOS ? TRUE : FALSE ) )
        curtune->at_doing = D_PLAYING;
      break;

    case BBA_CONTSONG:
    case BBA_PLAYSONG:
      if( curtune == NULL )
        break;

      curtune->at_doing = D_IDLE;
      if( rp_play_song( curtune, curtune->at_curss, b == BBA_CONTSONG ? TRUE : FALSE ) )
         curtune->at_doing = D_PLAYING;
      break;

    case BBA_STOP:
      rp_stop();
      gui_render_tracked( TRUE );  // Kill the VUMeters
      gui_render_wavemeter();
      curtune->at_doing = D_IDLE;
      break;
    
    case BBA_PREFS:
#ifndef __SDL_WRAPPER__
      if( prefwin )
        gui_close_prefs();
      else
        gui_open_prefs();
#else
      gui_open_prefs();
#endif
      break;
    
    case BBA_CUTTRK:
      cutting = TRUE;
    case BBA_COPYTRK:
      if( curtune->at_cbmarktrack != -1 )
      {
        gui_copyregion( curtune->at_cbmarktrack, curtune->at_cbmarkstartnote, curtune->at_cbmarkendnote, curtune->at_cbmarkbits, cutting );
        curtune->at_cbmarktrack = -1;
        if( curtune->at_curpanel == PN_TRACKER )
          gui_render_tracked( TRUE );      
        break;
      }

      if( track == -1 ) break;
      
      if( button == 1 )
      {
        toclip = CBF_CMD1|CBF_CMD2;
      } else {
        if( qual & IEQUALIFIER_CONTROL )
          toclip = CBF_NOTES;
        else
          toclip = CBF_NOTES|CBF_CMD1|CBF_CMD2;
      }

      gui_copyregion( track, 0, curtune->at_TrackLength-1, toclip, cutting );
      
      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;
    
    case BBA_PASTE:
      if( curtune->at_cbmarktrack != -1 ) curtune->at_cbmarktrack = -1;
      if( track == -1 ) break;

      setbefore_track( curtune, track );
      gui_paste( curtune, track, CBF_NOTES|CBF_CMD1|CBF_CMD2 );
      setafter_track( curtune, track );
      
      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;

    case BBA_NOTEUP:
      if( track == -1 ) break;
      
      setbefore_track( curtune, track );
      
      for( i=0; i<curtune->at_TrackLength; i++ )
      {
        if( ( curtune->at_Tracks[track][i].stp_Note > 0 ) &&
            ( curtune->at_Tracks[track][i].stp_Note < 60 ) )
          curtune->at_Tracks[track][i].stp_Note++;
      }
      
      setafter_track( curtune, track );

      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;

    case BBA_NOTEDN:
      if( track == -1 ) break;
      
      setbefore_track( curtune, track );

      for( i=0; i<curtune->at_TrackLength; i++ )
      {
        if( curtune->at_Tracks[track][i].stp_Note > 1 )
          curtune->at_Tracks[track][i].stp_Note--;
      }

      setafter_track( curtune, track );

      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;
    
    case BBA_REVERSE:
      if( track == -1 ) break;
      
      setbefore_track( curtune, track );

      l = curtune->at_TrackLength>>1;
      b = curtune->at_TrackLength-1;
      for( i=0; i<l; i++ )
      {
        tstp = curtune->at_Tracks[track][i];
        curtune->at_Tracks[track][i] = curtune->at_Tracks[track][b-i];
        curtune->at_Tracks[track][b-i] = tstp;
      }

      setafter_track( curtune, track );

      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;

    case BBA_SCRLUP:
      if( track == -1 ) break;
      
      setbefore_track( curtune, track );

      tstp = curtune->at_Tracks[track][0];
      
      for( i=1; i<curtune->at_TrackLength; i++ )
        curtune->at_Tracks[track][i-1] = curtune->at_Tracks[track][i];
      
      curtune->at_Tracks[track][curtune->at_TrackLength-1] = tstp;

      setafter_track( curtune, track );

      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;

    case BBA_SCRLDN:
      if( track == -1 ) break;
      
      setbefore_track( curtune, track );

      tstp = curtune->at_Tracks[track][curtune->at_TrackLength-1];
      
      for( i=curtune->at_TrackLength-1; i>=0; i-- )
        curtune->at_Tracks[track][i] = curtune->at_Tracks[track][i-1];
      
      curtune->at_Tracks[track][0] = tstp;

      setafter_track( curtune, track );

      if( curtune->at_curpanel == PN_TRACKER )
        gui_render_tracked( TRUE );      
      break;
  }
  
  return TRUE;
}

void gui_press_any_bbank( int32 z, int32 button )
{
  gui_press_bbank( &bbank[0], 16, z, button );
  if (curtune->at_curpanel == PN_TRACKER)
    gui_press_bbank( &tbank[0], 12, z, button );
}

void gui_release_any_bbank( int32 z, int32 button )
{
  gui_release_bbank( &bbank[0], 16, z, button );
  if (curtune->at_curpanel == PN_TRACKER)
    gui_release_bbank( &tbank[0], 12, z, button );
}

BOOL gui_check_any_bbank( int32 z, int32 button )
{
  if( gui_check_bbank( &bbank[0], 16, z, button ) ) return TRUE;
  if (curtune->at_curpanel == PN_TRACKER)
    if( gui_check_bbank( &tbank[0], 12, z, button ) ) return TRUE;
  return FALSE;
}

BOOL gui_check_nbox_butpress( int16 x, int16 y )
{ 
  int32 i, num;
  struct numberbox *nbp = NULL;
  
  if( curtune == NULL ) return FALSE;
  
  switch( curtune->at_curpanel )
  {
    case PN_TRACKER:
      nbp = &trk_nb[0];
      num = NB_END;
      break;
    
    case PN_INSED:
      nbp = &ins_nb[0];
      num = INB_END;
      break;
  }
  
  if( nbp == NULL ) return FALSE;
  
  for( i=0; i<num; i++ )
  {
    if( ( x >= nbp[i].x+nbp[i].w-28 ) && ( x < nbp[i].x+nbp[i].w-14 ) &&
        ( y >= nbp[i].y ) && ( y < nbp[i].y+nbp[i].h ) )
    {
      if( nbp[i].flags & NBF_ENABLED )
      {
        if( ( ( nbp[i].flags & NBF_UDDURINGPLAY ) == 0 ) &&
            ( curtune->at_doing == D_PLAYING ) )
            return FALSE;

        put_partbitmap( BM_PLUSMINUS, 0, 19, nbp[i].x+nbp[i].w-28, nbp[i].y-1, 14, 19 );
        nbp[i].pressed = 1;
      }
      return TRUE;
    }

    if( ( x >= nbp[i].x+nbp[i].w-14 ) && ( x < nbp[i].x+nbp[i].w ) &&
        ( y >= nbp[i].y ) && ( y < nbp[i].y+nbp[i].h ) )
    {
      if( nbp[i].flags & NBF_ENABLED )
      {
        if( ( ( nbp[i].flags & NBF_UDDURINGPLAY ) == 0 ) &&
            ( curtune->at_doing == D_PLAYING ) )
            return FALSE;

        put_partbitmap( BM_PLUSMINUS, 14, 19, nbp[i].x+nbp[i].w-14, nbp[i].y-1, 14, 19 );
        nbp[i].pressed = 2;
      }
      return TRUE;
    }
  }

  return FALSE;
}

BOOL gui_check_nbox_butrelease( int16 x, int16 y )
{
  int32 i, num;
  BOOL ret;
  struct numberbox *nbp = NULL;
  
  if( curtune == NULL ) return FALSE;
  
  ret = FALSE;
  switch( curtune->at_curpanel )
  {
    case PN_TRACKER:
      nbp = &trk_nb[0];
      num = NB_END;
      break;

    case PN_INSED:
      nbp = &ins_nb[0];
      num = INB_END;
      break;
  }
  
  if( nbp == NULL ) return FALSE;
  
  for( i=0; i<num; i++ )
  {
    if( ( x >= nbp[i].x+nbp[i].w-28 ) && ( x < nbp[i].x+nbp[i].w-14 ) &&
        ( y >= nbp[i].y ) && ( y < nbp[i].y+nbp[i].h ) )
    {
      if( nbp[i].pressed == 1 )
      {
        if( qual & IEQUALIFIER_RBUTTON )
          nbp[i].cnum+=10;
        else
          nbp[i].cnum++;
        gui_update_from_nbox( curtune->at_curpanel, nbp, i );
        ret = TRUE;
      }
    }

    if( ( x >= nbp[i].x+nbp[i].w-14 ) && ( x < nbp[i].x+nbp[i].w ) &&
        ( y >= nbp[i].y ) && ( y < nbp[i].y+nbp[i].h ) )
    {
      if( nbp[i].pressed == 2 )
      {
        if( qual & IEQUALIFIER_RBUTTON )
          nbp[i].cnum-=10;
        else
          nbp[i].cnum--;
        gui_update_from_nbox( curtune->at_curpanel, nbp, i );
        ret = TRUE;
      }
    }
    
    if( nbp[i].pressed != 0 )
    {
      nbp[i].pressed = 0;
      put_partbitmap( BM_PLUSMINUS, 0, 0, nbp[i].x+nbp[i].w-28, nbp[i].y-1, 28, 19 );
    }
  }
  
  return ret;
}

BOOL gui_check_ptbox_press( int16 x, int16 y )
{
  int32 i;
  
#ifndef __SDL_WRAPPER__
  // *sigh* i suck, i know i suck.
  x += pw_bl;
  y += pw_bt;
#else
  if (!prefwin_open) return FALSE;
#endif

  for( i=0; i<PTB_END; i++ )
  {
    if( ( x >= ptb[i].x ) && ( x < ptb[i].x+ptb[i].w ) &&
        ( y >= ptb[i].y ) && ( y < ptb[i].y+16 ) &&
        ( ptb[i].flags & TBF_ENABLED ) &&
        ( ptb[i].flags & TBF_VISIBLE ) &&
        ( ptb[i].content != NULL ) )
    {
      if( ( ptbx != NULL ) && ( ptbx != &ptb[i] ) )
      {
        ptbx->flags &= ~TBF_ACTIVE;
        gui_render_tbox( &prefbm, ptbx );
      }
      ptbx = &ptb[i];
#ifdef __SDL_WRAPPER__
      SDL_EnableUNICODE(SDL_TRUE);
#endif
      ptbx->flags |= TBF_ACTIVE;
      ptbx->cpos = ((x-ptbx->x)>>3)+ptbx->spos;
      gui_render_tbox( &prefbm, ptbx );
#ifdef __SDL_WRAPPER__
      bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
      return TRUE;
    }
  }

  return FALSE;
}

void gui_ptbox_finish_editing( void )
{
  if( ptbx == NULL ) return;
  
  ptbx = NULL;
#ifdef __SDL_WRAPPER__
  SDL_EnableUNICODE(SDL_FALSE);
#endif
}

void gui_tbox_finish_editing( void )
{
  if( etbx == NULL ) return;
  
  if( etbx == &tbx[TB_SONGNAME] )
  {
    setafter_string( UNT_STRING_SONGNAME, curtune, etbx->content );
    gui_render_tabs();
  }
  
  if( etbx == &tbx[TB_INSNAME] )
  {
    setafter_string( UNT_STRING_INSNAME, curtune, etbx->content );
    gui_render_inslistb( TRUE );
  }

  if( etbx == &tbx[TB_INSNAME2] )
  {
    setafter_string( UNT_STRING_INSNAME2, curtune, etbx->content );
    tbx[TB_INSNAME2].flags &= ~TBF_VISIBLE;
    tbx[TB_INSNAME2].content = NULL;
    gui_render_inslist( TRUE );
  }

#ifdef __SDL_WRAPPER__
  SDL_EnableUNICODE(SDL_FALSE);
#endif
  etbx = NULL;
}

BOOL gui_check_tbox_press( int16 x, int16 y )
{
  int32 i;
  int16 panel;

  panel = PN_TRACKER;
  if( curtune )
    panel = curtune->at_curpanel;

  for( i=0; i<TB_END; i++ )
  {
    if( panel == tbx[i].inpanel )
    {
      if( ( x >= tbx[i].x ) && ( x < tbx[i].x+tbx[i].w ) &&
          ( y >= tbx[i].y ) && ( y < tbx[i].y+16 ) &&
          ( tbx[i].flags & TBF_ENABLED ) &&
          ( tbx[i].flags & TBF_VISIBLE ) &&
          ( tbx[i].content != NULL ) )
      {
        if( ( etbx != NULL ) && ( etbx != &tbx[i] ) )
        {
          etbx->flags &= ~TBF_ACTIVE;
          gui_render_tbox( &mainbm, etbx );
          gui_tbox_finish_editing();
        }
        setbefore_string( curtune, tbx[i].content );
        etbx = &tbx[i];
        etbx->flags |= TBF_ACTIVE;
        etbx->cpos = ((x-etbx->x)>>3)+etbx->spos;
        gui_render_tbox( &mainbm, etbx );
#ifdef __SDL_WRAPPER__
        SDL_EnableUNICODE(SDL_TRUE);
#endif
        return TRUE;
      }
    }
  }

  return FALSE;
}

void gui_press_mute( int32 zone )
{
  int32 i, mch;

  if( curtune == NULL ) return;
  if( curtune->at_curpanel != PN_TRACKER ) return;
  
  mch=0;
  for( i=0; i<6; i++ )
    if( zone == zn_mute[i] ) break;
  
  if( i == 6 ) return;
  
  mch = i+curtune->at_curlch;

  if( curtune->at_Voices[mch].vc_SetTrackOn )
    put_partbitmap( BM_CHANMUTE, 0, 19, zones[zone].x, zones[zone].y, 14, 19 );
  else
    put_partbitmap( BM_CHANMUTE, 0,  0, zones[zone].x, zones[zone].y, 14, 19 );
}

void gui_release_mute( int32 zone )
{
  int32 i, mch;

  if( curtune == NULL ) return;
  if( curtune->at_curpanel != PN_TRACKER ) return;
  
  mch=0;
  for( i=0; i<6; i++ )
    if( zone == zn_mute[i] ) break;
  
  if( i == 6 ) return;
  
  mch = i+curtune->at_curlch;
 
  if( curtune->at_Voices[mch].vc_SetTrackOn )
    put_partbitmap( BM_CHANMUTE, 0,  0, zones[zone].x, zones[zone].y, 14, 19 );
  else
    put_partbitmap( BM_CHANMUTE, 0, 19, zones[zone].x, zones[zone].y, 14, 19 );
}

BOOL gui_check_mute( int32 zone )
{
  int32 i, mch;

  if( curtune == NULL ) return FALSE;
  if( curtune->at_curpanel != PN_TRACKER ) return FALSE;
  
  mch=0;
  for( i=0; i<6; i++ )
    if( zone == zn_mute[i] ) break;
  
  if( i == 6 ) return FALSE;
  
  mch = i+curtune->at_curlch;
 
  curtune->at_Voices[mch].vc_SetTrackOn ^= 1;
  curtune->at_Voices[mch].vc_TrackOn = curtune->at_Voices[mch].vc_SetTrackOn;

  if( curtune->at_Voices[mch].vc_SetTrackOn )
    put_partbitmap( BM_CHANMUTE, 0,  0, zones[zone].x, zones[zone].y, 14, 19 );
  else
    put_partbitmap( BM_CHANMUTE, 0, 19, zones[zone].x, zones[zone].y, 14, 19 );

  return TRUE;
}

BOOL gui_check_rmb_mute( int32 zone )
{
  int32 i, mch;

  if( curtune == NULL ) return FALSE;
  if( curtune->at_curpanel != PN_TRACKER ) return FALSE;
  
  mch=0;
  for( i=0; i<6; i++ )
    if( zone == zn_mute[i] ) break;
  
  if( i == 6 ) return FALSE;
  
  mch = i+curtune->at_curlch;

  // Already solo'd?
  for( i=0; i<curtune->at_Channels; i++ )
  {
    if( i == mch )
    {
      if( curtune->at_Voices[i].vc_SetTrackOn == 0 ) break;
    } else {
      if( curtune->at_Voices[i].vc_SetTrackOn != 0 ) break;
    }
  }
  
  if( i == curtune->at_Channels )
  {
    // Already solo'd, so set all tracks on
    for( i=0; i<curtune->at_Channels; i++ )
      curtune->at_Voices[i].vc_SetTrackOn = 1;
  } else {
    // Not solo'd, so solo it
    for( i=0; i<curtune->at_Channels; i++ )
      curtune->at_Voices[i].vc_SetTrackOn = i==mch ? 1 : 0;
  }
  
  for( i=0; i<curtune->at_Channels; i++ )
  {
    curtune->at_Voices[i].vc_TrackOn = curtune->at_Voices[i].vc_SetTrackOn;
    
    if( ( i >= curtune->at_curlch ) && ( i < (curtune->at_curlch+6) ) )
    {
      zone = zn_mute[i-curtune->at_curlch];
      if( curtune->at_Voices[i].vc_SetTrackOn )
        put_partbitmap( BM_CHANMUTE, 0,  0, zones[zone].x, zones[zone].y, 14, 19 );
      else
        put_partbitmap( BM_CHANMUTE, 0, 19, zones[zone].x, zones[zone].y, 14, 19 );
    }
  }

  return TRUE;
}

BOOL gui_maybe_quit( void )
{
  struct ahx_tune *at;
  BOOL anymod;
  
  anymod = FALSE;

  at = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
  while( at )
  {
    if( at->at_modified )
    {
      anymod = TRUE;
      break;
    }
    at = (struct ahx_tune *)IExec->GetSucc(&at->at_ln);
  }
  
  if( anymod )
    return gui_req( REQIMAGE_QUESTION, "HivelyTracker", "One or more songs have been modified. Really quit?", "_Yep!|Arrgh.. _No!" );
    
  return gui_req( REQIMAGE_QUESTION, "HivelyTracker", "Are you sure you want to quit?", "_Yep!|Arrgh.. _No!" );
}

void gui_wheelybin( int16 mousex, int16 mousey, int16 wheeldir )
{
  int32 i, chan;
  struct ahx_plsentry *cple;
  struct ahx_instrument *cins;

  if( wheeldir == 0 ) return;
  if( !curtune ) return;

  switch( curtune->at_curpanel )
  {
    case PN_TRACKER:

      // Check numberboxes
      for( i=0; i<NB_END; i++ )
      {
        if( ( mousex >= trk_nb[i].x ) &&
            ( mousex < (trk_nb[i].x+trk_nb[i].w ) ) &&
            ( mousey >= trk_nb[i].y ) &&
            ( mousey < (trk_nb[i].y+trk_nb[i].h ) ) )
          break;
      }
                
      if( i < NB_END )
      {
        if( trk_nb[i].flags & NBF_ENABLED )
        {
          if( ( ( trk_nb[i].flags & NBF_UDDURINGPLAY ) == 0 ) &&
              ( curtune->at_doing == D_PLAYING ) )
            return;

          trk_nb[i].cnum -= wheeldir;
          gui_update_from_nbox( curtune->at_curpanel, &trk_nb[0], i );
        }
        return;
      }
                    
      if( ( mousey >= (TRACKED_Y-22) ) && ( mousey < TRACKED_Y ) &&
          ( mousex >= (TRACKED_X+24) ) && ( mousex < (TRACKED_X+TRACKED_W) ) )
      {
        i    = (mousex-(TRACKED_X+24)) % 120;
        chan = (mousex-(TRACKED_X+24)) / 120 + curtune->at_curlch;
                      
        if( i < 78 ) return;
        if( chan >= curtune->at_Channels ) return;
                      
        modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, curtune->at_Positions[curtune->at_PosNr].pos_Track[chan] - wheeldir );
        gui_render_posed( TRUE );
        gui_render_tracked( TRUE );
        return;
      }

      if( ( mousex >= TRACKED_X ) && ( mousex < (TRACKED_X+TRACKED_W) ) &&
          ( mousey >= TRACKED_Y ) && ( mousey < (TRACKED_Y+TRACKED_H) ) )
      {
        if( curtune->at_doing == D_PLAYING ) return;

        i = curtune->at_NoteNr + wheeldir;
        if( i <= 0 ) i = 0;
        if( i >= curtune->at_TrackLength ) i = curtune->at_TrackLength-1;
        curtune->at_NoteNr = i;

        gui_render_tracker( FALSE );
        return;
      }

      if( ( mousex >= POSED_X ) && ( mousex < POSED_X+POSED_W ) &&
          ( mousey >= POSED_Y ) && ( mousey < POSED_Y+POSED_H ) )
      {
        if( curtune->at_doing == D_PLAYING )
        {
          int16 next;
          if( curtune->at_NextPosNr == -1 )
            next = curtune->at_PosNr + wheeldir;
          else
            next = curtune->at_NextPosNr + wheeldir;
          if( next < 0 )   next = 0;
          if( next > 999 ) next = 999;
          curtune->at_NextPosNr = next;
        } else {
          curtune->at_PosNr += wheeldir;
          if( curtune->at_PosNr < 0 )   curtune->at_PosNr = 0;
          if( curtune->at_PosNr > 999 ) curtune->at_PosNr = 999;
        }
        gui_render_tracker( FALSE );
        return;
      }

      if( ( mousex >= INSLIST_X ) && ( mousex < (INSLIST_X+INSLIST_W) ) &&
          ( mousey >= INSLIST_Y ) && ( mousey < (INSLIST_Y+INSLIST_H) ) )
      {
        curtune->at_curins += wheeldir;
        if( curtune->at_curins < 1 ) curtune->at_curins = 1;
        if( curtune->at_curins > 63 ) curtune->at_curins = 63;
        gui_render_inslist( FALSE );
        gui_set_various_things( curtune );
      }

      break;

    case PN_INSED:

      // Check numberboxes
      for( i=0; i<INB_END; i++ )
      {
        if( ( mousex >= ins_nb[i].x ) &&
            ( mousex < (ins_nb[i].x+ins_nb[i].w ) ) &&
            ( mousey >= ins_nb[i].y ) &&
            ( mousey < (ins_nb[i].y+ins_nb[i].h ) ) )
          break;
      }
                
      if( i < INB_END )
      {
        if( ins_nb[i].flags & NBF_ENABLED )
        {
          if( ( ( ins_nb[i].flags & NBF_UDDURINGPLAY ) == 0 ) &&
              ( curtune->at_doing == D_PLAYING ) )
            return;

          ins_nb[i].cnum -= wheeldir;
          gui_update_from_nbox( curtune->at_curpanel, &ins_nb[0], i );
        }
        return;
      }
                    
      cins = NULL;
      cple = NULL;
      if( curtune )
      {
        cins = &curtune->at_Instruments[curtune->at_curins];
        cple = &cins->ins_PList.pls_Entries[cins->ins_pcury];
      }

      if( ( cins != NULL ) &&
          ( mousex >= PERF_X ) && ( mousex < (PERF_X+PERF_W) ) &&
          ( mousey >= PERF_Y ) && ( mousey < (PERF_Y+PERF_H) ) )
      {
        cins->ins_pcury += wheeldir;
        gui_render_perf( curtune, cins, FALSE );
        return;
      }

      if( ( curtune != NULL ) &&
          ( mousex >= INSLSTB_X ) && ( mousex < (INSLSTB_X+INSLSTB_W) ) &&
          ( mousey >= INSLSTB_Y ) && ( mousey < (INSLSTB_Y+INSLSTB_H) ) )
      {
        curtune->at_curins += wheeldir;
        if( curtune->at_curins < 0 ) curtune->at_curins = 0;
        if( curtune->at_curins > 63 ) curtune->at_curins = 63;
        cins = &curtune->at_Instruments[curtune->at_curins];
        cple = &cins->ins_PList.pls_Entries[cins->ins_pcury];
        gui_set_various_things( curtune );
      }

      break;
  }
}

void gui_mouse_handler( int16 x, int16 y, uint32 code )
{
  int32 i, j;

  switch( code )
  {
    case SELECTDOWN:
      if( gui_check_tbox_press( x, y ) ) return;
      
      if( etbx )
      {
        etbx->flags &= ~TBF_ACTIVE;
        gui_render_tbox( &mainbm, etbx );
        gui_tbox_finish_editing();
        return;
      }
      
      if( ( curtune ) && ( curtune->at_curpanel == PN_TRACKER ) )
      {
        if( ( x >= TRACKED_X ) && ( x < 744  ) &&
            ( y >= TRACKED_Y ) && ( y < TRACKED_Y+TRACKED_H ) )
        {
          curtune->at_editing = E_TRACK;
            
          if( ( curtune ) && ( x > (TRACKED_X+23) ) )
          {
            i = (x-(TRACKED_X+24)) >> 3;
            j = i % 15;
            i /= 15;
            
            switch( j )
            {
              case 0:
              case 1:
              case 2:
              case 3:
                j = 0;
                break;
              case 4:
                j = 1;
                break;
              case 5:
              case 6:
                j = 2;
                break;
              case 7:
                j = 3;
                break;
              case 8:
                j = 4;
                break;
              case 9:
              case 10:
                j = 5;
                break;
              case 11:
                j = 6;
                break;
              case 12:
                j = 7;
                break;
              case 13:
              case 14:
                j = 8;
                break;
            }
            
            if( i < curtune->at_Channels )
              curtune->at_tracked_curs = i*9+j;
          }
            
          gui_render_tracked( TRUE );
          gui_render_posed( TRUE );
          break;
        }
      
        if( ( x >= POSED_X ) && ( x < POSED_X+POSED_W ) &&
            ( y >= POSED_Y ) && ( y < POSED_Y+POSED_H ) )
        {
          curtune->at_editing = E_POS;
          
          if( ( curtune ) && ( x > (POSED_X+27) ) )
          {
            i = (x-(POSED_X+28)) / 7;
            j = i % 7;
            i /= 7;
            
            switch( j )
            {
              case 3:
                j = 2;
                break;
              case 4:
                j = 3;
                break;
              case 5:
              case 6:
                j = 4;
                break;
            }
          
            if( i < curtune->at_Channels )
              curtune->at_posed_curs = i*5+j;
          }
          
          gui_render_tracked( TRUE );
          gui_render_posed( TRUE );
          break;
        }
      }
      
      if( gui_check_nbox_butpress( x, y ) )
        break;
      
      cz_lmb = gui_get_zone( &zones[0], numzones, x, y );
      if( cz_lmb != -1 )
      {
        gui_press_mute( cz_lmb );
        gui_press_any_bbank( cz_lmb, 0 );
      }
      break;
    case SELECTUP:
      if( etbx != NULL ) break;

      if( gui_check_nbox_butrelease( x, y ) )
        break;

      if( cz_lmb != -1 )
      {
        gui_release_any_bbank( cz_lmb, 0 );
        gui_press_mute( cz_lmb );
      }

      if( ( cz_lmb != -1 ) && ( cz_lmb == gui_get_zone( &zones[0], numzones, x, y ) ) )
      {
        if( gui_check_mute( cz_lmb ) )
        {
          cz_lmb = -1;
          break;
        }

        if( gui_check_any_bbank( cz_lmb, 0 ) )
        {
          cz_lmb = -1;
          if( quitting ) return;
          break;
        }

        if( cz_lmb == zn_close )
        {
          // Close gadget
          quitting = gui_maybe_quit();
        } else if( cz_lmb == zn_scrdep ) {
#ifndef __SDL_WRAPPER__
          if( scr ) IIntuition->ScreenToBack( scr );
#endif
        } else {
        
          // Check for tab zones
          for( i=0; i<8; i++ )
          {
            if( cz_lmb == ttab[i].zone )
            {
              BOOL dorend;
              
              dorend = FALSE;
              if( x < (zones[ttab[i].zone].x+20) )
              {
                if( ttab[i].tune->at_modified )
                  if( gui_req( REQIMAGE_QUESTION, "HivelyTracker", "This song has been modified. Continue?", "_Yep!|Arrgh.. _No!" ) == 0 )
                    break;

                // Only one tune?
                IExec->ObtainSemaphore( rp_list_ss );
                if( IExec->GetSucc(IExec->GetHead(rp_tunelist)) == NULL )
                {
                  rp_clear_tune( ttab[i].tune );
                  gui_set_various_things( ttab[i].tune );
                  dorend = TRUE;
                } else {
                  rp_free_tune( ttab[i].tune );
                  if( curtune == ttab[i].tune )
                  {
                    curtune = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
                    gui_set_various_things(curtune);
                  }

                  dorend = TRUE;
                }
                IExec->ReleaseSemaphore( rp_list_ss );
              } else {
                if( ttab[i].tune != curtune )
                {
                  curtune = ttab[i].tune;
                  dorend = TRUE;
                }
              }
              
              if( dorend )
              {
                gui_render_tabs();
                gui_render_tunepanel( TRUE );
              }
              break;
            }
          }

        }

        cz_lmb = -1;
        break;
      }

      if( ( curtune ) && ( curtune->at_curpanel == PN_TRACKER ) )
      {
        if( ( x >= INSLIST_X ) && ( x < (INSLIST_X+INSLIST_W) ) &&
            ( y >= INSLIST_Y ) && ( y < (INSLIST_Y+INSLIST_H) ) )
        {
          int32 i;
          
          i = (y-(INSLIST_Y+5))/14;
          if( i < 0 ) i = 0;
          curtune->at_curins = i+curtune->at_topins;
          gui_render_inslist( FALSE );
          gui_set_various_things( curtune );
        }
      }

      if( ( curtune ) && ( curtune->at_curpanel == PN_INSED ) )
      {
        if( ( x >= INSLSTB_X ) && ( x < (INSLIST_X+INSLSTB_W) ) &&
            ( y >= INSLSTB_Y ) && ( y < (INSLIST_Y+INSLSTB_H) ) )
        {
          int32 i;
          
          i = (y-(INSLSTB_Y+4))>>4;
          if( i < 0 ) i = 0;
          curtune->at_curins = i+curtune->at_topinsb;
          gui_set_various_things( curtune );
        }

        if( ( x >= PERF_X ) && ( x < (PERF_X+PERF_W) ) &&
            ( y >= PERF_Y ) && ( y < (PERF_Y+PERF_H) ) )
        {
          int32 i;
          struct ahx_instrument *cins;

          cins = &curtune->at_Instruments[curtune->at_curins];
          
          i = (y-(PERF_Y+4))>>4;
          if( i < 0 ) i = 0;
          cins->ins_pcury = i+cins->ins_ptop;
          gui_set_various_things( curtune );
        }
      }
      break;
    case MENUDOWN:
      if( etbx != NULL ) break;
      cz_rmb = gui_get_zone( &zones[0], numzones, x, y );
      if( cz_rmb != -1 )
      {
        gui_press_any_bbank( cz_rmb, 1 );
        gui_press_mute( cz_rmb );
      }
      break;
    case MENUUP:
      if( etbx != NULL ) break;

      if( cz_rmb != -1 )
      {
        gui_release_any_bbank( cz_rmb, 1 );
        gui_release_mute( cz_rmb );
      }

      if( ( cz_rmb != -1 ) && ( cz_rmb == gui_get_zone( &zones[0], numzones, x, y ) ) )
      {
        if( gui_check_rmb_mute( cz_rmb ) )
        {
          cz_rmb = -1;
          break;
        }

        if( gui_check_any_bbank( cz_rmb, 1 ) )
        {
          cz_rmb = -1;
          break;
        }

        cz_rmb = -1;
        break;
      }
      
      if( ( curtune ) && ( curtune->at_curpanel == PN_TRACKER ) )
      {
        if( ( x >= INSLIST_X ) && ( x < (INSLIST_X+INSLIST_W) ) &&
            ( y >= INSLIST_Y ) && ( y < (INSLIST_Y+INSLIST_H) ) )
        {
          int32 i, j;
          
          i = (y-(INSLIST_Y+5))/14;
          if( i < 0 ) i = 0;
          j = i+curtune->at_topins;
       
          tbx[TB_INSNAME2].content  = curtune->at_Instruments[j].ins_Name;
          tbx[TB_INSNAME2].y        = (INSLIST_Y+5)+(i*14)-1;
          tbx[TB_INSNAME2].flags   |= TBF_VISIBLE|TBF_ACTIVE;
          tbx[TB_INSNAME2].spos = 0;
          setbefore_string( curtune, tbx[TB_INSNAME2].content );
          etbx = &tbx[TB_INSNAME2];
          if( x < etbx->x ) x = etbx->x;
          etbx->cpos = ((x-etbx->x)>>3)+etbx->spos;
          gui_render_tbox( &mainbm, etbx );
        }
      }
      
      if( ( curtune ) && ( curtune->at_curpanel == PN_INSED ) )
      {
        if( ( x >= INSLSTB_X ) && ( x < (INSLSTB_X+INSLSTB_W) ) &&
            ( y >= INSLSTB_Y ) && ( y < (INSLSTB_Y+INSLSTB_H) ) )
        {
          int32 i, j;
          BOOL doit = TRUE;
          
          i = (y-INSLSTB_Y)>>4;
          i += curtune->at_topinsb;
          if( i < 1 ) i = 1;
          if( i > 63 ) i = 63;
          j = curtune->at_curins;
          
          if( qual & IEQUALIFIER_CONTROL )
          {
            rp_clear_instrument( &curtune->at_Instruments[i] );
            curtune->at_curins = i;
          } else {
            if( i == j ) break;
            
            curtune->at_curins = i;
            if( curtune->at_Instruments[i].ins_PList.pls_Length != 0 )
            {
              gui_render_inslistb( TRUE );
              doit = gui_req( REQIMAGE_QUESTION, "Copy Instrument", "That instrument is not empty.\nDo you want to copy over it?", "_Yes yes!|Whoooooooops! _No!!!" );
            }
          
            if( doit )
              curtune->at_Instruments[i] = curtune->at_Instruments[j];
          }
          
          if( doit )
          {
            gui_render_tunepanel( TRUE );
            curtune->at_modified = TRUE;
          }
        }
      }
      
      break;
  }
}

void gui_posed_moveright( void )
{
  curtune->at_posed_curs++;
                  
  if( (curtune->at_posed_curs/5+curtune->at_curlch) >= curtune->at_Channels )
  {
    curtune->at_posed_curs = 0;
    curtune->at_curlch = 0;
  }
                
  while( (curtune->at_posed_curs/5) > 5 )
  {
    curtune->at_curlch++;
    curtune->at_posed_curs -= 5;
  }
}

void gui_textbox_keypress( struct rawbm *bm, struct textbox **ttbx, struct IntuiMessage *msg )
{
  int32 i, j, actual;
  TEXT kbuf[80];
  int32 lqual;
  
  if( *ttbx == NULL ) return;

  lqual = msg->Qualifier;

  switch( msg->Code )
  {
    case 79:  // Left arrow
      if( lqual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
        (*ttbx)->cpos = 0;
      else
        if( (*ttbx)->cpos > 0 ) (*ttbx)->cpos--;
      gui_render_tbox( bm, *ttbx );
      break;
    case 78:  // Right arrow
      if( lqual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
        (*ttbx)->cpos = strlen( (*ttbx)->content );
      else
        (*ttbx)->cpos++;
      gui_render_tbox( bm, *ttbx );
      break;
    case 65:  // Backspace
      if( (*ttbx)->cpos == 0 ) break;
      
      j = strlen( (*ttbx)->content );

      if( lqual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
      {
        for( i=(*ttbx)->cpos; i<=j; i++ ) (*ttbx)->content[i-(*ttbx)->cpos] = (*ttbx)->content[i];
        (*ttbx)->cpos = 0;
        gui_render_tbox( bm, *ttbx );
        break;
      }
      
      for( i=(*ttbx)->cpos-1; i<j; i++ ) (*ttbx)->content[i] = (*ttbx)->content[i+1];
      (*ttbx)->cpos--;
      gui_render_tbox( bm, *ttbx );
      break;
    case 70:  // Del
      if( lqual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
      {
        (*ttbx)->content[(*ttbx)->cpos] = 0;
        gui_render_tbox( bm, *ttbx );
        break;
      }

      if( (*ttbx)->cpos >= strlen( (*ttbx)->content ) ) break;
      j = strlen( (*ttbx)->content );
      for( i=(*ttbx)->cpos; i<j; i++ ) (*ttbx)->content[i] = (*ttbx)->content[i+1];
      gui_render_tbox( bm, *ttbx );
      break;
    case 68:  // Enter
    case 197:  // ESC
      (*ttbx)->flags &= ~TBF_ACTIVE;
      gui_render_tbox( bm, *ttbx );
      if( bm == &mainbm )
        gui_tbox_finish_editing();
      else
        gui_ptbox_finish_editing();
      break;
    default:
#ifndef __SDL_WRAPPER__
      iev.ie_Code         = msg->Code;
      iev.ie_Qualifier    = msg->Qualifier;
      /* recover dead key codes & qualifiers */
      iev.ie_EventAddress = (APTR *) *((ULONG *)msg->IAddress);
      
      actual = (IKeymap->MapRawKey( &iev, kbuf, 80, 0 ) == 1);
#else
      actual = (((event.key.keysym.unicode&0xff)!=0) && ((event.key.keysym.unicode&0xff00)==0));
      kbuf[0] = event.key.keysym.unicode;
#endif
      if( ( actual ) && ( kbuf[0] > 31 ) && ( kbuf[0] < 128 ) )
      {
        if( strlen( (*ttbx)->content ) >= (*ttbx)->maxlen ) break;
        
        j = strlen( (*ttbx)->content );
        for( i=j+1; i>(*ttbx)->cpos; i-- ) (*ttbx)->content[i] = (*ttbx)->content[i-1];
        (*ttbx)->content[(*ttbx)->cpos] = kbuf[0];
        (*ttbx)->cpos++;
        gui_render_tbox( bm, *ttbx );
//      } else {
//        if( msg->Code < 128 )
//          printf( "%d (%d)\n", msg->Code, msg->Code+128 );
      }
      break;
  }
}

int32 gui_whichpop( int16 x, int16 y )
{
  int i;
  
  for( i=0; i<PP_END; i++ )
  {
    if( ( x >= pp[i].x ) && ( x < pp[i].x+20 ) &&
        ( y >= pp[i].y ) && ( y < pp[i].y+18 ) )
      return i;
  }
  
  return -1;
}

BOOL gui_checkpop_down( int16 x, int16 y )
{
  whichpop = gui_whichpop( x, y );
  if( whichpop == -1 ) return FALSE;

  gui_render_popup( whichpop, TRUE );
#ifdef __SDL_WRAPPER__
  bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
  return TRUE;
}

BOOL gui_checkpop_up( int16 x, int16 y )
{
  uint32 ok;

  if( whichpop != -1 )
  {
    if( gui_whichpop( x, y ) == whichpop )
    {
      switch( whichpop )
      {
        case PP_SONGDIR:
#ifndef __SDL_WRAPPER__
          ok = IAsl->AslRequestTags( dir_req,
            ASLFR_Window,        mainwin,
            ASLFR_SleepWindow,   TRUE,
            ASLFR_InitialDrawer, songdir,
            ASLFR_TitleText,     "Select default song directory",
            TAG_DONE );
          if( !ok ) break;

          strncpy( songdir, dir_req->fr_Drawer, 512 );
#else
          if (directoryrequester("Select default song directory", songdir))
            strncpy(remsongdir, songdir, 512);    
#endif
          gui_render_tbox( &prefbm, &ptb[PTB_SONGDIR] );
          break;

        case PP_INSTDIR:
#ifndef __SDL_WRAPPER__
          ok = IAsl->AslRequestTags( dir_req,
            ASLFR_Window,        mainwin,
            ASLFR_SleepWindow,   TRUE,
            ASLFR_InitialDrawer, instdir,
            ASLFR_TitleText,     "Select default instrument directory",
            TAG_DONE );
          if( !ok ) break;
          
          strncpy( instdir, dir_req->fr_Drawer, 512 );
#else
          if (directoryrequester("Select default instrument directory", instdir))
            strncpy(reminstdir, instdir, 512);    
#endif
          gui_render_tbox( &prefbm, &ptb[PTB_INSTDIR] );  
          break;

        case PP_SKINDIR:
#ifndef __SDL_WRAPPER__
          ok = IAsl->AslRequestTags( dir_req,
            ASLFR_Window,        mainwin,
            ASLFR_SleepWindow,   TRUE,
            ASLFR_InitialDrawer, skindir,
            ASLFR_TitleText,     "Select skin directory",
            TAG_DONE );
          if( !ok ) break;
          
          strncpy( skindir, dir_req->fr_Drawer, 512 );
#else
#ifdef __APPLE__
          osxGetResourcesPath(skindir, "Skins");
          directoryrequester("Select skin directory", skindir);
          // Convert absolute path to relative path for bundle resources
          char *resPath = osxGetResourcesPath(NULL, "");
          if(strncmp(resPath, skindir, strlen(resPath)) == 0)
            strcpy(skindir, skindir + strlen(resPath) + 1);
#else
          directoryrequester("Select skin directory", skindir);
#endif
#endif
          gui_render_tbox( &prefbm, &ptb[PTB_SKINDIR] );  
          break;
      }
    }
      
    gui_render_popup( whichpop, FALSE );
    whichpop = -1;
  }

#ifdef __SDL_WRAPPER__
  bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
  return gui_whichpop( x, y ) != -1;
}

int32 gui_whichpc( int16 x, int16 y )
{
  int i;
  
  for( i=0; i<PC_END; i++ )
  {
#if defined(WIN32) || defined(__APPLE__)
    if( i == 0 ) continue;
#endif
    if( ( x >= pcyc[i].x ) && ( x < pcyc[i].x+160 ) &&
        ( y >= pcyc[i].y ) && ( y < pcyc[i].y+24 ) )
      return i;
  }
  
  return -1;
}

BOOL gui_checkpc_down( int16 x, int16 y )
{
  whichcyc = gui_whichpc( x, y );
  if( whichcyc == -1 ) return FALSE;

  gui_render_pcycle( whichcyc, TRUE );
#ifdef __SDL_WRAPPER__
  bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
  return TRUE;
}

BOOL gui_checkpc_up( int16 x, int16 y )
{
  if( whichcyc != -1 )
  {
    if( gui_whichpc( x, y ) == whichcyc )
    {
      pcyc[whichcyc].copt = (pcyc[whichcyc].copt+1)%pcyc[whichcyc].numopts;
      
      switch( whichcyc )
      {
        case PC_WMODE:
          pref_fullscr = (pcyc[PC_WMODE].copt == 1);
          break;

        case PC_DEFSTEREO:
          pref_defstereo = pcyc[PC_DEFSTEREO].copt;
          break;

        case PC_BLANKZERO:
          pref_blankzeros = (pcyc[PC_BLANKZERO].copt == 1);
          if( curtune ) gui_render_tracked( TRUE );
          break;
        
        case PC_MAXUNDOBUF:
          pref_maxundobuf = pcyc[PC_MAXUNDOBUF].copt;
          break;
      }
    }
      
    gui_render_pcycle( whichcyc, FALSE );
#ifdef __SDL_WRAPPER__
    bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
  }
  return gui_whichpc( x, y ) != -1;
}

int32 gui_findlastpos( struct ahx_tune *at )
{
  int32 maxpos, i;
  
  for( maxpos=999; maxpos>0; maxpos-- )
  {
    for( i=0; i<MAX_CHANNELS; i++ )
      if( ( at->at_Positions[maxpos].pos_Track[i] != 0 ) ||
          ( at->at_Positions[maxpos].pos_Transpose[i] != 0 ) )
        return maxpos;
  }
  
  return 0;
}

void gui_render_everything( void )
{
  // Do the logo  
  put_bitmap( BM_LOGO, 0, 0, 256, 96 );
  put_bitmap( BM_DEPTH, 776, 0, 24, 24 );
  put_bitmap( BM_BLANK, 776, 24, 24, 72 );
  gui_render_main_buttonbank();
  gui_render_tunepanel( TRUE );
  gui_render_wavemeter();
#ifdef __SDL_WRAPPER__
  if (prefwin_open) gui_render_prefs();
#endif
}

BOOL gui_restart( void )
{
#ifndef __SDL_WRAPPER__
  int32 i;
  BOOL oh_crap = FALSE;

  // Shutdown just the things we have to shut in order to reopen
  // with new settings
  gui_close_prefs();
  about_close();

  IIntuition->CloseWindow( mainwin );
  mainwin = NULL;
  gui_sigs &= ~mainwin_sig;
  mainwin_sig = 0;
  
  if( scr ) IIntuition->CloseScreen( scr );
  scr = NULL;

  for( i=0; i<BM_END; i++ ) { IGraphics->FreeBitMap( bitmaps[i].bm ); bitmaps[i].bm = NULL; }
  for( i=0; i<TB_END; i++ ) { IGraphics->FreeBitMap( tbx[i].bm.bm ); tbx[i].bm.bm = NULL; }

  IGraphics->CloseFont( prpfont ); prpfont = NULL;
  IGraphics->CloseFont( fixfont ); fixfont = NULL;
  IGraphics->CloseFont( sfxfont ); sfxfont = NULL;

  // Set some defaults
  strcpy( fixfontname, "Bitstream Vera Sans Mono.font" );
  strcpy( sfxfontname, "Bitstream Vera Sans Mono.font" );
  strcpy( prpfontname, "Bitstream Vera Sans.font" );
  strcpy( skinext,     ".png" );

  numzones = 0;
  numpzones = 0;

  if (!oh_crap)
  {
    if (!gui_open())
    {
      oh_crap = TRUE;
    }
  }

  if( oh_crap )
  {
    BPTR cdir, lock;
    int32 i;
    struct ahx_tune *at;
    char rname[64];

    gui_req( REQIMAGE_ERROR, "Oh crap!", "There was an error re-opening the GUI,\nand Hively has to quit. All open songs\nwill be saved to 'Songs/Rescue'", "OK" );

    lock = IDOS->Lock( "Songs/Rescue", ACCESS_READ );
    if( !lock )
    {
      lock = IDOS->CreateDirTree( "Songs/Rescue" );
      if( !lock )
      {
        lock = IDOS->Lock( "", ACCESS_READ );
        if( !lock )
        {
          // Oh fuck.
          return FALSE;
        }
      }
    }
    cdir = IDOS->SetCurrentDir( lock );

    IExec->ObtainSemaphore( rp_list_ss );

    i=1;
    at = (struct ahx_tune *)IExec->GetHead(rp_tunelist);
    while( at )
    {
      sprintf( rname, "HVL.rescuetab%ld", i++ );
      rp_save_hvl( rname, at );
      at = (struct ahx_tune *)IExec->GetSucc(&at->at_ln);
    }

    IDOS->SetCurrentDir( cdir );
    IDOS->UnLock( lock );
    IExec->ReleaseSemaphore( rp_list_ss );
    return FALSE;
  }
#else
  // Just reload the skin...
  SDL_FreeSurface(bitmaps[BM_LOGO].srf );       bitmaps[BM_LOGO].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TAB_AREA].srf );   bitmaps[BM_TAB_AREA].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TAB_LEFT].srf );   bitmaps[BM_TAB_LEFT].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TAB_MID] .srf) ;   bitmaps[BM_TAB_MID] .srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TAB_RIGHT].srf );  bitmaps[BM_TAB_RIGHT].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_ITAB_LEFT].srf );  bitmaps[BM_ITAB_LEFT].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_ITAB_MID].srf );   bitmaps[BM_ITAB_MID].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_ITAB_RIGHT].srf ); bitmaps[BM_ITAB_RIGHT].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_BUTBANKR].srf );   bitmaps[BM_BUTBANKR].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_BUTBANKP].srf );   bitmaps[BM_BUTBANKP].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_BG_TRACKER].srf ); bitmaps[BM_BG_TRACKER].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_BG_INSED].srf );   bitmaps[BM_BG_INSED].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_PLUSMINUS].srf );  bitmaps[BM_PLUSMINUS].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_VUMETER] .srf) ;   bitmaps[BM_VUMETER] .srf = NULL;
  SDL_FreeSurface(bitmaps[BM_DEPTH].srf ) ;     bitmaps[BM_DEPTH].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_BLANK].srf ) ;     bitmaps[BM_BLANK].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_WAVEMETERS].srf ); bitmaps[BM_WAVEMETERS].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_PRF_BG].srf );     bitmaps[BM_PRF_BG].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_PRF_CYCLE].srf );  bitmaps[BM_PRF_CYCLE].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TRKBANKR].srf );   bitmaps[BM_TRKBANKR].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_TRKBANKP].srf );   bitmaps[BM_TRKBANKP].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_CHANMUTE].srf );   bitmaps[BM_CHANMUTE].srf = NULL;
  SDL_FreeSurface(bitmaps[BM_DIRPOPUP].srf );   bitmaps[BM_DIRPOPUP].srf = NULL;

  if (bitmaps[BM_ITAB_TEXT].srf)
  {
    SDL_FreeSurface(bitmaps[BM_ITAB_TEXT].srf );
    bitmaps[BM_ITAB_TEXT].srf = NULL;
  }

  if (bitmaps[BM_TAB_TEXT].srf)
  {
    SDL_FreeSurface(bitmaps[BM_TAB_TEXT].srf );
    bitmaps[BM_TAB_TEXT].srf = NULL;
  }

  if( !gui_open_skin_images() )
  {
    printf( "Error loading skin. Reverting to SIDMonster-Light...\n" );
    strcpy( skindir, "Skins/SIDMonster-Light" );
    if( !gui_open_skin_images() ) return FALSE;
  }

  gui_render_everything();
#endif
  return TRUE; 
}

#ifdef __SDL_WRAPPER__
struct IntuiMessage *translate_sdl_event(void)
{
  static uint32 mkqual = 0;
  static struct IntuiMessage fakemsg;

  fakemsg.IAddress  = NULL;

  switch (event.type)
  {
    case SDL_MOUSEMOTION:
      fakemsg.Class     = IDCMP_MOUSEMOVE;
      fakemsg.Qualifier = mkqual;
      fakemsg.Code      = 0;
      fakemsg.MouseX    = event.motion.x;
      fakemsg.MouseY    = event.motion.y;
      break;
    
    case SDL_MOUSEBUTTONDOWN:
    {
      static struct IntuiWheelData iwd;

      if (event.button.button == SDL_BUTTON_WHEELUP)
      {
        fakemsg.Class    = IDCMP_EXTENDEDMOUSE;
        fakemsg.Code     = IMSGCODE_INTUIWHEELDATA;
        fakemsg.IAddress = &iwd;
        fakemsg.MouseX    = event.motion.x;
        fakemsg.MouseY    = event.motion.y;
        iwd.WheelY    = -1;
        break;
      }

      if (event.button.button == SDL_BUTTON_WHEELDOWN)
      {
        fakemsg.Class    = IDCMP_EXTENDEDMOUSE;
        fakemsg.Code     = IMSGCODE_INTUIWHEELDATA;
        fakemsg.IAddress = &iwd;
        fakemsg.MouseX   = event.motion.x;
        fakemsg.MouseY   = event.motion.y;
        iwd.WheelY    = 1;
        break;
      }

      if (event.button.button == SDL_BUTTON_RIGHT) mkqual |= IEQUALIFIER_RBUTTON;
      fakemsg.Class     = IDCMP_MOUSEBUTTONS;
      fakemsg.Code      = event.button.button;
      fakemsg.Qualifier = mkqual;
      fakemsg.MouseX    = event.motion.x;
      fakemsg.MouseY    = event.motion.y;
      break;
    }

    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_RIGHT) mkqual &= ~IEQUALIFIER_RBUTTON;
      fakemsg.Class     = IDCMP_MOUSEBUTTONS;
      fakemsg.Code      = event.button.button | 0x100;
      fakemsg.Qualifier = mkqual;
      fakemsg.MouseX    = event.motion.x;
      fakemsg.MouseY    = event.motion.y;
      break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym)
      {
        case SDLK_LSHIFT: mkqual |= IEQUALIFIER_LSHIFT; break;
        case SDLK_RSHIFT: mkqual |= IEQUALIFIER_RSHIFT; break;
        case SDLK_LCTRL:  mkqual |= IEQUALIFIER_CONTROL; break;
        case SDLK_RCTRL:  if (!pref_rctrlplaypos) { mkqual |= IEQUALIFIER_CONTROL; } break;
        case SDLK_LALT:   mkqual |= IEQUALIFIER_LALT; break;
        case SDLK_LSUPER: mkqual |= IEQUALIFIER_LCOMMAND; break;
#ifdef __WIN32__
        /* Workaround: On Windows, SDLK_RALT is always prefixed by SDLK_RCTRL */
        /* but we want RALT to be treated separately, and since RCTRL+RALT    */
        /* isn't a key combo we care about, we make pressing RALT cancel out  */
        /* the dummy RCTRL press. */
        case SDLK_RALT:   mkqual &= ~IEQUALIFIER_CONTROL; break;
#endif
      }

      fakemsg.Class     = IDCMP_RAWKEY;
      fakemsg.Code      = sdl_keysym_to_amiga_rawkey(event.key.keysym.sym);
      fakemsg.Qualifier = mkqual;
      break;        

    case SDL_KEYUP:
      switch (event.key.keysym.sym)
      {
        case SDLK_LSHIFT: mkqual &= ~IEQUALIFIER_LSHIFT; break;
        case SDLK_RSHIFT: mkqual &= ~IEQUALIFIER_RSHIFT; break;
        case SDLK_LCTRL:  mkqual &= ~IEQUALIFIER_CONTROL; break;
        case SDLK_RCTRL:  if (!pref_rctrlplaypos) { mkqual &= ~IEQUALIFIER_CONTROL; } break;
        case SDLK_LALT:   mkqual &= ~IEQUALIFIER_LALT; break;
        case SDLK_LSUPER: mkqual &= ~IEQUALIFIER_LCOMMAND; break;
      }

      fakemsg.Class     = IDCMP_RAWKEY;
      fakemsg.Code      = sdl_keysym_to_amiga_rawkey(event.key.keysym.sym) | 0x80;
      fakemsg.Qualifier = mkqual;
      break;        
  }

  return &fakemsg;
}
#endif

void gui_handler( uint32 gotsigs )
{
  struct IntuiMessage *msg;
  
  const int8 hexkeys[] = { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 53, 51, 34, 18, 35 };
  
  const int8 inskeys[] = { 15, 29, 30, 31, 45, 46, 47, 61, 62, 63 };
  
  const int8 pianokeys[] = { 49, 33, 50, 34, 51, 52, 36, 53, 37, 54, 38, 55,
                       16,  2, 17,  3, 18, 19,  5, 20,  6, 21,  7, 22,
                       23,  9, 24, 10, 25 };

  const int8 extrakeys[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                       56, 40, 57, 41, 58, -1, -1, -1, -1, -1, -1, -1,
                       -1, -1, -1, -1, -1 };

  struct ahx_step *stp, *pstp;

#ifndef __SDL_WRAPPER__
  if( gotsigs & gui_tick_sig )
#else
  if( event.type == SDL_USEREVENT )
#endif
  {
    if( curtune == rp_curtune )
    {
      gui_render_tunepanel( FALSE );
      gui_render_vumeters();
    }
    if( (wm_count++)&1 )
      gui_render_wavemeter();

#ifdef __SDL_WRAPPER__
    if (prefwin_open)
      bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
    return;
#endif
  }

#ifndef __SDL_WRAPPER__
  if( gotsigs & prefwin_sig )
#else
  if( prefwin_open )
#endif
  {
    BOOL wantclose;
    int16 x, y;

    wantclose = FALSE;
#ifndef __SDL_WRAPPER__
    while( ( msg = (struct IntuiMessage *)IExec->GetMsg( prefwin->UserPort ) ) )
#else
    msg = translate_sdl_event();
#endif
    {
      switch( msg->Class )
      {
        case IDCMP_RAWKEY:
          // In a textbox?
          if( ptbx != NULL )
          {
            gui_textbox_keypress( &prefbm, &ptbx, msg );
#ifdef __SDL_WRAPPER__
            bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
            break;
          }

          switch( msg->Code )
          {
            case 197:
            case 13: // ESC is intermittently broken on my laptop :-/
              wantclose = TRUE;
              break;
          }
          break;
        case IDCMP_MOUSEBUTTONS:
#ifndef __SDL_WRAPPER__         
          x = msg->MouseX-pw_bl;
          y = msg->MouseY-pw_bt;
#else
          x = msg->MouseX-200;
          y = msg->MouseY-150;
#endif
          
          switch( msg->Code )
          {
            case SELECTDOWN:
              if( gui_check_ptbox_press( x, y ) ) return;

              if( ptbx )
              {
                ptbx->flags &= ~TBF_ACTIVE;
                gui_render_tbox( &prefbm, ptbx );
                gui_ptbox_finish_editing();
#ifdef __SDL_WRAPPER__
                bm_to_bm(&prefbm, -4, -4, &mainbm, 196, 146, 408, 308);
#endif
              }

              if( gui_checkpc_down( x, y ) ) break;
              if( gui_checkpop_down( x, y ) ) break;
              break;
            case SELECTUP:
              if( ptbx != NULL ) break;

              if( gui_checkpc_up( x, y ) ) break;
              if( gui_checkpop_up( x, y ) ) break;
              break;
          }
          break;

#ifndef __SDL_WRAPPER__
        case IDCMP_CLOSEWINDOW:
          wantclose = TRUE;
#endif
          break;
      }
#ifndef __SDL_WRAPPER__
      IExec->ReplyMsg( (struct Message *)msg );
#endif
    }
    
    if( wantclose ) gui_close_prefs();
#ifdef __SDL_WRAPPER__
    return;
#endif
  }

#ifndef __SDL_WRAPPER__
  if( gotsigs & mainwin_sig )
#endif
  {
    int32 panel, maxpos;
    struct ahx_plsentry *cple;
    struct ahx_instrument *cins;
    BOOL cutting;
    int16 next;
#ifndef __SDL_WRAPPER__
    while( ( msg = (struct IntuiMessage *)IExec->GetMsg( mainwin->UserPort ) ) )
#else
    msg = translate_sdl_event();
#endif

    {
      BOOL donkey;
      int32 i, j, k, l, chan, track, tran;
      struct IntuiWheelData *iwd;

      switch( msg->Class )
      {
        case IDCMP_EXTENDEDMOUSE:
          switch( msg->Code )
          {
            case IMSGCODE_INTUIWHEELDATA:
              iwd = (struct IntuiWheelData *)msg->IAddress;              
              gui_wheelybin( msg->MouseX, msg->MouseY, iwd->WheelY );              
              break;
          }
          break;
        case IDCMP_RAWKEY:
          qual   = msg->Qualifier;
          donkey = FALSE;
          
          // In a textbox?
          if( etbx != NULL )
          {
            gui_textbox_keypress( &mainbm, &etbx, msg );
            break;
          }
          
          // Keys for any mode
          switch( qual&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT|IEQUALIFIER_CONTROL|IEQUALIFIER_LALT|IEQUALIFIER_LCOMMAND) )
          {
            case 0:
              // No qualifiers
              switch( msg->Code )
              {
                case 13: // \ = drum pad mode
                  if( curtune->at_drumpadmode )
                    curtune->at_drumpadmode = FALSE;
                  else
                    curtune->at_drumpadmode = TRUE;
                  gui_set_various_things( curtune );
                  donkey = TRUE;
                  break;
                case 48: // Blank next to Z (shut up)
                  rp_stop();
                  gui_render_wavemeter();
                  gui_render_tracked( TRUE );  // Kill the VUMeters
                  curtune->at_doing = D_IDLE;
                  donkey = TRUE;
                  break;
                
                case 11: // Minus
                  posedadvance = !posedadvance;
                  donkey = TRUE;
                  break;

                case 12: // Equals
                  if( !curtune ) break;
                  if( curtune->at_doing == D_PLAYING ) break;
                  
                  rp_play_row( curtune );
                  donkey = TRUE;
                  break;

                case 80: // F1
                case 81: // F2
                case 82: // F3
                case 83: // F4
                case 84: // F5
                  donkey = TRUE;
                  basekey = (msg->Code-80)*12;
                  break;
                case 197:
                  donkey = TRUE;
                  
                  if( !curtune ) break;
                  
                  if( curtune->at_curpanel == PN_INSED )
                    curtune->at_curpanel = PN_TRACKER;
                  else
                    curtune->at_curpanel = PN_INSED;
                  gui_render_tunepanel( TRUE );
                  break;
                case 103:
                  donkey = TRUE;
                  curtune->at_doing = D_IDLE;
                  if( rp_play_pos( curtune, FALSE ) )
                    curtune->at_doing = D_PLAYING;
                  break;
                case 101:
                  donkey = TRUE;
                  curtune->at_doing = D_IDLE;
                  if( rp_play_song( curtune, curtune->at_curss, FALSE ) )
                    curtune->at_doing = D_PLAYING;
                  break;
                case 94:
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  curtune->at_baseins += 10;
                  if( curtune->at_baseins > 60 ) curtune->at_baseins = 60;
                  break;
                case 74:
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  curtune->at_baseins -= 10;
                  if( curtune->at_baseins < 0 ) curtune->at_baseins = 0;
                  break;
              }
              break;

            case IEQUALIFIER_LSHIFT:  // Shifty
            case IEQUALIFIER_RSHIFT:  // Shifty
            case IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT: // Shifty shift

              chan = track = -1;
              if( curtune )
              {
                chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
              }
              
              cutting = FALSE;

              switch( msg->Code )
              {
                case 79:  // Shift+Left (pos up)
                  donkey = TRUE;
                  if( !curtune ) break;
                  if( curtune->at_PosNr == 0 ) break;
                  curtune->at_PosNr--;
                  gui_render_posed( TRUE );
                  gui_render_tracked( TRUE );
                  break;
                  
                case 78:  // Shift+Right (pos down)
                  donkey = TRUE;
                  if( !curtune ) break;
                  if( curtune->at_PosNr == 999 ) break;
                  curtune->at_PosNr++;
                  gui_render_posed( TRUE );
                  gui_render_tracked( TRUE );
                  break;

                case 82:  // Shift+F3 (cut track to buffer)
                  cutting = TRUE;
                case 83:  // Shift+F4 (copy track to buffer)
                  donkey = TRUE;
                  if( track == -1 ) break;
                  if( curtune->at_curpanel != PN_TRACKER ) break;
                  gui_copyregion( track, 0, curtune->at_TrackLength-1, CBF_NOTES|CBF_CMD1|CBF_CMD2, cutting );
                  gui_render_tracked( TRUE );      
                  break;
                case 84:  // Shift+F5 (paste)
                  donkey = TRUE;
                  if( track == -1 ) break;
                  if( curtune->at_curpanel != PN_TRACKER ) break;
                  setbefore_track( curtune, track );
                  gui_paste( curtune, track, CBF_NOTES|CBF_CMD1|CBF_CMD2 );
                  setafter_track( curtune, track );
                  gui_render_tracked( TRUE );
                  break;
                case 85:  // Shift+F6 (remember pos 0)
                case 86:  // Shift+F7 (remember pos 1)
                case 87:  // Shift+F8 (remember pos 2)
                case 88:  // Shift+F9 (remember pos 3)
                case 89:  // Shift+F10 (remember pos 4)
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  curtune->at_rempos[msg->Code-85] = curtune->at_NoteNr;
                  gui_render_tracked( TRUE );
                  break;
                case 65:  // Shift+Backspace (delete row above)
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  
                  switch( curtune->at_curpanel )
                  {
                    case PN_INSED:
                      if( curtune->at_idoing != D_EDITING ) break;
                      
                      cins = &curtune->at_Instruments[curtune->at_curins];
                      cple = &cins->ins_PList.pls_Entries[255];

                      setbefore_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      if( cins->ins_pcury == 0 ) break;
                      
                      cins->ins_pcury--;
                      for( i=cins->ins_pcury; i<254; i++ )
                        cins->ins_PList.pls_Entries[i] = cins->ins_PList.pls_Entries[i+1];
                      
                      cple->ple_Note       = 0;
                      cple->ple_Fixed      = 0;
                      cple->ple_FX[0]      = 0;
                      cple->ple_FXParam[0] = 0;
                      cple->ple_FX[1]      = 0;
                      cple->ple_FXParam[1] = 0;
                      
                      setafter_plist( curtune, &cins->ins_PList.pls_Entries[0] );
                      
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case PN_TRACKER:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                          track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                          
                          if( curtune->at_NoteNr == 0 ) break;
                          
                          setbefore_track( curtune, track );
                          
                          curtune->at_NoteNr--;
                          
                          for( i=curtune->at_NoteNr; i<(curtune->at_TrackLength-1); i++ )
                            curtune->at_Tracks[track][i] = curtune->at_Tracks[track][i+1];
                          
                          curtune->at_Tracks[track][i].stp_Note       = 0;
                          curtune->at_Tracks[track][i].stp_Instrument = 0;
                          curtune->at_Tracks[track][i].stp_FX         = 0;
                          curtune->at_Tracks[track][i].stp_FXParam    = 0;
                          curtune->at_Tracks[track][i].stp_FXb        = 0;
                          curtune->at_Tracks[track][i].stp_FXbParam   = 0;
                          
                          setafter_track( curtune, track );

                          gui_render_tracked( TRUE );
                          break;

                        case E_POS:
                          if( curtune->at_PosNr == 0 ) break;
                          
                          curtune->at_PosNr--;
                          
                          maxpos = gui_findlastpos( curtune )+1;
                          if( maxpos <= curtune->at_PosNr ) maxpos = curtune->at_PosNr+1;
                          
                          setbefore_posregion( curtune, 0, curtune->at_PosNr, MAX_CHANNELS, maxpos-curtune->at_PosNr );

                          for( i=curtune->at_PosNr; i<999; i++ )
                            curtune->at_Positions[i] = curtune->at_Positions[i+1];
                          
                          for( j=0; j<MAX_CHANNELS; j++ )
                          {
                            curtune->at_Positions[i].pos_Track[j]     = 0;
                            curtune->at_Positions[i].pos_Transpose[j] = 0;
                          }
                          
                          setafter_posregion( curtune, 0, curtune->at_PosNr, MAX_CHANNELS, maxpos-curtune->at_PosNr );

                          gui_render_posed( TRUE );
                          break;
                      }
                      break;
                  }
                  break;
                  
                case 68:  // Shift+Enter (insert row)
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  
                  switch( curtune->at_curpanel )
                  {
                    case PN_INSED:
                      if( curtune->at_idoing != D_EDITING ) break;
                      
                      cins = &curtune->at_Instruments[curtune->at_curins];
                      cple = &cins->ins_PList.pls_Entries[cins->ins_pcury];
                      
                      setbefore_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      for( i=255; i>cins->ins_pcury; i-- )
                        cins->ins_PList.pls_Entries[i] = cins->ins_PList.pls_Entries[i-1];
                      
                      cple->ple_Note       = 0;
                      cple->ple_Fixed      = 0;
                      cple->ple_FX[0]      = 0;
                      cple->ple_FXParam[0] = 0;
                      cple->ple_FX[1]      = 0;
                      cple->ple_FXParam[1] = 0;
                      
                      setafter_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      gui_render_perf( curtune, cins, TRUE );
                      break;
                    
                    case PN_TRACKER:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                          track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                          
                          setbefore_track( curtune, track );

                          for( i=(curtune->at_TrackLength-1); i>curtune->at_NoteNr; i-- )
                            curtune->at_Tracks[track][i] = curtune->at_Tracks[track][i-1];
                          
                          curtune->at_Tracks[track][i].stp_Note       = 0;
                          curtune->at_Tracks[track][i].stp_Instrument = 0;
                          curtune->at_Tracks[track][i].stp_FX         = 0;
                          curtune->at_Tracks[track][i].stp_FXParam    = 0;
                          curtune->at_Tracks[track][i].stp_FXb        = 0;
                          curtune->at_Tracks[track][i].stp_FXbParam   = 0;

                          setafter_track( curtune, track );
                          
                          gui_render_tracked( TRUE );
                          break;
                        
                        case E_POS:
                          maxpos = gui_findlastpos( curtune )+1;
                          if( maxpos <= curtune->at_PosNr ) maxpos = curtune->at_PosNr+1;

                          setbefore_posregion( curtune, 0, curtune->at_PosNr, MAX_CHANNELS, maxpos-curtune->at_PosNr );
                        
                          for( i=999; i>curtune->at_PosNr; i-- )
                            curtune->at_Positions[i] = curtune->at_Positions[i-1];
                          
                          for( j=0; j<MAX_CHANNELS; j++ )
                          {
                            curtune->at_Positions[i].pos_Track[j]     = 0;
                            curtune->at_Positions[i].pos_Transpose[j] = 0;
                          }
                          
                          setafter_posregion( curtune, 0, curtune->at_PosNr, MAX_CHANNELS, maxpos-curtune->at_PosNr );
                          gui_render_posed( TRUE );
                          break;
                      }
                      break;
                  }
                  break;
              }
              break;
            
            case IEQUALIFIER_CONTROL:
              chan = track = -1;
              if( curtune )
              {
                chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
              }
              
              cutting = FALSE;

              switch( msg->Code )
              {
                case 1:  // Ctrl+n (set notejump to n)
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                  donkey = TRUE;
                  if( !curtune ) break;
                  
                  if( curtune->at_curpanel == PN_INSED )
                    curtune->at_inotejump = msg->Code%10;
                  else
                    curtune->at_notejump = msg->Code%10;
                  break;
                
                case 32:  // Ctrl+A (toggle voice)
                  if( !curtune ) break;
                  
                  if( curtune->at_editing == E_POS )
                    chan = (curtune->at_posed_curs/5)+curtune->at_curlch;
                  else
                    chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                  
                  curtune->at_Voices[chan].vc_SetTrackOn ^= 1;
                  gui_render_tracked( TRUE );
                  break;
                
                case 16:  // Ctrl+Q (all tracks on)
                  if( !curtune ) break;
                  
                  for( i=0; i<MAX_CHANNELS; i++ )
                    curtune->at_Voices[i].vc_SetTrackOn = 1;
                    
                  gui_render_tracked( TRUE );
                  break;
                
                case 60:  // Ctrl+Keypad . (clear instrument)
                  // Have to use Ctrl because i don't like how easy this was
                  // to do by mistake in PT.
                  if( !curtune ) break;
                  cins = &curtune->at_Instruments[curtune->at_curins];
                  rp_clear_instrument( cins );
                case 79:  // Ctrl+Left (instrument up)
                  donkey = TRUE;
                  if( !curtune ) break;
                  if( curtune->at_curins == 0 ) break;

                  curtune->at_curins--;
                  gui_render_inslist( FALSE );
                  gui_set_various_things( curtune );
                  break;
                  
                case 78:  // Ctrl+Right (instrument down)
                  donkey = TRUE;
                  if( !curtune ) break;
                  if( curtune->at_curins == 63 ) break;

                  curtune->at_curins++;
                  gui_render_inslist( FALSE );
                  gui_set_various_things( curtune );
                  break;

                case 82:  // Ctrl+F3 (cut commands to buffer)
                  cutting = TRUE;
                case 83:  // Ctrl+F4 (copy commands to buffer)
                  donkey = TRUE;
                  if( track == -1 ) break;
                  if( curtune->at_curpanel != PN_TRACKER ) break;
                  gui_copyregion( track, 0, curtune->at_TrackLength-1, CBF_CMD1|CBF_CMD2, cutting );
                  gui_render_tracked( TRUE );      
                  break;
                case 84:  // Ctrl+F5 (paste commands)
                  donkey = TRUE;
                  if( track == -1 ) break;
                  if( curtune->at_curpanel != PN_TRACKER ) break;
                  setbefore_track( curtune, track );
                  gui_paste( curtune, track, CBF_CMD1|CBF_CMD2 );
                  setafter_track( curtune, track );
                  gui_render_tracked( TRUE );
                  break;
                case 65:  // Ctrl+Backspace (delete row above (commands only))
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  
                  switch( curtune->at_curpanel )
                  {
                    case PN_INSED:
                      if( curtune->at_idoing != D_EDITING ) break;

                      cins = &curtune->at_Instruments[curtune->at_curins];
                      cple = &cins->ins_PList.pls_Entries[255];
                      
                      if( cins->ins_pcury == 0 ) break;
                      
                      setbefore_plist( curtune, &cins->ins_PList.pls_Entries[0] );
                      
                      cins->ins_pcury--;
                      for( i=cins->ins_pcury; i<254; i++ )
                      {
                        cins->ins_PList.pls_Entries[i].ple_FX[0]      = cins->ins_PList.pls_Entries[i+1].ple_FX[0];
                        cins->ins_PList.pls_Entries[i].ple_FXParam[0] = cins->ins_PList.pls_Entries[i+1].ple_FXParam[0];
                        cins->ins_PList.pls_Entries[i].ple_FX[1]      = cins->ins_PList.pls_Entries[i+1].ple_FX[1];
                        cins->ins_PList.pls_Entries[i].ple_FXParam[1] = cins->ins_PList.pls_Entries[i+1].ple_FXParam[1];
                      }
                      
                      cple->ple_FX[0]      = 0;
                      cple->ple_FXParam[0] = 0;
                      cple->ple_FX[1]      = 0;
                      cple->ple_FXParam[1] = 0;
                      
                      setafter_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case PN_TRACKER:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                          track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                          
                          if( curtune->at_NoteNr == 0 ) break;
                          
                          setbefore_track( curtune, track );

                          curtune->at_NoteNr--;
                          
                          for( i=curtune->at_NoteNr; i<(curtune->at_TrackLength-1); i++ )
                          {
                            curtune->at_Tracks[track][i].stp_FX       = curtune->at_Tracks[track][i+1].stp_FX;
                            curtune->at_Tracks[track][i].stp_FXParam  = curtune->at_Tracks[track][i+1].stp_FXParam;
                            curtune->at_Tracks[track][i].stp_FXb      = curtune->at_Tracks[track][i+1].stp_FXb;
                            curtune->at_Tracks[track][i].stp_FXbParam = curtune->at_Tracks[track][i+1].stp_FXbParam;
                          }
                          
                          curtune->at_Tracks[track][i].stp_FX         = 0;
                          curtune->at_Tracks[track][i].stp_FXParam    = 0;
                          curtune->at_Tracks[track][i].stp_FXb        = 0;
                          curtune->at_Tracks[track][i].stp_FXbParam   = 0;
                          
                          setafter_track( curtune, track );

                          gui_render_tracked( TRUE );
                          break;
                      }
                      break;
                  }
                  break;
                  
                case 68:  // Ctrl+Enter (insert row (commands only))
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  
                  switch( curtune->at_curpanel )
                  {
                    case PN_INSED:
                      if( curtune->at_idoing != D_EDITING ) break;
                      
                      cins = &curtune->at_Instruments[curtune->at_curins];
                      cple = &cins->ins_PList.pls_Entries[cins->ins_pcury];

                      setbefore_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      for( i=255; i>cins->ins_pcury; i-- )
                      {
                        cins->ins_PList.pls_Entries[i].ple_FX[0]      = cins->ins_PList.pls_Entries[i-1].ple_FX[0];
                        cins->ins_PList.pls_Entries[i].ple_FXParam[0] = cins->ins_PList.pls_Entries[i-1].ple_FXParam[0];
                        cins->ins_PList.pls_Entries[i].ple_FX[1]      = cins->ins_PList.pls_Entries[i-1].ple_FX[1];
                        cins->ins_PList.pls_Entries[i].ple_FXParam[1] = cins->ins_PList.pls_Entries[i-1].ple_FXParam[1];
                      }

                      cple->ple_FX[0]      = 0;
                      cple->ple_FXParam[0] = 0;
                      cple->ple_FX[1]      = 0;
                      cple->ple_FXParam[1] = 0;
                      
                      setafter_plist( curtune, &cins->ins_PList.pls_Entries[0] );

                      gui_render_perf( curtune, cins, TRUE );
                      break;
                    
                    case PN_TRACKER:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;
                          track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                          
                          setbefore_track( curtune, track );

                          for( i=(curtune->at_TrackLength-1); i>curtune->at_NoteNr; i-- )
                          {
                            curtune->at_Tracks[track][i].stp_FX       = curtune->at_Tracks[track][i-1].stp_FX;
                            curtune->at_Tracks[track][i].stp_FXParam  = curtune->at_Tracks[track][i-1].stp_FXParam;
                            curtune->at_Tracks[track][i].stp_FXb      = curtune->at_Tracks[track][i-1].stp_FXb;
                            curtune->at_Tracks[track][i].stp_FXbParam = curtune->at_Tracks[track][i-1].stp_FXbParam;
                          }

                          curtune->at_Tracks[track][i].stp_FX         = 0;
                          curtune->at_Tracks[track][i].stp_FXParam    = 0;
                          curtune->at_Tracks[track][i].stp_FXb        = 0;
                          curtune->at_Tracks[track][i].stp_FXbParam   = 0;
                          
                          setafter_track( curtune, track );

                          gui_render_tracked( TRUE );
                          break;
                      }
                      break;
                  }
                  break;
              }
              break;
            
            case IEQUALIFIER_LALT:
              switch( msg->Code )
              {
                case 16:  // Alt+Q (mute all channels)
                  donkey = TRUE;
                  if( !curtune ) break;
                  
                  for( i=0; i<MAX_CHANNELS; i++ )
                    curtune->at_Voices[i].vc_SetTrackOn = 0;
                  gui_render_tracked( TRUE );
                  break;

                case 32:  // Alt+A (mute all but the current channel)
                  donkey = TRUE;

                  if( !curtune ) break;
                  
                  if( curtune->at_editing == E_POS )
                    chan = curtune->at_posed_curs/5;
                  else
                    chan = curtune->at_tracked_curs/9;
                  
                  for( i=0; i<MAX_CHANNELS; i++ )
                  {
                    if( i == chan )
                      curtune->at_Voices[i].vc_SetTrackOn = 1;
                    else
                      curtune->at_Voices[i].vc_SetTrackOn = 0;
                  }                    
                  gui_render_tracked( TRUE );
                  break;

                case 79:  // Alt+Left (decrement currently selected track number)
                  donkey = TRUE;
                  if( !curtune ) break;

                  chan = -1;
                  switch( curtune->at_editing )
                  {
                    case E_TRACK:
                      chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;    
                      break;
                    
                    case E_POS:
                      chan  = curtune->at_posed_curs/5 + curtune->at_curlch;
                      break;
                  }
                  
                  if( chan != -1 )
                  {
                    track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]-1;
                    if (track < 0) track = 0;
                    modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, track );

                    gui_render_posed( TRUE );
                    gui_render_tracked( TRUE );
                  }
                  break;
                  
                case 78:  // Alt+Right (increment currently selected track number)
                  donkey = TRUE;
                  if( !curtune ) break;

                  chan = -1;
                  switch( curtune->at_editing )
                  {
                    case E_TRACK:
                      chan = (curtune->at_tracked_curs/9)+curtune->at_curlch;    
                      break;
                    
                    case E_POS:
                      chan  = curtune->at_posed_curs/5 + curtune->at_curlch;
                      break;
                  }
                  
                  if( chan != -1 )
                  {
                    track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]+1;
                    if (track > 255) track = 255;
                    modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, track );

                    gui_render_posed( TRUE );
                    gui_render_tracked( TRUE );
                  }
                  break;

                case 85:  // Alt+F6 (play from pos 0)
                case 86:  // Alt+F7 (play from pos 1)
                case 87:  // Alt+F8 (play from pos 2)
                case 88:  // Alt+F9 (play from pos 3)
                case 89:  // Alt+F10 (play from pos 4)
                  donkey = TRUE;
                  if( curtune == NULL ) break;
                  
                  rp_stop();
                  curtune->at_NoteNr = curtune->at_rempos[msg->Code-85];
                  if( curtune->at_NoteNr >= curtune->at_TrackLength )
                    curtune->at_NoteNr = curtune->at_TrackLength-1;
                  if( curtune->at_curpanel == PN_TRACKER )
                    gui_render_tracked( TRUE );
                  
                  curtune->at_doing = D_IDLE;
                  if( rp_play_pos( curtune, TRUE ) )
                    curtune->at_doing = D_PLAYING;
                  break;
              }
              break;
            
            case IEQUALIFIER_LCOMMAND:
              break;
          }

          if( curtune != NULL )
          {
            for( i=0; i<10; i++ )
            {
              if( msg->Code == inskeys[i] )
              {
                donkey = TRUE;
                curtune->at_curins = curtune->at_baseins+i;
                if( curtune->at_curins > 63 ) curtune->at_curins = 63;
                gui_render_inslist( FALSE );
                gui_set_various_things( curtune );
                
                if( curtune->at_drumpadmode )
                {
                  chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                        
                  if( curtune->at_curins > 0 )
                    rp_play_note( curtune, curtune->at_curins, curtune->at_drumpadnote, chan );
                  if( curtune->at_doing == D_EDITING )
                  {
                    stp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];
                    modify_stp_w( curtune, stp, UNT_STP_NOTEANDINS, (curtune->at_drumpadnote<<8)|curtune->at_curins );
                    curtune->at_NoteNr += curtune->at_notejump;
                    curtune->at_modified = TRUE;
                    while( curtune->at_NoteNr >= curtune->at_TrackLength )
                      curtune->at_NoteNr -= curtune->at_TrackLength;
                    gui_render_tracked( TRUE );
                  }                
                }
                
                break;
              }
            }
          }
              
          if( donkey ) break;
              
          if( curtune == NULL )
            panel = PN_TRACKER;
          else
            panel = curtune->at_curpanel;
          
          switch( panel )
          {
            case PN_TRACKER:      
              switch( msg->Code )
              {
                case 64:
                  donkey = TRUE;
                  rp_stop();
                  gui_render_tracked( TRUE );  // Kill the VUMeters
                  gui_render_wavemeter();

                  if( curtune->at_doing == D_EDITING )
                    curtune->at_doing = D_IDLE;
                  else
                    curtune->at_doing = D_EDITING;
                  gui_render_tracker( TRUE );
                  break;
                case 68:
                  donkey = TRUE;
                  if( curtune == NULL ) break;

                  if( curtune->at_editing == E_POS )
                    curtune->at_editing = E_TRACK;
                  else
                    curtune->at_editing = E_POS;
                  gui_render_tracker( TRUE );
                  break;
                case 79:
                  donkey = TRUE;
                  if( curtune == NULL ) break;
              
                  switch( curtune->at_editing )
                  {
                    case E_POS:
                      curtune->at_posed_curs--;
                      if( curtune->at_curlch == 0 )
                      {
                        if( curtune->at_posed_curs < 0 )
                          curtune->at_posed_curs = curtune->at_Channels*5-1;

                        while( (curtune->at_posed_curs/5) > 5 )
                        {
                          curtune->at_posed_curs -= 5;
                          curtune->at_curlch++;
                        }
                      } else {
                        while( curtune->at_posed_curs < 0 )
                        {
                          curtune->at_curlch--;
                          curtune->at_posed_curs += 5;
                        }
                      }

                      gui_render_tracker( TRUE );
                      break;
                    case E_TRACK:
                      curtune->at_tracked_curs--;
                      if( curtune->at_curlch == 0 )
                      {
                        if( curtune->at_tracked_curs < 0 )
                          curtune->at_tracked_curs = curtune->at_Channels*9-1;

                        while( (curtune->at_tracked_curs/9) > 5 )
                        {
                          curtune->at_tracked_curs -= 9;
                          curtune->at_curlch++;
                        }
                      } else {
                        while( curtune->at_tracked_curs < 0 )
                        {
                          curtune->at_curlch--;
                          curtune->at_tracked_curs += 9;
                        }
                      }

                      gui_render_tracker( TRUE );
                      break;
                  }
                  break;
                case 78:
                  donkey = TRUE;
                  if( curtune == NULL ) break;
              
                  switch( curtune->at_editing )
                  {
                    case E_POS:
                      gui_posed_moveright();
                      gui_render_tracker( TRUE );
                      break;
                    case E_TRACK:
                      curtune->at_tracked_curs++;
                  
                      if( (curtune->at_tracked_curs/9+curtune->at_curlch) >= curtune->at_Channels )
                      {
                        curtune->at_tracked_curs = 0;
                        curtune->at_curlch = 0;
                      }
                  
                      while( (curtune->at_tracked_curs/9) > 5 )
                      {
                        curtune->at_curlch++;
                        curtune->at_tracked_curs -= 9;
                      }
                  
                      gui_render_tracker( TRUE );
                      break;
                  }
                  break;
                case 66:
                  donkey = TRUE;
                  if( curtune == NULL ) break;
  
                  switch( curtune->at_editing )
                  {
                    case E_POS:
                      i = 0;
                      switch( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_CONTROL) )
                      {
                        case IEQUALIFIER_CONTROL:
                          i = (curtune->at_posed_curs%5) > 2 ? 3 : 0;
                        case 0:
                          curtune->at_posed_curs = ((curtune->at_posed_curs+5)/5)*5;

                          if( (curtune->at_posed_curs/5)+curtune->at_curlch >= curtune->at_Channels )
                          {
                            curtune->at_posed_curs = 0;
                            curtune->at_curlch = 0;
                          }
                  
                          while( (curtune->at_posed_curs/5) > 5 )
                          {
                            curtune->at_posed_curs -= 5;
                            curtune->at_curlch++;
                          }
                          
                          curtune->at_posed_curs += i;
                          break;

                        case IEQUALIFIER_LSHIFT|IEQUALIFIER_CONTROL:
                          i = (curtune->at_posed_curs%5) > 2 ? 3 : 0;
                        case IEQUALIFIER_LSHIFT:
                          if( curtune->at_posed_curs < 5 )
                          {
                            if( curtune->at_curlch > 0 )
                            {
                              curtune->at_curlch--;
                              curtune->at_posed_curs = 0;
                            } else {
                              curtune->at_posed_curs = 5*5;
                              curtune->at_curlch = curtune->at_Channels-6;
                              if( curtune->at_curlch < 0 )
                              {
                                curtune->at_curlch = 0;
                                curtune->at_posed_curs = (curtune->at_Channels-1)*5;
                              }
                            }
                          } else {
                            curtune->at_posed_curs = ((curtune->at_posed_curs-5)/5)*5;
                          }
                          curtune->at_posed_curs += i;
                          break;
                      }
                    
                      gui_render_tracker( TRUE );
                      break;
                    case E_TRACK:
                      i = 0;
                      switch( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_CONTROL) )
                      {
                        case IEQUALIFIER_CONTROL:
                          j = curtune->at_tracked_curs%9;
                          if(( j > 0 ) && ( j < 3 )) i = 1;
                          if(( j > 2 ) && ( j < 6 )) i = 3;
                          if( j > 5 ) i = 6;
                        case 0:
                          curtune->at_tracked_curs = ((curtune->at_tracked_curs+9)/9)*9;

                          if( (curtune->at_tracked_curs/9)+curtune->at_curlch >= curtune->at_Channels )
                          {
                            curtune->at_tracked_curs = 0;
                            curtune->at_curlch = 0;
                          }
                  
                          while( (curtune->at_tracked_curs/9) > 5 )
                          {
                            curtune->at_tracked_curs -= 9;
                            curtune->at_curlch++;
                          }
                          curtune->at_tracked_curs += i;
                          break;
                        
                        case IEQUALIFIER_LSHIFT|IEQUALIFIER_CONTROL:
                          j = curtune->at_tracked_curs%9;
                          if(( j > 0 ) && ( j < 3 )) i = 1;
                          if(( j > 2 ) && ( j < 6 )) i = 3;
                          if( j > 5 ) i = 6;
                        case IEQUALIFIER_LSHIFT:
                          if( curtune->at_tracked_curs < 9 )
                          {
                            if( curtune->at_curlch > 0 )
                            {
                              curtune->at_curlch--;
                              curtune->at_tracked_curs = 0;
                            } else {
                              curtune->at_tracked_curs = 5*9;
                              curtune->at_curlch = curtune->at_Channels-6;
                              if( curtune->at_curlch < 0 )
                              {
                                curtune->at_curlch = 0;
                                curtune->at_tracked_curs = (curtune->at_Channels-1)*9;
                              }
                            }
                          } else {
                            curtune->at_tracked_curs = ((curtune->at_tracked_curs-9)/9)*9;
                          }
                          curtune->at_tracked_curs += i;
                          break;
                      }

                      gui_render_tracker( TRUE );
                      break;
                  }
                  break;
              }
          
              switch( curtune->at_doing )
              {
                case D_EDITING:
                case D_IDLE:
                  if( curtune == NULL )
                    break;
              
                  for( i=0; i<16; i++ )
                    if( msg->Code == hexkeys[i] )
                      break;
                
                  if( ( i < 16 ) && ( (qual&IEQUALIFIER_CONTROL) == 0 ) )
                  {
                    switch( curtune->at_editing )
                    {
                      case E_TRACK:
                        if( curtune->at_doing == D_IDLE ) break;
                        if( ( curtune->at_tracked_curs%9 ) == 0 ) break; 
                    
                        donkey = TRUE;
                    
                        chan = curtune->at_tracked_curs/9 + curtune->at_curlch;
                        stp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];

                        switch( curtune->at_tracked_curs%9 )
                        {
                          case 1:
                            if( i>9 ) break;
                            curtune->at_modified = TRUE;
                            j = (stp->stp_Instrument%10)+(i*10);
                            if( j > 63 ) j = 63;
                            modify_stp_b( curtune, stp, UNT_STP_INSTRUMENT, j );
                            stp->stp_Instrument = j;
                            break;
                          case 2:
                            if( i>9 ) break;
                            curtune->at_modified = TRUE;
                            j = (stp->stp_Instrument/10)*10+i;
                            if( j > 63 ) j = 63;
                            modify_stp_b( curtune, stp, UNT_STP_INSTRUMENT, j );
                            break;
                          case 3:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FX, i );
                            break;
                          case 4:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FXPARAM, (stp->stp_FXParam&0xf)|(i<<4) );
                            break;
                          case 5:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FXPARAM, (stp->stp_FXParam&0xf0)|i );
                            break;
                          case 6:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FXB, i );
                            break;
                          case 7:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FXBPARAM, (stp->stp_FXbParam&0xf)|(i<<4) );
                            break;
                          case 8:
                            curtune->at_modified = TRUE;
                            modify_stp_b( curtune, stp, UNT_STP_FXBPARAM, (stp->stp_FXbParam&0xf0)|i );
                            break;
                        }
                 
                        curtune->at_NoteNr += curtune->at_notejump;
                        if( curtune->at_NoteNr >= curtune->at_TrackLength )
                          curtune->at_NoteNr = 0;
                        gui_render_tracker( TRUE );
                        break;                    
                      case E_POS:
                        donkey = TRUE;
                        chan  = curtune->at_posed_curs/5 + curtune->at_curlch;
                        track = (curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff);
                        tran  = (curtune->at_Positions[curtune->at_PosNr].pos_Transpose[chan]&0xff);
                        switch( curtune->at_posed_curs%5 )
                        {
                          case 0:
                            if( i > 9 ) break;
                            curtune->at_modified = TRUE;
                            j = (track%100)+(i*100);
                            if( j > 255 ) j = 255;
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, j );
                            break;
                          case 1:
                            if( i > 9 ) break;
                            curtune->at_modified = TRUE;
                            j = ((track/100)*100)+(track%10)+(i*10);
                            if( j > 255 ) j = 255;
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, j );
                            break;
                          case 2:
                            if( i > 9 ) break;
                            curtune->at_modified = TRUE;
                            j = ((track/100)*100)+(((track%100)/10)*10)+i;
                            if( j > 255 ) j = 255;
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, j );
                            break;
                          case 3:
                            j = (tran & 0x0f)|(i<<4);
                            curtune->at_modified = TRUE;
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, j );
                            break;
                          case 4:
                            j = (tran & 0xf0)|i;
                            curtune->at_modified = TRUE;
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, j );
                            break;
                        }
                        if( posedadvance ) gui_posed_moveright();
                        gui_render_tracker( TRUE );
                        break;
                    }
                  }
              
                  if( donkey ) break;
              
                  if( ( curtune->at_editing == E_TRACK ) &&
                      ( (qual&IEQUALIFIER_CONTROL) == 0 ) &&
                      ( (curtune->at_tracked_curs%9) == 0 ) )
                  {
                    // Piano!
                    for( i=0; i<29; i++ )
                    {
                      if( ( msg->Code == pianokeys[i] ) ||
                          ( ( extrakeys[i] != -1 ) && ( msg->Code == extrakeys[i] ) ) )
                      {
                        donkey = TRUE;
                        chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                        
                        j = i+basekey+1;
                        if( j > 60 ) break;
                        
                        curtune->at_drumpadnote = j;
                        
                        if( curtune->at_curins > 0 )
                          rp_play_note( curtune, curtune->at_curins, i+basekey+1, chan );
                        if( curtune->at_doing == D_EDITING )
                        {
                          stp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];
                          modify_stp_w( curtune, stp, UNT_STP_NOTEANDINS, ((i+basekey+1)<<8)|curtune->at_curins );
                          curtune->at_NoteNr += curtune->at_notejump;
                          curtune->at_modified = TRUE;
                          while( curtune->at_NoteNr >= curtune->at_TrackLength )
                            curtune->at_NoteNr -= curtune->at_TrackLength;
                          gui_render_tracked( TRUE );
                        }
                        break;
                      }
                    }
                  }
      
                  if( donkey ) break;

                  if( qual&IEQUALIFIER_CONTROL )
                  {                
                    cutting = FALSE;
                    switch( msg->Code )
                    {
                      case 19:  // Ctrl+R (restore positions)
                        donkey = TRUE;
                        if( curtune ) gui_set_rempos( curtune );
                        gui_render_tracked( TRUE );
                        break;
                      case 21:  // Ctrl+Y (reverse block)
                        donkey = TRUE;
                        if( !curtune ) break;
                      
                        if( curtune->at_cbmarktrack != -1 )
                        {
                          struct ahx_step tmpstp;

                          track = curtune->at_cbmarktrack;
                          
                          setbefore_track( curtune, track );
                          
                          j = ((curtune->at_cbmarkendnote-curtune->at_cbmarkstartnote)+1)>>1;
                          k = curtune->at_cbmarkstartnote;
                          l = curtune->at_cbmarkendnote;
                          for( i=0; i<j; i++ )
                          {
                            if( curtune->at_cbmarkbits & CBF_NOTES )
                            {
                              tmpstp.stp_Note       = curtune->at_Tracks[track][k].stp_Note;
                              tmpstp.stp_Instrument = curtune->at_Tracks[track][k].stp_Instrument;
                              curtune->at_Tracks[track][k].stp_Note       = curtune->at_Tracks[track][l].stp_Note;
                              curtune->at_Tracks[track][k].stp_Instrument = curtune->at_Tracks[track][l].stp_Instrument;
                              curtune->at_Tracks[track][l].stp_Note       = tmpstp.stp_Note;
                              curtune->at_Tracks[track][l].stp_Instrument = tmpstp.stp_Instrument;
                            }
                            if( curtune->at_cbmarkbits & CBF_CMD1 )
                            {
                              tmpstp.stp_FX      = curtune->at_Tracks[track][k].stp_FX;
                              tmpstp.stp_FXParam = curtune->at_Tracks[track][k].stp_FXParam;
                              curtune->at_Tracks[track][k].stp_FX         = curtune->at_Tracks[track][l].stp_FX;
                              curtune->at_Tracks[track][k].stp_FXParam    = curtune->at_Tracks[track][l].stp_FXParam;
                              curtune->at_Tracks[track][l].stp_FX         = tmpstp.stp_FX;
                              curtune->at_Tracks[track][l].stp_FXParam    = tmpstp.stp_FXParam;
                            }
                            if( curtune->at_cbmarkbits & CBF_CMD2 )
                            {
                              tmpstp.stp_FXb        = curtune->at_Tracks[track][k].stp_FXb;
                              tmpstp.stp_FXbParam   = curtune->at_Tracks[track][k].stp_FXbParam;
                              curtune->at_Tracks[track][k].stp_FXb        = curtune->at_Tracks[track][l].stp_FXb;
                              curtune->at_Tracks[track][k].stp_FXbParam   = curtune->at_Tracks[track][l].stp_FXbParam;
                              curtune->at_Tracks[track][l].stp_FXb        = tmpstp.stp_FXb;
                              curtune->at_Tracks[track][l].stp_FXbParam   = tmpstp.stp_FXbParam;
                            }
                            k++;
                            l--;
                          }

                          setafter_track( curtune, track );

                        } else {
                          struct ahx_step tmpstp;
                        
                          chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                          track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                          
                          setbefore_track( curtune, track );

                          j = curtune->at_TrackLength>>1;
                          k = curtune->at_TrackLength-1;
                          for( i=0; i<j; i++ )
                          {
                            tmpstp = curtune->at_Tracks[track][i];
                            curtune->at_Tracks[track][i]   = curtune->at_Tracks[track][k-i];
                            curtune->at_Tracks[track][k-i] = tmpstp;
                          }

                          setafter_track( curtune, track );
                        }

                        gui_render_tracked( TRUE );
                        break;

                      case 24:  // Ctrl+O (squish)
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        if( curtune->at_editing != E_TRACK ) break;
                        if( cb_length == 0 ) break;
              
                        chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                        track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                  
                        if( i == curtune->at_TrackLength-1 ) break;
                        
                        setbefore_track( curtune, track );
                    
                        for( i=curtune->at_NoteNr+1; i<curtune->at_TrackLength; i++ )
                        {
                          for( j=i; j<(curtune->at_TrackLength-1); j++ )
                            curtune->at_Tracks[track][j] = curtune->at_Tracks[track][j+1];
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_Note       = 0;
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_Instrument = 0;
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_FX         = 0;
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_FXParam    = 0;
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_FXb        = 0;
                          curtune->at_Tracks[track][curtune->at_TrackLength-1].stp_FXbParam   = 0;
                        }
                      
                        setafter_track( curtune, track );

                        gui_render_tracked( TRUE );
                        break;

                      case 23:  // Ctrl+I (Insert block)
                        donkey = TRUE;
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        if( curtune->at_editing != E_TRACK ) break;
                        if( cb_length == 0 ) break;
        
                        chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                        track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                  
                        setbefore_track( curtune, track );

                        for( i=curtune->at_TrackLength-1; i>=curtune->at_NoteNr+cb_length; i-- )
                        {
                          if( cb_contains & CBF_NOTES )
                          {
                            curtune->at_Tracks[track][i].stp_Note       = curtune->at_Tracks[track][i-cb_length].stp_Note;
                            curtune->at_Tracks[track][i].stp_Instrument = curtune->at_Tracks[track][i-cb_length].stp_Instrument;
                          }
                          if( cb_contains & CBF_CMD1 )
                          {
                            curtune->at_Tracks[track][i].stp_FX         = curtune->at_Tracks[track][i-cb_length].stp_FX;
                            curtune->at_Tracks[track][i].stp_FXParam    = curtune->at_Tracks[track][i-cb_length].stp_FXParam;
                          }
                          if( cb_contains & CBF_CMD2 )
                          {
                            curtune->at_Tracks[track][i].stp_FXb        = curtune->at_Tracks[track][i-cb_length].stp_FXb;
                            curtune->at_Tracks[track][i].stp_FXbParam   = curtune->at_Tracks[track][i-cb_length].stp_FXbParam;
                          }
                        }

                        gui_paste( curtune, track, CBF_NOTES|CBF_CMD1|CBF_CMD2 );
                    
                        setafter_track( curtune, track );

                        gui_render_tracked( TRUE );
                        break;          
                    
                      case 34:  // Ctrl+D (Delete block)
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        if( curtune->at_editing != E_TRACK ) break;
                        if( curtune->at_cbmarktrack == -1 ) break;
                      
                        donkey = TRUE;
                      
                        setbefore_track( curtune, curtune->at_cbmarktrack );

                        j = (curtune->at_cbmarkendnote - curtune->at_cbmarkstartnote) + 1;
                      
                        for( i=curtune->at_cbmarkstartnote; i<curtune->at_TrackLength-j; i++ )
                        {
                          if( curtune->at_cbmarkbits & CBF_NOTES )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note       = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_Note;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Instrument = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_Instrument;
                          }
                          if( curtune->at_cbmarkbits & CBF_CMD1 )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FX         = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_FX;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXParam    = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_FXParam;
                          }
                          if( curtune->at_cbmarkbits & CBF_CMD2 )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXb        = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_FXb;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXbParam   = curtune->at_Tracks[curtune->at_cbmarktrack][i+j].stp_FXbParam;
                          }
                        }
                      
                        for( ; i<curtune->at_TrackLength; i++ )
                        {
                          if( curtune->at_cbmarkbits & CBF_NOTES )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note       = 0;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Instrument = 0;
                          }
                          if( curtune->at_cbmarkbits & CBF_CMD1 )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FX         = 0;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXParam    = 0;
                          }
                          if( curtune->at_cbmarkbits & CBF_CMD2 )
                          {
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXb        = 0;
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_FXbParam   = 0;
                          }
                        }
          
                        setafter_track( curtune, curtune->at_cbmarktrack );

                        curtune->at_cbmarktrack = -1;
                    
                        gui_render_tracked( TRUE );
                        break;
                    
                      case 37:  // Ctrl+H (transpose block up)
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        if( curtune->at_editing != E_TRACK ) break;
                        if( curtune->at_cbmarktrack == -1 ) break;
                        if( ( curtune->at_cbmarkbits & CBF_NOTES ) == 0 ) break;
        
                        donkey = TRUE;
                    
                        setbefore_track( curtune, curtune->at_cbmarktrack );

                        for( i=curtune->at_cbmarkstartnote; i<=curtune->at_cbmarkendnote; i++ )
                          if( ( curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note < 60 ) &&
                              ( curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note > 0 ) )
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note++;
              
                        setafter_track( curtune, curtune->at_cbmarktrack );

                        gui_render_tracked( TRUE );
                        break;

                     case 40:  // Ctrl+L (transpose block down)
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        if( curtune->at_editing != E_TRACK ) break;
                        if( curtune->at_cbmarktrack == -1 ) break;
                        if( ( curtune->at_cbmarkbits & CBF_NOTES ) == 0 ) break;
                      
                        donkey = TRUE;
                      
                        setbefore_track( curtune, curtune->at_cbmarktrack );

                        for( i=curtune->at_cbmarkstartnote; i<=curtune->at_cbmarkendnote; i++ )
                          if( curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note > 1 )
                            curtune->at_Tracks[curtune->at_cbmarktrack][i].stp_Note--;
                
                        setafter_track( curtune, curtune->at_cbmarktrack );

                        gui_render_tracked( TRUE );
                        break;
                    
                      case 39:  // Ctrl+K (kill to end/start of track)
                        chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                        track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];

                        if( qual & IEQUALIFIER_LSHIFT )
                        {
                          setbefore_track( curtune, track );
                          for( i=0; i<=curtune->at_NoteNr; i++ )
                          {
                            curtune->at_Tracks[track][i].stp_Note       = 0;
                            curtune->at_Tracks[track][i].stp_Instrument = 0;
                            curtune->at_Tracks[track][i].stp_FX         = 0;
                            curtune->at_Tracks[track][i].stp_FXParam    = 0;
                            curtune->at_Tracks[track][i].stp_FXb        = 0;
                            curtune->at_Tracks[track][i].stp_FXbParam   = 0;
                          }
                          setafter_track( curtune, track );
                          gui_render_tracked( TRUE );
                          break;
                        }

                        setbefore_track( curtune, track );
                        for( i=curtune->at_NoteNr; i<curtune->at_TrackLength; i++ )
                        {
                          curtune->at_Tracks[track][i].stp_Note       = 0;
                          curtune->at_Tracks[track][i].stp_Instrument = 0;
                          curtune->at_Tracks[track][i].stp_FX         = 0;
                          curtune->at_Tracks[track][i].stp_FXParam    = 0;
                          curtune->at_Tracks[track][i].stp_FXb        = 0;
                          curtune->at_Tracks[track][i].stp_FXbParam   = 0;
                        }
                        setafter_track( curtune, track );
                        gui_render_tracked( TRUE );
                        break;

                      case 22:  // Ctrl+U (Undo)
                        rp_stop();
                        gui_render_wavemeter();
                        curtune->at_doing = D_IDLE;
                        if( qual & IEQUALIFIER_LSHIFT )
                          redo( curtune );
                        else
                          undo( curtune );
                        break;

                      case 50:  // Cut
                        cutting = TRUE;
                      case 51:  // Copy
                        donkey = TRUE;
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        
                        switch( curtune->at_editing )
                        {
                          case E_TRACK:
                            if( curtune->at_cbmarktrack == -1 ) break;
        
                            if( cutting ) setbefore_track( curtune, curtune->at_cbmarktrack );
                            gui_copyregion( curtune->at_cbmarktrack, curtune->at_cbmarkstartnote, curtune->at_cbmarkendnote, curtune->at_cbmarkbits, cutting );
                            if( cutting ) setafter_track( curtune, curtune->at_cbmarktrack );
                            curtune->at_cbmarktrack = -1;
                  
                            gui_render_tracked( TRUE );
                            break;
                          
                          case E_POS:
                            if( curtune->at_cbpmarklft == -1 ) break;
                            
                            gui_copyposregion( curtune, cutting );
                            curtune->at_cbpmarkmarklft = -1;
                            curtune->at_cbpmarklft = -1;
                            gui_render_posed( TRUE );
                            if( cutting ) gui_render_tracked( TRUE );
                            break;
                        }
                        break;
                    
                      case 25:  // Paste
                      case 52:  // V as well as P
                        donkey = TRUE;
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        
                        switch( curtune->at_editing )
                        {
                          case E_TRACK:
                            chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                            track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                            setbefore_track( curtune, track );
                            gui_paste( curtune, track, CBF_NOTES|CBF_CMD1|CBF_CMD2 );
                            setafter_track( curtune, track );
                            gui_render_tracked( TRUE );
                            break;
                          
                          case E_POS:
                            gui_pasteposregion( curtune );
                            gui_render_posed( TRUE );
                            gui_render_tracked( TRUE );
                            break;
                        }
                        break;
                      
                      case 53:  // Ctrl+B Mark
                        donkey = TRUE;
                        if( ( curtune->at_doing != D_EDITING ) && ( curtune->at_doing != D_IDLE ) ) break;
                        
                        switch( curtune->at_editing )
                        {
                          case E_TRACK:
                            if( curtune->at_cbmarktrack != -1 )
                            {
                              curtune->at_cbmarktrack = -1;
                              gui_render_tracked( TRUE );
                              break;
                            }

                            chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                            track = curtune->at_Positions[curtune->at_PosNr].pos_Track[chan];
                  
                            curtune->at_cbmarktrack     = track;
                            curtune->at_cbmarkmarknote  = curtune->at_NoteNr;
                            curtune->at_cbmarkcurnote   = curtune->at_NoteNr;
                            curtune->at_cbmarkmarkx     = curtune->at_tracked_curs%9;
                            curtune->at_cbmarkcurx      = curtune->at_tracked_curs%9;
                      
                            gui_render_tracked( TRUE );
                            break;
                          
                          case E_POS:
                            if( curtune->at_cbpmarkmarklft != -1 )
                            {
                              curtune->at_cbpmarkmarklft = -1;
                              gui_render_posed( TRUE );
                              break;
                            }
                          
                            curtune->at_cbpmarkmarkpos = curtune->at_PosNr;
                            curtune->at_cbpmarkmarklft = (((curtune->at_posed_curs/5)+curtune->at_curlch)<<1)|((curtune->at_posed_curs%5)>2?1:0);
                            gui_render_posed( TRUE );
                            break;
                        }
                        break;
                    }
                  }
                  
                  if( donkey ) break;
                  
                  switch( msg->Code )
                  {    
                    case 11:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                        
                          if( (curtune->at_tracked_curs%9) > 2 )
                          {
                            donkey = TRUE;                  
                            chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                            stp  = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];
                            if( curtune->at_NoteNr > 0 )
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr-1];
                            else
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_TrackLength-1];
                            
                            if( (curtune->at_tracked_curs%9) < 6 )
                            {
                              modify_stp_w( curtune, stp, UNT_STP_FXANDPARAM, (pstp->stp_FX<<8)|((pstp->stp_FXParam-1)&0xff) );
                            } else {
                              modify_stp_w( curtune, stp, UNT_STP_FXBANDPARAM, (pstp->stp_FXb<<8)|((pstp->stp_FXbParam-1)&0xff) );
                            }
                          
                            curtune->at_NoteNr += curtune->at_notejump;
                            curtune->at_modified = TRUE;
                            if( curtune->at_NoteNr >= curtune->at_TrackLength )
                              curtune->at_NoteNr = 0;
                            gui_render_tracker( FALSE );
                          }
                          break;  
                        case E_POS:
                          if( curtune->at_PosNr == 0 ) break;
                          
                          chan = curtune->at_posed_curs/5 + curtune->at_curlch;
                    
                          donkey = TRUE;
                          
                          switch( curtune->at_posed_curs%5 )
                          {
                            case 0:
                            case 1:
                            case 2:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, (curtune->at_Positions[curtune->at_PosNr-1].pos_Track[chan]-1)&0xff );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            default:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, (curtune->at_Positions[curtune->at_PosNr-1].pos_Transpose[chan]-1)&0xff );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            
                          }
                          break;
                      }
                      break;
                          
                    case 12:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          if( (curtune->at_tracked_curs%9) > 2 )
                          {
                            donkey = TRUE;                  
                            chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                            stp  = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];
                            if( curtune->at_NoteNr > 0 )
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr-1];
                            else
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_TrackLength-1];
                            
                            if( (curtune->at_tracked_curs%9) < 6 )
                            {
                              modify_stp_w( curtune, stp, UNT_STP_FXANDPARAM, (pstp->stp_FX<<8)|((pstp->stp_FXParam+1)&0xff) );
                            } else {
                              modify_stp_w( curtune, stp, UNT_STP_FXBANDPARAM, (pstp->stp_FXb<<8)|((pstp->stp_FXbParam+1)&0xff) );
                            }
                            
                            curtune->at_NoteNr += curtune->at_notejump;
                            curtune->at_modified = TRUE;
                            if( curtune->at_NoteNr >= curtune->at_TrackLength )
                              curtune->at_NoteNr = 0;
                            gui_render_tracker( FALSE );
                          }
                          break;  
                        case E_POS:
                          if( curtune->at_PosNr == 0 ) break;
                          
                          chan = curtune->at_posed_curs/5 + curtune->at_curlch;
                    
                          donkey = TRUE;
                          
                          switch( curtune->at_posed_curs%5 )
                          {
                            case 0:
                            case 1:
                            case 2:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, (curtune->at_Positions[curtune->at_PosNr-1].pos_Track[chan]+1)&0xff );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            default:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, (curtune->at_Positions[curtune->at_PosNr-1].pos_Transpose[chan]+1)&0xff );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            
                          }
                          break;
                      }
                      break;

                    case 13:
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing != D_EDITING ) break;
                      
                          if( (curtune->at_tracked_curs%9) > 2 )
                          {
                            donkey = TRUE;                  
                            chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                            stp  = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr];
                            if( curtune->at_NoteNr > 0 )
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_NoteNr-1];
                            else
                              pstp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[chan]&0xff][curtune->at_TrackLength-1];
                            
                            if( (curtune->at_tracked_curs%9) < 6 )
                            {
                              modify_stp_w( curtune, stp, UNT_STP_FXANDPARAM, (pstp->stp_FX<<8)|pstp->stp_FXParam );
                            } else {
                              modify_stp_w( curtune, stp, UNT_STP_FXBANDPARAM, (pstp->stp_FXb<<8)|pstp->stp_FXbParam );
                            }
                            
                            curtune->at_NoteNr += curtune->at_notejump;
                            curtune->at_modified = TRUE;
                            if( curtune->at_NoteNr >= curtune->at_TrackLength )
                              curtune->at_NoteNr = 0;
                            gui_render_tracker( FALSE );
                          }
                          break;  
                        case E_POS:
                          if( curtune->at_PosNr == 0 ) break;
                          
                          chan = curtune->at_posed_curs/5 + curtune->at_curlch;
                    
                          donkey = TRUE;
                          
                          switch( curtune->at_posed_curs%5 )
                          {
                            case 0:
                            case 1:
                            case 2:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, curtune->at_Positions[curtune->at_PosNr-1].pos_Track[chan] );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            default:
                              modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, curtune->at_Positions[curtune->at_PosNr-1].pos_Transpose[chan] );
                              curtune->at_PosNr++;
                              if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                              gui_render_posed( FALSE );
                              break;
                            
                          }
                          break;
                      }
                      break;

                          
                    case 70:
                      donkey = TRUE;
                      
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_doing == D_IDLE ) break;

                          stp = &curtune->at_Tracks[curtune->at_Positions[curtune->at_PosNr].pos_Track[curtune->at_tracked_curs/9+curtune->at_curlch]&0xff][curtune->at_NoteNr];
                          
                          switch( qual&(IEQUALIFIER_LSHIFT|IEQUALIFIER_LALT) )
                          {
                            case IEQUALIFIER_LSHIFT:
                              modify_stp_b( curtune, stp, UNT_STP_NOTE, 0 );
                              break;
                            case IEQUALIFIER_LALT:
                              modify_stp_b( curtune, stp, UNT_STP_INSTRUMENT, 0 );
                              break;
                            default:
                              modify_stp_w( curtune, stp, UNT_STP_NOTEANDINS, 0 );
                          }
                          curtune->at_NoteNr += curtune->at_notejump;
                          curtune->at_modified = TRUE;
                          if( curtune->at_NoteNr >= curtune->at_TrackLength )
                            curtune->at_NoteNr = 0;
                          gui_render_tracker( FALSE );
                          break;
                        case E_POS:
                          chan = curtune->at_posed_curs/5 + curtune->at_curlch;
                          
                          if( ( curtune->at_posed_curs%5 ) < 3 )
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRACK, 0 );
                          else
                            modify_pos_b( curtune, &curtune->at_Positions[curtune->at_PosNr], chan, UNT_POS_TRANS, 0 );

                          curtune->at_PosNr++;
                          if( curtune->at_PosNr == 1000 ) curtune->at_PosNr = 999;
                          gui_render_posed( FALSE );
                          break;
                      }
                      break;

                    case 76:
                      donkey = TRUE;
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_NoteNr > 0 )
                            curtune->at_NoteNr--;
                          else
                            curtune->at_NoteNr = curtune->at_TrackLength-1;
                          gui_render_tracked( FALSE );
                          break;
                        case E_POS:
                          if( curtune->at_PosNr > 0 )
                            curtune->at_PosNr--;
                          else
                            curtune->at_PosNr = 999;
                          gui_render_tracker( FALSE );
                          break;
                      }
                      break;
                    case 77:
                      donkey = TRUE;
                      switch( curtune->at_editing )
                      {
                        case E_TRACK:
                          if( curtune->at_NoteNr < curtune->at_TrackLength-1 )
                            curtune->at_NoteNr++;
                          else
                            curtune->at_NoteNr = 0;
                          gui_render_tracked( FALSE );
                          break;
                        case E_POS:
                          if( curtune->at_PosNr < 999 )
                            curtune->at_PosNr++;
                          else
                            curtune->at_PosNr = 0;
                          gui_render_tracker( FALSE );
                          break;
                      }
                      break;
                    case 85:
                    case 86:
                    case 87:
                    case 88:
                    case 89:
                      donkey = TRUE;
                      curtune->at_NoteNr = curtune->at_rempos[msg->Code-85];
                      if( curtune->at_NoteNr >= curtune->at_TrackLength )
                        curtune->at_NoteNr = curtune->at_TrackLength-1;
                      gui_render_tracker( FALSE );
                      break;
                  }
                  break;
                case D_PLAYING:
                  switch( msg->Code )
                  {
                    case 76:
                      if( curtune->at_editing != E_POS ) break;
                      if( curtune->at_NextPosNr == -1 )
                        next = curtune->at_PosNr-1;
                      else
                        next = curtune->at_NextPosNr-1;
                           
                      if( next < 0 ) next = 999;
                      curtune->at_NextPosNr = next;
                      gui_render_tracker( FALSE );
                      break;

                    case 77:
                      if( curtune->at_editing != E_POS ) break;
                      if( curtune->at_NextPosNr == -1 )
                        next = curtune->at_PosNr+1;
                      else
                        next = curtune->at_NextPosNr+1;
                           
                      if( next > 999 ) next = 0;
                      curtune->at_NextPosNr = next;
                      gui_render_tracker( FALSE );
                      break;
                  }
                  break;
                case D_RECORDING:
                  break;
              }
              break;
            case PN_INSED:
              if( curtune == NULL ) break;

              cins = &curtune->at_Instruments[curtune->at_curins];
              cple = &cins->ins_PList.pls_Entries[cins->ins_pcury];

              if( ( curtune->at_idoing == D_IDLE ) ||
                  ( ( curtune->at_idoing == D_EDITING ) && ( cins != NULL ) && ( cins->ins_pcurx == 0 ) ) )
              {
                for( i=0; i<29; i++ )
                {
                  if( ( msg->Code == pianokeys[i] ) ||
                      ( ( extrakeys[i] != -1 ) && ( msg->Code == extrakeys[i] ) ) )
                  {
                    donkey = TRUE;
                    if( curtune->at_idoing == D_IDLE )
                    {
                      chan = curtune->at_tracked_curs/9+curtune->at_curlch;
                      if( curtune->at_curins > 0 )
                        rp_play_note( curtune, curtune->at_curins, i+basekey+1, chan );
                    } else {
                      modify_ple_b( curtune, cins, cple, UNT_PLE_NOTE, i+basekey+1 );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                    }
                    break;
                  }
                }
              }
              
              if( ( curtune->at_idoing == D_EDITING ) &&
                  ( cple != NULL ) &&
                  ( cins->ins_pcurx > 0 ) )
              {
                for( i=0; i<16; i++ )
                  if( msg->Code == hexkeys[i] )
                    break;
                
                if( i<16 )
                {
                  switch( cins->ins_pcurx )
                  {
                    case 1:
                      if( i < 5 )
                      {
                        donkey = TRUE;
                        modify_ple_b( curtune, cins, cple, UNT_PLE_WAVEFORM, i );
                        cins->ins_pcury += curtune->at_inotejump;
                        gui_render_perf( curtune, cins, TRUE );
                      }
                      break;

                    case 2:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FX0, i );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case 3:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FXPARAM0, (cple->ple_FXParam[0]&0xf)|(i<<4) );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case 4:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FXPARAM0, (cple->ple_FXParam[0]&0xf0)|i );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case 5:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FX1, i );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case 6:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FXPARAM1, (cple->ple_FXParam[1]&0xf)|(i<<4) );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;

                    case 7:
                      donkey = TRUE;
                      modify_ple_b( curtune, cins, cple, UNT_PLE_FXPARAM1, (cple->ple_FXParam[1]&0xf0)|i );
                      cins->ins_pcury += curtune->at_inotejump;
                      gui_render_perf( curtune, cins, TRUE );
                      break;
                  }
                }
              }
              
              if( donkey ) break;
              
              switch( msg->Code )
              {
                case 70:
                  donkey = TRUE;
                  if( cple )
                  {
                    modify_ple_b( curtune, cins, cple, UNT_PLE_NOTE, 0 );
                    cins->ins_pcury += curtune->at_inotejump;
                    gui_render_perf( curtune, cins, FALSE );
                  }
                  break;
                case 68:
                  donkey = TRUE;
                  if( curtune->at_idoing != D_EDITING ) break;

                  if( cple )
                  {
                    modify_ple_w( curtune, cins, cple, UNT_PLE_FIXED, cple->ple_Fixed ^ 1 );
                    gui_render_perf( curtune, cins, TRUE );
                  }
                  break;
              
                case 64:
                  donkey = TRUE;
                  curtune->at_doing = D_IDLE;
                  rp_stop();
                  gui_render_wavemeter();
                  
                  if( curtune->at_idoing == D_IDLE )
                    curtune->at_idoing = D_EDITING;
                  else
                    curtune->at_idoing = D_IDLE;
                  
                  gui_render_perf( curtune, cins, TRUE );
                  break;
                
                case 76:
                  donkey = TRUE;
                  if( cins )
                  {
                    if( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
                      cins->ins_pcury-=(PERF_H>>4);
                    else
                      cins->ins_pcury--;

                    gui_render_perf( curtune, cins, FALSE );
                  }
                  break;
           
                case 77:
                  donkey = TRUE;
                  if( cins )
                  {
                    if( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
                      cins->ins_pcury+=(PERF_H>>4);
                    else
                      cins->ins_pcury++;
                    gui_render_perf( curtune, cins, FALSE );
                  }
                  break;
                
                case 79:
                  donkey = TRUE;
                  if( cins )
                  {
                    if( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
                      cins->ins_pcurx = 0;
                    else
                      cins->ins_pcurx--;
                    gui_render_perf( curtune, cins, FALSE );
                  }
                  break;

                case 78:
                  donkey = TRUE;
                  if( cins )
                  {
                    if( qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) )
                      cins->ins_pcurx = 7;
                    else
                      cins->ins_pcurx++;
                    gui_render_perf( curtune, cins, FALSE );
                  }
                  break;
              }
              
              break;
          }
/*
          if( donkey == FALSE )
          {
            if( msg->Code < 128 )
              printf( "Key: %d (%d)\n", msg->Code, msg->Code|128 );
          } */
          break;
        case IDCMP_MOUSEBUTTONS:
          qual = msg->Qualifier;
          gui_mouse_handler( msg->MouseX, msg->MouseY, msg->Code );
          break;
      }
#ifndef __SDL_WRAPPER__
      IExec->ReplyMsg( (struct Message *)msg );
#endif
    }
  }

#ifndef __SDL_WRAPPER__
  if( pref_dorestart )
  {
    if( !gui_restart() )
      quitting = TRUE;
    pref_dorestart = FALSE;
  }
#endif
}

void gui_shutdown( void )
{
  uint32 i;

  gui_close_prefs();
  if( gui_savethem ) gui_save_prefs();
#ifndef __SDL_WRAPPER__
  if( mainwin ) IIntuition->CloseWindow( mainwin );
  if( scr )     IIntuition->CloseScreen( scr );
#endif

  if( cbpbuf ) IExec->FreeVec( cbpbuf );

#ifndef __SDL_WRAPPER__
  if( ins_lreq ) IAsl->FreeAslRequest( ins_lreq );
  if( ins_sreq ) IAsl->FreeAslRequest( ins_sreq );
  if( sng_lreq ) IAsl->FreeAslRequest( sng_lreq );
  if( sng_sreq ) IAsl->FreeAslRequest( sng_sreq );
  if( dir_req )  IAsl->FreeAslRequest( dir_req );
  
  for( i=0; i<BM_END; i++ ) if( bitmaps[i].bm ) IGraphics->FreeBitMap( bitmaps[i].bm );
  for( i=0; i<TB_END; i++ ) if( tbx[i].bm.bm )  IGraphics->FreeBitMap( tbx[i].bm.bm );
  
  if( prpfont ) IGraphics->CloseFont( prpfont );
  if( fixfont ) IGraphics->CloseFont( fixfont );
  if( sfxfont ) IGraphics->CloseFont( sfxfont );
 
  if( gui_tick_signum != -1 ) IExec->FreeSignal( gui_tick_signum );

  if( IRequester )    IExec->DropInterface( (struct Interface *)IRequester );
  if( RequesterBase ) IExec->CloseLibrary( RequesterBase );
  if( IKeymap )       IExec->DropInterface( (struct Interface *)IKeymap );
  if( KeymapBase )    IExec->CloseLibrary( KeymapBase );
  if( IDiskfont )     IExec->DropInterface( (struct Interface *)IDiskfont );
  if( DiskfontBase )  IExec->CloseLibrary( DiskfontBase );
  if( IDataTypes )    IExec->DropInterface( (struct Interface *)IDataTypes );
  if( DataTypesBase ) IExec->CloseLibrary( DataTypesBase );
  if( IP96)           IExec->DropInterface( (struct Interface *)IP96 );
  if( GfxBase )       IExec->CloseLibrary( P96Base );
  if( IGraphics )     IExec->DropInterface( (struct Interface *)IGraphics );
  if( GfxBase )       IExec->CloseLibrary( GfxBase );
  if( IIntuition )    IExec->DropInterface( (struct Interface *)IIntuition );
  if( IntuitionBase ) IExec->CloseLibrary( IntuitionBase );
#else
  for( i=0; i<BM_END; i++ ) if( bitmaps[i].srf ) SDL_FreeSurface( bitmaps[i].srf );
  for( i=0; i<TB_END; i++ ) if( tbx[i].bm.srf )  SDL_FreeSurface( tbx[i].bm.srf );
  if( prefbm.srf ) SDL_FreeSurface( prefbm.srf );
  TTF_CloseFont( prpttf ); prpttf = NULL;
  TTF_CloseFont( fixttf ); fixttf = NULL;
  TTF_CloseFont( sfxttf ); sfxttf = NULL;
#endif
}

