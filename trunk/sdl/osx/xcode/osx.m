//
//  osx.m
//  Hivelytracker: OSX dialog and file reqester support
//
//  Created by Christopher O'Neill on 15/10/2012.
//

typedef Uint32   uint32;
typedef Sint32   int32;
typedef char     TEXT;

int32 gui_req( uint32 img, TEXT *title, TEXT *reqtxt, TEXT *buttons )
{
	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:[NSString stringWithUTF8String:title]];
	[alert setInformativeText:[NSString stringWithUTF8String:reqtxt]];
	NSMutableString *btnStr = [NSMutableString stringWithUTF8String:buttons];
	[btnStr replaceOccurrencesOfString:@"_" withString:@"" options:0
								 range:NSMakeRange(0, [btnStr length])];
	NSArray *btnArr = [btnStr componentsSeparatedByString:@"|"];
	for (btnStr in [[btnArr reverseObjectEnumerator] allObjects])
	{
		[alert addButtonWithTitle:btnStr];
	}
	
	return [alert runModal] - 1000;
}

#define FR_HVLSAVE 0
#define FR_AHXSAVE 1
#define FR_INSSAVE 2
#define FR_MODLOAD 3
#define FR_INSLOAD 4

char *filerequester( char *title, char *path, char *fname, int type )
{
	NSArray* fileTypes;
	NSSavePanel* savePanel;
	NSOpenPanel* panel;
	switch (type) {
		case FR_HVLSAVE:
			fileTypes = [NSArray arrayWithObjects:@"hvl", @"HVL", nil];
			break;
		case FR_AHXSAVE:
            fileTypes = [NSArray arrayWithObjects:@"ahx", @"AHX", nil];
			break;
		case FR_INSSAVE:
		case FR_INSLOAD:
			fileTypes = [NSArray arrayWithObjects:@"ins", @"INS", nil];
			break;
		case FR_MODLOAD:
			fileTypes = [NSArray arrayWithObjects:@"hvl", @"HVL", @"ahx", @"AHX", nil];
			break;
      default:
        return NULL;
	}
	
	if ((type == FR_INSLOAD) || (type == FR_MODLOAD)) {
		panel = [NSOpenPanel openPanel];
	} else {
		savePanel = [NSSavePanel savePanel];
		panel = (NSOpenPanel*) savePanel;
	}

  [panel setAllowedFileTypes:fileTypes];
  [panel setAllowsOtherFileTypes:YES];
  [panel setTitle:[NSString stringWithUTF8String:title]];
  [panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path] isDirectory:YES]];
  [panel setNameFieldStringValue:[NSString stringWithUTF8String:fname]];
  int i = [panel runModal];
	if (i == 1) {
		const char *cstr = [[[panel URL] path] UTF8String];
		char *rval = (char *) malloc(strlen(cstr) + 1);
		strcpy(rval, cstr);
		return rval;
	};
	
	return NULL;
}

BOOL directoryrequester( char *title, char *path )
{
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:NO];
	[panel setCanChooseDirectories:YES];
	[panel setTitle: [NSString stringWithUTF8String:title]];
	[panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path] isDirectory:YES]];
	int i = [panel runModal];
	if (i == 1) {
		const char *cstr = [[[panel URL] path] UTF8String];
		strncpy(path, cstr, 512);
		return TRUE;
	 };
	 
	 return FALSE;
}

char* osxGetPrefsPath()
{
  return NULL;
}