#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>


#include "hvl_replay.h"
#define AUDIO_BUFFER_LENGTH ((44100*2*2)/50)

char audiobuffer[3][AUDIO_BUFFER_LENGTH];

struct hvl_tune *ht = NULL;

UInt32			sSinWaveFrameCount = 0;
Float64			sSampleRate = 44100;
int				sNumChannels = 2;
UInt32 theFormatID = kAudioFormatLinearPCM;
// these are set based on which format is chosen
UInt32 theFormatFlags =  kLinearPCMFormatFlagIsSignedInteger
						| kAudioFormatFlagsNativeEndian
						| kLinearPCMFormatFlagIsPacked;
//						| kAudioFormatFlagIsNonInterleaved;
UInt32 theBytesInAPacket = 4;
UInt32 theBitsPerChannel = 16;
UInt32 theBytesPerFrame = 4; //AUDIO_BUFFER_LENGTH;

// these are the same regardless of format
UInt32 theFramesPerPacket = 1; // this shouldn't change


AudioUnit	gOutputUnit;
UInt32 pointerInPacket = 0;
UInt32 bufferInUse = 0;
volatile int catchedSignal =0;

// ________________________________________________________________________________
//
// Audio Unit Renderer!!!
//
OSStatus	MyRenderer(void 				*inRefCon, 
					   AudioUnitRenderActionFlags 	*ioActionFlags, 
					   const AudioTimeStamp 		*inTimeStamp, 
					   UInt32 						inBusNumber, 
					   UInt32 						inNumberFrames, 
					   AudioBufferList 			*ioData)

{

	UInt32 remainingBuffer, outputBuffer;
	outputBuffer = ioData->mBuffers[0].mDataByteSize;
	remainingBuffer = (AUDIO_BUFFER_LENGTH - pointerInPacket);
	if ( remainingBuffer > outputBuffer)
	{
		// buffer has enough unsend samples
	    memcpy (ioData->mBuffers[0].mData, audiobuffer[bufferInUse]+pointerInPacket, outputBuffer);
		pointerInPacket += outputBuffer;
		return noErr;
	} 
	if ( remainingBuffer < outputBuffer)
	{
		// buffer has only rest of unsend samples
		// first part
		memcpy (ioData->mBuffers[0].mData, audiobuffer[bufferInUse]+pointerInPacket, remainingBuffer);
		// second part
		UInt32 oldBuffer = bufferInUse;
		bufferInUse ^=1;
		memcpy (ioData->mBuffers[0].mData+remainingBuffer, audiobuffer[bufferInUse], (outputBuffer-remainingBuffer));
		pointerInPacket = (outputBuffer-remainingBuffer);
		hvl_DecodeFrame( ht, audiobuffer[oldBuffer], audiobuffer[oldBuffer]+2, 4 );
		return noErr;
	}

    if ( (remainingBuffer = outputBuffer))
	{
	    memcpy (ioData->mBuffers[0].mData, audiobuffer[bufferInUse]+pointerInPacket, outputBuffer);
		pointerInPacket = 0;
		hvl_DecodeFrame( ht, audiobuffer[bufferInUse], audiobuffer[bufferInUse]+2, 4 );
		bufferInUse ^=1;
		return noErr;
	}		
	
	return noErr;
}
// ________________________________________________________________________________
//
// CreateDefaultAU
//
void CreateDefaultAU(void)
{
	OSStatus err = noErr;
	
	// Open the default output unit
	//ComponentDescription desc;
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
    AudioComponentDescription desc;
#else
    ComponentDescription desc;
#endif
    desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
#else
	Component comp = FindNextComponent(NULL, &desc);
#endif
    if (comp == NULL) { printf ("No audio component found.\n"); return; }

#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
    err = AudioComponentInstanceNew(comp, &gOutputUnit);
    if (err) { printf (" open component failed %d\n", (int)err); return;}
#else
	err = OpenAComponent(comp, &gOutputUnit);
	if (comp == NULL) { printf ("OpenAComponent=%ld\n", (long int)err); return; }
#endif

    // Set up a callback function to generate output to the output unit
    AURenderCallbackStruct input;
	input.inputProc = MyRenderer;
	input.inputProcRefCon = NULL;
	
	err = AudioUnitSetProperty (gOutputUnit, 
								kAudioUnitProperty_SetRenderCallback, 
								kAudioUnitScope_Input,
								0, 
								&input, 
								sizeof(input));
	if (err) { printf ("AudioUnitSetProperty-CB=%ld\n", (long int)err); return; }
	AudioStreamBasicDescription streamDesc;
	
    UInt32 streamDescSize = sizeof( streamDesc );
	
    err = AudioUnitGetProperty( gOutputUnit,
							  kAudioUnitProperty_StreamFormat,
							  kAudioUnitScope_Global,
							  0,
							  &streamDesc,
							   &streamDescSize); 	
	if (err) { printf ("AudioUnitGetProperty-CB=%ld\n", (long int)err); return; }
    
}
// ________________________________________________________________________________
//
// Close DefaultAU
//
void CloseDefaultAU (void)
{
	// Clean up
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
    AudioComponentInstanceDispose (gOutputUnit);
#endif
}

