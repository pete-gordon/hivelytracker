//
//  osx.m
//  Hivelytracker: OSX dialog and file reqester support
//
//  Created by Christopher O'Neill on 15/10/2012.
//

#import <Cocoa/Cocoa.h>
#import "NSFileManager+DirectoryLocations.h"

typedef uint32_t   uint32;
typedef int32_t   int32;
typedef char     TEXT;

extern BOOL enableKeys;

int32 gui_req( uint32 img, const TEXT *title, const TEXT *reqtxt, const TEXT *buttons )
{
	int i;
	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:[NSString stringWithCString:title encoding:NSASCIIStringEncoding]];
	[alert setInformativeText:[NSString stringWithCString:reqtxt encoding:NSASCIIStringEncoding]];
	NSMutableString *btnStr = [NSMutableString stringWithCString:buttons encoding:NSASCIIStringEncoding];
	[btnStr replaceOccurrencesOfString:@"_" withString:@"" options:0
								 range:NSMakeRange(0, [btnStr length])];
	NSArray *btnArr = [btnStr componentsSeparatedByString:@"|"];
	for (btnStr in [[btnArr reverseObjectEnumerator] allObjects])
	{
		[alert addButtonWithTitle:btnStr];
	}

	enableKeys = TRUE;
	i = [alert runModal] - 1000;
	enableKeys = FALSE;
	return i;
}

#define FR_HVLSAVE 0
#define FR_AHXSAVE 1
#define FR_INSSAVE 2
#define FR_MODLOAD 3
#define FR_INSLOAD 4

char *filerequester( const char *title, const char *path, const char *fname, int type )
{
	NSArray *fileTypes;
	NSSavePanel *savePanel;
	NSOpenPanel *panel;
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
		panel = (NSOpenPanel *)savePanel;
	}

	[panel setAllowedFileTypes:fileTypes];
	[panel setAllowsOtherFileTypes:YES];
	[panel setTitle:[NSString stringWithCString:title encoding:NSASCIIStringEncoding]];
	[panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithCString:path encoding:NSASCIIStringEncoding] isDirectory:YES]];
	[panel setNameFieldStringValue:[NSString stringWithCString:fname encoding:NSASCIIStringEncoding]];
	
	enableKeys = TRUE;
	int i = [panel runModal];
	enableKeys = FALSE;
	if (i == 1) {
		const char *cstr = [[[panel URL] path] UTF8String];
		char *rval = (char *)malloc(strlen(cstr) + 1);
		strcpy(rval, cstr);
		return rval;
	};

	return NULL;
}

BOOL directoryrequester( const char *title, char *path )
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setCanChooseFiles:NO];
	[panel setCanChooseDirectories:YES];
	[panel setTitle: [NSString stringWithCString:title encoding:NSASCIIStringEncoding]];
	[panel setDirectoryURL:[NSURL fileURLWithPath:[NSString stringWithCString:path encoding:NSASCIIStringEncoding] isDirectory:YES]];
	enableKeys = FALSE;
	int i = [panel runModal];
	enableKeys = FALSE;
	if (i == 1) {
		const char *cstr = [[[panel URL] path] UTF8String];
		strncpy(path, cstr, 512);
		return TRUE;
	};

	return FALSE;
}

char *osxGetPrefsPath()
{
	static char *path;

	if(!path) {
		NSString *strPath = [[NSFileManager defaultManager] applicationSupportDirectory];
		strPath = [strPath stringByAppendingPathComponent:@"ht.prefs"];
		path = malloc([strPath length] + 1);
		strcpy(path, [strPath UTF8String]);
	}
	return path;
}

char *osxGetResourcesPath(char *path, const char *pathAppend)
{
	NSBundle *bundle = [NSBundle mainBundle];
	NSString *strPath = [bundle resourcePath];
	strPath = [strPath stringByAppendingPathComponent:[NSString stringWithUTF8String:pathAppend]];
	if(!path)
		path = malloc([strPath length] + 1);
	strcpy(path, [strPath UTF8String]);
	return path;
}
