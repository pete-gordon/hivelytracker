/*
** HivelyTracker SDL !!!!11
*/

#include <stdio.h>
#include <signal.h>

#include <system_includes.h>
#include <replay.h>
#include <gui.h>
#include <about.h>

BOOL quitting = FALSE;
extern BOOL pref_dorestart;
extern BOOL needaflip;
extern SDL_Surface *ssrf;
int srfdepth = 16;
extern BOOL pref_fullscr;
extern BOOL aboutwin_open;

SDL_Event event;

BOOL init( void )
{
  const SDL_VideoInfo *info = NULL;

  gui_pre_init();
  about_pre_init();

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER ) < 0 )
  {
    printf( "SDL init failed" );
    return FALSE;
  }
  atexit(SDL_Quit);

  if( (info = SDL_GetVideoInfo()) )
    srfdepth = info->vfmt->BitsPerPixel;

  SDL_WM_SetIcon( SDL_LoadBMP( "winicon.bmp" ), NULL );

  // Try to setup the video display
#if defined(WIN32) || defined(__APPLE__)
  // requesters cause all kinds of problems for fullscreen on windows and OSX, so ignore it
  ssrf = SDL_SetVideoMode( 800, 600, srfdepth, SRFTYPE );
#else
  ssrf = SDL_SetVideoMode( 800, 600, srfdepth, pref_fullscr ? SDL_FULLSCREEN : SRFTYPE );
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
  if( !about_init() ) return FALSE;

  return TRUE;
}

void shutdown( void )
{
  about_shutdown();
  gui_shutdown();
  rp_shutdown();
}

int main( int argc, char *argv[] )
{
  if( init() )
  {
    SDL_Flip(ssrf);
    gui_req(0, "Here be dragons!", "This is beta software. Don't spread it.\nDon't expect it to work, or not to crash.\n\nBeta 6 (" __DATE__ " " __TIME__ ")", "OK");
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
            if (aboutwin_open)
              about_handler(0);
            else
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
