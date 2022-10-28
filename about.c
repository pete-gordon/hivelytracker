/*
** Theres not many "about" dialogs that get their own C file ;-)
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <system_includes.h>


#include "replay.h"
#include "gui.h"

#define NUMSTARS 100

struct star
{
  int16 x, y;
  int16 sp;
};

struct star stars[NUMSTARS];

#ifndef __SDL_WRAPPER__
extern struct Screen *scr;
struct Window *aboutwin = NULL;
extern struct Window *mainwin;

uint32 about_winsig = 0;
uint32 about_sigs = 0;
uint32 about_timesig;

struct MsgPort     *ab_tioport = NULL;
struct TimeRequest *ab_tioreq = NULL;
BOOL                ab_tiopen = FALSE;
BOOL                ab_tiopending = FALSE;
#else
BOOL aboutwin_open;
extern struct rawbm mainbm;
extern SDL_Event event;
extern struct ahx_tune *curtune;
extern int32 wm_count;
extern struct ahx_tune *rp_curtune;
SDL_TimerID thetimer = 0;
#endif

int16 ab_x = -1, ab_y = -1;

struct rawbm about_bm;
struct rawbm afont_bm;
struct rawbm stuff_bm;

int16 slice[320/2];
float64 sinpos = 0, cospos = 0;

const TEXT *scrtxt = " hivelytracker version 1.9  ::  code by xeron of iris  "
                     "::  gui design and skins created by spot of up rough  "
                     "::  example tunes by xeron of iris, varthall of up rough, oxide of sonik and syphus  "
                     "::  hivelytracker is based on ahx by dexter and pink of abyss  "
                     "::  many thanks to pieknyman for helping me find and fix bugs in the replayer. you rule!  "
                     "::  large parts of ahxplay.library by ilkka lehtoranta and per johansson were referenced for the replayer  "
                     "::  thanks to passing by and kode54 for replayer improvements  "
                     "::  thanks to spot, varthall and the rest of up rough for their ideas, tunes and betatesting  "
                     "::  thanks to buzz for the aros and morphos ports, deltafire for the os x port, and pulkomandy for the haiku port  "
                     "::  thanks to the betatesters of the sdl port as well!  "
                     "::  greets... rno, tbl, tulou, scarab, nature, triad, dcs, fairlight, and yes, even loonies.  "
                     "::  remember kids, say no to blancmange!           "
                     "::  extra thanks to spot for his nagging and complaining, without "
                     " whom hively would not be as cool as it now is! "
                     ":: so... why did i write an ahx based tracker for os4? for one simple "
                     "reason: i wanted to create ahx tunes on my amigaone without having to "
                     "resort to uae. unfortunately ahx used the cia timers and paula directly "
                     "and so it didnt work properly at all under os4. so, i sat down and made "
                     "my own os4 ahx tracker, and i started adding new features as i did so. "
                     "i hope you enjoy it and the tunes made in it!          "
                     "messages from buzz: i would like to thank xeron for this great tracker, and being patient with me "
                     "while i port it. also thanks to all the betatesters on irc ... hi syphus. "
                     "i would also like to thank stingray for his useless tips and code advice. just kidding! "
                     "he is right about one thing though, my code does suck! finally hi to yaz: "
                     "thanks for sending me that new tune !         ";

const TEXT *scrtab = "abcdefghijklmnopqrstuvwxyz!,.:?1234567890";

int32 scro=0, scrs=30, scrc=0;
int32 bx1=0, bx2=320;

#ifdef __SDL_WRAPPER__
Uint32 about_timing( Uint32 interval, void *userdata )
{
  if( aboutwin_open )
  {
    SDL_Event     tmpevent;
    SDL_UserEvent userevent;

    userevent.type  = SDL_USEREVENT;
    userevent.code  = 88;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    tmpevent.type = SDL_USEREVENT;
    tmpevent.user = userevent;
    
    SDL_PushEvent( &tmpevent );
  }

  return 16;
}
#endif


void about_pre_init( void )
{
  int32 i;

#ifndef __SDL_WRAPPER__
  about_bm.bm = NULL;
  afont_bm.bm = NULL;
  stuff_bm.bm = NULL;
#else
  about_bm.srf = NULL;
  afont_bm.srf = NULL;
  stuff_bm.srf = NULL;
#endif
  
  for( i=0; i<160; i++ ) slice[i] = -1;
}

BOOL about_init( void )
{
  int32 r, g, b, i;

#ifndef __SDL_WRAPPER__
  ab_tioport = IExec->AllocSysObjectTags(ASOT_PORT, TAG_DONE);
  if( !ab_tioport )
  {
    printf( "Unable to create message port\n" );
    return FALSE;
  }
  
  about_timesig = 1L<<ab_tioport->mp_SigBit;
  
  ab_tioreq = (struct TimeRequest *)IExec->AllocSysObjectTags(ASOT_IOREQUEST,
    ASOIOR_ReplyPort, ab_tioport,
    ASOIOR_Size,      sizeof( struct TimeRequest ),
    TAG_DONE);
  if( !ab_tioreq )
  {
    printf( "Unable to create io request!\n" );
    return FALSE;
  }

  // Get timer.device
  ab_tiopen = !IExec->OpenDevice( "timer.device", UNIT_MICROHZ, (struct IORequest *)ab_tioreq, 0 );
  if( !ab_tiopen )
  {
    printf( "Unable to open timer.device\n" );
    return FALSE;
  }
  
  about_sigs = about_timesig;

  if( !make_image( &about_bm, 320, 240 ) ) return FALSE;
#else
  SDL_Rect cliprect = { .x = 4, .y = 4, .w = 320, .h = 240 };
  if( !make_image( &about_bm, 328, 248 ) ) return FALSE;
  about_bm.offx = 4;
  about_bm.offy = 4;
  SDL_SetClipRect(about_bm.srf, &cliprect);
#endif

  if( !make_image( &stuff_bm, 640,   2 ) ) return FALSE;
  if( !open_image( "aboutfont", &afont_bm ) ) return FALSE;

  set_fcol(&stuff_bm, 0x000000);
  fillrect_xy(&stuff_bm, 0, 0, 639, 1);

  r = 255<<8;
  g = 0;
  b = 0;

  for( i=0; i<106; i++ )
  {
    set_fcol(&stuff_bm, ((r&0xff00)<<8)|(g&0xff00)|((b&0xff00)>>8));
#ifndef __SDL_WRAPPER__
    IGraphics->WritePixel( &stuff_bm.rp, i, 0 );
#else
    fillrect_xy(&stuff_bm, i, 0, i, 0);
#endif
    r -= ((255*256)/106);
    g += ((255*256)/106);
  }

  r = 0;
  g = 255<<8;
  b = 0;

  for( i=106; i<213; i++ )
  {
    set_fcol(&stuff_bm, ((r&0xff00)<<8)|(g&0xff00)|((b&0xff00)>>8));
#ifndef __SDL_WRAPPER__
    IGraphics->WritePixel( &stuff_bm.rp, i, 0 );
#else
    fillrect_xy(&stuff_bm, i, 0, i, 0);
#endif
    g -= ((255*256)/107);
    b += ((255*256)/107);
  }

  r = 0;
  g = 0;
  b = 255<<8;

  for( i=213; i<320; i++ )
  {
    set_fcol(&stuff_bm, ((r&0xff00)<<8)|(g&0xff00)|((b&0xff00)>>8));
#ifndef __SDL_WRAPPER__
    IGraphics->WritePixel( &stuff_bm.rp, i, 0 );
#else
    fillrect_xy(&stuff_bm, i, 0, i, 0);
#endif
    b -= ((255*256)/107);
    r += ((255*256)/107);
  }

  set_fcol(&stuff_bm, 0xffffff);
  fillrect_xy(&stuff_bm, 0, 1, 0, 1);
  set_fcol(&stuff_bm, 0xaaaaaa);
  fillrect_xy(&stuff_bm, 1, 1, 1, 1);
  set_fcol(&stuff_bm, 0x888888);
  fillrect_xy(&stuff_bm, 2, 1, 2, 1);
  set_fcol(&stuff_bm, 0x444444);
  fillrect_xy(&stuff_bm, 3, 1, 3, 1);

  for( i=0; i<NUMSTARS; i++ )
  {
    stars[i].x = rand()%320;
    stars[i].y = (rand()%208)+16;
    stars[i].sp = (rand()%6)+1;
  }

  bm_to_bm(&stuff_bm, 0, 0, &stuff_bm, 320, 0, 320, 1);

  return TRUE;
}

void about_close( void )
{
#ifndef __SDL_WRAPPER__
  if( aboutwin == NULL ) return;
  
  ab_x = aboutwin->LeftEdge;
  ab_y = aboutwin->TopEdge;
  IIntuition->CloseWindow( aboutwin );
  aboutwin = NULL;
  
  about_sigs &= ~about_winsig;
  about_winsig = 0;

  if( ab_tiopending )
  {
    IExec->AbortIO( (struct IORequest *)ab_tioreq );
    IExec->WaitIO( (struct IORequest *)ab_tioreq );
    ab_tiopending = FALSE;
  }
#else
  aboutwin_open = FALSE;
  if (thetimer) SDL_RemoveTimer(thetimer);
  thetimer = 0;
  gui_render_everything();
#endif
}

void about_shutdown( void )
{
  about_close();

#ifndef __SDL_WRAPPER__
  if( about_bm.bm ) IGraphics->FreeBitMap( about_bm.bm );
  if( afont_bm.bm ) IGraphics->FreeBitMap( afont_bm.bm );
  if( stuff_bm.bm ) IGraphics->FreeBitMap( stuff_bm.bm );
  
  if( ab_tiopen )
  {
    if( ab_tiopending )
    {
      IExec->AbortIO( (struct IORequest *)ab_tioreq );
      IExec->WaitIO( (struct IORequest *)ab_tioreq );
      ab_tiopending = FALSE;
    }
    IExec->CloseDevice( (struct IORequest *)ab_tioreq );
  }
  if( ab_tioreq )   IExec->FreeSysObject( ASOT_IOREQUEST, ab_tioreq );
  if( ab_tioport )  IExec->FreeSysObject( ASOT_PORT, ab_tioport );

  about_bm.bm = NULL;
  afont_bm.bm = NULL;
  stuff_bm.bm = NULL;
#else
  if (about_bm.srf) SDL_FreeSurface(about_bm.srf);
  if (afont_bm.srf) SDL_FreeSurface(afont_bm.srf);
  if (stuff_bm.srf) SDL_FreeSurface(stuff_bm.srf);
  about_bm.srf = NULL;
  afont_bm.srf = NULL;
  stuff_bm.srf = NULL;
#endif
}

void about_open( void )
{
#ifndef __SDL_WRAPPER__
  if( aboutwin ) return;
  
  if( ab_x == -1 )
  {
    ab_x = mainwin->LeftEdge + 40;
    ab_y = mainwin->TopEdge + 40;
  }
  
  aboutwin = IIntuition->OpenWindowTags( NULL,
    WA_Left,        ab_x,
    WA_Top,         ab_y,
    WA_InnerWidth,  320,
    WA_InnerHeight, 240,
    WA_Title,       "HivelyTracker v1.9",
    WA_ScreenTitle, "HivelyTracker (c)2022 IRIS & Up Rough! - http://www.uprough.net - http://www.hivelytracker.co.uk",
    WA_RMBTrap,     TRUE,
    WA_IDCMP,       IDCMP_CLOSEWINDOW,  //|IDCMP_MOUSEBUTTONS,
    WA_Activate,    TRUE,
    WA_DragBar,     TRUE,
    WA_CloseGadget, TRUE,
    WA_DepthGadget, TRUE,
    scr ? WA_CustomScreen : TAG_IGNORE, scr,
    TAG_DONE );
    
 if( !aboutwin ) return;
 
 about_winsig = (1L<<aboutwin->UserPort->mp_SigBit);
 about_sigs   |= about_winsig;

  if( ab_tiopending )
  {
    IExec->AbortIO( (struct IORequest *)ab_tioreq );
    IExec->WaitIO( (struct IORequest *)ab_tioreq );
    ab_tiopending = FALSE;
  }

  ab_tioreq->Request.io_Command = TR_ADDREQUEST;
  ab_tioreq->Time.Seconds      = 0;
  ab_tioreq->Time.Microseconds = 20000;
  IExec->SendIO( (struct IORequest *)ab_tioreq );
  ab_tiopending = TRUE;
#else
  thetimer = SDL_AddTimer( 20, (SDL_NewTimerCallback)about_timing, NULL );
  aboutwin_open = TRUE;
#endif
}

void about_toggle( void )
{
#ifndef __SDL_WRAPPER__
  if( aboutwin )
#else
  if( aboutwin_open )
#endif
    about_close();
  else
    about_open();
}

void about_frame( void )
{
  int32 i;
  float64 stmp, ctmp;

#ifndef __SDL_WRAPPER__
  if( aboutwin == NULL ) return;
#else
  if( !aboutwin_open ) return;
#endif

  set_fcol(&about_bm, 0x000000);
  fillrect_xy(&about_bm, 0, 0, 319, 239);

  for( i=0; i<158; i++ )
    slice[i] = slice[i+2];

  scrs+=4;
  if( scrs > 31 )
  {
    scrs = 0;
    scro++;
    if( scrtxt[scro] == 0 ) scro = 0;
    for( scrc=0; scrc<strlen( scrtab ); scrc++ )
      if( scrtab[scrc] == scrtxt[scro] ) break;
    if( scrc == strlen( scrtab ) ) scrc = -1;
  }
  
  if( scrc > -1 )
  {
    slice[158] = (scrc<<5)+scrs;
    slice[159] = (scrc<<5)+scrs+2;
  } else {
    slice[158] = slice[159] = -1;
  }
  
  for( i=0; i<NUMSTARS; i++ )
  {
    stars[i].x -= stars[i].sp;
    if( stars[i].x < -3 )
    {
      stars[i].x = 320;
      stars[i].y = (rand()%208)+16;
      stars[i].sp = (rand()%6)+1;
    }

    bm_to_bm(&stuff_bm, 0, 1, &about_bm, stars[i].x, stars[i].y, 4, 1);
  }

  for( i=0, stmp=sinpos, ctmp=cospos; i<160; i++, stmp+=0.015f, ctmp -= 0.021f )
    if( slice[i] != -1 )
      bm_to_bm(&afont_bm, slice[i], 0, &about_bm, i<<1, ((int32)(sin(stmp)*cos(ctmp)*85))+101, 2, 38);

  sinpos += 0.1f;
  cospos -= 0.0712f;

  bm_to_bm(&stuff_bm, bx1, 0, &about_bm, 0,   8, 320, 1);
  bm_to_bm(&stuff_bm, bx2, 0, &about_bm, 0, 232, 320, 1);

  bx1 = (bx1+  4)%320;
  bx2 = (bx2+316)%320;

#ifndef __SDL_WRAPPER__
  IGraphics->BltBitMapRastPort( about_bm.bm, 0, 0, aboutwin->RPort, aboutwin->BorderLeft, aboutwin->BorderTop, 320, 240, 0x0C0 );
#else
  bm_to_bm(&about_bm, -4, -4, &mainbm, 236, 176, 328, 248);
#endif
}

//BOOL pause = FALSE;

void about_handler( uint32 gotsigs )
{
#ifndef __SDL_WRAPPER__
  struct IntuiMessage *msg;
  BOOL closeme;
  
  closeme = FALSE;

  if( gotsigs & about_timesig )
  {
    if( ab_tiopending )
    {
      IExec->WaitIO( (struct IORequest *)ab_tioreq );
      ab_tiopending = FALSE;
    }

    if( aboutwin != NULL )
    {
//      if( !pause )
      about_frame();

      ab_tioreq->Request.io_Command = TR_ADDREQUEST;
      ab_tioreq->Time.Seconds      = 0;
      ab_tioreq->Time.Microseconds = 28571;  // 35 FPS
      IExec->SendIO( (struct IORequest *)ab_tioreq );
      ab_tiopending = TRUE;
    }
  }

  if( gotsigs & about_winsig )
  {
    while( ( msg = (struct IntuiMessage *)IExec->GetMsg( aboutwin->UserPort ) ) )
    {
      switch( msg->Class )
      {
/*
        case IDCMP_MOUSEBUTTONS:
          if( msg->Code == SELECTUP ) pause ^= 1;
          break;
*/
        case IDCMP_CLOSEWINDOW:
          closeme = TRUE;
          break;
      }
      IExec->ReplyMsg( (struct Message *)msg );
    }
  }
  
  if( closeme ) about_close();
#else
  switch (event.type)
  {
    case SDL_USEREVENT:
      switch (event.user.code)
      {
        case 0:
          if( curtune == rp_curtune )
          {
            gui_render_tunepanel( FALSE );
            gui_render_vumeters();
          }
          if( (wm_count++)&1 )
            gui_render_wavemeter();
          bm_to_bm(&about_bm, -4, -4, &mainbm, 236, 176, 328, 248);
          break;
        
        case 88:
          about_frame();
          break;
      }
      break;
    
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT)
        about_close();
      break;

    case SDL_KEYUP:
      if (event.key.keysym.sym == SDLK_ESCAPE)
        about_close();
      break;
  }
#endif
}