// ________________________________________________________________________________
//
// Init Hively and CoreAudio
//
BOOL init( char *name )
{
	hvl_InitReplayer();
	ht = hvl_LoadTune( name, 44100, 0 );
	if( !ht ) return FALSE;
	CreateDefaultAU();
	return TRUE;  
}
// ________________________________________________________________________________
//
// ShutDown everything
//
void shut( void )
{
	CloseDefaultAU();
	if( ht ) hvl_FreeTune( ht );
}
// ________________________________________________________________________________
//
// Catch unix signals
//
void sig_handler(int signum)
{
	printf("Catched signal %d", signum);
	catchedSignal = signum;
}
// ________________________________________________________________________________
//
// MAIN Player, based on the Win32 player
//
int main(int argc, char *argv[])
{
	struct sigaction new_action;
	printf( "Hively Tracker command line player 1.9\n" );  
	
	if( argc < 2 )
	{
		printf( "Usage: play_hvl <tune>\n" );
		return 0;
	}
	
	if( init( argv[1] ) )
	{
		int i;
		
		printf( "Tune: '%s'\n", ht->ht_Name );
		printf( "Instruments:\n" );
		for( i=1; i<=ht->ht_InstrumentNr; i++ )
            printf( "%02d: %s\n", i, ht->ht_Instruments[i].ins_Name );
		
		OSStatus err = noErr;

		// We tell the Output Unit what format we're going to supply data to it
		// this is necessary if you're providing data through an input callback
		// AND you want the DefaultOutputUnit to do any format conversions
		// necessary from your format to the device's format.
		AudioStreamBasicDescription streamFormat;
		streamFormat.mSampleRate = sSampleRate;		//	the sample rate of the audio stream
		streamFormat.mFormatID = theFormatID;			//	the specific encoding type of audio stream
		streamFormat.mFormatFlags = theFormatFlags;		//	flags specific to each format
		streamFormat.mBytesPerPacket = theBytesInAPacket;	
		streamFormat.mFramesPerPacket = theFramesPerPacket;	
		streamFormat.mBytesPerFrame = theBytesPerFrame;		
		streamFormat.mChannelsPerFrame = sNumChannels;	
		streamFormat.mBitsPerChannel = theBitsPerChannel;	
		
//		printf ("Rendering source:\n\t");
//		printf ("SampleRate=%f,", streamFormat.mSampleRate);
//		printf ("BytesPerPacket=%ld,", (long int)streamFormat.mBytesPerPacket);
//		printf ("FramesPerPacket=%ld,", (long int)streamFormat.mFramesPerPacket);
//		printf ("BytesPerFrame=%ld,", (long int)streamFormat.mBytesPerFrame);
//		printf ("BitsPerChannel=%ld,", (long int)streamFormat.mBitsPerChannel);
//		printf ("ChannelsPerFrame=%ld\n", (long int)streamFormat.mChannelsPerFrame);
		
		err = AudioUnitSetProperty (gOutputUnit,
									kAudioUnitProperty_StreamFormat,
									kAudioUnitScope_Input,
									0,
									&streamFormat,
									sizeof(AudioStreamBasicDescription));
		if (err) { printf ("AudioUnitSetProperty-SF=%4.4s, %ld\n", (char*)&err, (long int)err); return -1; }
		
		// Initialize unit
		err = AudioUnitInitialize(gOutputUnit);
		if (err) { printf ("AudioUnitInitialize=%ld\n", (long int)err); return -1; }

		hvl_DecodeFrame( ht, audiobuffer[0], audiobuffer[0]+2, 4 );
		hvl_DecodeFrame( ht, audiobuffer[1], audiobuffer[1]+2, 4 );

		// Start the rendering
		// The DefaultOutputUnit will do any format conversions to the format of the default device
		err = AudioOutputUnitStart (gOutputUnit);
		if (err) { printf ("AudioOutputUnitStart=%ld\n", (long int)err); return -1; }
		
		// Setup signal handling
		// set up new handler to specify new action
		new_action.sa_handler = sig_handler;
		sigemptyset (&new_action.sa_mask);
		sigaction(1, &new_action, NULL);
		sigaction(2, &new_action, NULL);
		
		while (catchedSignal == 0)
		{
		// we call the CFRunLoopRunInMode to service any notifications that the audio
		// system has to deal with
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
		}
		// REALLY after you're finished playing STOP THE AUDIO OUTPUT UNIT!!!!!!	
		// but we never get here because we're running until the process is nuked...	
		err = AudioOutputUnitStop (gOutputUnit);
		if (err) { printf ("AudioOutputUnitStop=%ld\n", (long int)err); return -1; }
		
		err = AudioUnitUninitialize (gOutputUnit);
		if (err) { printf ("AudioUnitUninitialize=%ld\n", (long int)err); return -1; }
		shut();
	}
	return 0;
}

