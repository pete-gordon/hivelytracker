
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <windows.h>
#include <SDL.h>

// The amiga wrapper and the windows header don't get along...
typedef Uint32   uint32;
typedef Sint32   int32;
typedef char     TEXT;

int32 gui_req( uint32 img, TEXT *title, TEXT *reqtxt, TEXT *buttons )
{
  return MessageBoxA(NULL, reqtxt, title, strcmp(buttons, "OK") ? MB_OKCANCEL : MB_OK ) == IDOK;
}

#define FR_HVLSAVE 0
#define FR_AHXSAVE 1
#define FR_INSSAVE 2
#define FR_MODLOAD 3
#define FR_INSLOAD 4

char *filerequester( char *title, char *path, char *fname, int type )
{
  OPENFILENAME ofn;
  char *result;
  char *odir;

  ZeroMemory( &ofn, sizeof( ofn ) );
  ofn.lStructSize = sizeof( ofn );
  ofn.hwndOwner   = NULL;
  ofn.lpstrFile   = fname;
  ofn.nMaxFile    = 4096;

  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  switch( type )
  {
    case FR_HVLSAVE:
      ofn.Flags = OFN_PATHMUSTEXIST;
      ofn.lpstrFilter = "All Files\0*.*\0HVL module (*.hvl)\0*.HVL\0";
      ofn.nFilterIndex = 2;
      break;
    
    case FR_AHXSAVE:
      ofn.Flags = OFN_PATHMUSTEXIST;
      ofn.lpstrFilter = "All Files\0*.*\0AHX module (*.ahx)\0*.AHX\0";
      ofn.nFilterIndex = 2;
      break;

    case FR_INSSAVE:
      ofn.Flags = OFN_PATHMUSTEXIST;
      ofn.lpstrFilter = "All Files\0*.*\0Instrument (*.ins)\0*.INS\0";
      ofn.nFilterIndex = 2;
      break;

    case FR_MODLOAD:
      ofn.lpstrFilter = "All Files\0*.*\0Modules (*.ahx, *.hvl, *.mod)\0*.AHX;*.HVL;*.MOD\0";
      ofn.nFilterIndex = 2;
      break;

    case FR_INSLOAD:
      ofn.lpstrFilter = "All Files\0*.*\0Instrument (*.ins)\0*.INS\0";
      ofn.nFilterIndex = 2;
      break;

    default:
      ofn.lpstrFilter = "All Files\0*.*";
      ofn.nFilterIndex = 1;
      break;
  }

  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = path;

  odir = getcwd( NULL, 0 );

  switch( type )
  {
    case FR_HVLSAVE:
    case FR_AHXSAVE:
    case FR_INSSAVE:
      if( !GetSaveFileName( &ofn ) )
      {
        chdir( odir );
        free( odir );
        return NULL;
      }
      break;
    
    default:
      if( !GetOpenFileName( &ofn ) )
      {
        chdir( odir );
        free( odir );
        return NULL;
      }
      break;
  }

  chdir( odir );
  free( odir );

  if (strlen(ofn.lpstrFile)==0) return NULL;

  result = malloc(strlen(ofn.lpstrFile)+1);
  if (result)
  {
    strcpy(result, ofn.lpstrFile);
  }

  return NULL;
}
