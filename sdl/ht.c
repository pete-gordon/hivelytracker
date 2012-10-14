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
extern BOOL needaflip;
extern SDL_Surface *ssrf;
int srfdepth = 16;
extern BOOL pref_fullscr;

SDL_Event event;

BOOL init( void )
{
  const SDL_VideoInfo *info = NULL;

//  gui_pre_init();
//  about_pre_init();

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed" );
    return FALSE;
  }
  atexit(SDL_Quit);

  if( (info = SDL_GetVideoInfo()) )
  {
    switch( info->vfmt->BitsPerPixel )
    {
      // Great, cases we handle
      case 16:
      case 32:
        srfdepth = info->vfmt->BitsPerPixel;
        break;
      // Damn, cases we don't handle. Let's say 16 bpp gonna be ok
      default:
        break;
    }
  }

  // Try to setup the video display
  ssrf = SDL_SetVideoMode( 800, 600, srfdepth, pref_fullscr ? SDL_FULLSCREEN : SRFTYPE );
  if( !ssrf )
  {
    printf( "SDL video failed\n" );
    return FALSE;
  }

  SDL_WM_SetCaption( "HivelyTracker SDL", "HivelyTracker SDL" );

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
    quitting = FALSE;
    while( !quitting )
    {
      if (needaflip)
      {
        SDL_Flip(ssrf);
        needaflip = FALSE;
      }

      printf("Waiting for an event..\n");
      fflush(stdout);
//      if( !SDL_WaitEvent( &event ) ) break;
      while (!SDL_PollEvent(&event))
      {
        SDL_Delay(1);
      }
  
      do
      {
        printf("Got an event (%d)\n", event.type);
        fflush(stdout);
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
      printf("Dropped out of loop\n");
      fflush(stdout);
    }
    
    rp_stop();
  }
  shutdown();
  return 0;
}
