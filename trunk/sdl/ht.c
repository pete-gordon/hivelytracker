/*
** HivelyTracker SDL !!!!11
*/

#include <stdio.h>
#include <signal.h>

#include <system_includes.h>
#include <replay.h>
#include <gui.h>
//#include <about.h>

BOOL quitting = FALSE;
extern BOOL pref_dorestart;
extern BOOL needaflip;
extern SDL_Surface *ssrf;
int srfdepth = 16;
extern BOOL pref_fullscr;

SDL_Event event;

BOOL init( void )
{
  const SDL_VideoInfo *info = NULL;

  gui_pre_init();
//  about_pre_init();

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed" );
    return FALSE;
  }
  atexit(SDL_Quit);

  if( (info = SDL_GetVideoInfo()) )
    srfdepth = info->vfmt->BitsPerPixel;

  // Try to setup the video display
#ifndef WIN32
  ssrf = SDL_SetVideoMode( 800, 600, srfdepth, pref_fullscr ? SDL_FULLSCREEN : SRFTYPE );
#else
  // requesters cause all kinds of problems for fullscreen on windows, so ignore it
  ssrf = SDL_SetVideoMode( 800, 600, srfdepth, SRFTYPE );
#endif
  if( !ssrf )
  {
    printf( "SDL video failed\n" );
    return FALSE;
  }

  SDL_WM_SetCaption( "HivelyTracker SDL", "HivelyTracker SDL" );

  SDL_EnableKeyRepeat(500, 30);

  if (TTF_Init() == -1)
  {
    printf( "SDL TTF failed\n" );
    return FALSE;
  }

  atexit(TTF_Quit);

  if( !rp_init() )  return FALSE;
  if( !gui_init() ) return FALSE;  
//  if( !about_init() ) return FALSE;

  return TRUE;
}

void shutdown( void )
{
//  about_shutdown();
   gui_shutdown();
  rp_shutdown();
}

int main( int argc, char *argv[] )
{
  if( init() )
  {
    SDL_Flip(ssrf);
    gui_req(0, "Here be dragons!", "This is beta software. Don't spread it.\nDon't expect it to work, or not to crash.\n\nBeta 4 (" __DATE__ " " __TIME__ ")", "OK");
    quitting = FALSE;
    while( !quitting )
    {
      if (needaflip)
      {
        SDL_Flip(ssrf);
        needaflip = FALSE;
      }

      if( !SDL_WaitEvent( &event ) ) break;
  
      do
      {
        switch( event.type )
        {
          case SDL_QUIT:
            quitting = TRUE;
            break;

          default:
            gui_handler(0);
            break;
        }
      }
      while (SDL_PollEvent(&event));

      if( pref_dorestart )
      {
        if( !gui_restart() )
          quitting = TRUE;
        pref_dorestart = FALSE;
      }   
    }
    
    rp_stop();
  }
  shutdown();
  return 0;
}
