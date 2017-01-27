
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <SDL.h>
#include "sdl_wrapper.h"

#include <gtk/gtk.h>

typedef Uint32   uint32;
typedef Sint32   int32;
typedef char     TEXT;

int32 gui_req( uint32 img, TEXT *title, TEXT *reqtxt, TEXT *buttons )
{
  GtkWidget *dialog = NULL;
  GtkButtonsType btns = g_strcmp0((char*)btns, "OK") ? GTK_BUTTONS_OK_CANCEL : GTK_BUTTONS_OK;
  GtkMessageType mtyp = GTK_MESSAGE_INFO;
  SDL_bool result = SDL_FALSE;
  gint res;

  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, mtyp, btns, "%s", reqtxt);
  gtk_window_set_title(GTK_WINDOW (dialog), title);
  res = gtk_dialog_run(GTK_DIALOG (dialog));
  if ((res == GTK_RESPONSE_OK) || (res == GTK_RESPONSE_YES) || (res == GTK_RESPONSE_ACCEPT))
    result = SDL_TRUE;
  
  gtk_widget_destroy(dialog);
  while (gtk_events_pending())
    gtk_main_iteration();

  return result;
}

BOOL directoryrequester( char *title, char *path )
{
  GtkWidget *dialog;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
  BOOL result = FALSE;

  dialog = gtk_file_chooser_dialog_new(title,
				       NULL,
				       action,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				       NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);

  if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    strncpy(path, filename, 512);
    path[511] = 0;
    g_free(filename);
    result = TRUE;
  }
  
  gtk_widget_destroy(dialog);
  while (gtk_events_pending())
    gtk_main_iteration();

  return result;
}


#define FR_HVLSAVE 0
#define FR_AHXSAVE 1
#define FR_INSSAVE 2
#define FR_MODLOAD 3
#define FR_INSLOAD 4

char *filerequester( char *title, char *path, char *fname, int type )
{
  GtkWidget *dialog;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkFileFilter *filter = NULL, *allfilter = gtk_file_filter_new();
  char *result = NULL;
  char *accept_str = GTK_STOCK_OPEN;

  gtk_file_filter_set_name(allfilter, "All files");
  gtk_file_filter_add_pattern(allfilter, "*");

  switch( type )
  {
    case FR_HVLSAVE:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      accept_str = GTK_STOCK_SAVE;
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "HVL module (*.hvl)");
      gtk_file_filter_add_pattern(filter, "*.hvl");
      break;
    
    case FR_AHXSAVE:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      accept_str = GTK_STOCK_SAVE;
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "AHX module (*.ahx)");
      gtk_file_filter_add_pattern(filter, "*.ahx");
      break;

    case FR_INSSAVE:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      accept_str = GTK_STOCK_SAVE;
    case FR_INSLOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Instrument (*.ins)");
      gtk_file_filter_add_pattern(filter, "*.ins");
      break;

    case FR_MODLOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Modules (*.ahx, *.hvl, *.mod)");
      gtk_file_filter_add_pattern(filter, "*.ahx");
      gtk_file_filter_add_pattern(filter, "*.hvl");
      gtk_file_filter_add_pattern(filter, "*.mod");
      break;

    default:
      break;
  }

  dialog = gtk_file_chooser_dialog_new(title,
				       NULL,
				       action,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       accept_str, GTK_RESPONSE_ACCEPT,
				       NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), fname);

  if (filter)
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allfilter);

  if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    int i;
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    result = malloc(strlen(filename)+1);    
    strcpy(result, filename);
    g_free(filename);
  }
  
  gtk_widget_destroy(dialog);
  while (gtk_events_pending())
    gtk_main_iteration();

  return result;
}

