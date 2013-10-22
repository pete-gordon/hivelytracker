/*
** HivelyTracker !!!!11
*/

#include <stdio.h>
#include <signal.h>

#include <exec/types.h>

#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "replay.h"
#include "gui.h"
#include "about.h"

BOOL quitting = FALSE;

extern uint32 gui_sigs;
extern uint32 rp_sigs;
extern uint32 about_sigs;

extern struct List *rp_tunelist;

TEXT __attribute((used)) ver[] = "$VER: HivelyTracker 1.8 (7.5.2013)";

BOOL init( void )
{
  gui_pre_init();
  about_pre_init();
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

int main( void )
{
  signal( SIGINT, SIG_IGN );
  if( init() )
  {
    uint32 gotsigs;

    while( !quitting )
    {
      gotsigs = Wait( gui_sigs | rp_sigs | about_sigs | SIGBREAKF_CTRL_C );
      
      if( gotsigs & rp_sigs )
      {
        rp_handler( gotsigs );
        if( quitting ) break;   // If rp_handler() says we need to quit, don't do anything else but just QUIT...
      }
      
      if( gotsigs & SIGBREAKF_CTRL_C )
      {
        signal( SIGINT, SIG_IGN );
        quitting = TRUE;
      }
      
      if( gotsigs & about_sigs ) about_handler( gotsigs );
      
      if( gotsigs & gui_sigs ) gui_handler( gotsigs );
    }
    
    rp_stop();
  }
  shutdown();
  return 0;
}
