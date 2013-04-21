
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>

#include <Alert.h>
#include <Application.h>
#include <FilePanel.h>
#include <image.h>
#include <Messenger.h>
#include <Path.h>
#include <TextControl.h>

//#include "sdl_wrapper.h" // Conflicts with SupportDefs.h
#define FR_HVLSAVE 0
#define FR_AHXSAVE 1
#define FR_INSSAVE 2
#define FR_MODLOAD 3
#define FR_INSLOAD 4

#define REQIMAGE_QUESTION 0
#define REQIMAGE_WARNING 1
#define REQIMAGE_ERROR 2


extern "C" int32 gui_req( uint32 img, const char *title, const char *reqtxt, const char *buttons )
{
  const char* button[3];
  // Since buttons is a const char*, we can't use strtok directly on it.
  // Work on a copy.
  char* buffer = strdup(buttons);
  button[0] = strtok(buffer, "|");
  button[1] = strtok(NULL, "|");
  button[2] = strtok(NULL, "|");

  // TODO we don't handle the _ char in the button labels
  
  alert_type type;

  switch(img)
  {
	case REQIMAGE_QUESTION:
		type = B_INFO_ALERT;
		break;
	case REQIMAGE_WARNING:
		type = B_WARNING_ALERT;
		break;
	case REQIMAGE_ERROR:
		type = B_STOP_ALERT;
		break;
  }

  BAlert* alert = new BAlert(title, reqtxt, button[0], button[1], button[2],
	B_WIDTH_AS_USUAL, type);
  free(buffer);
  return alert->Go() == 0;
}


class FileHandler: public BLooper
{
	public:

	FileHandler()
		: BLooper("filepanel listener")
	{
		lock = create_sem(0, "filepanel sem");
		path = NULL;

		Run();
	}

	~FileHandler()
	{
		delete_sem(lock);
		free(path);
	}

	void MessageReceived(BMessage* message)
	{
		switch(message->what)
		{
			case B_REFS_RECEIVED:
			{
				entry_ref ref;

				message->FindRef("refs", 0, &ref);
				BEntry entry(&ref);
				BPath xpath;
				entry.GetPath(&xpath);
				path = strdup(xpath.Path());
				break;
			}

			case B_SAVE_REQUESTED:
			{
				entry_ref ref;

				message->FindRef("directory", 0, &ref);
				BEntry entry(&ref);
				BPath xpath;
				entry.GetPath(&xpath);
				const char* dir = xpath.Path();

				const char* name;
			    message->FindString("name", 0, &name);
				path = (char*)malloc(strlen(name) + strlen(dir) + 1);
				strcpy(path, dir);
				strcat(path, name);
				break;
			}

			case B_CANCEL:
			{
				// If the path was set, do not change it.
				// If it was not set, leave it null and HT will not load anything
				break;
			}

			default:
				printf("Unhandled message %x\n", message->what);
				BHandler::MessageReceived(message);
				return;
		}

		release_sem(lock);
	}

	void Wait() {
		acquire_sem(lock);
	}

	char* path;

	private:
		sem_id lock;
};


extern "C" bool directoryrequester( char *title, char *path )
{
	BEntry entry(path);
	entry_ref ref;
	entry.GetRef(&ref);

	FileHandler* handler = new FileHandler;

	BMessenger messenger(handler);

	BFilePanel* panel = new BFilePanel(B_OPEN_PANEL, &messenger, &ref,
		B_DIRECTORY_NODE, false);

	BWindow* win = panel->Window();
	
	win->LockLooper();
	win->SetTitle(title);
	win->UnlockLooper();

	panel->Show();
	handler->Wait();
	delete panel;

	if (handler->path) {
		strcpy(path, handler->path);
		return true;
	} else return false;
}

extern "C" char *filerequester( char *title, char *path, char *fname, int type )
{
	file_panel_mode mode;

	switch(type)
	{
		case FR_MODLOAD:
		case FR_INSLOAD:
			mode = B_OPEN_PANEL;
			break;
		default:
			mode = B_SAVE_PANEL;
	}

	BEntry entry(path);
	entry_ref ref;
	entry.GetRef(&ref);

	FileHandler* handler = new FileHandler;

	BMessenger messenger(handler);

	BFilePanel* panel = new BFilePanel(mode, &messenger, &ref, B_FILE_NODE, false);

	BWindow* win = panel->Window();
	
	win->LockLooper();
	win->SetTitle(title);

	if (mode == B_SAVE_PANEL) {
		BView* background = win->ChildAt(0);
		BView* nameView = background->FindView("text view");

		BTextControl* txt = dynamic_cast<BTextControl*>(nameView);
		txt->SetText(fname);
	}

	win->UnlockLooper();

	panel->Show();

	handler->Wait();

	delete panel;

    char* returnpath;
	if (handler->path)
   		returnpath = strdup(handler->path);
	else
		returnpath = NULL;
	handler->LockLooper();
	handler->Quit();
	return returnpath;
}

extern "C" void find_home()
{
    // Fix for haiku not starting apps in their home directory
	image_info info;
	int32 cookie;

	get_next_image_info(0, &cookie, &info);
	chdir(dirname(info.name));
}
